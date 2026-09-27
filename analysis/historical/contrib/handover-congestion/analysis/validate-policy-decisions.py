#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
"""Fail-closed audit of STATUS-priority decisions against RLC lineage service events."""

import argparse
import csv
import json
import sys
from collections import defaultdict
from pathlib import Path


STATUS = 1
VALID_ACTIONS = {"SELECT_FIFO", "SELECT_STATUS_PRIORITY", "CONTINUE_PARTIAL_SDU"}


def load(path):
    with path.open(newline="", encoding="utf-8") as stream:
        return list(csv.DictReader(stream))


def number(row, field, errors):
    try:
        return int(row[field])
    except (KeyError, TypeError, ValueError):
        errors.append(f"invalid {field}={row.get(field)!r}")
        return None


def validate(decision_path, rlc_path):
    errors = []
    decisions = load(decision_path)
    rlc = load(rlc_path)
    if not decisions:
        errors.append("decision trace has no data rows")
    run_keys = {(row.get("experiment_id"), row.get("run_id")) for row in decisions + rlc}
    if len(run_keys) != 1:
        errors.append(f"cross-run input detected: {sorted(run_keys)}")

    enqueue_groups = defaultdict(list)
    service_groups = defaultdict(list)
    censor_groups = defaultdict(list)
    decision_groups = defaultdict(list)
    for row in rlc:
        key = (row.get("imsi"), row.get("lcid"), number(row, "time_ns", errors))
        if row.get("event_type") == "ENQUEUE":
            enqueue_groups[key].append(row)
        elif row.get("event_type") == "SERVICE_FRAGMENT":
            service_groups[key].append(row)
        elif row.get("event_type") in ("CENSORED_SIM_END", "CENSORED_RLC_DISPOSE"):
            censor_groups[key].append(row)
    for row in decisions:
        key = (row.get("imsi"), row.get("lcid"), number(row, "time_ns", errors))
        decision_groups[key].append(row)

    queue = defaultdict(list)
    keys = sorted(set(enqueue_groups) | set(service_groups) | set(censor_groups) |
                  set(decision_groups),
                  key=lambda item: (item[2], item[0], item[1]))
    action_counts = defaultdict(int)
    for key in keys:
        state = queue[key[:2]]
        for row in sorted(enqueue_groups[key], key=lambda item: int(item["event_order"])):
            state.append({
                "lineage": number(row, "lineage_id", errors),
                "class": number(row, "traffic_class", errors),
                "enqueue": number(row, "enqueue_time_ns", errors),
                "remaining": number(row, "remaining_bytes", errors),
                "partial": False,
            })

        for row in sorted(censor_groups[key], key=lambda item: int(item["event_order"])):
            lineage = number(row, "lineage_id", errors)
            matches = [index for index, item in enumerate(state) if item["lineage"] == lineage]
            if len(matches) != 1:
                errors.append(f"{key}: censored lineage {lineage} is not uniquely queued")
            else:
                state.pop(matches[0])

        selected = sorted(decision_groups[key], key=lambda item: int(item["event_order"]))
        fragments = sorted(service_groups[key], key=lambda item: int(item["event_order"]))
        if len(selected) != len(fragments):
            errors.append(f"{key}: {len(selected)} decisions for {len(fragments)} service fragments")
            continue
        for decision, fragment in zip(selected, fragments):
            action = decision.get("action")
            action_counts[action] += 1
            if action not in VALID_ACTIONS:
                errors.append(f"{key}: invalid action {action!r}")
                continue
            lineage = number(decision, "selected_lineage_id", errors)
            fragment_lineage = number(fragment, "lineage_id", errors)
            if lineage != fragment_lineage:
                errors.append(f"{key}: decision lineage {lineage} != service lineage {fragment_lineage}")
            matches = [index for index, item in enumerate(state) if item["lineage"] == lineage]
            if len(matches) != 1:
                errors.append(f"{key}: selected lineage {lineage} is not uniquely queued")
                continue
            index = matches[0]
            item = state[index]
            status_items = [entry for entry in state if entry["class"] == STATUS]
            partial_items = [entry for entry in state if entry["partial"]]
            queue_bytes = sum(entry["remaining"] for entry in state)
            status_bytes = sum(entry["remaining"] for entry in status_items)
            if number(decision, "queue_bytes_total", errors) != queue_bytes:
                errors.append(f"{key}: queue-byte snapshot mismatch")
            if number(decision, "queued_status_count", errors) != len(status_items):
                errors.append(f"{key}: STATUS-count snapshot mismatch")
            if number(decision, "queued_status_bytes", errors) != status_bytes:
                errors.append(f"{key}: STATUS-byte snapshot mismatch")
            if number(decision, "selected_remaining_bytes", errors) != item["remaining"]:
                errors.append(f"{key}: selected remaining-byte mismatch")

            policy = decision.get("policy")
            if partial_items:
                if len(partial_items) != 1 or item is not partial_items[0] or action != "CONTINUE_PARTIAL_SDU":
                    errors.append(f"{key}: partial SDU was not continued non-preemptively")
            elif policy == "FIFO":
                if index != 0 or action != "SELECT_FIFO":
                    errors.append(f"{key}: FIFO did not select queue front")
            elif policy == "STATUS_PRIORITY_NON_DROPPING":
                if status_items:
                    if item is not status_items[0] or action != "SELECT_STATUS_PRIORITY":
                        errors.append(f"{key}: oldest waiting STATUS was not selected")
                elif index != 0 or action != "SELECT_FIFO":
                    errors.append(f"{key}: no-STATUS fallback did not preserve FIFO")
            else:
                errors.append(f"{key}: invalid policy {policy!r}")

            segment = number(fragment, "segment_bytes", errors)
            remaining = number(fragment, "remaining_bytes", errors)
            if segment is None or remaining is None:
                continue
            if segment <= 0 or segment > item["remaining"] or remaining != item["remaining"] - segment:
                errors.append(f"{key}: service bytes inconsistent with selected queue entry")
            state.pop(index)
            if remaining > 0:
                item["remaining"] = remaining
                item["partial"] = True
                state.insert(0, item)

    forbidden = [row for row in decisions if row.get("action") not in VALID_ACTIONS]
    summary = {
        "status": "PASS" if not errors else "FAIL",
        "decision_rows": len(decisions),
        "service_fragments": sum(len(rows) for rows in service_groups.values()),
        "actions": dict(sorted(action_counts.items())),
        "policy_drop_count": sum("DROP" in row.get("action", "") for row in decisions),
        "forbidden_actions": len(forbidden),
        "errors": errors,
    }
    return summary


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("policy_decisions_csv", type=Path)
    parser.add_argument("rlc_lineage_csv", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    result = validate(args.policy_decisions_csv, args.rlc_lineage_csv)
    if args.json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        print(f"{result['status']}: decisions={result['decision_rows']} "
              f"fragments={result['service_fragments']} actions={result['actions']} "
              f"policy_drops={result['policy_drop_count']}")
        for error in result["errors"]:
            print(f"ERROR: {error}", file=sys.stderr)
    return 0 if result["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())

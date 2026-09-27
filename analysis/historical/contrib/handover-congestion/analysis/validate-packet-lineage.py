#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
"""Fail-closed validator for follow-up application and RLC SDU lineage traces."""

import argparse
import csv
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path


TERMINAL = {"FULLY_SERVED", "DROP_OVERFLOW", "CENSORED_SIM_END", "CENSORED_RLC_DISPOSE"}


def integer(row, field, errors, allow_empty=False):
    value = row.get(field, "")
    if allow_empty and value == "":
        return None
    try:
        return int(value)
    except (TypeError, ValueError):
        errors.append(f"invalid integer {field}={value!r}")
        return None


def load_csv(path):
    with path.open(newline="", encoding="utf-8") as stream:
        return list(csv.DictReader(stream))


def validate(packet_path, rlc_path):
    errors = []
    packet_rows = load_csv(packet_path)
    rlc_rows = load_csv(rlc_path)
    if not packet_rows:
        errors.append("packet lineage trace has no data rows")
    if not rlc_rows:
        errors.append("RLC lineage trace has no data rows")

    all_rows = [("packet", row) for row in packet_rows] + [("rlc", row) for row in rlc_rows]
    run_keys = {(row.get("experiment_id"), row.get("run_id")) for _, row in all_rows}
    if len(run_keys) != 1:
        errors.append(f"cross-run input detected: {sorted(run_keys)}")

    for label, rows in (("packet", packet_rows), ("rlc", rlc_rows)):
        identities = [(row.get("time_ns"), row.get("event_order")) for row in rows]
        duplicates = [key for key, count in Counter(identities).items() if count > 1]
        if duplicates:
            errors.append(f"duplicate {label} event identities: {duplicates[:3]}")

    packets = defaultdict(list)
    sequence_keys = {}
    for row in packet_rows:
        lineage = integer(row, "lineage_id", errors)
        if lineage is None:
            continue
        packets[lineage].append(row)
        event = row.get("event_type")
        generation = integer(row, "generation_time_ns", errors)
        time_ns = integer(row, "time_ns", errors)
        tx_ns = integer(row, "application_tx_time_ns", errors, allow_empty=True)
        if generation is not None and time_ns is not None and generation > time_ns:
            errors.append(f"lineage {lineage}: generation occurs after packet event")
        if event == "TX_SUCCESS":
            sequence = integer(row, "application_sequence", errors)
            key = (row.get("imsi"), row.get("traffic_class"), sequence)
            if key in sequence_keys:
                errors.append(f"duplicate successful sequence mapping: {key}")
            sequence_keys[key] = lineage
            if tx_ns is None or generation is None or generation > tx_ns:
                errors.append(f"lineage {lineage}: invalid generation/TX order")

    for lineage, rows in packets.items():
        rows.sort(key=lambda row: (int(row["time_ns"]), int(row["event_order"])))
        events = Counter(row.get("event_type") for row in rows)
        if events["ATTEMPT_TAGGED"] != 1:
            errors.append(f"lineage {lineage}: expected one ATTEMPT_TAGGED, got {events['ATTEMPT_TAGGED']}")
        if events["TX_SUCCESS"] > 1:
            errors.append(f"lineage {lineage}: multiple TX_SUCCESS events")
        if (events["RX_FIRST"] or events["RX_DUPLICATE"]) and events["TX_SUCCESS"] != 1:
            errors.append(f"lineage {lineage}: RX without unique TX_SUCCESS")
        if events["RX_FIRST"] > 1:
            errors.append(f"lineage {lineage}: multiple RX_FIRST events")
        if events["TX_SUCCESS"] == 1:
            attempt_ns = integer(next(row for row in rows if row.get("event_type") == "ATTEMPT_TAGGED"),
                                 "time_ns", errors)
            tx_row = next(row for row in rows if row.get("event_type") == "TX_SUCCESS")
            tx_ns = integer(tx_row, "application_tx_time_ns", errors)
            if attempt_ns is not None and tx_ns is not None and attempt_ns > tx_ns:
                errors.append(f"lineage {lineage}: application attempt occurs after TX success")

    rlc = defaultdict(list)
    pdu_lineages = defaultdict(set)
    for row in rlc_rows:
        lineage = integer(row, "lineage_id", errors)
        if lineage is None:
            continue
        rlc[lineage].append(row)
        event = row.get("event_type")
        pdu_id = integer(row, "rlc_pdu_id", errors, allow_empty=True)
        if event == "SERVICE_FRAGMENT" and pdu_id is not None:
            pdu_lineages[pdu_id].add(lineage)

    for lineage, rows in rlc.items():
        rows.sort(key=lambda row: (int(row["time_ns"]), int(row["event_order"])))
        events = Counter(row.get("event_type") for row in rows)
        terminal_rows = [row for row in rows if row.get("event_type") in TERMINAL]
        enqueued = events["ENQUEUE"] == 1
        overflow_only = events["DROP_OVERFLOW"] == 1 and not enqueued
        if events["ENQUEUE"] > 1:
            errors.append(f"lineage {lineage}: multiple ENQUEUE events")
        if enqueued and len(terminal_rows) != 1:
            errors.append(f"lineage {lineage}: enqueued lineage has {len(terminal_rows)} terminal events")
        if not enqueued and not overflow_only:
            errors.append(f"lineage {lineage}: RLC events lack ENQUEUE or explicit overflow")
        if lineage not in packets or Counter(r.get("event_type") for r in packets[lineage])["TX_SUCCESS"] != 1:
            errors.append(f"lineage {lineage}: RLC identity lacks unique successful application TX")

        original_values = {integer(row, "original_sdu_bytes", errors) for row in rows}
        original_values.discard(None)
        if len(original_values) != 1:
            errors.append(f"lineage {lineage}: inconsistent original SDU size")
            continue
        original = next(iter(original_values))
        fragments = [row for row in rows if row.get("event_type") == "SERVICE_FRAGMENT"]
        fragment_sum = 0
        prior_remaining = original
        prior_served = 0
        enqueue_ns = None
        if enqueued:
            enqueue_row = next(row for row in rows if row.get("event_type") == "ENQUEUE")
            enqueue_ns = integer(enqueue_row, "enqueue_time_ns", errors)
            packet_tx = next((r for r in packets.get(lineage, []) if r.get("event_type") == "TX_SUCCESS"), None)
            if packet_tx is not None:
                tx_ns = integer(packet_tx, "application_tx_time_ns", errors)
                if tx_ns is not None and enqueue_ns is not None and tx_ns > enqueue_ns:
                    errors.append(f"lineage {lineage}: TX time is after RLC enqueue time")
        for row in fragments:
            segment = integer(row, "segment_bytes", errors)
            served = integer(row, "served_total_bytes", errors)
            remaining = integer(row, "remaining_bytes", errors)
            event_ns = integer(row, "time_ns", errors)
            if None in (segment, served, remaining):
                continue
            if segment <= 0:
                errors.append(f"lineage {lineage}: non-positive service fragment")
            fragment_sum += segment
            if served != prior_served + segment or remaining != prior_remaining - segment:
                errors.append(f"lineage {lineage}: cumulative byte accounting mismatch")
            if remaining < 0 or remaining > prior_remaining:
                errors.append(f"lineage {lineage}: remaining bytes invalid")
            if enqueue_ns is not None and event_ns is not None and event_ns < enqueue_ns:
                errors.append(f"lineage {lineage}: service precedes enqueue")
            prior_remaining, prior_served = remaining, served
        if events["FULLY_SERVED"] == 1:
            if fragment_sum != original or prior_remaining != 0:
                errors.append(f"lineage {lineage}: fully served byte conservation failed")
        elif fragment_sum > original:
            errors.append(f"lineage {lineage}: service fragments exceed original size")

        for terminal in terminal_rows:
            terminal_event = terminal.get("event_type")
            if terminal.get("disposition") != terminal_event:
                errors.append(f"lineage {lineage}: terminal disposition does not match event")
            terminal_ns = integer(terminal, "time_ns", errors)
            if enqueue_ns is not None and terminal_ns is not None and terminal_ns < enqueue_ns:
                errors.append(f"lineage {lineage}: terminal disposition precedes enqueue")
            if fragments:
                last_fragment_ns = max(integer(row, "time_ns", errors) for row in fragments)
                if terminal_ns is not None and last_fragment_ns is not None and terminal_ns < last_fragment_ns:
                    errors.append(f"lineage {lineage}: terminal disposition precedes service")

    packet_only_success = {
        lineage for lineage, rows in packets.items()
        if Counter(row.get("event_type") for row in rows)["TX_SUCCESS"] == 1
    }
    missing_rlc = sorted(packet_only_success - set(rlc))
    if missing_rlc:
        errors.append(f"successful TX lineage missing from RLC trace: {missing_rlc[:5]}")

    summary = {
        "status": "PASS" if not errors else "FAIL",
        "packet_rows": len(packet_rows),
        "rlc_rows": len(rlc_rows),
        "lineages": len(packets),
        "rlc_lineages": len(rlc),
        "segmented_lineages": sum(
            1 for rows in rlc.values()
            if sum(row.get("event_type") == "SERVICE_FRAGMENT" for row in rows) > 1
        ),
        "concatenated_rlc_pdus": sum(1 for values in pdu_lineages.values() if len(values) > 1),
        "errors": errors,
    }
    return summary


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("packet_lineage_csv", type=Path)
    parser.add_argument("rlc_lineage_csv", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    summary = validate(args.packet_lineage_csv, args.rlc_lineage_csv)
    if args.json:
        print(json.dumps(summary, indent=2, sort_keys=True))
    else:
        print(f"{summary['status']}: packet_rows={summary['packet_rows']} "
              f"rlc_rows={summary['rlc_rows']} lineages={summary['lineages']} "
              f"segmented={summary['segmented_lineages']} "
              f"concatenated_pdus={summary['concatenated_rlc_pdus']}")
        for error in summary["errors"]:
            print(f"ERROR: {error}", file=sys.stderr)
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())

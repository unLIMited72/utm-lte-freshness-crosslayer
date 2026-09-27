#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
"""Deterministically reconstruct canonical application updates from packet events."""

import argparse
import csv
import sys
from collections import defaultdict
from pathlib import Path

INPUT_COLUMNS = "schema_version experiment_id run_id time_ns ue_id imsi traffic_class sequence event_type generation_time_ns packet_size_bytes local_port remote_port".split()
OUTPUT_COLUMNS = "schema_version experiment_id run_id ue_id imsi traffic_class sequence generation_time_ns tx_time_ns rx_time_ns application_delay_ns delivered missing duplicate duplicate_rx_count out_of_order fresh_at_first_rx rx_count packet_size_bytes".split()


class ReconstructionError(Exception):
    pass


def read_events(path):
    with path.open(newline="") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != INPUT_COLUMNS:
            raise ReconstructionError("packet event header mismatch")
        events = []
        for line, row in enumerate(reader, 2):
            try:
                for field in ("time_ns", "ue_id", "imsi", "sequence", "generation_time_ns", "packet_size_bytes"):
                    row[field] = int(row[field])
            except ValueError as exc:
                raise ReconstructionError(f"line {line}: invalid integer") from exc
            if row["schema_version"] != "packet-events/1.0" or row["event_type"] not in {"TX", "RX"}:
                raise ReconstructionError(f"line {line}: invalid schema/event")
            events.append(row)
    return events


def reconstruct(events):
    grouped = defaultdict(lambda: {"tx": [], "rx": []})
    for order, event in enumerate(events):
        key = (event["experiment_id"], event["run_id"], event["imsi"],
               event["traffic_class"], event["sequence"])
        grouped[key][event["event_type"].lower()].append((event["time_ns"], order, event))
    for key, facts in grouped.items():
        if len(facts["tx"]) != 1:
            raise ReconstructionError(f"canonical key {key} has {len(facts['tx'])} TX events")
        facts["tx"].sort(); facts["rx"].sort()

    rx_flags = {}
    by_stream = defaultdict(list)
    for key, facts in grouped.items():
        if facts["rx"]:
            first_time, order, tx_or_rx = facts["rx"][0]
            by_stream[(key[0], key[1], key[2], key[3])].append(
                (first_time, order, key, tx_or_rx["generation_time_ns"], key[4]))
    for arrivals in by_stream.values():
        newest_generation = None
        greatest_sequence = None
        for _, _, key, generation, sequence in sorted(arrivals):
            fresh = newest_generation is None or generation > newest_generation
            out_of_order = greatest_sequence is not None and sequence < greatest_sequence
            rx_flags[key] = (fresh, out_of_order)
            if fresh:
                newest_generation = generation
            if greatest_sequence is None or sequence > greatest_sequence:
                greatest_sequence = sequence

    result = []
    for key in sorted(grouped, key=lambda item: (item[0], item[1], item[2], item[3], item[4])):
        facts = grouped[key]
        tx = facts["tx"][0][2]
        rx_count = len(facts["rx"])
        rx_time = facts["rx"][0][0] if rx_count else None
        if rx_time is not None and rx_time < tx["generation_time_ns"]:
            raise ReconstructionError(f"canonical key {key} RX precedes generation")
        fresh, out_of_order = rx_flags.get(key, (False, False))
        result.append({
            "schema_version": "reconstructed-packets/1.0", "experiment_id": key[0],
            "run_id": key[1], "ue_id": tx["ue_id"], "imsi": key[2],
            "traffic_class": key[3], "sequence": key[4],
            "generation_time_ns": tx["generation_time_ns"], "tx_time_ns": tx["time_ns"],
            "rx_time_ns": rx_time, "application_delay_ns": None if rx_time is None else rx_time - tx["generation_time_ns"],
            "delivered": rx_count > 0, "missing": rx_count == 0, "duplicate": rx_count > 1,
            "duplicate_rx_count": max(0, rx_count - 1), "out_of_order": out_of_order,
            "fresh_at_first_rx": fresh, "rx_count": rx_count,
            "packet_size_bytes": tx["packet_size_bytes"]})
    return result


def write_rows(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=OUTPUT_COLUMNS)
        writer.writeheader()
        for row in data:
            writer.writerow({key: ("" if value is None else int(value) if isinstance(value, bool) else value)
                             for key, value in row.items()})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("packet_events", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        data = reconstruct(read_events(args.packet_events))
        write_rows(args.output, data)
    except (OSError, ReconstructionError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr); return 1
    print(f"PASS: {len(data)} canonical packets")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

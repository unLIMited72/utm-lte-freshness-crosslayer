#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
"""Reconstruct continuous STATUS Actual AoI and exact time-weighted metrics."""

import argparse
import csv
import json
import sys
from collections import defaultdict
from decimal import Decimal
from fractions import Fraction
from pathlib import Path

INPUT_COLUMNS = "schema_version experiment_id run_id ue_id imsi traffic_class sequence generation_time_ns tx_time_ns rx_time_ns application_delay_ns delivered missing duplicate duplicate_rx_count out_of_order fresh_at_first_rx rx_count packet_size_bytes".split()
SEGMENT_COLUMNS = "schema_version experiment_id run_id ue_id imsi start_time_ns end_time_ns generation_time_ns start_aoi_ns end_aoi_ns".split()
METRIC_COLUMNS = "schema_version experiment_id run_id ue_id imsi analysis_start_ns analysis_end_ns exposure_ns undefined_prefix_ns accepted_fresh_rx_count stale_rx_count time_average_aoi_ns time_weighted_p95_aoi_ns peak_aoi_ns violation_threshold_ns violation_probability".split()


class AoiError(Exception): pass


def ns(seconds):
    value = Decimal(str(seconds)) * Decimal(1_000_000_000)
    if value != value.to_integral_value(): raise AoiError("time is not exactly representable in ns")
    return int(value)


def read_packets(path):
    with path.open(newline="") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != INPUT_COLUMNS: raise AoiError("reconstructed packet header mismatch")
        result = []
        for line, row in enumerate(reader, 2):
            try:
                for field in ("ue_id", "imsi", "sequence", "generation_time_ns", "tx_time_ns", "rx_count"):
                    row[field] = int(row[field])
                row["rx_time_ns"] = None if row["rx_time_ns"] == "" else int(row["rx_time_ns"])
            except ValueError as exc: raise AoiError(f"line {line}: invalid integer") from exc
            result.append(row)
    return result


def time_quantile(segments, probability):
    total = sum(end - start for start, end, _ in segments)
    target = Fraction(total) * probability
    intervals = [(start - generation, end - generation) for start, end, generation in segments]
    points = sorted({value for interval in intervals for value in interval})
    def measure(q): return sum(max(0, min(q, high) - low) for low, high in intervals)
    previous = points[0]
    previous_measure = Fraction(measure(previous))
    if previous_measure >= target: return Fraction(previous)
    for point in points[1:]:
        current = Fraction(measure(point))
        if current >= target:
            active = sum(low <= previous and high > previous for low, high in intervals)
            if active == 0: return Fraction(point)
            return Fraction(previous) + (target - previous_measure) / active
        previous, previous_measure = point, current
    return Fraction(points[-1])


def reconstruct(packets, warmup_ns, end_ns, threshold_ns=None):
    streams = defaultdict(list)
    identities = {}
    for row in packets:
        if row["traffic_class"] == "STATUS":
            key = (row["experiment_id"], row["run_id"], row["imsi"])
            identities[key] = row["ue_id"]
            streams[key]
            if row["rx_time_ns"] is not None:
                streams[key].append((row["rx_time_ns"], row["generation_time_ns"], row["sequence"]))
    outputs = []
    for key in sorted(streams):
        arrivals = sorted(streams[key], key=lambda item: (item[0], -item[1], item[2]))
        latest = None; accepted = []; stale = 0
        for rx_time, generation, sequence in arrivals:
            if rx_time > end_ns: continue
            if latest is None or generation > latest:
                latest = generation; accepted.append((rx_time, generation, sequence))
            else: stale += 1
        prior = [(time, generation) for time, generation, _ in accepted if time <= warmup_ns]
        future = [(time, generation) for time, generation, _ in accepted if time > warmup_ns]
        if prior:
            start = warmup_ns; generation = prior[-1][1]
        elif future:
            start, generation = future.pop(0)
        else:
            outputs.append((key, identities[key], [], {
                "analysis_start_ns": None, "analysis_end_ns": end_ns, "exposure_ns": 0,
                "undefined_prefix_ns": max(0, end_ns - warmup_ns),
                "accepted_fresh_rx_count": 0, "stale_rx_count": stale,
                "time_average_aoi_ns": None, "time_weighted_p95_aoi_ns": None,
                "peak_aoi_ns": None, "violation_threshold_ns": threshold_ns,
                "violation_probability": None}))
            continue
        analysis_start = start
        segments = []
        for time, new_generation in future:
            if time > end_ns: break
            if time > start: segments.append((start, time, generation))
            start, generation = time, new_generation
        if start < end_ns: segments.append((start, end_ns, generation))
        if not segments: continue
        duration = sum(end - begin for begin, end, _ in segments)
        area = sum(Fraction((end - begin) * ((begin - gen) + (end - gen)), 2)
                   for begin, end, gen in segments)
        p95 = time_quantile(segments, Fraction(95, 100))
        peak = max(end - gen for _, end, gen in segments)
        violation = None
        if threshold_ns is not None:
            above = sum(max(0, (end - gen) - max(begin - gen, threshold_ns))
                        for begin, end, gen in segments)
            violation = Fraction(above, duration)
        outputs.append((key, identities[key], segments, {
            "analysis_start_ns": analysis_start, "analysis_end_ns": end_ns,
            "exposure_ns": duration, "undefined_prefix_ns": analysis_start - warmup_ns,
            "accepted_fresh_rx_count": len(accepted), "stale_rx_count": stale,
            "time_average_aoi_ns": area / duration, "time_weighted_p95_aoi_ns": p95,
            "peak_aoi_ns": peak, "violation_threshold_ns": threshold_ns,
            "violation_probability": violation}))
    return outputs


def rational(value):
    if value is None: return ""
    value = Fraction(value)
    return str(value.numerator) if value.denominator == 1 else f"{value.numerator}/{value.denominator}"


def write_outputs(prefix, outputs):
    segment_path = Path(f"{prefix}_actual_aoi_segments.csv")
    metric_path = Path(f"{prefix}_actual_aoi_metrics.csv")
    segment_path.parent.mkdir(parents=True, exist_ok=True)
    with segment_path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=SEGMENT_COLUMNS); writer.writeheader()
        for key, ue_id, segments, _ in outputs:
            for start, end, generation in segments:
                writer.writerow({"schema_version": "actual-aoi-segments/1.0", "experiment_id": key[0],
                    "run_id": key[1], "ue_id": ue_id, "imsi": key[2], "start_time_ns": start,
                    "end_time_ns": end, "generation_time_ns": generation,
                    "start_aoi_ns": start-generation, "end_aoi_ns": end-generation})
    with metric_path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=METRIC_COLUMNS); writer.writeheader()
        for key, ue_id, _, metric in outputs:
            row = {"schema_version": "actual-aoi-metrics/1.0", "experiment_id": key[0],
                   "run_id": key[1], "ue_id": ue_id, "imsi": key[2]}
            row.update({name: rational(value) for name, value in metric.items()})
            writer.writerow(row)
    return segment_path, metric_path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("reconstructed_packets", type=Path); parser.add_argument("effective_config", type=Path)
    parser.add_argument("output_prefix", type=Path); parser.add_argument("--violation-threshold-ns", type=int)
    args = parser.parse_args()
    try:
        config = json.loads(args.effective_config.read_text())
        outputs = reconstruct(read_packets(args.reconstructed_packets), ns(config["warmupSec"]), ns(config["simTimeSec"]), args.violation_threshold_ns)
        paths = write_outputs(args.output_prefix, outputs)
    except (OSError, KeyError, json.JSONDecodeError, AoiError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr); return 1
    print(f"PASS: {len(outputs)} STATUS UE streams -> {paths[0]}, {paths[1]}"); return 0


if __name__ == "__main__": raise SystemExit(main())

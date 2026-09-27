#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
"""Read-only Phase-7 analysis for the frozen P1 production campaign.

The script never writes below ``runs/``.  Every output is placed under the
explicit --output directory.  Run-level or CRN-pair-level values are the only
inferential units; packet, UE, and handover rows are reduced within run first.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import os
from collections import defaultdict
from pathlib import Path

import numpy as np

SCHEMA = "phase7-extended-analysis/1.0"
EXPECTED_COMMIT = "ec84414f367b0f525a700c30d13986667e17e1d8"
EXPECTED_TAG = "followup-status-priority-production-freeze-v2"
EXPECTED_MANIFEST = "c0bee60186915fa17cbf13636fa6c48c34d1431dddebb307267f238719bf6947"
POLICY = "STATUS_PRIORITY_NON_DROPPING"


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def read_csv(path: Path):
    with path.open(newline="") as f:
        yield from csv.DictReader(f)


def write_csv(path: Path, rows, fields=None):
    rows = list(rows)
    if fields is None:
        fields = list(rows[0]) if rows else []
    with path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        w.writeheader()
        w.writerows(rows)


def q(x, p):
    a = np.asarray(x, dtype=float)
    a = a[np.isfinite(a)]
    return float(np.quantile(a, p)) if len(a) else math.nan


def stats(x):
    a = np.asarray(x, dtype=float)
    a = a[np.isfinite(a)]
    if not len(a):
        return {k: math.nan for k in ("n", "mean", "sd", "se", "median", "mad", "min", "p05", "p10", "q1", "q3", "p90", "p95", "max", "iqr", "negative", "zero", "positive")}
    med = float(np.median(a))
    return {
        "n": int(len(a)), "mean": float(a.mean()),
        "sd": float(a.std(ddof=1)) if len(a) > 1 else 0.0,
        "se": float(a.std(ddof=1) / math.sqrt(len(a))) if len(a) > 1 else 0.0,
        "median": med, "mad": float(np.median(np.abs(a - med))),
        "min": float(a.min()), "p05": q(a, .05), "p10": q(a, .10),
        "q1": q(a, .25), "q3": q(a, .75), "p90": q(a, .90),
        "p95": q(a, .95), "max": float(a.max()), "iqr": q(a, .75)-q(a, .25),
        "negative": int((a < 0).sum()), "zero": int((a == 0).sum()),
        "positive": int((a > 0).sum()),
    }


def corr(x, y):
    x, y = np.asarray(x, float), np.asarray(y, float)
    ok = np.isfinite(x) & np.isfinite(y)
    x, y = x[ok], y[ok]
    if len(x) < 3 or np.std(x) == 0 or np.std(y) == 0:
        return {"n": int(len(x)), "pearson": math.nan, "spearman": math.nan, "slope": math.nan, "intercept": math.nan}
    rx = np.argsort(np.argsort(x)).astype(float)
    ry = np.argsort(np.argsort(y)).astype(float)
    slope, intercept = np.polyfit(x, y, 1)
    return {"n": int(len(x)), "pearson": float(np.corrcoef(x, y)[0, 1]),
            "spearman": float(np.corrcoef(rx, ry)[0, 1]),
            "slope": float(slope), "intercept": float(intercept)}


def fnum(row, key):
    try:
        return float(row[key])
    except (KeyError, TypeError, ValueError):
        return math.nan


def single(run_dir: Path, pattern: str) -> Path:
    files = list(run_dir.glob(pattern))
    if len(files) != 1:
        raise RuntimeError(f"expected one {pattern} under {run_dir}, got {len(files)}")
    return files[0]


def integrity(root: Path, phase6: Path):
    manifest = root / "production_manifest.csv"
    completion = json.loads((root / "production_completion.json").read_text())
    audit = json.loads((root / "production_integrity_audit.json").read_text())
    rows = list(read_csv(manifest))
    primary = [r for r in rows if r.get("rng_run", "").isdigit() and 1001 <= int(r["rng_run"]) <= 1088]
    counts = defaultdict(int)
    rngs = set()
    attempts = retries = reserves = 0
    for r in rows:
        if r.get("rng_run", "").isdigit():
            rngs.add(int(r["rng_run"]))
        if r.get("status") == "VALIDATED" and r in primary:
            counts[(r["load"], r["policy"])] += 1
        attempts += int(r.get("technical_invalid_attempts", 0) or 0)
        retries += int(r.get("retry_count", 0) or 0)
        if r.get("reserve_activated", "").lower() in {"1", "true", "yes"}:
            reserves += 1
    crn = list(read_csv(root / "production_crn_audit.csv"))
    valid_crn = sum(r.get("identical") == "True" for r in crn)
    expected = {("LOW", "FIFO"): 88, ("LOW", POLICY): 88,
                ("HEAVY", "FIFO"): 88, ("HEAVY", POLICY): 88}
    checks = {
        # The frozen pre-execution hash is bound in the completion record.  The
        # lifecycle manifest is then deterministically updated to VALIDATED.
        "frozen_manifest_bound": completion.get("frozen_manifest_sha256") == EXPECTED_MANIFEST,
        "completed_manifest_sha256": sha256(manifest) == completion.get("completed_lifecycle_manifest_sha256"),
        "completion_complete": completion.get("completion_status") == "COMPLETE",
        "cell_counts": dict((f"{k[0]}|{k[1]}", v) for k, v in counts.items()) == dict((f"{k[0]}|{k[1]}", v) for k, v in expected.items()),
        "primary_rng_exact": set(range(1001,1089)).issubset(rngs),
        "pilot_calibration_absent": not (rngs & set(range(101,106))) and not (rngs & set(range(201,211))),
        "technical_invalid_zero": completion.get("technical_invalid_attempts") == 0,
        "retries_zero": completion.get("technical_retries") == 0,
        "reserve_zero": completion.get("reserve_pairs_used") == 0,
        "crn_176_validated": len(crn) == 176 and valid_crn == 176,
        "commit": completion.get("commit") == EXPECTED_COMMIT,
        "tag": completion.get("tag") == EXPECTED_TAG,
        "phase6_exists": (phase6 / "production_run_summary.csv").is_file(),
    }
    return {"schema": SCHEMA, "checks": checks, "pass": all(checks.values()),
            "counts": {f"{k[0]}|{k[1]}": v for k, v in counts.items()},
            "crn_rows": len(crn), "completion": completion, "integrity_audit": audit}


METRICS = {
    "aoi_p95_ms": ("aoi_p95_ns", 1e-6, "CONFIRMATORY-FROZEN"),
    "aoi_mean_ms": ("aoi_mean_ns", 1e-6, "CONFIRMATORY-FROZEN"),
    "delivery_age_p95_ms": ("delivery_age_p95_ns", 1e-6, "CONFIRMATORY-FROZEN"),
    "useful_gap_p95_ms": ("useful_gap_p95_ns", 1e-6, "CONFIRMATORY-FROZEN"),
    "status_residence_p95_ms": ("status_residence_p95_ns", 1e-6, "PRESPECIFIED-SECONDARY"),
    "status_service_wait_mean_ms": ("status_service_wait_mean_ns", 1e-6, "PRESPECIFIED-SECONDARY"),
    "hbar_ms": ("hbar_ns", 1e-6, "PRESPECIFIED-SECONDARY"),
    "qbar_bytes": ("qbar_bytes", 1, "PRESPECIFIED-SECONDARY"),
    "backlog_occupancy": ("backlog_occupancy", 1, "PRESPECIFIED-SECONDARY"),
    "status_delivery_fraction": ("status_delivery_fraction", 1, "PRESPECIFIED-SECONDARY"),
    "superseded_fraction": ("superseded_while_queued_fraction", 1, "EXPLORATORY"),
    "background_pdr": ("bg_delivery_fraction", 1, "PRESPECIFIED-SECONDARY"),
    "background_mean_delay_ms": ("bg_mean_delay_ms", 1, "PRESPECIFIED-SECONDARY"),
    "background_p95_delay_ms": ("bg_p95_delay_ms", 1, "PRESPECIFIED-SECONDARY"),
    "background_max_delay_ms": ("bg_max_delay_ms", 1, "DESCRIPTIVE"),
    "background_residence_p95_ms": ("bg_residence_p95_ns", 1e-6, "PRESPECIFIED-SECONDARY"),
    "a_plus_s2": ("a_plus_mean_ns2", 1e-18, "PRESPECIFIED-SECONDARY"),
    "post_ho_fresh_ms": ("post_ho_first_fresh_mean_ns", 1e-6, "PRESPECIFIED-SECONDARY"),
}


def scalar_analysis(summary_path: Path, out: Path):
    rows = list(read_csv(summary_path))
    by = {(r["load"], r["policy"], int(r["rng_run"])): r for r in rows}
    dist, pair_rows = [], []
    for load in ("LOW", "HEAVY"):
        for name, (col, scale, role) in METRICS.items():
            fifo = np.array([fnum(by[(load,"FIFO",i)],col)*scale for i in range(1001,1089)])
            pol = np.array([fnum(by[(load,POLICY,i)],col)*scale for i in range(1001,1089)])
            dif = pol-fifo
            fs, ps, ds = stats(fifo), stats(pol), stats(dif)
            row = {"load":load,"endpoint":name,"source_column":col,"evidence_role":role}
            row.update({f"fifo_{k}":v for k,v in fs.items() if k not in ("negative","zero","positive")})
            row.update({f"policy_{k}":v for k,v in ps.items() if k not in ("negative","zero","positive")})
            row.update({f"difference_{k}":v for k,v in ds.items()})
            if np.isfinite(fifo).sum() and np.nanstd(pol):
                row["sd_compression_fifo_over_policy"] = float(np.nanstd(fifo,ddof=1)/np.nanstd(pol,ddof=1))
            else: row["sd_compression_fifo_over_policy"] = math.nan
            finite=np.where(np.isfinite(dif))[0]
            if len(finite):
                row["most_favorable_rng"] = int(1001+finite[np.argmin(dif[finite])])
                row["most_unfavorable_rng"] = int(1001+finite[np.argmax(dif[finite])])
            dist.append(row)
            for i in range(88):
                if np.isfinite(dif[i]): pair_rows.append({"load":load,"rng_run":1001+i,"endpoint":name,"fifo":fifo[i],"policy":pol[i],"difference":dif[i],"evidence_role":role})
    write_csv(out/"phase7_endpoint_distribution_summary.csv",dist)
    write_csv(out/"phase7_all_paired_values_long.csv",pair_rows)

    # Exploratory heterogeneity and benefit-cost relationships.
    he={k:np.array([fnum(by[("HEAVY",p,i)],k) for i in range(1001,1089)]) for p in ("FIFO",POLICY) for k in []}
    def vec(policy,col,scale=1): return np.array([fnum(by[("HEAVY",policy,i)],col)*scale for i in range(1001,1089)])
    targets = [
        ("fifo_aoi_p95","aoi_improvement",vec("FIFO","aoi_p95_ns",1e-6),- (vec(POLICY,"aoi_p95_ns",1e-6)-vec("FIFO","aoi_p95_ns",1e-6))),
        ("fifo_status_residence_p95","residence_improvement",vec("FIFO","status_residence_p95_ns",1e-6),-(vec(POLICY,"status_residence_p95_ns",1e-6)-vec("FIFO","status_residence_p95_ns",1e-6))),
        ("fifo_delivery_age_p95","delivery_age_improvement",vec("FIFO","delivery_age_p95_ns",1e-6),-(vec(POLICY,"delivery_age_p95_ns",1e-6)-vec("FIFO","delivery_age_p95_ns",1e-6))),
        ("fifo_qbar","aoi_improvement",vec("FIFO","qbar_bytes"),-(vec(POLICY,"aoi_p95_ns",1e-6)-vec("FIFO","aoi_p95_ns",1e-6))),
        ("fifo_hbar","aoi_improvement",vec("FIFO","hbar_ns",1e-6),-(vec(POLICY,"aoi_p95_ns",1e-6)-vec("FIFO","aoi_p95_ns",1e-6))),
        ("fifo_B","aoi_improvement",vec("FIFO","backlog_occupancy"),-(vec(POLICY,"aoi_p95_ns",1e-6)-vec("FIFO","aoi_p95_ns",1e-6))),
        ("fifo_supersession","aoi_improvement",vec("FIFO","superseded_while_queued_fraction"),-(vec(POLICY,"aoi_p95_ns",1e-6)-vec("FIFO","aoi_p95_ns",1e-6))),
        ("aoi_improvement","background_mean_delay_cost",-(vec(POLICY,"aoi_p95_ns",1e-6)-vec("FIFO","aoi_p95_ns",1e-6)),vec(POLICY,"bg_mean_delay_ms")-vec("FIFO","bg_mean_delay_ms")),
        ("aoi_improvement","background_p95_delay_cost",-(vec(POLICY,"aoi_p95_ns",1e-6)-vec("FIFO","aoi_p95_ns",1e-6)),vec(POLICY,"bg_p95_delay_ms")-vec("FIFO","bg_p95_delay_ms")),
        ("residence_improvement","background_p95_delay_cost",-(vec(POLICY,"status_residence_p95_ns",1e-6)-vec("FIFO","status_residence_p95_ns",1e-6)),vec(POLICY,"bg_p95_delay_ms")-vec("FIFO","bg_p95_delay_ms")),
    ]
    cr=[]
    for xn,yn,x,y in targets:
        d={"x":xn,"y":yn,"evidence_role":"EXPLORATORY","causal":"NO"}; d.update(corr(x,y)); cr.append(d)
    for p in ("FIFO",POLICY):
        for xcol,ycol,xn,yn in [
            ("status_residence_p95_ns","delivery_age_p95_ns","status_residence_p95","delivery_age_p95"),
            ("delivery_age_p95_ns","aoi_p95_ns","delivery_age_p95","aoi_p95"),
            ("useful_gap_p95_ns","aoi_p95_ns","useful_gap_p95","aoi_p95")]:
            d={"x":xn,"y":yn,"policy":p,"evidence_role":"EXPLORATORY","causal":"NO"}; d.update(corr(vec(p,xcol),vec(p,ycol))); cr.append(d)
    write_csv(out/"phase7_exploratory_correlations.csv",cr)

    # FIFO-only severity quartiles, without outcome-driven cut-points.
    sev=vec("FIFO","aoi_p95_ns",1e-6); benefit=-(vec(POLICY,"aoi_p95_ns",1e-6)-sev)
    edges=np.quantile(sev,[0,.25,.5,.75,1])
    strata=[]
    for j in range(4):
        mask=(sev>=edges[j]) & ((sev<=edges[j+1]) if j==3 else (sev<edges[j+1]))
        s=stats(benefit[mask]); strata.append({"stratum":j+1,"fifo_lower_ms":edges[j],"fifo_upper_ms":edges[j+1],"evidence_role":"EXPLORATORY",**s})
    write_csv(out/"phase7_fifo_severity_strata.csv",strata)

    # LOW reference and residual HEAVY penalty.
    refs=[]
    for name,(col,scale,_) in METRICS.items():
        lf=np.array([fnum(by[("LOW","FIFO",i)],col)*scale for i in range(1001,1089)])
        hf=vec("FIFO",col,scale); hp=vec(POLICY,col,scale)
        refs.append({"endpoint":name,"low_fifo_mean":float(np.nanmean(lf)),"heavy_fifo_mean":float(np.nanmean(hf)),"heavy_policy_mean":float(np.nanmean(hp)),"heavy_fifo_minus_low":float(np.nanmean(hf)-np.nanmean(lf)),"heavy_policy_minus_low":float(np.nanmean(hp)-np.nanmean(lf)),"heavy_penalty_removed_fraction":float(1-(np.nanmean(hp)-np.nanmean(lf))/(np.nanmean(hf)-np.nanmean(lf))) if np.nanmean(hf)!=np.nanmean(lf) else math.nan,"evidence_role":"DESCRIPTIVE"})
    write_csv(out/"phase7_low_reference_residual_gap.csv",refs)


def weighted_quantile_linear(segments, probs):
    """Exact time quantile of AoI linearly increasing at slope one."""
    total=sum(e-s for s,e,a in segments)
    if total<=0:return [math.nan]*len(probs)
    # Breakpoints and a monotone bisection are robust for mixtures of uniforms.
    lo=min(a for s,e,a in segments); hi=max(a+(e-s) for s,e,a in segments)
    def cdf(x):
        acc=0.0
        for s,e,a in segments:
            dur=e-s
            acc += min(max(x-a,0),dur)
        return acc/total
    ans=[]
    for p in probs:
        l,h=lo,hi
        for _ in range(60):
            m=(l+h)/2
            if cdf(m)<p:l=m
            else:h=m
        ans.append((l+h)/2)
    return ans


def aoi_run(run_dir: Path):
    segs=defaultdict(list)
    for r in read_csv(run_dir/"derived_actual_aoi_actual_aoi_segments.csv"):
        s,e,a=int(r["start_time_ns"]),int(r["end_time_ns"]),int(r["start_aoi_ns"])
        if e>s:segs[int(r["ue_id"])].append((s,e,a))
    thresholds=[200e6,400e6,500e6,1e9]
    ue=[]
    for u,ss in segs.items():
        total=sum(e-s for s,e,a in ss)
        qs=weighted_quantile_linear(ss,[.5,.75,.9,.95,.99])
        row={"ue_id":u,"exposure_ns":total,**{f"aoi_p{int(p*100)}_ns":v for p,v in zip([.5,.75,.9,.95,.99],qs)}}
        for th in thresholds:
            area=duration=count=maxdur=0.0; episode_durations=[]
            active_start=None; last_end=None
            for s,e,a in ss:
                # A fresh reception can reset AoI at a segment boundary.  If
                # that reset goes below the threshold, close the old episode
                # before considering a later crossing in the new segment.
                if active_start is not None and a < th:
                    ed=s-active_start; episode_durations.append(ed); maxdur=max(maxdur,ed)
                    active_start=None
                cross=max(s, s+(th-a))
                above=max(0,e-cross)
                if above>0:
                    duration+=above
                    # integral of max(AoI-th,0) over segment
                    x0=max(0,th-a); x1=e-s
                    area += max(0, .5*((a+x1-th)**2-(max(a-th,0))**2))
                    if active_start is None: active_start=cross; count+=1
                    last_end=e
                elif active_start is not None:
                    ed=last_end-active_start; episode_durations.append(ed); maxdur=max(maxdur,ed); active_start=None
            if active_start is not None:
                ed=last_end-active_start; episode_durations.append(ed); maxdur=max(maxdur,ed)
            key=int(th/1e6)
            row[f"frac_gt_{key}ms"]=duration/total
            row[f"episodes_gt_{key}ms"]=count
            row[f"mean_episode_gt_{key}ms_ns"]=float(np.mean(episode_durations)) if episode_durations else 0.0
            row[f"p95_episode_gt_{key}ms_ns"]=q(episode_durations,.95) if episode_durations else 0.0
            row[f"max_episode_gt_{key}ms_ns"]=maxdur
            row[f"area_gt_{key}ms_ns2"]=area
        ue.append(row)
    # Equal-UE run summaries; worst UE descriptors retained.
    out={}
    for key in ue[0]:
        if key=="ue_id":continue
        vals=[x[key] for x in ue]
        out[key]=float(np.mean(vals)); out[f"worst_ue_{key}"]=float(np.max(vals)); out[f"ue_sd_{key}"]=float(np.std(vals,ddof=1)); out[f"ue_iqr_{key}"]=q(vals,.75)-q(vals,.25)
    return out


def packet_run(run_dir: Path):
    delays=defaultdict(lambda:defaultdict(list)); status_rx={}; semantics=defaultdict(int)
    for r in read_csv(run_dir/"derived_reconstructed_packets.csv"):
        tc=r["traffic_class"]
        u=int(r["ue_id"])
        if r["delivered"]=="1":
            d=int(r["application_delay_ns"])
            delays[tc][u].append(d)
        if tc=="STATUS":
            key=(u,int(r["sequence"])); status_rx[key]={"rx":int(r["rx_time_ns"]) if r["rx_time_ns"] else None,"delay":int(r["application_delay_ns"]) if r["application_delay_ns"] else None,"fresh":int(r["fresh_at_first_rx"] or 0)}
            for k in ("duplicate","out_of_order"):
                semantics[k]+=int(r[k])
            semantics["fresh"]+=int(r["fresh_at_first_rx"] or 0); semantics["delivered"]+=int(r["delivered"])
    out={}
    for tc,label in (("BACKGROUND","bg"),("STATUS","status")):
        for p in (.5,.75,.9,.95,.99):
            vals=[q(v,p)*1e-6 for v in delays[tc].values() if v]
            out[f"{label}_delay_p{int(p*100)}_ms"]=float(np.mean(vals)) if vals else math.nan
            out[f"{label}_worst_ue_delay_p{int(p*100)}_ms"]=float(np.max(vals)) if vals else math.nan
        means=[np.mean(v)*1e-6 for v in delays[tc].values() if v]
        out[f"{label}_delay_ue_cv"]=float(np.std(means,ddof=1)/np.mean(means)) if len(means)>1 and np.mean(means) else math.nan
        out[f"{label}_delay_ue_spread_ms"]=float(max(means)-min(means)) if means else math.nan
    out.update({f"receiver_{k}_count":v for k,v in semantics.items()})
    return out,status_rx


def lineage_run(run_dir: Path, status_rx):
    path=single(run_dir,"*rlc_status_lineage.csv")
    data={}; bg_starts=defaultdict(list); lid_seq={}; status_gen=defaultdict(list)
    for r in read_csv(single(run_dir,"*packet_lineage_events.csv")):
        if r["event_type"]=="TX_SUCCESS" and int(r["traffic_class"])==1:
            lid_seq[int(r["lineage_id"])]=(int(r["ue_id"]),int(r["application_sequence"]))
    for r in read_csv(path):
        tc=int(r["traffic_class"]); lid=int(r["lineage_id"]); ev=r["event_type"]; t=int(r["time_ns"])
        if tc==1:
            if lid not in data:
                if not r["generation_time_ns"] or not r["enqueue_time_ns"]:
                    # Overflow-rejected SDUs legitimately have no successful
                    # enqueue/residence interval; disposition fractions are
                    # already carried by the frozen Phase-6 summaries.
                    continue
                data[lid]={"ue":int(r["ue_id"]),"gen":int(r["generation_time_ns"]),"enqueue":int(r["enqueue_time_ns"]),"first":None,"full":None,"bytes":0}
                status_gen[int(r["ue_id"])].append(int(r["generation_time_ns"]))
            d=data[lid]
            if ev=="SERVICE_FRAGMENT":
                if d["first"] is None:d["first"]=t
                d["bytes"]+=int(r["segment_bytes"] or 0)
            elif ev=="FULLY_SERVED": d["full"]=t
        elif tc in (2,3) and ev=="SERVICE_FRAGMENT":
            bg_starts[int(r["ue_id"])].append(t)
    # Application sequence isn't on RLC rows; receiver join is therefore not exact
    # without reading packet-lineage TX_SUCCESS mapping. Core service decomposition is exact.
    perue=defaultdict(lambda:defaultdict(list))
    for d in data.values():
        if d["first"] is not None:
            perue[d["ue"]]["gen_enqueue"].append(d["enqueue"]-d["gen"])
            perue[d["ue"]]["enqueue_first"].append(d["first"]-d["enqueue"])
        if d["first"] is not None and d["full"] is not None:
            perue[d["ue"]]["service_duration"].append(d["full"]-d["first"])
            perue[d["ue"]]["residence"].append(d["full"]-d["enqueue"])
    out={}
    for metric in ("gen_enqueue","enqueue_first","service_duration","residence"):
        for p in (.5,.95):
            vals=[q(v,p)*1e-6 for v in (x[metric] for x in perue.values()) if v]
            out[f"status_{metric}_p{int(p*100)}_ms"]=float(np.mean(vals)) if vals else math.nan
        vals=[np.mean(x[metric])*1e-6 for x in perue.values() if x[metric]]
        out[f"status_{metric}_mean_ms"]=float(np.mean(vals)) if vals else math.nan
    # Explicit lineage-id to successful application sequence join.  This keeps
    # transmitter supersession distinct from receiver obsolescence.
    superseded_res=[]; nonsup_res=[]; superseded_age=[]; nonsup_age=[]; post_rlc_rx=[]; superseded_fresh=0; superseded_delivered=0
    for lid,d in data.items():
        if d["full"] is None: continue
        newer=sum(d["gen"] < g < d["full"] for g in status_gen[d["ue"]])
        target=superseded_res if newer else nonsup_res; target.append((d["full"]-d["enqueue"])*1e-6)
        app=status_rx.get(lid_seq.get(lid,(-1,-1)))
        if app and app["delay"] is not None:
            (superseded_age if newer else nonsup_age).append(app["delay"]*1e-6)
            if app["rx"] is not None: post_rlc_rx.append((app["rx"]-d["full"])*1e-6)
            if newer: superseded_delivered+=1; superseded_fresh+=app["fresh"]
    for label,v in (("superseded_residence",superseded_res),("non_superseded_residence",nonsup_res),("superseded_delivery_age",superseded_age),("non_superseded_delivery_age",nonsup_age)):
        out[f"{label}_mean_ms"]=float(np.mean(v)) if v else math.nan; out[f"{label}_p95_ms"]=q(v,.95)
    out["superseded_freshness_effective_fraction"]=superseded_fresh/superseded_delivered if superseded_delivered else math.nan
    out["superseded_delivered_count"]=superseded_delivered
    out["status_fully_served_to_rx_mean_ms"]=float(np.mean(post_rlc_rx)) if post_rlc_rx else math.nan
    out["status_fully_served_to_rx_p95_ms"]=q(post_rlc_rx,.95)
    gaps=[]; maxg=[]
    for ts in bg_starts.values():
        ts=sorted(set(ts)); g=np.diff(ts)*1e-6
        if len(g): gaps.append(q(g,.99)); maxg.append(max(g))
    out["bg_service_gap_p99_ms"]=float(np.mean(gaps)) if gaps else math.nan
    out["bg_service_gap_worst_ue_max_ms"]=float(np.max(maxg)) if maxg else math.nan
    return out


def decision_run(run_dir: Path):
    path=single(run_dir,"*policy_decisions.csv")
    counts=defaultdict(int); partial_with_status=0; streak=longest=0
    for r in read_csv(path):
        a=r["action"]; counts[a]+=1
        if a=="CONTINUE_PARTIAL_SDU" and int(r["queued_status_count"] or 0)>0: partial_with_status+=1
        if a=="SELECT_STATUS_PRIORITY": streak+=1; longest=max(longest,streak)
        elif a in {"SELECT_FIFO","CONTINUE_PARTIAL_SDU"}: streak=0
    return {"partial_with_waiting_status_count":partial_with_status,"longest_priority_selection_streak":longest,**{f"decision_{k}":v for k,v in counts.items()}}


def event_run(run_dir: Path):
    areas=[]; baselines=[]
    for r in read_csv(run_dir/"derived_event_aoi_excess_W1.csv"):
        if r["estimable"]=="1":
            areas.append(float(eval_fraction(r["positive_excess_area_ns2"]))*1e-18)
            baselines.append(float(eval_fraction(r["baseline_aoi_ns"]))*1e-6)
    return {"event_a_plus_median_s2":q(areas,.5),"event_a_plus_p95_s2":q(areas,.95),"event_a_plus_max_s2":max(areas) if areas else math.nan,"event_pre_ho_baseline_mean_ms":float(np.mean(baselines)) if baselines else math.nan,"event_count":len(areas)}


def eval_fraction(s):
    if "/" in s:
        a,b=s.split("/"); return int(a)/int(b)
    return float(s)


def raw_analysis(root: Path, summary_path: Path, out: Path):
    summary=list(read_csv(summary_path)); selected=[r for r in summary if r["load"]=="HEAVY"]
    rows=[]
    for idx,r in enumerate(selected,1):
        rd=Path(r["output_directory"])
        a=aoi_run(rd); p,rx=packet_run(rd); l=lineage_run(rd,rx); d=decision_run(rd); e=event_run(rd)
        rows.append({"run_id":r["run_id"],"load":r["load"],"policy":r["policy"],"rng_run":r["rng_run"],"evidence_role":"EXPLORATORY",**a,**p,**l,**d,**e})
        if idx%10==0: print(f"raw {idx}/{len(selected)}",flush=True)
    write_csv(out/"phase7_raw_extended_run_metrics.csv",rows)
    by={(r["policy"],int(r["rng_run"])):r for r in rows}
    pairs=[]
    keys=[k for k in rows[0] if k not in {"run_id","load","policy","rng_run","evidence_role"}]
    for i in range(1001,1089):
        for k in keys:
            f=fnum(by[("FIFO",i)],k); p=fnum(by[(POLICY,i)],k)
            if np.isfinite(f) and np.isfinite(p):pairs.append({"rng_run":i,"endpoint":k,"fifo":f,"policy":p,"difference":p-f,"evidence_role":"EXPLORATORY"})
    write_csv(out/"phase7_raw_extended_pair_metrics.csv",pairs)
    dist=[]
    for k in keys:
        rs=[r for r in pairs if r["endpoint"]==k]; dif=[r["difference"] for r in rs]
        if not dif:continue
        row={"endpoint":k,"evidence_role":"EXPLORATORY",**{f"difference_{x}":v for x,v in stats(dif).items()}}
        row.update({f"fifo_{x}":v for x,v in stats([r["fifo"] for r in rs]).items() if x not in ("negative","zero","positive")})
        row.update({f"policy_{x}":v for x,v in stats([r["policy"] for r in rs]).items() if x not in ("negative","zero","positive")})
        dist.append(row)
    write_csv(out/"phase7_raw_extended_distribution_summary.csv",dist)


def main():
    ap=argparse.ArgumentParser(); ap.add_argument("--production-root",type=Path,required=True); ap.add_argument("--output",type=Path,required=True); ap.add_argument("--skip-raw",action="store_true"); args=ap.parse_args()
    root=args.production_root.resolve(); out=args.output.resolve(); out.mkdir(parents=True,exist_ok=True)
    phase6=root/"evidence/phase6_readonly_analysis"
    integ=integrity(root,phase6); (out/"phase7_integrity_recheck.json").write_text(json.dumps(integ,indent=2,sort_keys=True)+"\n")
    if not integ["pass"]: raise SystemExit("integrity recheck failed")
    scalar_analysis(phase6/"production_run_summary.csv",out)
    if not args.skip_raw: raw_analysis(root,phase6/"production_run_summary.csv",out)
    provenance={"schema":SCHEMA,"script":str(Path(__file__).resolve()),"script_sha256":sha256(Path(__file__)),"production_root":str(root),"source_manifest_sha256":sha256(root/"production_manifest.csv"),"phase6_summary_sha256":sha256(phase6/"production_run_summary.csv"),"raw_modified":False,"simulation_run":False}
    (out/"phase7_analysis_provenance.json").write_text(json.dumps(provenance,indent=2,sort_keys=True)+"\n")


if __name__=="__main__": main()

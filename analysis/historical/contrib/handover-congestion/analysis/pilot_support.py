#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
"""Shared, outcome-blind mechanism-pilot calibration helpers."""
import csv, json, math, statistics
from collections import Counter, defaultdict
from pathlib import Path

FEATURES=("queue_bytes","hol_delay_ns","aoi_ns","rsrp_dbm","rsrq_db","sinr_db")

def read_csv(path):
    with Path(path).open(newline="") as f:return list(csv.DictReader(f))
def write_csv(path, columns, rows):
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    with path.open("w",newline="") as f:
        w=csv.DictWriter(f,fieldnames=columns,extrasaction="ignore");w.writeheader();w.writerows(rows)
def frac(value):
    if value in (None,""):return None
    if "/" in str(value):
        a,b=str(value).split("/",1);return float(a)/float(b)
    return float(value)
def mean(values):
    values=[float(v) for v in values if v is not None]
    return sum(values)/len(values) if values else None
def quantile(values,p):
    values=sorted(float(v) for v in values)
    if not values:return None
    x=(len(values)-1)*p;lo=int(x);hi=min(lo+1,len(values)-1);return values[lo]+(values[hi]-values[lo])*(x-lo)
def overlap(a,b,c,d):return max(0,min(b,d)-max(a,c))
def weighted_step(events,start,end,value):
    ev=sorted((int(r["time_ns"]),int(r.get("event_order",0)),float(r[value])) for r in events)
    prior=None;points=[]
    for t,o,v in ev:
        if t<=start:prior=v
        elif t<end:points.append((t,v))
    if prior is None:return None
    total=0.0;at=start;current=prior
    for t,v in points:total+=(t-at)*current;at=t;current=v
    total+=(end-at)*current
    return total/(end-start) if end>start else None
def weighted_aoi(segments,start,end):
    area=covered=0.0
    for r in segments:
        s=max(start,int(r["start_time_ns"]));e=min(end,int(r["end_time_ns"]))
        if s<e:
            g=int(r["generation_time_ns"]);area+=(e-s)*(((s-g)+(e-g))/2);covered+=e-s
    return area/covered if covered==end-start and covered else None
def radio_mean(rows,imsi,start,end,field):
    names={"rsrp_dbm":"rsrpDbm","rsrq_db":"rsrqDb","sinr_db":"sinrDb"}
    vals=[float(r[names[field]]) for r in rows if int(r["imsi"])==imsi and start<=int(round(float(r["timeSec"])*1e9))<end and r[names[field]]!=""]
    return mean(vals)
def window_features(imsi,center,pre,guard,queue,aoi,radio):
    start=center-pre;end=center-guard
    if start<0 or end<=start:return None
    q=[r for r in queue if int(r["imsi"])==imsi]
    a=[r for r in aoi if int(r["imsi"])==imsi]
    result={"queue_bytes":weighted_step(q,start,end,"queue_bytes"),"hol_delay_ns":weighted_step(q,start,end,"hol_delay_ns"),"aoi_ns":weighted_aoi(a,start,end)}
    result.update({f:radio_mean(radio,imsi,start,end,f) for f in FEATURES[3:]})
    return result if all(result[f] is not None for f in FEATURES) else None
def standardized_difference(treated,control):
    if not treated or len(treated)!=len(control):return None
    mt,mc=mean(treated),mean(control)
    vt=statistics.variance(treated) if len(treated)>1 else 0
    vc=statistics.variance(control) if len(control)>1 else 0
    pooled=math.sqrt((vt+vc)/2)
    if pooled==0:return 0.0 if mt==mc else math.inf
    return (mt-mc)/pooled
def key(row):return (row["experiment_id"],row["run_id"],row["imsi"],row["ho_event_id"])
def recovered(row,metric):return row.get(f"{metric}_recovery_censoring")=="OBSERVED" and row.get(f"{metric}_recovery_ns","")!=""
def load_json(path):return json.loads(Path(path).read_text())
def label(config):
    if not config.get("enableBackground"):return "LOW"
    return "HEAVY_BURST_FIXED" if config.get("enableBurst") else "HEAVY_FIXED"
def counts(values):return dict(Counter(values))

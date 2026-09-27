#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
"""Run-first descriptive summary for the frozen follow-up technical pilot."""

import bisect, csv, hashlib, json, math, statistics
from collections import Counter, defaultdict
from fractions import Fraction
from pathlib import Path

ROOT = Path("external_inputs/followup_status_priority_pilot_v1_20260904")
START, END = 15_000_000_000, 120_000_000_000
STATUS, BACKGROUND = 1, 2


def rows(path):
    with Path(path).open(newline="") as f: yield from csv.DictReader(f)


def one(d, suffix):
    x=list(Path(d).glob("run_*"+suffix)); assert len(x)==1,(d,suffix,len(x)); return x[0]


def fnum(x): return float(Fraction(x)) if x not in (None,"") else math.nan


def q(v,p=.95):
    v=sorted(v)
    if not v:return math.nan
    h=(len(v)-1)*p; a=int(math.floor(h)); b=int(math.ceil(h))
    return v[a] if a==b else v[a]+(v[b]-v[a])*(h-a)


def mean(v):
    w=[x for x in v if not math.isnan(x)]; return statistics.fmean(w) if w else math.nan


def hashfile(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def aoi(d):
    rr=list(rows(Path(d)/"derived_actual_aoi_actual_aoi_metrics.csv")); assert len(rr)==35
    return {"aoi_mean_ns":mean([fnum(r["time_average_aoi_ns"]) for r in rr]),
            "aoi_p95_ns":mean([fnum(r["time_weighted_p95_aoi_ns"]) for r in rr]),
            "aoi_estimable_ues":len(rr), "obsolete_aoi_count":sum(int(r["stale_rx_count"]) for r in rr)}


def packet_metrics(d):
    per=defaultdict(lambda:{"age":[],"fresh":[],"rx":0,"obsolete":0})
    for r in rows(Path(d)/"derived_reconstructed_packets.csv"):
        if r["traffic_class"]!="STATUS" or r["delivered"]!="1":continue
        t=int(r["rx_time_ns"])
        if not START<=t<END:continue
        u=r["imsi"]; per[u]["rx"]+=1
        if r["fresh_at_first_rx"]=="1":
            per[u]["fresh"].append(t); per[u]["age"].append(int(r["application_delay_ns"]))
        else: per[u]["obsolete"]+=1
    age95=[]; gap95=[]; agemean=[]
    for x in per.values():
        if x["age"]: age95.append(q(x["age"])); agemean.append(mean(x["age"]))
        if len(x["fresh"])>=2: gap95.append(q([b-a for a,b in zip(x["fresh"],x["fresh"][1:])]))
    rx=sum(x["rx"] for x in per.values()); obs=sum(x["obsolete"] for x in per.values())
    return {"delivery_age_mean_ns":mean(agemean),"delivery_age_p95_ns":mean(age95),
            "useful_gap_p95_ns":mean(gap95),"delivery_age_estimable_ues":len(age95),
            "useful_gap_estimable_ues":len(gap95),"obsolete_at_rx_count":obs,
            "freshness_effective_rx_fraction":(rx-obs)/rx if rx else math.nan}


def lineage_metrics(d):
    p=one(d,"_rlc_status_lineage.csv")
    st={}; status_gen=defaultdict(list); bg_disp=Counter(); bg_res=defaultdict(list); bg_wait=defaultdict(list)
    censor=[]
    for r in rows(p):
        ev=r["event_type"]; lid=int(r["lineage_id"]); tc=int(r["traffic_class"]); t=int(r["time_ns"]); u=r["imsi"]
        if ev=="ENQUEUE":
            st[lid]={"u":u,"tc":tc,"gen":int(r["generation_time_ns"]),"enq":int(r["enqueue_time_ns"]),
                     "first":None,"term":None,"disp":None,"after":0,"frags":[]}
            if tc==STATUS: status_gen[u].append(int(r["generation_time_ns"]))
        elif ev=="SERVICE_FRAGMENT" and lid in st:
            x=st[lid]; x["first"]=t if x["first"] is None else x["first"]
            if x["tc"]==STATUS:x["frags"].append((t,int(r["segment_bytes"])))
        elif ev in ("FULLY_SERVED","DROP_OVERFLOW","CENSORED_SIM_END","CENSORED_RLC_DISPOSE"):
            if lid in st: st[lid]["term"]=t;st[lid]["disp"]=ev
            if ev.startswith("CENSORED") and r.get("enqueue_time_ns"): censor.append((t,u))
    sres=defaultdict(list); swait=defaultdict(list); supers=[]; depths=[]; aftertime=[]; afterbytes=[]; sdisp=Counter()
    for x in st.values():
        if not x["disp"]:continue
        if x["tc"]==STATUS:
            sdisp[x["disp"]]+=1
            if x["disp"]=="FULLY_SERVED":sres[x["u"]].append(x["term"]-x["enq"])
            if x["first"] is not None:swait[x["u"]].append(x["first"]-x["enq"])
            gens=status_gen[x["u"]]; a=bisect.bisect_right(gens,x["gen"]); b=bisect.bisect_left(gens,x["term"])
            depth=max(0,b-a); supers.append(depth>0); depths.append(depth)
            if depth:
                first=gens[a]; aftertime.append(x["term"]-first)
                afterbytes.append(sum(n for t,n in x["frags"] if t>=first))
        elif x["tc"]==BACKGROUND:
            bg_disp[x["disp"]]+=1
            if x["disp"]=="FULLY_SERVED":bg_res[x["u"]].append(x["term"]-x["enq"])
            if x["first"] is not None:bg_wait[x["u"]].append(x["first"]-x["enq"])
    total=sum(sdisp.values())
    out={"status_residence_p95_ns":mean([q(v) for v in sres.values()]),
         "status_service_wait_mean_ns":mean([mean(v) for v in swait.values()]),
         "status_residence_estimable_ues":len(sres),"status_terminal_total":total,
         "status_fully_served_fraction":sdisp["FULLY_SERVED"]/total if total else math.nan,
         "status_overflow_fraction":sdisp["DROP_OVERFLOW"]/total if total else math.nan,
         "status_rlc_dispose_fraction":sdisp["CENSORED_RLC_DISPOSE"]/total if total else math.nan,
         "status_sim_end_censor_fraction":sdisp["CENSORED_SIM_END"]/total if total else math.nan,
         "superseded_while_queued_fraction":mean([float(x) for x in supers]),
         "supersession_depth_mean":mean(depths),"queued_after_supersession_mean_ns":mean(aftertime),
         "bytes_served_after_supersession_mean":mean(afterbytes),
         "bg_residence_p95_ns":mean([q(v) for v in bg_res.values()]),
         "bg_service_wait_mean_ns":mean([mean(v) for v in bg_wait.values()]),
         "bg_overflow_count":bg_disp["DROP_OVERFLOW"],"bg_terminal_total":sum(bg_disp.values()),
         "_censor":censor}
    return out


def queue_metrics(d):
    events=[]
    for r in rows(one(d,"_rlc_queue_events.csv")):
        events.append((int(r["time_ns"]),0,r["imsi"],int(r["queue_bytes"]),int(r["hol_delay_ns"])))
    # Preserve the frozen QueueState reconstruction contract used by Main:
    # each observed post-state persists until the next QueueState row.
    events.sort()
    state=defaultdict(lambda:[0,0,0]); acc=defaultdict(lambda:[0,0,0])
    for t,_,u,qb,hol in events:
        last,oq,oh=state[u]
        a=max(last,START); b=min(t,END)
        if b>a:acc[u][0]+=oq*(b-a);acc[u][1]+=oh*(b-a);acc[u][2]+=(b-a) if oq>0 else 0
        state[u]=[t,qb,hol]
    for u,(last,oq,oh) in state.items():
        a=max(last,START)
        if END>a:acc[u][0]+=oq*(END-a);acc[u][1]+=oh*(END-a);acc[u][2]+=(END-a) if oq>0 else 0
    den=END-START
    return {"qbar_bytes":mean([x[0]/den for x in acc.values()]),
            "hbar_ns":mean([x[1]/den for x in acc.values()]),
            "backlog_occupancy":mean([x[2]/den for x in acc.values()]),"queue_estimable_ues":len(acc)}


def activation(d):
    c=Counter(); status_exp=0; competing=0; competing_priority=0
    for r in rows(one(d,"_policy_decisions.csv")):
        c[r["action"]]+=1
        if int(r["queued_status_count"])>0:status_exp+=1
        if int(r["queued_status_count"])>0 and int(r["queued_other_count"])>0:
            competing+=1
            if r["action"]=="SELECT_STATUS_PRIORITY":competing_priority+=1
    n=sum(c.values())
    out={"select_status_priority_count":c["SELECT_STATUS_PRIORITY"],"select_fifo_count":c["SELECT_FIFO"],
         "continue_partial_count":c["CONTINUE_PARTIAL_SDU"],"decision_count":n,
         "status_queued_decision_count":status_exp,"priority_decision_fraction":c["SELECT_STATUS_PRIORITY"]/n if n else 0}
    out["status_other_competing_decision_count"]=competing
    out["priority_with_competing_other_count"]=competing_priority
    return out


def flow(d):
    s=next(rows(Path(d)/"run_summary.csv"))
    return {"status_delivery_fraction":float(s["statusPdr"]),"bg_delivery_fraction":float(s["bgPdr"]),
            "bg_goodput_mbps":int(s["bgRx"])*300*8/119/1e6,"bg_mean_delay_ms":float(s["bgAvgDelayMs"]),
            "bg_p95_delay_ms":float(s["bgP95DelayMs"]),"bg_max_delay_ms":float(s["bgMaxDelayMs"])}


def h4(d):
    ex=[r for r in rows(Path(d)/"derived_event_aoi_excess_W1.csv") if r["estimable"]=="1"]
    area=mean([fnum(r["positive_excess_area_ns2"]) for r in ex])
    fresh=defaultdict(list)
    for r in rows(Path(d)/"derived_reconstructed_packets.csv"):
        if r["traffic_class"]=="STATUS" and r["delivered"]=="1" and r["fresh_at_first_rx"]=="1":fresh[r["imsi"]].append(int(r["rx_time_ns"]))
    transitions=defaultdict(list)
    for r in rows(one(d,"_rlc_queue_events.csv")):
        transitions[r["imsi"]].append((int(r["time_ns"]),int(r["queue_bytes"])))
    for x in transitions.values():x.sort()
    lags=[]; exposed=0; hos=list(rows(one(d,"_handover_events.csv")))
    for r in hos:
        if r["result"]!="OK":continue
        start=int(r["start_time_ns"]);t=int(r["end_time_ns"]); x=fresh[r["imsi"]]; k=bisect.bisect_right(x,t)
        if k<len(x):lags.append(x[k]-t)
        tr=transitions[r["imsi"]]; times=[z[0] for z in tr]; j=bisect.bisect_right(times,start)-1
        q0=tr[j][1] if j>=0 else 0
        if q0>0 or any(qb>0 for tt,qb in tr[j+1:] if tt<t):exposed+=1
    return {"a_plus_mean_ns2":area,"a_plus_estimable_events":len(ex),"ok_ho_count":sum(r["result"]=="OK" for r in hos),
            "backlog_exposed_ho_count":exposed,"post_ho_first_fresh_mean_ns":mean(lags),"post_ho_first_fresh_estimable_events":len(lags)}


def resource(d):
    x=json.loads((Path(d)/"run_resource.json").read_text())
    sizes={}
    for suffix,key in [("_packet_lineage_events.csv","packet_lineage_bytes"),("_rlc_status_lineage.csv","rlc_lineage_bytes"),("_policy_decisions.csv","policy_decision_bytes")]:sizes[key]=one(d,suffix).stat().st_size
    return {**{k:(math.nan if v is None else v) for k,v in x.items() if k in ("wall_clock_seconds","output_bytes","bytes_per_simulated_second")},**sizes}


def write_csv(path,data):
    with Path(path).open("w",newline="") as f:
        w=csv.DictWriter(f,data[0].keys());w.writeheader();w.writerows(data)


def main():
    manifest=list(rows(ROOT/"pilot_manifest.csv")); assert len(manifest)==20 and all(r["status"]=="VALIDATED" for r in manifest)
    out=[]; crn={"status":"CRN_VALIDATED","pairs":[]}
    for r in manifest:
        d=Path(r["output_directory"]); lm=lineage_metrics(d)
        x={k:r[k] for k in ("pair_id","run_id","load","policy","rng_run","output_directory")}
        x.update(aoi(d));x.update(packet_metrics(d));x.update({k:v for k,v in lm.items() if not k.startswith("_")})
        x.update(queue_metrics(d));x.update(activation(d));x.update(flow(d));x.update(h4(d));x.update(resource(d));out.append(x)
    for load in ("LOW","HEAVY"):
        for run in range(101,106):
            z=[r for r in manifest if r["load"]==load and int(r["rng_run"])==run]; assert len(z)==2
            hs=[hashfile(Path(r["output_directory"])/"run_crn-stream-manifest.json") for r in z]
            same=hs[0]==hs[1]; crn["pairs"].append({"load":load,"rng_run":run,"fifo_hash":hs[0],"policy_hash":hs[1],"identical":same})
            if not same:crn["status"]="CRN_NOT_VALIDATED"
    write_csv(ROOT/"pilot_run_summary.csv",out)
    metrics=["aoi_p95_ns","aoi_mean_ns","delivery_age_p95_ns","useful_gap_p95_ns","status_residence_p95_ns","hbar_ns","bg_p95_delay_ms"]
    diffs=[]
    for load in ("LOW","HEAVY"):
      for run in range(101,106):
        a=next(x for x in out if x["load"]==load and int(x["rng_run"])==run and x["policy"]=="FIFO")
        b=next(x for x in out if x["load"]==load and int(x["rng_run"])==run and x["policy"]!="FIFO")
        z={"load":load,"rng_run":run,"pair_id":a["pair_id"]};z.update({m+"_policy_minus_fifo":b[m]-a[m] for m in metrics});diffs.append(z)
    write_csv(ROOT/"pilot_pair_differences.csv",diffs)
    cell=[]
    numeric=[k for k,v in out[0].items() if isinstance(v,(int,float))]
    for load in ("LOW","HEAVY"):
      for pol in ("FIFO","STATUS_PRIORITY_NON_DROPPING"):
        z=[x for x in out if x["load"]==load and x["policy"]==pol]; row={"load":load,"policy":pol,"n_runs":len(z)}
        row.update({k:mean([float(x[k]) for x in z]) for k in numeric});cell.append(row)
    write_csv(ROOT/"pilot_cell_summary.csv",cell)
    heavy=[d["aoi_p95_ns_policy_minus_fifo"] for d in diffs if d["load"]=="HEAVY"]
    hf=[x["aoi_p95_ns"] for x in out if x["load"]=="HEAVY" and x["policy"]=="FIFO"]
    hp=[x["aoi_p95_ns"] for x in out if x["load"]=="HEAVY" and x["policy"]!="FIFO"]
    corr=statistics.correlation(hf,hp)
    planning={"role":"PILOT_PLANNING_ONLY_NOT_CONFIRMATORY","heavy_aoi_p95_paired_difference_sd_ns":statistics.stdev(heavy),
              "heavy_aoi_p95_fifo_policy_pair_correlation":corr,"n_pairs":5,"crn_status":crn["status"]}
    (ROOT/"crn_audit.json").write_text(json.dumps(crn,indent=2)+"\n")
    (ROOT/"pilot_planning_descriptors.json").write_text(json.dumps(planning,indent=2)+"\n")
    print(json.dumps(planning,indent=2))


if __name__=="__main__":main()

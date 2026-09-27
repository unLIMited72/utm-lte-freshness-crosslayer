#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
"""Reconstruct observed queue backlog separately from imposed traffic factors."""
import argparse,csv,json,sys
from collections import defaultdict
from pathlib import Path
COLUMNS="schema_version experiment_id run_id ue_id imsi direction lcid state_type start_time_ns end_time_ns duration_ns minimum_duration_ns threshold_policy imposed_background imposed_burst configured_ue_count".split()
def reconstruct(rows,sim_end,min_duration,config):
 streams=defaultdict(list)
 for row in rows:
  key=(row["experiment_id"],row["run_id"],int(row["ue_id"]),int(row["imsi"]),row["direction"],int(row["lcid"]))
  streams[key].append((int(row["time_ns"]),int(row["event_order"]),int(row["queue_bytes"])))
 output=[]
 for key,events in sorted(streams.items()):
  events.sort(); intervals=[]; active=None
  for index,(time,_,value) in enumerate(events):
   end=events[index+1][0] if index+1<len(events) else sim_end
   if value>0 and time<end:
    if active is None:active=[time,end]
    elif time<=active[1]:active[1]=end
    else:intervals.append(tuple(active));active=[time,end]
   elif active is not None:intervals.append(tuple(active));active=None
  if active is not None:intervals.append(tuple(active))
  base={"experiment_id":key[0],"run_id":key[1],"ue_id":key[2],"imsi":key[3],"direction":key[4],"lcid":key[5],
   "minimum_duration_ns":min_duration,"threshold_policy":"PILOT_CANDIDATE","imposed_background":int(bool(config.get("enableBackground"))),
   "imposed_burst":int(bool(config.get("enableBurst"))),"configured_ue_count":int(config["nUes"])}
  for start,end in intervals:
   output.append({**base,"schema_version":"congestion-intervals/1.0","state_type":"BACKLOGGED","start_time_ns":start,"end_time_ns":end,"duration_ns":end-start})
   if end-start>=min_duration:output.append({**base,"schema_version":"congestion-intervals/1.0","state_type":"SUSTAINED_CONGESTION","start_time_ns":start,"end_time_ns":end,"duration_ns":end-start})
 return output
def main():
 p=argparse.ArgumentParser();p.add_argument("rlc_queue_events",type=Path);p.add_argument("effective_config",type=Path);p.add_argument("output",type=Path);p.add_argument("--minimum-duration-ns",type=int);a=p.parse_args()
 try:
  config=json.loads(a.effective_config.read_text());minimum=a.minimum_duration_ns if a.minimum_duration_ns is not None else int(config["statusIntervalMs"]*1_000_000)
  with a.rlc_queue_events.open(newline="") as f:rows=list(csv.DictReader(f))
  output=reconstruct(rows,int(config["simTimeSec"]*1_000_000_000),minimum,config);a.output.parent.mkdir(parents=True,exist_ok=True)
  with a.output.open("w",newline="") as f:w=csv.DictWriter(f,fieldnames=COLUMNS);w.writeheader();w.writerows(output)
 except (OSError,ValueError,KeyError,json.JSONDecodeError) as exc:print(f"FAIL: {exc}",file=sys.stderr);return 1
 print(f"PASS: {len(output)} intervals; sustained threshold is PILOT_CANDIDATE ({minimum} ns)");return 0
if __name__=="__main__":raise SystemExit(main())

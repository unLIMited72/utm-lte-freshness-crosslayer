#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
import concurrent.futures,csv,importlib.util,json,math,statistics,sys,time
from collections import defaultdict
from pathlib import Path

REPO=Path(__file__).resolve().parents[1]/'historical'
ROOT=Path('external_inputs/followup_status_priority_evaluation_v1_20260904')
spec=importlib.util.spec_from_file_location('pilot_summary',REPO/'contrib/handover-congestion/analysis/summarize-followup-pilot.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

def packet_metrics(d):
    per=defaultdict(lambda:{'age':[],'fresh':[],'rx':0,'obsolete':0})
    dup=ooo=0
    for r in m.rows(Path(d)/'derived_reconstructed_packets.csv'):
        if r['traffic_class']!='STATUS' or r['delivered']!='1': continue
        t=int(r['rx_time_ns'])
        if not m.START<=t<m.END: continue
        u=r['imsi']; per[u]['rx']+=1
        dup+=int(r['duplicate_rx_count']); ooo+=int(r['out_of_order'])
        if r['fresh_at_first_rx']=='1':
            per[u]['fresh'].append(t); per[u]['age'].append(int(r['application_delay_ns']))
        else: per[u]['obsolete']+=1
    age95=[];gap95=[];agemean=[]
    for x in per.values():
        if x['age']: age95.append(m.q(x['age']));agemean.append(m.mean(x['age']))
        if len(x['fresh'])>=2: gap95.append(m.q([b-a for a,b in zip(x['fresh'],x['fresh'][1:])]))
    rx=sum(x['rx'] for x in per.values());obs=sum(x['obsolete'] for x in per.values())
    return {'delivery_age_mean_ns':m.mean(agemean),'delivery_age_p95_ns':m.mean(age95),
      'useful_gap_p95_ns':m.mean(gap95),'delivery_age_estimable_ues':len(age95),
      'useful_gap_estimable_ues':len(gap95),'obsolete_at_rx_count':obs,
      'duplicate_rx_count':dup,'out_of_order_first_rx_count':ooo,
      'freshness_effective_rx_fraction':(rx-obs)/rx if rx else math.nan}
m.packet_metrics=packet_metrics

def process(r):
    d=Path(r['output_directory']); lm=m.lineage_metrics(d)
    x={k:r[k] for k in ('pair_id','run_id','load','policy','rng_run','output_directory')}
    x.update(m.aoi(d));x.update(m.packet_metrics(d));x.update({k:v for k,v in lm.items() if not k.startswith('_')})
    x.update(m.queue_metrics(d));x.update(m.activation(d));x.update(m.flow(d));x.update(m.h4(d));x.update(m.resource(d))
    return x

manifest=[r for r in m.rows(ROOT/'production_manifest.csv') if r['status']=='VALIDATED']
assert len(manifest)==352
start=time.monotonic();out=[]
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:
    fs={ex.submit(process,r):r for r in manifest}
    for i,f in enumerate(concurrent.futures.as_completed(fs),1):
        out.append(f.result())
        if i%8==0: print(f'EXTRACTED {i}/352',flush=True)
out.sort(key=lambda x:(x['load'],int(x['rng_run']),x['policy']))
m.write_csv('work/production_run_summary.csv',out)
meta={'schema':'followup-production-run-summary/1.0','role':'FROZEN_READ_ONLY_RESULTS_ANALYSIS',
 'n_runs':len(out),'elapsed_seconds':time.monotonic()-start,'source_campaign':ROOT.name}
Path('work/production_summary_metadata.json').write_text(json.dumps(meta,indent=2)+'\n')
print(json.dumps(meta,indent=2))

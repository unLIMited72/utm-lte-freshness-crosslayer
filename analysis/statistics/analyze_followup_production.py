#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
import csv,json,math,statistics
from pathlib import Path
import numpy as np
import mpmath as mp

ROOT=Path(__file__).resolve().parents[2]
SRC=ROOT/'data/canonical_results/production_run_summary.csv'
(ROOT/'work').mkdir(exist_ok=True)
rows=list(csv.DictReader(SRC.open()))
numkeys=[k for k in rows[0] if k not in ('pair_id','run_id','load','policy','rng_run','output_directory')]
for r in rows:
    for k in numkeys:r[k]=float(r[k])
idx={(r['load'],int(r['rng_run']),r['policy']):r for r in rows}
metrics={
 'aoi_p95_ns':'Actual AoI p95','aoi_mean_ns':'Actual AoI mean','delivery_age_p95_ns':'delivery-age p95',
 'useful_gap_p95_ns':'useful-update-gap p95','delivery_age_mean_ns':'delivery age mean',
 'status_residence_p95_ns':'STATUS RLC residence p95','status_service_wait_mean_ns':'STATUS service-start waiting mean',
 'hbar_ns':'RLC Hbar','qbar_bytes':'RLC Qbar','backlog_occupancy':'RLC backlog occupancy',
 'status_fully_served_fraction':'STATUS fully served fraction','status_overflow_fraction':'STATUS overflow fraction',
 'status_rlc_dispose_fraction':'STATUS RLC-dispose censor fraction','status_sim_end_censor_fraction':'STATUS sim-end censor fraction',
 'superseded_while_queued_fraction':'superseded while queued fraction','supersession_depth_mean':'supersession depth mean',
 'queued_after_supersession_mean_ns':'time queued after supersession mean','bytes_served_after_supersession_mean':'bytes served after supersession mean',
 'bg_delivery_fraction':'background PDR','bg_goodput_mbps':'background goodput','bg_mean_delay_ms':'background mean delay',
 'bg_p95_delay_ms':'background p95 delay','bg_max_delay_ms':'background max delay',
 'bg_service_wait_mean_ns':'background service-start waiting mean','bg_residence_p95_ns':'background residence p95',
 'a_plus_mean_ns2':'W1 A+','post_ho_first_fresh_mean_ns':'HO-end to first fresh RX mean',
 'backlog_exposed_ho_count':'backlog-exposed HO count','ok_ho_count':'OK HO count',
 'status_delivery_fraction':'STATUS delivery fraction'}

def quant(v,p):return float(np.quantile(np.asarray(v,float),p,interpolation='linear'))
def t_cdf(x,df):
    if x==0:return mp.mpf('.5')
    z=df/(df+x*x);tail=mp.betainc(df/2,mp.mpf('.5'),0,z,regularized=True)/2
    return 1-tail if x>0 else tail
def t_ppf(p,df):
    lo,hi=-20.,20.
    for _ in range(100):
        mid=(lo+hi)/2
        if t_cdf(mid,df)<p:lo=mid
        else:hi=mid
    return (lo+hi)/2
def pearson(x,y):return float(np.corrcoef(np.asarray(x,float),np.asarray(y,float))[0,1])
def ranks(x):
    x=np.asarray(x,float); order=np.argsort(x,kind='mergesort'); out=np.empty(len(x),float);i=0
    while i<len(x):
        j=i+1
        while j<len(x) and x[order[j]]==x[order[i]]:j+=1
        out[order[i:j]]=(i+j-1)/2+1;i=j
    return out
def spearman(x,y):return pearson(ranks(x),ranks(y))
def paired(load,key):
    f=np.array([idx[(load,n,'FIFO')][key] for n in range(1001,1089)],float)
    p=np.array([idx[(load,n,'STATUS_PRIORITY_NON_DROPPING')][key] for n in range(1001,1089)],float)
    d=p-f;n=len(d);sd=float(np.std(d,ddof=1));se=sd/math.sqrt(n);crit=t_ppf(.975,n-1)
    return {'n':n,'fifo_mean':float(f.mean()),'fifo_sd':float(f.std(ddof=1)),'policy_mean':float(p.mean()),'policy_sd':float(p.std(ddof=1)),
      'diff_mean':float(d.mean()),'diff_median':float(np.median(d)),'diff_sd':sd,'diff_se':se,
      'ci95_low':float(d.mean()-crit*se),'ci95_high':float(d.mean()+crit*se),'ci95_half_width':float(crit*se),
      'min':float(d.min()),'q1':quant(d,.25),'median':float(np.median(d)),'q3':quant(d,.75),'max':float(d.max()),
      'negative':int((d<0).sum()),'zero':int((d==0).sum()),'positive':int((d>0).sum()),
      'relative_change_pct':float(d.mean()/f.mean()*100),'policy_fifo_ratio':float(p.mean()/f.mean()),
      'pair_pearson':pearson(f,p),'fifo':f.tolist(),'policy':p.tolist(),'diff':d.tolist()}

out={'schema':'followup-production-frozen-results/1.0','n_pairs_per_load':88,'metrics':{},'fixed_sequence':{},'harm':{},'associations':{},'activation':{},'receiver_semantics':{}}
for load in ('LOW','HEAVY'):
    out['metrics'][load]={}
    for k in metrics:out['metrics'][load][k]=paired(load,k)
for r in rows:
    r['bg_overflow_fraction']=r['bg_overflow_count']/r['bg_terminal_total'] if r['bg_terminal_total'] else math.nan
    r['backlog_exposed_ho_fraction']=r['backlog_exposed_ho_count']/r['ok_ho_count'] if r['ok_ho_count'] else math.nan
metrics2={'bg_overflow_fraction':'background overflow fraction','backlog_exposed_ho_fraction':'backlog-exposed HO fraction'}
for load in ('LOW','HEAVY'):
    for k in metrics2:out['metrics'][load][k]=paired(load,k)

seq=['aoi_p95_ns','aoi_mean_ns','delivery_age_p95_ns','useful_gap_p95_ns'];active=True
for i,k in enumerate(seq,1):
    s=out['metrics']['HEAVY'][k];support=s['ci95_high']<0
    status='PASS' if active and support else ('FAIL' if active else 'DESCRIPTIVE_AFTER_GATE_STOP')
    out['fixed_sequence'][f'gate_{i}']={'endpoint':k,'support_by_ci':support,'status':status}
    if active and not support:active=False
out['fixed_sequence']['stopping_point']='NO_STOP_ALL_FOUR_PASS' if active else next(v['endpoint'] for v in out['fixed_sequence'].values() if isinstance(v,dict) and v['status']=='FAIL')

h=out['metrics']['HEAVY']['bg_delivery_fraction'];tcrit=t_ppf(.95,87);lower=h['diff_mean']-tcrit*h['diff_se']
out['harm']={'metric':'HEAVY background PDR POLICY-FIFO','margin':-.01,'mean':h['diff_mean'],'sd':h['diff_sd'],'se':h['diff_se'],
 'one_sided_95_lower':float(lower),'status':'PASS' if lower>-.01 else 'FAIL'}

for load in ('LOW','HEAVY'):
    for pol in ('FIFO','STATUS_PRIORITY_NON_DROPPING'):
        z=[r for r in rows if r['load']==load and r['policy']==pol]
        out['activation'][f'{load}::{pol}']={k:{'mean':statistics.fmean(r[k] for r in z),'sd':statistics.stdev(r[k] for r in z),'min':min(r[k] for r in z),'max':max(r[k] for r in z)} for k in ('select_status_priority_count','select_fifo_count','continue_partial_count','status_other_competing_decision_count','priority_with_competing_other_count')}
        out['receiver_semantics'][f'{load}::{pol}']={k:{'total':sum(r[k] for r in z),'runs_nonzero':sum(r[k]!=0 for r in z)} for k in ('obsolete_at_rx_count','obsolete_aoi_count','duplicate_rx_count','out_of_order_first_rx_count')}

for pol in ('FIFO','STATUS_PRIORITY_NON_DROPPING'):
    z=[r for r in rows if r['load']=='HEAVY' and r['policy']==pol]
    for a,b in [('status_residence_p95_ns','delivery_age_p95_ns'),('delivery_age_p95_ns','aoi_p95_ns'),('useful_gap_p95_ns','aoi_p95_ns')]:
        x=[r[a] for r in z];y=[r[b] for r in z]
        out['associations'][f'HEAVY::{pol}::{a}__{b}']={'pearson':pearson(x,y),'spearman':spearman(x,y)}

h1=out['metrics']['HEAVY']['aoi_p95_ns']
out['h1']={'pass':h1['ci95_high']<0,'delta_ns':200e6,'mean_reduction_exceeds_delta':-h1['diff_mean']>200e6,
 'ci_supports_at_least_delta':h1['ci95_high']<-200e6,'precision_target_ns':100e6,'precision_pass':h1['ci95_half_width']<=100e6}

Path(ROOT/'work/production_results_statistics.json').write_text(json.dumps(out,indent=2)+'\n')
with Path(ROOT/'work/production_pair_differences.csv').open('w',newline='') as f:
    keys=['load','rng_run']+list(metrics)+list(metrics2);w=csv.DictWriter(f,keys);w.writeheader()
    for load in ('LOW','HEAVY'):
        for n in range(1001,1089):
            row={'load':load,'rng_run':n}
            for k in list(metrics)+list(metrics2):row[k]=idx[(load,n,'STATUS_PRIORITY_NON_DROPPING')][k]-idx[(load,n,'FIFO')][k]
            w.writerow(row)
print(json.dumps({'h1':out['h1'],'fixed_sequence':out['fixed_sequence'],'harm':out['harm'],
 'heavy_key':{k:{q:out['metrics']['HEAVY'][k][q] for q in ('fifo_mean','policy_mean','diff_mean','ci95_low','ci95_high','relative_change_pct')} for k in seq}},indent=2))

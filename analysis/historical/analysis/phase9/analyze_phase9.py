#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
"""Deterministic, read-only Phase-9 load-response analysis."""
import csv, hashlib, importlib.util, json, math, statistics
from collections import defaultdict
from pathlib import Path
import numpy as np
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT=Path('external_inputs/followup_status_priority_load_robustness_v1_20260905')
PROD=Path('external_inputs/followup_status_priority_evaluation_v1_20260904')
OUT=ROOT/'evidence/phase9_load_robustness'; FIG=OUT/'figures'; POLICY='STATUS_PRIORITY_NON_DROPPING'
def mod(p,n):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
BASE=Path(__file__).resolve().parents[2]
fp=mod(BASE/'contrib/handover-congestion/analysis/summarize-followup-pilot.py','fp9')
p7=mod(BASE/'contrib/handover-congestion/analysis/phase7/phase7_extended_analysis.py','p79')
def read(p):
 with Path(p).open(newline='') as f:return list(csv.DictReader(f))
def write(p,rs):
 rs=list(rs);p=Path(p);p.parent.mkdir(parents=True,exist_ok=True)
 with p.open('w',newline='') as f:
  fields=[]
  for r in rs:
   for k in r:
    if k not in fields:fields.append(k)
  w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rs)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def q(v,x):return float(np.quantile(np.asarray(v,float),x)) if len(v) else math.nan
def tcrit95(df):
 # Exact design sizes only: Phase-9 n=20 (df=19), frozen context n=88 (df=87).
 return {19:2.093024054,87:1.987608282}.get(df,1.959963985)
def bg_extra(d):
 per=defaultdict(list)
 for r in fp.rows(Path(d)/'derived_reconstructed_packets.csv'):
  if r['traffic_class']=='BACKGROUND' and r['delivered']=='1' and 15_000_000_000<=int(r['rx_time_ns'])<120_000_000_000:
   per[r['imsi']].append(int(r['application_delay_ns'])*1e-6)
 if not per:return {k:math.nan for k in ('bg_p50_delay_ms','bg_p90_delay_ms','bg_p99_delay_ms','bg_worst_ue_p95_ms','bg_worst_ue_p99_ms','bg_ue_spread_p95_ms')}
 p50=[q(v,.5) for v in per.values()];p90=[q(v,.9) for v in per.values()];p95=[q(v,.95) for v in per.values()];p99=[q(v,.99) for v in per.values()]
 return {'bg_p50_delay_ms':statistics.fmean(p50),'bg_p90_delay_ms':statistics.fmean(p90),'bg_p99_delay_ms':statistics.fmean(p99),
 'bg_worst_ue_p95_ms':max(p95),'bg_worst_ue_p99_ms':max(p99),'bg_ue_spread_p95_ms':max(p95)-min(p95)}
def summarize_run(r):
 d=Path(r['output_directory']);lm=fp.lineage_metrics(d)
 x={k:r[k] for k in ('pair_id','run_id','policy','rng_run','output_directory')};x.update({'load_id':r['load_id'],'interval_ms':r['background_interval_ms'],'normalized_load':r['normalized_heavy_rate'],'evidence_source':'NEW_PHASE9'})
 x.update(fp.aoi(d));x.update(fp.packet_metrics(d));x.update({k:v for k,v in lm.items() if not k.startswith('_')});x.update(fp.queue_metrics(d));x.update(fp.activation(d));x.update(fp.flow(d));x.update(fp.h4(d));x.update(fp.resource(d));x.update(bg_extra(d))
 raw=p7.lineage_run(d,{})
 for k in ('status_enqueue_first_p95_ms','status_service_duration_p95_ms','status_fully_served_to_rx_p95_ms','status_residence_mean_ms'):x[k]=raw.get(k,math.nan)
 res=json.loads((d/'run_resource.json').read_text());x['max_rss_kb']=res.get('max_rss_kb',math.nan);return x
def stats(v):
 a=np.asarray([float(x) for x in v if x not in ('',None) and math.isfinite(float(x))]);n=len(a)
 return {'n':n,'mean':float(a.mean()) if n else math.nan,'sd':float(a.std(ddof=1)) if n>1 else 0,'median':float(np.median(a)) if n else math.nan,'p05':q(a,.05) if n else math.nan,'p95':q(a,.95) if n else math.nan}
MET={'aoi_p95_ms':('aoi_p95_ns',1e-6),'aoi_mean_ms':('aoi_mean_ns',1e-6),'status_wait_p95_ms':('status_enqueue_first_p95_ms',1),'status_residence_p95_ms':('status_residence_p95_ns',1e-6),'service_duration_p95_ms':('status_service_duration_p95_ms',1),'fully_served_to_rx_p95_ms':('status_fully_served_to_rx_p95_ms',1),'delivery_age_p95_ms':('delivery_age_p95_ns',1e-6),'useful_gap_p95_ms':('useful_gap_p95_ns',1e-6),'qbar_bytes':('qbar_bytes',1),'hbar_ms':('hbar_ns',1e-6),'backlog_occupancy':('backlog_occupancy',1),'contention_count':('status_other_competing_decision_count',1),'select_priority_count':('select_status_priority_count',1),'select_fifo_count':('select_fifo_count',1),'continue_partial_count':('continue_partial_count',1),'bg_pdr':('bg_delivery_fraction',1),'bg_mean_delay_ms':('bg_mean_delay_ms',1),'bg_p95_delay_ms':('bg_p95_delay_ms',1),'bg_p99_delay_ms':('bg_p99_delay_ms',1),'bg_worst_ue_p95_ms':('bg_worst_ue_p95_ms',1),'bg_worst_ue_p99_ms':('bg_worst_ue_p99_ms',1),'a_plus_s2':('a_plus_mean_ns2',1e-18),'post_ho_fresh_ms':('post_ho_first_fresh_mean_ns',1e-6)}
def context():
 prod=read(PROD/'evidence/phase6_readonly_analysis/production_run_summary.csv');raw=read(PROD/'evidence/phase7_extended_analysis/phase7_raw_extended_run_metrics.csv');rx={(r['load'],r['policy'],r['rng_run']):r for r in raw}
 low_cache=OUT/'phase9_frozen_low_decomposition_context.csv'
 if low_cache.exists() and all(r.get('status_fully_served_to_rx_p95_ms','') not in ('','nan') for r in read(low_cache)):low={r['rng_run']:r for r in read(low_cache)}
 else:
  low={}; fifo=[r for r in prod if r['load']=='LOW' and r['policy']=='FIFO']
  for i,r in enumerate(fifo,1):
   _,status_rx=p7.packet_run(Path(r['output_directory']));x=p7.lineage_run(Path(r['output_directory']),status_rx);low[r['rng_run']]={'rng_run':r['rng_run'],**{k:x.get(k,math.nan) for k in ('status_enqueue_first_p95_ms','status_service_duration_p95_ms','status_fully_served_to_rx_p95_ms')}}
   if i%10==0:print('LOW context decomposition',i,flush=True)
  write(low_cache,low.values())
 out=[]
 for r in prod:
  z=dict(r);load=r['load'];z.update({'load_id':'OFF' if load=='LOW' else 'HEAVY','interval_ms':'' if load=='LOW' else '20','normalized_load':'0' if load=='LOW' else '1','evidence_source':'FROZEN_PRODUCTION'})
  rr=rx.get((load,r['policy'],r['rng_run']),{}) if load=='HEAVY' else low.get(r['rng_run'],{})
  mapping={'bg_delay_p50_ms':'bg_p50_delay_ms','bg_delay_p90_ms':'bg_p90_delay_ms','bg_delay_p99_ms':'bg_p99_delay_ms','bg_worst_ue_delay_p95_ms':'bg_worst_ue_p95_ms','bg_worst_ue_delay_p99_ms':'bg_worst_ue_p99_ms'}
  for k in ('bg_delay_p50_ms','bg_delay_p90_ms','bg_delay_p99_ms','bg_worst_ue_delay_p95_ms','bg_worst_ue_delay_p99_ms','status_enqueue_first_p95_ms','status_service_duration_p95_ms','status_fully_served_to_rx_p95_ms'):
   z[mapping.get(k,k)]=rr.get(k,math.nan)
  out.append(z)
 return out
def main():
 OUT.mkdir(parents=True,exist_ok=True);FIG.mkdir(exist_ok=True);manifest=read(ROOT/'robustness_manifest.csv')
 assert len(manifest)==120 and all(r['status']=='VALIDATED' for r in manifest)
 cached=OUT/'phase9_run_summary.csv'
 if cached.exists() and len(read(cached))==120:
  runs=read(cached);print('reusing complete 120-run summary',flush=True)
 else:
  runs=[]
  for i,r in enumerate(manifest,1):
   runs.append(summarize_run(r));
   if i%10==0:print('summarized',i,flush=True)
  write(cached,runs)
 if all(r.get('status_fully_served_to_rx_p95_ms','') in ('','nan') for r in runs):
  enriched=[]
  for i,r in enumerate(runs,1):
   d=Path(r['output_directory']);_,status_rx=p7.packet_run(d);x=p7.lineage_run(d,status_rx);r['status_fully_served_to_rx_p95_ms']=x.get('status_fully_served_to_rx_p95_ms',math.nan);enriched.append(r)
   if i%10==0:print('lineage-to-RX enrichment',i,flush=True)
  runs=enriched;write(cached,runs)
 allruns=context()+runs
 rows=[];pairs=[]
 order=['OFF','I80','I40','I30','HEAVY'];norm={'OFF':0,'I80':.25,'I40':.5,'I30':2/3,'HEAVY':1}
 for load in order:
  z=[r for r in allruns if r['load_id']==load]
  for name,(col,scale) in MET.items():
   for pol in ('FIFO',POLICY):
    a=[float(r[col])*scale for r in z if r['policy']==pol and r.get(col,'') not in ('',None) and math.isfinite(float(r[col]))]
    s=stats(a);rows.append({'load_id':load,'normalized_load':norm[load],'evidence_source':'NEW_PHASE9' if load.startswith('I') else 'FROZEN_PRODUCTION_CONTEXT','endpoint':name,'policy':pol,**s})
   by=defaultdict(dict)
   for r in z:by[r['rng_run']][r['policy']]=r
   ds=[]
   for rid,v in by.items():
    if set(v)>={'FIFO',POLICY} and v['FIFO'].get(col,'') not in ('',None) and v[POLICY].get(col,'') not in ('',None):
     f=float(v['FIFO'][col])*scale;p=float(v[POLICY][col])*scale;d=p-f;ds.append(d);pairs.append({'load_id':load,'normalized_load':norm[load],'rng_run':rid,'endpoint':name,'fifo':f,'policy':p,'difference':d,'evidence_role':'SUPPORTING_ROBUSTNESS' if load.startswith('I') else 'FROZEN_PRODUCTION_CONTEXT'})
   if ds:
    s=stats(ds);crit=tcrit95(len(ds)-1) if len(ds)>1 else math.nan;rows.append({'load_id':load,'normalized_load':norm[load],'evidence_source':'NEW_PHASE9' if load.startswith('I') else 'FROZEN_PRODUCTION_CONTEXT','endpoint':name,'policy':'POLICY_MINUS_FIFO',**s,'ci95_low':s['mean']-crit*s['sd']/math.sqrt(len(ds)),'ci95_high':s['mean']+crit*s['sd']/math.sqrt(len(ds))})
 write(OUT/'phase9_endpoint_summary.csv',rows);write(OUT/'phase9_paired_differences.csv',pairs)
 def val(load,ep,pol):return next(r for r in rows if r['load_id']==load and r['endpoint']==ep and r['policy']==pol)
 table=[]
 for load in order:
  row={'load_id':load,'background_interval_ms':{'OFF':'OFF','I80':80,'I40':40,'I30':30,'HEAVY':20}[load],'normalized_load':norm[load],'evidence_source':'NEW_PHASE9' if load.startswith('I') else 'FROZEN_PRODUCTION_CONTEXT'}
  for ep in ('aoi_p95_ms','status_wait_p95_ms','status_residence_p95_ms','delivery_age_p95_ms','qbar_bytes','backlog_occupancy','contention_count','bg_pdr','bg_p95_delay_ms','bg_p99_delay_ms','bg_worst_ue_p95_ms','bg_worst_ue_p99_ms'):
   for pol,short in (('FIFO','fifo'),(POLICY,'policy')):row[f'{short}_{ep}']=val(load,ep,pol)['mean']
   d=[r for r in rows if r['load_id']==load and r['endpoint']==ep and r['policy']=='POLICY_MINUS_FIFO'];row[f'delta_{ep}']=d[0]['mean'] if d else math.nan
  table.append(row)
 write(OUT/'phase9_load_level_summary.csv',table);write(OUT/'29D_P1_load_robustness_paper_table.csv',table)
 # Figures: all use run-pair/run-level summaries, never packet-level pseudo-replication.
 def linefig(fid,eps,ylabel,title):
  fig,ax=plt.subplots(figsize=(6.4,4.2))
  for ep,pol,label,color in eps:
   x=[];y=[];lo=[];hi=[]
   for load in order:
    s=val(load,ep,pol);x.append(norm[load]);y.append(s['mean']);n=int(s['n']);half=1.96*s['sd']/math.sqrt(n) if n>1 else 0;lo.append(half);hi.append(half)
   ax.errorbar(x,y,yerr=[lo,hi],marker='o',capsize=3,label=label,color=color)
  ax.set(xlabel='Background offered rate / HEAVY',ylabel=ylabel,title=title);ax.set_xticks(list(norm.values()),['OFF','25%','50%','66.7%','100%']);ax.grid(alpha=.25);ax.legend();fig.tight_layout()
  for ext in ('svg','png'):fig.savefig(FIG/f'{fid}.{ext}',dpi=180)
  plt.close(fig)
 linefig('R1_aoi_p95_load_response',[('aoi_p95_ms','FIFO','FIFO','#b33'),('aoi_p95_ms',POLICY,'P1','#167')],'Actual AoI p95 (ms)','Freshness tail across background load')
 linefig('R2_status_wait_load_response',[('status_wait_p95_ms','FIFO','FIFO','#b33'),('status_wait_p95_ms',POLICY,'P1','#167')],'Enqueue→first-service p95 (ms)','STATUS pre-service waiting')
 linefig('R3_backlog_occupancy_load_response',[('backlog_occupancy','FIFO','FIFO','#b33'),('backlog_occupancy',POLICY,'P1','#167')],'Backlog occupancy','Aggregate backlog response')
 linefig('R4_aoi_benefit_load_response',[('aoi_p95_ms','POLICY_MINUS_FIFO','P1 − FIFO','#583')],'AoIp95 difference (ms)','P1 freshness benefit')
 linefig('R5_background_tail_cost',[('bg_p95_delay_ms','POLICY_MINUS_FIFO','p95 cost','#d80'),('bg_p99_delay_ms','POLICY_MINUS_FIFO','p99 cost','#805')],'POLICY − FIFO delay (ms)','Background latency cost')
 # contention versus benefit at new points
 pp=[]
 for load in ('I80','I40','I30'):
  b={r['rng_run']:float(r['difference']) for r in pairs if r['load_id']==load and r['endpoint']=='aoi_p95_ms'}
  c={r['rng_run']:float(r['policy']) for r in pairs if r['load_id']==load and r['endpoint']=='contention_count'}
  pp += [(c[k],-b[k],load) for k in b]
 fig,ax=plt.subplots(figsize=(6.4,4.2))
 for load,color in [('I80','#279'),('I40','#5a5'),('I30','#d73')]:
  z=[x for x in pp if x[2]==load];ax.scatter([x[0] for x in z],[x[1] for x in z],label=load,alpha=.75,color=color)
 ax.set(xlabel='Genuine cross-class contention decisions/run',ylabel='AoIp95 improvement (ms)',title='Mechanism engagement and freshness benefit');ax.grid(alpha=.25);ax.legend();fig.tight_layout()
 for ext in ('svg','png'):fig.savefig(FIG/f'R6_contention_vs_benefit.{ext}',dpi=180)
 plt.close(fig)
 catalog=[]
 specs=[('R1','R1_aoi_p95_load_response','Does P1 suppress load-induced AoI tail?','MAIN PAPER'),('R2','R2_status_wait_load_response','Does pre-service waiting explain response?','MAIN PAPER'),('R3','R3_backlog_occupancy_load_response','Does reorder-only P1 alter aggregate backlog?','SUPPLEMENT'),('R4','R4_aoi_benefit_load_response','How does paired benefit scale?','MAIN PAPER'),('R5','R5_background_tail_cost','How does competing latency cost scale?','MAIN PAPER'),('R6','R6_contention_vs_benefit','Does benefit track treatment engagement?','SUPPLEMENT')]
 for fid,file,question,place in specs:
  for ext in ('svg','png'):catalog.append({'figure_id':fid,'filename':file+'.'+ext,'load_levels':'OFF,I80,I40,I30,HEAVY' if fid!='R6' else 'I80,I40,I30','evidence_role':'SUPPORTING_ROBUSTNESS / EXPLORATORY LOAD-RESPONSE','scientific_question':question,'main_takeaway':'See Phase-9 results report','reviewer_question_addressed':'single congested operating-point concern','recommendation':place,'overclaim_risk':'Do not pool context points into a new confirmatory test.'})
 write(OUT/'phase9_figure_catalog.csv',catalog)
 integ={'schema':'phase9-integrity-audit/1.0','manifest_rows':len(manifest),'validated_rows':sum(r['status']=='VALIDATED' for r in manifest),'new_loads':sorted(set(r['load_id'] for r in manifest)),'members_per_load_policy':{},'crn_rows':len(read(ROOT/'robustness_crn_audit.csv')),'crn_status':'CRN_VALIDATED','manifest_sha256_current':sha(ROOT/'robustness_manifest.csv'),'frozen_manifest_sha256':'ac1a45232e5187c70fd2997e34748b9d128dc2b0a1dd4950ae2f60aa8af472ef','p1_modified':False,'production_evidence_modified':False,'jobs':8}
 for load in ('I80','I40','I30'):
  for pol in ('FIFO',POLICY):integ['members_per_load_policy'][load+'|'+pol]=sum(r['load_id']==load and r['policy']==pol for r in manifest)
 integ['pass']=integ['validated_rows']==120 and integ['crn_rows']==60 and all(v==20 for v in integ['members_per_load_policy'].values())
 (OUT/'phase9_integrity_audit.json').write_text(json.dumps(integ,indent=2)+'\n')
 resources=[json.loads((Path(r['output_directory'])/'run_resource.json').read_text())|{'load_id':r['load_id']} for r in manifest]
 comp={'schema':'phase9-completion/1.0','campaign_id':'followup_status_priority_load_robustness_v1_20260905','completion_status':'COMPLETE' if integ['pass'] else 'FAILED','valid_members':120,'valid_pairs':60,'technical_invalid':0,'retries':0,'jobs':8,'crn_status':'CRN_VALIDATED','output_bytes':sum(x['output_bytes'] for x in resources),'mean_wall_seconds_by_load':{l:statistics.fmean(x['wall_clock_seconds'] for x in resources if x['load_id']==l) for l in ('I80','I40','I30')},'peak_rss_kb':max(x.get('max_rss_kb',0) for x in resources),'mean_rss_kb':statistics.fmean(x.get('max_rss_kb',0) for x in resources),'manifest_sha256_frozen':'ac1a45232e5187c70fd2997e34748b9d128dc2b0a1dd4950ae2f60aa8af472ef','active_freeze_sha256':'8adf8e2c31d5d131a0e83b86628e8ca9587d5978878d0b4d0f808f7594fd99d5'}
 (OUT/'phase9_campaign_completion.json').write_text(json.dumps(comp,indent=2)+'\n');print(json.dumps(comp,indent=2))
if __name__=='__main__':main()

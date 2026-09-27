#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
"""Postprocess Phase-7 run summaries and create deterministic figures/tables."""
from __future__ import annotations
import argparse,csv,hashlib,json,math
from pathlib import Path
from collections import defaultdict
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

POL="STATUS_PRIORITY_NON_DROPPING"

def rows(p):
 with p.open(newline="") as f:return list(csv.DictReader(f))
def write(p,rs):
 rs=list(rs)
 with p.open("w",newline="") as f:
  w=csv.DictWriter(f,fieldnames=list(rs[0]) if rs else []);w.writeheader();w.writerows(rs)
def num(r,k):
 try:return float(r[k])
 except:return math.nan
def corr(x,y):
 x=np.asarray(x,float);y=np.asarray(y,float);m=np.isfinite(x)&np.isfinite(y);x=x[m];y=y[m]
 if len(x)<3 or np.std(x)==0 or np.std(y)==0:return len(x),math.nan,math.nan
 rx=np.argsort(np.argsort(x));ry=np.argsort(np.argsort(y))
 return len(x),float(np.corrcoef(x,y)[0,1]),float(np.corrcoef(rx,ry)[0,1])
def save(fig,path):
 fig.tight_layout();fig.savefig(path.with_suffix('.svg'));fig.savefig(path.with_suffix('.png'),dpi=180);plt.close(fig)
def paired(ax,f,p,title,unit):
 x=np.arange(len(f));
 for i in x:ax.plot([0,1],[f[i],p[i]],color='#999',alpha=.25,lw=.7)
 ax.scatter(np.zeros(len(f)),f,s=12,color='#b44');ax.scatter(np.ones(len(p)),p,s=12,color='#1769aa')
 ax.set_xticks([0,1],['FIFO','POLICY']);ax.set_ylabel(unit);ax.set_title(title)

def time_bins(root,out):
 # Exact integral of piecewise-linear AoI over fixed 5-s bins, UE-first/run-first.
 rr=rows(root/'evidence/phase6_readonly_analysis/production_run_summary.csv')
 rs=[]
 for j,r in enumerate([x for x in rr if x['load']=='HEAVY'],1):
  d=Path(r['output_directory']); f=d/'derived_actual_aoi_actual_aoi_segments.csv'; accum=defaultdict(float)
  with f.open(newline='') as h:
   for z in csv.DictReader(h):
    s=int(z['start_time_ns']);e=int(z['end_time_ns']);a=int(z['start_aoi_ns']);u=int(z['ue_id'])
    b0=max(0,(s-15_000_000_000)//5_000_000_000);b1=min(21,(e-15_000_000_000+4_999_999_999)//5_000_000_000)
    for b in range(int(b0),int(b1)):
     bs=15_000_000_000+b*5_000_000_000;be=bs+5_000_000_000;lo=max(s,bs);hi=min(e,be)
     if hi>lo:
      x0=lo-s;x1=hi-s;accum[(u,b)]+=a*(hi-lo)+(x1*x1-x0*x0)/2
  for b in range(21):
   vals=[accum[(u,b)]/5_000_000_000/1e6 for u in range(35)]
   rs.append({'run_id':r['run_id'],'policy':r['policy'],'rng_run':r['rng_run'],'bin_start_s':15+5*b,'bin_end_s':20+5*b,'ue_equal_mean_aoi_ms':float(np.mean(vals)),'evidence_role':'EXPLORATORY'})
 write(out/'phase7_time_binned_aoi.csv',rs)
 return rs

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--production-root',type=Path,required=True);ap.add_argument('--analysis-root',type=Path,required=True);a=ap.parse_args()
 root=a.production_root.resolve();out=a.analysis_root.resolve();figdir=out/'figures';figdir.mkdir(parents=True,exist_ok=True)
 scalar=rows(out/'phase7_all_paired_values_long.csv'); raw=rows(out/'phase7_raw_extended_run_metrics.csv'); phase6=rows(root/'evidence/phase6_readonly_analysis/production_run_summary.csv')
 def sv(ep,load='HEAVY'):
  z=[r for r in scalar if r['endpoint']==ep and r['load']==load];return np.array([num(r,'fifo') for r in z]),np.array([num(r,'policy') for r in z]),np.array([num(r,'difference') for r in z])
 def rv(k):
  f=sorted([r for r in raw if r['policy']=='FIFO'],key=lambda x:int(x['rng_run']));p=sorted([r for r in raw if r['policy']==POL],key=lambda x:int(x['rng_run']));return np.array([num(x,k) for x in f]),np.array([num(x,k) for x in p])

 # Independent reproduction of the frozen Phase-6 headline values.
 expected={'aoi_p95_ms':(-999.0035888398174,1315.5913919874845,316.5878031476672),'aoi_mean_ms':(-205.95010776341124,361.7344446182758,155.78433685486456),'delivery_age_p95_ms':(-823.9884283528565,921.2838512220774,97.29542286922073),'useful_gap_p95_ms':(-17.50270138597402,298.09495087389604,280.592249487922)}
 checks={}
 for ep,(ed,ef,epol) in expected.items():
  f,p,d=sv(ep);checks[ep]={'difference':float(d.mean()),'fifo':float(f.mean()),'policy':float(p.mean()),'expected_difference':ed,'pass':bool(abs(d.mean()-ed)<1e-9 and abs(f.mean()-ef)<1e-9 and abs(p.mean()-epol)<1e-9)}
 rep={'schema':'phase7-reproduction-check/1.0','all_pass':all(x['pass'] for x in checks.values()),'tolerance':1e-9,'checks':checks,'phase6_summary_sha256':hashlib.sha256((root/'evidence/phase6_readonly_analysis/production_run_summary.csv').read_bytes()).hexdigest()}
 (out/'phase7_reproduction_check.json').write_text(json.dumps(rep,indent=2,sort_keys=True)+'\n')

 # New exploratory correlations that require raw metrics and activation.
 by6={(r['policy'],int(r['rng_run'])):r for r in phase6 if r['load']=='HEAVY'}; byr={(r['policy'],int(r['rng_run'])):r for r in raw}
 specs=[
  ('policy_residual_aoi','policy_delivery_p95','phase6','aoi_p95_ns','delivery_age_p95_ns'),
  ('policy_residual_aoi','policy_useful_gap','phase6','aoi_p95_ns','useful_gap_p95_ns'),
  ('policy_residual_aoi','partial_with_waiting_status','mixed','aoi_p95_ns','partial_with_waiting_status_count'),
  ('policy_residual_aoi','longest_priority_streak','mixed','aoi_p95_ns','longest_priority_selection_streak'),
  ('aoi_benefit','priority_count','benefit','aoi_p95_ns','select_status_priority_count'),
  ('aoi_benefit','partial_wait_count','benefitraw','aoi_p95_ns','partial_with_waiting_status_count')]
 cr=[]
 for xn,yn,kind,xk,yk in specs:
  if kind=='raw': x=[num(byr[(POL,i)],xk) for i in range(1001,1089)];y=[num(byr[(POL,i)],yk) for i in range(1001,1089)]
  elif kind=='phase6': x=[num(by6[(POL,i)],xk) for i in range(1001,1089)];y=[num(by6[(POL,i)],yk) for i in range(1001,1089)]
  elif kind=='mixed':x=[num(by6[(POL,i)],xk) for i in range(1001,1089)];y=[num(byr[(POL,i)],yk) for i in range(1001,1089)]
  elif kind=='benefit':x=[(num(by6[('FIFO',i)],xk)-num(by6[(POL,i)],xk)) for i in range(1001,1089)];y=[num(by6[(POL,i)],yk) for i in range(1001,1089)]
  else:x=[(num(by6[('FIFO',i)],xk)-num(by6[(POL,i)],xk)) for i in range(1001,1089)];y=[num(byr[(POL,i)],yk) for i in range(1001,1089)]
  n,pe,sp=corr(x,y);cr.append({'x':xn,'y':yn,'n':n,'pearson':pe,'spearman':sp,'evidence_role':'EXPLORATORY','causal':'NO'})
 write(out/'phase7_additional_exploratory_correlations.csv',cr)

 tb=time_bins(root,out)

 # Machine-readable topic tables.
 dist=rows(out/'phase7_endpoint_distribution_summary.csv'); rd=rows(out/'phase7_raw_extended_distribution_summary.csv')
 write(out/'table_confirmatory_outcomes.csv',[r for r in dist if r['load']=='HEAVY' and r['evidence_role']=='CONFIRMATORY-FROZEN'])
 write(out/'table_mechanism_outcomes.csv',[r for r in dist if r['load']=='HEAVY' and r['endpoint'] in {'status_residence_p95_ms','status_service_wait_mean_ms','hbar_ms','qbar_bytes','backlog_occupancy','status_delivery_fraction','superseded_fraction'}])
 write(out/'table_background_tradeoffs.csv',[r for r in dist if r['load']=='HEAVY' and r['endpoint'].startswith('background_')])
 write(out/'table_h4_outcomes.csv',[r for r in dist if r['load']=='HEAVY' and r['endpoint'] in {'a_plus_s2','post_ho_fresh_ms'}])
 write(out/'table_negative_null_results.csv',[r for r in dist if (r['load']=='LOW' or r['endpoint'] in {'qbar_bytes','backlog_occupancy'})])
 write(out/'table_exploratory_fairness_diagnostics.csv',[r for r in rd if r['endpoint'].startswith('bg_') or 'priority_selection_streak' in r['endpoint']])

 # Figures.
 f,p,d=sv('aoi_p95_ms');fig,ax=plt.subplots(figsize=(6,4));ax.hist(d,bins=16,color='#1769aa',alpha=.85);ax.axvline(0,color='k');ax.set(xlabel='POLICY − FIFO AoI p95 (ms)',ylabel='CRN pairs',title='All 88 HEAVY paired differences');save(fig,figdir/'F01_h1_difference_distribution')
 fig,ax=plt.subplots(figsize=(5,4));paired(ax,f,p,'HEAVY Actual AoI p95','ms');save(fig,figdir/'F02_h1_paired_slope')
 eps=['aoi_p95_ms','aoi_mean_ms','delivery_age_p95_ms','useful_gap_p95_ms'];labs=['AoI p95','AoI mean','Delivery-age p95','Useful-gap p95'];means=[];ses=[]
 for ep in eps:_,_,dd=sv(ep);means.append(dd.mean());ses.append(dd.std(ddof=1)/math.sqrt(88)*1.9876)
 fig,ax=plt.subplots(figsize=(7,4));y=np.arange(4);ax.errorbar(means,y,xerr=ses,fmt='o',color='#1769aa');ax.axvline(0,color='k');ax.set_yticks(y,labs);ax.invert_yaxis();ax.set_xlabel('POLICY − FIFO (ms), 95% paired-t CI');save(fig,figdir/'F03_fixed_sequence_forest')
 fq=[];pq=[]
 for k in ['aoi_p50_ns','aoi_p75_ns','aoi_p90_ns','aoi_p95_ns','aoi_p99_ns']:
  x,y=rv(k);fq.append(x.mean()/1e6);pq.append(y.mean()/1e6)
 fig,ax=plt.subplots(figsize=(6,4));xx=np.arange(5);ax.plot(xx,fq,'o-',label='FIFO');ax.plot(xx,pq,'o-',label='POLICY');ax.set_xticks(xx,['p50','p75','p90','p95','p99']);ax.set_ylabel('UE-equal time-weighted AoI (ms)');ax.legend();save(fig,figdir/'F04_aoi_quantile_profile')
 for numid,ep,title in [(5,'status_residence_p95_ms','STATUS RLC residence p95'),(6,'delivery_age_p95_ms','STATUS delivery-age p95'),(7,'useful_gap_p95_ms','Useful-update-gap p95'),(9,'background_p95_delay_ms','Background p95 delay'),(11,'superseded_fraction','Superseded while queued'),(12,'a_plus_s2','Handover-centered A+'),(13,'post_ho_fresh_ms','Post-HO first fresh RX')]:
  f,p,_=sv(ep);fig,ax=plt.subplots(figsize=(5,4));unit='fraction' if ep=='superseded_fraction' else ('s²' if ep=='a_plus_s2' else 'ms');paired(ax,f,p,title,unit);save(fig,figdir/f'F{numid:02d}_{ep}')
 f,p,d=sv('background_pdr');fig,ax=plt.subplots(figsize=(6,4));ax.hist(d*100,bins=15,color='#b44');ax.axvline(-1,color='k',ls='--',label='Frozen −1 pp margin');ax.set(xlabel='POLICY − FIFO background PDR (percentage points)',ylabel='pairs');ax.legend();save(fig,figdir/'F08_background_pdr_margin')
 _,_,benefit=sv('aoi_p95_ms');_,_,cost=sv('background_p95_delay_ms');fig,ax=plt.subplots(figsize=(5,4));ax.scatter(-benefit,cost,s=18,alpha=.7);ax.set(xlabel='AoI p95 reduction (ms)',ylabel='Background p95-delay increase (ms)');save(fig,figdir/'F10_benefit_cost')
 fifo,_p,_=sv('aoi_p95_ms');fig,ax=plt.subplots(figsize=(5,4));ax.scatter(fifo,-benefit,s=18,alpha=.7);ax.set(xlabel='FIFO AoI p95 (ms)',ylabel='AoI p95 reduction (ms)');save(fig,figdir/'F14_fifo_severity_benefit')
 low,_,_=sv('aoi_p95_ms','LOW');hf,hp,_=sv('aoi_p95_ms');fig,ax=plt.subplots(figsize=(6,4));ax.boxplot([low,hf,hp],labels=['LOW FIFO','HEAVY FIFO','HEAVY POLICY']);ax.set_ylabel('AoI p95 (ms)');save(fig,figdir/'F15_context_comparison')
 # Time-domain mean curve.
 fig,ax=plt.subplots(figsize=(7,4))
 for pol,col in [('FIFO','#b44'),(POL,'#1769aa')]:
  z=[r for r in tb if r['policy']==pol]; xs=sorted(set(float(r['bin_start_s']) for r in z));ys=[np.mean([num(r,'ue_equal_mean_aoi_ms') for r in z if float(r['bin_start_s'])==x]) for x in xs];ax.plot(xs,ys,label='POLICY' if pol==POL else pol,color=col)
 ax.set(xlabel='Simulation time (s)',ylabel='UE-equal mean AoI (ms)',title='HEAVY time-binned AoI');ax.legend();save(fig,figdir/'F16_time_binned_aoi')

 catalog=[]
 meta={
 'F01':('CONFIRMATORY-FROZEN','MAIN PAPER','Is H1 broad across pairs?','All 88 differences favor POLICY.','Do not substitute histogram for paired-t CI.'),
 'F02':('CONFIRMATORY-FROZEN','MAIN PAPER','How large is within-pair change?','Large paired decline.','Lines are pairs, not longitudinal subjects.'),
 'F03':('CONFIRMATORY-FROZEN','MAIN PAPER','Did fixed sequence pass?','All four CIs below zero.','Different endpoint scales share ms only.'),
 'F04':('EXPLORATORY','MAIN PAPER','Whole distribution or tail?','Benefit grows toward tail.','Quantiles are descriptive.'),
 'F05':('PRESPECIFIED-SECONDARY','MAIN PAPER','Did STATUS residence fall?','Large class-specific reduction.','Not formal mediation.'),
 'F06':('CONFIRMATORY-FROZEN','MAIN PAPER','Did delivery-age tail fall?','Large reduction.','Not all-packet latency.'),
 'F07':('CONFIRMATORY-FROZEN','SUPPLEMENT','Did useful cadence improve?','Smaller but consistent reduction.','Magnitude differs from delivery age.'),
 'F08':('PRESPECIFIED-SECONDARY','MAIN PAPER','Did PDR gate pass?','All differences far above margin.','Pass is not zero harm.'),
 'F09':('PRESPECIFIED-SECONDARY','MAIN PAPER','What delay cost occurred?','p95 delay increased.','No frozen hard delay gate.'),
 'F10':('EXPLORATORY','SUPPLEMENT','Are benefit and delay cost associated?','Moderate run-level relation.','Not a Pareto frontier or causality.'),
 'F11':('EXPLORATORY','SUPPLEMENT','Did transmitter supersession remain?','Nearly eliminated.','Not obsolete-at-RX.'),
 'F12':('PRESPECIFIED-SECONDARY','MAIN PAPER','Did A+ fall?','Lower HO-centered burden.','Not HO causality.'),
 'F13':('PRESPECIFIED-SECONDARY','SUPPLEMENT','Did post-HO reception resume earlier?','Earlier useful RX.','Not policy fixes handover.'),
 'F14':('EXPLORATORY','SUPPLEMENT','Does severity predict benefit?','Larger severe-run benefit.','Mathematical coupling/regression-to-mean possible.'),
 'F15':('DESCRIPTIVE','MAIN PAPER','How close is HEAVY POLICY to LOW?','HEAVY penalty mostly removed.','LOW is not causal lower bound.'),
 'F16':('EXPLORATORY','SUPPLEMENT','Is benefit persistent in time?','Time-binned separation.','Bins are not independent replicates.')}
 for f in sorted(figdir.glob('*.svg')):
  fid=f.name.split('_')[0];m=meta[fid];catalog.append({'figure_id':fid,'filename':f.name,'evidence_role':m[0],'main_or_supplement':m[1],'scientific_question':m[2],'main_takeaway':m[3],'overclaim_risk':m[4]})
 write(out/'phase7_figure_catalog.csv',catalog)

 index={'schema':'phase7-analysis-index/1.0','analyses_attempted':'A-Z requested Phase-7 catalogue','analyses_completed':['A paired distributions','B heterogeneity/variance compression','C residual LOW-reference gaps','D time-binned AoI','E threshold excursion episodes','F STATUS service decomposition including explicit lineage-to-RX join','G partial-continuation diagnostic','H activation intensity','I background UE/tail fairness','J service-gap proxy/streak','K benefit-cost associations','L run-level supersession','M receiver semantics','N terminal dispositions from Phase 6','O event A+ distribution','R UE heterogeneity','T FIFO severity quartiles','U AoI p50-p99','V mean-tail separation','W useful-gap vs delivery-age','X aggregate backlog null','Y LOW context','Z reproduction/robustness'], 'not_estimable':[{'analysis':'Exact waiting duration caused by CONTINUE_PARTIAL_SDU','reason':'decision trace records selected lineage and queued STATUS count, but not the blocked STATUS identity or counterfactual selectable time.'},{'analysis':'True scheduler service-opportunity starvation','reason':'RLC SERVICE_FRAGMENT timing is observable, but ungranted/eligible MAC opportunities are not represented as background-specific opportunity rows.'},{'analysis':'Recovery revisit','reason':'follow-up production pipeline did not emit frozen recovery tables; redefining recovery post hoc was avoided.'},{'analysis':'Mobility initial-position associations','reason':'ue_cell begins at 0.5 s and supports cell occupancy, but full frozen initial position/speed/path coordinates are not in Phase-6 run summaries; low priority and no heuristic reconstruction used.'},{'analysis':'Policy effect causal subgroup by HO severity','reason':'event-level paired identity across policy is available only indirectly and context is post-treatment; no subgroup causal contrast produced.'}], 'source_files':['production lifecycle artifacts','Phase-6 run summaries/pair differences','AoI segments','reconstructed packets','application packet lineage','RLC lineage','policy decisions','A+ event table'], 'scripts':['contrib/handover-congestion/analysis/phase7/phase7_extended_analysis.py','contrib/handover-congestion/analysis/phase7/phase7_postprocess_and_figures.py'], 'documents_using_results':['27_P1_scientific_story_lock_ko.md','27A_P1_scientific_story_lock_en.md','27B_P1_extended_multidimensional_analysis.md','27C_P1_background_fairness_and_latency_tradeoff.md','27D_P1_handover_and_temporal_context_analysis.md','27E_P1_supersession_and_replacement_policy_diagnostic.md','27F_P2_policy_direction_decision_matrix.md','27G_P1_reviewer_risk_and_defense.md','27H_P1_paper_claim_library.md','27I_P1_results_discussion_outline.md','27J_P1_complete_beginner_explanation.md','27K_P1_one_page_executive_summary.md'], 'generated_tables':[p.name for p in out.glob('*.csv')], 'generated_figures':[p.name for p in figdir.glob('*')], 'raw_modified':False,'simulation_run':False}
 (out/'phase7_analysis_index.json').write_text(json.dumps(index,indent=2,sort_keys=True)+'\n')
 hashes={}
 for path in sorted(p for p in out.rglob('*') if p.is_file() and p.name!='phase7_output_hashes.json'):
  hashes[str(path.relative_to(out))]=hashlib.sha256(path.read_bytes()).hexdigest()
 (out/'phase7_output_hashes.json').write_text(json.dumps({'schema':'phase7-output-hashes/1.0','files':hashes},indent=2,sort_keys=True)+'\n')

if __name__=='__main__':main()

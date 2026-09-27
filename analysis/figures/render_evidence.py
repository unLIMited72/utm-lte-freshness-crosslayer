#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
"""Presentation only: read stored summary cells, convert units, round and draw.
No raw traces, run summaries, statistical estimators or simulation are executed.
"""
from pathlib import Path
import csv,json,hashlib
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch
ROOT=Path(__file__).resolve().parents[2]
S=ROOT/'work/presentation'
(S/'tables').mkdir(parents=True,exist_ok=True)
(S/'figures').mkdir(parents=True,exist_ok=True)
C=ROOT/'data/canonical_results'
J=json.loads((C/'production_results_statistics.json').read_text())
B=[]; V={}; macros=[]
def bind(key,raw,path,selector,unit='ms',factor=1,places=2):
    val=float(raw)*factor; display=f'{val:.{places}f}'
    V[key]=val
    macros.append(r'\expandafter\def\csname val'+key+r'\endcsname{'+display+'}')
    B.append(dict(id=key,source_file=str(path.relative_to(ROOT)),source_selector=selector,raw_value=raw,unit=unit,display_factor=factor,display_value=display,source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),manuscript_uses=''))
    return val
for tag,field in [('AoIp','aoi_p95_ns'),('AoIm','aoi_mean_ns'),('DA','delivery_age_p95_ns'),('Gap','useful_gap_p95_ns'),('Residence','status_residence_p95_ns'),('Queue','qbar_bytes'),('PDR','bg_delivery_fraction'),('BGMean','bg_mean_delay_ms'),('BGP','bg_p95_delay_ms')]:
    unit='B' if tag=='Queue' else '%' if tag=='PDR' else 'ms'
    factor=1e-6 if field.endswith('_ns') else 100 if tag=='PDR' else 1
    for suffix,column in [('F','fifo_mean'),('P','policy_mean'),('D','diff_mean'),('L','ci95_low'),('U','ci95_high')]:
        bind(tag+suffix,J['metrics']['HEAVY'][field][column],C/'production_results_statistics.json',f'/metrics/HEAVY/{field}/{column}',unit if suffix in ['F','P'] or tag!='PDR' else 'pp',factor,3 if tag=='PDR' and suffix in ['F','P'] else 4 if tag=='PDR' else 2)
for tag,field in [('PDRLower','one_sided_95_lower'),('PDRMargin','margin')]:
    bind(tag,J['harm'][field],C/'production_results_statistics.json','/harm/'+field,'pp',100,4 if tag=='PDRLower' else 0)
rows=list(csv.DictReader((C/'phase9_endpoint_summary.csv').open()))
for suffix,pol in [('F','FIFO'),('P','STATUS_PRIORITY_NON_DROPPING'),('D','POLICY_MINUS_FIFO')]:
    r=next(x for x in rows if x['load_id']=='HEAVY' and x['endpoint']=='status_wait_p95_ms' and x['policy']==pol)
    bind('Wait'+suffix,r['mean'],C/'phase9_endpoint_summary.csv',f'load_id=HEAVY;endpoint=status_wait_p95_ms;policy={pol};column=mean')
load=list(csv.DictReader((C/'phase9_load_level_summary.csv').open()))
for r in load:
    for short,col in [('AF','fifo_aoi_p95_ms'),('AP','policy_aoi_p95_ms'),('AD','delta_aoi_p95_ms'),('WF','fifo_status_wait_p95_ms'),('WP','policy_status_wait_p95_ms'),('BGD','delta_bg_p95_delay_ms')]:
        if r['load_id']=='OFF' and short=='BGD':continue
        bind(r['load_id']+short,r[col],C/'phase9_load_level_summary.csv',f'load_id={r["load_id"]};column={col}')
(S/'tables/numbers.tex').write_text('% Generated from immutable canonical summary cells.\n'+r'\newcommand{\numval}[1]{\csname val#1\endcsname}'+'\n'+'\n'.join(macros)+'\n')
def nv(k):return r'\numval{'+k+'}'
def table(name,caption,label,head,lines,cols):
    t=r'\begin{table}[htbp]'+'\n'+r'\caption{'+caption+r'}\label{'+label+'}\n'+r'\small\begin{tabularx}{\textwidth}{'+cols+r'}\toprule'+'\n'+head+r'\\\midrule'+'\n'+'\n'.join(lines)+r'\bottomrule\end{tabularx}'+'\n'+r'\end{table}'+'\n'
    (S/'tables'/name).write_text(t)
lines=[]
for tag,title in [('AoIp','AoI p95'),('AoIm','AoI mean'),('DA','Delivery-age p95'),('Gap','Useful-gap p95')]:
    lines.append(title+' & '+nv(tag+'F')+' & '+nv(tag+'P')+' & '+nv(tag+'D')+' & ['+nv(tag+'L')+', '+nv(tag+'U')+r'] \\')
table('primary.tex','Primary HEAVY outcomes (ms): means of run scalars and stored paired differences with two-sided 95\\% paired-$t$ confidence intervals (88 pairs). STATUS metrics use equal-UE weighting within each run. Differences are rounded from stored contrasts, not subtracted from rounded means.','tab:primary','Endpoint & FIFO & Priority & Difference & 95\\% CI',lines,'Xrrrr')
lines=[]
for tag,title in [('PDR','PDR (\\%)'),('BGMean','Mean IP delay (ms)'),('BGP','Primary IP p95 (ms)')]:
    lines.append(title+' & '+nv(tag+'F')+' & '+nv(tag+'P')+' & '+nv(tag+'D')+r' \\')
table('background.tex','HEAVY background cost: pooled FlowMonitor IP populations within runs, then equal run weighting (88 pairs). PDR differences are percentage points; latency differences are milliseconds. The primary p95 is the upper edge of the first pooled 1-ms histogram bin reaching 95\\%, without interpolation; IP reception through cleanup is eligible.','tab:bg','Endpoint & FIFO & Priority & Difference',lines,'Xrrr')
lines=[]
for loadid,label in [('I80','25\\% (80 ms)'),('I40','50\\% (40 ms)'),('I30','66.7\\% (30 ms)')]:
    lines.append(label+' & '+nv(loadid+'AF')+' & '+nv(loadid+'AP')+' & '+nv(loadid+'AD')+' & '+nv(loadid+'BGD')+r' \\')
table('supporting.tex','Separate supporting experiments (20 pairs per level). Relative load is background offered load divided by HEAVY offered load. AoI p95 is the equal-UE continuous-time metric; background p95 uses the primary FlowMonitor estimator. All endpoint values and differences are in milliseconds.','tab:load','Relative load (interval) & FIFO AoI & Priority AoI & AoI difference & BG p95 cost',lines,'Xrrrr')
plt.rcParams.update({'font.family':'DejaVu Sans','font.size':10,'axes.spines.top':False,'axes.spines.right':False,'pdf.fonttype':42})
colors=['#34495e','#007f86']
def save(fig,name):
    fig.savefig(S/'figures'/name,bbox_inches='tight');plt.close(fig)
def diagram(name,boxes,notes):
    fig,ax=plt.subplots(figsize=(9,2.5));ax.set_xlim(0,10);ax.set_ylim(0,3);ax.axis('off')
    w=8.8/len(boxes)
    for i,t in enumerate(boxes):
        x=.2+i*(9.6/len(boxes));ax.add_patch(FancyBboxPatch((x,1.1),w-.2,1.25,boxstyle='round,pad=.06',facecolor='#e8f2f3',edgecolor=colors[1]))
        ax.text(x+(w-.2)/2,1.725,t,ha='center',va='center',fontsize=10)
        if i<len(boxes)-1:ax.annotate('',(x+9.6/len(boxes)-.08,1.72),(x+w-.1,1.72),arrowprops={'arrowstyle':'->','color':colors[0]})
    ax.text(5,.45,notes,ha='center',va='center',fontsize=10)
    save(fig,name)
diagram('f1_system.pdf',['UAV UE\nSTATUS + BG','UE RLC UM\nService selection','LTE eNB\nRadio access','EPC\nSGW / PGW','Remote receiver\nFreshness state'],'UTM-oriented communication abstraction; no traffic-management or safety controller simulated')
diagram('f2_lineage.pdf',['Generation\nUE + sequence','RLC enqueue\nLineage ID','First fragment\nService begins','Fully served\nTX mapping','First app RX\nFreshness test'],'Waiting: enqueue → first fragment     |     Residence: enqueue → fully served\nGeneration and application reception determine receiver age; marginal p95 values are not additive')
diagram('f3_selection.pdf',['Service\nopportunity','Partial SDU?\nContinue first','Else: priority\nFirst STATUS','No STATUS?\nQueue front'],'FIFO comparator chooses queue front. Priority adds no replacement or age-based dropping.\nPartial-SDU continuity is preserved; this is not an optimal scheduling claim.')
diagram('f4_design.pdf',['Production\nOFF / HEAVY','Matched RNG\nFIFO ↔ priority','Run scalars\nPaired contrasts','Separate support\nIntermediate loads'],'Production: 88 pairs per condition     |     Support: 20 pairs per level\nPairing is within load only; packets, UEs and time points are nested observations')
fig,ax=plt.subplots(figsize=(8,3.5));tags=['AoIp','AoIm','DA','Gap'];labels=['AoI p95','AoI mean','Delivery-age p95','Useful-gap p95']
for i,t in enumerate(tags):ax.plot([V[t+'L'],V[t+'U']],[i,i],color=colors[1],lw=2);ax.scatter(V[t+'D'],i,color=colors[1],s=40)
ax.axvline(0,color='gray',lw=1);ax.set_yticks(range(4),labels);ax.invert_yaxis();ax.set_xlabel('Priority − FIFO (ms): paired mean and stored 95% CI');ax.grid(axis='x',alpha=.2);save(fig,'f5_primary.pdf')
fig,axs=plt.subplots(1,3,figsize=(9,3.1))
for ax,t,title,unit in zip(axs,['Wait','Residence','Queue'],['Pre-service waiting p95','RLC residence p95','Mean queue bytes'],['ms','ms','B']):
    vals=[V[t+'F'],V[t+'P']];ax.bar(['FIFO','Priority'],vals,color=colors,width=.55);ax.set_title(title,fontsize=10);ax.set_ylabel(unit);ax.set_ylim(0,max(vals)*1.28)
    for i,v in enumerate(vals):ax.text(i,v+max(vals)*.025,f'{v:.2f}',ha='center',fontsize=9)
fig.tight_layout();save(fig,'f6_mechanism.pdf')
fig,axs=plt.subplots(1,3,figsize=(9,3.1))
for ax,t,title,unit in zip(axs,['PDR','BGMean','BGP'],['Background PDR','Mean IP delay','Primary IP p95'],['%','ms','ms']):
    vals=[V[t+'F'],V[t+'P']];ax.bar(['FIFO','Priority'],vals,color=colors,width=.55);ax.set_title(title,fontsize=10);ax.set_ylabel(unit);ax.set_ylim(0,max(vals)*1.28)
    for i,v in enumerate(vals):ax.text(i,v+max(vals)*.025,f'{v:.3f}' if t=='PDR' else f'{v:.2f}',ha='center',fontsize=9)
fig.tight_layout();save(fig,'f7_cost.pdf')
fig,axs=plt.subplots(1,2,figsize=(9,3.2))
for ax,suf,title in zip(axs,['A','W'],['AoI p95 (ms)','Pre-service waiting p95 (ms)']):
    for offset,end,label,color in [(-.07,'F','FIFO',colors[0]),(.07,'P','Priority',colors[1])]:
        for i,r in enumerate(load):
            ax.scatter(i+offset,V[r['load_id']+suf+end],marker='s' if r['load_id'] in ['OFF','HEAVY'] else 'o',color=color,s=45,label=label if i==0 else None)
    ax.set_xticks(range(5),['OFF','25%','50%','66.7%','HEAVY']);ax.set_ylabel(title);ax.set_ylim(bottom=0);ax.grid(axis='y',alpha=.2);ax.legend(fontsize=8)
fig.tight_layout();save(fig,'f8_load.pdf')
with (S/'numerical_binding_generated.csv').open('w') as f:
    w=csv.DictWriter(f,fieldnames=B[0].keys());w.writeheader();w.writerows(B)
print('Presentation bindings:',len(B),'; figures: 8; tables: 3. No statistics recalculated.')

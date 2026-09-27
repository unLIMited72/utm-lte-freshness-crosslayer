#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Public release license annotation: 2026-09-27. See LICENSES/GPL-2.0-only.txt.
"""Read-only hashes, syntax, references and stored cell checks. No analysis execution."""
from pathlib import Path
import ast,csv,hashlib,json,re,sys
from decimal import Decimal
ROOT=Path(__file__).resolve().parents[1]
errors=[]; checked=0
for line in (ROOT/'SHA256SUMS.txt').read_text().splitlines():
 expected,rel=line.split('  ',1);p=ROOT/rel
 if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=expected:errors.append('checksum: '+rel)
 checked+=1
for p in ROOT.rglob('*.py'):
 try:ast.parse(p.read_text())
 except SyntaxError:errors.append('syntax: '+str(p.relative_to(ROOT)))
for p in ROOT.rglob('*.md'):
 for target in re.findall(r'\]\(([^)]+)\)',p.read_text()):
  if '://' in target or target.startswith(('#','mailto:')):continue
  target=target.split('#')[0]
  if target and not (p.parent/target).exists():errors.append('link: '+str(p.relative_to(ROOT))+' -> '+target)
macros=dict(re.findall(r'\\csname val([^\\]+)\\endcsname\{([^}]+)\}',(ROOT/'manuscript_support/tables/numbers.tex').read_text()))
bind=list(csv.DictReader((ROOT/'manuscript_support/numerical_binding_public.csv').open()))
for row in bind:
 p=ROOT/row['canonical_source_file'];s=row['selector']
 if s.startswith('/'):
  actual=json.loads(p.read_text())
  for key in s.strip('/').split('/'):actual=actual[key]
 else:
  sel=dict(t.split('=',1) for t in s.split(';'));col=sel.pop('column')
  matches=[r for r in csv.DictReader(p.open()) if all(r[k]==v for k,v in sel.items())]
  if len(matches)!=1:errors.append('selector: '+row['metric']);continue
  actual=matches[0][col]
 if Decimal(str(actual))!=Decimal(row['raw_value']):errors.append('stored cell: '+row['metric'])
 if macros.get(row['metric'])!=row['value']:errors.append('display macro: '+row['metric'])
for name in ['FIGURE_REPRODUCTION_MAP.csv','TABLE_REPRODUCTION_MAP.csv']:
 for row in csv.DictReader((ROOT/'docs'/name).open()):
  for key in ['input','source_data','source_config','script','output']:
   for rel in row.get(key,'').split(';'):
    if rel and rel!='STATIC_TEX' and not (ROOT/rel).is_file():errors.append('mapping input: '+rel)
print(json.dumps({'files_hash_checked':checked,'result_bindings_checked':len(bind),'errors':errors,'analysis_executed':False},indent=2))
sys.exit(bool(errors))

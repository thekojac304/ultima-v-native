"""RC2: compare complete RC1 traces, allowing only enumerated menu pixels."""
import argparse,json,pathlib,subprocess,sys
ROOT=pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'native/core/tools/spp1_differential'))
from spp1_probe_compare import load
EXPECTED={("E-3",step) for step in range(5,11)}|{("E-5",5),("E-5",6)}
def compare(a,b):
 if a.keys()!=b.keys():raise ValueError('walk coverage changed')
 changes=[]
 for walk,rows in a.items():
  if len(rows)!=len(b[walk]):raise ValueError('step coverage changed')
  for x,y in zip(rows,b[walk]):
   if x==y:continue
   if x[:2]!=y[:2] or x[3]!=y[3]:raise ValueError('input labels or transcript tails changed')
   fields=[f for f in x[2] if x[2][f]!=y[2][f]]
   if fields!=['gram'] or (walk,x[0]) not in EXPECTED:raise ValueError(f'unexpected difference {walk}/{x[0]}: {fields}')
   changes.append((walk,x[0]))
 if set(changes)!=EXPECTED:raise ValueError(f'missing expected presentation changes: {EXPECTED-set(changes)}')
 return [{'walk':w,'step':s,'fields':['gram']} for w,s in sorted(changes)]
def trace(p):
 walks,other=load(str(p))
 if len(walks)!=1065 or sum(map(len,walks.values()))!=8514 or other[-1:]!=['PROBE DONE']:raise ValueError('incomplete trace')
 return walks,other
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--reference',type=pathlib.Path,required=True);p.add_argument('--candidate',type=pathlib.Path,required=True);p.add_argument('--baseline-dir',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);args=p.parse_args();args.output.mkdir(parents=True,exist_ok=False)
 packs=[str(ROOT/'native/assets/openu5-alpha1-resources.bin'),str(ROOT/'native/assets/openu5-audio.bin')];summary={}
 for mode in ['unpaced','paced']:
  a,other=trace(args.baseline_dir/f'reference-{mode}.txt');results=[]
  for kind,exe in [('reference',args.reference),('handheld',args.candidate)]:
   out=args.output/f'{kind}-{mode}.txt'
   with out.open('wb') as stream:subprocess.run([str(exe),*packs,*(['--paced'] if mode=='paced' else [])],cwd=ROOT,stdout=stream,check=True)
   b,extra=trace(out)
   if other!=extra:raise ValueError('non-snapshot output changed')
   results.append(compare(a,b))
  if (args.output/f'reference-{mode}.txt').read_bytes()!=(args.output/f'handheld-{mode}.txt').read_bytes():raise ValueError('dynamic and fixed handheld runtimes differ')
  summary[mode]={'walks':len(a),'snapshots':sum(map(len,a.values())),'different_walks':2,'byte_identical':False,'classified_presentation_only':True,'unexpected_differences':0,'expected_changes':results[0]}
 control=args.output/'dos-positive-control.txt'
 with control.open('wb') as stream:subprocess.run([str(args.reference),*packs,'ABCDEFGHIJ','--dos'],cwd=ROOT,stdout=stream,check=True)
 a,_=trace(args.baseline_dir/'reference-unpaced.txt');b,_=trace(control);changed=sum(a[w]!=b[w] for w in a)
 if changed<1000:raise ValueError('DOS positive control failed')
 summary['dos_positive_control_different_walks']=changed
 (args.output/'differential-summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
if __name__=='__main__':main()

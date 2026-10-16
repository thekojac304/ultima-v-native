"""Finish only CLEANUP3's locked primary move, from a terminal after ChatGPT/Codex is closed.
Creates a NEW fully verified primary recovery copy, repairs Git and links, then
runs fresh builds and the preservation verifier. Never kills apps or forces locks.
"""
import argparse,datetime,hashlib,importlib.util,json,os,shutil,subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
BASE=Path('C:/Dev/U5Native');STATE=BASE/'archive/CLEANUP3';OLD=Path('C:/Dev/OpenU5-Native');NEW=BASE/'source'
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--execute',action='store_true');a=ap.parse_args()
 if not a.execute:ap.error('Review the report, close ChatGPT/Codex and run this file with --execute from a separate terminal.')
 os.chdir(BASE)
 assert OLD.is_dir() and not NEW.exists(),'Expected old primary only; inspect state instead of overwriting'
 stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
 out=STATE/('finish-'+stamp);out.mkdir()
 spec=importlib.util.spec_from_file_location('migration',OLD/'docs/workspace/CLEANUP3/migrate.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 def invoke(args):subprocess.run([str(x) for x in args],cwd=BASE,env={**os.environ,'GIT_OPTIONAL_LOCKS':'0','PYTHONDONTWRITEBYTECODE':'1'},check=True)
 # All previous preservation conditions must still pass. New user development
 # needs a new plan/baseline, not an exception to this guard.
 invoke(['python','-B',OLD/'docs/workspace/CLEANUP3/verify_migration.py','prefinish-'+stamp])
 repos=[r for r in m.mapping() if r['kind'] in ('primary','repository','worktree')]
 snapshots={r['source']:m.snapshot(m.current(r)) for r in repos}
 (out/'git-before.json').write_text(json.dumps(snapshots,indent=2))
 required=sum(rec.get('bytes',0) for _,rec in m.walk(OLD))
 assert shutil.disk_usage(BASE).free>required+2*1024**3,'Insufficient room for independent primary recovery'
 backup=out/'primary-recovery';backup.mkdir()
 def copy_one(item):
  p,r=item;q=backup/r['path']
  if r['kind']=='dir':q.mkdir(parents=True,exist_ok=True)
  elif r['kind']=='file':
   q.parent.mkdir(parents=True,exist_ok=True);r['sha256']=m.digest(p);shutil.copy2(p,q)
   assert m.digest(q)==r['sha256'] and p.stat().st_mtime_ns==r['mtime_ns'],p
  return r
 with ThreadPoolExecutor(max_workers=12) as pool:records=list(pool.map(copy_one,m.walk(OLD),buffersize=96))
 assert {r['path'] for r in records if r['kind']!='link'}=={r['path'] for _,r in m.walk(backup)}
 assert m.git(backup,'for-each-ref','--format=%(refname)|%(objectname)')==snapshots[str(OLD)]['refs']
 assert not any(x.startswith('?') for x in m.git(backup,'rev-list','--objects','--all','--missing=print').splitlines())
 (out/'primary-files.json').write_text(json.dumps(records,indent=2))
 (out/'recovery-verified.json').write_text(json.dumps({'files':sum(r['kind']=='file' for r in records),'bytes':required,'passed':True}))
 worktrees=[Path(r['destination']) for r in m.mapping() if r['kind']=='worktree']
 moved=False
 try:
  OLD.rename(NEW);moved=True
  m.git(NEW,'worktree','repair',*[str(p) for p in worktrees]);m.retarget_links()
  def check_one(r):
   p=NEW/r['path']
   if r['kind']=='file':
    metadata=r['path'].startswith('.git/worktrees/') and r['path'].endswith('/gitdir')
    assert metadata or m.digest(p)==r['sha256'],p
   elif r['kind']=='link':assert p.exists() and os.readlink(p).lower()==m.live_target(r['target']).lower(),p
   else:assert p.is_dir(),p
  with ThreadPoolExecutor(max_workers=12) as pool:list(pool.map(check_one,records,buffersize=96))
  for r in repos:
   after=m.snapshot(m.current(r));before=snapshots[r['source']]
   assert all(after[k]==before[k] for k in before if k!='worktrees'),r
 except Exception as error:
  if moved:
   assert not OLD.exists();NEW.rename(OLD)
   m.git(OLD,'worktree','repair',*[str(p) for p in worktrees]);m.retarget_links()
  (out/'failure.json').write_text(json.dumps({'error':str(error),'primary_rolled_back':moved,'source':str(OLD)}))
  raise
 (STATE/'moves/00.json').write_text(json.dumps({'source':str(OLD),'destination':str(NEW),'method':'same-volume rename + git worktree repair','recovery':str(backup),'utc':stamp},indent=2))
 layout=json.loads((BASE/'workspace.json').read_text(encoding='utf-8-sig'));layout['primary']=str(NEW);layout['primary_migration']='complete; reopen Codex at this path';(BASE/'workspace.json').write_text(json.dumps(layout,indent=2))
 # All validation uses NEW output/log directories, never the earlier attempts.
 validation=(STATE/'validate-builds.ps1').read_text(encoding='utf-8-sig').replace('cleanup3-validation','cleanup3-finish-'+stamp).replace("'archive\\CLEANUP3\\tests'", "'archive\\CLEANUP3\\finish-"+stamp+"\\tests'")
 script=out/'validate-builds.ps1';script.write_text(validation)
 for suite in ('tooling','primary','host','windows','firmware'):invoke(['pwsh','-NoProfile','-File',script,'-Suite',suite])
 invoke(['python','-B',NEW/'docs/workspace/CLEANUP3/restore_drift_metadata.py','finish-'+stamp])
 (NEW/'docs/WORKSPACE_ORGANIZATION.md').write_text((STATE/'WORKSPACE_ORGANIZATION.new.md').read_text(encoding='utf-8-sig'))
 label='source-finished-'+stamp
 invoke(['python','-B',NEW/'docs/workspace/CLEANUP3/verify_migration.py',label])
 receipt=json.loads((STATE/('final-'+label+'.json')).read_text());assert receipt['remaining_old_standalone_u5_roots']==0
 import csv
 for name in ('actual-moves.csv','detailed-mapping.csv'):
  shutil.copy2(STATE/name,out/(name+'.before'))
 all_moves=[json.loads(p.read_text()) for p in sorted((STATE/'moves').glob('*.json'))]
 with (STATE/'actual-moves.csv').open('w',encoding='utf-8',newline='') as f:
  w=csv.DictWriter(f,fieldnames=['source','destination','method','utc'],extrasaction='ignore');w.writeheader();w.writerows(all_moves)
 with (STATE/'detailed-mapping.csv').open(encoding='utf-8',newline='') as f:details=list(csv.DictReader(f))
 for r in details:
  if r['kind'] in ('primary','worktree'):r['actual_common_git']=str(NEW/'.git');r['git_relationship']='Shared native .git in U5Native/source'
  if r['kind']=='primary':r['migration_status']='MOVED AND VERIFIED (offline)';r['actual_current_path']=str(NEW)
 with (STATE/'detailed-mapping.csv').open('w',encoding='utf-8',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(details[0]));w.writeheader();w.writerows(details)
 for name in ('actual-moves.csv','detailed-mapping.csv'):shutil.copy2(STATE/name,NEW/'docs/workspace/CLEANUP3'/name)
 report=NEW/'docs/workspace/CLEANUP3/REPORT.md'
 prefix=f'# CLEANUP3 — consolidation completed offline ({stamp})\n\nAll 25 mapped moves are now complete. 23 former standalone Ultima V roots are consolidated; C:\\Dev now has 12 top-level directories. The primary is C:\\Dev\\U5Native\\source. Fresh host, Windows, firmware and tooling validation passed again. Full preservation receipt: `{STATE/("final-"+label+".json")}`. Fresh logs/recovery: `{out}`. Reopen Codex at the new source path. Hardware acceptance remains deferred.\n\nThe earlier in-app checkpoint below is retained as historical evidence of the resolved lock.\n\n---\n\n'
 report.write_text(prefix+report.read_text(encoding='utf-8-sig'),encoding='utf-8')
 shutil.copy2(BASE/'README.md',out/'README.before.md')
 (BASE/'README.md').write_text('Ultima V Native is consolidated under this parent. The primary is source/; all 15 linked checkouts are in worktrees/. All 25 moves passed preservation checks and fresh host, Windows, firmware and tooling validation. See source/docs/WORKSPACE_ORGANIZATION.md and source/docs/workspace/CLEANUP3/REPORT.md. Recovery copies remain under archive/CLEANUP3. Hardware acceptance remains deferred. Reopen Codex at source/.\n',encoding='utf-8')
 (NEW/'docs/workspace/CLEANUP3/offline-completion.json').write_text(json.dumps({'passed':True,'receipt':str(STATE/('final-'+label+'.json')),'root_moves':25,'top_level_u5_roots_consolidated':23,'remaining_old_top_level_u5_roots':0,'recovery_and_tests':str(out)},indent=2),encoding='utf-8')
 print('Primary migration completed and validated. Reopen Codex at',NEW)
if __name__=='__main__':main()

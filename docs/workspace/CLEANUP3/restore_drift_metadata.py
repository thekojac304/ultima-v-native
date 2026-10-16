"""Preserve the 24 known same-byte drift-test outputs and restore original mtimes.
Never restores/overwrites file contents; refuses any content change or other path.
"""
import argparse,importlib.util,json,os,shutil
from pathlib import Path
BASE=Path('C:/Dev/U5Native');STATE=BASE/'archive/CLEANUP3'
PRIMARY=BASE/'source' if (BASE/'source').exists() else Path('C:/Dev/OpenU5-Native')
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('label');a=ap.parse_args();assert a.label.replace('-','').isalnum()
 spec=importlib.util.spec_from_file_location('m',PRIMARY/'docs/workspace/CLEANUP3/migrate.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 entries=m.read('post-test-timestamp-changes.json');assert len(entries)==24
 checked=[]
 for r in entries:
  assert r['root_index'] in (0,3) and any(r['relative'].startswith('native/core/'+x+'/') for x in ('build-quests','build-persistence','build-gameplay','build-dialogue'))
  root=m.current(m.mapping()[r['root_index']]);p=root/r['relative']
  assert not p.is_symlink() and not p.is_junction() and m.digest(p)==r['before_sha256']==r['after_sha256'],p
  checked.append((r,p,root))
 output=STATE/('drift-output-'+a.label);output.mkdir(exist_ok=False)
 restored=[]
 for r,p,root in checked:
  st=p.stat()
  if st.st_mtime_ns==r['before_mtime_ns']:continue
  copy=output/root.name/r['relative'];copy.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,copy)
  assert m.digest(copy)==r['before_sha256']
  os.utime(p,ns=(st.st_atime_ns,r['before_mtime_ns']))
  assert p.stat().st_mtime_ns==r['before_mtime_ns'] and m.digest(p)==r['before_sha256']
  restored.append({'path':str(p),'fresh_output_copy':str(copy),'sha256':r['before_sha256'],'fresh_mtime_ns':st.st_mtime_ns,'restored_original_mtime_ns':r['before_mtime_ns']})
 m.save('metadata-restoration-'+a.label+'.json',{'passed':True,'files':restored,'contents_changed':0})
 print(f'Preserved {len(restored)} same-byte drift outputs; original modification times restored. No contents rewritten.')
if __name__=='__main__':main()

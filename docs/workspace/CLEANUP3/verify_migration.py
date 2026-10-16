"""Verify CLEANUP3 full inventories, Git baselines, selected hashes and root reduction.
Reads frozen migration evidence; writes a new additive verification receipt.
"""
import argparse,csv,hashlib,importlib.util,json,os,subprocess,time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
BASE=Path('C:/Dev/U5Native')
STATE=BASE/'archive/CLEANUP3'
PRIMARY=BASE/'source' if (BASE/'source').exists() else Path('C:/Dev/OpenU5-Native')
spec=importlib.util.spec_from_file_location('migration',PRIMARY/'docs/workspace/CLEANUP3/migrate.py')
m=importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
APPROVED={'.gitignore','docs/WORKSPACE_ORGANIZATION.md','native/tools/cleanup_builds.py','docs/workspace/CLEANUP3/migrate.py'}
def verify():
    counts={'files':0,'directories':0,'links':0,'unchanged_file_bytes':0}
    root_results=[]
    for i,row in enumerate(m.mapping()):
        root=m.current(row)
        assert root.is_dir(),row
        assert row['kind']=='primary' and (STATE/'primary-blocker.json').exists() or root==Path(row['destination']),row
        label=f'{i:02d}-{Path(row["source"]).name}'
        records=m.read(f'manifests/{label}.json')
        expected={r['path']:r for r in records}
        actual={r['path']:(p,r) for p,r in m.walk(root)}
        assert not set(expected)-set(actual),row
        extra=set(actual)-set(expected)
        assert not extra or row['kind']=='primary' and all(x.startswith('docs/workspace/CLEANUP3/') for x in extra),sorted(extra)[:10]
        def one(item):
            rel,rec=item; p,now=actual[rel]
            assert rec['kind']==now['kind'],p
            if rec['kind']=='file':
                h=m.digest(p)
                pointer=rel=='.git' or row['kind']=='primary' and rel.startswith('.git/worktrees/') and rel.endswith('/gitdir')
                edited=row['kind']=='primary' and rel in APPROVED
                if not pointer and not edited:
                    assert h==rec['sha256'],f'Changed bytes: {p}'
                    assert rec['mtime_ns']==now['mtime_ns'],f'Changed timestamp: {p}'
                return (rec['kind'],rec['bytes'],rel if h!=rec['sha256'] else None)
            if rec['kind']=='link':
                assert now['target'].lower()==m.live_target(rec['target']).lower(),p
                assert p.exists(),f'Broken junction: {p}'
            return (rec['kind'],0,None)
        with ThreadPoolExecutor(max_workers=12) as pool:
            checked=list(pool.map(one,expected.items(),buffersize=96))
        changed=[x[2] for x in checked if x[2]]
        for kind,size,change in checked:
            counts[{'file':'files','dir':'directories','link':'links'}[kind]]+=1
            if not change: counts['unchanged_file_bytes']+=size
        root_results.append({'root':str(root),'entries':len(records),'expected_changes':changed})
        print(f'FULL INVENTORY VERIFIED: {root}',flush=True)
    git_results=[]
    before=m.read('git-before.json')
    for row in m.mapping():
        if row['source'] not in before: continue
        root=m.current(row); b=before[row['source']]; a=m.snapshot(root)
        for key in ('head','branch','staged','refs','stashes','reflogs'):
            assert a[key]==b[key],(root,key)
        if row['kind']=='primary':
            def filter_status(s):
                return '\n'.join(x for x in s.splitlines() if x[3:] not in APPROVED and not x[3:].startswith('docs/workspace/CLEANUP3/'))
            assert filter_status(a['status'])==filter_status(b['status']),root
            changed=m.git(root,'diff','--name-only').splitlines()
            assert set(changed)<=APPROVED,changed
            assert not b['diff'], 'Unexpected initial primary diff needs explicit comparison'
            def ignore_list(s): return '\n'.join(x for x in s.splitlines() if not x.startswith('docs/workspace/CLEANUP3/local/'))
            assert ignore_list(a['ignored'])==ignore_list(b['ignored']),root
        else:
            for key in ('status','diff','ignored'):
                assert a[key]==b[key],(root,key)
        if row['kind']!='repository':
            common=m.git(root,'rev-parse','--path-format=absolute','--git-common-dir').strip()
            assert Path(common)==PRIMARY/'.git',(root,common)
        git_results.append({'root':str(root),'baseline_passed':True})
    registration=m.git(PRIMARY,'worktree','list','--porcelain')
    expected_paths={m.current(x) for x in m.mapping() if x['kind'] in ('primary','worktree')}
    registered={Path(x[9:]) for x in registration.splitlines() if x.startswith('worktree ')}
    assert registered==expected_paths and len(registered)==16
    assert 'prunable' not in registration and 'locked' not in registration
    # Verify CLEANUP2's 774 selected files and all frozen inventory checksums,
    # translated through the exact migration mapping, without rewriting evidence.
    snap=json.loads((PRIMARY/'docs/workspace/CLEANUP2/local/cleanup3-entry.json').read_text())
    selected=[]
    def mapped(path):
        p=Path(path)
        for r in sorted(m.mapping(),key=lambda x:-len(x['source'])):
            old=Path(r['source'])
            if p.is_relative_to(old):return m.current(r)/p.relative_to(old)
        raise AssertionError(path)
    for p,rec in snap['files'].items():
        new=mapped(p)
        assert new.stat().st_size==rec['bytes'] and m.digest(new)==rec['sha256'],new
        selected.append(str(new))
    census=PRIMARY/'docs/workspace/CLEANUP1/inventory'
    for name,sha in snap['inventories'].items():assert m.digest(census/name)==sha,name
    packages=list(csv.DictReader((census/'release-catalog.csv').open(encoding='utf-8-sig',newline='')))
    for r in packages:assert m.digest(mapped(r['path']))==r['sha256'],r['path']
    dev_before={r['name'] for r in m.read('dev-before.json')}
    moved={Path(r['source']).name for r in m.mapping() if Path(r['source']).parent==Path('C:/Dev') and Path(r['destination']).exists()}
    dev_after={p.name for p in Path('C:/Dev').iterdir()}
    assert dev_after==(dev_before-moved)|{'U5Native'},(dev_before,dev_after)
    # Connectivity diagnostics are evidence, including the unchanged origin/HEAD warning.
    connectivity=[]
    for root in (PRIMARY,BASE/'archive/openu5'):
        p=subprocess.run(['git','--no-optional-locks','-C',str(root),'fsck','--connectivity-only','--no-dangling'],capture_output=True,text=True)
        missing=m.git(root,'rev-list','--objects','--all','--missing=print')
        assert not any(x.startswith('?') for x in missing.splitlines()),root
        assert p.returncode==(2 if root==PRIMARY else 0),(root,p.stderr)
        if p.returncode:assert 'refs/remotes/origin/HEAD' in p.stderr and len(p.stderr.splitlines())==1,p.stderr
        connectivity.append({'root':str(root),'fsck_exit':p.returncode,'diagnostic':p.stderr,'missing_reachable_objects':0})
    return {'passed':True,'counts':counts,'roots':root_results,'git':git_results,
            'selected_hashes':len(selected),'frozen_inventories':len(snap['inventories']),
            'release_catalog_hashes':len(packages),'native_registrations':len(registered),
            'dev_before':sorted(dev_before),'dev_after':sorted(dev_after),'root_folders_consolidated':len(moved),
            'remaining_old_standalone_u5_roots':int(PRIMARY==Path('C:/Dev/OpenU5-Native')),'connectivity':connectivity,'utc':time.time()}
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('label');args=p.parse_args()
    assert args.label.replace('-','').isalnum()
    result=verify();m.save('final-'+args.label+'.json',result)
    print(json.dumps({k:v for k,v in result.items() if k not in {'roots','git','connectivity','dev_before','dev_after'}},indent=2))

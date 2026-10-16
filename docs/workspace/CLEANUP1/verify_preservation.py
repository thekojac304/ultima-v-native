"""Verify CLEANUP1 preservation and RC1 provenance without modifying inputs.

This checks retained receipts and bytes, not new hardware acceptance. Writes only
the verification receipt and a copy of the pre-existing unstaged diff to inventory/.
"""
import csv
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import zipfile

sys.dont_write_bytecode = True
DATA = Path(__file__).resolve().parent / 'inventory'
RC = Path('C:/Dev/ultima-v-native-rc1')
EXPECTED = 'a96656b2330097c18c52b78a8840d3749c0e9854'


def git(root, *args):
    p = subprocess.run(['git','--no-optional-locks','-C',str(root),*args],capture_output=True,
                       encoding='utf-8',errors='replace',env={**os.environ,'GIT_OPTIONAL_LOCKS':'0'})
    return dict(exit=p.returncode,out=p.stdout,err=p.stderr)


def sha(p):
    h = hashlib.sha256()
    with open(p,'rb') as f:
        for b in iter(lambda:f.read(4*1024*1024),b''):
            h.update(b)
    return h.hexdigest()


def main():
    before = json.loads((DATA/'git-and-workspace.json').read_text())
    checks = []
    def check(name, value, details=None):
        checks.append(dict(check=name,passed=bool(value),details=details))
    check('RC1 HEAD',git(RC,'rev-parse','HEAD')['out'].strip()==EXPECTED)
    check('RC1 branch',git(RC,'branch','--show-current')['out'].strip()=='release/v0.5.0-rc1')
    check('RC1 source clean',not git(RC,'status','--porcelain=v1','-uall')['out'])
    source = RC/'native/targets/tdeck/dist'
    evidence = source/'v050-rc1-evidence'
    manifest_path = source/'v050-rc1-manifest.json'
    manifest = json.loads(manifest_path.read_text())
    check('Retained manifest full commit',manifest['git_commit']==EXPECTED)
    check('Retained final source receipt',(evidence/'final-source-commit.txt').read_text().strip()==EXPECTED)
    for key, a in manifest['artifacts'].items():
        p=Path(a['path'])
        check(key+' size and SHA-256',p.stat().st_size==a['bytes'] and sha(p)==a['sha256'],a)
    zip_path=Path(manifest['artifacts']['windows_zip']['path'])
    sys.path.insert(0,str(RC/'native/tools/rc1'))
    from release_manifest import verify_windows_build_info, validate
    with zipfile.ZipFile(zip_path) as z:
        check('RC1 ZIP CRC',z.testzip() is None)
        allowed={'UltimaVNative.exe','SDL3.dll','README-Windows.txt','CONTROLS.txt','Prepare-Game-Data.cmd',
                 'prepare-assets.mjs','LICENSE.txt','NOTICE.txt','THIRD-PARTY-LICENSES.txt','BUILD-INFO.txt'}
        check('RC1 ZIP whitelist',set(z.namelist())==allowed and len(z.namelist())==len(allowed))
        info=z.read('BUILD-INFO.txt').decode('utf-8-sig')
        verify_windows_build_info(info,EXPECTED)
        check('Windows BUILD-INFO exact version/commit',True,info.split('SHA-256')[0])
        exe=z.read('UltimaVNative.exe')
        pe=struct.unpack_from('<I',exe,0x3c)[0]
        check('Windows x64 PE and embedded identity',exe[:2]==b'MZ' and exe[pe:pe+4]==b'PE\0\0'
              and struct.unpack_from('<H',exe,pe+4)[0]==0x8664 and EXPECTED[:12].encode() in exe
              and (EXPECTED[:12]+'-dirty').encode() not in exe and b'0.5.0-rc1\0' in exe)
        build_matches=[]
        for build in (RC/'native/targets/windows').glob('build*'):
            if (build/'UltimaVNative.exe').exists() and (build/'SDL3.dll').exists():
                if (build/'UltimaVNative.exe').read_bytes()==exe and (build/'SDL3.dll').read_bytes()==z.read('SDL3.dll'):
                    build_matches.append(str(build))
        check('Windows ZIP matches retained built EXE and DLL',bool(build_matches),build_matches)
    firmware=Path(manifest['artifacts']['tdeck_launcher_bin']['path']).read_bytes()
    validate(firmware)
    check('Firmware ESP checksum/header and exact version/identity',firmware[48:80].split(b'\0')[0]==b'0.5.0-rc1'
          and EXPECTED[:12].encode() in firmware and (EXPECTED[:12]+'-dirty').encode() not in firmware)
    check('Firmware equals optimized build app',firmware==(RC/'native/targets/tdeck/build-rc1-optimized/openu5_tdeck.bin').read_bytes())
    for target,name,total in [('host','final-host-ctest.log',212),('windows','final-windows-build-test.log',38)]:
        log=evidence/name
        check(target+' retained test receipt hash',sha(log)==manifest['tests'][target]['log_sha256'])
        check(target+' retained full suite count',bool(re.search(r'100% tests passed(?:, 0 tests failed)? out of '+str(total)+r'\b',log.read_text(errors='replace'))),
              f'{total}/{total}; historical receipt verified, suite not rerun')
    check('Retained TypeScript 11/11 receipt',bool(re.search(r'Tests\s+11 passed \(11\)',(evidence/'final-typescript.log').read_text(errors='replace'))))
    tool_log=(evidence/'final-tooling-regression.log').read_text(errors='replace')
    check('Retained tooling 5/5 receipt','Ran 5 tests' in tool_log and bool(re.search(r'^OK\s*$',tool_log,re.M)))
    for mode in ('paced','unpaced'):
        traces=[evidence/f'reference-{mode}.txt',evidence/f'final-differential/reference-{mode}.txt',evidence/f'final-differential/handheld-{mode}.txt']
        hashes=[sha(p) for p in traces]
        check(mode+' frozen/final trace byte identity',len(set(hashes))==1 and hashes[0]==manifest['tests']['differential'][mode]['sha256'],hashes)
    check('SPP1 rollback identity',sha(Path('C:/Dev/spp1-hw-package/OpenU5-TDeck-SPP1-5ba2fa46b74f-Launcher.bin'))=='1aeecb70dec68141122e4aa88a9ce140cad87a617ff5668ec3eb8fa1d8b123c2')
    post=[]
    for r in before['repositories']:
        if r['scope']!='full':continue
        root=Path(r['path'])
        now={k:git(root,*args) for k,args in {
            'head':('rev-parse','HEAD'), 'status':('status','--porcelain=v1','-uall'),
            'branches':('for-each-ref','--format=%(refname)|%(objectname)|%(upstream)|%(upstream:track)','refs/heads','refs/remotes'),
            'worktrees':('worktree','list','--porcelain'),'stashes':('stash','list')}.items()}
        check(str(root)+' HEAD, refs, worktrees and stashes unchanged',all(now[k]['out']==r[k]['out'] and now[k]['exit']==r[k]['exit'] for k in ['head','branches','worktrees','stashes']))
        if str(root)!='C:\\Dev\\OpenU5-Native':
            check(str(root)+' status unchanged',now['status']['out']==r['status']['out'])
        else:
            original=[l for l in r['status']['out'].splitlines() if 'docs/workspace/CLEANUP1/' not in l]
            current=[l for l in now['status']['out'].splitlines() if 'docs/workspace/CLEANUP1/' not in l and 'docs/WORKSPACE_ORGANIZATION.md' not in l and l[3:]!='.gitignore']
            check('Primary original status preserved; only audit docs and narrow ignore change added',original==current)
            check('Primary index unchanged',not git(root,'diff','--cached','--name-only')['out'])
        post.append(dict(path=str(root),**now))
    # Stat every original file; rehash every existing untracked file. This supplements
    # Git status without re-reading gigabytes of immutable build outputs.
    missing, changed, local_hash_mismatch=[],[],[]
    with (DATA/'files.csv').open(encoding='utf-8-sig',newline='') as f:
        rows=list(csv.DictReader(f))
    for row in rows:
        p=Path(row['path'])
        if not p.is_file():
            missing.append(str(p));continue
        st=p.stat()
        if st.st_size!=int(row['bytes']) or st.st_mtime_ns!=int(row['mtime_ns']):
            if str(p)!='C:\\Dev\\OpenU5-Native\\.gitignore':changed.append(str(p))
        if row['git_state']=='untracked' and row['sha256'] and sha(p)!=row['sha256']:
            local_hash_mismatch.append(str(p))
    check('All inventoried files remain accessible',not missing,missing)
    check('Inventoried sizes/mtimes unchanged except approved ignore edit',not changed,changed)
    check('All pre-existing untracked file hashes unchanged',not local_hash_mismatch,local_hash_mismatch)
    dos=Path('C:/Dev/uvw-dos2-base')
    patch=git(dos,'diff','--binary')['out']
    (DATA/'preexisting-dos2-unstaged.patch').write_text(patch,encoding='utf-8')
    check('DOS-base pre-existing edit still limited to CMakeLists',git(dos,'diff','--name-only')['out'].strip()=='native/core/CMakeLists.txt')
    # A raw Git connectivity check has an existing dangling origin/HEAD. Keep the
    # actual diagnostic and separately check reachable objects; never repair refs.
    connectivity=git(RC,'fsck','--connectivity-only','--no-dangling')
    reachability=git(RC,'rev-list','--objects','--all','--missing=print')
    check('No missing objects reachable from retained refs',reachability['exit']==0 and not any(l.startswith('?') for l in reachability['out'].splitlines()))
    result=dict(time_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),checks=checks,
                passed=sum(x['passed'] for x in checks),failed=sum(not x['passed'] for x in checks),
                final_repositories=post,git_connectivity_diagnostic=connectivity,
                origin_head=git(RC,'symbolic-ref','refs/remotes/origin/HEAD'),
                hardware_validation='NOT PERFORMED; RC acceptance pending',
                validation_scope='Preservation checks and retained receipt verification; no gameplay source changed')
    (DATA/'preservation-verification.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps({'passed':result['passed'],'failed':result['failed'],
                      'failures':[x for x in checks if not x['passed']],
                      'git_connectivity_diagnostic':connectivity},indent=2))
    if result['failed']:raise SystemExit(1)


if __name__=='__main__':main()

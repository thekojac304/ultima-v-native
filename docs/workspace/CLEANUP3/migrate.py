"""CLEANUP3 staged, fail-closed local migration. No pruning or history changes.

Machine evidence lives outside the moving checkout. Recovery copies are physical
copies (never hardlinks); junctions are captured as metadata, never traversed.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
import stat
from concurrent.futures import ThreadPoolExecutor

DEV = Path('C:/Dev')
BASE = DEV / 'U5Native'
STATE = BASE / 'archive/CLEANUP3'
RECOVERY = STATE / 'recovery'
OLD = DEV / 'OpenU5-Native'
NEW = BASE / 'source'


def save(name, value):
    p = STATE / name
    p.parent.mkdir(parents=True, exist_ok=True)
    with p.open('x', encoding='utf-8', newline='\n') as f:
        json.dump(value, f, indent=2)


def read(name):
    return json.loads((STATE / name).read_text(encoding='utf-8'))


def run(args, cwd=DEV, accept=(0,)):
    p = subprocess.run([str(a) for a in args], cwd=cwd, capture_output=True,
                       encoding='utf-8', errors='replace',
                       env={**os.environ, 'GIT_OPTIONAL_LOCKS': '0'})
    if p.returncode not in accept:
        raise RuntimeError(f'{args}: exit {p.returncode}\n{p.stdout}\n{p.stderr}')
    return p.stdout


def git(root, *args):
    return run(['git', '--no-optional-locks', '-C', root, *args])


def digest(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda: f.read(4 * 1024 * 1024), b''):
            h.update(b)
    return h.hexdigest()


def walk(root):
    """Yield physical entries including empty dirs; never descend reparse points."""
    stack = [root]
    while stack:
        parent = stack.pop()
        for entry in sorted(os.scandir(parent), key=lambda e: e.name):
            p = Path(entry.path)
            rel = p.relative_to(root).as_posix()
            s = entry.stat(follow_symlinks=False)
            if s.st_file_attributes & 0x400:
                yield p, {'path': rel, 'kind': 'link', 'target': os.readlink(p)}
            elif stat.S_ISDIR(s.st_mode):
                yield p, {'path': rel, 'kind': 'dir'}
                stack.append(p)
            elif stat.S_ISREG(s.st_mode):
                yield p, {'path': rel, 'kind': 'file', 'bytes': s.st_size,
                          'mtime_ns': s.st_mtime_ns}
            else:
                raise RuntimeError(f'Unsupported filesystem entry: {p}')


def snapshot(root):
    return {key: git(root, *args) for key, args in {
        'head': ['rev-parse', 'HEAD'], 'branch': ['branch', '--show-current'],
        'status': ['status', '--porcelain=v1', '-uall'],
        'diff': ['diff', '--binary'], 'staged': ['diff', '--cached', '--binary'],
        'refs': ['for-each-ref', '--format=%(refname)|%(objectname)'],
        'stashes': ['stash', 'list', '--format=%H %gs'],
        'ignored': ['ls-files', '--others', '--ignored', '--exclude-standard'],
        'worktrees': ['worktree', 'list', '--porcelain'],
        'reflogs': ['reflog', 'show', '--all', '--format=%H %gD %gs'],
    }.items()}


def mapping():
    return read('mapping.json')


def current(row):
    a, b = Path(row['source']), Path(row['destination'])
    if a.exists() == b.exists():
        raise RuntimeError(f'Expected exactly one live root: {a}, {b}')
    return a if a.exists() else b


def translate(text):
    # Root boundary matching prevents accidentally rewriting an unrelated prefix.
    import re
    for row in sorted(mapping(), key=lambda x: -len(x['source'])):
        old = row['source'].replace('\\', '/')
        pattern = re.escape(old).replace('/', r'[/\\]') + r'(?=$|[/\\])'
        text = re.sub(pattern, lambda m: row['destination'], text, flags=re.I)
    return text


def live_target(target):
    import ntpath
    prefix = '\\\\?\\' if target.startswith('\\\\?\\') else ''
    clean = target[len(prefix):].replace('/', '\\')
    for row in sorted(mapping(), key=lambda x: -len(x['source'])):
        old = row['source'].replace('/', '\\')
        if clean.lower() == old.lower() or clean.lower().startswith(old.lower() + '\\'):
            return prefix + str(current(row)) + clean[len(old):]
    return target


def retarget_links():
    for i, row in enumerate(mapping()):
        root = current(row)
        name = f'{i:02d}-{Path(row["source"]).name}'
        for rec in read(f'manifests/{name}.json'):
            if rec['kind'] != 'link': continue
            p = root / rec['path']
            target = live_target(rec['target'])
            if os.readlink(p).lower() == target.lower(): continue
            assert p.is_junction(), f'Refuse unknown reparse type: {p}'
            old_target = os.readlink(p)
            os.rmdir(p)  # Removes only this verified junction, never its target.
            import _winapi
            try:
                _winapi.CreateJunction(target.removeprefix('\\\\?\\'), str(p))
            except Exception:
                _winapi.CreateJunction(old_target.removeprefix('\\\\?\\'), str(p))
                raise
            assert p.exists(), f'Unresolved new target: {p} -> {target}'


def prepare():
    if STATE.exists():
        raise RuntimeError('Preparation already exists; inspect rather than overwrite')
    census = OLD / 'docs/workspace/CLEANUP1/inventory'
    meta = json.loads((census / 'git-and-workspace.json').read_text())
    repos = [r for r in meta['repositories'] if r['scope'] == 'full']
    native = git(OLD, 'worktree', 'list', '--porcelain')
    rows = []
    for r in repos:
        src = Path(r['path'])
        dst = (NEW if src == OLD else BASE / ('archive/openu5' if src.name == 'openu5'
                    else 'worktrees/' + src.name))
        rows.append({'source': str(src), 'destination': str(dst),
                     'kind': 'primary' if src == OLD else 'repository' if src.name == 'openu5' else 'worktree',
                     'batch': 'git' if src == OLD or src.name != 'openu5' else 'archive'})
    for name, dst, kind, batch in [
        ('spp1-hw-package', 'releases/historical/spp1-hw-package', 'hardware rollback package', 'artifacts'),
        ('uvw-look-polish-evidence', 'releases/historical/uvw-look-polish-evidence', 'acceptance evidence', 'artifacts'),
        ('uvw-original-dos-evidence', 'releases/historical/uvw-original-dos-evidence', 'acceptance evidence', 'artifacts'),
        ('spp2-scratch', 'archive/investigations/spp2-scratch', 'unique investigation', 'artifacts'),
        ('_Archive/u5-ti89', 'archive/u5-ti89', 'historical TI project/tools', 'archive'),
        ('ultima-v-archive', 'archive/legacy-layout', 'CLEANUP2 archive convention', 'layout'),
        ('ultima-v-builds', 'builds/legacy-layout', 'CLEANUP2 builds convention', 'builds'),
        ('openu5-release-artifacts', 'releases/legacy-layout', 'CLEANUP2 releases convention', 'layout'),
    ]:
        rows.append({'source': str(DEV / name), 'destination': str(BASE / dst), 'kind': kind, 'batch': batch})
    for r in rows:
        assert Path(r['source']).is_dir(), r
        assert not Path(r['destination']).exists(), r
        assert Path(r['destination']).is_relative_to(BASE), r
    assert len(rows) == 25
    assert len([x for x in rows if Path(x['source']).parent == DEV]) == 23
    STATE.mkdir(parents=True)
    save('mapping.json', rows)
    save('dev-before.json', [{'name': p.name, 'link': os.readlink(p) if p.is_symlink() or p.is_junction() else None}
                             for p in sorted(DEV.iterdir()) if p != BASE])
    with (STATE / 'mapping.csv').open('x', encoding='utf-8', newline='') as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)
    # Reuse existing census and immutable historical snapshots for dependency evidence.
    shutil.copy2(census / 'path-dependencies-all.txt', STATE / 'historical-path-dependencies.txt')
    shutil.copy2(census / 'reparse-points.csv', STATE / 'historical-reparse-points.csv')
    before = {r['source']: snapshot(Path(r['source'])) for r in rows if r['kind'] in ('primary', 'repository', 'worktree')}
    assert before[str(DEV / '_Archive/openu5')]['stashes'].startswith('81211c6226aaa94a72145d040f51f345f4479dbb ')
    assert 'wip3' in before[str(DEV / '_Archive/openu5')]['stashes']
    assert git(DEV / 'ultima-v-native-rc1', 'rev-parse', 'HEAD').strip() == 'a96656b2330097c18c52b78a8840d3749c0e9854'
    save('git-before.json', before)
    (STATE / 'worktrees-before.txt').write_text(native)
    save('capacity.json', {'free_bytes': shutil.disk_usage(DEV).free,
                          'recovery': str(RECOVERY), 'same_volume': True,
                          'note': 'Independent physical copy, not protection against C: device loss'})
    print(f'Prepared {len(rows)} exact moves, 23 top-level roots, 17 Git baselines', flush=True)


def backup():
    if not (STATE / 'rehearsal.json').exists():
        raise RuntimeError('Rollback rehearsal required first')
    for i, row in enumerate(mapping()):
        label = f'{i:02d}-{Path(row["source"]).name}'
        manifest = f'manifests/{label}.json'
        if (STATE / manifest).exists():
            print(f'Already verified: {label}', flush=True); continue
        src, dst = Path(row['source']), RECOVERY / label
        dst.mkdir(parents=True, exist_ok=True)
        def copy_entry(item):
            p, rec = item
            out = dst / rec['path']
            if rec['kind'] == 'dir':
                out.mkdir(parents=True, exist_ok=True)
            elif rec['kind'] == 'file':
                out.parent.mkdir(parents=True, exist_ok=True)
                rec['sha256'] = digest(p)
                if not out.exists():
                    shutil.copy2(p, out)
                elif digest(out) != rec['sha256']:
                    # A stopped writer may leave one partial backup file. The
                    # source is untouched; safely recopy only within this exact
                    # recovery root and never follow a reparse point.
                    assert out.is_relative_to(RECOVERY) and not out.is_symlink() and not out.is_junction()
                    shutil.copy2(p, out)
                assert digest(out) == rec['sha256'], p
                assert p.stat().st_mtime_ns == rec['mtime_ns'], f'Changed while copying: {p}'
            return rec
        with ThreadPoolExecutor(max_workers=12) as pool:
            records = list(pool.map(copy_entry, walk(src), buffersize=96))
        # Backup inventory is verified independently, including empty directories.
        expected = {r['path'] for r in records if r['kind'] != 'link'}
        actual = {r['path'] for _, r in walk(dst)}
        assert expected == actual, label
        save(manifest, records)
        save(f'backup/{label}.json', {'source': str(src), 'copy': str(dst),
             'files': sum(r['kind'] == 'file' for r in records),
             'bytes': sum(r.get('bytes', 0) for r in records),
             'links_recorded_not_followed': sum(r['kind'] == 'link' for r in records),
             'manifest_sha256': digest(STATE / manifest)})
        print(f'BACKUP VERIFIED {label}: {len(records)} entries', flush=True)
    save('backup-complete.json', {'roots': len(mapping()), 'utc': time.time()})


def rehearsal():
    root = STATE / 'rollback-rehearsal'
    root.mkdir()
    a, b = root / 'before', root / 'after'
    a.mkdir()
    git(a, 'init')
    (a / 'tracked').write_text('base\n')
    git(a, 'add', 'tracked')
    git(a, '-c', 'user.name=CLEANUP3', '-c', 'user.email=local@example.invalid', 'commit', '-m', 'rehearsal')
    wt1, wt2 = root / 'wt-before', root / 'wt-after'
    git(a, 'worktree', 'add', '--detach', str(wt1))
    (wt1 / 'tracked').write_text('uncommitted\n')
    (wt1 / 'private').write_bytes(b'ignored/untracked recovery test')
    git(a, 'worktree', 'move', str(wt1), str(wt2))
    a.rename(b)
    git(b, 'worktree', 'repair', str(wt2))
    assert git(wt2, 'diff') and (wt2 / 'private').is_file()
    b.rename(a)
    git(a, 'worktree', 'repair', str(wt2))
    git(a, 'worktree', 'move', str(wt2), str(wt1))
    assert git(wt1, 'diff') and (wt1 / 'private').read_bytes() == b'ignored/untracked recovery test'
    save('rehearsal.json', {'passed': True, 'procedure': 'git worktree move; rename primary; git worktree repair; inverse in reverse order',
                           'root': str(root), 'git_version': git(a, '--version').strip()})
    print('Git migration and inverse rollback rehearsal passed', flush=True)


def verify_root(i, row, label):
    src = current(row)
    name = f'{i:02d}-{Path(row["source"]).name}'
    records = read(f'manifests/{name}.json')
    expected = {r['path']: r for r in records}
    actual = {r['path']: (p, r) for p, r in walk(src)}
    # Source's CLEANUP3 report/tools can be added after baseline, but nothing else.
    extra = set(actual) - set(expected)
    assert not extra or row['kind'] == 'primary' and all(x.startswith('docs/workspace/CLEANUP3') for x in extra), sorted(extra)[:10]
    assert not set(expected) - set(actual), sorted(set(expected) - set(actual))[:10]
    def verify_entry(item):
        rel, rec = item
        p, now = actual[rel]
        assert rec['kind'] == now['kind'], p
        if rec['kind'] == 'file':
            h = digest(p)
            if h != rec['sha256']:
                # Only Git's relocated pointers may change during the move phase.
                allowed = rel == '.git' or row['kind'] == 'primary' and rel.startswith('.git/worktrees/') and rel.endswith('/gitdir')
                assert allowed, f'Unexpected changed bytes: {p}'
                return rel
            assert now['mtime_ns'] == rec['mtime_ns'] or rel == '.git' or row['kind'] == 'primary' and rel.startswith('.git/worktrees/') and rel.endswith('/gitdir'), f'Unexpected changed timestamp: {p}'
        elif rec['kind'] == 'link':
            assert now['target'].lower() == live_target(rec['target']).lower(), p
        return None
    with ThreadPoolExecutor(max_workers=12) as pool:
        changed = [x for x in pool.map(verify_entry, expected.items(), buffersize=96) if x]
    return {'source': row['source'], 'current': str(src), 'files_verified': sum(r['kind'] == 'file' for r in records),
            'entries_verified': len(records), 'git_pointer_updates': changed}


def verify_git():
    before = read('git-before.json')
    results = []
    for row in mapping():
        if row['source'] not in before:
            continue
        p = current(row)
        after = snapshot(p)
        b = before[row['source']]
        for key in ('head', 'branch', 'status', 'diff', 'staged', 'refs', 'stashes', 'ignored', 'reflogs'):
            assert after[key] == b[key], f'Git baseline changed: {p}: {key}'
        entries = [x[9:] for x in after['worktrees'].splitlines() if x.startswith('worktree ')]
        for entry in entries:
            assert Path(entry).is_dir(), entry
            assert git(Path(entry), 'rev-parse', '--git-common-dir').strip(), entry
        results.append({'root': str(p), 'baseline_fields_identical': True, 'registrations_resolve': len(entries)})
    return results


def move_batch(batch):
    assert (STATE / 'backup-complete.json').exists(), 'Recovery required'
    assert (STATE / 'recovery-restore.json').exists(), 'Restoration verification required'
    rows = mapping()
    for i, row in enumerate(rows):
        selected = row['batch'] == batch and row['kind'] != 'primary'
        if batch == 'primary':
            selected = row['kind'] == 'primary'
        if not selected:
            continue
        src, dst = Path(row['source']), Path(row['destination'])
        if not src.exists():
            assert dst.is_dir() and (STATE / f'moves/{i:02d}.json').exists(), f'Unjournaled destination: {dst}'
            retarget_links()
            receipt = STATE / f'verification/{i:02d}-moved.json'
            result = verify_root(i, row, 'resume')
            if not receipt.exists(): save(f'verification/{i:02d}-moved.json', result)
            print(f'ALREADY MOVED, REVERIFIED {dst}', flush=True)
            continue
        assert not dst.exists(), dst
        # Source inventory and Git must still equal baseline immediately before moving.
        verify_root(i, row, 'before')
        if row['source'] in read('git-before.json'):
            before_git = read('git-before.json')[row['source']]
            now_git = snapshot(src)
            assert all(now_git[k] == before_git[k] for k in now_git if k != 'worktrees'), 'Git changed since baseline'
        dst.parent.mkdir(parents=True, exist_ok=True)
        if row['kind'] == 'worktree':
            git(OLD if OLD.exists() else NEW, 'worktree', 'move', str(src), str(dst))
        else:
            assert src.resolve() == src and src.is_relative_to(DEV), src
            src.rename(dst)
        if row['kind'] == 'primary':
            git(NEW, 'worktree', 'repair', *[str(current(r)) for r in rows if r['kind'] == 'worktree'])
        save(f'moves/{i:02d}.json', {'source': str(src), 'destination': str(dst), 'utc': time.time(),
                'method': 'git worktree move' if row['kind'] == 'worktree' else 'rename + git worktree repair' if row['kind'] == 'primary' else 'same-volume rename'})
        retarget_links()
        save(f'verification/{i:02d}-moved.json', verify_root(i, row, 'after'))
        print(f'MOVED AND VERIFIED {src} -> {dst}', flush=True)
    # Link targets get repaired only after coordinated root moves; byte checks
    # happen now, accessibility is checked once the new target graph is complete.
    save(f'batches/{batch}.json', {'git': verify_git(), 'utc': time.time()})
    print(f'BATCH {batch} VERIFIED', flush=True)


def restore_test():
    assert (STATE / 'backup-complete.json').exists()
    # Restore the independent archived Git store into an isolated path. Physical
    # files and refs/stash are verified; links remain captured metadata.
    row = next(r for r in mapping() if r['kind'] == 'repository')
    i = mapping().index(row)
    label = f'{i:02d}-openu5'
    restored = STATE / 'restore-test/openu5'
    shutil.copytree(RECOVERY / label, restored)
    records = read(f'manifests/{label}.json')
    for r in records:
        if r['kind'] == 'file':
            assert digest(restored / r['path']) == r['sha256']
    assert git(restored, 'stash', 'list', '--format=%H %gs') == read('git-before.json')[row['source']]['stashes']
    assert git(restored, 'cat-file', '-t', '81211c6226aaa94a72145d040f51f345f4479dbb').strip() == 'commit'
    git(restored, 'fsck', '--connectivity-only', '--no-dangling')
    # Restore and compare DOS-base working files, including its modified CMake and
    # untracked probe, from its independently hashed recovery copy.
    dos = next(r for r in mapping() if Path(r['source']).name == 'uvw-dos2-base')
    j = mapping().index(dos)
    dos_restored = STATE / 'restore-test/dos2'
    shutil.copytree(RECOVERY / f'{j:02d}-uvw-dos2-base', dos_restored)
    for r in read(f'manifests/{j:02d}-uvw-dos2-base.json'):
        if r['kind'] == 'file':
            assert digest(dos_restored / r['path']) == r['sha256']
    save('recovery-restore.json', {'passed': True, 'archived_stash': '81211c6226aaa94a72145d040f51f345f4479dbb',
                                  'restored': [str(restored), str(dos_restored)],
                                  'note': 'Git migration/inverse separately exercised by rehearsal; no live metadata modified'})
    print('Physical restore and stash/DOS-base verification passed', flush=True)


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('operation', choices=['prepare', 'rehearsal', 'backup', 'restore-test', 'move'])
    ap.add_argument('--batch', choices=['artifacts', 'archive', 'layout', 'builds', 'git', 'primary'])
    args = ap.parse_args()
    if args.operation == 'move':
        if not args.batch: ap.error('--batch required')
        move_batch(args.batch)
    else:
        {'prepare': prepare, 'rehearsal': rehearsal, 'backup': backup, 'restore-test': restore_test}[args.operation]()

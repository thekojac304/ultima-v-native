"""CLEANUP1 read-only collector. Writes only to --output; never mutates Git or inputs.

Run from PowerShell: python docs/workspace/CLEANUP1/audit_workspace.py --output <new-directory>
Do not use this as a deletion tool. Reparse points are inventoried, never traversed.
"""
import argparse
import collections
import csv
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


def git(root, *args):
    p = subprocess.run(['git', '--no-optional-locks', '-C', str(root), *args],
                       capture_output=True, encoding='utf-8', errors='replace',
                       env={**os.environ, 'GIT_OPTIONAL_LOCKS': '0'})
    return {'exit': p.returncode, 'out': p.stdout, 'err': p.stderr}


def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(4 * 1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def csvwrite(path, rows, fields):
    with open(path, 'w', newline='', encoding='utf-8-sig') as f:
        writer = csv.DictWriter(f, fieldnames=fields, extrasaction='ignore')
        writer.writeheader()
        writer.writerows(rows)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--output', required=True)
    args = ap.parse_args()
    out = Path(args.output).resolve()
    out.mkdir(parents=True, exist_ok=False)
    primary = Path('C:/Dev/OpenU5-Native')
    dev = primary.parent
    start = datetime.datetime.now(datetime.timezone.utc).isoformat()
    errors, links, dirs, files, repos = [], [], [], [], []
    wt = git(primary, 'worktree', 'list', '--porcelain')
    worktrees = [Path(line[9:]) for line in wt['out'].splitlines() if line.startswith('worktree ')]
    scopes = list(worktrees)
    for name in ('spp1-hw-package', 'spp2-scratch', 'uvw-look-polish-evidence', 'uvw-original-dos-evidence',
                 '_Archive/openu5', '_Archive/u5-ti89'):
        p = dev / name
        if p.exists() and p not in scopes:
            scopes.append(p)
    rootentries = []
    # Unrelated projects get names and root Git identity only; no source/content audit.
    for base in (dev, dev / '_Archive'):
        for p in sorted(base.iterdir()):
            st = p.lstat()
            rootentries.append({'path': str(p), 'directory': p.is_dir(),
                                'reparse': bool(st.st_file_attributes & 1024),
                                'in_scope': p in scopes})
            if (p / '.git').exists() and p not in scopes:
                repos.append({'path': str(p), 'scope': 'unrelated identity only',
                              'head': git(p, 'rev-parse', 'HEAD'),
                              'branch': git(p, 'symbolic-ref', '--short', '-q', 'HEAD'),
                              'common_dir': git(p, 'rev-parse', '--path-format=absolute', '--git-common-dir'),
                              'remotes': git(p, 'remote', '-v')})
    repo_paths = set()
    for scope in scopes:
        def walk(path):
            try:
                entries = list(os.scandir(path))
            except OSError as e:
                errors.append({'path': str(path), 'error': str(e)})
                return 0, 0
            total, count = 0, 0
            for ent in entries:
                p = Path(ent.path)
                # Exclude this report to avoid self-referential size/hash inventories.
                if p == out or p == primary / 'docs/workspace/CLEANUP1':
                    continue
                try:
                    st = ent.stat(follow_symlinks=False)
                    relative = p.relative_to(scope).as_posix()
                    if st.st_file_attributes & 1024:
                        try:
                            target = os.readlink(p)
                        except OSError as e:
                            target = str(e)
                        links.append({'path': str(p), 'scope': str(scope), 'target': target,
                                      'exists': p.exists(), 'attributes': st.st_file_attributes})
                        continue
                    if ent.name == '.git':
                        repo_paths.add(str(path))
                    if ent.is_dir(follow_symlinks=False):
                        b, n = walk(p)
                        dirs.append({'path': str(p), 'scope': str(scope), 'relative': relative,
                                     'bytes': b, 'files': n})
                        total += b
                        count += n
                    else:
                        files.append({'path': str(p), 'scope': str(scope), 'relative': relative,
                                      'bytes': st.st_size, 'mtime_ns': st.st_mtime_ns,
                                      'attributes': st.st_file_attributes, 'git_state': '', 'sha256': ''})
                        total += st.st_size
                        count += 1
                except OSError as e:
                    errors.append({'path': str(p), 'error': str(e)})
            return total, count
        b, n = walk(scope)
        dirs.append({'path': str(scope), 'scope': str(scope), 'relative': '.', 'bytes': b, 'files': n})
        print(f'Inventoried {scope}: {n:,} files, {b:,} bytes', flush=True)
    bypath = {f['path'].casefold(): f for f in files}
    for rootstr in sorted(repo_paths):
        root = Path(rootstr)
        r = {'path': str(root), 'scope': 'full',
             'head': git(root, 'rev-parse', 'HEAD'),
             'branch': git(root, 'symbolic-ref', '--short', '-q', 'HEAD'),
             'common_dir': git(root, 'rev-parse', '--path-format=absolute', '--git-common-dir'),
             'git_dir': git(root, 'rev-parse', '--absolute-git-dir'),
             'remotes': git(root, 'remote', '-v'),
             'status': git(root, 'status', '--porcelain=v1', '-uall'),
             'branches': git(root, 'for-each-ref', '--format=%(refname)|%(objectname)|%(upstream)|%(upstream:track)', 'refs/heads', 'refs/remotes'),
             'worktrees': git(root, 'worktree', 'list', '--porcelain'),
             'stashes': git(root, 'stash', 'list'),
             'tags': git(root, 'show-ref', '--tags'),
             'tracked': git(root, 'ls-files', '-z'),
             'untracked': git(root, 'ls-files', '--others', '--exclude-standard', '-z'),
             'ignored': git(root, 'ls-files', '--others', '--ignored', '--exclude-standard', '-z'),
             'diff_stat': git(root, 'diff', '--stat'),
             'staged_stat': git(root, 'diff', '--cached', '--stat')}
        for key, label in [('tracked', 'tracked'), ('ignored', 'ignored'), ('untracked', 'untracked')]:
            for name in r[key]['out'].split('\0'):
                f = bypath.get(str(root / name).casefold())
                if f:
                    f['git_state'] = label
        repos.append(r)
    # Hash packages, runtime binaries, original data, and unique local work/evidence.
    # Exclude dependency runtime executables and CMake compiler probes.
    binary_suffix = {'.zip', '.7z', '.tar', '.gz', '.tgz', '.bin', '.uf2', '.exe', '.dll', '.elf'}
    hash_count = 0
    for f in files:
        rel = f['relative'].lower()
        parts = rel.split('/')
        original = parts[0] == 'original'
        local = f['git_state'] == 'untracked'
        binary = Path(rel).suffix in binary_suffix and not any(x in parts for x in ('node_modules', '.git', 'cmakefiles', 'sdl3-src', 'build-tools', 'build-zig'))
        evidence = (Path(rel).suffix in {'.log', '.json', '.txt', '.md', '.ps1', '.py', '.bak'}
                    and (f['scope'].endswith(('evidence', 'scratch', 'hw-package')) or f['git_state'] == 'ignored'))
        if original or local or binary or evidence:
            try:
                before = os.stat(f['path'])
                f['sha256'] = sha(f['path'])
                after = os.stat(f['path'])
                if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
                    errors.append({'path': f['path'], 'error': 'Changed while hashing; hash not stable'})
                    f['sha256'] = ''
                hash_count += 1
            except OSError as e:
                errors.append({'path': f['path'], 'error': str(e)})
    groups = collections.defaultdict(list)
    for f in files:
        if f['sha256']:
            groups[(f['bytes'], f['sha256'])].append(f['path'])
    duplicates = [{'bytes_each': size, 'sha256': digest, 'paths': paths,
                   'redundant_bytes_upper_bound': size * (len(paths) - 1)}
                  for (size, digest), paths in groups.items() if len(paths) > 1 and size > 0]
    duplicates.sort(key=lambda d: d['redundant_bytes_upper_bound'], reverse=True)
    # Branch comparisons use retained local refs; do not fetch or change remote-tracking refs.
    comparisons = []
    branchrefs = git(primary, 'for-each-ref', '--format=%(refname)', 'refs/heads')['out'].splitlines()
    for ref in branchrefs:
        row = {'branch': ref}
        for target in ('origin/main', 'enhanced/enhanced-mode', 'release/v0.5.0-rc1'):
            row[target] = git(primary, 'rev-list', '--left-right', '--count', f'{ref}...{target}')
        comparisons.append(row)
    result = {'started_utc': start, 'finished_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'root_entries': rootentries, 'scopes': [str(s) for s in scopes], 'worktrees': wt,
              'repositories': repos, 'branch_comparisons': comparisons,
              'links': links, 'errors': errors, 'hash_count': hash_count,
              'limitations': ['Logical file sizes, not allocated disk usage; reparse targets excluded.',
                              'Unrelated projects: root Git identity only.',
                              'No network fetch; remote relationships use cached refs.',
                              'Live filesystem, not an atomic snapshot; no cleanup permitted by this collector.',
                              'Report directory and collector excluded to avoid self-reference.']}
    (out / 'git-and-workspace.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    csvwrite(out / 'files.csv', files, ['path', 'scope', 'relative', 'bytes', 'mtime_ns', 'attributes', 'git_state', 'sha256'])
    csvwrite(out / 'directories.csv', sorted(dirs, key=lambda d: d['path']), ['path', 'scope', 'relative', 'bytes', 'files'])
    (out / 'duplicate-hashes.json').write_text(json.dumps(duplicates, indent=2), encoding='utf-8')
    print(f'Done: {len(files):,} files; {len(dirs):,} directories; {hash_count:,} hashes; {len(errors)} errors; {len(links)} reparse points', flush=True)


if __name__ == '__main__':
    main()

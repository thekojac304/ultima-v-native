"""Targeted CLEANUP2 preservation checks; no census or input mutation.

Local snapshots are additive and refuse overwrite. Source paths intentionally
describe the audited Windows workspace. Only local/ receives generated output.
"""
import argparse
import csv
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
CENSUS = HERE.parent / 'CLEANUP1' / 'inventory'
LOCAL = HERE / 'local'


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for b in iter(lambda: f.read(4 * 1024 * 1024), b''):
            h.update(b)
    return h.hexdigest()


def git(root, *args):
    p = subprocess.run(['git', '--no-optional-locks', '-C', str(root), *args],
                       capture_output=True, encoding='utf-8', errors='strict',
                       env={**os.environ, 'GIT_OPTIONAL_LOCKS': '0'})
    if p.returncode:
        raise RuntimeError(f'git {args} at {root}: {p.stderr}')
    return p.stdout


def rows(name):
    with (CENSUS / name).open(encoding='utf-8-sig', newline='') as f:
        yield from csv.DictReader(f)


def snapshot():
    meta = json.loads((CENSUS / 'git-and-workspace.json').read_text())
    repositories = {}
    for r in meta['repositories']:
        if r['scope'] != 'full':
            continue
        root = r['path']
        repositories[root] = {
            'head': git(root, 'rev-parse', 'HEAD').strip(),
            'branch': git(root, 'branch', '--show-current').strip(),
            'status': git(root, 'status', '--porcelain=v1', '-uall'),
            'diff': git(root, 'diff', '--binary'),
            'staged': git(root, 'diff', '--cached', '--binary'),
            'refs': git(root, 'for-each-ref', '--format=%(refname)|%(objectname)'),
            'worktrees': git(root, 'worktree', 'list', '--porcelain'),
            'stashes': git(root, 'stash', 'list', '--format=%H %gs'),
        }
    selected = {r['path']: r['sha256'] for r in rows('release-catalog.csv')}
    for r in rows('files.csv'):
        if r['sha256'] and (r['git_state'] == 'untracked'
                or (r['scope'] == str(ROOT) and r['relative'].startswith('original/'))
                or ('ultima-v-native-rc1' in r['scope'] and '/dist/' in '/' + r['relative'])):
            selected[r['path']] = r['sha256']
    files = {}
    for path, expected in selected.items():
        actual = sha(path)
        if actual != expected:
            raise RuntimeError(f'CLEANUP1 hash mismatch: {path}')
        files[path] = {'bytes': Path(path).stat().st_size, 'sha256': actual}
    links = {}
    for r in meta['links']:
        p = Path(r['path'])
        target = os.readlink(p)
        if target != r['target'] or not p.exists():
            raise RuntimeError(f'Reparse target changed/unavailable: {p}')
        links[str(p)] = target
    # Check frozen inventories too; never update their historical checksum list.
    inventories = {}
    for line in (CENSUS / 'audit-files.sha256').read_text().splitlines():
        expected, name = line.split(' *', 1)
        actual = sha(CENSUS / name)
        if actual != expected:
            raise RuntimeError(f'CLEANUP1 inventory checksum mismatch: {name}')
        inventories[name] = actual
    return {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'repositories': repositories, 'files': files, 'links': links,
            'inventories': inventories,
            'dev_root': sorted(p.name for p in ROOT.parent.iterdir()),
            'repo_root': sorted(p.name for p in ROOT.iterdir()),
            'scope': 'Existing census selected hashes, Git states, links; no full file census'}


def filtered_status(value):
    def cleanup(path):
        return path == '.gitignore' or path == 'CONTRIBUTING.md' or path == 'docs/WORKSPACE_ORGANIZATION.md' or path.startswith('docs/workspace/')
    return [line for line in value.splitlines() if not cleanup(line[3:])]


def compare(before, after):
    checks = []
    def check(name, ok):
        checks.append({'check': name, 'passed': bool(ok)})
    for key in ('files', 'links', 'inventories', 'repo_root'):
        check(key + ' preserved', before[key] == after[key])
    added = set(after['dev_root']) - set(before['dev_root'])
    check('Dev roots retained; only planned roots added',
          set(before['dev_root']) <= set(after['dev_root']) and
          added <= {'ultima-v-builds', 'ultima-v-archive', 'openu5-release-artifacts'})
    check('repository set preserved', before['repositories'].keys() == after['repositories'].keys())
    for root, b in before['repositories'].items():
        a = after['repositories'][root]
        if root != str(ROOT):
            # Native linked worktrees share refs and registration with primary.
            for key in ('head', 'branch', 'status', 'diff', 'staged', 'stashes'):
                check(root + ' ' + key, a[key] == b[key])
        else:
            check('primary remains main', a['branch'] == b['branch'] == 'main')
            check('primary unrelated status preserved', filtered_status(a['status']) == filtered_status(b['status']))
            check('primary index empty', not b['staged'] and not a['staged'])
            check('primary stashes preserved', a['stashes'] == b['stashes'])
            check('primary commits descend from baseline',
                  subprocess.run(['git', '-C', str(ROOT), 'merge-base', '--is-ancestor', b['head'], a['head']]).returncode == 0)
            names = git(ROOT, 'diff', '--name-only', b['head'], a['head']).splitlines()
            check('committed changes limited to cleanup files', not filtered_status('\n'.join(' M ' + n for n in names)))
        def refs(value):
            return [x for x in value.splitlines() if not x.startswith('refs/heads/main|')]
        check(root + ' other refs preserved', refs(a['refs']) == refs(b['refs']))
        # Only primary HEAD may advance in the shared registration listing.
        def worktrees(value):
            blocks = value.strip().split('\n\n')
            return ['\n'.join(l for l in block.splitlines() if not (
                block.startswith('worktree ' + ROOT.as_posix() + '\n') and l.startswith('HEAD '))) for block in blocks]
        check(root + ' worktree registration preserved', worktrees(a['worktrees']) == worktrees(b['worktrees']))
    return {'checks': checks, 'passed': sum(c['passed'] for c in checks),
            'failed': sum(not c['passed'] for c in checks)}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('operation', choices=['snapshot', 'compare'])
    p.add_argument('labels', nargs='+')
    args = p.parse_args()
    if any(not x.replace('-', '').isalnum() for x in args.labels):
        p.error('labels must contain only letters, digits and hyphens')
    LOCAL.mkdir(exist_ok=True)
    if args.operation == 'snapshot':
        if len(args.labels) != 1:
            p.error('snapshot needs one label')
        result = snapshot()
        name = args.labels[0]
    else:
        if len(args.labels) != 2:
            p.error('compare needs two labels')
        before, after = [json.loads((LOCAL / (x + '.json')).read_text()) for x in args.labels]
        result = compare(before, after)
        name = '-vs-'.join(args.labels)
    with (LOCAL / (name + '.json')).open('x', encoding='utf-8') as f:
        json.dump(result, f, indent=2)
    if args.operation == 'compare':
        print(json.dumps(result, indent=2))
        if result['failed']:
            raise SystemExit(1)
    else:
        print(f"Snapshot {name}: {len(result['repositories'])} repositories, {len(result['files'])} hashes, {len(result['links'])} links, {len(result['inventories'])} inventories")


if __name__ == '__main__':
    main()

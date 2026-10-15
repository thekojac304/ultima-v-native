"""Create conservative classifications and exact review candidates from the census.

No cleanup actions are executed. Input inventory is immutable; derived reports are
written next to it. Rules prefer retention when provenance or dependencies vary.
"""
import collections
import csv
import json
from pathlib import Path
import re
import zipfile

HERE = Path(__file__).resolve().parent
DATA = HERE / 'inventory'
PRIMARY = 'C:\\Dev\\OpenU5-Native'
RC = 'C:\\Dev\\ultima-v-native-rc1'
SPECIAL = {'build-shops', 'build-real-arenas', 'build-dialogue', 'build-quests',
           'build-gameplay', 'build-persistence', 'build-dungeon', 'build-tools', 'build-zig'}


def readcsv(name):
    with (DATA / name).open(encoding='utf-8-sig', newline='') as f:
        return list(csv.DictReader(f))


def writecsv(name, rows, fields):
    with (DATA / name).open('w', encoding='utf-8-sig', newline='') as f:
        w = csv.DictWriter(f, fields, extrasaction='ignore')
        w.writeheader()
        w.writerows(rows)


def main():
    meta = json.loads((DATA / 'git-and-workspace.json').read_text())
    files, dirs = readcsv('files.csv'), readcsv('directories.csv')
    duplicates = json.loads((DATA / 'duplicate-hashes.json').read_text())
    repos = {r['path']: r for r in meta['repositories'] if r['scope'] == 'full'}
    detached = {p for p, r in repos.items() if not r['branch']['out'].strip()}
    # Byte identity is established by SHA-256, not by matching archive names.
    sdl = next(d for d in duplicates if d['sha256'] == '7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68')
    authority = next(p for p in sdl['paths'] if '\\build-pkg\\' in p)
    duplicate_candidates = set(sdl['paths']) - {authority}
    # Establish compiler-object provenance from the exact retained command database.
    # These objects remain in their known-good builds; they are not reclaim actions.
    object_provenance = {}
    for f in files:
        database = Path(f['path'])
        if database.name != 'compile_commands.json' or 'node_modules' in f['relative'].split('/'):
            continue
        try:
            commands = json.loads(database.read_text(encoding='utf-8'))
        except (OSError, ValueError):
            continue
        for entry in commands:
            if not entry.get('output'):
                continue
            compiler_match = re.match(r'^\s*(?:"([^"]+)"|(\S+))', entry.get('command',''))
            if not compiler_match:
                continue
            compiler = Path(compiler_match[1] or compiler_match[2])
            source = Path(entry['file'])
            target = Path(entry['output'])
            if not target.is_absolute():
                target = Path(entry['directory']) / target
            if source.is_file() and compiler.is_file():
                object_provenance[str(target).casefold()] = dict(path=str(target),source=str(source),
                    compiler=str(compiler),command_database=str(database),proposed_action='Retain; no deletion proposed')
    proven_objects = []
    cache_dirs = []
    for d in dirs:
        parts = d['relative'].split('/')
        if d['scope'] == RC or '_Archive' in d['scope'] or 'node_modules' in parts:
            continue
        p = Path(d['path'])
        if p.name not in {'__pycache__', '.vite-cache'}:
            continue
        children = [f for f in files if f['path'].startswith(d['path'] + '\\')]
        # Python caches must have retained adjacent source; no source-only .pyc.
        if p.name == '__pycache__' and not all(
                Path(f['path']).suffix == '.pyc' and
                (p.parent / (Path(f['path']).name.split('.')[0] + '.py')).exists()
                for f in children):
            continue
        if p.name == '.vite-cache' and not all('/deps/' in '/' + f['relative'] for f in children):
            continue
        cache_dirs.append(d)
    cacheprefixes = [d['path'] + '\\' for d in cache_dirs]
    class_counts, class_bytes = collections.Counter(), collections.Counter()
    for f in files:
        parts = f['relative'].split('/')
        suffix = Path(f['relative']).suffix.lower()
        r = repos.get(f['scope'])
        relationship = (f"{r['branch']['out'].strip() or 'DETACHED'} @ {r['head']['out'].strip()}" if r else 'non-Git material')
        cls, reason = 'UNKNOWN', 'Purpose or exact reproducibility not established; retain for investigation'
        if '.git' in parts:
            cls, reason = 'KEEP — ACTIVE', 'Git objects, refs, reflogs, index and worktree metadata; preserve history and recoverability'
        elif f['scope'] == RC:
            cls, reason = 'KEEP — REFERENCE', 'Complete RC1 source/build/fixtures/evidence retained through hardware acceptance'
            if f['git_state'] == 'tracked' and suffix not in {'.log'}:
                cls, reason = 'KEEP — ACTIVE', 'Current RC1 tracked source, tests, tools or documentation'
        elif f['path'] in duplicate_candidates:
            cls, reason = 'DUPLICATE', 'SHA-256 identical SDL source archive; retained authority: ' + authority
        elif any(f['path'].startswith(prefix) for prefix in cacheprefixes):
            cls, reason = 'REGENERABLE', 'Verified Python bytecode with adjacent source or generated Vite dependency cache'
        elif parts[0] in {'original', 'reference'} or f['relative'].startswith(('game/assets/', 'native/assets/')):
            cls, reason = 'KEEP — REFERENCE', 'Original/private game inputs or derived resource packs; preservation and test dependency'
        elif any(p in SPECIAL for p in parts):
            cls, reason = 'KEEP — ACTIVE', 'Specialized fixture/generator/toolchain workspace; explicit cleanup policy or CMake dependency'
        elif f['scope'].endswith('spp1-hw-package'):
            cls, reason = 'KEEP — REFERENCE', 'Documented SPP1 physical rollback package and build/package provenance'
        elif f['scope'].endswith(('uvw-look-polish-evidence', 'uvw-original-dos-evidence')):
            cls, reason = 'KEEP — REFERENCE', 'Acceptance screenshots, comparison traces, investigation scripts and test evidence'
        elif '_Archive' in f['scope']:
            cls, reason = 'KEEP — REFERENCE', 'Existing historical repository or TI89 preservation material, including tools and stash'
        elif f['scope'].endswith('spp2-scratch'):
            cls, reason = 'ARCHIVE', 'SPP2 investigation scripts, before-state binary, mutant outputs and local SDL source; preserve as a unit'
        elif f['scope'] in detached:
            cls, reason = 'KEEP — REFERENCE', 'Registered historical baseline checkout with probe/build/evidence material; retirement not approved'
        elif f['git_state'] == 'tracked':
            cls, reason = 'KEEP — ACTIVE', 'Tracked source, tests, project tools and established documentation'
            if suffix == '.log' or f['relative'].startswith(('re/notes/', 'docs/history/')):
                cls, reason = 'KEEP — REFERENCE', 'Tracked historical validation/investigation record; not a disposable log'
        elif suffix == '.obj' and f['path'].casefold() in object_provenance:
            cls, reason = 'REGENERABLE', 'Compiler object mapped to retained source, existing compiler and exact compile_commands.json; keep known-good build intact'
            proven_objects.append(dict(object_provenance[f['path'].casefold()],bytes=f['bytes']))
        elif suffix in {'.log', '.out', '.err'} or any('findings' in p or 'evidence' in p for p in parts):
            cls, reason = 'KEEP — REFERENCE', 'Local validation or investigation evidence; retain provenance even when ignored'
        elif suffix in {'.bin', '.elf', '.map', '.exe', '.dll', '.zip', '.gz', '.tgz', '.7z', '.uf2'}:
            cls, reason = 'KEEP — REFERENCE', 'Retained runtime/build/package or offline dependency; older does not imply disposable'
        elif 'node_modules' in parts:
            cls, reason = 'KEEP — ACTIVE', 'Installed shared dependency tree; junction consumers rely on its availability'
        elif f['relative'].startswith(('native/core/fixtures/', 'extractor/vendor/')):
            cls, reason = 'KEEP — REFERENCE', 'Private fixture or offline vendor input; retain original and generated data'
        f.update(classification=cls, proposed_action='Retain in place', justification=reason,
                 git_relationship=relationship, approval_required='no action')
        if cls in {'DUPLICATE', 'REGENERABLE'}:
            f.update(proposed_action='Delete only after explicit approval and fresh verification', approval_required='yes')
            if f['path'].casefold() in object_provenance and suffix == '.obj':
                f.update(proposed_action='Retain in known-good build; regeneration command recorded; no deletion proposed',approval_required='no action')
        if cls == 'ARCHIVE':
            f.update(proposed_action='Archive complete investigation only after explicit approval; keep source until restoration verified', approval_required='yes')
        class_counts[cls] += 1
        class_bytes[cls] += int(f['bytes'])
    writecsv('classified-files.csv', files, list(files[0]))
    writecsv('generated-object-provenance.csv',proven_objects,['path','bytes','source','compiler','command_database','proposed_action'])
    relationships = {f['scope']: f['git_relationship'] for f in files}
    # Directory class is a conservative container policy; children can be more specific.
    for d in dirs:
        parts = d['relative'].split('/')
        cls, reason = 'KEEP — ACTIVE', 'Container policy: retain; use classified-files.csv for individual contents'
        if d['scope'] == RC or d['scope'] in detached or '_Archive' in d['scope'] or d['scope'].endswith(('evidence', 'hw-package')):
            cls, reason = 'KEEP — REFERENCE', 'Preserved release/baseline/historical container; no whole-tree removal approved'
        elif d['scope'].endswith('spp2-scratch'):
            cls, reason = 'ARCHIVE', 'Whole investigation proposed for verified archive after dependency review'
        elif d in cache_dirs:
            cls, reason = 'REGENERABLE', 'Exact verified cache-only directory; deletion needs approval'
        elif any(p.lower().startswith('build') for p in parts) and not any(p in SPECIAL for p in parts):
            cls, reason = 'UNKNOWN', 'Mixed build container can contain unique scripts, binaries, configuration and evidence'
        d.update(classification=cls, proposed_action='Retain in place; inspect child classifications',
                 justification=reason, git_relationship=relationships.get(d['scope'], 'none'))
    writecsv('classified-directories.csv', dirs, list(dirs[0]))
    writecsv('reparse-points.csv', [dict(l, classification='KEEP — ACTIVE', bytes=0,
             proposed_action='Retain link and target in place', justification='Shared dependency; do not traverse during cleanup',
             git_relationship=relationships.get(l['scope'],'none'))
             for l in meta['links']], ['path', 'scope', 'target', 'exists', 'attributes', 'classification', 'bytes', 'proposed_action', 'justification','git_relationship'])
    # Record all duplicate groups and authoritative copy decisions, without authorizing deletion.
    dupe_rows = []
    for group in duplicates:
        retained = authority if group is sdl else min(group['paths'], key=lambda p: (
            0 if p.startswith(RC) and '\\dist\\' in p and 'attempt-' not in p else
            1 if p.startswith(PRIMARY) else 2, len(p), p))
        for path in group['paths']:
            dupe_rows.append(dict(path=path, bytes=group['bytes_each'], sha256=group['sha256'],
                authoritative_retained_copy=retained,
                proposed_action=('Retain authoritative copy' if path == retained else
                                 'Approval candidate: remove duplicate SDL archive' if path in duplicate_candidates else
                                 'Retain: evidence/fixtures/runtime/private data may require this independent path')))
    writecsv('duplicate-review.csv', dupe_rows, ['path','bytes','sha256','authoritative_retained_copy','proposed_action'])
    plan = []
    def action(path, destination, kind, classification, size, why, gate):
        plan.append(dict(path=path, destination=destination, proposed_action=kind,
                         classification=classification, bytes=size, approval_required='yes', justification=why, prerequisites=gate))
    for d in cache_dirs:
        action(d['path'], '', 'Delete cache directory', 'REGENERABLE', d['bytes'], 'Verified cache-only contents',
               'Close relevant processes; repeat census; verify adjacent source; refuse reparse points and paths outside exact root')
    for p in sorted(duplicate_candidates):
        action(p, authority, 'Delete redundant SDL source archive', 'DUPLICATE', sdl['bytes_each'],
               'Hash identity verified; canonical offline source archive remains', 'Rehash both files and check no active download/extraction')
    # Copy first, then verify. These are reference archives, not deletion candidates.
    packages = []
    for f in files:
        rel, p = f['relative'], Path(f['path'])
        if '/dist/' in '/' + rel and p.suffix.lower()=='.zip' and 'node_modules' not in rel:
            provenance = ''
            try:
                with zipfile.ZipFile(p) as z:
                    bad = z.testzip()
                    provenance = z.read('BUILD-INFO.txt').decode('utf-8-sig') if 'BUILD-INFO.txt' in z.namelist() else 'No BUILD-INFO'
                    if bad:
                        provenance += '\nBAD CRC: ' + bad
            except (OSError, zipfile.BadZipFile) as e:
                provenance = str(e)
            packages.append(dict(path=str(p), bytes=f['bytes'], sha256=f['sha256'],
                                 classification='KEEP — REFERENCE', proposed_action='Retain original; proposed verified archival copy', provenance=provenance))
        if (('launcher' in p.name.lower() or p.name=='openu5_tdeck.bin')
                and p.suffix.lower()=='.bin' and '_Archive' not in f['scope']):
            data = p.read_bytes()
            packages.append(dict(path=str(p), bytes=f['bytes'], sha256=f['sha256'],
                classification='KEEP — REFERENCE', proposed_action='Retain original and build context',
                provenance='ESP app version descriptor: ' + data[48:80].split(b'\0')[0].decode('utf-8','replace')))
    writecsv('release-catalog.csv', packages, ['path','bytes','sha256','classification','proposed_action','provenance'])
    top = {d['path']: d for d in dirs if d['relative']=='.'}
    for source in ['C:\\Dev\\spp2-scratch', 'C:\\Dev\\uvw-base\\audit-fw']:
        d = next(d for d in dirs if d['path']==source)
        action(source, 'C:\\Dev\\ultima-v-archive\\' + Path(source).name,
               'Archive complete investigation; retain original until approved retirement', 'ARCHIVE', d['bytes'],
               'Contains unique scripts, before-state artifacts and validation material',
               'Capture all files and timestamps, manifest SHA-256, test restoration; update hardcoded scripts/doc links before any source removal')
    for source in ['C:\\Dev\\uvw-look-polish-evidence', 'C:\\Dev\\uvw-original-dos-evidence', 'C:\\Dev\\spp1-hw-package']:
        dest = 'C:\\Dev\\openu5-release-artifacts\\historical\\' + Path(source).name
        action(source, dest, 'Copy complete reference package/evidence; keep original', 'KEEP — REFERENCE', top[source]['bytes'],
               'Formalize retained acceptance/rollback material', 'No overwrite; verify every copied hash and preserve relative layout; original paths stay')
    for source in [RC+'\\native\\targets\\tdeck\\dist', RC+'\\native\\targets\\windows\\dist']:
        d = next(d for d in dirs if d['path']==source)
        target = 'C:\\Dev\\openu5-release-artifacts\\v0.5.0-rc1\\' + ('tdeck' if '\\tdeck\\' in source else 'windows')
        action(source, target, 'Copy complete RC1 packages and evidence; keep original', 'KEEP — REFERENCE', d['bytes'],
               'Packages, manifests, failed attempts and frozen traces preserved together', 'No overwrite; verify manifest, all hashes and archive restoration; retain active build trees and private fixtures')
    for source in sorted(detached):
        action(source, 'C:\\Dev\\ultima-v-archive\\worktrees\\' + Path(source).name,
               'Archive full checkout for possible later retirement; no removal proposed yet', 'ARCHIVE', top[source]['bytes'],
               'Historical comparison checkout; unique local material or dependencies may exist',
               'Retain detached HEAD in durable ref/bundle; capture ignored/untracked files and diffs; verify restore; separately approve git worktree removal only after dependency closure')
    for source in sorted(p for p in repos if p not in detached and p not in {PRIMARY, RC} and '_Archive' not in p):
        action(source, 'C:\\Dev\\ultima-v-worktrees\\' + Path(source).name,
               'Optional later git worktree move; defer through RC acceptance', 'KEEP — ACTIVE', top[source]['bytes'],
               'Consolidate active checkout paths only if continued use is confirmed',
               'Resolve all incoming junctions/absolute paths; capture state; use git worktree move; rebuild from scratch; repeat host/Windows validation')
    writecsv('approval-plan.csv', plan, list(plan[0]))
    summary = dict(file_count=len(files), directory_count=len(dirs), root_count=len(top),
        logical_bytes=sum(int(d['bytes']) for d in top.values()), reparse_points=len(meta['links']),
        hash_count=meta['hash_count'], class_counts=dict(class_counts), class_bytes=dict(class_bytes),
        conservative_cache_bytes=sum(int(d['bytes']) for d in cache_dirs),
        duplicate_archive_candidate_bytes=sum(sdl['bytes_each'] for p in duplicate_candidates),
        conservative_recoverable_bytes=sum(int(d['bytes']) for d in cache_dirs)+sum(sdl['bytes_each'] for p in duplicate_candidates),
        proven_object_bytes_retained=sum(int(f['bytes']) for f in proven_objects),
        duplicate_upper_bound_bytes=sum(g['redundant_bytes_upper_bound'] for g in duplicates),
        duplicate_group_count=len(duplicates),
        notes=['Recovery estimates are logical bytes; actual NTFS allocation/hardlinks not measured.',
               'Duplicate upper bound includes required independent fixtures, active executables, private data and evidence; it is not an approved recovery estimate.',
               'Directory totals overlap and must not be summed except scope roots.',
               'Copying or archiving on the same volume does not reclaim space.',
               'No package, evidence, worktree, build, branch, or file has been deleted/moved.'])
    (DATA/'classification-summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    print(json.dumps(summary,indent=2))


if __name__ == '__main__':
    main()

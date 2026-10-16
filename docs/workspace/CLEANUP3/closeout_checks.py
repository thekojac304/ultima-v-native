"""Read-only CLEANUP3 closeout checks. Does not migrate, package or build sources."""
import argparse, csv, hashlib, importlib.util, json, os, re, subprocess, sys
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

BASE = Path('C:/Dev/U5Native')
PRIMARY = BASE / 'source'
STATE = BASE / 'archive/CLEANUP3'
FINISH = STATE / 'finish-20261016T002344Z'
BUILD = BASE / 'builds/cleanup3-finish-20261016T002344Z'
RC1 = BASE / 'worktrees/ultima-v-native-rc1'
COMMIT = 'a96656b2330097c18c52b78a8840d3749c0e9854'

def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(4 * 1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()

def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))

def run(args, cwd=PRIMARY):
    return subprocess.run([str(x) for x in args], cwd=cwd, capture_output=True,
                          encoding='utf-8', errors='replace',
                          env={**os.environ, 'GIT_OPTIONAL_LOCKS': '0',
                               'PYTHONDONTWRITEBYTECODE': '1'}, check=True).stdout

def firmware():
    target = RC1 / 'native/targets/tdeck'
    sys.path.insert(0, str(target))
    from package_launcher import validate
    from check_app_budget import check_build, smallest_app_partition
    build = BUILD / 'rc1-tdeck'
    meta = read(build / 'project_description.json')
    image, elf = build / meta['app_bin'], build / meta['app_elf']
    data = image.read_bytes()
    validate(data)
    assert len(data) == 1075088
    assert data[48:80].split(b'\0')[0] == b'0.5.0-rc1'
    assert COMMIT[:12].encode() in data and (COMMIT[:12] + '-dirty').encode() not in data
    assert data[176:208].hex() == sha(elf), 'Image does not match the retained ELF'
    assert Path(meta['project_path']) == target and Path(meta['build_dir']) == build
    assert run(['git', '--no-optional-locks', '-C', RC1, 'rev-parse', 'HEAD']).strip() == COMMIT
    assert not run(['git', '--no-optional-locks', '-C', RC1, 'status', '--porcelain=v1', '-uall'])
    partition = build / 'partition_table/partition-table.bin'
    size = smallest_app_partition(partition.read_bytes())
    assert size - len(data) == 235632
    budget, lines = check_build(build)
    assert budget == 'ok', lines
    ninja = Path('C:/Users/lawma/.espressif/tools/ninja/1.12.1/ninja.exe')
    dry_runs = {name: run([ninja, '-C', folder, '-n', '-j', '2', 'all'])
                for name, folder in [('app', build), ('bootloader', build / 'bootloader')]}
    assert all(not re.search(r'Building (?:C|CXX|ASM)|Linking (?:C|CXX)', text)
               for text in dry_runs.values()), dry_runs
    log = (build / '.ninja_log').read_text(encoding='utf-8')
    outputs = {line.split('\t')[3].replace('\\', '/') for line in log.splitlines()
               if not line.startswith('#') and len(line.split('\t')) >= 5}
    failed_object = 'esp-idf/esp_lcd/CMakeFiles/__idf_esp_lcd.dir/rgb/esp_lcd_panel_rgb.c.obj'
    assert {meta['app_elf'], failed_object} <= outputs
    cfg = read(build / 'config/sdkconfig.json')
    return {'passed': True, 'source_commit': COMMIT, 'source_clean': True,
            'image': str(image), 'image_bytes': len(data), 'image_sha256': sha(image),
            'elf': str(elf), 'elf_sha256': sha(elf), 'image_elf_hash_matches': True,
            'version': data[48:80].split(b'\0')[0].decode(),
            'project_name': data[80:112].split(b'\0')[0].decode(),
            'idf_version': data[144:176].split(b'\0')[0].decode(),
            'image_integrity': 'ESP32-S3 header, segments, XOR checksum and appended SHA-256 valid',
            'partition_sha256': sha(partition), 'partition_bytes': size,
            'partition_free_bytes': size-len(data), 'budget': budget, 'budget_lines': lines,
            'project_description_sha256': sha(build / 'project_description.json'),
            'sdkconfig_sha256': sha(build / 'sdkconfig'),
            'source_sdkconfig_sha256': sha(target / 'sdkconfig'),
            'compiler': meta['c_compiler'],
            'compiler_version': run([meta['c_compiler'], '--version']).splitlines()[0],
            'sdk_git_revision': meta.get('git_revision'),
            'configuration': {k: cfg.get(k) for k in ('IDF_TARGET', 'COMPILER_OPTIMIZATION_SIZE', 'APP_REPRODUCIBLE_BUILD', 'ESPTOOLPY_FLASHSIZE')},
            'ninja_log_sha256': sha(build / '.ninja_log'),
            'formerly_failed_object_completed': True, 'ninja_dry_runs': dry_runs,
            'historical_release_sha256': '742dffcfc2675d02473e1566951af7ff36d743b205051aaac40d72f6893ec787',
            'historical_release_replaced': False, 'hardware': 'NOT RUN'}

def receipts():
    tests = FINISH / 'tests'
    values = []
    for p in sorted(tests.glob('*.json')):
        r = read(p)
        assert Path(r['log']) == tests / (r['name'] + '.log')
        text = Path(r['log']).read_text(encoding='utf-8-sig')
        expected = 1 if r['name'] == 'firmware-build' else 0
        assert r['exit_code'] == expected, r
        entry = {**r, 'receipt_sha256': sha(p), 'log_sha256': sha(r['log'])}
        if r['name'] in ('primary-host-dev', 'host-tests', 'windows-tests'):
            n = {'primary-host-dev': 202, 'host-tests': 212, 'windows-tests': 38}[r['name']]
            assert re.search(r'100% tests passed(?:, 0 tests failed)? out of '+str(n)+r'\b', text)
            folder = {'primary-host-dev': 'primary-host-dev', 'host-tests': 'rc1-host', 'windows-tests': 'rc1-windows'}[r['name']]
            last = BUILD / folder / 'Testing/Temporary/LastTest.log'
            last_text = last.read_text(encoding='utf-8-sig')
            assert len(re.findall(r'^Test Passed\.', last_text, re.M)) == n
            assert 'Test Failed.' not in last_text
            entry.update({'passed_tests': n, 'last_test_log_sha256': sha(last)})
        if r['name'] == 'firmware-build':
            assert r['arguments'][-3:] == ['-j', '6', 'all']
            assert 'internal compiler error: Segmentation fault' in text
            entry['cause'] = 'Xtensa GCC ICE in ESP-IDF esp_lcd_panel_rgb.c'
        values.append(entry)
    assert len(values) == 11
    return {'passed': True, 'successful_receipts': 10,
            'original_failed_firmware_attempt_preserved': True, 'receipts': values}

def recovery():
    spec = importlib.util.spec_from_file_location('migration', PRIMARY / 'docs/workspace/CLEANUP3/migrate.py')
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    results = []
    def check(copy, records, live):
        actual = {r['path']: (p, r) for p, r in m.walk(copy)}
        expected = {r['path']: r for r in records if r['kind'] != 'link'}
        assert actual.keys() == expected.keys(), copy
        def one(item):
            rel, rec = item
            p, now = actual[rel]
            assert now['kind'] == rec['kind'], p
            if rec['kind'] == 'file':
                assert p.stat().st_size == rec['bytes'] and sha(p) == rec['sha256'], p
                current = live / rel
                if current.exists() and current.is_file():
                    assert (p.stat().st_dev, p.stat().st_ino) != (current.stat().st_dev, current.stat().st_ino), p
        with ThreadPoolExecutor(max_workers=12) as pool:
            list(pool.map(one, expected.items(), buffersize=96))
        return {'root': str(copy), 'files': sum(r['kind']=='file' for r in records),
                'bytes': sum(r.get('bytes',0) for r in records),
                'hashes_and_inventory_passed': True, 'independent_file_identities': True}
    for i, row in enumerate(m.mapping()):
        label = f'{i:02d}-{Path(row["source"]).name}'
        manifest = STATE / 'manifests' / (label + '.json')
        receipt = read(STATE / 'backup' / (label + '.json'))
        assert sha(manifest) == receipt['manifest_sha256']
        results.append(check(STATE / 'recovery' / label, read(manifest), m.current(row)))
        print('RECOVERY VERIFIED:', label, flush=True)
    results.append(check(FINISH / 'primary-recovery', read(FINISH / 'primary-files.json'), PRIMARY))
    assert len(results)==26
    return {'passed': True, 'copies': results,
            'physical_files': sum(r['files'] for r in results),
            'logical_bytes': sum(r['bytes'] for r in results),
            'note': 'Independent physical copies on C:, not an off-drive disaster backup'}


def migration_module():
    spec = importlib.util.spec_from_file_location('migration', PRIMARY / 'docs/workspace/CLEANUP3/migrate.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def paths():
    m = migration_module()
    links = []
    for i, row in enumerate(m.mapping()):
        root = m.current(row)
        assert root == Path(row['destination'])
        label = f'{i:02d}-{Path(row["source"]).name}'
        for rec in read(STATE / 'manifests' / (label + '.json')):
            if rec['kind'] == 'link':
                p = root / rec['path']
                actual = os.readlink(p)
                assert actual.lower() == m.live_target(rec['target']).lower(), p
                assert p.resolve(strict=True).exists(), p
                links.append({'current_path': str(p), 'current_target': actual, 'resolves': True})
    assert len(links) == 54
    cache_paths = {}
    for folder, source in [('primary-host-dev', PRIMARY / 'native/core'),
                           ('rc1-host', RC1 / 'native/core'),
                           ('rc1-windows', RC1 / 'native/targets/windows'),
                           ('rc1-tdeck', RC1 / 'native/targets/tdeck')]:
        text = (BUILD / folder / 'CMakeCache.txt').read_text(encoding='utf-8-sig')
        home = re.search(r'^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$', text, re.M).group(1)
        assert Path(home) == source, (folder, home)
        cache_paths[folder] = home
    layout = read(BASE / 'workspace.json')
    assert Path(layout['primary']) == PRIMARY and not Path('C:/Dev/OpenU5-Native').exists()
    active = [BASE / 'tools/Build-U5Native.ps1', BASE / 'workspace.json',
              BASE / 'README.md', BASE / 'tools/README.md',
              PRIMARY / 'native/tools/cleanup_builds.py',
              PRIMARY / 'docs/WORKSPACE_ORGANIZATION.md', PRIMARY / '.git/config']
    for p in active:
        text = p.read_text(encoding='utf-8-sig').replace('\\', '/').lower()
        for row in m.mapping():
            assert row['source'].replace('\\', '/').lower() not in text, p
    registration = run(['git', 'worktree', 'list', '--porcelain'])
    assert 'prunable' not in registration and 'locked' not in registration
    assert {Path(x[9:]) for x in registration.splitlines() if x.startswith('worktree ')} == {
        Path(r['destination']) for r in m.mapping() if r['kind'] in ('primary', 'worktree')}
    return {'passed': True, 'junctions': links, 'cache_source_directories': cache_paths,
            'active_path_consumers': {str(p): sha(p) for p in active},
            'worktree_registrations': registration, 'old_primary_absent': True}


def git():
    # Run after the authorized documentation commit; the original verifier is a
    # deliberately stricter, frozen PRE-COMMIT preservation check.
    m = migration_module()
    approved = read(STATE / 'closeout-20261016T011631Z/closeout-staging.json')
    before = read(STATE / 'git-before.json')
    old = before['C:\\Dev\\OpenU5-Native']['head'].strip()
    head = run(['git', 'rev-parse', 'HEAD']).strip()
    assert run(['git', 'rev-list', '--parents', '-n', '1', head]).split() == [head, old]
    assert run(['git', 'log', '-1', '--format=%s']).strip() == approved['commit_subject']
    changed = run(['git', 'diff', '--name-only', old, head]).splitlines()
    assert set(changed) == set(approved['paths']), changed
    tree = run(['git', 'ls-tree', '-r', head]).splitlines()
    blobs = {line.split('\t', 1)[1]: line.split()[2] for line in tree}
    assert {p: blobs[p] for p in approved['paths']} == approved['staged_blob_ids']
    results = []
    for row in m.mapping():
        if row['source'] not in before:
            continue
        root, b = Path(row['destination']), before[row['source']]
        a = m.snapshot(root)
        primary = row['kind'] == 'primary'
        native = row['kind'] != 'repository'
        assert a['head'].strip() == (head if primary else b['head'].strip())
        for key in ('branch', 'staged', 'stashes'):
            assert a[key] == b[key], (root, key)
        if primary:
            expected = '\n'.join(x for x in b['status'].splitlines()
                                 if not x[3:].startswith('docs/workspace/CLEANUP3/'))
            assert a['status'].strip() == expected.strip(), a['status']
            assert not a['diff'] and a['ignored'] == b['ignored']
        else:
            for key in ('status', 'diff', 'ignored'):
                assert a[key] == b[key], (root, key)
        def refs(text):
            return dict(x.split('|', 1) for x in text.splitlines())
        expected_refs = refs(b['refs'])
        if native:
            expected_refs['refs/heads/main'] = head
        assert refs(a['refs']) == expected_refs, root
        def logs(text):
            return Counter(re.sub(r'@\{[0-9]+\}', '@{}', line) for line in text.splitlines())
        old_logs, new_logs = logs(b['reflogs']), logs(a['reflogs'])
        assert not (old_logs - new_logs), ('lost reflog entries', root)
        additions = list((new_logs - old_logs).elements())
        assert len(additions) == (2 if native else 0), (root, additions)
        assert all(x.startswith(head + ' ') and ('commit: ' + approved['commit_subject']) in x for x in additions)
        results.append({'root': str(root), 'head': a['head'].strip(),
                        'refs_preserved_except_authorized_main_commit': True,
                        'stashes_working_changes_ignored_files_and_prior_reflogs_preserved': True})
    assert len(results) == 17
    assert not run(['git', 'diff', '--cached', '--name-only'])
    return {'passed': True, 'branch': 'main', 'commit': head, 'parent': old,
            'committed_paths': changed, 'repositories': results,
            'remaining_primary_status': run(['git', 'status', '--short', '-uall']),
            'index_empty': True, 'remote_operation': False}

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('check', choices=['firmware', 'receipts', 'recovery', 'paths', 'git'])
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    assert not args.output.exists(), args.output
    result = globals()[args.check]()
    with args.output.open('x', encoding='utf-8') as f:
        json.dump(result, f, indent=2)
    print(json.dumps({k:v for k,v in result.items() if k not in ('copies','receipts','ninja_dry_runs','repositories','junctions')}, indent=2))

if __name__ == '__main__':
    main()

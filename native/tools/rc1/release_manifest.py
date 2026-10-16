"""Verify matching clean-commit RC packages and write their external artifact manifest."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'native/targets/tdeck'))
from package_launcher import validate


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_windows_build_info(info, commit, version="0.5.0-rc1"):
    versions = re.findall(r'^Version:\s*(.*?)\s*$', info, re.MULTILINE)
    sources = re.findall(r'^Source:\s*(.*?)\s*$', info, re.MULTILINE)
    if versions != [version]:
        raise ValueError(f'Windows package must identify exactly {version}')
    if len(sources) != 1 or not re.fullmatch(re.escape(commit) + r' \([^()]+\)', sources[0]):
        raise ValueError('Wrong or dirty Windows package source provenance')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', choices=('0.5.0-rc1', '0.5.0-rc2'), default='0.5.0-rc1')
    parser.add_argument('--windows-zip', type=Path, required=True)
    parser.add_argument('--windows-build', type=Path, required=True)
    parser.add_argument('--firmware-bin', type=Path, required=True)
    parser.add_argument('--firmware-build', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--smoke-summary', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    git = lambda *a: subprocess.check_output(['git', *a], cwd=ROOT, text=True).strip()
    commit = git('rev-parse', 'HEAD')
    if git('status', '--porcelain', '--untracked-files=normal'):
        raise ValueError('RC source tree is not clean')
    short = commit[:12]
    allowed = {'UltimaVNative.exe', 'SDL3.dll', 'README-Windows.txt', 'CONTROLS.txt',
               'Prepare-Game-Data.cmd', 'prepare-assets.mjs', 'LICENSE.txt', 'NOTICE.txt',
               'THIRD-PARTY-LICENSES.txt', 'BUILD-INFO.txt'}
    with zipfile.ZipFile(args.windows_zip) as archive:
        if set(archive.namelist()) != allowed or len(archive.namelist()) != len(allowed):
            raise ValueError('Windows ZIP whitelist/duplicate-file violation')
        if archive.testzip() is not None:
            raise ValueError('Corrupt ZIP entry')
        exe = archive.read('UltimaVNative.exe')
        if exe != (args.windows_build / 'UltimaVNative.exe').read_bytes():
            raise ValueError('ZIP executable differs from the intended final build')
        if archive.read('SDL3.dll') != (args.windows_build / 'SDL3.dll').read_bytes():
            raise ValueError('ZIP runtime differs from the intended final build')
        pe = struct.unpack_from('<I', exe, 0x3c)[0]
        if exe[:2] != b'MZ' or exe[pe:pe+4] != b'PE\0\0' or struct.unpack_from('<H', exe, pe+4)[0] != 0x8664:
            raise ValueError('Expected a Windows AMD64 executable')
        info = archive.read('BUILD-INFO.txt').decode('utf-8-sig')
        verify_windows_build_info(info, commit, args.version)
        if short.encode() not in exe or (short + '-dirty').encode() in exe or (args.version + '\0').encode() not in exe:
            raise ValueError('Wrong embedded Windows version/commit')
    data = args.firmware_bin.read_bytes()
    validate(data)
    meta = json.loads((args.firmware_build / 'project_description.json').read_text(encoding='utf-8'))
    if data != (args.firmware_build / meta['app_bin']).read_bytes():
        raise ValueError('Launcher package differs from final firmware build')
    if data[48:80].split(b'\0')[0] != args.version.encode() or short.encode() not in data or (short + '-dirty').encode() in data:
        raise ValueError('Wrong firmware version/source identity')
    evidence = args.evidence
    tests = {}
    for target, filename, minimum in [('host', 'final-host-ctest.log', 213 if args.version == '0.5.0-rc2' else 212), ('windows', 'final-windows-build-test.log', 40 if args.version == '0.5.0-rc2' else 38)]:
        log = (evidence / filename).read_text(encoding='utf-8', errors='replace')
        match = re.search(r'100% tests passed(?:, 0 tests failed)? out of (\d+)', log)
        if not match or int(match[1]) < minimum:
            raise ValueError(f'Incomplete final {target} suite')
        tests[target] = {'passed': int(match[1]), 'failed': 0, 'log_sha256': sha(evidence / filename)}
    diff = json.loads((evidence / 'final-differential/differential-summary.json').read_text(encoding='utf-8'))
    if args.version == '0.5.0-rc1':
        if any(not diff[mode]['byte_identical'] or diff[mode]['different_walks'] for mode in ('paced', 'unpaced')):
            raise ValueError('Final differential failed')
    else:
        expected = {('E-3', step) for step in range(5, 11)} | {('E-5', 5), ('E-5', 6)}
        for mode in ('paced', 'unpaced'):
            d = diff[mode]
            changes = d['expected_changes']
            if (d['walks'] != 1065 or d['snapshots'] != 8514 or d['different_walks'] != 2
                or not d['classified_presentation_only'] or d['unexpected_differences'] != 0
                or len(changes) != len(expected) or {(c['walk'], c['step']) for c in changes} != expected
                or any(c['fields'] != ['gram'] for c in changes)):
                raise ValueError('RC2 differential contains unclassified or missing changes')
    if '8/8 boundary mutants killed' not in (evidence / 'final-boundary-mutations.log').read_text(encoding='utf-8'):
        raise ValueError('Final boundary mutations incomplete')
    if '2/2 release tooling mutants killed' not in (evidence / 'final-release-tooling-mutations.log').read_text(encoding='utf-8'):
        raise ValueError('Release tooling mutation checks incomplete')
    smoke = json.loads(args.smoke_summary.read_text(encoding='utf-8'))
    if smoke['git_commit'] != commit or smoke['zip_sha256'] != sha(args.windows_zip):
        raise ValueError('Package smoke is from another commit or ZIP')
    ts_log = (evidence / 'final-typescript.log').read_text(encoding='utf-8', errors='replace')
    if not re.search(r'Tests\s+11 passed \(11\)', ts_log):
        raise ValueError('Relevant TypeScript verification incomplete')
    tooling_log = (evidence / 'final-tooling-regression.log').read_text(encoding='utf-8', errors='replace')
    expected_tooling = 'Ran 7 tests' if args.version == '0.5.0-rc2' else 'Ran 5 tests'
    if expected_tooling not in tooling_log or not re.search(r'^OK\s*$', tooling_log, re.MULTILINE):
        raise ValueError('Release provenance regression checks incomplete')
    firmware = json.loads((evidence / 'final-firmware.json').read_text(encoding='utf-8'))
    if firmware['image_sha256'] != sha(args.firmware_bin):
        raise ValueError('Firmware metrics do not describe this package')
    bundle_manifest = json.loads((ROOT / 'native/tools/asset-prep/dist/prepare-assets.manifest.json').read_text(encoding='utf-8'))
    dependencies = {'SDL3': '3.4.16', **{name: json.loads((ROOT / 'node_modules' / name / 'package.json').read_text(encoding='utf-8'))['version'] for name in bundle_manifest['packages']}}
    manifest = {'version': args.version, 'rc_status': 'hardware validation pending',
                'git_commit': commit, 'branch': git('branch', '--show-current'), 'source_tree_clean': True,
                'build_date_utc': datetime.now(timezone.utc).isoformat(),
                'toolchains': {'windows': [line.strip() for line in info.splitlines() if line.startswith(('Toolchain:', 'Config:', 'SDL:', 'Asset prep:'))],
                               'firmware_idf': meta['git_revision'],
                               'firmware_compiler': subprocess.check_output([meta['c_compiler'], '--version'], text=True).splitlines()[0],
                               'node': subprocess.check_output(['node', '--version'], text=True).strip(),
                               'host_compiler': subprocess.check_output(['c++', '--version'], text=True).splitlines()[0]},
                'dependencies': dependencies,
                'artifacts': {key: {'filename': path.name, 'path': str(path.resolve()), 'bytes': path.stat().st_size, 'sha256': sha(path)}
                              for key, path in [('windows_zip', args.windows_zip), ('tdeck_launcher_bin', args.firmware_bin)]},
                'firmware': {key: firmware[key] for key in ['image_bytes', 'partition_bytes', 'partition_free_bytes',
                              'launcher_allocation_bytes', 'launcher_free_bytes', 'elf_sections', 'largest_frames']},
                'tests': {**tests, 'differential': diff, 'boundary_mutations': '8/8 killed; restored PASS',
                          'package_smoke': smoke, 'relevant_typescript': {'passed': 11, 'failed': 0},
                          'release_tooling_regressions': {'passed': 7 if args.version == '0.5.0-rc2' else 5, 'failed': 0},
                          'release_tooling_mutations': '2/2 killed; restored tests PASS'},
                'hardware': {'earlier_spp1': 'PASS, user-reported', 'this_rc': 'NOT RUN',
                             'checklist': str(ROOT / ('docs/V050_FINAL_UI_POLISH.md' if args.version == '0.5.0-rc2' else 'docs/V050_RC1_HARDWARE_CHECKLIST.md'))}}
    args.output.write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps({'git_commit': commit, 'artifacts': manifest['artifacts'], 'rc_status': manifest['rc_status']}, indent=2))


if __name__ == '__main__':
    main()

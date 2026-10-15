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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
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
        if f'Source:       {commit} (' not in info or 'UNCOMMITTED CHANGES' in info:
            raise ValueError('Wrong Windows package source provenance')
        if short.encode() not in exe or (short + '-dirty').encode() in exe or 'Version:      0.5.0-rc1' not in info:
            raise ValueError('Wrong embedded Windows version/commit')
    data = args.firmware_bin.read_bytes()
    validate(data)
    meta = json.loads((args.firmware_build / 'project_description.json').read_text())
    if data != (args.firmware_build / meta['app_bin']).read_bytes():
        raise ValueError('Launcher package differs from final firmware build')
    if data[48:80].split(b'\0')[0] != b'0.5.0-rc1' or short.encode() not in data or (short + '-dirty').encode() in data:
        raise ValueError('Wrong firmware version/source identity')
    evidence = args.evidence
    tests = {}
    for target, filename, minimum in [('host', 'final-host-ctest.log', 212), ('windows', 'final-windows-build-test.log', 38)]:
        log = (evidence / filename).read_text(errors='replace')
        match = re.search(r'100% tests passed(?:, 0 tests failed)? out of (\d+)', log)
        if not match or int(match[1]) < minimum:
            raise ValueError(f'Incomplete final {target} suite')
        tests[target] = {'passed': int(match[1]), 'failed': 0, 'log_sha256': sha(evidence / filename)}
    diff = json.loads((evidence / 'final-differential/differential-summary.json').read_text())
    if any(not diff[mode]['byte_identical'] or diff[mode]['different_walks'] for mode in ('paced', 'unpaced')):
        raise ValueError('Final differential failed')
    if '8/8 boundary mutants killed' not in (evidence / 'final-boundary-mutations.log').read_text():
        raise ValueError('Final boundary mutations incomplete')
    smoke = json.loads(args.smoke_summary.read_text())
    if smoke['git_commit'] != commit or smoke['zip_sha256'] != sha(args.windows_zip):
        raise ValueError('Package smoke is from another commit or ZIP')
    firmware = json.loads((evidence / 'final-firmware.json').read_text())
    if firmware['image_sha256'] != sha(args.firmware_bin):
        raise ValueError('Firmware metrics do not describe this package')
    bundle_manifest = json.loads((ROOT / 'native/tools/asset-prep/dist/prepare-assets.manifest.json').read_text())
    dependencies = {'SDL3': '3.4.16', **{name: json.loads((ROOT / 'node_modules' / name / 'package.json').read_text())['version'] for name in bundle_manifest['packages']}}
    manifest = {'version': '0.5.0-rc1', 'rc_status': 'hardware validation pending',
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
                          'package_smoke': smoke, 'relevant_typescript': 'see final-typescript.log'},
                'hardware': {'earlier_spp1': 'PASS, user-reported', 'this_rc': 'NOT RUN',
                             'checklist': str(ROOT / 'docs/V050_RC1_HARDWARE_CHECKLIST.md')}}
    args.output.write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps({'git_commit': commit, 'artifacts': manifest['artifacts'], 'rc_status': manifest['rc_status']}, indent=2))


if __name__ == '__main__':
    main()

"""Extract a new RC ZIP and test missing assets, portable data prep, and real scenarios."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[3]

# Keep the required developer opt-in in step with scenarios/CMakeLists.txt.
SMOKE_CASES = [
    ('portable-boot', 'boot_title', 'original', 'boot_title', False),
    ('look-original', 'look_signpost', 'original', 'look_signpost_original', True),
    ('look-enhanced', 'look_signpost', 'enhanced', 'look_signpost_enhanced', True),
    ('shop-enhanced', 'enhanced_shop_prices', 'enhanced', 'enhanced_shop_prices', True),
    ('dos-panels-original', 'original_dos_panels', 'original', 'original_dos_panels', True),
]


def scenario_arguments(exe, script, mode, developer):
    arguments = [str(exe), '--autotest', str(script), '--mode', mode]
    if developer:
        arguments.append('--developer')
    return arguments


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('zip', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists():
        raise ValueError('Smoke output must be a fresh directory; existing evidence is never replaced')
    output.mkdir(parents=True)
    extract = output / 'extracted'
    with zipfile.ZipFile(args.zip) as archive:
        if any(Path(name).name != name or ':' in name or '\\' in name for name in archive.namelist()):
            raise ValueError('ZIP must contain only flat package files')
        archive.extractall(extract)
    exe = extract / 'UltimaVNative.exe'
    (extract / 'portable.txt').write_text('RC package smoke test\n')
    env = dict(os.environ, SDL_VIDEODRIVER='offscreen', SDL_AUDIODRIVER='dummy')
    env['PATH'] = str(extract) + os.pathsep + os.environ.get('SystemRoot', 'C:\\Windows') + '\\System32' + os.pathsep + os.environ.get('SystemRoot', 'C:\\Windows')
    script = ROOT / 'native/targets/windows/scenarios/boot_title.txt'
    result = subprocess.run([str(exe), '--autotest', str(script)], cwd=output, env=env, capture_output=True, timeout=30)
    log = extract / 'data/ultima5/logs/windows.log'
    if result.returncode != 10 or 'resource packs are missing or unreadable' not in log.read_text(encoding='utf-8', errors='replace'):
        raise ValueError(f'Missing-asset handling failed: exit {result.returncode}')
    (output / 'missing-assets.log').write_bytes(result.stdout + result.stderr + log.read_bytes())
    print('PASS missing assets: exit 10 and actionable pack diagnostic', flush=True)
    node = shutil.which('node')
    if not node:
        raise ValueError('Node.js is needed for the player data-preparation smoke')
    with (output / 'data-preparation.log').open('wb') as out:
        subprocess.run([node, str(extract / 'prepare-assets.mjs'), '--game', str(ROOT / 'original/u5/ultima5'),
                        '--out', str(extract / 'data/ultima5')], cwd=output, stdout=out, stderr=subprocess.STDOUT, check=True, timeout=300)
    pack_hashes = {}
    for pack in sorted((ROOT / 'native/assets').glob('openu5-*.bin')):
        made = extract / 'data/ultima5' / pack.name
        if made.read_bytes() != pack.read_bytes():
            raise ValueError(f'Prepared pack does not match established input: {pack.name}')
        pack_hashes[pack.name] = hashlib.sha256(made.read_bytes()).hexdigest()
    print('PASS bundled data prep: all three private packs match established hashes', flush=True)
    commit = subprocess.check_output(['git', 'rev-parse', '--short=12', 'HEAD'], cwd=ROOT, text=True).strip()
    for name, scenario, mode, _ctest_name, developer in SMOKE_CASES:
        work = output / name
        work.mkdir()
        args_exe = scenario_arguments(exe, ROOT / f'native/targets/windows/scenarios/{scenario}.txt', mode, developer)
        data = extract / 'data' if name == 'portable-boot' else work
        if name != 'portable-boot':
            (work / 'ultima5').mkdir()
            for pack in (extract / 'data/ultima5').glob('openu5-*.bin'):
                shutil.copy2(pack, work / 'ultima5' / pack.name)
            args_exe += ['--data-dir', str(work)]
        with (work / 'run.log').open('wb') as out:
            subprocess.run(args_exe, cwd=work, env=env, stdout=out, stderr=subprocess.STDOUT, check=True, timeout=240)
        content = (data / 'ultima5/logs/windows.log').read_text(encoding='utf-8', errors='replace')
        if f'git {commit},' not in content or f'{commit}-dirty' in content:
            raise ValueError(f'Wrong packaged source identity in {name}')
        print('PASS', name, flush=True)
    summary = {'git_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
               'zip_sha256': hashlib.sha256(args.zip.read_bytes()).hexdigest(),
               'missing_assets_exit': 10, 'portable_launch': True, 'bundled_data_prep': True,
               'prepared_private_pack_hashes': pack_hashes, 'scenarios_passed': [case[0] for case in SMOKE_CASES]}
    (output / 'package-smoke-summary.json').write_text(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()

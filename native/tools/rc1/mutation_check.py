"""Mutate the new capability contracts; restore exact source bytes after every run."""
import argparse
import copy
import json
from pathlib import Path
import subprocess
import sys

from verify_capabilities import verify

ROOT = Path(__file__).resolve().parents[3]
HEADER = ROOT / 'native/core/include/openu5/platform_profile.h'
MUTANTS = [
    ('typed_spell', 'platform_typed_spell_names() { return false; }', 'platform_typed_spell_names() { return true; }'),
    ('quit_save', 'platform_quit_save_key() { return false; }', 'platform_quit_save_key() { return true; }'),
    ('dos_interface', 'platform_dos_interface() { return false; }', 'platform_dos_interface() { return true; }'),
    ('dos_screen', 'platform_dos_screen() { return false; }', 'platform_dos_screen() { return true; }'),
    ('dos_text', 'platform_dos_text(DosText, int) { return nullptr; }', 'platform_dos_text(DosText, int) { return "mutant"; }'),
    ('board_capability', 'kPlatformSupportsDosPresentation = false;', 'kPlatformSupportsDosPresentation = true;'),
]


def build_and_run(build):
    subprocess.run(['cmake', '--build', str(build), '--target', 'rc1_profile_handheld', '-j', '2'], check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return subprocess.run([str(build / 'rc1_profile_handheld.exe')], capture_output=True, text=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('--firmware-commands', type=Path, required=True)
    parser.add_argument('--windows-commands', type=Path, required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    raw = HEADER.read_bytes()
    try:
        assert build_and_run(build).returncode == 0, 'Unmutated capability test must pass'
        for name, old, new in MUTANTS:
            source = raw.decode('utf-8')
            assert source.count(old) == 1, f'Unmatched mutant {name}'
            HEADER.write_bytes(source.replace(old, new).encode())
            result = build_and_run(build)
            if result.returncode == 0:
                raise ValueError(f'SURVIVED {name}')
            print('KILLED', name, result.stderr.splitlines()[0], flush=True)
            HEADER.write_bytes(raw)
    finally:
        HEADER.write_bytes(raw)
        restored = build_and_run(build)
        if restored.returncode:
            raise ValueError('Restored capability test failed')
    firmware = json.loads(args.firmware_commands.read_text(encoding='utf-8'))
    windows = json.loads(args.windows_commands.read_text(encoding='utf-8'))
    verify(firmware, True); verify(windows, False)
    bad = copy.deepcopy(firmware)
    for entry in bad:
        source = entry['file'].replace('\\', '/')
        if ('/native/core/src/' in source or '/native/targets/tdeck/main/' in source) and 'OPENU5_FIXED_HANDHELD_PROFILE=1' in entry.get('command', ''):
            entry['command'] = entry['command'].replace('-DOPENU5_FIXED_HANDHELD_PROFILE=1', '')
            break
    try:
        verify(bad, True)
    except ValueError:
        print('KILLED missing firmware TU capability', flush=True)
    else:
        raise ValueError('SURVIVED missing firmware capability')
    bad = copy.deepcopy(windows)
    for entry in bad:
        if '/native/core/src/' in entry['file'].replace('\\', '/'):
            entry['command'] += ' -DOPENU5_FIXED_HANDHELD_PROFILE=1'
            break
    try:
        verify(bad, False)
    except ValueError:
        print('KILLED leaked Windows TU capability', flush=True)
    else:
        raise ValueError('SURVIVED Windows capability leak')
    print('8/8 boundary mutants killed; source restored; unmutated test PASS')


if __name__ == '__main__':
    main()

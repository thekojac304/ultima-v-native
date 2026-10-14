#!/usr/bin/env python
"""SPP1 RED-first proof: run the CURRENT tests against a baseline commit's production files.

    python native/core/tools/spp1_red_first.py <build-dir> <baseline-commit> <group> [--target T] [--test T]

For the chosen group the listed production files are replaced by their blobs at
<baseline-commit> (``git show``), the test target is rebuilt and run, and the files are
restored and touched (so ninja never keeps an object built from the baseline). The
output, with the RED lines, goes to stdout; the exit code is the test's.

Groups name the files each SPP1 correction changed (docs/SHARED_PRESERVATION_PARITY_SPP1.md).
"""
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
CMAKE = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/cmake.exe'
# cmake runs ninja and g++ from PATH: put the toolchain first, whatever the caller's PATH.
os.environ['PATH'] = os.path.dirname(CMAKE).replace('/', os.sep) + os.pathsep + os.environ.get('PATH', '')
PACK = os.path.join(ROOT, 'native', 'assets', 'openu5-alpha1-resources.bin')

GROUPS = {
    'G': ['native/core/include/openu5/commands.h', 'native/core/src/commands.cpp',
          'native/core/include/openu5/ui_session.h', 'native/core/src/ui_session.cpp'],
    'F': ['native/core/include/openu5/magic.h', 'native/core/src/magic.cpp', 'native/core/src/combat.cpp',
          'native/core/src/ui_session.cpp', 'native/core/src/world_magic.cpp', 'native/core/src/commands.cpp',
          'native/core/src/dungeon_orchestration.cpp', 'native/targets/tdeck/main/alpha_runtime.cpp',
          'native/targets/tdeck/main/alpha_runtime.h'],
}


def show(commit, path):
    return subprocess.run(['git', '-C', ROOT, 'show', f'{commit}:{path}'], capture_output=True, check=True).stdout


def main():
    args = sys.argv[1:]
    if len(args) < 3:
        print(__doc__)
        return 2
    build, commit, group = args[0], args[1], args[2]
    target = 'spp1_runtime'
    test_cmd = None
    if '--target' in args:
        target = args[args.index('--target') + 1]
    if '--test' in args:
        test_cmd = args[args.index('--test') + 1]
    files = GROUPS[group]
    saved = {}
    try:
        for f in files:
            p = os.path.join(ROOT, f)
            saved[f] = open(p, 'rb').read()
            try:
                blob = show(commit, f)
            except subprocess.CalledProcessError:
                continue  # new in the batch: the baseline had no such file
            open(p, 'wb').write(blob)
        b = subprocess.run([CMAKE, '--build', build, '--target', target], capture_output=True, text=True)
        if b.returncode:
            print('BUILD FAILED (the baseline production files do not build with the current tests)')
            print(b.stdout[-4000:], b.stderr[-2000:])
            return 3
        exe = os.path.join(build, (test_cmd or target) + '.exe')
        r = subprocess.run([exe, PACK], capture_output=True, text=True, errors='replace')
        lines = [l for l in r.stdout.splitlines() if l.startswith(('RED', 'GREEN', 'SPP1'))]
        print('\n'.join(lines))
        print(f'exit {r.returncode}')
        return r.returncode
    finally:
        for f, data in saved.items():
            p = os.path.join(ROOT, f)
            open(p, 'wb').write(data)
            os.utime(p, None)
        subprocess.run([CMAKE, '--build', build, '--target', target], capture_output=True)


if __name__ == '__main__':
    sys.exit(main())

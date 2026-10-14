#!/usr/bin/env python
"""SPP2 RED-first proof: run the CURRENT tests against a baseline commit's production files.

    python native/core/tools/spp2_red_first.py <build-dir> <baseline-commit> <group> [--target T]...

For the chosen group the listed production files are replaced by their blobs at
<baseline-commit> (``git show``), each test target is rebuilt and run, and the files are
restored and touched (so ninja never keeps an object built from the baseline). The RED /
GREEN lines go to stdout; the exit code is the worst test's.

Groups name the files each SPP2 correction changes (docs/SHARED_PRESERVATION_PARITY_SPP2.md).
The default targets are spp2_runtime and spp1_runtime.
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
    # M / N: the dispatcher-turn latch settled at the command's end (render) and followed by its
    # NPC approach and music.
    'M': ['native/targets/tdeck/main/alpha_runtime.cpp', 'native/targets/tdeck/main/alpha_runtime.h'],
    # T: TALK.OVL's own answers in a town (D-111)
    'T': ['native/core/src/dialogue_orchestration.cpp'],
    # G: Ctrl-S and the overworld "What?" (D-108, D-112). ui_input_adapter.h is NOT swapped: the
    # tests name DeviceShortcut::DosSound, so the baseline is the commit that only appended it.
    'G': ['native/targets/tdeck/main/alpha_runtime.cpp', 'native/targets/tdeck/main/alpha_runtime.h',
          'native/core/src/commands.cpp', 'native/core/src/ui_session.cpp'],
}
EXTRA = {'spp2_runtime': [PACK, os.path.join(ROOT, 'native', 'assets', 'openu5-audio.bin')]}


def show(commit, path):
    return subprocess.run(['git', '-C', ROOT, 'show', f'{commit}:{path}'], capture_output=True, check=True).stdout


def main():
    args = sys.argv[1:]
    if len(args) < 3:
        print(__doc__)
        return 2
    build, commit, group = args[0], args[1], args[2]
    targets = [args[i + 1] for i, a in enumerate(args) if a == '--target'] or ['spp2_runtime', 'spp1_runtime']
    files = GROUPS[group]
    saved = {}
    worst = 0
    try:
        for f in files:
            p = os.path.join(ROOT, f)
            saved[f] = open(p, 'rb').read()
            try:
                blob = show(commit, f)
            except subprocess.CalledProcessError:
                continue  # new in the batch: the baseline had no such file
            open(p, 'wb').write(blob)
        for target in targets:
            b = subprocess.run([CMAKE, '--build', build, '--target', target], capture_output=True, text=True)
            if b.returncode:
                print(f'{target}: BUILD FAILED (the baseline production files do not build with the current tests)')
                print(b.stdout[-4000:], b.stderr[-2000:])
                worst = max(worst, 3)
                continue
            exe = os.path.join(build, target + '.exe')
            cmd = [exe] + EXTRA.get(target, [PACK])
            r = subprocess.run(cmd, capture_output=True, text=True, errors='replace')
            lines = [l for l in r.stdout.splitlines() if l.startswith(('RED', 'GREEN', 'SPP'))]
            print(f'== {target} against {commit} ({group})')
            print('\n'.join(lines))
            print(f'exit {r.returncode}')
            worst = max(worst, r.returncode)
        return worst
    finally:
        for f, data in saved.items():
            p = os.path.join(ROOT, f)
            open(p, 'wb').write(data)
            os.utime(p, None)
        for target in targets:
            subprocess.run([CMAKE, '--build', build, '--target', target], capture_output=True)


if __name__ == '__main__':
    sys.exit(main())

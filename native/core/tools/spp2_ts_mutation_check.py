#!/usr/bin/env python
"""SPP2 TypeScript-reference mutation check: each mutant undoes one part of an SPP2 correction to
the reference (game/src); the named vitest files must go RED. Files are restored byte for byte.

    python native/core/tools/spp2_ts_mutation_check.py [ID ...]
"""
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
GAME = os.path.join(ROOT, 'game')
GTS = 'game/src/core/game.ts'
TALK = ['tests/spp2-talk-refusal.test.ts', 'tests/contexto-tap-equivalencia.test.ts']

# (id, file, old, new, vitest files)
MUTANTS = [
    ('TS-T1', GTS, '    if (!npc) return "Nobody\'s here!";', '    if (!npc) return "Funny, no response!";', TALK),
    ('TS-T2', GTS, 'return npc.type === 0x70 && !approach ?', 'return false && !approach ?', TALK),
    ('TS-T3', GTS, 'return npc.type === 0x70 && !approach ?', 'return npc.type === 0x70 && (approach || !approach) ?', TALK),
    ('TS-T4', GTS, '    if (pos.location === 0 || !this.npcManager) return "Funny, no response!";',
     '    if (!this.npcManager) return "Funny, no response!";', TALK),
]


def vitest(files):
    npx = 'npx.cmd' if os.name == 'nt' else 'npx'
    r = subprocess.run([npx, 'vitest', 'run', *files], cwd=GAME, capture_output=True, text=True, errors='replace')
    return r.returncode == 0, r.stdout[-600:]


def main():
    pick = set(sys.argv[1:])
    chosen = [m for m in MUTANTS if not pick or m[0] in pick]
    tally = {'KILLED': 0, 'SURVIVED': 0, 'STALE': 0}
    for mid, rel, old, new, files in chosen:
        path = os.path.join(ROOT, rel)
        original = open(path, 'rb').read()
        text = original.decode('utf-8')
        if text.count(old) != 1:
            print(f'{mid} STALE (anchor count {text.count(old)}) {rel}')
            tally['STALE'] += 1
            continue
        try:
            open(path, 'wb').write(text.replace(old, new).encode('utf-8'))
            ok, _ = vitest(files)
            verdict = 'SURVIVED' if ok else 'KILLED'
            tally[verdict] += 1
            print(f'{mid} {verdict} {rel}')
        finally:
            open(path, 'wb').write(original)
    ok, tail = vitest(sorted({f for m in chosen for f in m[4]}))
    print('restored tree: ' + ('GREEN' if ok else 'NOT GREEN\n' + tail))
    print(' '.join(f'{k} {v}' for k, v in tally.items()))
    return 0 if tally['SURVIVED'] == tally['STALE'] == 0 and ok else 1


if __name__ == '__main__':
    sys.exit(main())

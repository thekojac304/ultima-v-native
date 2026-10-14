#!/usr/bin/env python
"""SPP2 mutation check: each mutant removes or moves one part of an SPP2 correction; the
group's tests must go RED. A mutant that does not build is reported INVALID, never KILLED.

    python native/core/tools/spp2_mutation_check.py <build-dir> [GROUP|ID ...]

Every anchor must occur exactly once (else the mutant is reported STALE). Files are
restored byte for byte and touched after each mutant, so ninja never reuses a mutated
object (the Batch 26 lesson). Mutants listed in EQUIVALENT are expected to survive and
say why.
"""
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
CMAKE = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/cmake.exe'
# cmake runs ninja and g++ from PATH: put the toolchain first, whatever the caller's PATH.
os.environ['PATH'] = os.path.dirname(CMAKE).replace('/', os.sep) + os.pathsep + os.environ.get('PATH', '')
PACK = os.path.join(ROOT, 'native', 'assets', 'openu5-alpha1-resources.bin')
AUDIO = os.path.join(ROOT, 'native', 'assets', 'openu5-audio.bin')
ARGS = {'spp2_runtime': [PACK, AUDIO]}
RT = 'native/targets/tdeck/main/alpha_runtime.cpp'

SPP2 = ('spp2_runtime',)
BOTH_RT = ('spp2_runtime', 'spp1_runtime')
# (id, group, file, old, new, targets, tests)
MUTANTS = [
    # M: the latch settled on the frame a timer-ended command ends
    ('M1', 'M', RT, '    if(settle_dispatcher_turn())dirty_reason_="dispatcher-turn";\n', '', SPP2, SPP2),
    ('M2', 'M', RT,
     '    if(service_hit_cue()){dirty_=true;dirty_reason_="combat-hit-cue";}\n'
     '    // SPP2. A command that ended on a timer',
     '    if(service_hit_cue()){dirty_=true;dirty_reason_="combat-hit-cue";}\n'
     '    if(false)// SPP2. A command that ended on a timer', SPP2, SPP2),
    # the frame-time settle moved BEFORE the pacer that ends the hold: the turn a frame late
    ('M3', 'M', RT,
     '    if(service_dialogue_pacer()){dirty_=true;dirty_reason_="dialogue-pause";}\n'
     '    if(service_poison_flash()){dirty_=true;dirty_reason_="poison-tick";}\n'
     '    if(service_hit_cue()){dirty_=true;dirty_reason_="combat-hit-cue";}\n',
     '    if(settle_dispatcher_turn())dirty_reason_="dispatcher-turn";\n'
     '    if(service_dialogue_pacer()){dirty_=true;dirty_reason_="dialogue-pause";}\n'
     '    if(service_poison_flash()){dirty_=true;dirty_reason_="poison-tick";}\n'
     '    if(service_hit_cue()){dirty_=true;dirty_reason_="combat-hit-cue";}\n    if(false)', SPP2, SPP2),
    # the fallback at the start of route_input (a timer that ran out before a frame saw it)
    ('M4', 'M', RT,
     '    // the turn before reading the next key.\n    settle_dispatcher_turn();\n',
     '    // the turn before reading the next key.\n', SPP2, SPP2),
    # N: what the turn brings follows it
    ('N1', 'N', RT, '    drain_pending_npc_initiation();\n    sync_music();dirty_=true;\n    return true;',
     '    sync_music();dirty_=true;\n    return true;', BOTH_RT, BOTH_RT),
    ('N2', 'N', RT, '    drain_pending_npc_initiation();\n    sync_music();dirty_=true;\n    return true;',
     '    drain_pending_npc_initiation();\n    dirty_=true;\n    return true;', SPP2, SPP2),
    # the re-entrancy guard: never settle inside command()
    ('N3', 'N', RT, '    if(!dispatcher_turn_.armed||command_depth_)return false;', '    if(!dispatcher_turn_.armed)return false;', SPP2, SPP2),
]
EQUIVALENT = {
    'N3': 'defensive: the only render() nested in command() is the unpaced Camp scene (route_event -> '
          'apply_camp_scene_event), and Camp (H) never arms the latch, so no test can reach it',
}


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True, errors='replace')


def build(build_dir, targets):
    r = run([CMAKE, '--build', build_dir, '--target', *targets])
    return r.returncode == 0


def tests_pass(build_dir, tests):
    for t in tests:
        r = run([os.path.join(build_dir, t + '.exe')] + ARGS.get(t, [PACK]))
        if r.returncode:
            reds = [l for l in r.stdout.splitlines() if l.startswith('RED')]
            return False, reds[:3]
    return True, []


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    build_dir = sys.argv[1]
    pick = set(sys.argv[2:])
    chosen = [m for m in MUTANTS if not pick or m[0] in pick or m[1] in pick]
    tally = {'KILLED': 0, 'SURVIVED': 0, 'EQUIVALENT': 0, 'INVALID': 0, 'STALE': 0}
    for mid, group, rel, old, new, targets, tests in chosen:
        path = os.path.join(ROOT, rel)
        original = open(path, 'rb').read()
        text = original.decode('utf-8')
        if text.count(old) != 1:
            print(f'{mid} STALE (anchor count {text.count(old)}) {rel}')
            tally['STALE'] += 1
            continue
        try:
            open(path, 'wb').write(text.replace(old, new).encode('utf-8'))
            if not build(build_dir, targets):
                print(f'{mid} INVALID (does not build)')
                tally['INVALID'] += 1
                continue
            ok, reds = tests_pass(build_dir, tests)
            verdict = ('EQUIVALENT' if mid in EQUIVALENT else 'SURVIVED') if ok else 'KILLED'
            tally[verdict] += 1
            note = ('  (' + EQUIVALENT[mid] + ')') if ok and mid in EQUIVALENT else ''
            print(f'{mid} {verdict} {rel}' + ('' if ok else '  <- ' + ' | '.join(reds)) + note)
        finally:
            open(path, 'wb').write(original)
            os.utime(path, None)
    rebuilt = build(build_dir, sorted({t for m in chosen for t in m[5]}))
    ok, reds = tests_pass(build_dir, sorted({t for m in chosen for t in m[6]}))
    print('restored tree: ' + ('GREEN' if rebuilt and ok else 'NOT GREEN ' + ' | '.join(reds)))
    print(' '.join(f'{k} {v}' for k, v in tally.items()))
    return 0 if tally['SURVIVED'] == tally['INVALID'] == tally['STALE'] == 0 and rebuilt and ok else 1


if __name__ == '__main__':
    sys.exit(main())

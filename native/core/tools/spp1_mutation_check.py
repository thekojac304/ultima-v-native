#!/usr/bin/env python
"""SPP1 mutation check: each mutant removes or inverts one part of an SPP1 correction; the
group's tests must go RED. A mutant that does not build is reported INVALID, never KILLED.

    python native/core/tools/spp1_mutation_check.py <build-dir> [GROUP|ID ...]

Every anchor must occur exactly once (else the mutant is reported STALE). Files are
restored byte for byte and touched after each mutant, so ninja never reuses a mutated
object (the Batch 26 lesson).
"""
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
CMAKE = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/cmake.exe'
# cmake runs ninja and g++ from PATH: put the toolchain first, whatever the caller's PATH.
os.environ['PATH'] = os.path.dirname(CMAKE).replace('/', os.sep) + os.pathsep + os.environ.get('PATH', '')
PACK = os.path.join(ROOT, 'native', 'assets', 'openu5-alpha1-resources.bin')
RT = 'native/targets/tdeck/main/alpha_runtime.cpp'
CMD = 'native/core/src/commands.cpp'
UIS = 'native/core/src/ui_session.cpp'
CMB = 'native/core/src/combat.cpp'
MAG = 'native/core/src/magic.cpp'
WMG = 'native/core/src/world_magic.cpp'

# (id, group, file, old, new, targets, tests)
SPP1 = ('spp1_runtime',)
MUTANTS = [
    ('G1', 'G', CMD, 'if(cmd.item==0x0b)std::snprintf(text,sizeof(text),"%u",unsigned(c.game.karma));',
     'if(cmd.item==0x0b)std::snprintf(text,sizeof(text),"Karma %u",unsigned(c.game.karma));', SPP1, SPP1),
    ('G2', 'G', CMD, 'else std::snprintf(text,sizeof(text),"1.16");', 'else std::snprintf(text,sizeof(text),"1.0");', SPP1, SPP1),
    ('G3', 'G', CMD, 'auto turn=execute_dungeon_command(c,tick);turn.event_count+=result.event_count;return turn;',
     '(void)tick;return result;', SPP1, SPP1),
    ('G4', 'G', UIS, '    if (a.character == 0x0b || a.character == 0x16) { control_key(a.character); return true; }\n    const auto k = lower_ascii(a.character);\n    // Windows port, desktop Original mode: (Q)uit & Save, CAST2.OVL 0x10FE.',
     '    const auto k = lower_ascii(a.character);\n    // Windows port, desktop Original mode: (Q)uit & Save, CAST2.OVL 0x10FE.', SPP1, SPP1),
    ('G5', 'G', UIS, '    if (a.character == 0x0b || a.character == 0x16) { control_key(a.character); return true; } // SPP1 G\n', '', SPP1, SPP1),
    ('F1', 'F', RT, 'pending_combat_spell_=-1;pending_spell_caster_=-1;command(spent);\n    }\n    if(i.request==openu5::UiRequestId::Party)',
     'pending_combat_spell_=-1;pending_spell_caster_=-1;(void)spent;\n    }\n    if(i.request==openu5::UiRequestId::Party)', SPP1, SPP1),
    ('F2', 'F', RT, 'c.caster=pending_spell_caster_>=0?pending_spell_caster_:caster&&caster->member!=255',
     'c.caster=caster&&caster->member!=255', SPP1, SPP1),
    ('F3', 'F', RT, 'if(absorbed||gate!=openu5::CastGate::Ok){command(c);return;}', 'if(absorbed){command(c);return;}(void)gate;', SPP1, SPP1),
    ('F4', 'F', RT, '(pending_use_item_==6||(pending_use_item_>=8&&pending_use_item_<16))&&!context_.combat',
     '(pending_use_item_>=8&&pending_use_item_<16)&&!context_.combat', SPP1, SPP1),
    ('F5', 'F', RT, 'spent.item_named=dos;\n        pending_use_item_=-1;command(spent);',
     'spent.item_named=dos;\n        pending_use_item_=-1;if(dos)command(spent);', SPP1, SPP1),
    ('F6', 'F', CMB, '        if (member < 0) {\n            e.advance();\n            return CombatResult::Ok;\n        }',
     '        if (member < 0) {\n            return CombatResult::Ok;\n        }', SPP1, SPP1),
    ('F7', 'F', UIS, '         (platform_dos_interface() && request == UiRequestId::Inventory));',
     '         (platform_dos_interface() && (request == UiRequestId::Inventory || request == UiRequestId::Target)));', SPP1, SPP1),
    ('F8', 'F', MAG, '    if (caster.level < d->circle)\n        return CastGate::LowLevel;\n', '', SPP1, SPP1),
    ('F9', 'F', MAG, '    if (caster.current_mp < d->circle)\n        return CastGate::LowMp;\n', '', SPP1, SPP1),
]


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, errors='replace', **kw)


def build(build_dir, targets):
    r = run([CMAKE, '--build', build_dir, '--target', *targets])
    return r.returncode == 0


def tests_pass(build_dir, tests):
    for t in tests:
        r = run([os.path.join(build_dir, t + '.exe'), PACK])
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
    tally = {'KILLED': 0, 'SURVIVED': 0, 'INVALID': 0, 'STALE': 0}
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
            verdict = 'SURVIVED' if ok else 'KILLED'
            tally[verdict] += 1
            print(f'{mid} {verdict} {rel}' + ('' if ok else '  <- ' + ' | '.join(reds)))
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

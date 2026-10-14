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
CORE = ('spp1_core_tests',)
BOTH = ('spp1_core_tests', 'spp1_runtime')
DNG = 'native/core/src/dungeon.cpp'
ORC = 'native/core/src/dungeon_orchestration.cpp'
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
    ('A1', 'A', RT, '    if(!armed)return;\n    dispatcher_turn_.armed=true;', '    armed=false;if(!armed)return;\n    dispatcher_turn_.armed=true;', SPP1, SPP1),
    ('A2', 'A', RT, '    if(game_.turns_since_start!=latch.turns)return; // the command charged its own turn\n', '', SPP1, SPP1),
    ('A3', 'A', RT, 'town?"rznucmlgojspftx":"rznucmlgojspftxk";', 'town?"rznucmlgojspftx":"rznucmlgojspftxka";', SPP1, SPP1),
    ('A4', 'A', CMD, '        if (c.dungeon) {\n            Command tick; tick.kind = CommandKind::DungeonCommand; tick.item = int16_t(DungeonAction::Tick);\n            return execute_dungeon_command(c, tick);\n        }\n        const auto before = c.game.turns_since_start;',
     '        const auto before = c.game.turns_since_start;', SPP1, SPP1),
    ('A5', 'A', RT, '    if(!at_command_prompt())return; // still inside the command\n', '', SPP1, SPP1),
    ('A6', 'A', RT, 'const char *set=dungeon?"rznumcs":', 'const char *set=dungeon?"cs":', SPP1, SPP1),
    ('A7', 'A', RT, '           pending_combat_spell_<0&&pending_ready_member_<0&&pending_use_item_<0&&pending_order_from_<0&&',
     '           pending_combat_spell_<0&&pending_use_item_<0&&pending_order_from_<0&&', SPP1, SPP1),
    ('C1', 'C', CMB, "        if (g.keys <= 0) { e.message(\"No Keys!\", a->id); e.advance(); break; }\n        if (x < 0 || x > 3) { e.advance(); break; }\n        static constexpr int xs[] = {1, -1, 0, 0}, ys[] = {0, 0, 1, -1};\n        const int tx = a->position.x + xs[x], ty = a->position.y + ys[x];\n        const int key",
     "        if (x < 0 || x > 3) { e.advance(); break; }\n        static constexpr int xs[] = {1, -1, 0, 0}, ys[] = {0, 0, 1, -1};\n        const int tx = a->position.x + xs[x], ty = a->position.y + ys[x];\n        const int key", CORE, CORE),
    ('C2', 'C', CMB, "        } else if (t == 0x97 || t == 0x98) { // a magic lock always breaks the key, no draw (0x0e1c)\n            broke();",
     "        } else if (t == 0x97 || t == 0x98) { // a magic lock always breaks the key, no draw (0x0e1c)\n            (void)e.rand(0, 29); broke();", CORE, CORE),
    ('C3', 'C', CMB, 'e.message("Unlocked", a->id); }', 'e.message("Unlocked!", a->id); }', CORE, CORE),
    ('C4', 'C', CMB, "            if (lock < 0x80) broke(); // untrapped: the key breaks, no draw (0x0bcb)",
     "            if (lock < 0x80) { (void)e.rand(1, 30); broke(); }", CORE, CORE),
    ('C5', 'C', CMB, "            if (e.occupant(px, py) || s.loot[py * 11 + px] > 0) return true;",
     "            if (e.occupant(px, py)) return true;", CORE, CORE),
    ('C6', 'C', CMB, '            e.message("Won\'t budge", a->id);\n            e.advance();', '            e.message("Won\'t budge!", a->id);\n            e.advance();', CORE, CORE),
    ('C7', 'C', CMB, "int16_t(turns ? cls + (facing[x] ^ 2) : t);", "int16_t(turns ? cls + facing[x] : t);", CORE, CORE),
    ('C8', 'C', CMB, "const int fill = (t & 0xfc) == 0xb4 ? 0x45 : 0x44, cls = t & 0xfc;", "const int fill = 0x44, cls = t & 0xfc;", CORE, CORE),
    ('C9', 'C', CMB, "            if (plate) { e.triggers(tx, ty); e.advance(); break; }", "            (void)plate;", CORE, CORE),
    ('C10', 'C', CMB, '        if (x) e.message("No effect!", a->id);\n        e.advance();\n        break;', '        e.message("No effect!", a->id);\n        e.advance();\n        break;', CORE, CORE),
    ('C11', 'C', CMB, '        if (x) e.message("No effect!", a->id);\n        e.advance();\n        break;', '        if (x) e.message("No effect!", a->id);\n        break;', CORE, CORE),
    ('C12', 'C', UIS, "              if (!keys_owned_) break;\n", "", SPP1, SPP1),
    ('C13', 'C', UIS, "                       pending_command_.kind == CommandKind::CombatSearch ||\n                       pending_command_.kind == CommandKind::CombatJimmy ||\n                       pending_command_.kind == CommandKind::CombatPush;",
     "                       pending_command_.kind == CommandKind::CombatSearch;", SPP1, SPP1),
    ('C14', 'C', UIS, "            if (arena) { Command c; c.kind = CommandKind::CombatYell; command(c); }\n", "            (void)arena;\n", SPP1, SPP1),
    ('E1', 'E', DNG, "        if (!g.torch_turns && t.light_spell_minutes <= 0) {\n            r.msg(\"You see:\\ndarkness.\");\n            return;\n        }\n        if (dir < 0 || dir > 3) return; // Space",
     "        if (dir < 0 || dir > 3) return; // Space", BOTH, BOTH),
    ('E2', 'E', DNG, "        auto &ch = g.party.characters[member];\n        if (raw == 0x50)", "        auto &ch = g.party.characters[0];\n        if (raw == 0x50)", BOTH, BOTH),
    ('E3', 'E', ORC, "    if (action != DungeonAction::Attack && action != DungeonAction::LookDrink)\n        tr = advance_turn",
     "    if (action != DungeonAction::Attack)\n        tr = advance_turn", BOTH, BOTH),
    ('E4', 'E', DNG, "        if (raw == 0x61) raw = 0; // 0x0070: exactly the hidden pit-fall reads as a passage\n", "", BOTH, BOTH),
    ('E5', 'E', DNG, "        else if (raw == 0x51) { ch.current_hp = ch.max_hp;", "        else if ((raw & 7) == 1) { ch.current_hp = ch.max_hp;", BOTH, BOTH),
    ('E6', 'E', UIS, "                    cancelled_dungeon.hours = int16_t(kDungeonLookPass); command(cancelled_dungeon);",
     "                    (void)kDungeonLookPass;", BOTH, BOTH),
    ('E7', 'E', RT, "        dungeon_look(-1,false); // SPP1 E: ESC at \"Player: \" is \"None!\" and the Look's turn\n", "", BOTH, BOTH),
    ('E8', 'E', UIS, "        if (dos) { append(UiTextChannel::Message, \"D-What?\"); return true; }\n        { command_echo(\"Look...\");",
     "        { command_echo(\"Look...\");", BOTH, BOTH),
    ('E9', 'E', DNG, "                 : r.rand(1, 255) == 255 ? \"an unfortunate software pirate.\"", "                 : false ? \"an unfortunate software pirate.\"", BOTH, BOTH),
    ('D1', 'D', MAG, "        if (kind == MagicEffect::Repel || kind == MagicEffect::Blink || kind == MagicEffect::Reveal)\n            return CastTargetPrompt::None;\n",
     "        if (false && kind == MagicEffect::Repel)\n            return CastTargetPrompt::None;\n", ('spp1_runtime', 'batch5_tests'), ('spp1_runtime', 'batch5_tests')),
    ('D2', 'D', MAG, "    case MagicEffect::Poof:\n        return CastTargetPrompt::WorldDirection;", "        return CastTargetPrompt::WorldDirection;", SPP1, SPP1),
    ('D3', 'D', WMG, "if(tile==0x5b||(tile>=0x90&&tile<=0x93)", "if((tile>=0x90&&tile<=0x93)", SPP1, SPP1),
    ('D4', 'D', RT, "            if(spell==28||spell==40||spell==44||spell==45){", "            if(false){", SPP1, SPP1),
    ('D5', 'D', RT, 'spell==34||spell==38?"Creature: ":"Aim! "', '"Aim! "', SPP1, SPP1),
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

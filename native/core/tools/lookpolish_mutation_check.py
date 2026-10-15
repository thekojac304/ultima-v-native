"""LOOK-POLISH1 mutants (docs/WINDOWS_LOOK_ENHANCED_POLISH.md section 6).

Each mutant is one edit to the production code (or the TypeScript reference),
built and run against look_post, look_polish_runtime (host, GCC) or the vitest
file; a mutant is KILLED when a test fails, SURVIVED when they all pass, and
NOT RUN when it does not build. The file is restored (and touched, so ninja
rebuilds it) after each mutant, and the restored tree is rebuilt and re-run at
the end.

  python native/core/tools/lookpolish_mutation_check.py <build-dir> [ids]
"""
import os, subprocess, sys, time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
CORE = os.path.join(ROOT, 'native', 'core')
BIN = r'C:/Dev/TamaPoke/.build-tools/w64devkit/bin'
NINJA = BIN + '/ninja.exe'
NODE_DIR = r'C:/Program Files/nodejs'

LOOK = 'native/core/src/look.cpp'
VIEWS = 'native/targets/tdeck/main/device_ui_views.h'
BOARD = 'native/targets/tdeck/main/tdeck_board.cpp'
RT = 'native/targets/tdeck/main/alpha_runtime.cpp'
TS = 'game/src/core/game.ts'

MUTANTS = [
    ('M1', LOOK, 'tile>=0xe0&&tile<=0xe2&&step<64', 'tile>=0xe0&&tile<=0xe2&&step<0', 'no chase: the post\'s own phrase'),
    ('M2', LOOK, 'if(tile==0xe0)--y;', 'if(tile==0xe0)++y;', '0xE0 points down'),
    ('M3', LOOK, 'else if(tile==0xe1)++x;else --x;', 'else if(tile==0xe1)--x;else ++x;', '0xE1 / 0xE2 swapped'),
    ('M4', LOOK, '        tile=seen(x,y);\n    }', '        tile=map.tile_at(x,y);\n    }', 'the chase reads the bare map, not the composited one'),
    ('M5', LOOK, '        if(map.geometry.wraps){x&=255;y&=255;}\n        tile=seen(x,y);', '        tile=seen(x,y);', 'no wrap in the chase'),
    ('M6', LOOK, '        tile=seen(x,y);\n    }\n    if(tile<0){say("Thou dost see darkness.");return CommandStatus::Success;}', '        tile=seen(x,y);\n    }', 'off-map after the chase is not darkness'),
    ('M7', VIEWS, 'const size_t edge=cells-1; // the blank margin cell', 'const size_t edge=cells; // the blank margin cell', 'no margin cell after the price'),
    ('M8', VIEWS, 'std::snprintf(price,sizeof(price),"%ld gp",long(row.price));', 'std::snprintf(price,sizeof(price),"%ldg",long(row.price));', 'the old "g" suffix against the digits'),
    ('M9', RT, 'shop_view_.rows[row].priced=!companions;', 'shop_view_.rows[row].priced=true;', 'companions priced (the "0g" rows)'),
    ('M10', RT, 'shop_view_.rows[row].counted=shop_.phase==openu5::ShopPhase::Sell;', 'shop_view_.rows[row].counted=false;', 'Sell shows no count'),
    ('M11', RT, 'else fit_list_row(selection_view_.rows[row],sizeof(selection_view_.rows[row]),label,21);', 'else std::snprintf(selection_view_.rows[row],sizeof(selection_view_.rows[row]),"%.22s",label);', 'list rows cut at 22 again'),
    ('M12', VIEWS, 'if(char *marker=std::strstr(short_tail," [equipped]"))std::memcpy(marker," [eq]",6);', '', 'no short marker'),
    ('M13', VIEWS, '                current.rows[row].priced!=cached.rows[row].priced||\n', '', 'the row cache ignores `priced`'),
    ('M14', BOARD, 'char line[24]{};if(current)format_shop_row(line,sizeof(line),shop->rows[i]);', 'char line[24]{};if(current){if(shop->rows[i].quantity>1)std::snprintf(line,sizeof(line)," %-12.12s %3ldg",shop->rows[i].name,long(shop->rows[i].price));else std::snprintf(line,sizeof(line)," %-14.14s %3ldg",shop->rows[i].name,long(shop->rows[i].price));}', 'the Board\'s old two formats'),
    ('M15', VIEWS, '        while(name_length&&name[name_length-1]==\' \')--name_length;\n        std::memcpy(dest,name,name_length);at=name_length;\n        dest[at++]=\'.\';', '        std::memcpy(dest,name,name_length);at=name_length;', 'a cut name gets no \'.\''),
    ('M16', RT, 'else{char fitted[16]{};fitted[fit_label(fitted,13,now,"")]=\'\\0\';std::snprintf(selection_view_.detail2,sizeof(selection_view_.detail2),"Current: %s",fitted);}}', 'else std::snprintf(selection_view_.detail2,sizeof(selection_view_.detail2),"Current: %.18s",now);}', 'the Ready detail overflows again'),
    ('T1', TS, '    for (let step = 0; tile >= 0xe0 && tile <= 0xe2 && step < 64; step++) {', '    for (let step = 0; tile >= 0xe0 && tile <= 0xe2 && step < 0; step++) {', 'TS: no chase'),
    ('T2', TS, '      if (tile === 0xe0) ny -= 1;', '      if (tile === 0xe0) ny += 1;', 'TS: 0xE0 points down'),
    ('T3', TS, '      else if (tile === 0xe1) nx += 1;\n      else nx -= 1;', '      else if (tile === 0xe1) nx -= 1;\n      else nx += 1;', 'TS: 0xE1 / 0xE2 swapped'),
]


def run(cmd, cwd, env=None, timeout=1800):
    p = subprocess.run(cmd, cwd=cwd, env=env, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=timeout)
    return p.returncode, p.stdout + p.stderr


def host_tests(build):
    env = dict(os.environ)
    env['PATH'] = BIN + os.pathsep + env['PATH']
    rc, out = run([NINJA, '-C', build, 'look_post_tests', 'look_polish_runtime'], CORE, env)
    if rc:
        return None, out[-2000:]
    results = []
    rc1, o1 = run([os.path.join(build, 'look_post_tests.exe'), os.path.join(ROOT, 'original', 'u5', 'ultima5')], CORE, env)
    rc2, o2 = run([os.path.join(build, 'look_polish_runtime.exe'), os.path.join(ROOT, 'native', 'assets', 'openu5-alpha1-resources.bin'),
                   os.path.join(ROOT, 'native', 'assets', 'openu5-audio.bin')], CORE, env)
    fails = [l for l in (o1 + o2).splitlines() if l.startswith('FAIL') or l.startswith('RED')]
    return (rc1 == 0 and rc2 == 0), fails


def ts_tests():
    env = dict(os.environ)
    env['PATH'] = NODE_DIR + os.pathsep + env['PATH']
    rc, out = run(['cmd', '/c', 'npx', 'vitest', 'run', 'tests/look-signpost.test.ts'], os.path.join(ROOT, 'game'), env)
    fails = [l.strip() for l in out.splitlines() if 'FAIL' in l and 'look-signpost' in l]
    return rc == 0, fails


def main():
    build = os.path.abspath(sys.argv[1])
    only = set(sys.argv[2].split(',')) if len(sys.argv) > 2 else None
    killed = survived = notrun = 0
    for mid, rel, old, new, what in MUTANTS:
        if only and mid not in only:
            continue
        path = os.path.join(ROOT, rel)
        raw = open(path, 'rb').read()
        text = raw.decode('utf-8')
        if text.count(old) != 1:
            print(f'{mid} ANCHOR-MISSING ({text.count(old)}) {what}', flush=True)
            notrun += 1
            continue
        open(path, 'wb').write(text.replace(old, new).encode('utf-8'))
        try:
            if mid.startswith('T'):
                ok, fails = ts_tests()
            else:
                ok, fails = host_tests(build)
        finally:
            open(path, 'wb').write(raw)
            os.utime(path, None)
        if ok is None:
            print(f'{mid} NOT RUN (does not build) -- {what}', flush=True)
            notrun += 1
        elif ok:
            print(f'{mid} SURVIVED -- {what}', flush=True)
            survived += 1
        else:
            print(f'{mid} KILLED -- {what}; by: {fails[0][:150] if fails else "(exit code)"} (+{max(0, len(fails) - 1)} more)', flush=True)
            killed += 1
    time.sleep(1)
    ok, fails = host_tests(build)
    tok, tfails = ts_tests()
    print(f'restored tree: host {"PASS" if ok else "FAIL"}, vitest {"PASS" if tok else "FAIL"}')
    print(f'killed {killed}, survived {survived}, not run {notrun}')
    return 0 if survived == 0 and notrun == 0 and ok and tok else 1


if __name__ == '__main__':
    sys.exit(main())

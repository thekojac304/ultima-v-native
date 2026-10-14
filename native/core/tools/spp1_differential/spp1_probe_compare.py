"""Compare two tdeck_diff_probe outputs walk by walk.

    python spp1_probe_compare.py base.txt head.txt [--examples N]

For every walk: identical, or the first differing step (its input token), the fields that
differ there, and both transcript tails / prompts. Walks are then grouped into classes by
(family, token at the first divergence, differing fields) so each class can be explained.
"""
import re
import sys
from collections import OrderedDict, defaultdict

LINE = re.compile(r'^(\S+) (\d{3}) (.{14}|.*?) gram=([0-9a-f]{16}) text=([0-9a-f]{16}) state=([0-9a-f]{16}) '
                  r'mode=(-?\d+) req=(-?\d+) prompt=\[(.*?)\] arena=([0-9a-f]{16}) audio=([0-9a-f]{16}) '
                  r'save=([0-9a-f]{16}) cursor=(\S+) turns=(-?\d+) clock=(\S+) seed=([0-9a-f]{8}) tail=(.*)$')
FIELDS = ['gram', 'text', 'state', 'mode', 'req', 'prompt', 'arena', 'audio', 'save', 'cursor', 'turns', 'clock', 'seed']


def load(path):
    walks = OrderedDict()
    other = []
    for raw in open(path, encoding='utf-8', errors='replace'):
        raw = raw.rstrip('\n')
        m = LINE.match(raw)
        if not m:
            if raw and not raw.startswith('['):
                other.append(raw)
            continue
        walk, step, label = m.group(1), int(m.group(2)), m.group(3).strip() or 'SPACE'
        vals = dict(zip(FIELDS, m.groups()[3:16]))
        walks.setdefault(walk, []).append((step, label, vals, m.group(17)))
    return walks, other


def family(walk):
    parts = walk.split('-')
    if parts[0] in ('A', 'Aact', 'Dtown', 'Dpalace', 'Ddun', 'Ddun40'):
        return '-'.join(parts[:2])          # the command letter; the 8 response scripts fold together
    if parts[0] == 'C':
        return '-'.join(parts[:2])          # the shop dialog
    return walk                             # B, E, F, G, H: each script is its own walk


def main():
    a_path, b_path = sys.argv[1], sys.argv[2]
    examples = int(sys.argv[sys.argv.index('--examples') + 1]) if '--examples' in sys.argv else 1
    a, a_other = load(a_path)
    b, b_other = load(b_path)
    steps = sum(len(v) for v in a.values())
    same = 0
    classes = OrderedDict()
    only = [w for w in a if w not in b] + [w for w in b if w not in a]
    for walk, sa in a.items():
        sb = b.get(walk)
        if sb is None:
            continue
        if sa == sb:
            same += 1
            continue
        n = min(len(sa), len(sb))
        i = next((k for k in range(n) if sa[k] != sb[k]), n)
        if i == n:
            key = (family(walk), 'LENGTH', ())
            detail = f'{walk}: lengths {len(sa)} vs {len(sb)}'
        else:
            fa, fb = sa[i][2], sb[i][2]
            diff = tuple(f for f in FIELDS if fa[f] != fb[f])
            dturn = int(fb['turns']) - int(fa['turns'])
            token = ' '.join(x[1] for x in sa[1:i + 1][-3:])
            key = (family(walk), sa[i][1], diff + (f'dturns={dturn:+d}',))
            detail = (f'{walk} step {sa[i][0]} after [{token}]\n'
                      f'      base: mode={fa["mode"]} req={fa["req"]} turns={fa["turns"]} clock={fa["clock"]} prompt=[{fa["prompt"]}] tail={sa[i][3][-260:]}\n'
                      f'      head: mode={fb["mode"]} req={fb["req"]} turns={fb["turns"]} clock={fb["clock"]} prompt=[{fb["prompt"]}] tail={sb[i][3][-260:]}')
        classes.setdefault(key, []).append(detail)
    print(f'walks {len(a)} (head {len(b)}), steps {steps} (head {sum(len(v) for v in b.values())})')
    print(f'identical walks {same}, differing walks {len(a) - same - len([w for w in a if w not in b])}, '
          f'walks in one output only {len(only)} {only[:10]}')
    print(f'non-snapshot lines: base {len(a_other)}, head {len(b_other)}; equal={a_other == b_other}')
    for x, y in zip(a_other, b_other):
        if x != y:
            print(f'   base: {x}\n   head: {y}')
    print(f'\n{len(classes)} classes (family, token at the first divergence, differing fields):')
    byfam = defaultdict(int)
    for (fam, tok, diff), items in classes.items():
        byfam[fam.split('-')[0]] += len(items)
        print(f'\n== {fam}  token={tok!r}  fields={",".join(diff)}  walks={len(items)}')
        for d in items[:examples]:
            print('   ' + d)
    print('\ndiffering walks by family prefix:', dict(byfam))


if __name__ == '__main__':
    main()

"""Assign every walk that differs between the baseline and SPP1 probe outputs to the SPP1 item that
explains it, by its FIRST differing step. Anything no rule explains is printed as UNEXPLAINED and
the exit code is 1.

    python spp1_probe_classify.py base.txt head.txt

Rules (docs/SHARED_PRESERVATION_PARITY_SPP1.md, "T-Deck differential"):
  A    head spent exactly one more turn, text/mode/prompt unchanged, after a command letter in the
       dispatcher's arm set for that context (K:0x3178 returns 1); the Z viewer's page keys count
       as Z; the probe's own Talk at no one ("Funny, no response!", TALK -> 0 -> 1) counts as T
  A*   the same spent town turn, after which a guard's "Pay tribute?" arrives (the baseline shows
       the same demand after its own town turns)
  F+A  "Use on whom?" cancelled: the potion is spent (CAST 0x136a, D-51 retired), its echo
       printed, and the dispatcher's turn spent
  B    the paid rumour's gossip and "says <keeper>." (SHOPPES2 0x0636-0x065c)
  C    J / P / Y in the arena open Jimmy- / Push- / "Yell what?" instead of "What?"
  E    the dungeon L is "Look..." + Player: (DNGLOOK 0x0000); the T-Deck's D is the Look aimed Here
  G    Ctrl-K / Ctrl-V echo the karma / "1.16" (K:0x2712); in the dungeon they also tick
"""
import os
import sys
from collections import Counter, OrderedDict

sys.path.insert(0, os.path.dirname(__file__))
from spp1_probe_compare import load  # noqa: E402
from spp1_probe_acheck import ARM, context  # noqa: E402


def command_letter(sa, i):
    for k in range(i, 0, -1):
        prev, tok = sa[k - 1][2], sa[k][1]
        if prev['mode'] in ('0', '1') and prev['req'] == '0' and prev['prompt'] == '' and len(tok) == 1 and tok.isalpha():
            return tok, k
    return None, 0


def classify(walk, sa, sb, i, base_has_tribute):
    fa, fb = sa[i][2], sb[i][2]
    ta, tb = sa[i][3], sb[i][3]
    tok = sa[i][1]
    dturn = int(fb['turns']) - int(fa['turns'])
    same_view = fa['text'] == fb['text'] and fa['mode'] == fb['mode'] and fa['prompt'] == fb['prompt']
    ctx = context(walk)
    if tok in ('^K', '^V'):
        want = '[60]' if tok == '^K' else '[1.16]'
        if tb.endswith(want) and ta.endswith('[What?]') and dturn == (1 if ctx == 'dungeon' else 0):
            return 'G'
    if walk.startswith('B-') and tok in ('j', 'p', 'y') and fa['mode'] == '2' and ta.endswith('[What?]'):
        if tb.endswith({'j': '[Jimmy-]', 'p': '[Push-]', 'y': '[Yell]'}[tok]):
            return 'C'
    if walk.startswith('C-82') and 'says Sam.' in tb and 'says Sam.' not in ta and dturn == 0:
        return 'B'
    if ctx == 'dungeon' and tok == 'l' and ta.endswith('[What?]') and tb.endswith('[Look...]') and fb['prompt'] == 'Player: ':
        return 'E'
    if ctx == 'dungeon' and tok == 'd' and '[Drink]' in ta and tb.endswith('[Look...]') and fb['prompt'] == 'Player: ' and dturn == -1:
        return 'E'
    if dturn != 1:
        return None
    cmd, k = command_letter(sa, i)
    if i == 0 and walk.startswith('C-') and 'Funny, no response!' in ta:
        return 'A'
    if cmd is not None and 'Z-stats...' in ' '.join(x[3] for x in sa[max(0, k - 4):k]) and cmd not in ARM.get(ctx, ''):
        cmd = 'z'   # the letter was a page key of the Z viewer (the runtime's, not a UiMode)
    if cmd is None or cmd not in ARM.get(ctx, set()):
        return None
    if same_view:
        return 'A'
    if fb['prompt'] == 'Pay tribute?' and fa['mode'] == '0' and base_has_tribute:
        return 'A*'
    if cmd == 'u' and tb.endswith('[Potion]') and tb[:-len('[Potion]')] == ta:
        return 'F+A'
    return None


def main():
    a, _ = load(sys.argv[1])
    b, _ = load(sys.argv[2])
    base_has_tribute = any('Pay tribute?' == s[2]['prompt'] for v in a.values() for s in v)
    counts, unexplained, identical = Counter(), [], 0
    by_item = OrderedDict()
    for walk, sa in a.items():
        sb = b.get(walk)
        if sb == sa:
            identical += 1
            continue
        if sb is None or len(sa) != len(sb):
            unexplained.append(f'{walk}: missing or length differs')
            continue
        i = next(k for k in range(len(sa)) if sa[k] != sb[k])
        item = classify(walk, sa, sb, i, base_has_tribute)
        if item is None:
            unexplained.append(f'{walk} step {sa[i][0]} token {sa[i][1]!r}\n    base {sa[i][3][-200:]}\n    head {sb[i][3][-200:]}')
        else:
            counts[item] += 1
            by_item.setdefault(item, []).append(walk)
    print(f'walks {len(a)}: identical {identical}, differing {len(a) - identical}')
    for item in ('A', 'A*', 'F+A', 'B', 'C', 'E', 'G'):
        ws = by_item.get(item, [])
        print(f'  {item:4} {counts[item]:4}  e.g. {", ".join(ws[:6])}')
    print(f'UNEXPLAINED {len(unexplained)}')
    for u in unexplained:
        print('  ' + u)
    return 1 if unexplained else 0


if __name__ == '__main__':
    sys.exit(main())

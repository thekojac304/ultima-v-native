"""Assign every walk that differs between the baseline (00ae0410) and LOOK-POLISH1 probe outputs
to the change that explains it, from ALL its differing steps; anything no rule explains is
printed as UNEXPLAINED and the exit code is 1.

    python lookpolish_probe_classify.py base.txt head.txt

Rules (docs/WINDOWS_LOOK_ENHANCED_POLISH.md, section 7):
  P  D-117, Look at a sign's post: the first differing step's baseline transcript ends in
     "parched desert" and the head's in "the sign of ..."; afterwards only the panel (gram), the
     transcript hash (text) and its tail differ -- the same mode, prompt, state, turns, clock,
     seed, audio and save at every step.
  S  the shop panel's rows (one price column, "<n> gp", no price on a companion, Sell's count):
     only the panel pixels (gram) differ, and every differing step shows a shop (mode Shop, or
     a Y/N, number or text prompt over the shop panel).
  L  the compact selector's rows ([equipped] / counts kept whole, the Ready detail line): only
     gram differs, and every differing step is a list (party, inventory, equipment, spell).
"""
import os
import sys
from collections import Counter

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'spp1_differential'))
from spp1_probe_compare import load, FIELDS  # noqa: E402

SHOP_MODES = {'4', '6', '7', '8'}          # Shop, TextEntry, NumericEntry, YesNo (over the shop panel)
LIST_MODES = {'9', '10', '11', '12'}       # Party / Inventory / Equipment / Spell selection
STRICT = [f for f in FIELDS if f not in ('gram', 'text')]


def classify(sa, sb):
    diff_steps = [k for k in range(min(len(sa), len(sb))) if sa[k] != sb[k]]
    if len(sa) != len(sb) or not diff_steps:
        return None, 'different step counts'
    fields = set()
    for k in diff_steps:
        fields |= {f for f in FIELDS if sa[k][2][f] != sb[k][2][f]}
        if sa[k][3] != sb[k][3]:
            fields.add('tail')
    first = diff_steps[0]
    ta, tb = sa[first][3], sb[first][3]
    if not (fields & set(STRICT)) and 'parched desert' in ta and 'the sign of' in tb:
        return 'P', ''
    if fields == {'gram'}:
        modes = {sa[k][2]['mode'] for k in diff_steps}
        if modes <= SHOP_MODES:
            return 'S', ''
        if modes <= LIST_MODES:
            return 'L', ''
        return None, 'gram only, modes %s' % sorted(modes)
    return None, 'fields %s' % sorted(fields)


def main():
    a, _ = load(sys.argv[1])
    b, _ = load(sys.argv[2])
    tally, odd = Counter(), []
    steps = sum(len(v) for v in a.values())
    same = 0
    for walk, sa in a.items():
        sb = b.get(walk)
        if sb is None:
            odd.append((walk, 'missing in head'))
            continue
        if sa == sb:
            same += 1
            continue
        rule, why = classify(sa, sb)
        if rule:
            tally[rule] += 1
        else:
            odd.append((walk, why))
    extra = [w for w in b if w not in a]
    print('walks %d, steps %d, identical walks %d, differing %d' % (len(a), steps, same, len(a) - same))
    for rule in 'PSL':
        print('  %s %d' % (rule, tally[rule]))
    for walk, why in odd:
        print('UNEXPLAINED %s: %s' % (walk, why))
    for walk in extra:
        print('UNEXPLAINED %s: only in head' % walk)
    return 1 if odd or extra else 0


if __name__ == '__main__':
    sys.exit(main())

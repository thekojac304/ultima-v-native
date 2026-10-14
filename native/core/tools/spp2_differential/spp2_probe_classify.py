"""Assign every walk that differs between the SPP1 (fc4a63aa) and SPP2 probe outputs to the SPP2
item that explains it, by its FIRST differing step; then check that the walk ends in the same
turn count (the change is WHEN a turn lands or WHAT is printed, never how many turns are spent).
Anything no rule explains is printed as UNEXPLAINED and the exit code is 1.

    python spp2_probe_classify.py base.txt head.txt

Rules (docs/SHARED_PRESERVATION_PARITY_SPP2.md, section 7.2):
  T  a town Talk with nobody at the target: the head prints TALK's "Nobody's here!" (0x0491)
     where the baseline printed "Funny, no response!"; same turns, same mode
  M  a command that ended on a timer (Mix's "Mixing..." hold, the View scroll's reveal): the
     head has the dispatcher's turn one step earlier (+1), the view otherwise the same
  N  a latch-charged town turn brought an NPC's approach: the head opens the conversation (or
     shop) in the same input; the baseline is still at the prompt, with the same turn count
"""
import os
import sys
from collections import Counter, OrderedDict

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'spp1_differential'))
from spp1_probe_compare import load  # noqa: E402

TALK = ("Nobody's here!", 'No response!', 'The guard offers')


def classify(walk, sa, sb, i):
    fa, fb = sa[i][2], sb[i][2]
    ta, tb = sa[i][3], sb[i][3]
    dturn = int(fb['turns']) - int(fa['turns'])
    if dturn == 0 and fa['mode'] == fb['mode'] and '[Funny, no response!]' in ta and any(t in tb for t in TALK) \
            and ta.replace('[Funny, no response!]', '') == tb.replace("[Nobody's here!]", '').replace('[No response!]', ''):
        return 'T'
    if dturn == 1 and fa['mode'] == fb['mode'] == '0' and fa['text'] == fb['text'] and fa['prompt'] == fb['prompt'] and \
            (ta.endswith('[Done!]') or '[View!]' in ta):
        return 'M'
    if dturn == 0 and fa['mode'] == '0' and fb['mode'] in ('3', '6', '7') and fb['prompt'] in ('Your interest?', '') and \
            tb.startswith(ta[:-2] if ta.endswith('[]') else ta):
        return 'N'
    return None


def main():
    a, _ = load(sys.argv[1])
    b, _ = load(sys.argv[2])
    tally, notes, odd, unconverged = Counter(), Counter(), [], []
    for walk, sa in a.items():
        sb = b[walk]
        if sa == sb:
            continue
        i = next((k for k in range(min(len(sa), len(sb))) if sa[k] != sb[k]), None)
        item = classify(walk, sa, sb, i) if i is not None else None
        if item is None:
            odd.append(walk)
            fa, fb = sa[i][2], sb[i][2]
            print(f'UNEXPLAINED {walk} step {i} [{sa[i][1]}]: turns {fa["turns"]}->{fb["turns"]} mode {fa["mode"]}->{fb["mode"]}'
                  f'\n   base {sa[i][3][-200:]}\n   head {sb[i][3][-200:]}')
            continue
        tally[item] += 1
        end_a, end_b = sa[-1][2], sb[-1][2]
        # An M walk that ends on WAIT (no key after the hold) ends one turn apart by design: SPP1
        # settles the timer-ended command's turn only at the next key, which never comes.
        if item == 'M' and sa[-1][1] == 'WAIT' and int(end_b['turns']) - int(end_a['turns']) == 1:
            notes['M walks ending on WAIT (SPP1 still owes the turn)'] += 1
            continue
        if item in ('T', 'M') and (end_a['turns'] != end_b['turns'] or end_a['clock'] != end_b['clock']):
            unconverged.append(f'{walk} ({item}): end turns {end_a["turns"]}/{end_b["turns"]} clock {end_a["clock"]}/{end_b["clock"]}')
    total = sum(tally.values()) + len(odd)
    print(f'differing walks {total}: ' + ', '.join(f'{k} {v}' for k, v in sorted(tally.items())) + f', unexplained {len(odd)}')
    for k, v in notes.items():
        print(f'  of which {k}: {v}')
    for u in unconverged:
        print('NOT CONVERGED ' + u)
    print(f'T/M walks that end on a different turn count or clock: {len(unconverged)}')
    return 1 if odd or unconverged else 0


if __name__ == '__main__':
    sys.exit(main())

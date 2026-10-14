"""SPP1 A check over two probe outputs: every first divergence where the head spent exactly one
more turn must follow a command the kernel dispatcher K:0x3178 charges (the arm sets in
AlphaRuntime::arm_dispatcher_turn), typed at that context's command prompt.

    python spp1_probe_acheck.py base.txt head.txt
"""
import sys
from collections import Counter

sys.path.insert(0, __import__('os').path.dirname(__file__))
from spp1_probe_compare import load, FIELDS  # noqa: E402

ARM = {'world': set('rznucmlgojspftxk'), 'town': set('rznucmlgojspftxa'), 'dungeon': set('rznumcs')}


def context(walk):
    p = walk.split('-')[0]
    if p in ('A', 'Aact', 'E', 'F') or walk.startswith('H-0'):
        return 'world'
    if p in ('Dtown', 'Dpalace', 'T', 'C') or walk.startswith('H-1'):
        return 'town'
    if p in ('Ddun', 'Ddun40', 'G'):
        return 'dungeon'
    return 'arena'


def main():
    a, _ = load(sys.argv[1])
    b, _ = load(sys.argv[2])
    tally, odd, text_or_mode = Counter(), [], []
    for walk, sa in a.items():
        sb = b[walk]
        if sa == sb:
            continue
        i = next((k for k in range(min(len(sa), len(sb))) if sa[k] != sb[k]), None)
        if i is None:
            continue
        fa, fb = sa[i][2], sb[i][2]
        if int(fb['turns']) - int(fa['turns']) != 1:
            continue
        # the base did not spend a turn on this step; the head did
        base_spent = int(fa['turns']) - int(sa[i - 1][2]['turns']) if i else 0
        head_spent = int(fb['turns']) - int(sb[i - 1][2]['turns']) if i else 0
        cmd = None
        for k in range(i, 0, -1):
            prev = sa[k - 1][2]
            tok = sa[k][1]
            if prev['mode'] in ('0', '1') and prev['req'] == '0' and prev['prompt'] == '' and len(tok) == 1 and tok.isalpha():
                cmd = tok
                break
        ctx = context(walk)
        tally[(ctx, cmd, base_spent, head_spent)] += 1
        if cmd is None or cmd not in ARM.get(ctx, set()):
            odd.append(f'{walk} step {sa[i][0]} token {sa[i][1]!r} cmd={cmd} ctx={ctx} '
                       f'base[{fa["mode"]},{fa["prompt"]}]{sa[i][3][-120:]}  head[{fb["mode"]},{fb["prompt"]}]{sb[i][3][-120:]}')
        if fa['text'] != fb['text'] or fa['mode'] != fb['mode'] or fa['prompt'] != fb['prompt']:
            text_or_mode.append(f'{walk} step {sa[i][0]} token {sa[i][1]!r} cmd={cmd}\n'
                                f'    base mode={fa["mode"]} req={fa["req"]} prompt=[{fa["prompt"]}] {sa[i][3][-200:]}\n'
                                f'    head mode={fb["mode"]} req={fb["req"]} prompt=[{fb["prompt"]}] {sb[i][3][-200:]}')
    print('+1-turn first divergences by (context, command letter, base turns spent on the step, head turns spent):')
    for k, v in sorted(tally.items(), key=lambda kv: (kv[0][0], str(kv[0][1]))):
        print(f'   {k}: {v}')
    print(f'\ncommand letters OUTSIDE the arm set (must be explained): {len(odd)}')
    for o in odd:
        print('   ' + o)
    print(f'\n+1 divergences that ALSO change the text, mode or prompt: {len(text_or_mode)}')
    for o in text_or_mode:
        print('   ' + o)


if __name__ == '__main__':
    main()

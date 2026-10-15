"""Strict comparison with the frozen pre-optimization handheld traces and DOS control."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'native/core/tools/spp1_differential'))
from spp1_probe_compare import load


def inspect(path):
    walks, other = load(str(path))
    if len(walks) != 1065 or sum(map(len, walks.values())) != 8514 or other[-1:] != ['PROBE DONE']:
        raise ValueError(f'Incomplete differential trace: {path}')
    return walks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--baseline-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    packs = [str(ROOT / 'native/assets/openu5-alpha1-resources.bin'), str(ROOT / 'native/assets/openu5-audio.bin')]
    summary = {}
    for mode in ('unpaced', 'paced'):
        baseline = args.baseline_dir / f'reference-{mode}.txt'
        a = inspect(baseline)
        for kind, exe in [('reference', args.reference), ('handheld', args.candidate)]:
            result = args.output / f'{kind}-{mode}.txt'
            with result.open('wb') as out:
                subprocess.run([str(exe.resolve()), *packs, *(['--paced'] if mode == 'paced' else [])], stdout=out, check=True, cwd=ROOT)
            b = inspect(result)
            changed = [walk for walk in a if a[walk] != b.get(walk)]
            if changed or baseline.read_bytes() != result.read_bytes():
                raise ValueError(f'{mode}/{kind}: unexpected difference, walks {changed[:10]}')
        summary[mode] = {'walks': len(a), 'snapshots': sum(map(len, a.values())), 'different_walks': 0,
                         'byte_identical': True, 'sha256': hashlib.sha256(baseline.read_bytes()).hexdigest()}
    control = args.output / 'dos-positive-control.txt'
    with control.open('wb') as out:
        subprocess.run([str(args.reference.resolve()), *packs, 'ABCDEFGHIJ', '--dos'], stdout=out, check=True, cwd=ROOT)
    a = inspect(args.baseline_dir / 'reference-unpaced.txt')
    b = inspect(control)
    changed = sum(a[walk] != b.get(walk) for walk in a)
    if changed < 1000:
        raise ValueError(f'DOS positive control failed: only {changed} differing walks')
    summary['dos_positive_control_different_walks'] = changed
    print(json.dumps(summary, indent=2))
    (args.output / 'differential-summary.json').write_text(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()

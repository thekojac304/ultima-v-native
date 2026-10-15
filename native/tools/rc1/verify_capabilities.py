"""Reject missing or leaking handheld capability definitions in actual build commands."""
import argparse
import json
from pathlib import Path


def verify(commands, handheld):
    selected = []
    for entry in commands:
        source = entry['file'].replace('\\', '/').lower()
        if '/native/core/src/' in source or '/native/targets/tdeck/main/' in source:
            selected.append(entry)
    if len(selected) < 70:
        raise ValueError(f'Incomplete shared source command list: {len(selected)}')
    for entry in selected:
        command = entry.get('command', ' '.join(entry.get('arguments', [])))
        fixed = '-DOPENU5_FIXED_HANDHELD_PROFILE=1' in command or '/DOPENU5_FIXED_HANDHELD_PROFILE=1' in command
        if fixed != handheld:
            raise ValueError(f'Wrong handheld capability for {entry["file"]}')
        if handheld and ('-Werror' not in command or '-fstack-usage' not in command):
            raise ValueError(f'Lost firmware warning/stack checks for {entry["file"]}')
    return len(selected)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('commands', type=Path)
    parser.add_argument('--handheld', action='store_true')
    args = parser.parse_args()
    count = verify(json.loads(args.commands.read_text()), args.handheld)
    print(f'PASS: {count} shared translation units, handheld={args.handheld}')

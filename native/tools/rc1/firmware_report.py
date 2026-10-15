"""Record firmware sections, linked desktop candidates, and compiler stack frames.

Uses the active ESP-IDF toolchain; no machine-specific tool paths. Stack frames
are compiler estimates, not whole-call-chain or hardware high-water guarantees.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys


def report(build: Path, prefix: str) -> dict:
    meta = json.loads((build / 'project_description.json').read_text(encoding='utf-8'))
    image = (build / meta['app_bin']).read_bytes()
    elf = build / meta['app_elf']
    sections_text = subprocess.check_output([prefix + 'size', '-A', str(elf)], text=True)
    sections = {}
    for line in sections_text.splitlines():
        match = re.match(r'^(\.[^\s]+)\s+(\d+)\s+\d+\s*$', line)
        if match:
            sections[match[1]] = int(match[2])
    nm = subprocess.check_output([prefix + 'nm', '-S', '--size-sort', '-C', str(elf)], text=True)
    candidates = []
    required = {}
    for line in nm.splitlines():
        match = re.match(r'^([0-9a-f]+) ([0-9a-f]+) (\S) (.*)$', line)
        if not match:
            continue
        name = match[4]
        if re.search(r'dos_|Dos|platform_|typed_spell|spell_entry|desktop', name):
            candidates.append({'symbol': name, 'bytes': int(match[2], 16), 'type': match[3]})
        if any(term in name for term in ('world_look(', 'AlphaRuntime::command(', 'ShopRun::', 'AlphaSave::')):
            required[name] = int(match[2], 16)
    frames = {}
    frame_kinds = {}
    for path in sorted(build.rglob('*.su')):
        if '/main/' not in path.as_posix():
            continue
        for line in path.read_text(encoding='utf-8', errors='replace').splitlines():
            match = re.match(r'^.*?:\d+:\d+:(.*?)\t(\d+)\t(.+)$', line)
            if match:
                name, size, kind = match[1], int(match[2]), match[3]
                frames[name] = max(frames.get(name, 0), size)
                frame_kinds[name] = kind
    sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'targets' / 'tdeck'))
    from check_app_budget import smallest_app_partition, launcher_allocation
    partition = smallest_app_partition((build / 'partition_table' / 'partition-table.bin').read_bytes())
    return {
        'project_version': meta['project_version'], 'idf_version': meta.get('git_revision'),
        'image_bytes': len(image), 'image_sha256': hashlib.sha256(image).hexdigest(),
        'partition_bytes': partition, 'partition_free_bytes': partition - len(image),
        'launcher_allocation_bytes': launcher_allocation(len(image)),
        'launcher_free_bytes': launcher_allocation(len(image)) - len(image),
        'elf_sections': sections, 'linked_desktop_candidates': candidates,
        'required_gameplay_symbols': required, 'stack_frames': frames,
        'stack_frame_kinds': frame_kinds,
        'largest_frames': sorted(frames.items(), key=lambda item: (-item[1], item[0]))[:25],
        'stack_scope': 'main component compiler frames; not whole call chains or hardware measurements',
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('--tool-prefix', default='xtensa-esp32s3-elf-')
    args = parser.parse_args()
    print(json.dumps(report(args.build.resolve(), args.tool_prefix), indent=2))

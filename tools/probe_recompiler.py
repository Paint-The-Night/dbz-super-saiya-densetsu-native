#!/usr/bin/env python3
"""Reproduce the pinned C-generation/build experiment, without running the game."""
import argparse
from collections import Counter
import json
from pathlib import Path
import platform
import subprocess
import sys

from inspect_rom import ROOT, ROM, inspect

REVISION = '555322683a5dadba2d118e147af887f3d5c8188d'
ENGINE = ROOT / '.local/tools/snesrecomp'


def run_logged(command, path, timeout):
    with path.open('w') as log:
        subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                       timeout=timeout, check=True)


def summarize(output):
    manifest = json.loads((output / 'generated/program_manifest.json').read_text())
    nodes = manifest['nodes']
    objects = sorted((output / 'build').rglob('*.c.o'))
    return {
        'framework_revision': REVISION, 'host_architecture': platform.machine(),
        'rom_sha256': inspect(ROM)['sha256'],
        'root_count': len(manifest['roots']),
        'discovered_function_variants': len(nodes),
        'distinct_discovered_entry_addresses': len({n['key']['pc24'] for n in nodes.values()}),
        'classification': dict(Counter(n['disposition'] for n in nodes.values())),
        'reset_variant': {k: nodes['008000:M1X1'][k] for k in ('disposition', 'reasons')},
        'emitted_bank_files': len(list((output / 'generated').glob('bank*_v2.c'))),
        'static_library_exists': (output / 'build/libsnesrecomp_game.a').is_file(),
        'object_format': subprocess.check_output(['file', '-b', str(objects[0])], text=True).strip() if objects else None,
        'playable_executable_built': False, 'behavioral_equivalence_tested': False,
        'decompilation_completion_percentage': None,
        'limitations': [
            'Static discovery with default auto_vectors configuration; no execution traces supplied.',
            'Variants distinguish CPU register-width states, not necessarily different functions.',
            'AOT eligibility is an analyzer classification, not independently verified correctness.',
            'Unknown and undiscovered code prevent any whole-game coverage percentage.',
            'Generated default build disables warnings and permits implicit function declarations.',
            'A static archive does not establish complete linking or successful execution.'
        ]
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / '.local/probes/dbz-recomp')
    parser.add_argument('--summarize-only', action='store_true')
    args = parser.parse_args()
    inspect(ROM)
    actual = subprocess.check_output(['git', '-C', str(ENGINE), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != REVISION:
        parser.error(f'Expected framework {REVISION}, got {actual}')
    output = args.output.resolve()
    if not args.summarize_only:
        if output.exists() and any(output.iterdir()):
            parser.error('Use a fresh output directory; existing results will not be overwritten.')
        ROOT.joinpath('research').mkdir(exist_ok=True)
        run_logged([sys.executable, str(ENGINE / 'snesrecomp_cli.py'), 'build',
                    '--rom', str(ROM), '--output', str(output), '--name', 'DBZ Super Saiya Densetsu'],
                   ROOT / 'research/snesrecomp-generation.log', 180)
        run_logged(['sh', str(output / 'build.sh')], ROOT / 'research/snesrecomp-build.log', 180)
    print(json.dumps(summarize(output), indent=2))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Check resumed execution against an uninterrupted run, including PCM phase."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

binary, rom = map(lambda p: str(Path(p).resolve()), sys.argv[1:])
with tempfile.TemporaryDirectory(prefix='dbz-checkpoint-') as directory:
    root = Path(directory)
    def run(name, frames, checkpoint=None, succeeds=True):
        dest = root / name
        dest.mkdir()
        args = [binary, '--rom', rom, '--headless', '--verify', '--frames', str(frames), '--dump-dir', str(dest)]
        if checkpoint:
            args += ['--load-checkpoint', str(checkpoint)]
        result = subprocess.run(args, capture_output=True, text=True)
        assert result.returncode == (0 if succeeds else 1), result.stdout + result.stderr
        if succeeds:
            assert json.loads((dest / 'report.json').read_text())['equivalence_passed']
        else:
            assert 'checkpoint' in result.stderr.lower(), result.stderr
        return dest

    whole = run('whole', 360)
    prefix = run('prefix', 211)
    tail = run('tail', 149, prefix / 'final.dbzstate')
    for name in ['final.state', 'final.dbzstate']:
        assert (whole / name).read_bytes() == (tail / name).read_bytes(), name
    a = [json.loads(s) for s in (whole / 'frames.jsonl').read_text().splitlines()][211:]
    b = [json.loads(s) for s in (tail / 'frames.jsonl').read_text().splitlines()]
    for x, y in zip(a, b, strict=True):
        for field in ['state_hash', 'video_hash', 'audio_hash', 'pc', 'input']:
            assert x[field] == y[field], (x['frame'], field, x[field], y[field])
    original = (prefix / 'final.dbzstate').read_bytes()
    variants = {'truncated': original[:-1], 'oversized': original+b'x'}
    for name, offset in [('magic', 0), ('rom', 8), ('digest', 72), ('phase', 136), ('payload', 200)]:
        changed = bytearray(original)
        changed[offset] ^= 1
        variants[name] = changed
    for name, data in variants.items():
        path = root / (name + '.dbzstate')
        path.write_bytes(data)
        run(name, 1, path, succeeds=False)
print('PASS: checkpoint split/resume matches all state, video, and PCM hashes; seven invalid formats rejected.')

#!/usr/bin/env python3
"""Recreate the opening checkpoint, then verify the documented battle route.

Requires only the local ROM, not an archived checkpoint or game save.
"""
import csv
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    binary, rom = (str(Path(arg).resolve()) for arg in sys.argv[1:])
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix="dbz-battle-") as directory:
        workspace = Path(directory)

        def run(name, frames, inputs, checkpoint=None):
            dest = workspace / name
            dest.mkdir()
            args = [binary, "--rom", rom, "--headless", "--verify",
                    "--frames", str(frames), "--inputs", str(root / inputs),
                    "--dump-dir", str(dest), "--capture-every", str(frames)]
            if checkpoint:
                args += ["--load-checkpoint", str(checkpoint)]
            result = subprocess.run(args, capture_output=True, text=True, timeout=240)
            if result.returncode:
                raise AssertionError(result.stdout + result.stderr)
            report = json.loads((dest / "report.json").read_text())
            assert report["verification_enabled"] is True, report
            assert report["equivalence_passed"] is True, report
            assert report["frames"] == frames, report
            trace = (dest / "frames.jsonl").read_text().splitlines()
            assert len(trace) == frames, (name, len(trace))
            return dest

        opening = run("opening", 3000, "tests/start-game.inputs")
        battle = run("battle", 1800, "research/scout/battle-route.inputs",
                     opening / "final.dbzstate")
        with (battle / "executed-addresses.csv").open() as stream:
            visits = {row["cpu_address"].lower(): int(row["hits"])
                      for row in csv.DictReader(stream)}
        # The documented route must reach the actor-slot update. This is a
        # reachability check; instruction fidelity is covered by test-native.
        assert visits.get("00:98d5", 0) > 0, "actor-slot update not reached"
        print("PASS: reset-to-opening and 1800-frame battle route match state, video, and audio.")


if __name__ == "__main__":
    main()

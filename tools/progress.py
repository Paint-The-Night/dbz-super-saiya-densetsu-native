#!/usr/bin/env python3
"""Print the reconstruction progress using a documented, reproducible denominator.

The headline is deliberately scoped: it measures verified native instruction
sites against the static SNESRecomp instruction worklist, not the whole ROM.
The worklist is incomplete by construction, so this is a tracker, not a claim
that the game is nearly finished.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "research/recompiler-probe.json"
MANIFEST = ROOT / ".local/probes/dbz-recomp-reproduction/generated/program_manifest.json"

# These are the fixed, reviewed regions listed in docs/progress.md. Keeping the
# inventory here makes the percentage auditable without parsing generated C.
NATIVE_SITES = 1491
NATIVE_BYTES = 3056
NATIVE_ENTRIES = 60


def discovered_instruction_variants() -> tuple[int, str]:
    if MANIFEST.exists():
        data = json.loads(MANIFEST.read_text())
        return sum(node.get("instruction_count", 0) for node in data["nodes"].values()), "local manifest"
    data = json.loads(PROBE.read_text())
    # Derived once from the pinned manifest and retained in the checked-in probe
    # record so public clones can reproduce the headline without .local/.
    return data["discovered_instruction_variants"], "pinned probe record"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    args = parser.parse_args()
    denominator, source = discovered_instruction_variants()
    percentage = 100 * NATIVE_SITES / denominator
    result = {
        "headline": {
            "label": "verified native instruction sites / discovered instruction worklist",
            "completed": NATIVE_SITES,
            "total": denominator,
            "percent": round(percentage, 2),
            "denominator_source": source,
        },
        "inventory": {
            "native_rom_bytes": NATIVE_BYTES,
            "native_instruction_sites": NATIVE_SITES,
            "native_entry_regions": NATIVE_ENTRIES,
        },
        "static_worklist": {
            "discovered_function_variants": 388,
            "distinct_entry_addresses": 375,
            "instruction_variants": denominator,
        },
        "caveat": "The static worklist is incomplete; this is not whole-game completion.",
    }
    if args.json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        print(f"Verified native reconstruction: {NATIVE_SITES}/{denominator} instruction sites ({percentage:.2f}%)")
        print(f"Native regions: {NATIVE_ENTRIES}; original ROM bytes covered: {NATIVE_BYTES}")
        print("Scope: discovered static worklist only; the whole-game denominator is unknown.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

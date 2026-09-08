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
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "research/recompiler-probe.json"
MANIFEST = ROOT / ".local/probes/dbz-recomp-reproduction/generated/program_manifest.json"
README = ROOT / "README.md"

# These are the fixed, reviewed regions listed in docs/progress.md. Keeping the
# inventory here makes the percentage auditable without parsing generated C.
NATIVE_SITES = 12541
NATIVE_BYTES = 28210
NATIVE_ENTRIES = 430


def discovered_instruction_variants() -> tuple[int, str]:
    if MANIFEST.exists():
        data = json.loads(MANIFEST.read_text())
        return sum(node.get("instruction_count", 0) for node in data["nodes"].values()), "local manifest"
    data = json.loads(PROBE.read_text())
    # Derived once from the pinned manifest and retained in the checked-in probe
    # record so public clones can reproduce the headline without .local/.
    return data["discovered_instruction_variants"], "pinned probe record"


def readme_progress_block(denominator: int) -> str:
    percentage = 100 * NATIVE_SITES / denominator
    return (
        f"**{percentage:.2f}% — {NATIVE_SITES:,} / {denominator:,} discovered instruction sites reconstructed and verified**\n\n"
        "This percentage uses the pinned static-analysis worklist as its denominator.\n"
        "That worklist is incomplete, so it is a useful progress gauge for this project,\n"
        "not a claim about whole-game completion. Recalculate it\n"
        "with `python3 tools/progress.py`; use `--json` for automation.\n"
        "Run `python3 tools/progress.py --check-readme` to verify this block, or\n"
        "`python3 tools/progress.py --update-readme` to synchronize it. These progress\n"
        "checks are ROM-free and run as part of the test suite."
    )


def sync_readme(denominator: int, *, update: bool) -> bool:
    text = README.read_text()
    block = readme_progress_block(denominator)
    pattern = re.compile(
        r"\*\*[0-9.]+% — [0-9,]+ / [0-9,]+ discovered instruction sites reconstructed and verified\*\*"
        r"\n\nThis percentage uses the pinned static-analysis worklist as its denominator\."
        r"\n[\s\S]*?checks are ROM-free and run as part of the test suite\."
    )
    match = pattern.search(text)
    if match is None:
        raise ValueError("README progress block not found")
    current = match.group(0)
    if current == block:
        return True
    if update:
        README.write_text(text[:match.start()] + block + text[match.end():])
    return False


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--check-readme", action="store_true", help="fail if README progress is stale")
    modes.add_argument("--update-readme", action="store_true", help="update the README progress block")
    args = parser.parse_args()
    denominator, source = discovered_instruction_variants()
    if args.check_readme or args.update_readme:
        try:
            current = sync_readme(denominator, update=args.update_readme)
        except (OSError, ValueError) as exc:
            print(f"README progress check failed: {exc}")
            return 1
        if args.check_readme:
            print("README progress is up to date." if current else "README progress is stale.")
            return 0 if current else 1
        print("README progress updated." if not current else "README progress already up to date.")
        return 0
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

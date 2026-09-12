#!/usr/bin/env python3
"""Extract Klepto bank-$02 menu/chrome diffs into src/text_en_chrome.inc.

Offline research only — product path never applies IPS. Bank $00 code hooks are
intentionally omitted (they JSL into Klepto's $10 gate stub).
"""
from __future__ import annotations

import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ROM = ROOT / "Backup" / "Dragon Ball Z - Super Saiya Densetsu (Japan) (Rev 1).sfc"
DEFAULT_IPS = ROOT / "research" / "klepto-reference" / "klepto-ssd-en-1.02.ips"
DEFAULT_OUT = ROOT / "src" / "text_en_chrome.inc"


def apply_ips(rom: bytes, ips: bytes, header: int = 512) -> bytes:
    out = bytearray(b"\x00" * header + rom)
    i = 5
    while i < len(ips) - 3:
        off = (ips[i] << 16) | (ips[i + 1] << 8) | ips[i + 2]
        i += 3
        if off == 0x454F46:
            break
        size = (ips[i] << 8) | ips[i + 1]
        i += 2
        if size == 0:
            rsize = (ips[i] << 8) | ips[i + 1]
            i += 2
            val = ips[i]
            i += 1
            data = bytes([val]) * rsize
            size = rsize
        else:
            data = ips[i : i + size]
            i += size
        out[off : off + size] = data
    return bytes(out[header:])


def collect_runs(jp: bytes, en: bytes, start: int, end: int):
    runs = []
    i = start
    while i < end:
        if jp[i] != en[i]:
            j = i
            while j < end and jp[j] != en[j]:
                j += 1
            runs.append((i, en[i:j]))
            i = j
        else:
            i += 1
    return runs


def fmt_bytes(b: bytes) -> str:
    lines = []
    for k in range(0, len(b), 12):
        chunk = b[k : k + 12]
        lines.append("  " + ",".join(f"0x{x:02x}" for x in chunk) + ",")
    return "\n".join(lines)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--rom", type=Path, default=DEFAULT_ROM)
    ap.add_argument("--klepto-ips", type=Path, default=DEFAULT_IPS)
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = ap.parse_args()

    jp = args.rom.read_bytes()
    if len(jp) == 1048576 + 512:
        jp = jp[512:]
    en = apply_ips(jp, args.klepto_ips.read_bytes())
    runs = collect_runs(jp, en, 0x10000, 0x18000)
    total = sum(len(b) for _, b in runs)

    lines = [
        "/* EN overworld/menu chrome — offline Klepto 1.02 bank $02 extract.",
        " * DO NOT EDIT BY HAND — regenerate:",
        " *   python3 tools/extract_chrome.py",
        " * Runtime: cart read filter overlays these file offsets; ROM image stays JP.",
        " * Bank $00 code hooks intentionally omitted (depend on Klepto $10 gate).",
        " */",
        "#ifndef DBZ_TEXT_EN_CHROME_INC",
        "#define DBZ_TEXT_EN_CHROME_INC",
        f"#define DBZ_TEXT_EN_CHROME_RUNS {len(runs)}u",
        "typedef struct { uint32_t file_off; uint16_t len; const uint8_t *bytes; } DbzTextEnChromeRun;",
    ]
    for idx, (off, b) in enumerate(runs):
        lines.append(f"static const uint8_t DBZ_EN_CHROME_{idx}[] = {{")
        lines.append(fmt_bytes(b))
        lines.append("};")
    lines.append(
        "static const DbzTextEnChromeRun DBZ_TEXT_EN_CHROME[DBZ_TEXT_EN_CHROME_RUNS] = {"
    )
    for idx, (off, b) in enumerate(runs):
        lines.append(f"  {{ 0x{off:05x}u, {len(b)}u, DBZ_EN_CHROME_{idx} }},")
    lines.append("};")
    lines.append("#endif /* DBZ_TEXT_EN_CHROME_INC */")
    lines.append("")
    args.out.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {args.out} runs={len(runs)} bytes={total}")


if __name__ == "__main__":
    main()

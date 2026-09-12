#!/usr/bin/env python3
"""Extract Klepto chrome/title diffs into src/text_en_chrome.inc.

Offline research only — product path never applies IPS.

Overlays (cartRomFilter, lang=EN):
  - LoROM bank $02 menu/field chrome (Talk/Look/Fly/…)
  - Bank $00 *data* the JP loader already consumes:
      $00:F150 compressed title-logo/subtitle stream (AE29 from $00:F130)
      $00:AE6A title/copyright tile-index strings
      $00:BD86 packed glyph tables + B3DE/B460 pointer immediates

Skipped (JSL $10:A000 / unused stubs — not present on clean Rev 1):
  - $00:A296 JSR $AE38 → $FCF0 + DMA src/size/dest
  - $00:A3C6 LDX #$F130 / JSR $AE29 → JSR $FD30
  - $00:C704 PLB/PLP/RTL → JMP $F400 → JSL $10:A000
  - $00:FCF0 / $FD30 / $FDB5 unused-space stubs
  - Bank $10 gate (46 B) and bank $11 font (already text_en_font.inc)

Title *data* (overlaid):
  - $00:F150 payload (C559 from identical F130 header)
  - $00:AE6A / BD86 tile-index strings + B3DE/B460 pointer immediates
"""
from __future__ import annotations

import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ROM = ROOT / "Backup" / "Dragon Ball Z - Super Saiya Densetsu (Japan) (Rev 1).sfc"
DEFAULT_IPS = ROOT / "research" / "klepto-reference" / "klepto-ssd-en-1.02.ips"
DEFAULT_OUT = ROOT / "src" / "text_en_chrome.inc"

# Bank $00 *code* that JSLs $10:A000 or unused-space stubs. Title kanji is
# BG0 tm=$6000 / chr=$5000 (stream $00:EFE3 unpatched). Logo is BG1 tm=$6800 /
# chr=$4000. Katakana circles are OBJ (not in this chrome table). EN small-font
# legend/copyright is AE6A data (overlaid). $10-gate equivalent for font is
# dbz_text_en_font_ensure. Kanji nametable + circle OAM are blanked by
# dbz_text_en_title_ensure (leave-vblank).
BANK00_CODE_SKIP = (
    (0x02290, 0x022C0),  # $00:A296 JSR $FCF0 + DMA src/size/dest
    (0x023C0, 0x023D0),  # $00:A3C6 JSR $FD30
    (0x04700, 0x04710),  # $00:C704 JMP $F400 → JSL $10:A000
    (0x07CF0, 0x08000),  # $00:FCF0 / $FD30 / $FDB5 stubs
)

# Inclusive scan windows (unheadered file offsets).
SCAN_WINDOWS = (
    (0x00000, 0x08000, "bank $00 data"),  # filtered by BANK00_CODE_SKIP
    (0x10000, 0x18000, "bank $02 chrome"),
)


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


def in_skip(off: int, skip: tuple[tuple[int, int], ...]) -> bool:
    return any(a <= off < b for a, b in skip)


def skip_end(off: int, skip: tuple[tuple[int, int], ...]) -> int:
    for a, b in skip:
        if a <= off < b:
            return b
    return off


def collect_runs(
    jp: bytes,
    en: bytes,
    start: int,
    end: int,
    skip: tuple[tuple[int, int], ...] = (),
):
    runs = []
    i = start
    while i < end:
        if in_skip(i, skip):
            i = skip_end(i, skip)
            continue
        if jp[i] != en[i]:
            j = i
            while j < end and jp[j] != en[j] and not in_skip(j, skip):
                j += 1
            runs.append((i, bytes(en[i:j])))
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

    runs: list[tuple[int, bytes]] = []
    for start, end, _label in SCAN_WINDOWS:
        skip = BANK00_CODE_SKIP if start == 0x00000 else ()
        runs.extend(collect_runs(jp, en, start, end, skip))
    runs.sort(key=lambda r: r[0])
    total = sum(len(b) for _, b in runs)
    bank00 = sum(len(b) for off, b in runs if off < 0x8000)
    bank02 = sum(len(b) for off, b in runs if 0x10000 <= off < 0x18000)

    lines = [
        "/* EN overworld/menu chrome + title-logo data — offline Klepto 1.02 extract.",
        " * DO NOT EDIT BY HAND — regenerate:",
        " *   python3 tools/extract_chrome.py",
        " * Runtime: cart read filter overlays these file offsets; ROM image stays JP.",
        " * Bank $00 *code* hooks omitted (they JSL Klepto $10:A000 / unused stubs).",
        " * Title CHR: JP AE29 decompresses $00:F130 (header identical; payload @ F150).",
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
    print(
        f"wrote {args.out} runs={len(runs)} bytes={total} "
        f"(bank00_data={bank00} bank02={bank02})"
    )


if __name__ == "__main__":
    main()

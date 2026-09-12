#!/usr/bin/env python3
"""Dump Super Saiya Densetsu main-script pointers/strings (read-only).

Evidence (file offsets = unheadered Rev 1, LoROM):
  - 366× u16 LE pointers at $30000 → CPU 06:8000 / 86:8000
  - String body ~$302DC–$35108 (06:82DC…)
  - Far-pointer table of script banks at 07:8F16 (24-bit entries);
    index 2 = 06:8000 (main dialogue).
  - Decoder leaf 01:CFB2 LDA [$00],Y; glyph words STA $7E3000,X;
    controls include F4 xx 7E (box/speaker), FF line, FE page, 7F end.

Optional: align Klepto IPS 1.02 English *reference* bytes by string index
(never apply IPS at runtime — research/klepto-reference/ only).

Usage:
  python3 tools/extract_script_strings.py
  python3 tools/extract_script_strings.py --klepto --limit 20
  python3 tools/extract_script_strings.py --json out.json
  python3 tools/extract_script_strings.py --klepto --c-inc src/text_en_scripts.inc
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ROM = ROOT / "Backup/Dragon Ball Z - Super Saiya Densetsu (Japan) (Rev 1).sfc"
KLEPTO_IPS = ROOT / "research/klepto-reference/klepto-ssd-en-1.02.ips"
EXPECTED_SHA256 = "962aa7a09765a97164af67098877a8fe5b7f1ea9db738561eb466b7600fd241c"
PTR_TABLE = 0x30000
PTR_COUNT = 366
SCRIPT_END_HINT = 0x35108
FAR_TABLE_CPU = (0x07, 0x8F16)

# Documented control / structure bytes (dialogue stream, not actor DEC3).
CONTROLS = {
    0x01: "SPACE_OR_GAP",
    0x7C: "PERIOD_ISH",
    0x7B: "BANG_ISH",
    0x7D: "ELLIPSIS_OR_BLANK",
    0x7F: "END_BOX",
    0xFE: "PAGE_BREAK",
    0xFF: "LINE_BREAK",
}


def sha256(data: bytes) -> str:
    import hashlib

    return hashlib.sha256(data).hexdigest()


def lorom_file(bank: int, addr: int) -> int:
    return ((bank & 0x7F) << 15) | (addr & 0x7FFF)


def cpu_of_file(fo: int) -> str:
    bank = 0x80 + (fo >> 15)
    addr = 0x8000 + (fo & 0x7FFF)
    return f"{bank:02X}:{addr:04X}"


def apply_ips(headered: bytearray, ips: bytes) -> None:
    assert ips[:5] == b"PATCH" and ips[-3:] == b"EOF"
    pos = 5
    while pos < len(ips) - 3:
        if ips[pos : pos + 3] == b"EOF":
            break
        offset = (ips[pos] << 16) | (ips[pos + 1] << 8) | ips[pos + 2]
        pos += 3
        size = (ips[pos] << 8) | ips[pos + 1]
        pos += 2
        if size == 0:
            rle = (ips[pos] << 8) | ips[pos + 1]
            fill = ips[pos + 2]
            pos += 3
            headered[offset : offset + rle] = bytes([fill]) * rle
        else:
            headered[offset : offset + size] = ips[pos : pos + size]
            pos += size


def load_rom(path: Path) -> bytes:
    raw = path.read_bytes()
    header = 512 if len(raw) % 32768 == 512 else 0
    data = raw[header:]
    if sha256(data) != EXPECTED_SHA256:
        raise SystemExit("ROM SHA-256 mismatch — refuse to dump (need pinned Rev 1)")
    return data


def ptr_file(ptr: int) -> int:
    return PTR_TABLE + (ptr & 0x7FFF)


def annotate(raw: bytes) -> str:
    parts: list[str] = []
    i = 0
    while i < len(raw):
        b = raw[i]
        if b == 0xF4 and i + 2 < len(raw) and raw[i + 2] == 0x7E:
            parts.append(f"[F4 face={raw[i+1]:02X}]")
            i += 3
            continue
        name = CONTROLS.get(b)
        if name:
            parts.append(f"[{name}]")
            i += 1
            continue
        parts.append(f"{b:02X}")
        i += 1
    return " ".join(parts)


def extract_strings(blob: bytes) -> list[dict]:
    ptrs = list(struct.unpack_from("<" + "H" * PTR_COUNT, blob, PTR_TABLE))
    out = []
    for i, p in enumerate(ptrs):
        start = ptr_file(p)
        if i + 1 < len(ptrs):
            end = ptr_file(ptrs[i + 1])
        else:
            end = SCRIPT_END_HINT
        if end < start:
            end = start
        raw = blob[start:end]
        out.append(
            {
                "index": i,
                "ptr_cpu": f"06:{p:04X}",
                "file_offset": start,
                "length": len(raw),
                "hex": raw.hex(" "),
                "annotated": annotate(raw),
            }
        )
    return out


def far_table(blob: bytes, n: int = 8) -> list[dict]:
    base = lorom_file(*FAR_TABLE_CPU)
    rows = []
    for i in range(n):
        o = base + i * 3
        lo = blob[o] | (blob[o + 1] << 8)
        bank = blob[o + 2]
        rows.append({"index": i, "cpu": f"{bank:02X}:{lo:04X}", "file": lorom_file(bank, lo)})
    return rows



OVERLAY_MAX = 0x800  # DBZ_TEXT_EN_OVERLAY_MAX — skip longer EN blobs
MAIN_BANK_SEL = 2


def file_to_cpu06(fo: int) -> int:
    """LoROM file offset → bank-06-style 24-bit CPU long (matches i18n jp_base)."""
    bank = fo >> 15
    addr = 0x8000 + (fo & 0x7FFF)
    return (bank << 16) | addr


def emit_c_inc(jp_strings: list[dict], en_strings: list[dict], out: Path) -> dict:
    """Write src/text_en_scripts.inc for i18n.c (DbzEnScript + DBZ_MAIN_SCRIPT_BANK_SEL in scope)."""
    entries: list[tuple[int, str, int, int]] = []
    skipped_empty: list[int] = []
    skipped_over: list[tuple[int, int]] = []
    chunks: list[str] = [
        "/* Auto-generated EN main-script tables (Klepto 1.02 reference bytes).",
        " * DO NOT EDIT BY HAND — regenerate:",
        " *   python3 tools/extract_script_strings.py --klepto --c-inc src/text_en_scripts.inc",
        " * Policy: offline IPS apply for research extract only; product never patches ROM.",
        " * Bank sel = $0733 index 2 → 06:8000. Skips empty / len>$0800 (overlay max).",
        " */",
        "#ifndef DBZ_TEXT_EN_SCRIPTS_INC",
        "#define DBZ_TEXT_EN_SCRIPTS_INC",
        "",
    ]
    for jp_s, en_s in zip(jp_strings, en_strings):
        i = int(en_s["index"])
        raw = bytes.fromhex(en_s["hex"].replace(" ", "")) if en_s["hex"] else b""
        if len(raw) != en_s["length"]:
            raise SystemExit(f"hex/length mismatch at index {i}")
        if len(raw) == 0:
            skipped_empty.append(i)
            continue
        if len(raw) > OVERLAY_MAX:
            skipped_over.append((i, len(raw)))
            continue
        jp_base = file_to_cpu06(int(jp_s["file_offset"]))
        name = f"EN_SCRIPT_{i}"
        parts = [f"0x{b:02x}" for b in raw]
        body_lines = []
        for c in range(0, len(parts), 12):
            slice_ = parts[c : c + 12]
            comma = "," if c + 12 < len(parts) else ""
            body_lines.append("  " + ", ".join(slice_) + comma)
        chunks.append(f"static const uint8_t {name}[] = {{")
        chunks.extend(body_lines)
        chunks.append("};")
        chunks.append("")
        entries.append((i, name, jp_base, len(raw)))

    over_note = ", ".join(f"{i}({n})" for i, n in skipped_over) or "none"
    empty_note = (
        f"{skipped_empty[0]}–{skipped_empty[-1]} ({len(skipped_empty)})"
        if skipped_empty
        else "none"
    )
    chunks.append(f"#define DBZ_EN_MAIN_SCRIPT_COUNT {len(entries)}u")
    chunks.append(f"#define DBZ_EN_MAIN_SCRIPT_PTR_TOTAL {PTR_COUNT}u")
    chunks.append(f"/* Skipped empty: {empty_note}; over-overlay: {over_note} */")
    chunks.append("")
    chunks.append("static const DbzEnScript EN_SCRIPTS[] = {")
    for i, name, jp_base, _ln in entries:
        chunks.append(
            f"  {{DBZ_MAIN_SCRIPT_BANK_SEL, {i}u, {name}, sizeof {name}, 0x{jp_base:06X}u}},"
        )
    chunks.append("};")
    chunks.append("")
    chunks.append("#endif /* DBZ_TEXT_EN_SCRIPTS_INC */")
    chunks.append("")
    out.write_text("\n".join(chunks))
    return {
        "path": str(out),
        "covered": len(entries),
        "ptr_total": PTR_COUNT,
        "skipped_empty": skipped_empty,
        "skipped_over": skipped_over,
        "bytes": sum(e[3] for e in entries),
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--rom", type=Path, default=ROM)
    ap.add_argument("--klepto", action="store_true", help="Also dump Klepto EN reference strings")
    ap.add_argument("--klepto-ips", type=Path, default=KLEPTO_IPS)
    ap.add_argument("--limit", type=int, default=0, help="Print only first N strings (0=all meta)")
    ap.add_argument("--json", type=Path, help="Write full dump JSON")
    ap.add_argument(
        "--c-inc",
        type=Path,
        help="With --klepto: write generated C include (EN script tables for i18n.c)",
    )
    args = ap.parse_args()

    jp = load_rom(args.rom)
    report: dict = {
        "policy": "read-only dump; no ROM writes; Klepto is reference wording only",
        "rom_sha256": sha256(jp),
        "pointer_table_file": PTR_TABLE,
        "pointer_table_cpu": "06:8000",
        "pointer_count": PTR_COUNT,
        "far_bank_table": far_table(jp),
        "encoding_notes": [
            "Custom 1-byte glyph indices (not ASCII/Shift-JIS).",
            "F4 <id> 7E opens a dialogue box / speaker face.",
            "FF line break; FE page; 7F end-of-box; 01 treated as gap by 01:CFB9.",
            "Glyph tile words are written to $7E3000,X by 01:D000 / related.",
            "Script byte fetch leaf: 01:CFB4 LDA [$00],Y (entry 01:CFB2).",
            "Message select: $0733 indexes 07:8F16 far table; $0723 string index.",
            "Actor walker 05:DEC3 is NOT this dialogue stream.",
        ],
        "jp_strings": extract_strings(jp),
    }

    if args.klepto:
        if not args.klepto_ips.is_file():
            raise SystemExit(f"missing Klepto IPS: {args.klepto_ips}")
        headered = bytearray(b"\x00" * 512 + jp)
        apply_ips(headered, args.klepto_ips.read_bytes())
        en = bytes(headered[512:])
        report["klepto_en_strings"] = extract_strings(en)
        report["klepto_note"] = (
            "Offline IPS apply for research only — product path never patches ROM"
        )

    if args.c_inc:
        if not args.klepto or "klepto_en_strings" not in report:
            raise SystemExit("--c-inc requires --klepto (EN reference extract)")
        meta = emit_c_inc(report["jp_strings"], report["klepto_en_strings"], args.c_inc)
        report["c_inc"] = meta
        print(
            f"wrote {meta['path']}: covered {meta['covered']}/{meta['ptr_total']} "
            f"({meta['bytes']} bytes); empty={len(meta['skipped_empty'])} "
            f"over={meta['skipped_over']}",
            file=sys.stderr,
        )

    if args.json:
        args.json.write_text(json.dumps(report, indent=2) + "\n")
        print(f"wrote {args.json}", file=sys.stderr)

    print(f"ROM sha256={report['rom_sha256']}")
    print("Far bank table (07:8F16):")
    for row in report["far_bank_table"]:
        print(f"  [{row['index']}] {row['cpu']}")
    print("Encoding notes:")
    for line in report["encoding_notes"]:
        print(f"  - {line}")

    limit = args.limit if args.limit > 0 else 8
    print(f"\nJP strings (first {limit}):")
    for s in report["jp_strings"][:limit]:
        print(f"  [{s['index']:3d}] {s['ptr_cpu']} len={s['length']:3d} {s['annotated'][:100]}")
        print(f"         hex: {s['hex'][:90]}{'…' if len(s['hex'])>90 else ''}")

    if args.klepto:
        print(f"\nKlepto EN reference (first {limit}):")
        for s in report["klepto_en_strings"][:limit]:
            print(f"  [{s['index']:3d}] {s['ptr_cpu']} len={s['length']:3d} {s['annotated'][:100]}")
            print(f"         hex: {s['hex'][:90]}{'…' if len(s['hex'])>90 else ''}")

    print(f"\nTotal JP strings: {len(report['jp_strings'])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

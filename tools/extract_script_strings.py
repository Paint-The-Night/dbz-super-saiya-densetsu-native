#!/usr/bin/env python3
"""Dump Super Saiya Densetsu script pointers/strings (read-only).

Evidence (file offsets = unheadered Rev 1, LoROM):
  - Far-pointer table at 07:8F16 (8× 24-bit entries). $0733 indexes it.
      [0] 07:8F2E  menu / status
      [1] 07:8F82  battle / command
      [2] 06:8000  main dialogue (366× u16)
      [3] 07:9226  battle / event
      [4] 07:9374  short battle set
      [5] 07:9386  alias set (2)
      [6] 07:938A  alias set (2)
      [7] 07:938E  short menu set
  - Bank-$07 pointer tables are packed sequentially from 07:8F2E through
    07:93AE (first string of sel 0). Counts are those spans / 2.
  - Main body ~06:82DC; last JP string ends near file $35108.
  - Decoder leaf 01:CFB2 LDA [$00],Y; glyph words STA $7E3000,X;
    controls include F4 xx 7E (box/speaker), FF line, FE page, 7F end box.

Length rule (dest-based, not consecutive-pointer):
  A slot's bytes run from its dest to the next *higher unique dest* in that
  table. Equal dests are aliases (same stream) — JP 23–42 share dest with 43.
  The last dest has no successor: walk 7F-not-FE (dialogue) or FF FF / FD FF FF
  (menu). Do **not** use a fixed SCRIPT_END_HINT for the last main slot —
  Klepto packs earlier strings so 06:C64F + $35108 is leftover JP, not one line.

Optional: align Klepto IPS 1.02 English *reference* bytes by string index
(never apply IPS at runtime — research/klepto-reference/ only).

Usage:
  python3 tools/extract_script_strings.py
  python3 tools/extract_script_strings.py --klepto --limit 20
  python3 tools/extract_script_strings.py --json out.json
  python3 tools/extract_script_strings.py --klepto --c-inc src/text_en_scripts.inc
  python3 tools/extract_script_strings.py --klepto --c-inc-banks src/text_en_banks.inc
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
SCRIPT_END_HINT = 0x35108  # JP last-string sentinel only; not used for Klepto 365
FAR_TABLE_CPU = (0x07, 0x8F16)
FAR_TABLE_COUNT = 8
MAIN_BANK_SEL = 2
OVERLAY_MAX = 0x800  # DBZ_TEXT_EN_OVERLAY_MAX

# Documented control / structure bytes (dialogue stream, not actor DEC3).
CONTROLS = {
    0x01: "SPACE_OR_GAP",
    0x7C: "PERIOD_ISH",
    0x7B: "BANG_ISH",
    0x7D: "ELLIPSIS_OR_BLANK",
    0x7F: "END_BOX",
    0xFD: "MENU_END_ISH",
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


def file_to_cpu(fo: int) -> int:
    """LoROM file offset → 24-bit CPU long in the file's bank."""
    bank = fo >> 15
    addr = 0x8000 + (fo & 0x7FFF)
    return (bank << 16) | addr


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


def walk_dialogue_end(data: bytes) -> int:
    """Bytes until END_BOX (7F) not followed by PAGE (FE); keep trailing FF."""
    i = 0
    n = len(data)
    while i < n:
        b = data[i]
        if b == 0xF4 and i + 2 < n and data[i + 2] == 0x7E:
            i += 3
            continue
        if b == 0x7F:
            if i + 1 < n and data[i + 1] == 0xFE:
                i += 2
                if i < n and data[i] == 0xFF:
                    i += 1
                continue
            i += 1
            while i < n and data[i] == 0xFF:
                i += 1
            return i
        i += 1
    return n


def walk_menu_end(data: bytes) -> int:
    """Bytes until FF FF or FD FF FF (menu / battle streams)."""
    n = len(data)
    i = 0
    while i < n - 1:
        if data[i] == 0xFF and data[i + 1] == 0xFF:
            return i + 2
        if data[i] == 0xFD and i + 2 < n and data[i + 1] == 0xFF and data[i + 2] == 0xFF:
            return i + 3
        i += 1
    return n


def dest_lengths(ptrs: list[int], last_len: int) -> list[int]:
    """Length for each slot from dest → next higher unique dest (aliases share)."""
    unique = sorted(set(ptrs))
    nxt = {}
    for i, p in enumerate(unique):
        nxt[p] = (unique[i + 1] - p) if i + 1 < len(unique) else last_len
    return [nxt[p] for p in ptrs]


def in_table_body(ptr: int, first_ptr: int) -> bool:
    """Dests below the table's first pointer sit in an earlier bank-07 region
    (e.g. sel 7 trailing 93AE aliases of sel 0 string 0) — treat as unused."""
    return ptr >= first_ptr


def extract_ptr_table(
    blob: bytes,
    bank: int,
    tbl_addr: int,
    count: int,
    *,
    style: str,
    last_walk_limit: int = 0x200,
) -> list[dict]:
    fo = lorom_file(bank, tbl_addr)
    ptrs = [blob[fo + i * 2] | (blob[fo + i * 2 + 1] << 8) for i in range(count)]
    max_dest = max(ptrs)
    last_start = lorom_file(bank, max_dest)
    probe = blob[last_start : last_start + last_walk_limit]
    if style == "dialogue":
        last_len = walk_dialogue_end(probe)
    else:
        last_len = walk_menu_end(probe)
    if last_len <= 0:
        last_len = 1
    lens = dest_lengths(ptrs, last_len)
    body0 = ptrs[0] if ptrs else 0
    out = []
    for i, p in enumerate(ptrs):
        start = lorom_file(bank, p)
        ln = max(0, lens[i]) if in_table_body(p, body0) else 0
        raw = blob[start : start + ln] if ln else b""
        out.append(
            {
                "index": i,
                "ptr_cpu": f"{bank:02X}:{p:04X}",
                "file_offset": start,
                "length": len(raw),
                "hex": raw.hex(" "),
                "annotated": annotate(raw),
            }
        )
    return out


def extract_main_strings(blob: bytes) -> list[dict]:
    """Main 06:8000 — dest-based lengths; last slot via 7F-not-FE walk."""
    ptrs = list(struct.unpack_from("<" + "H" * PTR_COUNT, blob, PTR_TABLE))
    max_dest = max(ptrs)
    last_start = PTR_TABLE + (max_dest & 0x7FFF)
    last_len = walk_dialogue_end(blob[last_start : last_start + 0x1000])
    lens = dest_lengths([p & 0x7FFF for p in ptrs], last_len)
    # dest_lengths used 15-bit file-relative; remap via original ptrs
    unique15 = sorted(set(p & 0x7FFF for p in ptrs))
    nxt15 = {}
    for i, p in enumerate(unique15):
        nxt15[p] = (unique15[i + 1] - p) if i + 1 < len(unique15) else last_len
    out = []
    for i, p in enumerate(ptrs):
        start = PTR_TABLE + (p & 0x7FFF)
        ln = max(0, nxt15[p & 0x7FFF])
        raw = blob[start : start + ln]
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


def far_table(blob: bytes, n: int = FAR_TABLE_COUNT) -> list[dict]:
    base = lorom_file(*FAR_TABLE_CPU)
    rows = []
    for i in range(n):
        o = base + i * 3
        lo = blob[o] | (blob[o + 1] << 8)
        bank = blob[o + 2]
        rows.append(
            {
                "index": i,
                "cpu": f"{bank:02X}:{lo:04X}",
                "bank": bank,
                "addr": lo,
                "file": lorom_file(bank, lo),
            }
        )
    return rows


def packed_bank07_counts(blob: bytes, far: list[dict]) -> list[dict]:
    """Infer pointer counts for bank-$07 far entries from sequential packing."""
    fo0 = lorom_file(0x07, far[0]["addr"])
    first_str_addr = blob[fo0] | (blob[fo0 + 1] << 8)
    b07 = [(r["index"], r["addr"]) for r in far if r["bank"] == 0x07]
    b07.sort(key=lambda x: x[1])
    out = []
    for k, (sel, addr) in enumerate(b07):
        end = b07[k + 1][1] if k + 1 < len(b07) else first_str_addr
        count = (end - addr) // 2
        out.append(
            {
                "bank_sel": sel,
                "tbl_cpu": f"07:{addr:04X}",
                "tbl_addr": addr,
                "tbl_end": end,
                "count": count,
                "style": "menu",
            }
        )
    return out


def extract_all_banks(blob: bytes) -> dict[int, list[dict]]:
    far = far_table(blob)
    banks: dict[int, list[dict]] = {}
    banks[MAIN_BANK_SEL] = extract_main_strings(blob)
    for spec in packed_bank07_counts(blob, far):
        banks[spec["bank_sel"]] = extract_ptr_table(
            blob,
            0x07,
            spec["tbl_addr"],
            spec["count"],
            style=spec["style"],
        )
    return banks


def _emit_byte_array(name: str, raw: bytes) -> list[str]:
    parts = [f"0x{b:02x}" for b in raw]
    lines = [f"static const uint8_t {name}[] = {{"]
    for c in range(0, len(parts), 12):
        slice_ = parts[c : c + 12]
        comma = "," if c + 12 < len(parts) else ""
        lines.append("  " + ", ".join(slice_) + comma)
    lines.append("};")
    lines.append("")
    return lines


def emit_c_inc(jp_strings: list[dict], en_strings: list[dict], out: Path) -> dict:
    """Write src/text_en_scripts.inc (main bank sel 2)."""
    dest_name: dict[int, str] = {}
    entries: list[tuple[int, str, int, int]] = []
    skipped_empty: list[int] = []
    skipped_over: list[tuple[int, int]] = []
    aliased: list[int] = []
    chunks: list[str] = [
        "/* Auto-generated EN main-script tables (Klepto 1.02 reference bytes).",
        " * DO NOT EDIT BY HAND — regenerate:",
        " *   python3 tools/extract_script_strings.py --klepto --c-inc src/text_en_scripts.inc",
        " * Policy: offline IPS apply for research extract only; product never patches ROM.",
        " * Bank sel = $0733 index 2 → 06:8000.",
        " * Lengths are dest-based (aliases share bytes). Last slot uses 7F-not-FE walk",
        " * (do not use file $35108 as Klepto end — leftover JP after the real line).",
        " * Indices 23–42 share dest with 43 in JP *and* Klepto (unused alias slots).",
        " */",
        "#ifndef DBZ_TEXT_EN_SCRIPTS_INC",
        "#define DBZ_TEXT_EN_SCRIPTS_INC",
        "",
    ]
    # First pass: unique dests
    for jp_s, en_s in zip(jp_strings, en_strings):
        i = int(en_s["index"])
        raw = bytes.fromhex(en_s["hex"].replace(" ", "")) if en_s["hex"] else b""
        if len(raw) != en_s["length"]:
            raise SystemExit(f"hex/length mismatch at index {i}")
        dest = int(en_s["file_offset"])
        if len(raw) == 0:
            skipped_empty.append(i)
            continue
        if len(raw) > OVERLAY_MAX:
            skipped_over.append((i, len(raw)))
            continue
        if dest not in dest_name:
            name = f"EN_SCRIPT_{i}"
            dest_name[dest] = name
            chunks.extend(_emit_byte_array(name, raw))
        else:
            aliased.append(i)
        jp_base = file_to_cpu(int(jp_s["file_offset"]))
        entries.append((i, dest_name[dest], jp_base, len(raw)))

    over_note = ", ".join(f"{i}({n})" for i, n in skipped_over) or "none"
    empty_note = (
        f"{skipped_empty[0]}–{skipped_empty[-1]} ({len(skipped_empty)})"
        if skipped_empty
        else "none"
    )
    alias_note = (
        f"{aliased[0]}–{aliased[-1]} ({len(aliased)}) share dest with next distinct"
        if aliased
        else "none"
    )
    chunks.append(f"#define DBZ_EN_MAIN_SCRIPT_COUNT {len(entries)}u")
    chunks.append(f"#define DBZ_EN_MAIN_SCRIPT_PTR_TOTAL {PTR_COUNT}u")
    chunks.append(f"#define DBZ_EN_MAIN_SCRIPT_UNIQUE {len(dest_name)}u")
    chunks.append(
        f"/* skipped empty: {empty_note}; over-overlay: {over_note}; aliases: {alias_note} */"
    )
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
        "unique": len(dest_name),
        "ptr_total": PTR_COUNT,
        "skipped_empty": skipped_empty,
        "skipped_over": skipped_over,
        "aliased": aliased,
        "bytes": sum(e[3] for e in entries),
    }


def emit_c_inc_banks(
    jp_banks: dict[int, list[dict]],
    en_banks: dict[int, list[dict]],
    out: Path,
) -> dict:
    """Write src/text_en_banks.inc — far-table sels other than main (2)."""
    chunks: list[str] = [
        "/* Auto-generated EN menu/battle bank tables (Klepto 1.02 reference).",
        " * DO NOT EDIT BY HAND — regenerate:",
        " *   python3 tools/extract_script_strings.py --klepto --c-inc-banks src/text_en_banks.inc",
        " * Policy: offline IPS apply for research extract only; product never patches ROM.",
        " * Far table 07:8F16: sel 0=07:8F2E 1=07:8F82 3=07:9226 4=07:9374",
        " *   5=07:9386 6=07:938A 7=07:938E. Pointer counts from packed spans.",
        " * Same glyph encoding as main (Klepto Latin tiles); pointer-swap overlay.",
        " */",
        "#ifndef DBZ_TEXT_EN_BANKS_INC",
        "#define DBZ_TEXT_EN_BANKS_INC",
        "",
    ]
    per_bank: dict[int, int] = {}
    entries: list[tuple[int, int, str, int, int]] = []
    skipped_empty: list[tuple[int, int]] = []
    skipped_over: list[tuple[int, int, int]] = []
    unique_arrays = 0

    for sel in sorted(k for k in en_banks if k != MAIN_BANK_SEL):
        en_list = en_banks[sel]
        jp_list = jp_banks.get(sel, [])
        jp_by_i = {int(s["index"]): s for s in jp_list}
        dest_name: dict[int, str] = {}
        covered = 0
        for en_s in en_list:
            i = int(en_s["index"])
            raw = bytes.fromhex(en_s["hex"].replace(" ", "")) if en_s["hex"] else b""
            dest = int(en_s["file_offset"])
            if len(raw) == 0:
                skipped_empty.append((sel, i))
                continue
            if len(raw) > OVERLAY_MAX:
                skipped_over.append((sel, i, len(raw)))
                continue
            if dest not in dest_name:
                name = f"EN_B{sel}_{i}"
                dest_name[dest] = name
                chunks.extend(_emit_byte_array(name, raw))
                unique_arrays += 1
            jp_s = jp_by_i.get(i)
            jp_base = file_to_cpu(int(jp_s["file_offset"])) if jp_s else 0
            entries.append((sel, i, dest_name[dest], jp_base, len(raw)))
            covered += 1
        per_bank[sel] = covered

    for sel, n in per_bank.items():
        chunks.append(f"#define DBZ_EN_BANK{sel}_COUNT {n}u")
    chunks.append(f"#define DBZ_EN_BANK_SCRIPT_COUNT {len(entries)}u")
    chunks.append(
        f"/* skipped empty={len(skipped_empty)} over={skipped_over or 'none'} */"
    )
    chunks.append("")
    chunks.append("static const DbzEnScript EN_BANK_SCRIPTS[] = {")
    for sel, i, name, jp_base, _ln in entries:
        chunks.append(f"  {{{sel}u, {i}u, {name}, sizeof {name}, 0x{jp_base:06X}u}},")
    chunks.append("};")
    chunks.append("")
    chunks.append("#endif /* DBZ_TEXT_EN_BANKS_INC */")
    chunks.append("")
    out.write_text("\n".join(chunks))
    return {
        "path": str(out),
        "covered": len(entries),
        "unique": unique_arrays,
        "per_bank": per_bank,
        "skipped_empty": skipped_empty,
        "skipped_over": skipped_over,
        "bytes": sum(e[4] for e in entries),
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
        help="With --klepto: write generated C include (EN main-script tables)",
    )
    ap.add_argument(
        "--c-inc-banks",
        type=Path,
        help="With --klepto: write generated C include (EN menu/battle banks)",
    )
    args = ap.parse_args()

    jp = load_rom(args.rom)
    jp_far = far_table(jp)
    jp_banks = extract_all_banks(jp)
    report: dict = {
        "policy": "read-only dump; no ROM writes; Klepto is reference wording only",
        "rom_sha256": sha256(jp),
        "pointer_table_file": PTR_TABLE,
        "pointer_table_cpu": "06:8000",
        "pointer_count": PTR_COUNT,
        "far_bank_table": [
            {k: v for k, v in r.items() if k != "file"} | {"file": r["file"]}
            for r in jp_far
        ],
        "bank07_packed": packed_bank07_counts(jp, jp_far),
        "encoding_notes": [
            "Custom 1-byte glyph indices (not ASCII/Shift-JIS).",
            "F4 <id> 7E opens a dialogue box / speaker face (main script).",
            "FF line break; FE page; 7F end-of-box; 01 treated as gap by 01:CFB9.",
            "Menu/battle streams often end FF FF or FD FF FF (no F4 face).",
            "Glyph tile words are written to $7E3000,X by 01:D000 / related.",
            "Script byte fetch leaf: 01:CFB4 LDA [$00],Y (entry 01:CFB2).",
            "Message select: $0733 indexes 07:8F16 far table; $0723 string index.",
            "Actor walker 05:DEC3 is NOT this dialogue stream.",
            "Main 23–42: unused alias slots (same dest as 43) in JP and Klepto.",
            "Main 365: last dest; length from 7F-not-FE walk, not file $35108.",
        ],
        "jp_strings": jp_banks[MAIN_BANK_SEL],
        "jp_banks": {str(k): v for k, v in jp_banks.items() if k != MAIN_BANK_SEL},
    }

    en_banks: dict[int, list[dict]] | None = None
    if args.klepto:
        if not args.klepto_ips.is_file():
            raise SystemExit(f"missing Klepto IPS: {args.klepto_ips}")
        headered = bytearray(b"\x00" * 512 + jp)
        apply_ips(headered, args.klepto_ips.read_bytes())
        en = bytes(headered[512:])
        en_banks = extract_all_banks(en)
        report["klepto_en_strings"] = en_banks[MAIN_BANK_SEL]
        report["klepto_en_banks"] = {
            str(k): v for k, v in en_banks.items() if k != MAIN_BANK_SEL
        }
        report["klepto_note"] = (
            "Offline IPS apply for research only — product path never patches ROM"
        )

    if args.c_inc:
        if not args.klepto or en_banks is None:
            raise SystemExit("--c-inc requires --klepto (EN reference extract)")
        meta = emit_c_inc(jp_banks[MAIN_BANK_SEL], en_banks[MAIN_BANK_SEL], args.c_inc)
        report["c_inc"] = meta
        print(
            f"wrote {meta['path']}: covered {meta['covered']}/{meta['ptr_total']} "
            f"unique={meta['unique']} ({meta['bytes']} entry-bytes); "
            f"empty={len(meta['skipped_empty'])} over={meta['skipped_over']} "
            f"aliases={len(meta['aliased'])}",
            file=sys.stderr,
        )

    if args.c_inc_banks:
        if not args.klepto or en_banks is None:
            raise SystemExit("--c-inc-banks requires --klepto")
        meta = emit_c_inc_banks(jp_banks, en_banks, args.c_inc_banks)
        report["c_inc_banks"] = meta
        print(
            f"wrote {meta['path']}: covered {meta['covered']} unique={meta['unique']} "
            f"per_bank={meta['per_bank']} ({meta['bytes']} entry-bytes)",
            file=sys.stderr,
        )

    if args.json:
        args.json.write_text(json.dumps(report, indent=2) + "\n")
        print(f"wrote {args.json}", file=sys.stderr)

    print(f"ROM sha256={report['rom_sha256']}")
    print("Far bank table (07:8F16):")
    for row in report["far_bank_table"]:
        print(f"  [{row['index']}] {row['cpu']}")
    print("Bank $07 packed pointer counts:")
    for spec in report["bank07_packed"]:
        print(
            f"  sel {spec['bank_sel']} {spec['tbl_cpu']}-{spec['tbl_end']:04X} "
            f"n={spec['count']}"
        )
    print("Encoding notes:")
    for line in report["encoding_notes"]:
        print(f"  - {line}")

    limit = args.limit if args.limit > 0 else 8
    print(f"\nJP main strings (first {limit}):")
    for s in report["jp_strings"][:limit]:
        print(f"  [{s['index']:3d}] {s['ptr_cpu']} len={s['length']:3d} {s['annotated'][:100]}")
        print(f"         hex: {s['hex'][:90]}{'…' if len(s['hex'])>90 else ''}")

    if args.klepto and en_banks is not None:
        print(f"\nKlepto EN main (first {limit}):")
        for s in report["klepto_en_strings"][:limit]:
            print(f"  [{s['index']:3d}] {s['ptr_cpu']} len={s['length']:3d} {s['annotated'][:100]}")
            print(f"         hex: {s['hex'][:90]}{'…' if len(s['hex'])>90 else ''}")
        s365 = report["klepto_en_strings"][365]
        print(f"\nKlepto EN [365] len={s365['length']} (7F-not-FE walk; not $35108 leftover)")
        print("Klepto EN other banks (first 3 each):")
        for sel in sorted(int(k) for k in report["klepto_en_banks"]):
            rows = report["klepto_en_banks"][str(sel)]
            nonempty = sum(1 for r in rows if r["length"] > 0)
            print(f"  sel {sel}: {nonempty}/{len(rows)} nonempty")
            for s in rows[:3]:
                print(f"    [{s['index']:3d}] {s['ptr_cpu']} len={s['length']:3d} {s['annotated'][:80]}")

    print(f"\nTotal JP main strings: {len(report['jp_strings'])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

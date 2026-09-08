#!/usr/bin/env python3
"""Scout-lane analysis for DBZ Super Saiya Densetsu native reconstruction.

READ-ONLY on src/. Writes only under research/scout/.
Regenerates hot-pcs.{json,md}, call-graph-leaves.md, priority-queue.md.
"""
from __future__ import annotations

import csv
import hashlib
import json
import struct
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ROM_PATH = ROOT / "Backup/Dragon Ball Z - Super Saiya Densetsu (Japan) (Rev 1).sfc"
EXPECTED_SHA = "962aa7a09765a97164af67098877a8fe5b7f1ea9db738561eb466b7600fd241c"
OUT = Path(__file__).resolve().parent

# Reviewed native PC ranges in bank $00 (inclusive), from docs/progress.md + native.c dispatch.
NATIVE_RANGES = [
    (0x8000, 0x800D),
    (0x8275, 0x828E),
    (0x828F, 0x82A2),
    (0x82A3, 0x82C6),
    (0x82D4, 0x82FA),
    (0x82FB, 0x8323),
    (0x8324, 0x83CB),
    (0x83CC, 0x83D9),
    (0x83E4, 0x8401),
    (0x8402, 0x8432),
    (0x8433, 0x845F),
    (0x8460, 0x8470),
    (0x8471, 0x849B),
    (0x86A0, 0x86B5),
    (0x86B6, 0x86C7),
    (0x86C8, 0x86FB),
    (0x86FC, 0x8710),
    (0x8711, 0x8723),
    (0x8724, 0x8731),
    (0x8732, 0x877D),
    (0x8887, 0x88AD),
    (0x88AE, 0x88C4),
    (0x88C5, 0x88D7),
    (0x88D8, 0x88E9),
    (0x88EA, 0x8907),
    (0x8908, 0x8920),
    (0x8921, 0x8953),
    (0x8954, 0x8995),
    (0x8996, 0x89AB),
    (0x89AC, 0x89B7),
    (0x8AD6, 0x8AE4),
    (0x8AF1, 0x8B26),
    (0x8B28, 0x8B51),
    (0x8DFE, 0x8E25),
    (0x8E26, 0x8E75),
    (0x9B36, 0x9B78),
    (0x9B79, 0x9BBE),
]

# Known JSL callees from already-native code (bank:addr) that are NOT yet native.
NATIVE_CALLEES = []  # all previously listed callees are now native in src/native.c

# Scene-mode jump table at $00:8B54 (words); JMP ($8B54,X) from native 8B51.
SCENE_TABLE_BASE = 0x8B54

# Opcode lengths for 65c816 (M/X=1 default for size estimates; variable ops noted).
# We use a conservative fixed-length table assuming 8-bit A/X for scanning.
# For variable-length (imm), use short form; scanning still finds most RTS/RTL/JSR/JSL.
OPLEN = {
    0x00: 2, 0x01: 2, 0x02: 2, 0x03: 2, 0x04: 2, 0x05: 2, 0x06: 2, 0x07: 2,
    0x08: 1, 0x09: 2, 0x0A: 1, 0x0B: 1, 0x0C: 3, 0x0D: 3, 0x0E: 3, 0x0F: 4,
    0x10: 2, 0x11: 2, 0x12: 2, 0x13: 2, 0x14: 2, 0x15: 2, 0x16: 2, 0x17: 2,
    0x18: 1, 0x19: 3, 0x1A: 1, 0x1B: 1, 0x1C: 3, 0x1D: 3, 0x1E: 3, 0x1F: 4,
    0x20: 3, 0x21: 2, 0x22: 4, 0x23: 2, 0x24: 2, 0x25: 2, 0x26: 2, 0x27: 2,
    0x28: 1, 0x29: 2, 0x2A: 1, 0x2B: 1, 0x2C: 3, 0x2D: 3, 0x2E: 3, 0x2F: 4,
    0x30: 2, 0x31: 2, 0x32: 2, 0x33: 2, 0x34: 2, 0x35: 2, 0x36: 2, 0x37: 2,
    0x38: 1, 0x39: 3, 0x3A: 1, 0x3B: 1, 0x3C: 3, 0x3D: 3, 0x3E: 3, 0x3F: 4,
    0x40: 1, 0x41: 2, 0x42: 2, 0x43: 2, 0x44: 3, 0x45: 2, 0x46: 2, 0x47: 2,
    0x48: 1, 0x49: 2, 0x4A: 1, 0x4B: 1, 0x4C: 3, 0x4D: 3, 0x4E: 3, 0x4F: 4,
    0x50: 2, 0x51: 2, 0x52: 2, 0x53: 2, 0x54: 3, 0x55: 2, 0x56: 2, 0x57: 2,
    0x58: 1, 0x59: 3, 0x5A: 1, 0x5B: 1, 0x5C: 4, 0x5D: 3, 0x5E: 3, 0x5F: 4,
    0x60: 1, 0x61: 2, 0x62: 3, 0x63: 2, 0x64: 2, 0x65: 2, 0x66: 2, 0x67: 2,
    0x68: 1, 0x69: 2, 0x6A: 1, 0x6B: 1, 0x6C: 3, 0x6D: 3, 0x6E: 3, 0x6F: 4,
    0x70: 2, 0x71: 2, 0x72: 2, 0x73: 2, 0x74: 2, 0x75: 2, 0x76: 2, 0x77: 2,
    0x78: 1, 0x79: 3, 0x7A: 1, 0x7B: 1, 0x7C: 3, 0x7D: 3, 0x7E: 3, 0x7F: 4,
    0x80: 2, 0x81: 2, 0x82: 3, 0x83: 2, 0x84: 2, 0x85: 2, 0x86: 2, 0x87: 2,
    0x88: 1, 0x89: 2, 0x8A: 1, 0x8B: 1, 0x8C: 3, 0x8D: 3, 0x8E: 3, 0x8F: 4,
    0x90: 2, 0x91: 2, 0x92: 2, 0x93: 2, 0x94: 2, 0x95: 2, 0x96: 2, 0x97: 2,
    0x98: 1, 0x99: 3, 0x9A: 1, 0x9B: 1, 0x9C: 3, 0x9D: 3, 0x9E: 3, 0x9F: 4,
    0xA0: 2, 0xA1: 2, 0xA2: 2, 0xA3: 2, 0xA4: 2, 0xA5: 2, 0xA6: 2, 0xA7: 2,
    0xA8: 1, 0xA9: 2, 0xAA: 1, 0xAB: 1, 0xAC: 3, 0xAD: 3, 0xAE: 3, 0xAF: 4,
    0xB0: 2, 0xB1: 2, 0xB2: 2, 0xB3: 2, 0xB4: 2, 0xB5: 2, 0xB6: 2, 0xB7: 2,
    0xB8: 1, 0xB9: 3, 0xBA: 1, 0xBB: 1, 0xBC: 3, 0xBD: 3, 0xBE: 3, 0xBF: 4,
    0xC0: 2, 0xC1: 2, 0xC2: 2, 0xC3: 2, 0xC4: 2, 0xC5: 2, 0xC6: 2, 0xC7: 2,
    0xC8: 1, 0xC9: 2, 0xCA: 1, 0xCB: 1, 0xCC: 3, 0xCD: 3, 0xCE: 3, 0xCF: 4,
    0xD0: 2, 0xD1: 2, 0xD2: 2, 0xD3: 2, 0xD4: 2, 0xD5: 2, 0xD6: 2, 0xD7: 2,
    0xD8: 1, 0xD9: 3, 0xDA: 1, 0xDB: 1, 0xDC: 3, 0xDD: 3, 0xDE: 3, 0xDF: 4,
    0xE0: 2, 0xE1: 2, 0xE2: 2, 0xE3: 2, 0xE4: 2, 0xE5: 2, 0xE6: 2, 0xE7: 2,
    0xE8: 1, 0xE9: 2, 0xEA: 1, 0xEB: 1, 0xEC: 3, 0xED: 3, 0xEE: 3, 0xEF: 4,
    0xF0: 2, 0xF1: 2, 0xF2: 2, 0xF3: 2, 0xF4: 3, 0xF5: 2, 0xF6: 2, 0xF7: 2,
    0xF8: 1, 0xF9: 3, 0xFA: 1, 0xFB: 1, 0xFC: 3, 0xFD: 3, 0xFE: 3, 0xFF: 4,
}


def lorom_offset(bank: int, addr: int) -> int:
    return ((bank & 0x7F) * 0x8000) + (addr & 0x7FFF)


def is_native(bank: int, addr: int) -> bool:
    if bank & 0x7F:
        return False
    for a, b in NATIVE_RANGES:
        if a <= addr <= b:
            return True
    return False


def parse_cpu_addr(s: str) -> tuple[int, int]:
    bank_s, addr_s = s.split(":")
    return int(bank_s, 16), int(addr_s, 16)


def load_coverage(path: Path) -> dict[tuple[int, int], int]:
    hits: dict[tuple[int, int], int] = {}
    with path.open() as f:
        r = csv.DictReader(f)
        for row in r:
            bank, addr = parse_cpu_addr(row["cpu_address"])
            hits[(bank, addr)] = int(row["hits"])
    return hits


def merge_hits(*maps: dict[tuple[int, int], int]) -> dict[tuple[int, int], int]:
    out: dict[tuple[int, int], int] = defaultdict(int)
    for m in maps:
        for k, v in m.items():
            out[k] += v
    return dict(out)


def scan_bank0_calls(rom: bytes) -> tuple[dict[int, list[tuple[str, int]]], dict[int, list[tuple[str, int]]]]:
    """Scan bank0 $8000-$FFFF for JSR/JSL sites. Returns callers_of[dest] and jsl_callers."""
    jsr_callers: dict[int, list[tuple[str, int]]] = defaultdict(list)
    jsl_callers: dict[tuple[int, int], list[tuple[str, int]]] = defaultdict(list)
    pc = 0x8000
    end = 0x10000
    while pc < end:
        off = lorom_offset(0, pc)
        op = rom[off]
        ln = OPLEN.get(op, 1)
        if op == 0x20 and pc + 3 <= end:  # JSR abs
            dest = rom[off + 1] | (rom[off + 2] << 8)
            jsr_callers[dest].append(("00", pc))
        elif op == 0x22 and pc + 4 <= end:  # JSL long
            dest = rom[off + 1] | (rom[off + 2] << 8)
            bank = rom[off + 3]
            jsl_callers[(bank, dest)].append(("00", pc))
        # Also scan other banks lightly for JSL/JSR to bank0 targets? Done separately.
        pc += ln
    return jsr_callers, jsl_callers


def scan_all_banks_for_calls_to(rom: bytes, targets: set[tuple[int, int]]) -> dict[tuple[int, int], list[str]]:
    """Byte-scan all LoROM banks for JSR/JSL absolute forms pointing at targets."""
    callers: dict[tuple[int, int], list[str]] = defaultdict(list)
    for bank in range(0x20):  # 1MiB / 32K = 32 banks
        base = bank * 0x8000
        chunk = rom[base : base + 0x8000]
        cpu_base = 0x8000
        i = 0
        while i < len(chunk):
            op = chunk[i]
            cpu = cpu_base + i
            if op == 0x20 and i + 2 < len(chunk) and bank == 0:
                dest = chunk[i + 1] | (chunk[i + 2] << 8)
                key = (0, dest)
                if key in targets:
                    callers[key].append(f"00:{cpu:04X}/JSR")
            if op == 0x22 and i + 3 < len(chunk):
                dest = chunk[i + 1] | (chunk[i + 2] << 8)
                db = chunk[i + 3]
                key = (db, dest)
                if key in targets:
                    callers[key].append(f"{bank:02X}:{cpu:04X}/JSL")
            ln = OPLEN.get(op, 1)
            # Avoid getting stuck; if length would leave bank, break
            if i + ln > len(chunk):
                break
            i += ln
    return callers


def estimate_routine(rom: bytes, bank: int, entry: int, max_bytes: int = 0x200) -> dict:
    """Linear-scan from entry until RTS/RTL/RTI/JMP/JML or max_bytes; count nested JSR/JSL."""
    pc = entry
    end_pc = entry
    calls = []
    returns = []
    branches_out = []
    visited = set()
    budget = max_bytes
    while budget > 0 and pc < 0x10000:
        if pc in visited:
            break
        visited.add(pc)
        off = lorom_offset(bank, pc)
        if off >= len(rom):
            break
        op = rom[off]
        ln = OPLEN.get(op, 1)
        end_pc = pc + ln - 1
        if op == 0x60:  # RTS
            returns.append(("RTS", pc))
            break
        if op == 0x6B:  # RTL
            returns.append(("RTL", pc))
            break
        if op == 0x40:  # RTI
            returns.append(("RTI", pc))
            break
        if op == 0x4C and ln == 3:  # JMP abs
            dest = rom[off + 1] | (rom[off + 2] << 8)
            branches_out.append(("JMP", dest))
            # follow same-bank JMP if nearby forward/back within window
            if abs(dest - entry) < max_bytes and dest not in visited:
                pc = dest
                budget -= ln
                continue
            break
        if op == 0x5C and ln == 4:  # JML long
            dest = rom[off + 1] | (rom[off + 2] << 8)
            db = rom[off + 3]
            branches_out.append(("JML", (db, dest)))
            break
        if op == 0x6C or op == 0x7C:  # JMP indirect
            branches_out.append(("JMP_IND", pc))
            break
        if op == 0x20 and ln == 3:
            dest = rom[off + 1] | (rom[off + 2] << 8)
            calls.append(("JSR", f"{bank:02X}:{dest:04X}"))
        if op == 0x22 and ln == 4:
            dest = rom[off + 1] | (rom[off + 2] << 8)
            db = rom[off + 3]
            calls.append(("JSL", f"{db:02X}:{dest:04X}"))
        # relative branches: don't terminate
        pc += ln
        budget -= ln
    size = end_pc - entry + 1
    is_leaf = len(calls) == 0 and any(r[0] in ("RTS", "RTL") for r in returns)
    near_leaf = len(calls) <= 2 and any(r[0] in ("RTS", "RTL") for r in returns)
    return {
        "entry": f"{bank:02X}:{entry:04X}",
        "end": f"{bank:02X}:{end_pc:04X}",
        "approx_size": size,
        "returns": returns,
        "calls": calls,
        "branches_out": branches_out,
        "is_leaf": is_leaf,
        "near_leaf": near_leaf or is_leaf,
    }


def cluster_hits(hits: dict[tuple[int, int], int], gap: int = 8) -> list[dict]:
    """Cluster consecutive executed addresses into routine-ish regions."""
    items = sorted(((b, a), h) for (b, a), h in hits.items())
    if not items:
        return []
    clusters = []
    cur_bank, cur_start = items[0][0]
    cur_end = cur_start
    cur_hits = items[0][1]
    cur_entry_hits = items[0][1]
    for (bank, addr), h in items[1:]:
        if bank == cur_bank and addr <= cur_end + gap:
            cur_end = addr
            cur_hits += h
        else:
            clusters.append({
                "bank": cur_bank,
                "start": cur_start,
                "end": cur_end,
                "hits": cur_hits,
                "entry_hits": cur_entry_hits,
            })
            cur_bank, cur_start, cur_end = bank, addr, addr
            cur_hits = h
            cur_entry_hits = h
    clusters.append({
        "bank": cur_bank,
        "start": cur_start,
        "end": cur_end,
        "hits": cur_hits,
        "entry_hits": cur_entry_hits,
    })
    return clusters


def read_scene_table(rom: bytes, count: int = 16) -> list[tuple[int, int]]:
    off = lorom_offset(0, SCENE_TABLE_BASE)
    out = []
    for i in range(count):
        dest = rom[off + i * 2] | (rom[off + i * 2 + 1] << 8)
        out.append((i, dest))
    return out


def main() -> int:
    rom = ROM_PATH.read_bytes()
    sha = hashlib.sha256(rom).hexdigest()
    if sha != EXPECTED_SHA:
        raise SystemExit(f"ROM hash mismatch: {sha}")

    # Prefer the dedicated scout captures, but use the current checked-in route
    # captures when those exploratory names are absent.
    start_cov = ROOT / "artifacts/scout-start-game/executed-addresses.csv"
    if not start_cov.exists():
        start_cov = ROOT / "artifacts/start-game/executed-addresses.csv"
    battle_cov = ROOT / "artifacts/scout-battle/executed-addresses.csv"
    if not battle_cov.exists():
        battle_cov = ROOT / "artifacts/grok-integration-battle/executed-addresses.csv"
    start_hits = load_coverage(start_cov) if start_cov.exists() else {}
    battle_hits = load_coverage(battle_cov) if battle_cov.exists() else {}
    combined = merge_hits(start_hits, battle_hits)

    # Interpreted-only hits
    interp = {k: v for k, v in combined.items() if not is_native(k[0], k[1])}
    # Also per-route
    interp_start = {k: v for k, v in start_hits.items() if not is_native(k[0], k[1])}
    interp_battle = {k: v for k, v in battle_hits.items() if not is_native(k[0], k[1])}

    # Rank individual PC hits
    top_pcs = sorted(interp.items(), key=lambda kv: -kv[1])[:80]
    clusters = sorted(cluster_hits(interp), key=lambda c: -c["hits"])[:60]
    clusters_battle = sorted(cluster_hits(interp_battle), key=lambda c: -c["hits"])[:40]

    scene_table = read_scene_table(rom, 16)

    # Build candidate entries: native callees, scene table, hot cluster starts, gaps after natives
    candidates: list[dict] = []

    def add_candidate(bank, addr, reason, priority_boost=0):
        key = (bank, addr)
        if is_native(bank, addr):
            return
        est = estimate_routine(rom, bank, addr)
        h = combined.get(key, 0)
        # sum hits in estimated range
        range_hits = 0
        end_addr = int(est["end"].split(":")[1], 16)
        for a in range(addr, min(end_addr + 1, addr + est["approx_size"])):
            range_hits += combined.get((bank, a), 0)
        candidates.append({
            "bank": bank,
            "addr": addr,
            "label": f"{bank:02X}:{addr:04X}",
            "reason": reason,
            "priority_boost": priority_boost,
            "hits_entry": h,
            "hits_range": range_hits,
            "estimate": est,
        })

    for bank_s, addr, why in NATIVE_CALLEES:
        add_candidate(int(bank_s, 16), addr, why, priority_boost=50)

    for idx, dest in scene_table:
        add_candidate(0, dest, f"scene-mode table $8B54[{idx}] (JMP from native 8B51)", priority_boost=40 if idx <= 2 else 25)

    # Prefer contiguous after key natives
    for addr, why, boost in [
        (0x8E76, "falls after native scene pointer build 8E26–8E75; also scene table[1]", 60),
        (0x8B7A, "scene table[0]; first mode handler after HDMA wipe", 58),
        (0x8E96, "scene table[2]", 35),
        (0x9BBF, "falls after native VRAM enqueue producers 9B36–9BBE", 55),
        (0x8996, "already native — skip", 0),  # skip handled by is_native
        (0x89B8, "after WRAM cursor helper 89AC–89B7", 20),
        (0x8AE5, "actor-type table / fallthrough after 8AD6–8AE4", 15),
        (0x877E, "after VRAM CPU copy 8732–877D", 30),
        (0x849C, "after mosaic/window 8471–849B", 25),
        (0x82C7, "gap after RNG 82A3–82C6 before controller", 10),
        (0x800E, "reset continuation after 8000–800D", 15),
        (0x9102, "scene table[3]", 20),
        (0x91B6, "scene table[4]", 18),
        (0x9251, "scene table[5]", 16),
    ]:
        if why.startswith("already"):
            continue
        add_candidate(0, addr, why, priority_boost=boost)

    # Hot cluster starts (interpreted)
    for c in clusters[:25]:
        add_candidate(c["bank"], c["start"], f"hot interpreted cluster {c['bank']:02X}:{c['start']:04X}–{c['end']:04X} ({c['hits']} hits)", priority_boost=10)

    # Dedup by label, keep max boost / merge reasons
    by_label: dict[str, dict] = {}
    for c in candidates:
        lab = c["label"]
        if lab not in by_label:
            by_label[lab] = c
        else:
            old = by_label[lab]
            old["priority_boost"] = max(old["priority_boost"], c["priority_boost"])
            if c["reason"] not in old["reason"]:
                old["reason"] += "; " + c["reason"]
            old["hits_range"] = max(old["hits_range"], c["hits_range"])

    ranked = list(by_label.values())

    def score(c: dict) -> float:
        est = c["estimate"]
        leaf_bonus = 25 if est["is_leaf"] else (12 if est["near_leaf"] else 0)
        size = est["approx_size"]
        # Prefer medium leaves; penalize huge blobs and tiny stubs equally lightly
        if size <= 12:
            size_penalty = 8
        elif size <= 100:
            size_penalty = 0
        elif size <= 200:
            size_penalty = 8
        else:
            size_penalty = 20
        a = c["addr"]
        b = c["bank"]
        pref = 0
        # Task preference order, strongly weighted over raw hit farming
        if b == 0 and 0x8E00 <= a <= 0x90FF:
            pref += 55  # (a) scene setup 8E00+
        if (b == 0 and 0x8B28 <= a <= 0x8DFF) or c["label"] in ("04:85B6", "03:FB8D"):
            pref += 48  # (b) HDMA/queue callers 8B28+
        if b == 0 and 0x8954 <= a < 0x8B28:
            pref += 28  # (c) 8954+
        if b == 0 and 0x9B36 <= a <= 0x9CFF:
            pref += 40  # (d) 9B36+ producers
        # (e) battle-route / hot leaves that are also on the native call chain
        if c["priority_boost"] >= 50:
            pref += 20
        if "hot interpreted" in c["reason"] and pref < 40:
            # demote pure hot clusters unless they gained pref above
            pref -= 15
        hit_term = min(c["hits_range"] / 8000.0, 25)
        return c["priority_boost"] + leaf_bonus - size_penalty + pref + hit_term

    ranked.sort(key=score, reverse=True)

    # Callers for leaf candidates
    target_set = {(c["bank"], c["addr"]) for c in ranked[:40]}
    for bank_s, addr, _ in NATIVE_CALLEES:
        target_set.add((int(bank_s, 16), addr))
    callers_map = scan_all_banks_for_calls_to(rom, target_set)

    # --- Write hot-pcs.json ---
    hot_json = {
        "rom_sha256": sha,
        "method": {
            "routes": [
                {
                    "name": "start-game",
                    "frames": 3000,
                    "inputs": "tests/start-game.inputs",
                    "coverage": "artifacts/scout-start-game/executed-addresses.csv",
                    "note": "Opening to Kame House overworld menu; --headless without --verify",
                },
                {
                    "name": "battle-route",
                    "frames": 1800,
                    "inputs": "research/scout/battle-route.inputs",
                    "checkpoint": "artifacts/scout-start-game/final.dbzstate",
                    "coverage": "artifacts/scout-battle/executed-addresses.csv",
                    "note": "docs/battle-route.md from opening checkpoint; Fly + Up toward first encounter",
                },
            ],
            "native_filter": "Bank $00 ranges from docs/progress.md / native.c dispatch excluded",
            "combined_interpreted_unique_pcs": len(interp),
            "combined_interpreted_hits": sum(interp.values()),
            "start_interpreted_hits": sum(interp_start.values()),
            "battle_interpreted_hits": sum(interp_battle.values()),
        },
        "top_interpreted_pcs": [
            {
                "cpu_address": f"{b:02X}:{a:04X}",
                "hits": h,
                "start_hits": start_hits.get((b, a), 0),
                "battle_hits": battle_hits.get((b, a), 0),
            }
            for (b, a), h in top_pcs
        ],
        "top_interpreted_clusters": [
            {
                "range": f"{c['bank']:02X}:{c['start']:04X}–{c['end']:04X}",
                "hits": c["hits"],
                "size": c["end"] - c["start"] + 1,
            }
            for c in clusters[:40]
        ],
        "battle_route_top_clusters": [
            {
                "range": f"{c['bank']:02X}:{c['start']:04X}–{c['end']:04X}",
                "hits": c["hits"],
            }
            for c in clusters_battle[:25]
        ],
    }
    (OUT / "hot-pcs.json").write_text(json.dumps(hot_json, indent=2) + "\n")

    # --- hot-pcs.md ---
    lines = [
        "# Hot interpreted PCs — scout lane",
        "",
        f"ROM SHA-256 `{sha}` (verified).",
        "",
        "## Routes sampled",
        "",
        "1. **start-game** — 3000 frames, `tests/start-game.inputs` → `artifacts/scout-start-game/`.",
        "2. **battle-route** — 1800 frames from that opening checkpoint using `research/scout/battle-route.inputs` (from `docs/battle-route.md`) → `artifacts/scout-battle/`.",
        "",
        "Coverage counts every visited ROM PC while natives run (visited increments before native/interpret dispatch). Hits below exclude already-native bank `$00` ranges.",
        "",
        f"- Combined interpreted hits: **{sum(interp.values()):,}** across **{len(interp)}** unique PCs",
        f"- Start-game interpreted hits: {sum(interp_start.values()):,}",
        f"- Battle-route interpreted hits: {sum(interp_battle.values()):,}",
        "",
        "## Top 40 interpreted PC hits (combined)",
        "",
        "| Rank | Address | Hits | Start | Battle |",
        "|---:|---|---:|---:|---:|",
    ]
    for i, ((b, a), h) in enumerate(top_pcs[:40], 1):
        lines.append(
            f"| {i} | `{b:02X}:{a:04X}` | {h} | {start_hits.get((b,a),0)} | {battle_hits.get((b,a),0)} |"
        )
    lines += [
        "",
        "## Top interpreted clusters (gap≤8)",
        "",
        "| Rank | Range | Hits | Size |",
        "|---:|---|---:|---:|",
    ]
    for i, c in enumerate(clusters[:30], 1):
        lines.append(
            f"| {i} | `{c['bank']:02X}:{c['start']:04X}–{c['end']:04X}` | {c['hits']} | {c['end']-c['start']+1} |"
        )
    lines += [
        "",
        "## Battle-route-only hot clusters",
        "",
        "| Rank | Range | Hits |",
        "|---:|---|---:|",
    ]
    for i, c in enumerate(clusters_battle[:20], 1):
        lines.append(f"| {i} | `{c['bank']:02X}:{c['start']:04X}–{c['end']:04X}` | {c['hits']} |")
    lines.append("")
    (OUT / "hot-pcs.md").write_text("\n".join(lines))

    # --- call-graph-leaves.md ---
    leaf_lines = [
        "# Call-graph leaves / near-leaves — scout lane",
        "",
        "Leaf ≈ no nested JSR/JSL before RTS/RTL on a linear scan. Near-leaf ≤ 2 callees.",
        "Sizes are approximate (M/X=8 length table; REP/SEP can skew a few bytes).",
        "",
        "## Callees of already-native regions (not yet native)",
        "",
    ]
    for bank_s, addr, why in NATIVE_CALLEES:
        bank = int(bank_s, 16)
        est = estimate_routine(rom, bank, addr)
        cl = callers_map.get((bank, addr), [])
        leaf_lines += [
            f"### `{bank:02X}:{addr:04X}` — {'LEAF' if est['is_leaf'] else ('NEAR-LEAF' if est['near_leaf'] else 'INTERNAL')}",
            "",
            f"- Range ≈ `{est['entry']}–{est['end']}` (~{est['approx_size']} bytes)",
            f"- Returns: {est['returns']}",
            f"- Nested calls: {est['calls'] or 'none'}",
            f"- Why: {why}",
            f"- Static callers found: {', '.join(cl[:12]) or '(see native JSL site)'}",
            f"- Replay hits at entry: {combined.get((bank, addr), 0)}; range≈{sum(combined.get((bank,a),0) for a in range(addr, addr+est['approx_size']))}",
            "",
        ]

    leaf_lines += [
        "## Scene dispatch table `$00:8B54` (from native `8B51`)",
        "",
        "Native HDMA/queue wipe ends in `JMP ($8B54,X)` with X = (DP+$24 & $7F) * 2.",
        "",
        "| Index | Target | Approx size | Leaf? | Nested calls | Hot hits (range) |",
        "|---:|---|---:|:---:|---|---:|",
    ]
    for idx, dest in scene_table:
        est = estimate_routine(rom, 0, dest)
        rh = sum(combined.get((0, a), 0) for a in range(dest, dest + est["approx_size"]))
        leaf_lines.append(
            f"| {idx} | `00:{dest:04X}` | {est['approx_size']} | "
            f"{'Y' if est['is_leaf'] else ('~' if est['near_leaf'] else 'n')} | "
            f"{', '.join(c[1] for c in est['calls'][:4]) or '—'} | {rh} |"
        )

    leaf_lines += [
        "",
        "## Notable bank `$00` leaves / near-leaves near hot PCs & native frontiers",
        "",
    ]

    # Pick good leaf candidates from ranked
    shown = 0
    for c in ranked:
        est = c["estimate"]
        if not est["near_leaf"] and c["priority_boost"] < 30:
            continue
        if shown >= 25:
            break
        cl = callers_map.get((c["bank"], c["addr"]), [])
        leaf_lines += [
            f"### `{c['label']}` — {'LEAF' if est['is_leaf'] else ('NEAR-LEAF' if est['near_leaf'] else 'CANDIDATE')}",
            "",
            f"- Range ≈ `{est['entry']}–{est['end']}` (~{est['approx_size']} bytes)",
            f"- Nested: {est['calls'] or 'none'}; exits: {est['returns'] or est['branches_out']}",
            f"- Callers (static sample): {', '.join(cl[:10]) or 'n/a'}",
            f"- Rationale: {c['reason']}",
            f"- Hits entry/range: {c['hits_entry']}/{c['hits_range']}",
            "",
        ]
        shown += 1

    leaf_lines += [
        "## Good next-native heuristics",
        "",
        "1. **`00:8E76` / `00:8B7A`** — immediate scene-mode handlers after already-native `8B28`/`8E26`.",
        "2. **`00:90C1`** — direct JSL from native scene setup `8E14`; finishes the `8DFE` chain.",
        "3. **`00:9BBF+`** — producer-side continuation after enqueue `9B36`/`9B79`.",
        "4. **`04:85B6` / `03:FB8D`** — mandatory callees of native HDMA wipe; unblock full `8B28` ownership.",
        "5. Prefer leaves ≤ ~80 bytes with known JSR/JSL from natives for differential tests.",
        "",
    ]
    (OUT / "call-graph-leaves.md").write_text("\n".join(leaf_lines))

    # --- priority-queue.md ---
    pq = [
        "# Priority queue — next reconstruction targets (scout)",
        "",
        "Ranked for the reconstruct lane. Prefer: (a) scene setup `8E00+`, (b) HDMA/queue callers `8B28+`,",
        "(c) `8954+`, (d) `9B36+` producers, (e) battle-route hot leaves.",
        "Already-native ranges excluded. Sizes approximate.",
        "",
        "| Rank | Target | ~Size | Leaf? | One-line rationale |",
        "|---:|---|---:|:---:|---|",
    ]
    for i, c in enumerate(ranked[:20], 1):
        est = c["estimate"]
        leaf = "Y" if est["is_leaf"] else ("~" if est["near_leaf"] else "n")
        rationale = c["reason"].split(";")[0].strip()
        if c["hits_range"]:
            rationale += f" (replay range hits≈{c['hits_range']})"
        pq.append(
            f"| {i} | `{c['label']}` | {est['approx_size']} | {leaf} | {rationale} |"
        )
    pq += [
        "",
        "## Top 10 quick list",
        "",
    ]
    for i, c in enumerate(ranked[:10], 1):
        pq.append(f"{i}. `{c['label']}` — {c['reason'].split(';')[0].strip()}")
    pq += [
        "",
        "## Preference mapping",
        "",
        "| Preference | Targets in top 20 |",
        "|---|---|",
    ]
    prefs = {
        "(a) scene setup 8E00+": [c for c in ranked[:20] if c["bank"] == 0 and 0x8E00 <= c["addr"] < 0x9100],
        "(b) HDMA/queue 8B28+": [c for c in ranked[:20] if (c["bank"] == 0 and 0x8B28 <= c["addr"] < 0x8E00) or c["label"] in ("04:85B6", "03:FB8D")],
        "(c) 8954+ palette/WRAM": [c for c in ranked[:20] if c["bank"] == 0 and 0x8954 <= c["addr"] < 0x8B28],
        "(d) 9B36+ producers": [c for c in ranked[:20] if c["bank"] == 0 and 0x9B36 <= c["addr"] < 0xA000],
        "(e) battle/hot call-chain leaves": [c for c in ranked[:20] if c["label"] in ("00:C559", "00:C68C", "00:849C", "00:85EF", "00:8FE5") or c["hits_range"] > 100000],
    }
    for name, items in prefs.items():
        pq.append(f"| {name} | {', '.join('`'+x['label']+'`' for x in items) or '—'} |")
    pq += [
        "",
        "## Notes for reconstruct lane",
        "",
        "- Best first picks: **`04:85B6`** (tiny leaf, many callers incl. native `8B3E`), then **`00:8E76`→`00:8D2B`** (scene table[1] abutting native `8E75`).",
        "- Close native `8DFE` chain with **`00:90C1`** (pulls hot **`00:C559`/`00:C68C`**) and later **`06:F14D`.",
        "- HDMA wipe still JSLs **`03:FB8D`**; scene table[0] **`00:8B7A`** is larger—tackle after small leaves.",
        "- Producer frontier: **`00:9BBF`** after native `9BBE`.",
        "- Regen: `python3 research/scout/analyze_scout.py` after new `artifacts/scout-*/executed-addresses.csv`.",
        "",
    ]
    (OUT / "priority-queue.md").write_text("\n".join(pq))

    # Print summary for parent
    summary = {
        "written": [
            str(OUT / "hot-pcs.json"),
            str(OUT / "hot-pcs.md"),
            str(OUT / "call-graph-leaves.md"),
            str(OUT / "priority-queue.md"),
            str(OUT / "analyze_scout.py"),
            str(OUT / "battle-route.inputs"),
        ],
        "top10": [c["label"] + " — " + c["reason"].split(";")[0].strip() for c in ranked[:10]],
    }
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

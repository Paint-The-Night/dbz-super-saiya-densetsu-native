# Text / dialogue API (Super Saiya Densetsu)

Stable C surface over recovered bank-`$01` dialogue leaves. JA keeps
bit-identical behavior vs lakesnes. EN (and later fan tables) consume this API
— no IPS/BPS, no patched ROM bytes.

## Language

| Call | Role |
|------|------|
| `dbz_i18n_set` / `dbz_i18n_get` | Boot menu language (`src/i18n.*`) |
| `dbz_i18n_en_script(bank_sel, idx, &len)` | Raw EN glyph bytes (Klepto-compatible) |
| `dbz_i18n_filter_script_byte` | Delegates to `dbz_text_filter_script_byte` |

Changing language clears any active EN stream (`dbz_text_unbind`).

## Message bind + pointer-swap

| Call | Role |
|------|------|
| `dbz_text_pointer_swap_after_resolve(cpu)` | After `01:CF3A` stores the JP string long at DP+`$00`: if `lang==EN` and a C table hits `($0733,$0723)`, copy bytes into WRAM overlay and rewrite DP+`$00`/`$02` |
| `dbz_text_unbind` | Clear active stream |
| `dbz_text_active_stream` | Snapshot (`active`, `bank_sel`, `str_idx`, `overlay_addr24`, `len`) |
| `dbz_text_filter_script_byte` | Fetch filter extension point (physical swap already feeds EN via WRAM) |

### Overlay contract

| Item | Value |
|------|-------|
| Bank / offset | `$7E:E000` (`DBZ_TEXT_EN_OVERLAY_*`) |
| Max length | `$0800` bytes |
| JA | Never written |
| Evidence | Host scratch — high `$7E` unused in `docs/ram-map.md`; not a recovered game buffer |

## Native leaves

### `text_script_cf3a_step` — `01:CF3A–CFA0`

Message resolve + text cursor setup. **JSL** from `01:CCC4`, `01:CDAE`, `01:CE6D`.

| Step | Behavior |
|------|----------|
| Inputs | `$0733` (far-table index), `$0723` (string index), `$071D/$071E/$071F/$0721/$0731` (cursor) |
| Far table | `$0733×3` → `LDA $078F16,X` (24-bit entries); index **2** = `06:8000` |
| Pointer | `$0723` ASL → Y; `LDA [$00],Y` → DP+`$00` (string offset); bank stays in DP+`$02` |
| EN hook | Immediately after `STA $00` (string ptr): `dbz_text_pointer_swap_after_resolve` |
| Cursor | WRMPY `$071E×$80` → DP+`$03`; Y ← `$071D`; DP+`$20` glyph index |
| Exit | RTL at `$CFA0`; BMI on `$0731` may fall into `01:CFA1` helper |

Do **not** confuse with bank-`$05` actor ops at the same PC numbers (`actor_op_cf08_step` / `actor_op_cf60_step`).

### `text_script_cfb2_step` — `01:CFB2–CFC3`

Script fetch leaf.

| Step | Behavior |
|------|----------|
| `LDX $20` | Glyph buffer index |
| `LDA [$00],Y` | Via `dbz_i18n_filter_script_byte` |
| `STA $04` / `INY` / `CMP #$01` | Gap → `JSL $01D00B`; else fall to `$CFC4` dispatch |

With EN pointer-swap, `[$00]` points at the overlay, so different-length streams walk correctly. JA leaves DP on the ROM string (bit-identical).

### Related (not yet full text-API wrappers)

| PC | Notes |
|----|-------|
| `01:D000` / `D00B` | Glyph word → `$7E3000,X`; gap/cursor INC (native `actor_timer_d00b_step`) |
| `01:CE2F` | `LDA [$00],Y` peek → `$072E` (sees overlay after swap) |
| `01:D027` | Line-scan `LDA [$00],Y` (same) |
| `01:CC21` | `STZ $071D/$071E/$071F` on message open |

## Stream encoding

- Custom 1-byte glyph indices (not ASCII / Shift-JIS).
- `F4 <face> 7E` — open box / speaker.
- `FF` line, `FE` page, `7F` end box, `01` gap (`CFB9` path).
- High bytes (`$A0+`, …) take multi-byte / table expand paths in `$CFC4+`.

## Font (EN) — contract

| Item | Value |
|------|-------|
| JP upload leaf | `00:90C1`: DP+`$83/$85` = `$08:8000` → `C559` decompress → `$7E9000` → `C68C` rearrange → DMA (`90E6`) to VRAM word **`$2000`** |
| Klepto ref tiles | 2bpp SNES, **`$1800`** bytes (384 tiles) at CPU `$91:8036` / file `$088036` (JP lead-in was `$FF`; gate stub at `$10:A000` copies when `$83:$85==$08:8000`) — **data source only** |
| Native API | `dbz_text_en_font_ensure(cpu)` / `dbz_text_en_font_ready()` / `dbz_text_en_font_reset()` |
| EN behavior | On `00:90C1` RTL, ensure installs tiles into `$7E9000` + PPU `vram[$2000..]` and sets DP+`$89=$1800`. Idempotent while `lang==EN`. |
| JA | Ensure is a no-op; never writes VRAM / `$7E9000` |
| Glyph indices | Script bytes map 1:1 to 2bpp tile numbers (attr `#$24` in `$7E3000`); Latin lowercase starts ~`$05`=`a` |
| Data file | `src/text_en_font.inc` (extracted from Klepto IPS offline; not applied at runtime) |

## EN script table coverage

### Main dialogue — `$0733==2` / `06:8000`

| Item | Value |
|------|-------|
| Pointers | 366 u16 at `06:8000` |
| Covered EN indices | **366 / 366** (`DBZ_EN_MAIN_SCRIPT_COUNT`; 346 unique dests) |
| Aliases 23–42 | Same dest as **43** in JP *and* Klepto (unused slots, not missing lines). Table entries share `EN_SCRIPT_23`. |
| Index 365 | Last dest. Klepto packed earlier strings to `06:C64F`; a naïve end at file `$35108` is **2745** leftover JP bytes, not one line. Length is a **7F-not-FE walk → 90 bytes** (3 boxes, matches JP shape). Fits `$0800`. |
| Overlay | Keep `$7E:E000` / `$0800`. Max main blob 171 B; 365 is 90 B. ROM has no `00 E0 7E` long; high `$7E` unused in `docs/ram-map.md`. No split / no larger window. |
| Data | `src/text_en_scripts.inc` |

### Menu / battle — far table `07:8F16` sels 0/1/3–7

Bank-`$07` pointer tables are **packed sequentially** `07:8F2E` … `07:93AE` (first string of sel 0). Counts = span/2. Same 1-byte Klepto glyph encoding (often `FF FF` / `FD FF FF`, no `F4` face). Same pointer-swap overlay.

| `$0733` | Table | EN hits | Notes |
|---------|--------|---------|-------|
| 0 | `07:8F2E` | **42 / 42** | Menu / status |
| 1 | `07:8F82` | **338 / 338** | Battle / command |
| 3 | `07:9226` | **167 / 167** | Battle / event |
| 4 | `07:9374` | **9 / 9** | Short battle set |
| 5 | `07:9386` | **2 / 2** | Alias set |
| 6 | `07:938A` | **2 / 2** | Alias set |
| 7 | `07:938E` | **9 / 16** | Trailing 7 point at `93AE` (sel 0 string 0) — unused, skipped |
| **Banks total** | | **569** | `DBZ_EN_BANK_SCRIPT_COUNT`; max blob 147 B |

Data: `src/text_en_banks.inc` (parallel module; `dbz_i18n_en_script` searches it after main).

**Combined: 366 + 569 = 935 EN streams.** Misses: 7 unused sel-7 slots (JA also unused).

## Tooling

`tools/extract_script_strings.py` — dump JP pointers/strings; `--klepto` aligns EN
reference bytes by index. Lengths are **dest-based** (next higher unique dest;
aliases share bytes). Last dest walks 7F-not-FE (dialogue) or `FF FF`/`FD FF FF`
(menu). Do not use file `$35108` as a Klepto last-string end.

Regenerate (requires pinned Rev 1 ROM + Klepto IPS under
`research/klepto-reference/`):

```sh
python3 tools/extract_script_strings.py --klepto \
  --c-inc src/text_en_scripts.inc \
  --c-inc-banks src/text_en_banks.inc
```

Do not hand-edit `text_en_scripts.inc` or `text_en_banks.inc`. Product path never applies IPS.

## Files

| File | Role |
|------|------|
| `src/text_script.h` / `.c` | Bind / overlay / filter / EN font API |
| `src/text_en_font.inc` | Embedded 2bpp EN tileset (`$1800`) |
| `src/text_en_scripts.inc` | Generated EN main-script glyph tables (366 entries / 346 unique) |
| `src/text_en_banks.inc` | Generated EN menu/battle tables (569 entries) |
| `src/i18n.h` / `.c` | Language + `dbz_i18n_en_script` API (includes both tables) |
| `src/native_video.inc` | `text_script_cf3a_step`, `text_script_cfb2_step` |
| `docs/i18n.md` | Policy + ship state |

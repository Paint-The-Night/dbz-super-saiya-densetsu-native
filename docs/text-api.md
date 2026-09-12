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

## Tooling

`tools/extract_script_strings.py` — dump JP pointers/strings; `--klepto` aligns EN reference bytes by index.

## Files

| File | Role |
|------|------|
| `src/text_script.h` / `.c` | Bind / overlay / filter / EN font API |
| `src/text_en_font.inc` | Embedded 2bpp EN tileset (`$1800`) |
| `src/i18n.h` / `.c` | Language + EN script tables |
| `src/native_video.inc` | `text_script_cf3a_step`, `text_script_cfb2_step` |
| `docs/i18n.md` | Policy + ship state |

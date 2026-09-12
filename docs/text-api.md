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

## Font (EN) — research status

| Item | Status |
|------|--------|
| JP font | Compressed graphics (RHDN / LotSS); not in C yet |
| Klepto EN font | IPS cluster ~file `$088000` (CPU bank `$91`, JP=`FF` empty) — **reference only** |
| Upload | Likely via `00:8732` VRAM CPU copy / `00:0800` DMA queue after `$7E3000` decode |
| This pass | Documented; no upload leaf hooked. Stub: when a clear `lang==EN` font-upload call site is nativized, load tiles into bank `$91` overlay or VRAM without ROM mutation |

## Tooling

`tools/extract_script_strings.py` — dump JP pointers/strings; `--klepto` aligns EN reference bytes by index.

## Files

| File | Role |
|------|------|
| `src/text_script.h` / `.c` | Bind / overlay / filter API |
| `src/i18n.h` / `.c` | Language + EN script tables |
| `src/native_video.inc` | `text_script_cf3a_step`, `text_script_cfb2_step` |
| `docs/i18n.md` | Policy + ship state |

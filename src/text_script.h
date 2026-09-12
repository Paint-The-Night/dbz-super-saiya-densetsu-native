#ifndef DBZ_TEXT_SCRIPT_H
#define DBZ_TEXT_SCRIPT_H

/*
 * Dialogue / text-script API (Super Saiya Densetsu).
 *
 * Stable surface over recovered bank-$01 leaves so JA stays bit-identical and
 * EN (or future fan tables) consume the same contracts without ROM patches.
 *
 * Pipeline (evidence — see docs/text-api.md):
 *   01:CF3A  message resolve ($0733 ×3 → 07:8F16 far bank; $0723 → u16 ptr)
 *            → DP+$00 long = string start; cursor math → DP+$20; Y←$071D
 *   01:CFB2  fetch LDA [$00],Y → $04; glyph path → $7E3000,X (01:D000/D00B)
 *   00:90C1  font/tileset: decompress $08:8000 → $7E9000, DMA VRAM $2000
 *
 * EN pointer-swap: after resolve, dbz_text_pointer_swap_after_resolve may
 * retarget DP+$00 to a host WRAM overlay holding C-table glyph bytes.
 * EN font: dbz_text_en_font_ensure installs Klepto-ref 2bpp tiles via the
 * same VRAM $2000 path (hooked at 00:90C1 RTL).
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Host WRAM scratch for EN streams. Not a recovered game buffer — high $7E
 * ($E000+) is unused in docs/ram-map; JA never writes here. */
#define DBZ_TEXT_EN_OVERLAY_BANK 0x7Eu
#define DBZ_TEXT_EN_OVERLAY_OFF  0xE000u
#define DBZ_TEXT_EN_OVERLAY_MAX  0x0800u /* enough: main max 171, banks max 147, idx 365 = 90 */

#define DBZ_TEXT_MAIN_BANK_SEL   2u /* $0733 index → 06:8000 at 07:8F16 */

/* EN 2bpp font (Klepto ref $91:8036) — size / VRAM word dest. */
#define DBZ_TEXT_EN_FONT_SIZE       0x1800u
#define DBZ_TEXT_EN_FONT_VRAM_WORD  0x2000u

typedef struct {
  bool active;
  uint8_t bank_sel;
  uint16_t str_idx;
  uint32_t overlay_addr24; /* bank<<16 | off */
  size_t len;
} DbzTextStream;

/* Clear any EN bind (also called from dbz_i18n_set when leaving EN). */
void dbz_text_unbind(void);

/* After 01:CF3A has stored the JP string long at DP+$00: if lang==EN and a
 * C table hits (bank_sel,str_idx), copy bytes into the WRAM overlay and
 * rewrite DP+$00/$02 so subsequent LDA [$00],Y walks EN. JA: no-op. */
void dbz_text_pointer_swap_after_resolve(Cpu *c);

/* Active stream snapshot (NULL fields when inactive). */
const DbzTextStream *dbz_text_active_stream(void);

/* Fetch filter used by 01:CFB4. JA: identity. EN+overlay: if addr falls in
 * the active overlay window, return overlay ROM/WRAM byte (already installed);
 * otherwise rom_byte. Physical pointer-swap means the CPU read already sees
 * EN — this remains the extension point for logical overlays. */
uint8_t dbz_text_filter_script_byte(uint32_t addr24, uint8_t rom_byte);

/* EN font (2bpp Latin tileset). Ready after a successful ensure while lang==EN.
 * ensure: copies tiles into $7E9000 + PPU VRAM[$2000..] (00:90C1 DMA dest).
 * reset: clear ready (called on language change). JA paths never upload. */
bool dbz_text_en_font_ready(void);
void dbz_text_en_font_ensure(Cpu *c);
void dbz_text_en_font_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* DBZ_TEXT_SCRIPT_H */

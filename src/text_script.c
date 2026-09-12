#include "text_script.h"
#include "i18n.h"
#include "snes.h"

#include <string.h>

#include "text_en_font.inc"
#include "text_en_intro.inc"

static DbzTextStream g_stream;
static bool g_en_font_ready;
static bool g_en_intro_ready;

void dbz_text_unbind(void) {
  memset(&g_stream, 0, sizeof g_stream);
}

const DbzTextStream *dbz_text_active_stream(void) {
  return &g_stream;
}

static uint16_t read_u16(Cpu *c, uint32_t addr24) {
  uint8_t lo = c->read(c->mem, addr24);
  uint8_t hi = c->read(c->mem, (addr24 + 1u) & 0xffffffu);
  return (uint16_t)(lo | ((uint16_t)hi << 8));
}

static void write_dp_long(Cpu *c, uint32_t addr24) {
  uint16_t dp0 = (uint16_t)(c->dp + 0x00);
  c->write(c->mem, dp0, (uint8_t)addr24);
  c->write(c->mem, (uint16_t)(dp0 + 1), (uint8_t)(addr24 >> 8));
  c->write(c->mem, (uint16_t)(dp0 + 2), (uint8_t)(addr24 >> 16));
}

void dbz_text_pointer_swap_after_resolve(Cpu *c) {
  if(!c || !c->read || !c->write) return;
  if(dbz_i18n_get() != DBZ_LANG_EN) {
    dbz_text_unbind();
    return;
  }

  uint8_t bank_sel = c->read(c->mem, 0x000733u);
  uint16_t str_idx = read_u16(c, 0x000723u);
  size_t len = 0;
  const uint8_t *bytes = dbz_i18n_en_script(bank_sel, str_idx, &len);
  if(!bytes || len == 0 || len > DBZ_TEXT_EN_OVERLAY_MAX) {
    dbz_text_unbind();
    return;
  }

  uint32_t base = ((uint32_t)DBZ_TEXT_EN_OVERLAY_BANK << 16) | DBZ_TEXT_EN_OVERLAY_OFF;
  for(size_t i = 0; i < len; i++) {
    c->write(c->mem, (base + (uint32_t)i) & 0xffffffu, bytes[i]);
  }
  write_dp_long(c, base);

  g_stream.active = true;
  g_stream.bank_sel = bank_sel;
  g_stream.str_idx = str_idx;
  g_stream.overlay_addr24 = base;
  g_stream.len = len;
}

uint8_t dbz_text_filter_script_byte(uint32_t addr24, uint8_t rom_byte) {
  /* Physical pointer-swap installs EN into WRAM; CPU read already returns it.
   * Keep this as the stable filter entry for future logical overlays. */
  if(dbz_i18n_get() != DBZ_LANG_EN || !g_stream.active) return rom_byte;
  uint32_t base = g_stream.overlay_addr24;
  if(addr24 >= base && (addr24 - base) < g_stream.len) return rom_byte;
  return rom_byte;
}

void dbz_text_en_font_reset(void) {
  g_en_font_ready = false;
  g_en_intro_ready = false;
}

void dbz_text_en_intro_reset(void) {
  g_en_intro_ready = false;
}

bool dbz_text_en_font_ready(void) {
  return g_en_font_ready && dbz_i18n_get() == DBZ_LANG_EN;
}

/*
 * EN font upload — same contract as Klepto gate at $10:A000 / raw tiles at
 * $91:8036: when scene leaf 00:90C1 would decompress $08:8000 → $7E9000 and
 * DMA to VRAM word $2000, install the 2bpp Latin tileset instead.
 *
 * Evidence:
 *   - 00:90C1 seeds DP+$83/$85 = $08:8000, C559/C68C, DMA via 90E6 to $2116=$2000
 *   - Klepto raw copy $1800 bytes from $11:8036 → [$86] ($7E9000) when those DP
 *     values match; tiles are already SNES 2bpp (readable Latin at glyph indices)
 *   - Dialogue glyph indices land in $7E3000 tilemap with attr #$24 against this
 *     BG character base
 *
 * JA: no-op (never touches VRAM / $7E9000). No ROM writes.
 */
void dbz_text_en_font_ensure(Cpu *c) {
  if(!c || !c->mem || !c->write) return;
  if(dbz_i18n_get() != DBZ_LANG_EN) {
    g_en_font_ready = false;
    return;
  }
  if(g_en_font_ready) return;

  Snes *snes = (Snes *)c->mem;
  if(!snes->ppu) return;

  /* Mirror into decompress buffer (post-rearrange layout = Klepto raw). */
  for(uint32_t i = 0; i < DBZ_TEXT_EN_FONT_SIZE; i++) {
    c->write(c->mem, (0x7e9000u + i) & 0xffffffu, DBZ_TEXT_EN_FONT_2BPP[i]);
  }
  /* DP+$89 = DMA length (90E6 reads it); keep in sync if caller re-DMAs. */
  c->write(c->mem, (uint16_t)(c->dp + 0x89), (uint8_t)(DBZ_TEXT_EN_FONT_SIZE & 0xffu));
  c->write(c->mem, (uint16_t)(c->dp + 0x8a), (uint8_t)(DBZ_TEXT_EN_FONT_SIZE >> 8));

  /* Direct VRAM install at the 90C1 destination (word address $2000). */
  const uint32_t words = DBZ_TEXT_EN_FONT_SIZE / 2u;
  for(uint32_t i = 0; i < words; i++) {
    uint16_t w = (uint16_t)DBZ_TEXT_EN_FONT_2BPP[i * 2u] |
                 ((uint16_t)DBZ_TEXT_EN_FONT_2BPP[i * 2u + 1u] << 8);
    snes->ppu->vram[(DBZ_TEXT_EN_FONT_VRAM_WORD + i) & 0x7fffu] = w;
  }

  g_en_font_ready = true;
}

/*
 * Opening crawl EN — BG1/BG2 tilemaps at VRAM $6000/$6800 use Latin tile
 * indices (0xDA+) against CHR at $2000. Same scene keeps one static nametable
 * and scrolls it; CF3A never runs ($0733 stays 0). Data from Klepto 1.02
 * offline extract (text_en_intro.inc).
 */
void dbz_text_en_intro_ensure(Cpu *c) {
  if(!c || !c->mem) return;
  if(dbz_i18n_get() != DBZ_LANG_EN) {
    g_en_intro_ready = false;
    return;
  }
  Snes *snes = (Snes *)c->mem;
  if(!snes->ppu) return;

  /* Intro signature: BG3 nametable $7000, CHR base $4000 (speed-line layer). */
  BgLayer *bg3 = &snes->ppu->bgLayer[2];
  if(bg3->tilemapAdr != 0x7000u || bg3->tileAdr != 0x4000u) {
    g_en_intro_ready = false;
    return;
  }

  /* Fingerprint JP crawl page — only swap when that nametable is resident. */
  uint64_t h = 14695981039346656037ull;
  for(uint32_t i = 0; i < 0x800u; i++) {
    uint16_t w = snes->ppu->vram[(0x6000u + i) & 0x7fffu];
    h ^= (uint8_t)w; h *= 1099511628211ull;
    h ^= (uint8_t)(w >> 8); h *= 1099511628211ull;
  }
  /* Arm on JP crawl nametable; keep refreshing while intro stays active
   * (JP DMA can rewrite BG1 mid-scene — re-apply EN without waiting for FNV). */
  if(h != DBZ_TEXT_EN_INTRO_JP_BG1_FNV && !g_en_intro_ready)
    return;

  for(uint32_t i = 0; i < DBZ_TEXT_EN_INTRO_CHR_BYTES / 2u; i++) {
    uint16_t w = (uint16_t)DBZ_TEXT_EN_INTRO_CHR[i * 2u] |
                 ((uint16_t)DBZ_TEXT_EN_INTRO_CHR[i * 2u + 1u] << 8);
    snes->ppu->vram[(0x2000u + i) & 0x7fffu] = w;
  }
  for(uint32_t i = 0; i < DBZ_TEXT_EN_INTRO_BG_BYTES / 2u; i++) {
    uint16_t w1 = (uint16_t)DBZ_TEXT_EN_INTRO_BG1[i * 2u] |
                  ((uint16_t)DBZ_TEXT_EN_INTRO_BG1[i * 2u + 1u] << 8);
    uint16_t w2 = (uint16_t)DBZ_TEXT_EN_INTRO_BG2[i * 2u] |
                  ((uint16_t)DBZ_TEXT_EN_INTRO_BG2[i * 2u + 1u] << 8);
    snes->ppu->vram[(0x6000u + i) & 0x7fffu] = w1;
    snes->ppu->vram[(0x6800u + i) & 0x7fffu] = w2;
  }
  /* Intro CHR is not the dialogue Latin set — force dialogue ensure to re-run later. */
  g_en_font_ready = false;
  g_en_intro_ready = true;
}

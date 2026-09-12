#include "text_script.h"
#include "i18n.h"

#include <string.h>

static DbzTextStream g_stream;

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

bool dbz_text_en_font_ready(void) {
  /* Klepto ref places EN tiles in empty bank $91 (file $088000). No extract
   * or VRAM upload leaf is hooked yet — keep false so callers use JP font. */
  return false;
}

void dbz_text_en_font_ensure(Cpu *c) {
  (void)c;
  /* TODO: when lang==EN and a clear upload leaf exists, overlay bank $91
   * tiles or enqueue VRAM via 00:8732 / $0800 DMA — never patch ROM. */
}

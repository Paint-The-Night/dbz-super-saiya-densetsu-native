#include "menu_ext.h"
#include "i18n.h"
#include "text_script.h"

#include <string.h>

/* LoROM bank $02 file offsets. */
#define DBZ_LOROM2(addr) (0x10000u + ((uint32_t)(addr) & 0x7FFFu))

#define DBZ_OFF_E232_MODE1_MAX   DBZ_LOROM2(0xE235u) /* mode1 byte0 of $02:E232 */
#define DBZ_OFF_E0F6_MODE1_W     DBZ_LOROM2(0xE0F6u)
#define DBZ_OFF_E0F6_MODE1_H     DBZ_LOROM2(0xE0F7u)
#define DBZ_OFF_DA18_PTR_LO      DBZ_LOROM2(0xDA18u)
#define DBZ_OFF_DA18_PTR_HI      DBZ_LOROM2(0xDA19u)

/* Unused $FF in bank $02 — host descriptor landing pad. */
#define DBZ_PARTY_DESC_ADDR      0xF500u
#define DBZ_OFF_PARTY_DESC       DBZ_LOROM2(DBZ_PARTY_DESC_ADDR)

/*
 * Packed menu descriptor (00:9C91):
 *   +0 width_param (tiles-1 via DEC/BPL), +1 row_param (rows-1),
 *   +2..3 VRAM word (LE), then (width_param+1) * (row_param+1) tile bytes.
 * Party VRAM base $04D8 unchanged; 8 tiles/row, 6 rows.
 */
#define DBZ_PARTY_TILES_PER_ROW  8u
#define DBZ_PARTY_ROWS           6u
#define DBZ_PARTY_DESC_LEN       (4u + DBZ_PARTY_TILES_PER_ROW * DBZ_PARTY_ROWS)

/* EN tiles: a=0x05…; capitals C=E9 S=14 T=47 O=5A; mid r=52 s=53 t=54…;
 * vowels/letters o=1B n=1A l=18 (from Talk/Look/Menu evidence). */
static const uint8_t PARTY_DESC_EN[DBZ_PARTY_DESC_LEN] = {
  0x07, 0x05, 0xD8, 0x04,
  /* Cards   */ 0xE9, 0x05, 0x52, 0x0A, 0x53, 0x01, 0x01, 0x01,
  /* Status  */ 0x14, 0x54, 0x05, 0x54, 0x55, 0x53, 0x01, 0x01,
  /* Order   */ 0x5A, 0x52, 0x0A, 0x0C, 0x52, 0x01, 0x01, 0x01,
  /* Text    */ 0x47, 0x0C, 0x58, 0x54, 0x01, 0x01, 0x01, 0x01,
  /* Save    */ 0x14, 0x05, 0x56, 0x0C, 0x01, 0x01, 0x01, 0x01,
  /* Controls*/ 0xE9, 0x1B, 0x1A, 0x54, 0x52, 0x1B, 0x18, 0x53,
};

/*
 * JA rows 0–4: exact Rev 1 $02:DACE payload (6 tiles) + pad $01×2.
 * Row 5 「ソウサ」: ソ=0x52 ウ=0x06 (ひこう) サ=0x4E (セ=0x51 gojuon).
 */
static const uint8_t PARTY_DESC_JA[DBZ_PARTY_DESC_LEN] = {
  0x07, 0x05, 0xD8, 0x04,
  /* カード等 — ROM DACE+4 */ 0xCD, 0x57, 0x6C, 0x49, 0x43, 0xC7, 0x01, 0x01,
  /* status */ 0x1C, 0x06, 0x2B, 0x3A, 0x0B, 0x01, 0x01, 0x01,
  /* order  */ 0x18, 0x2A, 0xB5, 0x09, 0x07, 0x01, 0x01, 0x01,
  /* text   */ 0x50, 0xD8, 0x43, 0xC7, 0x01, 0x01, 0x01, 0x01,
  /* save   */ 0x51, 0x43, 0xCF, 0x01, 0x01, 0x01, 0x01, 0x01,
  /* ソウサ (う=0x06 from ひこう) */ 0x52, 0x06, 0x4E, 0x01, 0x01, 0x01, 0x01, 0x01,
};

static const uint8_t *party_desc_bytes(void) {
  return (dbz_i18n_get() == DBZ_LANG_EN) ? PARTY_DESC_EN : PARTY_DESC_JA;
}

uint8_t dbz_menu_ext_cart_filter(Snes *snes, uint32_t file_off, uint8_t rom_byte) {
  /* Structural: party max + taller/wider window (both languages). */
  if(file_off == DBZ_OFF_E232_MODE1_MAX) return 6u;
  if(file_off == DBZ_OFF_E0F6_MODE1_W) return 0x0Cu;
  if(file_off == DBZ_OFF_E0F6_MODE1_H) return 0x0Eu;

  /* Retarget DA16[1] → $02:F500. */
  if(file_off == DBZ_OFF_DA18_PTR_LO) return (uint8_t)(DBZ_PARTY_DESC_ADDR & 0xFFu);
  if(file_off == DBZ_OFF_DA18_PTR_HI) return (uint8_t)((DBZ_PARTY_DESC_ADDR >> 8) & 0xFFu);

  /* Serve extended descriptor from unused ROM hole. */
  if(file_off >= DBZ_OFF_PARTY_DESC &&
     file_off < DBZ_OFF_PARTY_DESC + DBZ_PARTY_DESC_LEN) {
    return party_desc_bytes()[file_off - DBZ_OFF_PARTY_DESC];
  }

  return dbz_text_en_chrome_cart_filter(snes, file_off, rom_byte);
}

void dbz_menu_ext_install(Snes *snes) {
  if(!snes) return;
  snes->cartRomFilter = dbz_menu_ext_cart_filter;
}

bool dbz_menu_ext_party_open(const Snes *snes) {
  if(!snes) return false;
  return snes->ram[0x0025] == 1u &&
         snes->ram[0x0D66] == 1u &&
         snes->ram[0x0D64] == 6u;
}

bool dbz_menu_ext_controls_selected(const Snes *snes) {
  return dbz_menu_ext_party_open(snes) &&
         snes->ram[0x0D62] == DBZ_MENU_PARTY_CONTROLS_INDEX;
}

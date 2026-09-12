#include "i18n.h"
#include "text_script.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define DBZ_KEEP EMSCRIPTEN_KEEPALIVE
#else
#define DBZ_KEEP
#endif

static DbzLang g_lang = DBZ_LANG_JA;

#if defined(DBZ_I18N_EXPERIMENTAL_HOOKS) && DBZ_I18N_EXPERIMENTAL_HOOKS
uint8_t (*dbz_i18n_remap_script_byte)(uint8_t jp_byte) = 0;
#endif

/* Main-script bank selector ($0733) for far table entry 06:8000 at 07:8F16 index 2. */
#define DBZ_MAIN_SCRIPT_BANK_SEL 2u

typedef struct {
  uint8_t bank_sel;
  uint16_t str_idx;
  const uint8_t *bytes;
  size_t len;
  uint32_t jp_base_addr24; /* JP string start (06:xxxx or 07:xxxx) */
} DbzEnScript;

/*
 * EN glyph tables generated from Klepto 1.02 reference bytes.
 *   text_en_scripts.inc — $0733==2 / 06:8000 main dialogue
 *   text_en_banks.inc   — $0733 0/1/3–7 menu & battle (07:8F2E…)
 * Pointer-swap (text_script) installs hits into the WRAM overlay when lang==EN.
 */
#include "text_en_scripts.inc"
#include "text_en_banks.inc"

static const uint8_t *lookup_en_table(const DbzEnScript *tab, size_t n,
                                     uint8_t bank_sel, uint16_t str_idx,
                                     size_t *out_len) {
  for(size_t i = 0; i < n; i++) {
    if(tab[i].bank_sel == bank_sel && tab[i].str_idx == str_idx) {
      if(out_len) *out_len = tab[i].len;
      return tab[i].bytes;
    }
  }
  return 0;
}

DBZ_KEEP void dbz_i18n_set(DbzLang lang) {
  DbzLang next = (lang == DBZ_LANG_EN) ? DBZ_LANG_EN : DBZ_LANG_JA;
  if(next != g_lang) {
    dbz_text_unbind();
    dbz_text_en_font_reset();
  }
  g_lang = next;
}

DBZ_KEEP DbzLang dbz_i18n_get(void) {
  return g_lang;
}

static const char *const STR_JA[DBZ_STR_COUNT] = {
  "LANGUAGE",
  "ENGLISH",
  "JAPANESE",
  "UP/DOWN  A=OK",
  "English: native translation layer in progress",
  "Playing — drag/tap on canvas · keyboard still works. P pause, Tab turbo.",
  "CONTROLS",
  "TRADITIONAL",
  "DIRECT TOUCH",
  "UP/DOWN  A=OK",
  "Playing — tap menus directly · Controls in chrome. P pause, Tab turbo."
};

static const char *const STR_EN[DBZ_STR_COUNT] = {
  "LANGUAGE",
  "ENGLISH",
  "JAPANESE",
  "UP/DOWN  A=OK",
  "English: native translation layer in progress",
  "Playing — drag/tap on canvas · keyboard still works. P pause, Tab turbo.",
  "CONTROLS",
  "TRADITIONAL",
  "DIRECT TOUCH",
  "UP/DOWN  A=OK",
  "Playing — tap menus directly · Controls in chrome. P pause, Tab turbo."
};

const char *dbz_i18n_str(int id) {
  if(id < 0 || id >= DBZ_STR_COUNT) return "";
  const char *const *table = (g_lang == DBZ_LANG_EN) ? STR_EN : STR_JA;
  return table[id] ? table[id] : "";
}

const uint8_t *dbz_i18n_en_script(uint8_t bank_sel, uint16_t str_idx, size_t *out_len) {
  if(out_len) *out_len = 0;
  if(g_lang != DBZ_LANG_EN) return 0;
  const uint8_t *hit = lookup_en_table(EN_SCRIPTS, sizeof EN_SCRIPTS / sizeof EN_SCRIPTS[0],
                                      bank_sel, str_idx, out_len);
  if(hit) return hit;
  return lookup_en_table(EN_BANK_SCRIPTS, sizeof EN_BANK_SCRIPTS / sizeof EN_BANK_SCRIPTS[0],
                        bank_sel, str_idx, out_len);
}

uint8_t dbz_i18n_filter_script_byte(uint32_t addr24, uint8_t rom_byte) {
  if(g_lang != DBZ_LANG_EN) return rom_byte;
#if defined(DBZ_I18N_EXPERIMENTAL_HOOKS) && DBZ_I18N_EXPERIMENTAL_HOOKS
  if(dbz_i18n_remap_script_byte) return dbz_i18n_remap_script_byte(rom_byte);
#endif
  /* EN different-length lines use text_script pointer-swap (WRAM overlay). */
  return dbz_text_filter_script_byte(addr24, rom_byte);
}

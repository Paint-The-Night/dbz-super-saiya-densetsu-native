#ifndef DBZ_I18N_H
#define DBZ_I18N_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  DBZ_LANG_JA = 0,
  DBZ_LANG_EN = 1
} DbzLang;

/* Boot / host UI string ids (not in-game dialogue — that comes later). */
enum {
  DBZ_STR_LANG_TITLE = 0,
  DBZ_STR_LANG_ENGLISH,
  DBZ_STR_LANG_JAPANESE,
  DBZ_STR_LANG_HINT,
  DBZ_STR_EN_WIP_STATUS,
  DBZ_STR_PLAYING_STATUS,
  DBZ_STR_COUNT
};

void dbz_i18n_set(DbzLang lang);
DbzLang dbz_i18n_get(void);

/* Placeholder string table. Returns UTF-8 C string; never NULL. */
const char *dbz_i18n_str(int id);

/*
 * Dialogue script-byte filter used by the 01:CFB4 fetch leaf.
 * JA (default): always returns `rom_byte` unchanged (bit-identical path).
 * EN: returns an override byte when an active C-table remaps `addr24`;
 * otherwise returns `rom_byte`. Does not modify ROM.
 */
uint8_t dbz_i18n_filter_script_byte(uint32_t addr24, uint8_t rom_byte);

/*
 * Look up a raw EN script blob for far-table index ($0733) + string index ($0723).
 * Covers main 06:8000 (sel 2) and menu/battle sels 0/1/3–7. Returns NULL when no
 * C override exists (caller must keep JP stream).
 * Bytes use the game's glyph encoding (Klepto-compatible); not UTF-8.
 */
const uint8_t *dbz_i18n_en_script(uint8_t bank_sel, uint16_t str_idx, size_t *out_len);

/*
 * Experimental dialogue remap hook (off unless DBZ_I18N_EXPERIMENTAL_HOOKS=1).
 * When set and lang==EN, natives may call this while walking script bytes.
 * Default is NULL (no remap). Prefer dbz_i18n_filter_script_byte + tables.
 */
#if defined(DBZ_I18N_EXPERIMENTAL_HOOKS) && DBZ_I18N_EXPERIMENTAL_HOOKS
extern uint8_t (*dbz_i18n_remap_script_byte)(uint8_t jp_byte);
#endif

#ifdef __cplusplus
}
#endif

#endif /* DBZ_I18N_H */

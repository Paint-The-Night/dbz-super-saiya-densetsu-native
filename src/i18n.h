#ifndef DBZ_I18N_H
#define DBZ_I18N_H

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
 * Experimental dialogue remap hook (off unless DBZ_I18N_EXPERIMENTAL_HOOKS=1).
 * When set and lang==EN, natives may call this while walking script bytes.
 * Default is NULL (no remap). Do not enable until a leaf is verified.
 */
#if defined(DBZ_I18N_EXPERIMENTAL_HOOKS) && DBZ_I18N_EXPERIMENTAL_HOOKS
extern uint8_t (*dbz_i18n_remap_script_byte)(uint8_t jp_byte);
#endif

#ifdef __cplusplus
}
#endif

#endif /* DBZ_I18N_H */

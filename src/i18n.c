#include "i18n.h"
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

DBZ_KEEP void dbz_i18n_set(DbzLang lang) {
  g_lang = (lang == DBZ_LANG_EN) ? DBZ_LANG_EN : DBZ_LANG_JA;
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
  "Playing — drag/tap on canvas · keyboard still works. P pause, Tab turbo."
};

static const char *const STR_EN[DBZ_STR_COUNT] = {
  "LANGUAGE",
  "ENGLISH",
  "JAPANESE",
  "UP/DOWN  A=OK",
  "English: native translation layer in progress",
  "Playing — drag/tap on canvas · keyboard still works. P pause, Tab turbo."
};

const char *dbz_i18n_str(int id) {
  if(id < 0 || id >= DBZ_STR_COUNT) return "";
  const char *const *table = (g_lang == DBZ_LANG_EN) ? STR_EN : STR_JA;
  return table[id] ? table[id] : "";
}

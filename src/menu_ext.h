#ifndef DBZ_MENU_EXT_H
#define DBZ_MENU_EXT_H

/*
 * Party-submenu engine extension: native 6th row "Controls" / 「そうさ」.
 *
 * Evidence (docs/ram-map.md, docs/text-api.md):
 *   $0D66=1 party list; $0D64 exclusive max (ROM $02:E232 mode×3);
 *   window w/h from $02:E0F2 mode×4; labels via 00:9C67 → $02:DA16[1] →
 *   packed descriptor at $02:DACE (5 rows). Cursor wrap at 01:BA44 vs $0D64.
 *
 * Extension pattern (fan-game API — no IPS):
 *   1. cartRomFilter bumps mode-1 max 5→6 and window 10×12→12×14.
 *   2. DA16[1] retargets to $02:F500 (unused $FF) where C serves a 6-row
 *      descriptor (JA rows 0–4 bit-identical to Rev 1; EN matches chrome).
 *   3. Host intercepts A / Direct tap on index 5 → CONTROLS parchment.
 */

#include <stdint.h>
#include <stdbool.h>
#include "snes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Party Controls row index (0=Cards … 4=Save, 5=Controls). */
#define DBZ_MENU_PARTY_CONTROLS_INDEX 5u

/* Install cart filter (chains EN chrome). Call with text leave-vblank install. */
void dbz_menu_ext_install(Snes *snes);

/* Combined cart filter: structural party ext + EN chrome. */
uint8_t dbz_menu_ext_cart_filter(Snes *snes, uint32_t file_off, uint8_t rom_byte);

/* True when party submenu is open with Controls row reachable. */
bool dbz_menu_ext_party_open(const Snes *snes);

/* True when live cursor is on Controls (index 5). */
bool dbz_menu_ext_controls_selected(const Snes *snes);

#ifdef __cplusplus
}
#endif

#endif /* DBZ_MENU_EXT_H */

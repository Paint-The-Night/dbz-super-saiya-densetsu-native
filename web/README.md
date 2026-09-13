# Web (Emscripten) export

Static browser build of the hybrid native port for **densetsu.garyperrigo.com**.

## Hard rules

- **Never** embed, vend, or sync the commercial ROM (or `Backup/*.sfc`).
- Players load their own legally obtained **Japanese Rev 1** `.sfc` via the File API.
- Optional IndexedDB remember stores the ROM **only in the player’s browser**.
- Battery saves use IDBFS under `/dbz-saves` (also local to the browser).
- **No IPS / ROM patching** at runtime. EN and JP boot the same clean 1 MiB image;
  language is native C i18n (`src/i18n.*`). See `docs/i18n.md`.

Expected ROM SHA-256:

```
962aa7a09765a97164af67098877a8fe5b7f1ea9db738561eb466b7600fd241c
```

## Prerequisites

- [emsdk](https://emscripten.org/docs/getting_started/downloads.html) with `emcc` on `PATH`
  (or `EMSDK=/path/to/emsdk`)
- CMake ≥ 3.20, Ninja, Python 3

On this box the SDK lives at `/workspace/emsdk`:

```sh
source /workspace/emsdk/emsdk_env.sh
```

## Build

```sh
./tools/build-web.sh
```

Output: `web/dist/` containing `index.html`, `style.css`, content-hashed
`dbz.<hash>.js` / `dbz.<hash>.wasm`, and helper `_headers` / `mime.types.example`.
**Does not** copy `ips.js` or `patches/`.

Reproducible flags are set in `CMakeLists.txt` when `EMSCRIPTEN` is detected
(`src/web_main.c`, `-sUSE_SDL=2`, single-threaded, no pthreads).

## Deploy (S3 + CloudFront)

Distribution id: **E2QBWNDG548RRY** · bucket `densetsu-garyperrigo-com`.

```sh
DIST=web/dist
BUCKET=s3://densetsu-garyperrigo-com
# Sync hashed assets + index/css; drop legacy ips/patches from the bucket
aws s3 sync "$DIST/" "$BUCKET/" --delete \
  --exclude '*' \
  --include 'index.html' --include 'style.css' \
  --include 'dbz.*.js' --include 'dbz.*.wasm' --include '_headers'

aws s3 cp "$DIST/index.html" "$BUCKET/index.html" \
  --cache-control 'no-cache' \
  --content-type 'text/html; charset=utf-8'

# Hashed wasm/js (example — use actual names from dist)
for f in "$DIST"/dbz.*.wasm; do
  aws s3 cp "$f" "$BUCKET/$(basename "$f")" \
    --cache-control 'public,max-age=31536000,immutable' \
    --content-type 'application/wasm'
done
for f in "$DIST"/dbz.*.js; do
  aws s3 cp "$f" "$BUCKET/$(basename "$f")" \
    --cache-control 'public,max-age=31536000,immutable' \
    --content-type 'application/javascript'
done

aws cloudfront create-invalidation --distribution-id E2QBWNDG548RRY --paths '/*'
```

Point densetsu.garyperrigo.com at that distribution.

### Headers

| Path | Content-Type | Cache-Control |
|------|----------------|---------------|
| `index.html` | `text/html` | `no-cache` (or short max-age) |
| `style.css` | `text/css` | `public, max-age=3600` |
| `dbz.*.js` | `application/javascript` | long / immutable |
| `dbz.*.wasm` | **`application/wasm`** | long / immutable |

**No COOP/COEP** (or `Cross-Origin-Embedder-Policy`) headers are required:
this build is **single-threaded** (no `-pthread`).

## Language + Controls select + native i18n

Language and controls are **not** website HTML panels. After a validated
Japanese Rev 1 ROM is loaded, the **first screens in the game viewport** are
SDL-drawn boot menus (`src/web_main.c`):

1. **LANGUAGE / 言語** — Japanese or English.
2. **CONTROLS / 操作** — Traditional (gestures/pad) or Direct touch.

Not a third row on the language menu — two screens. The same CONTROLS parchment
reopens mid-play from the chrome **Controls** button (or Direct → party Text)
via `_dbz_web_open_controls_menu` and updates `dbz-controls` live without reboot.

- **Japanese** — `dbz_i18n_set(DBZ_LANG_JA)` then (after Controls) boot clean Rev 1.
- **English** — `dbz_i18n_set(DBZ_LANG_EN)` then boot the **same** clean Rev 1.
  Dialogue remains Japanese until native text/font hooks and EN tables exist
  (status may note the translation layer is in progress). No mojibake from IPS.

Host-menu input: D-pad / flick Up·Down, A / tap confirm; Direct mode tap-hits
the two Densetsu rows.

Choices persist as `localStorage` `dbz-lang` (`en`|`ja`) and `dbz-controls`
(`traditional`|`direct`). The menus still show every session. Quiet override:
hold **Start** while confirming the ROM load to skip **both** screens and use
the remembered language + controls pair.

Klepto IPS lives under `research/klepto-reference/` as wording reference only.

### Host overlay chrome (`draw_densetsu_window`)

LANGUAGE and CONTROLS share `draw_densetsu_window` in `src/web_main.c`. Chrome
is **pixel-sampled** from `artifacts/scout-start-game/frame-003000.bmp`
(LakeSnes 2×): cream/orange/dark bevel, baked 16×16 corner studs, ocean
playfield, dark-ink labels, and a blinking action-menu triangle cursor. No ROM
patching — SDL framebuffer only.


## How ROM load works

1. Page loads hashed `dbz.js` / `dbz.wasm` (engine only). Loader is the first **site** screen.
2. User picks or drops a `.sfc`, or clicks **Play remembered ROM** (IndexedDB; click required for Web Audio unlock).
3. JS validates JP SHA-256 and holds the clean 1 MiB image (no patch step).
4. C `_dbz_web_enter_lang_menu` draws LANGUAGE, then CONTROLS, on the SDL canvas.
5. On Controls confirm, C sets `dbz_i18n_set` + controls mode; JS calls `_dbz_web_load_rom` / `_dbz_web_load_prepared_rom` with the **clean** buffer (C re-checks SHA-256).
6. Emulator frame loop starts; real title/boot proceeds.

## Smoke test

```sh
# After build-web.sh:
test -f web/dist/index.html
test -z "$(find web/dist -name 'ips.js' -o -path '*/patches/*')"
ls web/dist/dbz.*.js web/dist/dbz.*.wasm
```

## Controls

### Keyboard (unchanged)

Arrows, Z/A/X/S/D/C, Enter, Right Shift; P pause; Tab turbo.

### Canvas-native touch (mode-gated)

The game canvas is the input surface. **Both** Traditional and Direct use SNES
bits 0–11 for confirms (`snes_setButtonState`). Direct is **tap-the-thing**:
hit-test a measured menu rect, **write** the live cursor WRAM, pulse A — not
N× D-pad scroll.

**Traditional** (default) — current gesture map:

| Gesture | SNES bit(s) |
|---------|-------------|
| Finger drag (held &gt; ~280 ms past deadzone) | D-pad Up/Down/Left/Right (8-way hold) |
| Quick tap | A (bit 8), ~3 frames |
| Long-press (~350 ms, little move) | B (bit 0), ~3 frames |
| Swipe flick | Single-step D-pad pulse (~2 frames) |
| Two-finger tap | Start (bit 3) |
| Three-finger tap | Select (bit 2) |

**Direct touch** — not a fake pad. JS calls `_dbz_web_canvas_tap(x, y)` in
512×480 framebuffer pixels:

| Surface | Behavior |
|---------|----------|
| Host LANGUAGE / CONTROLS | Hit-test the two Densetsu rows; set host cursor and confirm |
| In-game | **Write→A:** if DP+`$25==1` and tap hits a measured rect (command 0 / party 1 / flight `$0A` / Status `$0F`), write `$0D62` (Status also `$0D63` + `$1100,Y` mirror) then pulse A. Party Text opens host CONTROLS. Hit-rect **Y origins are one `DBZ_UI_ROW_PITCH` (32px @2×) above** the raw tip/divider measurements — playtest 2026-09-12 had to tap the row below; Status 3×3 gets the same 32px lift. See `docs/ram-map.md`. |

Two-finger Start / three-finger Select still work in Direct (system shortcuts,
not a virtual pad overlay). Classic pad stays **hidden by default** in Direct;
the a11y checkbox can still show it.

Chrome strip below the canvas (text links, not a fake SNES pad):
**Select · L · R · Controls · Pause · Turbo**.

Host **SKIP** (Densetsu cream/orange, top-right of the stage) appears once
the ROM is running and hides after the overworld command UI is seeded. Tap
it (HTML overlay **or** canvas top-right hit-rect — Traditional and Direct)
to jump the opening via Start on title/crawl/clouds, A through Raditz
dialogue only, then one A to open the Kame House command menu. While armed
the button shows an orange pulse; pending pulses are cleared and inputs
freeze once `$0D66==0` / `$0D64==5` so A cannot thrash Talk→party (`$0723=9e`).

Optional **Show classic pad** checkbox is Traditional / accessibility-only;
default UX hides `#touch-pad`.

C APIs (frame-synced in `web_main.c`):

- `dbz_web_set_button` — classic pad hold mask (`keys_touch`)
- `dbz_web_set_gesture_mask` — canvas virtual-stick hold (`keys_gesture`)
- `dbz_web_pulse_button(button, frames)` — queued pulses OR’d each `frame_tick`
- `dbz_web_set_controls_mode` / `dbz_web_get_controls_mode` — 0 Traditional, 1 Direct
- `dbz_web_canvas_tap(x, y)` — Direct hit-test / in-game write→A
- `dbz_web_open_controls_menu` — reopen CONTROLS parchment mid-play
- `dbz_web_skip_cutscene` — host **SKIP** (top-right). Visible only during
  the opening (title / crawl / Raditz), then latches off on overworld.
  Synthesizes the game’s own skip: **Start** on title/crawl/clouds
  (`tests/start-game.inputs` frames 300/600), **A** while Raditz dialogue
  is active, then one **A** to open the command menu. Not a WRAM warp;
  Start is not sent after intro visuals; post-menu A is suppressed.
  Headless: `DBZ_SKIP_SIM=1` on `dbz-port`.
- Each frame: `keys_kb | keys_touch | keys_gesture | pulse` → `snes_setButtonState`

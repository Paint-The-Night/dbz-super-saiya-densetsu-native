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

## Language select + native i18n

Language is **not** a website HTML panel. After a validated Japanese Rev 1 ROM is
loaded, the **first screen in the game viewport** is an SDL-drawn
**LANGUAGE / 言語** boot menu (`src/web_main.c`).

- **Japanese** — `dbz_i18n_set(DBZ_LANG_JA)` then boot clean Rev 1.
- **English** — `dbz_i18n_set(DBZ_LANG_EN)` then boot the **same** clean Rev 1.
  Dialogue remains Japanese until native text/font hooks and EN tables exist
  (status may note the translation layer is in progress). No mojibake from IPS.

Controls: D-pad / flick Up·Down, A / tap confirm.

Choice is remembered in `localStorage` key `dbz-lang` (cursor default), but the
in-game boot menu is shown every session. Quiet override: hold **Start** while
confirming the ROM load to skip the menu and use the remembered language.

Klepto IPS lives under `research/klepto-reference/` as wording reference only.

### Host overlay chrome (`draw_densetsu_window`)

Boot-menu (and future host overlays) share `draw_densetsu_window` in
`src/web_main.c`. Chrome is **pixel-sampled** from
`artifacts/scout-start-game/frame-003000.bmp` (LakeSnes 2×): cream/orange/dark
bevel, baked 16×16 corner studs, ocean playfield, dark-ink labels, and a blinking
action-menu triangle cursor. No ROM patching — SDL framebuffer only.


## How ROM load works

1. Page loads hashed `dbz.js` / `dbz.wasm` (engine only). Loader is the first **site** screen.
2. User picks or drops a `.sfc`, or clicks **Play remembered ROM** (IndexedDB; click required for Web Audio unlock).
3. JS validates JP SHA-256 and holds the clean 1 MiB image (no patch step).
4. C `_dbz_web_enter_lang_menu` draws the in-game LANGUAGE menu on the SDL canvas.
5. On confirm, C sets `dbz_i18n_set`; JS calls `_dbz_web_load_rom` / `_dbz_web_load_prepared_rom` with the **clean** buffer (C re-checks SHA-256).
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

### Canvas-native touch (primary)

The game canvas is the input surface. Gestures synthesize the same SNES bits
0–11 that the keyboard uses (`snes_setButtonState`). No menu WRAM is poked.

| Gesture | SNES bit(s) |
|---------|-------------|
| Finger drag (held &gt; ~280 ms past deadzone) | D-pad Up/Down/Left/Right (8-way hold) |
| Quick tap | A (bit 8), ~3 frames |
| Long-press (~350 ms, little move) | B (bit 0), ~3 frames |
| Swipe flick | Single-step D-pad pulse (~2 frames) |
| Two-finger tap | Start (bit 3) |
| Three-finger tap | Select (bit 2) |

Chrome strip below the canvas (text links, not a fake SNES pad):
**Select · L · R · Pause · Turbo**.

Optional **Show classic pad** checkbox in Controls is accessibility-only;
default UX hides `#touch-pad`.

C APIs (frame-synced in `web_main.c`):

- `dbz_web_set_button` — classic pad hold mask (`keys_touch`)
- `dbz_web_set_gesture_mask` — canvas virtual-stick hold (`keys_gesture`)
- `dbz_web_pulse_button(button, frames)` — queued pulses OR’d each `frame_tick`
- Each frame: `keys_kb | keys_touch | keys_gesture | pulse` → `snes_setButtonState`

### Next: menu hit-test via cursor RAM

Investigation of `docs/ram-map.md` / tests found **no** recovered battle or
overworld **menu selection cursor** byte suitable for “tap this row to select
it”. Existing “cursor” names are WRAM/OAM/script stream cursors, not UI index.
**Do not invent RAM addresses.** When a menu cursor address is recovered,
optional assist can queue N× Up/Down then A from tap Y without bypassing
`snes_setButtonState`.

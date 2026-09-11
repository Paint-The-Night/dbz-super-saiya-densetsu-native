# Web (Emscripten) export

Static browser build of the hybrid native port for **densetsu.garyperrigo.com**.

## Hard rules

- **Never** embed, vend, or sync the commercial ROM (or `Backup/*.sfc`).
- Players load their own legally obtained **Japanese Rev 1** `.sfc` via the File API.
- Optional IndexedDB remember stores the ROM **only in the player’s browser**.
- Battery saves use IDBFS under `/dbz-saves` (also local to the browser).

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

Output: `web/dist/` containing `index.html`, `style.css`, `ips.js`, `dbz.js`, `dbz.wasm`,
`patches/*` (IPS only), and helper `_headers` / `mime.types.example`.

Reproducible flags are set in `CMakeLists.txt` when `EMSCRIPTEN` is detected
(`src/web_main.c`, `-sUSE_SDL=2`, single-threaded, no pthreads).

## Deploy (S3 + CloudFront example)

```sh
# Sync static assets; do NOT upload any .sfc
aws s3 sync web/dist/ s3://YOUR_BUCKET/ \
  --delete \
  --exclude '*' \
  --include 'index.html' --include 'style.css' --include 'ips.js' \
  --include 'dbz.js' --include 'dbz.wasm' --include '_headers' \
  --include 'patches/*'

aws s3 cp web/dist/index.html s3://YOUR_BUCKET/index.html \
  --cache-control 'no-cache' \
  --content-type 'text/html; charset=utf-8'

aws s3 cp web/dist/dbz.wasm s3://YOUR_BUCKET/dbz.wasm \
  --cache-control 'public,max-age=31536000,immutable' \
  --content-type 'application/wasm'

aws s3 cp web/dist/dbz.js s3://YOUR_BUCKET/dbz.js \
  --cache-control 'public,max-age=31536000,immutable' \
  --content-type 'application/javascript'
```

Point the densetsu.garyperrigo.com distribution at that bucket/prefix.

### Headers

| Path | Content-Type | Cache-Control |
|------|----------------|---------------|
| `index.html` | `text/html` | `no-cache` (or short max-age) |
| `style.css` | `text/css` | `public, max-age=3600` |
| `dbz.js` | `application/javascript` | long / immutable if hashed later |
| `dbz.wasm` | **`application/wasm`** | long / immutable if hashed later |

**No COOP/COEP** (or `Cross-Origin-Embedder-Policy`) headers are required:
this build is **single-threaded** (no `-pthread`).


## Language select + English patch

The first screen is a SNES-styled **言語選択 / Language** menu (English / 日本語).

- **Japanese** — unmodified Rev 1 (same SHA-256 gate as before).
- **English** — after the clean JP hash check, the browser fetches
  `patches/klepto-ssd-en-1.02.ips` (Klepto Software 1.02 / romhacking.net #326),
  prepends a 512-byte zero header, applies the IPS **in RAM**, strips the header,
  then calls `_dbz_web_load_prepared_rom` (skips the C hash so the patched image
  can boot). No patched ROM is ever hosted or uploaded.

See `web/patches/README.md` for the IPS SHA-256 and credit.

Choice is stored in `localStorage` key `dbz-lang`.

## How ROM load works

1. Page loads `dbz.js` / `dbz.wasm` (engine only).
2. User picks or drops a `.sfc`, or clicks **Play remembered ROM** (IndexedDB; click required for Web Audio unlock).
3. JS validates JP SHA-256 (`ips.js` / `DbzPatch.prepareRom`); English applies IPS in RAM.
4. JS copies the prepared 1 MiB buffer and calls `_dbz_web_load_prepared_rom`
   (hash already checked in JS; C skips re-hash). Legacy `_dbz_web_load_rom` still hashes.
5. On success, SDL starts the frame loop via `emscripten_set_main_loop`.

## Smoke test

```sh
# After build-web.sh:
test -f web/dist/dbz.wasm && test -f web/dist/dbz.js
# Optional: Node can instantiate the module object, but full SDL/WebAudio
# play requires a real browser (manual).
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

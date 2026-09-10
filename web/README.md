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

Output: `web/dist/` containing `index.html`, `style.css`, `dbz.js`, `dbz.wasm`
(and helper `_headers` / `mime.types.example`).

Reproducible flags are set in `CMakeLists.txt` when `EMSCRIPTEN` is detected
(`src/web_main.c`, `-sUSE_SDL=2`, single-threaded, no pthreads).

## Deploy (S3 + CloudFront example)

```sh
# Sync static assets; do NOT upload any .sfc
aws s3 sync web/dist/ s3://YOUR_BUCKET/ \
  --delete \
  --exclude '*' \
  --include 'index.html' --include 'style.css' \
  --include 'dbz.js' --include 'dbz.wasm' --include '_headers'

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

## How ROM load works

1. Page loads `dbz.js` / `dbz.wasm` (engine only).
2. User picks or drops a `.sfc`, or clicks **Play remembered ROM** (IndexedDB; click required for Web Audio unlock).
3. JS copies bytes into WASM heap and calls `_dbz_web_load_rom`.
4. `dbz_load_rom_bytes` checks size + SHA-256; mismatch shows an on-page error.
5. On success, SDL starts the frame loop via `emscripten_set_main_loop`.

## Smoke test

```sh
# After build-web.sh:
test -f web/dist/dbz.wasm && test -f web/dist/dbz.js
# Optional: Node can instantiate the module object, but full SDL/WebAudio
# play requires a real browser (manual).
```

## Controls

Same as desktop: arrows, Z/A/X/S/D/C, Enter, Right Shift; P pause; Tab turbo.

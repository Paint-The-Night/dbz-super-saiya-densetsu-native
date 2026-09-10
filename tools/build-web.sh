#!/usr/bin/env bash
# Build a static Emscripten/WASM site into web/dist/ (no ROM embedded).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DIST="${ROOT}/web/dist"
BUILD="${ROOT}/build-web"
EMSDK_ROOT="${EMSDK:-${EMSDK_ROOT:-/workspace/emsdk}}"

if [[ -f "${EMSDK_ROOT}/emsdk_env.sh" ]]; then
  # shellcheck disable=SC1091
  source "${EMSDK_ROOT}/emsdk_env.sh"
elif ! command -v emcc >/dev/null 2>&1; then
  echo "emcc not found. Install emsdk and set EMSDK=/path/to/emsdk" >&2
  exit 1
fi

command -v emcmake >/dev/null
command -v cmake >/dev/null

echo "==> Configuring Emscripten build in ${BUILD}"
emcmake cmake -S "${ROOT}" -B "${BUILD}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDBZ_TEST_ROM=

echo "==> Building dbz.js / dbz.wasm"
cmake --build "${BUILD}" --target dbz-port --parallel "$(nproc 2>/dev/null || echo 4)"

echo "==> Assembling ${DIST}"
rm -rf "${DIST}"
mkdir -p "${DIST}"
cp -f "${ROOT}/web/index.html" "${ROOT}/web/style.css" "${DIST}/"
cp -f "${BUILD}/dbz.js" "${BUILD}/dbz.wasm" "${DIST}/"
# Optional source map if present
[[ -f "${BUILD}/dbz.wasm.map" ]] && cp -f "${BUILD}/dbz.wasm.map" "${DIST}/" || true

# Sanity: refuse if any ROM-like payload slipped in
if find "${DIST}" -type f \( -name '*.sfc' -o -name '*.smc' -o -name '*.srm' \) | grep -q .; then
  echo "ERROR: ROM/save artifact found under dist — aborting" >&2
  exit 2
fi

# Expected ROM SHA must not appear as a binary blob match of the ROM itself;
# the ASCII hash string in index.html is intentional documentation.
ROM_HASH=962aa7a09765a97164af67098877a8fe5b7f1ea9db738561eb466b7600fd241c
if command -v python3 >/dev/null; then
  python3 - << PY
from pathlib import Path
dist = Path("${DIST}")
# Fail if any file is exactly 1048576 or 1049088 bytes (ROM sizes)
bad = [p for p in dist.rglob("*") if p.is_file() and p.stat().st_size in (1048576, 1049088)]
if bad:
    raise SystemExit(f"ERROR: dist contains ROM-sized file(s): {bad}")
print("ROM-size check: OK")
PY
fi

echo "==> Dist contents:"
find "${DIST}" -type f -printf '%10s  %p\n' | sort
cat > "${DIST}/_headers" << 'HDR'
# Cloudflare Pages / similar; CloudFront needs matching Cache-Control & Content-Type
/*
  X-Content-Type-Options: nosniff

/index.html
  Cache-Control: no-cache

/style.css
  Cache-Control: public, max-age=3600

/dbz.js
  Cache-Control: public, max-age=31536000, immutable

/dbz.wasm
  Content-Type: application/wasm
  Cache-Control: public, max-age=31536000, immutable
HDR

cat > "${DIST}/mime.types.example" << 'MIME'
# For nginx / custom servers:
# types { application/wasm wasm; }
# Or: add_header Content-Type application/wasm;
MIME

echo "==> Done. Serve ${DIST} (no COOP/COEP required; single-threaded)."
echo "    Local smoke:  npx --yes serve -p 8765 '${DIST}'"

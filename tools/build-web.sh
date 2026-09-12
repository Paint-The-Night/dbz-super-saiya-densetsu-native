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
cp -f "${ROOT}/web/style.css" "${ROOT}/web/ips.js" "${DIST}/"
# Content-hash wasm/js so CloudFront immutable cache cannot serve stale exports
JS_HASH=$(sha256sum "${BUILD}/dbz.js" | cut -c1-8)
WASM_HASH=$(sha256sum "${BUILD}/dbz.wasm" | cut -c1-8)
JS_NAME="dbz.${JS_HASH}.js"
WASM_NAME="dbz.${WASM_HASH}.wasm"
cp -f "${BUILD}/dbz.js" "${DIST}/${JS_NAME}"
cp -f "${BUILD}/dbz.wasm" "${DIST}/${WASM_NAME}"
# Optional source map if present
[[ -f "${BUILD}/dbz.wasm.map" ]] && cp -f "${BUILD}/dbz.wasm.map" "${DIST}/${WASM_NAME}.map" || true
# Rewrite index.html to hashed assets
python3 - << PY2
from pathlib import Path
html = Path("${ROOT}/web/index.html").read_text()
js_name = "${JS_NAME}"
wasm_name = "${WASM_NAME}"
html = html.replace('src="dbz.js"', f'src="{js_name}"')
# locateFile must map dbz.wasm -> hashed name; also map dbz.js if requested
old_lf = """locateFile: function(path) { return path; },"""
new_lf = f"""locateFile: function(path) {{
        if(path === 'dbz.wasm' || path.endsWith('.wasm')) return '{wasm_name}';
        if(path === 'dbz.js') return '{js_name}';
        return path;
      }},"""
if old_lf not in html:
    raise SystemExit('locateFile stub not found in index.html')
html = html.replace(old_lf, new_lf, 1)
Path("${DIST}/index.html").write_text(html)
print(f"index uses {js_name} + {wasm_name}")
PY2
# Translation patches only (never ROMs)
mkdir -p "${DIST}/patches"
if [[ -d "${ROOT}/web/patches" ]]; then
  cp -f "${ROOT}/web/patches/"* "${DIST}/patches/" 2>/dev/null || true
fi
# Refuse accidental ROM drop into patches/
if find "${DIST}/patches" -type f \( -name '*.sfc' -o -name '*.smc' \) 2>/dev/null | grep -q .; then
  echo "ERROR: ROM found under dist/patches — aborting" >&2
  exit 2
fi

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

/ips.js
  Cache-Control: public, max-age=3600

/patches/*
  Cache-Control: public, max-age=86400

/dbz.*.js
  Cache-Control: public, max-age=31536000, immutable

/dbz.*.wasm
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

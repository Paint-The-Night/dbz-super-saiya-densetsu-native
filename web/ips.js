/* IPS (and optional BPS) apply for Klepto SSD EN 1.02 — browser only, never hosts a ROM. */
(function (global) {
  'use strict';

  var JP_SHA256 = '962aa7a09765a97164af67098877a8fe5b7f1ea9db738561eb466b7600fd241c';
  var PATCH_URL = 'patches/klepto-ssd-en-1.02.ips';
  var patchCache = null;

  function toHex(buf) {
    var u8 = buf instanceof Uint8Array ? buf : new Uint8Array(buf);
    var out = '';
    for (var i = 0; i < u8.length; i++) out += (u8[i] + 0x100).toString(16).slice(1);
    return out;
  }

  async function sha256Hex(u8) {
    var digest = await crypto.subtle.digest('SHA-256', u8);
    return toHex(digest);
  }

  function applyIps(target, patch) {
    if (patch.length < 8) throw new Error('IPS patch too small');
    var magic = String.fromCharCode(patch[0], patch[1], patch[2], patch[3], patch[4]);
    if (magic !== 'PATCH') throw new Error('Not an IPS patch (missing PATCH header)');
    var i = 5;
    var rom = target;
    while (i + 3 <= patch.length) {
      if (patch[i] === 0x45 && patch[i + 1] === 0x4f && patch[i + 2] === 0x46) break; /* EOF */
      if (i + 5 > patch.length) throw new Error('Truncated IPS record');
      var offset = (patch[i] << 16) | (patch[i + 1] << 8) | patch[i + 2];
      i += 3;
      var size = (patch[i] << 8) | patch[i + 1];
      i += 2;
      if (size === 0) {
        if (i + 3 > patch.length) throw new Error('Truncated IPS RLE');
        var rle = (patch[i] << 8) | patch[i + 1];
        i += 2;
        var val = patch[i++];
        var end = offset + rle;
        if (end > rom.length) {
          var grown = new Uint8Array(end);
          grown.set(rom);
          rom = grown;
        }
        for (var j = offset; j < end; j++) rom[j] = val;
      } else {
        if (i + size > patch.length) throw new Error('Truncated IPS data');
        var end2 = offset + size;
        if (end2 > rom.length) {
          var grown2 = new Uint8Array(end2);
          grown2.set(rom);
          rom = grown2;
        }
        rom.set(patch.subarray(i, i + size), offset);
        i += size;
      }
    }
    return rom;
  }

  /* Minimal BPS (Beat) applier — used only if a .bps is hosted instead of .ips. */
  function applyBps(source, patch) {
    if (patch.length < 19) throw new Error('BPS patch too small');
    if (patch[0] !== 0x42 || patch[1] !== 0x50 || patch[2] !== 0x53 || patch[3] !== 0x31)
      throw new Error('Not a BPS patch');
    var pos = 4;
    function readVL() {
      var data = 0, shift = 1, x;
      while (true) {
        if (pos >= patch.length) throw new Error('Truncated BPS VLQ');
        x = patch[pos++];
        data += (x & 0x7f) * shift;
        if (x & 0x80) break;
        shift <<= 7;
        data += shift;
      }
      return data;
    }
    var sourceSize = readVL();
    var targetSize = readVL();
    var metaLen = readVL();
    pos += metaLen;
    if (source.length !== sourceSize) throw new Error('BPS source size mismatch');
    var target = new Uint8Array(targetSize);
    var outputOffset = 0;
    var sourceRelative = 0;
    var targetRelative = 0;
    while (outputOffset < targetSize) {
      var cmd = readVL();
      var length = (cmd >> 2) + 1;
      var action = cmd & 3;
      if (action === 0) {
        for (var a = 0; a < length; a++) {
          target[outputOffset] = source[outputOffset];
          outputOffset++;
        }
      } else if (action === 1) {
        for (var b = 0; b < length; b++) target[outputOffset++] = patch[pos++];
      } else if (action === 2) {
        var offsetData = readVL();
        sourceRelative += (offsetData & 1 ? -1 : 1) * (offsetData >> 1);
        for (var c = 0; c < length; c++) target[outputOffset++] = source[sourceRelative++];
      } else {
        var offsetData2 = readVL();
        targetRelative += (offsetData2 & 1 ? -1 : 1) * (offsetData2 >> 1);
        for (var d = 0; d < length; d++) {
          target[outputOffset] = target[targetRelative++];
          outputOffset++;
        }
      }
    }
    return target;
  }

  async function fetchPatchBytes() {
    if (patchCache) return patchCache;
    var res = await fetch(PATCH_URL, { cache: 'force-cache' });
    if (!res.ok) throw new Error('Could not download English patch (' + res.status + '). File missing at ' + PATCH_URL);
    var buf = new Uint8Array(await res.arrayBuffer());
    if (buf.length < 8) throw new Error('English patch file is empty or corrupt');
    patchCache = buf;
    return buf;
  }

  /** Validate clean JP Rev 1, optionally apply Klepto IPS in RAM. Returns 1 MiB Uint8Array. */
  async function prepareRom(u8, lang) {
    if (!(u8 instanceof Uint8Array)) u8 = new Uint8Array(u8);
    var payload;
    if (u8.length === 1049088) payload = u8.subarray(512);
    else if (u8.length === 1048576) payload = u8;
    else {
      var err = new Error('Wrong file size. Need 1,048,576 bytes (or +512-byte copier header).');
      err.code = 1;
      throw err;
    }
    var digest = await sha256Hex(payload);
    if (digest !== JP_SHA256) {
      var err2 = new Error('SHA-256 mismatch. Only unmodified Japanese Rev 1 is accepted.');
      err2.code = 2;
      throw err2;
    }
    var clean = new Uint8Array(payload); /* owned copy */
    if (lang !== 'en') return clean;

    var headered = new Uint8Array(1049088);
    headered.set(clean, 512); /* 512 leading zero bytes */
    var patch = await fetchPatchBytes();
    var magic5 = String.fromCharCode(patch[0], patch[1], patch[2], patch[3], patch[4]);
    var patchedHeadered;
    if (magic5 === 'PATCH') patchedHeadered = applyIps(headered, patch);
    else if (patch[0] === 0x42 && patch[1] === 0x50 && patch[2] === 0x53 && patch[3] === 0x31)
      patchedHeadered = applyBps(headered, patch);
    else throw new Error('Unrecognized patch format (need IPS or BPS)');

    if (patchedHeadered.length < 1049088)
      throw new Error('Patched ROM shorter than expected');
    return new Uint8Array(patchedHeadered.subarray(512, 512 + 1048576));
  }

  global.DbzPatch = {
    JP_SHA256: JP_SHA256,
    PATCH_URL: PATCH_URL,
    sha256Hex: sha256Hex,
    applyIps: applyIps,
    applyBps: applyBps,
    prepareRom: prepareRom,
    fetchPatchBytes: fetchPatchBytes
  };
})(typeof window !== 'undefined' ? window : globalThis);

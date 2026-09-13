#include "checkpoint.h"
#include "rom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t little32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void put32(uint8_t *p, uint32_t v) {
  for(unsigned i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (i * 8));
}

bool dbz_checkpoint_validate(const uint8_t *data, size_t length,
                             int expect_state_size,
                             uint32_t *out_phase, int *out_state_size) {
  if(!data || length < (size_t)DBZ_CHECKPOINT_HEADER) return false;
  if(memcmp(data, "DBZCHK1\n", 8) != 0) return false;
  if(memcmp(data + 8, DBZ_ROM_SHA256, 64) != 0) return false;
  uint32_t phase = little32(data + 136);
  uint32_t size = little32(data + 140);
  if(phase >= 600988u) return false;
  if(expect_state_size > 0 && size != (uint32_t)expect_state_size) return false;
  if(length != (size_t)size + (size_t)DBZ_CHECKPOINT_HEADER) return false;
  char digest[65];
  dbz_sha256(data + 136, (size_t)size + 8, digest);
  if(memcmp(data + 72, digest, 64) != 0) return false;
  if(out_phase) *out_phase = phase;
  if(out_state_size) *out_state_size = (int)size;
  return true;
}

bool dbz_checkpoint_apply(const uint8_t *data, size_t length,
                          Snes *game, Snes *reference, int expect_state_size,
                          uint32_t *out_phase) {
  uint32_t phase = 0;
  int size = 0;
  if(!game) return false;
  if(!dbz_checkpoint_validate(data, length, expect_state_size, &phase, &size))
    return false;
  if(!snes_loadState(game, (uint8_t *)(data + DBZ_CHECKPOINT_HEADER), size))
    return false;
  if(reference &&
     !snes_loadState(reference, (uint8_t *)(data + DBZ_CHECKPOINT_HEADER), size))
    return false;
  if(out_phase) *out_phase = phase;
  return true;
}

bool dbz_checkpoint_load_file(const char *path, Snes *game, Snes *reference,
                              int expect_state_size, uint32_t *out_phase) {
  if(!path || !game || expect_state_size <= 0) return false;
  FILE *f = fopen(path, "rb");
  if(!f) return false;
  size_t length = (size_t)expect_state_size + (size_t)DBZ_CHECKPOINT_HEADER;
  uint8_t *data = malloc(length);
  if(!data) { fclose(f); return false; }
  bool ok = fread(data, 1, length, f) == length && fgetc(f) == EOF && !ferror(f);
  fclose(f);
  if(!ok) { free(data); return false; }
  ok = dbz_checkpoint_apply(data, length, game, reference, expect_state_size, out_phase);
  free(data);
  return ok;
}

bool dbz_checkpoint_save_file(const char *dir, const uint8_t *state, int size,
                              uint32_t phase) {
  if(!dir || !state || size <= 0) return false;
  uint8_t *data = calloc(1, (size_t)size + (size_t)DBZ_CHECKPOINT_HEADER);
  if(!data) return false;
  memcpy(data, "DBZCHK1\n", 8);
  memcpy(data + 8, DBZ_ROM_SHA256, 64);
  put32(data + 136, phase);
  put32(data + 140, (uint32_t)size);
  memcpy(data + DBZ_CHECKPOINT_HEADER, state, (size_t)size);
  char digest[65];
  dbz_sha256(data + 136, (size_t)size + 8, digest);
  memcpy(data + 72, digest, 64);
  char path[4096];
  if(snprintf(path, sizeof(path), "%s/final.dbzstate", dir) >= (int)sizeof(path)) {
    free(data);
    return false;
  }
  FILE *f = fopen(path, "wb");
  if(!f) { free(data); return false; }
  bool ok = fwrite(data, 1, (size_t)size + (size_t)DBZ_CHECKPOINT_HEADER, f) ==
            (size_t)size + (size_t)DBZ_CHECKPOINT_HEADER;
  if(fclose(f) != 0) ok = false;
  free(data);
  return ok;
}

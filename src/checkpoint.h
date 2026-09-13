#ifndef DBZ_CHECKPOINT_H
#define DBZ_CHECKPOINT_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "snes.h"

/* DBZCHK1 header is 144 bytes; payload is snes_saveState size. */
enum { DBZ_CHECKPOINT_HEADER = 144 };

/* Validate a full .dbzstate buffer (header+payload). On success writes
 * *out_phase and *out_state_size. Does not touch the emulator. */
bool dbz_checkpoint_validate(const uint8_t *data, size_t length,
                             int expect_state_size,
                             uint32_t *out_phase, int *out_state_size);

/* Load validated checkpoint bytes into game (and optional reference). */
bool dbz_checkpoint_apply(const uint8_t *data, size_t length,
                          Snes *game, Snes *reference, int expect_state_size,
                          uint32_t *out_phase);

/* File helpers used by the desktop host. */
bool dbz_checkpoint_load_file(const char *path, Snes *game, Snes *reference,
                              int expect_state_size, uint32_t *out_phase);
bool dbz_checkpoint_save_file(const char *dir, const uint8_t *state, int size,
                              uint32_t phase);

#endif

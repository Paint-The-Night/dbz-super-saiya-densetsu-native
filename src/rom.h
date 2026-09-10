#ifndef DBZ_ROM_H
#define DBZ_ROM_H
#include <stddef.h>
#include <stdint.h>
uint8_t *dbz_read_rom(const char *path, size_t *size);
/* Validate in-memory ROM bytes (header optional). Returns owned 1 MiB buffer or NULL. */
uint8_t *dbz_load_rom_bytes(const uint8_t *data, size_t length, size_t *size);
void dbz_sha256(const uint8_t *data, size_t size, char result[65]);
#define DBZ_ROM_SHA256 "962aa7a09765a97164af67098877a8fe5b7f1ea9db738561eb466b7600fd241c"
#endif

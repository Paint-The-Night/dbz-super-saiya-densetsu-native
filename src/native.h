#ifndef DBZ_NATIVE_H
#define DBZ_NATIVE_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "cpu.h"

typedef struct {
  Cpu *cpu;
  bool enabled;
  uint64_t native_steps, interpreted_steps, reset_steps, display_steps, rng_steps;
  uint64_t controller_steps, scroll_steps;
  uint64_t upload_steps;
  uint64_t sprite_steps, palette_steps;
  uint64_t display_control_steps;
  uint64_t cycles_native, cycles_interpreted;
  uint64_t rng_entries[2];
  uint32_t visited[0x100000];
  uint32_t recent_pc[32];
  uint32_t recent_index;
} DbzExecution;

void dbz_set_execution(DbzExecution *execution);
bool dbz_native_step(Cpu *cpu, DbzExecution *execution);
void lakesnes_cpu_runOpcode(Cpu *cpu);
void dbz_write_execution(FILE *f, const DbzExecution *execution);
#endif

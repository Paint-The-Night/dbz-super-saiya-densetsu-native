/* Game-specific reconstruction for Japanese Rev 1. The host verifies the full
 * ROM hash before enabling these fixed-address replacements.
 *
 * Each call performs one statically known instruction of a named game routine.
 * Bus transactions and interrupt sampling stay at their original boundaries;
 * the opcode bytes are fetched for bus timing, not decoded by an interpreter.
 * This intentionally conservative ABI lets interrupts resume in mid-routine.
 * Scheduling semantics follow the MIT LakeSnes reference (see its notice).
 */
#include "native.h"
#include "snes.h"
#include <inttypes.h>

static DbzExecution *active;
void dbz_set_execution(DbzExecution *execution) { active = execution; }

static void sample_interrupt(Cpu *c) {
  c->intWanted = c->nmiWanted || (c->irqWanted && !c->i);
}
static uint8_t fetch(Cpu *c) {
  uint32_t address = ((uint32_t)c->k << 16) | c->pc++;
  return c->read(c->mem, address);
}
static void idle(Cpu *c) { c->idle(c->mem, false); }
static void implied(Cpu *c) {
  sample_interrupt(c);
  if(c->intWanted) c->read(c->mem, ((uint32_t)c->k << 16) | c->pc);
  else idle(c);
}
static void zn(Cpu *c, uint16_t value, bool byte) {
  c->z = (byte ? value & 255u : value) == 0;
  c->n = (value & (byte ? 0x80u : 0x8000u)) != 0;
}
static void set_a8(Cpu *c, uint8_t v) {
  c->a = (c->a & 0xff00u) | v;
  zn(c, v, true);
}
static void set_m(Cpu *c, bool m) {
  (void)fetch(c); // REP/SEP operand bus cycle
  sample_interrupt(c);
  c->mf = c->e || m;
  idle(c);
}
static uint16_t dp_address(Cpu *c, uint8_t offset) {
  (void)fetch(c);
  if(c->dp & 255u) idle(c);
  return (uint16_t)(c->dp + offset);
}
static uint16_t read16(Cpu *c, uint16_t addr) {
  uint16_t low = c->read(c->mem, addr);
  sample_interrupt(c);
  return low | ((uint16_t)c->read(c->mem, (uint16_t)(addr + 1)) << 8);
}
static void store_dp(Cpu *c, uint8_t offset, bool byte) {
  uint16_t address = dp_address(c, offset);
  if(byte) sample_interrupt(c);
  c->write(c->mem, address, (uint8_t)c->a);
  if(!byte) {
    sample_interrupt(c);
    c->write(c->mem, (uint16_t)(address + 1), (uint8_t)(c->a >> 8));
  }
}
static void store_register8(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c);
  sample_interrupt(c);
  c->write(c->mem, ((uint32_t)c->db << 16) | address, (uint8_t)c->a);
}
static uint8_t pull(Cpu *c) {
  c->sp++;
  if(c->e) c->sp = 0x100u | (c->sp & 255u);
  return c->read(c->mem, c->sp);
}
static void return_long(Cpu *c) {
  idle(c); idle(c);
  uint16_t low = pull(c);
  uint16_t high = pull(c);
  c->pc = (uint16_t)((high << 8) + low + 1u);
  sample_interrupt(c);
  c->k = pull(c);
}
static void return_short(Cpu *c) {
  idle(c); idle(c);
  uint16_t low = pull(c), high = pull(c);
  c->pc = (uint16_t)((high << 8) + low + 1u);
  sample_interrupt(c); idle(c);
}
static void load_dp8(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  sample_interrupt(c); set_a8(c, c->read(c->mem, address));
}

/* Upload four cached scroll words, low byte then high byte to each PPU latch.
 * DP+$33/$35 are BG2 H/V; DP+$37/$39 are BG1 H/V. */
static bool scroll_step(Cpu *c, uint16_t pc) {
  unsigned off = pc - 0x82fb;
  if(!c->mf || (off != 40 && off % 5 != 0 && off % 5 != 2)) return false;
  static const uint16_t registers[] = {0x210f,0x210f,0x2110,0x2110,0x210d,0x210d,0x210e,0x210e};
  (void)fetch(c);
  if(off == 40) return_short(c);
  else if(off % 5 == 0) load_dp8(c, (uint8_t)(0x33 + off / 5));
  else store_register8(c, registers[off / 5]);
  return true;
}

/* Wait for auto-joypad completion, then update both ports:
 * pressed = held & ~previous; previous = held. */
static bool controller_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf) return false;
  bool byte = pc <= 0x82da || pc == 0x82fa;
  if(c->mf != byte) return false;
  switch(pc) {
    case 0x82d4: case 0x82d7: case 0x82d8: case 0x82da:
    case 0x82dc: case 0x82df: case 0x82e1: case 0x82e2:
    case 0x82e4: case 0x82e6: case 0x82e8: case 0x82ea:
    case 0x82ed: case 0x82ef: case 0x82f0: case 0x82f2:
    case 0x82f4: case 0x82f6: case 0x82f8: case 0x82fa: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x82d4:
      (void)fetch(c); (void)fetch(c); sample_interrupt(c);
      set_a8(c, c->read(c->mem, ((uint32_t)c->db << 16) | 0x4212)); break;
    case 0x82d7:
      implied(c); c->c = c->a & 1u; set_a8(c, (uint8_t)c->a >> 1); break;
    case 0x82d8:
      if(!c->c) sample_interrupt(c);
      (void)fetch(c);
      if(c->c) { sample_interrupt(c); idle(c); c->pc = 0x82d4; }
      break;
    case 0x82da: set_m(c, false); break;
    case 0x82f8: set_m(c, true); break;
    case 0x82dc: case 0x82ea: {
      (void)fetch(c); (void)fetch(c);
      uint32_t address = ((uint32_t)c->db << 16) | (pc == 0x82dc ? 0x4218 : 0x421a);
      uint16_t low = c->read(c->mem, address); sample_interrupt(c);
      c->a = low | ((uint16_t)c->read(c->mem, address + 1) << 8);
      zn(c, c->a, false); break;
    }
    case 0x82df: store_dp(c, 0x43, false); break;
    case 0x82ed: store_dp(c, 0x45, false); break;
    case 0x82e6: store_dp(c, 0x47, false); break;
    case 0x82f4: store_dp(c, 0x49, false); break;
    case 0x82e1: case 0x82ef: implied(c); c->x = c->a; zn(c, c->x, false); break;
    case 0x82e2: case 0x82f0: case 0x82e4: case 0x82f2: {
      bool exclusive = pc == 0x82e2 || pc == 0x82f0;
      uint8_t offset = pc >= 0x82f0 ? (exclusive ? 0x4d : 0x45) : (exclusive ? 0x4b : 0x43);
      uint16_t address = dp_address(c, offset), value = read16(c, address);
      c->a = exclusive ? c->a ^ value : c->a & value; zn(c, c->a, false); break;
    }
    case 0x82e8: case 0x82f6: {
      uint16_t address = dp_address(c, pc == 0x82e8 ? 0x4b : 0x4d);
      c->write(c->mem, address, (uint8_t)c->x); sample_interrupt(c);
      c->write(c->mem, (uint16_t)(address + 1), (uint8_t)(c->x >> 8)); break;
    }
    case 0x82fa: return_short(c); break;
  }
  return true;
}

static void add16(Cpu *c, uint16_t value) {
  uint32_t sum = (uint32_t)c->a + value + c->c;
  c->v = ((~(c->a ^ value) & (c->a ^ sum)) & 0x8000u) != 0;
  c->c = sum > 65535u;
  c->a = (uint16_t)sum;
  zn(c, c->a, false);
}

/* $00828F: clear forced blank. $008299: set forced blank.
 * Both preserve the brightness bits in the cached INIDISP byte at DP+$29. */
static bool display_step(Cpu *c, uint16_t pc) {
  if(!c->mf) return false;
  uint16_t offset = pc >= 0x8299 ? pc - 0x8299 : pc - 0x828f;
  if(offset != 0 && offset != 2 && offset != 4 && offset != 6 && offset != 9) return false;
  bool blank = pc >= 0x8299;
  (void)fetch(c);
  switch(offset) {
    case 0: {
      uint16_t address = dp_address(c, 0x29);
      sample_interrupt(c);
      set_a8(c, c->read(c->mem, address));
      break;
    }
    case 2:
      sample_interrupt(c); (void)fetch(c);
      set_a8(c, blank ? (uint8_t)(c->a | 0x80u) : (uint8_t)(c->a & 0x7fu));
      break;
    case 4: store_dp(c, 0x29, true); break;
    case 6: store_register8(c, 0x2100); break;
    case 9: return_long(c); break;
  }
  return true;
}

/* $0082A3/$0082B5: independent 16-bit LCG streams at DP+$2F/DP+$31.
 * Recovered transition: seed = (seed * 5 + 17) modulo 65536.
 * The original adds in two stages, so its carry/overflow flags must also be
 * retained. Decimal arithmetic and emulation-mode entries use the baseline. */
static bool rng_step(Cpu *c, uint16_t pc, DbzExecution *x) {
  if(c->e || c->d) return false;
  bool second = pc >= 0x82b5;
  uint16_t off = pc - (second ? 0x82b5 : 0x82a3);
  if(off != 0 && off != 2 && off != 4 && off != 5 && off != 6 &&
     off != 7 && off != 9 && off != 10 && off != 13 && off != 15 && off != 17) return false;
  if(off != 0 && off != 17 && c->mf) return false;
  if(off == 17 && !c->mf) return false;
  uint8_t seed_offset = second ? 0x31 : 0x2f;
  (void)fetch(c);
  switch(off) {
    case 0: set_m(c, false); x->rng_entries[second]++; break;
    case 2: {
      uint16_t addr = dp_address(c, seed_offset);
      c->a = read16(c, addr); zn(c, c->a, false); break;
    }
    case 4: case 5:
      implied(c); c->c = (c->a & 0x8000u) != 0;
      c->a = (uint16_t)(c->a << 1); zn(c, c->a, false); break;
    case 6: case 9: implied(c); c->c = false; break;
    case 7: {
      uint16_t addr = dp_address(c, seed_offset);
      uint16_t value = read16(c, addr); add16(c, value); break;
    }
    case 10:
      // Immediate word addressing advances PC before the operand bus reads.
      { uint16_t low = c->pc++; uint16_t high = c->pc++;
        (void)c->read(c->mem, ((uint32_t)c->k << 16) | low);
        sample_interrupt(c);
        (void)c->read(c->mem, ((uint32_t)c->k << 16) | high);
        add16(c, 17); }
      break;
    case 13: store_dp(c, seed_offset, false); break;
    case 15: set_m(c, true); break;
    case 17: return_long(c); break;
  }
  return true;
}

#include "native_video.inc"

/* Reset prefix $008000-$00800D. The following JSL and remaining initialization
 * stay in the reference CPU until reconstructed and verified separately. */
static bool reset_step(Cpu *c, uint16_t pc) {
  if(pc > 0x800b || (pc > 0x8004 && !c->mf)) return false;
  if(pc == 0x8005 || pc == 0x8007 || pc == 0x800a) return false;
  (void)fetch(c);
  switch(pc) {
    case 0x8000: implied(c); c->i = true; break;
    case 0x8001: implied(c); c->d = false; break;
    case 0x8002: implied(c); c->c = false; break;
    case 0x8003: {
      implied(c); bool previous_c = c->c; c->c = c->e; c->e = previous_c;
      if(c->e) { c->mf = c->xf = true; c->sp = 0x100u | (c->sp & 255u); }
      if(c->xf) { c->x &= 255u; c->y &= 255u; }
      break;
    }
    case 0x8004: set_m(c, true); break;
    case 0x8006: case 0x8009:
      sample_interrupt(c); (void)fetch(c); set_a8(c, 0); break;
    case 0x8008:
      c->a = (uint16_t)((c->a << 8) | (c->a >> 8)); zn(c, c->a, true);
      idle(c); sample_interrupt(c); idle(c); break;
    case 0x800b: store_register8(c, 0x4200); break;
    default: return false;
  }
  return true;
}

bool dbz_native_step(Cpu *c, DbzExecution *x) {
  uint8_t bank = (uint8_t)(c->k & 0x7fu);
  if(c->resetWanted || c->waiting || c->stopped || c->intWanted ||
     (bank != 0 && bank != 2 && bank != 4 && bank != 6))
    return false;
  bool done = false;
  if(bank == 6) {
    if(c->pc >= 0xf14d && c->pc <= 0xf1eb) {
      done = scroll_clamp_f14d_step(c, c->pc); x->scroll_steps += done;
    }
    return done;
  }
  if(bank == 4) {
    if(c->pc >= 0x85b6 && c->pc <= 0x85f0) {
      done = slot_alloc_85b6_step(c, c->pc); x->display_control_steps += done;
    }
    return done;
  }
  if(bank == 2) {
    if(c->pc >= 0xd812 && c->pc <= 0xd843) {
      done = scroll_snapshot_d812_step(c, c->pc); x->scroll_steps += done;
    }
    return done;
  }
  if(c->pc >= 0x8000 && c->pc <= 0x800d) {
    done = reset_step(c, c->pc); x->reset_steps += done;
  } else if(c->pc >= 0x828f && c->pc <= 0x82a2) {
    done = display_step(c, c->pc); x->display_steps += done;
  } else if(c->pc >= 0x82a3 && c->pc <= 0x82c6) {
    done = rng_step(c, c->pc, x); x->rng_steps += done;
  } else if(c->pc >= 0x82d4 && c->pc <= 0x82fa) {
    done = controller_step(c, c->pc); x->controller_steps += done;
  } else if(c->pc >= 0x82fb && c->pc <= 0x8323) {
    done = scroll_step(c, c->pc); x->scroll_steps += done;
  } else if(c->pc >= 0x8324 && c->pc <= 0x83cb) {
    done = frame_upload_step(c, c->pc); x->upload_steps += done;
  } else if(c->pc >= 0x86a0 && c->pc <= 0x86b5) {
    done = sprite_tail_step(c, c->pc); x->sprite_steps += done;
  } else if(c->pc >= 0x86b6 && c->pc <= 0x86c7) {
    done = palette_clear_step(c, c->pc); x->palette_steps += done;
  } else if((c->pc >= 0x8275 && c->pc <= 0x828e) || (c->pc >= 0x8460 && c->pc <= 0x8470)) {
    done = display_control_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x83e4 && c->pc <= 0x8401) {
    done = display_transition_init_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x8433 && c->pc <= 0x845f) {
    done = display_transition_finish_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x83cc && c->pc <= 0x83d9) {
    done = display_transition_dispatch_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x8471 && c->pc <= 0x849b) {
    done = mosaic_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x8402 && c->pc <= 0x8432) {
    done = transition_timing_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x86fc && c->pc <= 0x8710) {
    done = ppu_multiply_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x8711 && c->pc <= 0x8723) {
    done = vram_queue_find_step(c, c->pc); x->upload_steps += done;
  } else if(c->pc >= 0x8724 && c->pc <= 0x8731) {
    done = vram_queue_mark_step(c, c->pc); x->upload_steps += done;
  } else if(c->pc >= 0x8887 && c->pc <= 0x88ad) {
    done = vram_dma_setup_step(c, c->pc); x->upload_steps += done;
  } else if(c->pc >= 0x86c8 && c->pc <= 0x86fb) {
    done = attr_nibble_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x8732 && c->pc <= 0x877d) {
    done = vram_cpu_copy_step(c, c->pc); x->upload_steps += done;
  } else if(c->pc >= 0x88ae && c->pc <= 0x88c4) {
    done = pointer_preset_88ae_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0x88c5 && c->pc <= 0x88d7) {
    done = palette_copy_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0x88d8 && c->pc <= 0x88e9) {
    done = pointer_preset_88d8_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0x88ea && c->pc <= 0x8907) {
    done = pointer_resolve_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0x8908 && c->pc <= 0x8920) {
    done = palette_rom_copy_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0x8921 && c->pc <= 0x8953) {
    done = dual_palette_load_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0x89b8 && c->pc <= 0x8a06) {
    done = actor_accum_89b8_step(c, c->pc); x->display_control_steps += done;
  } else if((c->pc >= 0x89ac && c->pc <= 0x89b7) ||
            (c->pc >= 0x8ad6 && c->pc <= 0x8ae4) ||
            (c->pc >= 0x8af1 && c->pc <= 0x8b26)) {
    done = small_helper_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x8b28 && c->pc <= 0x8b51) {
    done = hdma_queue_wipe_step(c, c->pc); x->upload_steps += done;
  } else if(c->pc >= 0x8996 && c->pc <= 0x89ab) {
    done = wram_fill_ffff_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0x9b36 && c->pc <= 0x9b78) {
    done = vram_queue_enqueue_desc_step(c, c->pc); x->upload_steps += done;
  } else if(c->pc >= 0x9b79 && c->pc <= 0x9bbe) {
    done = vram_queue_enqueue_dp_step(c, c->pc); x->upload_steps += done;
  } else if(c->pc >= 0x9bbf && c->pc <= 0x9c4a) {
    done = vram_queue_enqueue_tiled_step(c, c->pc); x->upload_steps += done;
  } else if(c->pc >= 0x8954 && c->pc <= 0x8995) {
    done = palette_index_load_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0x8e26 && c->pc <= 0x8e75) {
    done = scene_pointer_build_step(c, c->pc); x->palette_steps += done;
  } else if((c->pc >= 0x8dfe && c->pc <= 0x8e25)) {
    done = scene_setup_8dfe_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0x8d2b && c->pc <= 0x8d4d) {
    done = mul_table_8d2b_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x8e76 && c->pc <= 0x8e93) {
    done = scene_path_8e76_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0x90c1 && c->pc <= 0x90e5) {
    done = scene_gfx_90c1_step(c, c->pc); x->upload_steps += done;
  } else if(c->pc >= 0x90e6 && c->pc <= 0x9101) {
    done = dma_vram_from_7e9000_step(c, c->pc); x->upload_steps += done;
  } else if(c->pc >= 0x946f && c->pc <= 0x9484) {
    done = palette_dd44_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0xc559 && c->pc <= 0xc5ec) {
    done = decompress_c559_step(c, c->pc); x->upload_steps += done;
  } else if(c->pc >= 0xc68c && c->pc <= 0xc706) {
    done = tile_rearrange_c68c_step(c, c->pc); x->upload_steps += done;
  }
  return done;
}

void cpu_runOpcode(Cpu *cpu) {
  if(!active || cpu != active->cpu) { lakesnes_cpu_runOpcode(cpu); return; }
  Snes *snes = cpu->mem;
  uint64_t before = snes->cycles;
  uint32_t pc = ((uint32_t)cpu->k << 16) | cpu->pc;
  active->recent_pc[active->recent_index++ & 31u] = pc;
  if(!cpu->resetWanted && !cpu->intWanted && !cpu->waiting && !cpu->stopped &&
      (pc & 0xffffu) >= 0x8000 && (pc >> 16 & 0x7fu) < 0x20)
    active->visited[((pc >> 16 & 0x1fu) * 32768u) + (pc & 0x7fffu)]++;
  if(active->enabled && dbz_native_step(cpu, active)) {
    active->native_steps++; active->cycles_native += snes->cycles - before;
  } else {
    lakesnes_cpu_runOpcode(cpu);
    active->interpreted_steps++; active->cycles_interpreted += snes->cycles - before;
  }
}

void dbz_write_execution(FILE *f, const DbzExecution *x) {
  unsigned unique = 0;
  for(unsigned i = 0; i < 0x100000; i++) unique += x->visited[i] != 0;
  fprintf(f, "\"native_steps\":%" PRIu64 ",\"interpreted_steps\":%" PRIu64
    ",\"native_master_cycles\":%" PRIu64 ",\"interpreted_master_cycles\":%" PRIu64
    ",\"reset_steps\":%" PRIu64 ",\"display_steps\":%" PRIu64 ",\"rng_steps\":%" PRIu64
    ",\"rng_entries\":[%" PRIu64 ",%" PRIu64 "],\"unique_executed_rom_offsets\":%u",
    x->native_steps, x->interpreted_steps, x->cycles_native, x->cycles_interpreted,
    x->reset_steps, x->display_steps, x->rng_steps, x->rng_entries[0], x->rng_entries[1], unique);
  fprintf(f, ",\"controller_steps\":%" PRIu64 ",\"scroll_steps\":%" PRIu64,
    x->controller_steps, x->scroll_steps);
  fprintf(f, ",\"upload_steps\":%" PRIu64, x->upload_steps);
  fprintf(f, ",\"sprite_steps\":%" PRIu64 ",\"palette_steps\":%" PRIu64, x->sprite_steps, x->palette_steps);
  fprintf(f, ",\"display_control_steps\":%" PRIu64, x->display_control_steps);
}

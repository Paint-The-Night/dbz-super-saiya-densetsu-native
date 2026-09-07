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
     (bank != 0 && bank != 1 && bank != 2 && bank != 3 && bank != 4 && bank != 6))
    return false;
  bool done = false;
  if(bank == 1) {
    if(c->pc >= 0xb61e && c->pc <= 0xb65c) {
      done = wram_fill_b61e_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0xb65d && c->pc <= 0xb674) {
      done = wram_tail_b65d_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0xb675 && c->pc <= 0xb6ab) {
      done = vram_seed_b67a_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0xb6ac && c->pc <= 0xb6be) {
      done = wram_fill_b6ac_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0xb6bf && c->pc <= 0xb6ec) {
      done = vram_seed_b6bf_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0xb6ed && c->pc <= 0xb6ff) {
      done = wram_fill_b6ed_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0xb700 && c->pc <= 0xb72a) {
      done = vram_seed_b700_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0xb72b && c->pc <= 0xb7b9) {
      done = table_dispatch_b72b_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0xf4b2 && c->pc <= 0xf53a) {
      done = actor_prep_f4b2_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf802 && c->pc <= 0xf83c) {
      done = actor_prep_f802_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf983 && c->pc <= 0xf99f) {
      done = actor_ptr_f983_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xfb2a && c->pc <= 0xfbb2) {
      done = actor_expand_fb2a_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xc535 && c->pc <= 0xc583) {
      done = flag_rewrite_c535_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xc9f9 && c->pc <= 0xcaca) {
      done = actor_gfx_c9f9_step(c, c->pc); x->upload_steps += done;
    }
    return done;
  }
  if(bank == 3) {
    if(c->pc >= 0xfb8d && c->pc <= 0xfbd9) {
      done = early_exit_fb8d_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xfbda && c->pc <= 0xfc32) {
      done = scene_entry_fbda_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xfc33 && c->pc <= 0xfc73) {
      done = scene_specialty_fc47_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xfc74 && c->pc <= 0xfc92) {
      done = scene_pred_fc74_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe32e && c->pc <= 0xe33f) {
      done = swap_epilogue_e32e_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe340 && c->pc <= 0xe36a) {
      done = early_gate_e340_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe36b && c->pc <= 0xe418) {
      done = mvn_restore_e36b_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xa6cb && c->pc <= 0xa6e0) {
      done = scene_gate_a6cb_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x9255 && c->pc <= 0x9266) {
      done = clear_1000_9255_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xef29 && c->pc <= 0xefd6) {
      done = scene_actor_ef29_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xefd7 && c->pc <= 0xf020) {
      done = clear_siblings_efd7_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe419 && c->pc <= 0xe460) {
      done = actor_backup_e419_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe461 && c->pc <= 0xe48f) {
      done = actor_restore_e461_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe4d1 && c->pc <= 0xe513) {
      done = actor_type_list_e4d1_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xbb46 && c->pc <= 0xbb86) {
      done = wipe_actor_bb46_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf021 && c->pc <= 0xf03f) {
      done = actor_scan_f021_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf06f && c->pc <= 0xf08c) {
      done = actor_type_f06f_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xfd64 && c->pc <= 0xfe0d) {
      done = scene_gate_fd64_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xfe0e && c->pc <= 0xfe29) {
      done = scene_snap_fe0e_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf375 && c->pc <= 0xf39d) {
      done = scene_gate_f375_step(c, c->pc); x->display_control_steps += done;
    }
    return done;
  }
  if(bank == 6) {
    if(c->pc >= 0xef11 && c->pc <= 0xef9a) {
      done = queue_fill_ef11_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0xf082 && c->pc <= 0xf14c) {
      done = tile_fetch_f089_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0xf14d && c->pc <= 0xf1eb) {
      done = scroll_clamp_f14d_step(c, c->pc); x->scroll_steps += done;
    } else if(c->pc >= 0xe837 && c->pc <= 0xe8ed) {
      done = scene_wipe_e837_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe8ee && c->pc <= 0xe941) {
      done = actor_attr_e8ee_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe942 && c->pc <= 0xe968) {
      done = wipe_gate_e942_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xead4 && c->pc <= 0xeb51) {
      done = palette_select_ead4_step(c, c->pc); x->palette_steps += done;
    } else if(c->pc >= 0xeb52 && c->pc <= 0xebc0) {
      done = gfx_select_eb52_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0xebc1 && c->pc <= 0xec09) {
      done = scroll_blit_ebc1_step(c, c->pc); x->upload_steps += done;
    }
    return done;
  }
  if(bank == 4) {
    if(c->pc >= 0x844a && c->pc <= 0x848f) {
      done = specialty_sync_844a_step(c, c->pc); x->display_control_steps += done;
    } else if((c->pc >= 0x8490 && c->pc <= 0x84b8) ||
              (c->pc >= 0x84b9 && c->pc <= 0x84e1)) {
      done = apu_boot_8490_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x84e2 && c->pc <= 0x8513) {
      done = apu_handshake_84e2_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x8514 && c->pc <= 0x85a0) {
      done = apu_command_8514_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x85a1 && c->pc <= 0x85b5) {
      done = specialty_cmd_85a1_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x85b6 && c->pc <= 0x85f0) {
      done = slot_alloc_85b6_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x85f1 && c->pc <= 0x8673) {
      done = apu_transfer_85f1_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x8674 && c->pc <= 0x869d) {
      done = frame_tail_8674_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x8938 && c->pc <= 0x8973) {
      done = vram_seed_8938_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0x89e2 && c->pc <= 0x8a4e) {
      done = mode7_ppu_89e2_step(c, c->pc); x->display_control_steps += done;
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
  } else if((c->pc >= 0x849c && c->pc <= 0x8586) ||
            (c->pc >= 0x85b9 && c->pc <= 0x85e6)) {
    done = oam_writeback_849c_step(c, c->pc); x->sprite_steps += done;
  } else if(c->pc >= 0x85e9 && c->pc <= 0x869d) {
    done = oam_writeback_85e9_step(c, c->pc); x->sprite_steps += done;
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
  } else if(c->pc >= 0x877e && c->pc <= 0x87e8) {
    done = specialty_table_877e_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x87e9 && c->pc <= 0x87f8) {
    done = specialty_wrap_87e9_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x87f9 && c->pc <= 0x8885) {
    done = specialty_gfx_87f9_step(c, c->pc); x->upload_steps += done;
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
  } else if((c->pc >= 0x8dfa && c->pc <= 0x8e25)) {
    done = scene_setup_8dfe_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0x8d2b && c->pc <= 0x8d4d) {
    done = mul_table_8d2b_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x8d4e && c->pc <= 0x8dab) {
    done = specialty_select_8d4e_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x8e76 && c->pc <= 0x8e93) {
    done = scene_path_8e76_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0x8b7a && c->pc <= 0x8c83) {
    done = scene_mode0_8b7a_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0x8c84 && c->pc <= 0x8d2a) {
    done = scene_cont_8c84_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0x8dac && c->pc <= 0x8de3) {
    done = actor_slot_walk_8dac_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0x8f46 && c->pc <= 0x8fb1) {
    done = scene_body_8f46_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0x8fb2 && c->pc <= 0x9082) {
    done = scene_cont_8fb2_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0x9083 && c->pc <= 0x90c0) {
    done = scene_cont_9083_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0x8e96 && c->pc <= 0x8f45) {
    done = scene_mode2_8e96_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0xa956 && c->pc <= 0xa960) {
    done = hblank_wait_a95b_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0xa961 && c->pc <= 0xa99a) {
    done = mode7_matrix_a961_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0xab00 && c->pc <= 0xab2b) {
    done = scroll_latch_ab00_step(c, c->pc); x->scroll_steps += done;
  } else if(c->pc >= 0xb960 && c->pc <= 0xb9a4) {
    done = mode7_hdma_b960_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x90c1 && c->pc <= 0x90e5) {
    done = scene_gfx_90c1_step(c, c->pc); x->upload_steps += done;
  } else if(c->pc >= 0x90e6 && c->pc <= 0x9101) {
    done = dma_vram_from_7e9000_step(c, c->pc); x->upload_steps += done;
  } else if(c->pc >= 0x946f && c->pc <= 0x9484) {
    done = palette_dd44_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0x9485 && c->pc <= 0x94e6) {
    done = ppu_preset_9485_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0x94e7 && c->pc <= 0x9518) {
    done = actor_clear_94e7_step(c, c->pc); x->palette_steps += done;
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

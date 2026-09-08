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
     (bank != 0 && bank != 1 && bank != 2 && bank != 3 && bank != 4 && bank != 5 && bank != 6 && bank != 0x1d))
    return false;
  bool done = false;
  if(bank == 1) {
    if(c->pc >= 0xc91f && c->pc <= 0xc959) {
      done = actor_graphics_c91f_step(c,c->pc); x->upload_steps += done;
    } else if(c->pc >= 0xb61e && c->pc <= 0xb65c) {
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
    } else if(c->pc >= 0xb55a && c->pc <= 0xb61d) {
      done = wram_pattern_b55a_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0xb72b && c->pc <= 0xb7b9) {
      done = table_dispatch_b72b_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0xf4b2 && c->pc <= 0xf53a) {
      done = actor_prep_f4b2_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf53b && c->pc <= 0xf65f) {
      done = actor_prep_f53b_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf660 && c->pc <= 0xf67a) {
      done = type_gate_f660_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf67b && c->pc <= 0xf691) {
      done = decompress_setup_f67b_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0xf692 && c->pc <= 0xf6db) {
      done = actor_fill_f692_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf719 && c->pc <= 0xf7f1) {
      done = actor_scale_f719_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf802 && c->pc <= 0xf83c) {
      done = actor_prep_f802_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf983 && c->pc <= 0xf99f) {
      done = actor_ptr_f983_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf9a0 && c->pc <= 0xf9ab) {
      done = actor_wrap_f9a0_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xfb2a && c->pc <= 0xfbb2) {
      done = actor_expand_fb2a_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc == 0xedf9) {
      done = actor_return_edf9_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc == 0x8481) {
      done = actor_jump_8481_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc == 0xcd1e) {
      done = actor_call_cd1e_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc == 0xec4c) {
      done = actor_call_ec4c_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc == 0xedf4) {
      done = actor_call_edf4_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc == 0xcd27 || c->pc == 0xcd4b || c->pc == 0xcd8a) {
      done = actor_call_cdb2_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xfa1a && c->pc <= 0xfa85) {
      done = actor_adjust_fa1a_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xfa86 && c->pc <= 0xfae8) {
      done = actor_copy_fa86_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xfae9 && c->pc <= 0xfb29) {
      done = actor_blit_fae9_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xfbbc && c->pc <= 0xfbff) {
      done = actor_optional_fbbc_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xfc00 && c->pc <= 0xfc22) {
      done = actor_scale_fc00_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xc535 && c->pc <= 0xc583) {
      done = flag_rewrite_c535_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xc885 && c->pc <= 0xc8aa) {
      done = palette_copy_c885_step(c, c->pc); x->palette_steps += done;
    } else if(c->pc >= 0xc8c8 && c->pc <= 0xc8e4) {
      done = palette_select_c8c8_step(c, c->pc); x->palette_steps += done;
    } else if(c->pc >= 0xc987 && c->pc <= 0xc9f8) {
      done = actor_sec_c987_step(c, c->pc); x->upload_steps += done;
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
    } else if(c->pc >= 0xc232 && c->pc <= 0xc24a) {
      done = actor_seed_c232_step(c, c->pc); x->display_control_steps += done;
    }
    return done;
  }

  if(bank == 5) {
    if(c->pc >= 0x9626 && c->pc <= 0x96d6) {
      done = oam_place_9626_step(c, c->pc); x->sprite_steps += done;
    } else if(c->pc >= 0x96d7 && c->pc <= 0x96e1) {
      done = index_add20_96d7_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x96e2 && c->pc <= 0x96ec) {
      done = index_add20_96e2_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x96ed && c->pc <= 0x9700) {
      done = gfx_vmain_96f3_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0xbe54 && c->pc <= 0xbeca) {
      done = actor_pack_be54_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xc029 && c->pc <= 0xc07a) {
      done = actor_stream_c029_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xbecb && c->pc <= 0xc010) {
      done = actor_stream_becb_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xc07b && c->pc <= 0xc0ec) {
      done = actor_palette_c07b_step(c, c->pc); x->palette_steps += done;
    } else if(c->pc >= 0x9aff && c->pc <= 0x9b36) {
      done = actor_becb_caller_9aff_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x9b37 && c->pc <= 0x9b88) {
      done = actor_prep_9b37_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x9b91 && c->pc <= 0x9c0a) {
      done = actor_adjust_9b91_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xb572 && c->pc <= 0xb581) {
      done = type_lookup_b572_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xb582 && c->pc <= 0xb593) {
      done = type_lookup_b582_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xb594 && c->pc <= 0xb5c3) {
      done = type_remap_b594_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xc0ed && c->pc <= 0xc115) {
      done = palette_push_c0ed_step(c, c->pc); x->palette_steps += done;
    } else if(c->pc >= 0xc116 && c->pc <= 0xc1bb) {
      done = palette_slot_c116_step(c, c->pc); x->palette_steps += done;
    } else if(c->pc >= 0xc1c8 && c->pc <= 0xc1ed) {
      done = actor_flag_c1c8_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xc1ee && c->pc <= 0xc272) {
      done = actor_timer_c1ee_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe24f && c->pc <= 0xe25f) {
      done = actor_hdl_e24f_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe260 && c->pc <= 0xe2bb) {
      done = actor_hdl_e260_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe2bd && c->pc <= 0xe2f3) {
      done = actor_hdl_e2bd_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe2f8 && c->pc <= 0xe31e) {
      done = actor_hdl_e2f8_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe31f && c->pc <= 0xe341) {
      done = actor_hdl_e31f_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe342 && c->pc <= 0xe38d) {
      done = actor_hdl_e342_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe38e && c->pc <= 0xe3cb) {
      done = oam_caller_e38e_step(c, c->pc); x->sprite_steps += done;
    } else if(c->pc >= 0xe3cc && c->pc <= 0xe3f1) {
      done = scale_helper_e3cc_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x86de && c->pc <= 0x871c) {
      done = actor_flag_wrap_86f5_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x8857 && c->pc <= 0x8886) {
      done = actor_c07b_caller_8857_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xb0b1 && c->pc <= 0xb0bd) {
      done = actor_dual_c07b_b0b1_step(c, c->pc); x->palette_steps += done;
    } else if(c->pc >= 0xb045 && c->pc <= 0xb0b0) {
      done = actor_seed_b045_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xb0be && c->pc <= 0xb0f8) {
      done = actor_seed_b0be_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xb0f9 && c->pc <= 0xb10f) {
      done = actor_seed_b0f9_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xb110 && c->pc <= 0xb11c) {
      done = actor_clear_b110_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x9768 && c->pc <= 0x97c8) {
      done = actor_clear_9768_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x97c9 && c->pc <= 0x9882) {
      done = actor_walk_97c9_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x8116 && c->pc <= 0x81ea) {
      done = actor_c07b_caller_8116_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x90cc && c->pc <= 0x9114) {
      done = actor_c07b_caller_90cc_step(c, c->pc); x->palette_steps += done;
    } else if(c->pc >= 0x9115 && c->pc <= 0x9200) {
      done = actor_cont_9115_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x9488 && c->pc <= 0x9490) {
      done = early_exit_9488_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xca10 && c->pc <= 0xca88) {
      done = actor_ca10_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xca8d && c->pc <= 0xcaf7) {
      done = actor_jt_ca90_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xdec3 && c->pc <= 0xdf5f) {
      done = actor_script_dec3_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xb00a && c->pc <= 0xb044) {
      done = actor_seed_b00a_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x9883 && c->pc <= 0x98c7) {
      done = scene_seed_9886_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xc41f && c->pc <= 0xc44f) {
      done = scene_index_c41f_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x90a5 && c->pc <= 0x90b2) {
      done = carry_pred_90a5_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xda64 && c->pc <= 0xda7b) {
      done = face_merge_da64_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xc9fa && c->pc <= 0xca06) {
      done = clear_1502_c9fa_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xdfe3 && c->pc <= 0xdff2) {
      done = gate_dfe3_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xc98f && c->pc <= 0xc9f9) {
      done = actor_init_c98f_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xb6b2 && c->pc <= 0xb6cd) {
      done = slot_select_b6b2_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xdf60 && c->pc <= 0xdfc1) {
      done = actor_face_df60_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe102 && c->pc <= 0xe116) {
      done = actor_dispatch_e102_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x93a9 && c->pc <= 0x9487) {
      done = actor_seed_93a9_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xa435 && c->pc <= 0xa52d) {
      done = actor_probe_a435_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xa52e && c->pc <= 0xa580) {
      done = actor_probe_a52e_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x95c9 && c->pc <= 0x95f5) {
      done = actor_face_95c9_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x95f6 && c->pc <= 0x9625) {
      done = actor_face_95f6_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x9491 && c->pc <= 0x94a0) {
      done = actor_remap_9491_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xa9f4 && c->pc <= 0xaa0a) {
      done = actor_bit_a9f4_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xb124 && c->pc <= 0xb130) {
      done = actor_seed_b124_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xbddc && c->pc <= 0xbe11) {
      done = actor_probe_bddc_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xd485 && c->pc <= 0xd520) {
      done = actor_op_d48f_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xd521 && c->pc <= 0xd571) {
      done = actor_op_d521_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf3aa && c->pc <= 0xf40f) {
      done = actor_hdl_f3aa_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf998 && c->pc <= 0xf9af) {
      done = clear_7f_f998_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf91e && c->pc <= 0xf951) {
      done = queue_seed_f91e_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf952 && c->pc <= 0xf997) {
      done = mirror_xor_f952_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xdd51 && c->pc <= 0xdd68) {
      done = actor_wrap_dd51_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xdd69 && c->pc <= 0xddd0) {
      done = actor_move_dd69_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xddd1 && c->pc <= 0xde25) {
      done = actor_helper_ddd1_step(c, c->pc); x->display_control_steps += done;
    } else if((c->pc >= 0xde26 && c->pc <= 0xde62) || (c->pc >= 0xdea1 && c->pc <= 0xdeb1)) {
      done = actor_helper_de26_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xdff3 && c->pc <= 0xe017) {
      done = actor_scale_dff3_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe018 && c->pc <= 0xe088) {
      done = actor_helper_e018_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf548 && c->pc <= 0xf54e) {
      done = actor_ora_f548_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf54f && c->pc <= 0xf684) {
      done = actor_hdl_f54f_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf685 && c->pc <= 0xf6a7) {
      done = queue_seed_f685_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf6a8 && c->pc <= 0xf6b8) {
      done = actor_adj_f6a8_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf6b9 && c->pc <= 0xf6ee) {
      done = actor_fill_f6b9_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf6ef && c->pc <= 0xf76a) {
      done = actor_hdl_f708_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf76b && c->pc <= 0xf82a) {
      done = actor_hdl_f76b_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xd572 && c->pc <= 0xd60e) {
      done = actor_op_d5b9_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xd60f && c->pc <= 0xd6d8) {
      done = actor_op_d60f_step(c, c->pc); x->display_control_steps += done;
    } else if((c->pc >= 0xd6d9 && c->pc <= 0xd6e9) || (c->pc >= 0xd6eb && c->pc <= 0xd7f3)) {
      done = actor_op_d743_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xd7f4 && c->pc <= 0xd81d) {
      done = actor_op_d7f4_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xd81e && c->pc <= 0xd848) {
      done = actor_op_d81e_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xd849 && c->pc <= 0xd857) {
      done = actor_op_d849_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xd858 && c->pc <= 0xd8d8) {
      done = actor_op_d873_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf410 && c->pc <= 0xf47f) {
      done = actor_hdl_f410_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xa581 && c->pc <= 0xa5f6) {
      done = actor_probe_a581_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xa5f7 && c->pc <= 0xa8e8) {
      done = actor_probe_a5f7_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xa8e9 && c->pc <= 0xa90c) {
      done = actor_exit_a8e9_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xa90d && c->pc <= 0xa93c) {
      done = actor_mask_a90d_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xa93d && c->pc <= 0xa960) {
      done = actor_index_a93d_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xb11d && c->pc <= 0xb123) {
      done = actor_seed_b11d_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xd8db && c->pc <= 0xda40) {
      done = actor_op_d8db_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xda45 && c->pc <= 0xda63) {
      done = actor_op_da45_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xda7c && c->pc <= 0xda8c) {
      done = actor_op_da7c_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xda8d && c->pc <= 0xda9b) {
      done = actor_op_da8d_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xda9c && c->pc <= 0xdb2a) {
      done = actor_op_da9c_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xd274 && c->pc <= 0xd305) {
      done = actor_op_d274_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xd306 && c->pc <= 0xd32f) {
      done = actor_op_d306_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xd330 && c->pc <= 0xd3d5) {
      done = actor_op_d330_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xd3d6 && c->pc <= 0xd484) {
      done = actor_op_d3d6_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xdb2b && c->pc <= 0xdc3a) {
      done = actor_op_db2b_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xdc3b && c->pc <= 0xdd50) {
      done = actor_op_dc3b_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe089 && c->pc <= 0xe0b0) {
      done = stream_ptr_e089_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf480 && c->pc <= 0xf547) {
      done = actor_hdl_f480_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf000 && c->pc <= 0xf069) {
      done = actor_setup_f000_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf06a && c->pc <= 0xf0e9) {
      done = actor_setup_f06a_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf0ea && c->pc <= 0xf118) {
      done = actor_setup_f0ea_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf119 && c->pc <= 0xf13a) {
      done = actor_hdl_f119_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf13b && c->pc <= 0xf164) {
      done = actor_hdl_f13b_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf167 && c->pc <= 0xf352) {
      done = actor_hdl_f167_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf353 && c->pc <= 0xf363) {
      done = actor_gate_f353_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf364 && c->pc <= 0xf385) {
      done = actor_scale_f364_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe446 && c->pc <= 0xe564) {
      done = actor_hdl_e446_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe565 && c->pc <= 0xe5a9) {
      done = actor_hdl_e565_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe5aa && c->pc <= 0xe61a) {
      done = actor_hdl_e5aa_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe61b && c->pc <= 0xe6b0) {
      done = actor_hdl_e61b_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe6b1 && c->pc <= 0xe6d5) {
      done = actor_hdl_e6b1_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe6d6 && c->pc <= 0xe7fe) {
      done = actor_hdl_e6d6_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe7ff && c->pc <= 0xe844) {
      done = actor_hdl_e7ff_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe845 && c->pc <= 0xe87e) {
      done = actor_hdl_e845_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe87f && c->pc <= 0xe8db) {
      done = actor_hdl_e87f_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe8dc && c->pc <= 0xe920) {
      done = actor_hdl_e8dc_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe921 && c->pc <= 0xe97a) {
      done = actor_hdl_e921_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe97b && c->pc <= 0xe9a1) {
      done = actor_hdl_e97b_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe9a2 && c->pc <= 0xe9d3) {
      done = actor_hdl_e9a2_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xe9d4 && c->pc <= 0xea51) {
      done = actor_hdl_e9d4_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xea52 && c->pc <= 0xea8b) {
      done = stream_prolog_ea52_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xea8c && c->pc <= 0xeacf) {
      done = stream_expand_eaa4_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xead0 && c->pc <= 0xeb30) {
      done = queue_stream_ead0_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xeb31 && c->pc <= 0xecd3) {
      done = actor_hdl_eb36_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xece4 && c->pc <= 0xed45) {
      done = actor_hdl_ece4_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xed46 && c->pc <= 0xed87) {
      done = actor_hdl_ed4a_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xed88 && c->pc <= 0xedb3) {
      done = actor_hdl_ed88_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xedb4 && c->pc <= 0xedfa) {
      done = actor_hdl_edb4_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xee01 && c->pc <= 0xee55) {
      done = actor_hdl_ee01_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xee5e && c->pc <= 0xeed3) {
      done = actor_hdl_ee5e_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xeed4 && c->pc <= 0xef1f) {
      done = actor_hdl_eed4_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xef26 && c->pc <= 0xefff) {
      done = actor_hdl_ef26_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf386 && c->pc <= 0xf3a1) {
      done = actor_adj_f386_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf82b && c->pc <= 0xf85d) {
      done = actor_setup_f82b_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf85e && c->pc <= 0xf885) {
      done = actor_clear_f85e_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf8a1 && c->pc <= 0xf8b1) {
      done = actor_type_f8a1_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf8b2 && c->pc <= 0xf90f) {
      done = tile_expand_f8b2_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0xf910 && c->pc <= 0xf91d) {
      done = queue_len_f910_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc == 0x9390) {
      done = actor_return_9390_step(c, c->pc); x->display_control_steps += done;
    }
    return done;
  }
  if(bank == 0x1d) {
    if(c->pc >= 0x8807 && c->pc <= 0x8842) {
      done = slot_seed_1d8807_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x8843 && c->pc <= 0x8905) {
      done = timer_walk_1d8843_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x8906 && c->pc <= 0x8967) {
      done = timer_leaf_1d8906_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x8a77 && c->pc <= 0x8a84) {
      done = slot_select_1d8a77_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x8a85 && c->pc <= 0x8aa1) {
      done = list_scan_1d8a85_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x8aa2 && c->pc <= 0x8ab2) {
      done = list_append_1d8aa2_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x8ab3 && c->pc <= 0x8abc) {
      done = list_clear_1d8ab3_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x8abd && c->pc <= 0x8b45) {
      done = flight_input_1d8abd_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x8b46 && c->pc <= 0x8b7b) {
      done = flight_cont_1d8b46_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x8b7c && c->pc <= 0x8bdb) {
      done = flight_post_1d8b7c_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x89d1 && c->pc <= 0x89f7) {
      done = timer_seed_1d89d1_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc >= 0x8583 && c->pc <= 0x85db) {
      done = face_queue_1d8583_step(c, c->pc); x->display_control_steps += done;
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
    if(c->pc >= 0x8282 && c->pc <= 0x82eb) {
      done = stream_unpack_8282_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0x83b2 && c->pc <= 0x8410) {
      done = bitplane_83b2_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0x8411 && c->pc <= 0x8449) {
      done = wram_copy_8411_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0x844a && c->pc <= 0x848f) {
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
    } else if(c->pc >= 0x871e && c->pc <= 0x8802) {
      done = frame_gfx_871e_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0x889e && c->pc <= 0x88c5) {
      done = wram_fill_889e_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0x8938 && c->pc <= 0x8973) {
      done = vram_seed_8938_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0x8974 && c->pc <= 0x89e1) {
      done = vram_queue_8974_step(c, c->pc); x->upload_steps += done;
    } else if(c->pc >= 0x89e2 && c->pc <= 0x8a4e) {
      done = mode7_ppu_89e2_step(c, c->pc); x->display_control_steps += done;
    } else if(c->pc == 0x8c4f) {
      done = audio_return_8c4f_step(c, c->pc); x->display_control_steps += done;
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
  } else if(c->pc >= 0x91b6 && c->pc <= 0x91bc) {
    done = scene_thunk_91b6_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x943f && c->pc <= 0x9443) {
    done = scene_thunk_943f_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x952b && c->pc <= 0x9534) {
    done = scene_thunk_952b_9530_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x95a9 && c->pc <= 0x95b6) {
    done = scene_thunk_95a9_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x9444 && c->pc <= 0x9458) {
    done = scene_thunk_9444_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x9459 && c->pc <= 0x946e) {
    done = scene_thunk_9459_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x97e9 && c->pc <= 0x97fc) {
    done = scene_seed_97e9_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc == 0x99c2) {
    done = scene_return_99c2_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc == 0x9735) {
    done = scene_return_9735_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc == 0x9ac8) {
    done = scene_call_9ac8_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x987a && c->pc <= 0x9883) {
    done = scene_branch_987a_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x991d && c->pc <= 0x9926) {
    done = scene_branch_991d_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc == 0x9af5) {
    done = scene_call_9af5_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc == 0x9afa) {
    done = scene_call_9afa_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc == 0xfcd8) {
    done = scene_call_fcd8_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x99c3 && c->pc <= 0x99cb) {
    done = scene_trampoline_99c3_step(c, c->pc); x->display_control_steps += done;
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
  } else if(c->pc >= 0x98d5 && c->pc <= 0x990e) {
    done = actor_slot_dispatch_98d5_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0x8e96 && c->pc <= 0x8f45) {
    done = scene_mode2_8e96_step(c, c->pc); x->palette_steps += done;
  } else if(c->pc >= 0xa956 && c->pc <= 0xa960) {
    done = hblank_wait_a95b_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0xa961 && c->pc <= 0xa99a) {
    done = mode7_matrix_a961_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0xa99b && c->pc <= 0xaad8) {
    done = mode7_epilogue_a99b_step(c, c->pc); x->display_control_steps += done;
  } else if(c->pc >= 0xae38 && c->pc <= 0xae69) {
    done = mode7_dma_ae38_step(c, c->pc); x->display_control_steps += done;
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
  } else if(c->pc >= 0xc5ed && c->pc <= 0xc68b) {
    done = decompress_c5ed_step(c, c->pc); x->upload_steps += done;
  } else if(c->pc >= 0xc68c && c->pc <= 0xc706) {
    done = tile_rearrange_c68c_step(c, c->pc); x->upload_steps += done;
  } else if(c->pc >= 0xc707 && c->pc <= 0xc781) {
    done = tile_rearrange_c707_step(c, c->pc); x->upload_steps += done;
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

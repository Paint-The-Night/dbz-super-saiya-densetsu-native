/* Included after native.c's bus helpers. Fixed-address reconstruction of the
 * frame upload routine, not an instruction decoder. Original region 8324-83CB.
 * OAM shadow: 0400..061F. Palette shadow: 0200..03FF, dirty flag 0715.
 * VRAM queue: 8-byte entries at 0800; bit 7 of byte 0 means occupied.
 * Entry layout: VMAIN, VMADD word, source word, source bank, length word.
 */
static void ldy_dp16(Cpu *c, uint8_t offset);
static void branch_to(Cpu *c, bool take, uint16_t target) {
  if(!take) sample_interrupt(c);
  (void)fetch(c);
  if(take) { sample_interrupt(c); idle(c); c->pc = target; }
}
static uint32_t video_address(Cpu *c, uint16_t base, bool indexed) {
  (void)fetch(c); (void)fetch(c);
  if(indexed) idle(c); // these routines require 16-bit X
  return (((uint32_t)c->db << 16) + base + (indexed ? c->x : 0)) & 0xffffffu;
}
static void video_store(Cpu *c, uint16_t base, bool indexed, uint16_t value, bool byte) {
  uint32_t address = video_address(c, base, indexed);
  if(byte) sample_interrupt(c);
  c->write(c->mem, address, (uint8_t)value);
  if(!byte) { sample_interrupt(c); c->write(c->mem, (address + 1) & 0xffffffu, (uint8_t)(value >> 8)); }
}
static void video_load(Cpu *c, uint16_t base, bool indexed) {
  uint32_t address = video_address(c, base, indexed);
  if(c->mf) { sample_interrupt(c); set_a8(c, c->read(c->mem, address)); }
  else {
    uint16_t low = c->read(c->mem, address); sample_interrupt(c);
    c->a = low | ((uint16_t)c->read(c->mem, (address + 1) & 0xffffffu) << 8);
    zn(c, c->a, false);
  }
}
static void index_constant(Cpu *c, bool y, uint16_t value) {
  uint16_t low = c->pc++, high = c->pc++;
  (void)c->read(c->mem, ((uint32_t)c->k << 16) | low); sample_interrupt(c);
  (void)c->read(c->mem, ((uint32_t)c->k << 16) | high);
  if(y) c->y = value; else c->x = value;
  zn(c, value, false);
}
static void byte_constant(Cpu *c, uint8_t value) {
  sample_interrupt(c); (void)fetch(c); set_a8(c, value);
}
static void xba_native(Cpu *c) {
  uint8_t low = (uint8_t)c->a, high = (uint8_t)(c->a >> 8);
  c->a = (uint16_t)((low << 8) | high); zn(c, high, true);
  idle(c); sample_interrupt(c); idle(c);
}
static void and_constant8(Cpu *c, uint8_t value) {
  sample_interrupt(c); (void)fetch(c); set_a8(c, (uint8_t)c->a & value);
}
static void ora_constant8(Cpu *c, uint8_t value) {
  sample_interrupt(c); (void)fetch(c); set_a8(c, (uint8_t)c->a | value);
}
static bool display_control_step(Cpu *c, uint16_t pc) {
  if(c->e || !c->mf) return false;
  bool brightness = pc >= 0x8460;
  bool irq_enable = pc >= 0x8275 && pc <= 0x8281;
  bool irq_disable = pc >= 0x8282 && pc <= 0x828e;
  if(brightness && pc > 0x8470) return false;
  if(!brightness && !irq_enable && !irq_disable) return false;
  static const uint16_t off_brightness[] = {0x8460,0x8463,0x8465,0x8468,0x846b,0x846d,0x8470};
  static const uint16_t off_disable[] = {0x8282,0x8284,0x8285,0x8287,0x8289,0x828b,0x828e};
  static const uint16_t off_enable[] = {0x8275,0x8277,0x8278,0x827a,0x827c,0x827e,0x8281};
  const uint16_t *list = brightness ? off_brightness : (irq_enable ? off_enable : off_disable);
  unsigned count = 7;
  bool valid = false; for(unsigned i=0;i<count;i++) valid |= pc == list[i];
  if(!valid) return false;
  (void)fetch(c);
  switch(pc) {
    case 0x8275: case 0x8282: byte_constant(c, 0); break;
    case 0x8460: video_load(c, 0x0716, false); break;
    case 0x8277: case 0x8284: xba_native(c); break;
    case 0x8278: case 0x8285: load_dp8(c, 0x28); break;
    case 0x827a: ora_constant8(c, 0x81); break;
    case 0x8287: and_constant8(c, 0x0e); break;
    case 0x827c: case 0x8289: store_dp(c, 0x28, true); break;
    case 0x827e: case 0x828b: store_register8(c, 0x4200); break;
    case 0x8463: and_constant8(c, 0x1f); break;
    case 0x8465: store_register8(c, 0x0719); break;
    case 0x8468: video_load(c, 0x0718, false); break;
    case 0x846b: store_dp(c, 0x29, true); break;
    case 0x846d: store_register8(c, 0x2100); break;
    case 0x8281: case 0x828e: case 0x8470: return_long(c); break;
  }
  return true;
}
static bool display_transition_init_step(Cpu *c, uint16_t pc) {
  if(c->e || !c->mf) return false;
  switch(pc) {
    case 0x83e4: case 0x83e7: case 0x83e9: case 0x83ec: case 0x83ee:
    case 0x83f0: case 0x83f3: case 0x83f6: case 0x83f8: case 0x83fa:
    case 0x83fc: case 0x83fe: case 0x8401: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x83e4: video_load(c, 0x0716, false); break;
    case 0x83e7: and_constant8(c, 0x1f); break;
    case 0x83e9: store_register8(c, 0x0719); break;
    case 0x83ec: load_dp8(c, 0x29); break;
    case 0x83ee: and_constant8(c, 0x0f); break;
    case 0x83f0: store_register8(c, 0x0718); break;
    case 0x83f3: video_load(c, 0x0716, false); break;
    case 0x83f6: branch_to(c, c->n, 0x83fc); break;
    case 0x83f8: byte_constant(c, 1); break;
    case 0x83fa: branch_to(c, true, 0x83fe); break;
    case 0x83fc: byte_constant(c, 2); break;
    case 0x83fe: store_register8(c, 0x071a); break;
    case 0x8401: return_long(c); break;
  }
  return true;
}
static void stz_abs8(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c); sample_interrupt(c);
  c->write(c->mem, ((uint32_t)c->db << 16) | address, 0);
}
static void store_y_abs16(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c); sample_interrupt(c);
  c->write(c->mem, ((uint32_t)c->db << 16) | address, (uint8_t)c->y);
  sample_interrupt(c);
  c->write(c->mem, ((uint32_t)c->db << 16) | ((address + 1) & 0xffffu), (uint8_t)(c->y >> 8));
}
static void adc16_constant(Cpu *c, uint16_t value) {
  uint16_t old = c->a; unsigned carry = c->c ? 1u : 0u;
  (void)fetch(c); sample_interrupt(c); (void)fetch(c);
  unsigned result = (unsigned)old + value + carry; uint16_t out = (uint16_t)result;
  c->c = result > 0xffffu;
  c->v = (uint16_t)(~(old ^ value) & (old ^ out) & 0x8000u) != 0;
  c->a = out; zn(c, out, false);
}
static void sbc16_constant(Cpu *c, uint16_t value) {
  uint16_t old = c->a;
  (void)fetch(c); sample_interrupt(c); (void)fetch(c);
  unsigned result = (unsigned)old - value - (c->c ? 0u : 1u);
  uint16_t out = (uint16_t)result;
  c->v = ((old ^ out) & (old ^ value) & 0x8000u) != 0;
  c->c = result < 0x10000u;
  c->a = out; zn(c, out, false);
}
static void cpy16_constant(Cpu *c, uint16_t value) {
  uint16_t low = c->pc++, high = c->pc++;
  (void)c->read(c->mem, ((uint32_t)c->k << 16) | low); sample_interrupt(c);
  (void)c->read(c->mem, ((uint32_t)c->k << 16) | high);
  c->c = c->y >= value; zn(c, (uint16_t)(c->y - value), false);
}
static void store_a_abs_y8(Cpu *c, uint16_t base) {
  (void)fetch(c); (void)fetch(c); idle(c); sample_interrupt(c);
  c->write(c->mem, (((uint32_t)c->db << 16) + base + c->y) & 0xffffffu, (uint8_t)c->a);
}
static void load_a_abs_y8(Cpu *c, uint16_t base) {
  (void)fetch(c); (void)fetch(c); idle(c); // 16-bit Y always pays the index cycle
  sample_interrupt(c); set_a8(c, c->read(c->mem, (((uint32_t)c->db << 16) + base + c->y) & 0xffffffu));
}

static void load_a_abs_y16(Cpu *c, uint16_t base) {
  (void)fetch(c); (void)fetch(c); idle(c);
  uint32_t low = (((uint32_t)c->db << 16) + base + c->y) & 0xffffffu;
  uint16_t value = c->read(c->mem, low); sample_interrupt(c);
  c->a = value | ((uint16_t)c->read(c->mem, (low + 1) & 0xffffffu) << 8);
  zn(c, c->a, false);
}
static void load_a_long_x8(Cpu *c, uint32_t base) {
  (void)fetch(c); (void)fetch(c); (void)fetch(c);
  sample_interrupt(c);
  set_a8(c, c->read(c->mem, (base + c->x) & 0xffffffu));
}
static bool small_helper_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf) return false;
  if((pc==0x8af1 || pc==0x8afc) && !c->mf) return false;
  if((pc==0x8af9 || pc==0x8b04) && c->mf) return false;
  if((pc==0x8af3 || pc==0x8af4 || pc==0x8af5 || pc==0x8af8 || pc==0x8aff || pc==0x8b00 || pc==0x8b03) && c->mf) return false;
  if((pc==0x8b15 || pc==0x8b1a || pc==0x8b1d || pc==0x8b1e || pc==0x8b1f || pc==0x8b20 ||
      pc==0x8b21 || pc==0x8b24 || pc==0x8b26 || pc==0x8ad6 || pc==0x8ad8 || pc==0x8ad9 ||
      pc==0x8adc || pc==0x8add || pc==0x8ae1 || pc==0x8ae4) && !c->mf) return false;
  if(pc==0x8b18 && c->mf==false) return false;
  switch(pc) {
    case 0x89ac: case 0x89af: case 0x89b1: case 0x89b4: case 0x89b7:
    case 0x8af1: case 0x8af3: case 0x8af4: case 0x8af5: case 0x8af8: case 0x8af9: case 0x8afb:
    case 0x8afc: case 0x8afe: case 0x8aff: case 0x8b00: case 0x8b03: case 0x8b04: case 0x8b06:
    case 0x8b07: case 0x8b08: case 0x8b09: case 0x8b0a: case 0x8b0b: case 0x8b0c: case 0x8b0d:
    case 0x8b0e: case 0x8b0f: case 0x8b10: case 0x8b11: case 0x8b12: case 0x8b13: case 0x8b14:
    case 0x8b15: case 0x8b18: case 0x8b1a: case 0x8b1d: case 0x8b1e: case 0x8b1f: case 0x8b20:
    case 0x8b21: case 0x8b24: case 0x8b26:
    case 0x8ad6: case 0x8ad8: case 0x8ad9: case 0x8adc: case 0x8add: case 0x8ae1: case 0x8ae4: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x89ac: store_y_abs16(c, 0x072c); break;
    case 0x89af: byte_constant(c, 0x80); break;
    case 0x89b1: store_register8(c, 0x071b); break;
    case 0x89b4: stz_abs8(c, 0x072a); break;
    case 0x89b7: return_long(c); break;
    case 0x8af1: set_m(c, false); break;
    case 0x8af3: implied(c); c->a = c->y; zn(c, c->a, false); break;
    case 0x8af4: implied(c); c->c = false; break;
    case 0x8af5: adc16_constant(c, 0x0020); break;
    case 0x8af8: implied(c); c->y = c->a; zn(c, c->y, false); break;
    case 0x8af9: set_m(c, true); break;
    case 0x8afb: return_long(c); break;
    case 0x8afc: set_m(c, false); break;
    case 0x8afe: implied(c); c->a = c->x; zn(c, c->a, false); break;
    case 0x8aff: implied(c); c->c = false; break;
    case 0x8b00: adc16_constant(c, 0x0020); break;
    case 0x8b03: implied(c); c->x = c->a; zn(c, c->x, false); break;
    case 0x8b04: set_m(c, true); break;
    case 0x8b06: return_long(c); break;
    case 0x8b07: case 0x8b08: case 0x8b09: case 0x8b0a: case 0x8b0b: case 0x8b0c:
      implied(c); c->y++; zn(c,c->y,false); break;
    case 0x8b0e: case 0x8b0f: case 0x8b10: case 0x8b11: case 0x8b12: case 0x8b13:
      implied(c); c->x++; zn(c,c->x,false); break;
    case 0x8b0d: case 0x8b14: return_long(c); break;
    case 0x8b15: index_constant(c, true, 0); break;
    case 0x8b18: byte_constant(c, 0); break;
    case 0x8b1a: store_a_abs_y8(c, 0x0d00); break;
    case 0x8b1d: case 0x8b1e: case 0x8b1f: case 0x8b20:
      implied(c); c->y++; zn(c,c->y,false); break;
    case 0x8b21: cpy16_constant(c, 0x0040); break;
    case 0x8b24: branch_to(c, !c->c, 0x8b1a); break;
    case 0x8b26: return_long(c); break;
    case 0x8ad6: byte_constant(c, 0); break;
    case 0x8ad8: xba_native(c); break;
    case 0x8ad9: load_a_abs_y8(c, 0x0b1c); break;
    case 0x8adc: implied(c); c->x = c->a; zn(c, c->x, false); break;
    case 0x8add: load_a_long_x8(c, 0x008ae5); break;
    case 0x8ae1: store_a_abs_y8(c, 0x0b01); break;
    case 0x8ae4: return_long(c); break;
  }
  return true;
}
static bool display_transition_finish_step(Cpu *c, uint16_t pc) {
  if(c->e || !c->mf) return false;
  switch(pc) {
    case 0x8433: case 0x8436: case 0x8438: case 0x843b: case 0x843d:
    case 0x843f: case 0x8441: case 0x8443: case 0x8446: case 0x8448:
    case 0x844a: case 0x844c: case 0x844e: case 0x8451: case 0x8453:
    case 0x8456: case 0x8459: case 0x845c: case 0x845f: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8433: {
      uint32_t address=video_address(c,0x0719,false);uint8_t value=c->read(c->mem,address);
      idle(c);sample_interrupt(c);value--;c->write(c->mem,address,value);zn(c,value,true);break;
    }
    case 0x8436: branch_to(c, !c->n, 0x845f); break;
    case 0x8438: video_load(c,0x0716,false); break;
    case 0x843b: branch_to(c,c->n,0x8443); break;
    case 0x843d: load_dp8(c,0x27); break;
    case 0x843f: store_dp(c,0x25,true); break;
    case 0x8441: branch_to(c,true,0x8451); break;
    case 0x8443: video_load(c,0x0717,false); break;
    case 0x8446: branch_to(c,c->n,0x844e); break;
    case 0x8448: load_dp8(c,0x24); break;
    case 0x844a: ora_constant8(c,0x80); break;
    case 0x844c: store_dp(c,0x24,true); break;
    case 0x844e: stz_abs8(c,0x0717); break;
    case 0x8451: byte_constant(c,0); break;
    case 0x8453: store_register8(c,0x0d8c); break;
    case 0x8456: store_register8(c,0x0719); break;
    case 0x8459: store_register8(c,0x0716); break;
    case 0x845c: store_register8(c,0x071a); break;
    case 0x845f: return_long(c); break;
  }
  return true;
}
static void asl_a8(Cpu *c) {
  implied(c); c->c=((uint8_t)c->a&0x80u)!=0; set_a8(c,(uint8_t)c->a<<1);
}
static void tax8(Cpu *c) { implied(c); c->x=(uint8_t)c->a; zn(c,c->x,true); }
static bool display_transition_dispatch_step(Cpu *c, uint16_t pc) {
  if(c->e || !c->mf || (pc != 0x83cc && pc != 0x83cf && pc != 0x83d1 &&
      pc != 0x83d3 && pc != 0x83d4 && pc != 0x83d7 && pc != 0x83d8 && pc != 0x83d9)) return false;
  (void)fetch(c);
  switch(pc) {
    case 0x83cc: video_load(c,0x0716,false); break;
    case 0x83cf: branch_to(c,c->z,0x8401); break;
    case 0x83d1: byte_constant(c,0); break;
    case 0x83d3: xba_native(c); break;
    case 0x83d4: video_load(c,0x071a,false); break;
    case 0x83d7: asl_a8(c); break;
    case 0x83d8: tax8(c); break;
    case 0x83d9: {
      (void)fetch(c); (void)fetch(c); idle(c);
      uint16_t table=(uint16_t)(0x83dc+c->x);
      uint16_t low=c->read(c->mem,((uint32_t)c->k<<16)|table); sample_interrupt(c);
      c->pc=(uint16_t)(low|((uint16_t)c->read(c->mem,((uint32_t)c->k<<16)|((table+1)&0xffff))<<8));
      break;
    }
  }
  return true;
}
static void inc_a8(Cpu *c) { implied(c); set_a8(c,(uint8_t)c->a+1); }
static void ora_dp8(Cpu *c, uint8_t offset) {
  uint16_t address=dp_address(c,offset); sample_interrupt(c);
  set_a8(c,(uint8_t)c->a|c->read(c->mem,address));
}
static void sbc8_value(Cpu *c, uint8_t value) {
  uint8_t old=(uint8_t)c->a;unsigned borrow=c->c?0:1;unsigned result=(unsigned)old-value-borrow;
  uint8_t out=(uint8_t)result;c->v=((old^out)&(old^value)&0x80u)!=0;c->c=result<0x100u;
  set_a8(c,out);
}
static void sbc_dp8(Cpu *c, uint8_t offset) {
  uint16_t address=dp_address(c,offset);sample_interrupt(c);sbc8_value(c,c->read(c->mem,address));
}
static bool mosaic_step(Cpu *c, uint16_t pc) {
  if(c->e || c->d || !c->mf) return false;
  switch(pc) {
    case 0x8471: case 0x8474: case 0x8476: case 0x8478: case 0x847b:
    case 0x847c: case 0x847e: case 0x8481: case 0x8483: case 0x8485:
    case 0x8487: case 0x8489: case 0x848b: case 0x848d: case 0x848e:
    case 0x8490: case 0x8492: case 0x8493: case 0x8494: case 0x8495:
    case 0x8496: case 0x8498: case 0x849b: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8471: video_load(c,0x0716,false); break;
    case 0x8474: and_constant8(c,0x20); break;
    case 0x8476: branch_to(c,c->z,0x849b); break;
    case 0x8478: video_load(c,0x0718,false); break;
    case 0x847b: inc_a8(c); break;
    case 0x847c: store_dp(c,0x00,true); break;
    case 0x847e: video_load(c,0x0d8c,false); break;
    case 0x8481: branch_to(c,!c->n,0x8487); break;
    case 0x8483: and_constant8(c,0x0f); break;
    case 0x8485: branch_to(c,true,0x8489); break;
    case 0x8487: byte_constant(c,0x0f); break;
    case 0x8489: store_dp(c,0x10,true); break;
    case 0x848b: byte_constant(c,0); break;
    case 0x848d: implied(c); c->c=true; break;
    case 0x848e: sbc_dp8(c,0x00); break;
    case 0x8490: and_constant8(c,0x0f); break;
    case 0x8492: case 0x8493: case 0x8494: case 0x8495: asl_a8(c); break;
    case 0x8496: ora_dp8(c,0x10); break;
    case 0x8498: store_register8(c,0x2106); break;
    case 0x849b: return_long(c); break;
  }
  return true;
}
static void push_byte_native(Cpu *c, uint8_t value) { c->write(c->mem,c->sp,value); c->sp--; }
static void jsl_8460(Cpu *c) {
  uint16_t low=c->pc++,high=c->pc++;
  (void)c->read(c->mem,((uint32_t)c->k<<16)|low);sample_interrupt(c);
  (void)c->read(c->mem,((uint32_t)c->k<<16)|high);
  push_byte_native(c,c->k);idle(c);uint8_t bank=fetch(c);
  push_byte_native(c,(uint8_t)((c->pc-1)>>8));sample_interrupt(c);
  push_byte_native(c,(uint8_t)(c->pc-1));c->pc=0x8460;c->k=bank;
}
static void inc_abs8_native(Cpu *c,uint16_t address) {
  uint32_t a=video_address(c,address,false);uint8_t value=c->read(c->mem,a);idle(c);sample_interrupt(c);
  value++;c->write(c->mem,a,value);zn(c,value,true);
}
static void dec_abs8_native(Cpu *c,uint16_t address) {
  uint32_t a=video_address(c,address,false);uint8_t value=c->read(c->mem,a);idle(c);sample_interrupt(c);
  value--;c->write(c->mem,a,value);zn(c,value,true);
}
static void cmp_constant8(Cpu *c,uint8_t value) {
  sample_interrupt(c);(void)fetch(c);uint8_t result=(uint8_t)c->a-value;c->c=(uint8_t)c->a>=value;zn(c,result,true);
}
static bool transition_timing_step(Cpu *c,uint16_t pc) {
  if(c->e||c->d||!c->mf) return false;
  switch(pc) {
    case 0x8402:case 0x8405:case 0x8407:case 0x840a:case 0x840e:case 0x8411:case 0x8413:case 0x8415:
    case 0x8417:case 0x841a:case 0x841c:case 0x841f:case 0x8423:case 0x8426:case 0x8428:case 0x842a:
    case 0x842d:case 0x842f:case 0x8432: break;
    default:return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8402:case 0x8417: dec_abs8_native(c,0x0719);break;
    case 0x8405:case 0x841a: branch_to(c,!c->z,0x8432);break;
    case 0x8407: inc_abs8_native(c,0x0718);break;
    case 0x840a:case 0x841f: jsl_8460(c);break;
    case 0x840e:case 0x8423: video_load(c,0x0718,false);break;
    case 0x8411: cmp_constant8(c,0x0f);break;
    case 0x8413: branch_to(c,!c->c,0x8432);break;
    case 0x8415: branch_to(c,true,0x8428);break;
    case 0x841c: dec_abs8_native(c,0x0718);break;
    case 0x8426: branch_to(c,!c->z,0x8432);break;
    case 0x8428: byte_constant(c,3);break;
    case 0x842a: store_register8(c,0x071a);break;
    case 0x842d: byte_constant(c,0x10);break;
    case 0x842f: store_register8(c,0x0719);break;
    case 0x8432: return_long(c);break;
  }
  return true;
}
/* Hide every unused OAM sprite by setting its Y coordinate to F0. DP+$55 is
 * the byte offset immediately following the last used 4-byte sprite record. */
static bool sprite_tail_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || !c->mf) return false;
  switch(pc) {
    case 0x86a0: case 0x86a2: case 0x86a5: case 0x86a7: case 0x86a9:
    case 0x86ac: case 0x86ad: case 0x86ae: case 0x86af: case 0x86b0:
    case 0x86b3: case 0x86b5: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x86a0: {
      uint16_t address = dp_address(c, 0x55); c->x = read16(c, address); zn(c, c->x, false); break;
    }
    case 0x86a2: case 0x86b0: {
      uint16_t low = c->pc++, high = c->pc++;
      (void)c->read(c->mem, ((uint32_t)c->k << 16) | low); sample_interrupt(c);
      (void)c->read(c->mem, ((uint32_t)c->k << 16) | high);
      c->c = c->x >= 0x200; zn(c, (uint16_t)(c->x - 0x200), false); break;
    }
    case 0x86a5: branch_to(c, c->z, 0x86b5); break;
    case 0x86a7: byte_constant(c, 0xf0); break;
    case 0x86a9: video_store(c, 0x0401, true, c->a, true); break;
    case 0x86ac: case 0x86ad: case 0x86ae: case 0x86af:
      implied(c); c->x++; zn(c, c->x, false); break;
    case 0x86b3: branch_to(c, !c->c, 0x86a7); break;
    case 0x86b5: return_long(c); break;
  }
  return true;
}
/* Clear the 256-color palette shadow as 256 descending word writes. */
static bool palette_clear_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf) return false;
  bool byte = pc == 0x86b6 || pc == 0x86b9 || pc == 0x86c7;
  if(c->mf != byte) return false;
  switch(pc) {
    case 0x86b6: case 0x86b9: case 0x86bb: case 0x86be: case 0x86c1:
    case 0x86c2: case 0x86c3: case 0x86c5: case 0x86c7: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x86b6: index_constant(c, false, 0x01fe); break;
    case 0x86b9: set_m(c, false); break;
    case 0x86bb: {
      uint16_t low = c->pc++, high = c->pc++;
      (void)c->read(c->mem, ((uint32_t)c->k << 16) | low); sample_interrupt(c);
      (void)c->read(c->mem, ((uint32_t)c->k << 16) | high);
      c->a = 0; zn(c, c->a, false); break;
    }
    case 0x86be: video_store(c, 0x0200, true, c->a, false); break;
    case 0x86c1: case 0x86c2: implied(c); c->x--; zn(c, c->x, false); break;
    case 0x86c3: branch_to(c, !c->n, 0x86bb); break;
    case 0x86c5: set_m(c, true); break;
    case 0x86c7: return_long(c); break;
  }
  return true;
}

static void load_dp_y16(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  c->y = read16(c, address); zn(c, c->y, false);
}
static void store_dp_y16(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  c->write(c->mem, address, (uint8_t)c->y); sample_interrupt(c);
  c->write(c->mem, (uint16_t)(address + 1), (uint8_t)(c->y >> 8));
}
static void load_y_abs16(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c);
  uint32_t low = ((uint32_t)c->db << 16) | address;
  uint16_t value = c->read(c->mem, low); sample_interrupt(c);
  value |= (uint16_t)c->read(c->mem, (low + 1) & 0xffffffu) << 8;
  c->y = value; zn(c, c->y, false);
}
static void stz_abs16(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c);
  uint32_t low = ((uint32_t)c->db << 16) | address;
  c->write(c->mem, low, 0); sample_interrupt(c);
  c->write(c->mem, (low + 1) & 0xffffffu, 0);
}
static void store_a_abs16(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c);
  uint32_t low = ((uint32_t)c->db << 16) | address;
  c->write(c->mem, low, (uint8_t)c->a); sample_interrupt(c);
  c->write(c->mem, (low + 1) & 0xffffffu, (uint8_t)(c->a >> 8));
}

static void stx_abs16(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c);
  uint32_t low = ((uint32_t)c->db << 16) | address;
  c->write(c->mem, low, (uint8_t)c->x); sample_interrupt(c);
  c->write(c->mem, (low + 1) & 0xffffffu, (uint8_t)(c->x >> 8));
}
static void cpx_abs16(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c);
  uint32_t low = ((uint32_t)c->db << 16) | address;
  uint16_t value = c->read(c->mem, low); sample_interrupt(c);
  value |= (uint16_t)c->read(c->mem, (low + 1) & 0xffffffu) << 8;
  c->c = c->x >= value; zn(c, (uint16_t)(c->x - value), false);
}
static void cmp_abs16(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c);
  uint32_t low = ((uint32_t)c->db << 16) | address;
  uint16_t value = c->read(c->mem, low); sample_interrupt(c);
  value |= (uint16_t)c->read(c->mem, (low + 1) & 0xffffffu) << 8;
  c->c = c->a >= value; zn(c, (uint16_t)(c->a - value), false);
}
static void bit_abs8(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c); sample_interrupt(c);
  uint8_t value = c->read(c->mem, ((uint32_t)c->db << 16) | address);
  c->z = ((uint8_t)c->a & value) == 0;
  c->n = (value & 0x80u) != 0;
  c->v = (value & 0x40u) != 0;
}
static void rol_a8(Cpu *c) {
  implied(c);
  unsigned result = ((unsigned)(uint8_t)c->a << 1) | (c->c ? 1u : 0u);
  c->c = (result & 0x100u) != 0;
  set_a8(c, (uint8_t)result);
}
static void load_dp16_to_a(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  c->a = read16(c, address); zn(c, c->a, false);
}
static void phx16(Cpu *c) {
  idle(c);
  c->write(c->mem, c->sp, (uint8_t)(c->x >> 8)); c->sp--;
  sample_interrupt(c);
  c->write(c->mem, c->sp, (uint8_t)c->x); c->sp--;
}
static void plx16(Cpu *c) {
  idle(c); idle(c);
  c->sp++; uint8_t low = c->read(c->mem, c->sp);
  sample_interrupt(c);
  c->sp++; uint8_t high = c->read(c->mem, c->sp);
  c->x = (uint16_t)(low | ((uint16_t)high << 8)); zn(c, c->x, false);
}
static void lda_indirect_long_y8(Cpu *c, uint8_t offset) {
  uint16_t dp = dp_address(c, offset);
  uint32_t pointer = c->read(c->mem, dp);
  pointer |= (uint32_t)c->read(c->mem, (uint16_t)(dp + 1)) << 8;
  pointer |= (uint32_t)c->read(c->mem, (uint16_t)(dp + 2)) << 16;
  sample_interrupt(c);
  set_a8(c, c->read(c->mem, (pointer + c->y) & 0xffffffu));
}
static void lda_indirect_long_y16(Cpu *c, uint8_t offset) {
  uint16_t dp = dp_address(c, offset);
  uint32_t pointer = c->read(c->mem, dp);
  pointer |= (uint32_t)c->read(c->mem, (uint16_t)(dp + 1)) << 8;
  pointer |= (uint32_t)c->read(c->mem, (uint16_t)(dp + 2)) << 16;
  uint32_t low = (pointer + c->y) & 0xffffffu;
  uint16_t value = c->read(c->mem, low); sample_interrupt(c);
  c->a = value | ((uint16_t)c->read(c->mem, (low + 1) & 0xffffffu) << 8);
  zn(c, c->a, false);
}
static void store_a_abs_x16(Cpu *c, uint16_t base) {
  uint32_t address = video_address(c, base, true);
  c->write(c->mem, address, (uint8_t)c->a); sample_interrupt(c);
  c->write(c->mem, (address + 1) & 0xffffffu, (uint8_t)(c->a >> 8));
}
static void adc8_dp(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset); sample_interrupt(c);
  uint8_t value = c->read(c->mem, address);
  uint8_t old = (uint8_t)c->a;
  unsigned sum = (unsigned)old + value + (c->c ? 1u : 0u);
  c->v = ((~(old ^ value) & (old ^ (uint8_t)sum)) & 0x80u) != 0;
  c->c = sum > 0xffu;
  set_a8(c, (uint8_t)sum);
}
/* $008711: walk the VRAM DMA queue until an inactive entry (bit7 clear). */
static bool vram_queue_find_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || !c->mf) return false;
  switch(pc) {
    case 0x8711: case 0x8714: case 0x8717: case 0x8719: case 0x871a: case 0x871b:
    case 0x871c: case 0x871d: case 0x871e: case 0x871f: case 0x8720: case 0x8721:
    case 0x8723: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8711: index_constant(c, false, 0); break;
    case 0x8714: video_load(c, 0x0800, true); break;
    case 0x8717: branch_to(c, !c->n, 0x8723); break;
    case 0x8719: case 0x871a: case 0x871b: case 0x871c:
    case 0x871d: case 0x871e: case 0x871f: case 0x8720:
      implied(c); c->x++; zn(c, c->x, false); break;
    case 0x8721: branch_to(c, true, 0x8714); break;
    case 0x8723: return_long(c); break;
  }
  return true;
}

/* $008724: clear the next queue entry's occupied flag (X+8). */
static bool vram_queue_mark_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || !c->mf) return false;
  switch(pc) {
    case 0x8724: case 0x8725: case 0x8726: case 0x8727: case 0x8728: case 0x8729:
    case 0x872a: case 0x872b: case 0x872c: case 0x872d: case 0x8730: case 0x8731: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8724: phx16(c); break;
    case 0x8725: case 0x8726: case 0x8727: case 0x8728:
    case 0x8729: case 0x872a: case 0x872b: case 0x872c:
      implied(c); c->x++; zn(c, c->x, false); break;
    case 0x872d: video_store(c, 0x0800, true, 0, true); break;
    case 0x8730: plx16(c); break;
    case 0x8731: return_long(c); break;
  }
  return true;
}

/* $0086FC: feed DP+$00/+01 into Mode-7 multiply registers and read the product. */
static bool ppu_multiply_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || !c->mf) return false;
  switch(pc) {
    case 0x86fc: case 0x86fe: case 0x8701: case 0x8703: case 0x8706:
    case 0x8709: case 0x870b: case 0x870e: case 0x8710: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x86fc: load_dp8(c, 0x00); break;
    case 0x86fe: store_register8(c, 0x211b); break;
    case 0x8701: load_dp_y16(c, 0x01); break;
    case 0x8703: store_y_abs16(c, 0x211b); break;
    case 0x8706: load_y_abs16(c, 0x2134); break;
    case 0x8709: store_dp_y16(c, 0x00); break;
    case 0x870b: {
      (void)fetch(c); (void)fetch(c); sample_interrupt(c);
      set_a8(c, c->read(c->mem, ((uint32_t)c->db << 16) | 0x2136)); break;
    }
    case 0x870e: store_dp(c, 0x02, true); break;
    case 0x8710: return_long(c); break;
  }
  return true;
}

/* $008887: program DMA1 for a VRAM word fill from bank 0 / length DP+$02. */
static bool vram_dma_setup_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf) return false;
  bool word = (pc >= 0x889e && pc <= 0x88a6);
  if(c->mf == word) return false;
  switch(pc) {
    case 0x8887: case 0x8889: case 0x888c: case 0x888e: case 0x8891: case 0x8893:
    case 0x8896: case 0x8899: case 0x889c: case 0x889e: case 0x88a1: case 0x88a3:
    case 0x88a6: case 0x88a8: case 0x88aa: case 0x88ad: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8887: byte_constant(c, 0x80); break;
    case 0x8889: store_register8(c, 0x2115); break;
    case 0x888c: byte_constant(c, 0x09); break;
    case 0x888e: store_register8(c, 0x4310); break;
    case 0x8891: byte_constant(c, 0x18); break;
    case 0x8893: store_register8(c, 0x4311); break;
    case 0x8896: stz_abs8(c, 0x4314); break;
    case 0x8899: stz_abs8(c, 0x4317); break;
    case 0x889c: set_m(c, false); break;
    case 0x889e: stz_abs16(c, 0x4312); break;
    case 0x88a1: load_dp16_to_a(c, 0x02); break;
    case 0x88a3: store_a_abs16(c, 0x4315); break;
    case 0x88a6: set_m(c, true); break;
    case 0x88a8: byte_constant(c, 0x02); break;
    case 0x88aa: store_register8(c, 0x420b); break;
    case 0x88ad: return_long(c); break;
  }
  return true;
}

/* $0088EA: index a 3-byte pointer table at [DP+$06] by DP+$03 and mark palette dirty. */
static bool pointer_resolve_step(Cpu *c, uint16_t pc) {
  if(c->e || c->d || c->xf || !c->mf) return false;
  switch(pc) {
    case 0x88ea: case 0x88ec: case 0x88ed: case 0x88ef: case 0x88f0: case 0x88f1:
    case 0x88f3: case 0x88f4: case 0x88f6: case 0x88f8: case 0x88f9: case 0x88fb:
    case 0x88fd: case 0x88fe: case 0x8900: case 0x8902: case 0x8904: case 0x8907: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x88ea: byte_constant(c, 0); break;
    case 0x88ec: xba_native(c); break;
    case 0x88ed: load_dp8(c, 0x03); break;
    case 0x88ef: asl_a8(c); break;
    case 0x88f0: implied(c); c->c = false; break;
    case 0x88f1: adc8_dp(c, 0x03); break;
    case 0x88f3: implied(c); c->y = c->a; zn(c, c->y, false); break;
    case 0x88f4: lda_indirect_long_y8(c, 0x06); break;
    case 0x88f6: store_dp(c, 0x00, true); break;
    case 0x88f8: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x88f9: lda_indirect_long_y8(c, 0x06); break;
    case 0x88fb: store_dp(c, 0x01, true); break;
    case 0x88fd: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x88fe: lda_indirect_long_y8(c, 0x06); break;
    case 0x8900: store_dp(c, 0x02, true); break;
    case 0x8902: byte_constant(c, 0x01); break;
    case 0x8904: store_register8(c, 0x0715); break;
    case 0x8907: return_long(c); break;
  }
  return true;
}

/* $0088C5: copy words from [DP+$00],Y down into the palette shadow via X. */
static bool palette_copy_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf) return false;
  if(pc == 0x88c5 && !c->mf) return false;
  if(pc == 0x88d2 && c->mf) return false;
  if((pc==0x88c7||pc==0x88c9||pc==0x88cc||pc==0x88cd||pc==0x88ce||pc==0x88cf||pc==0x88d0) && c->mf) return false;
  if((pc==0x88d4||pc==0x88d7) && !c->mf) return false;
  switch(pc) {
    case 0x88c5: case 0x88c7: case 0x88c9: case 0x88cc: case 0x88cd:
    case 0x88ce: case 0x88cf: case 0x88d0: case 0x88d2: case 0x88d4: case 0x88d7: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x88c5: set_m(c, false); break;
    case 0x88c7: lda_indirect_long_y16(c, 0x00); break;
    case 0x88c9: store_a_abs_x16(c, 0x0200); break;
    case 0x88cc: case 0x88cd: implied(c); c->x--; zn(c, c->x, false); break;
    case 0x88ce: case 0x88cf: implied(c); c->y--; zn(c, c->y, false); break;
    case 0x88d0: branch_to(c, !c->n, 0x88c7); break;
    case 0x88d2: set_m(c, true); break;
    case 0x88d4: inc_abs8_native(c, 0x0715); break;
    case 0x88d7: return_long(c); break;
  }
  return true;
}


static void lda16_constant(Cpu *c, uint16_t value) {
  uint16_t low = c->pc++, high = c->pc++;
  (void)c->read(c->mem, ((uint32_t)c->k << 16) | low); sample_interrupt(c);
  (void)c->read(c->mem, ((uint32_t)c->k << 16) | high);
  c->a = value; zn(c, c->a, false);
}
static void adc8_constant(Cpu *c, uint8_t value) {
  sample_interrupt(c); (void)fetch(c);
  uint8_t old = (uint8_t)c->a;
  unsigned sum = (unsigned)old + value + (c->c ? 1u : 0u);
  c->v = ((~(old ^ value) & (old ^ (uint8_t)sum)) & 0x80u) != 0;
  c->c = sum > 0xffu;
  set_a8(c, (uint8_t)sum);
}
static void lsr_a8(Cpu *c) {
  implied(c); c->c = ((uint8_t)c->a & 1u) != 0; set_a8(c, (uint8_t)c->a >> 1);
}
static void lda_long_x16(Cpu *c, uint32_t base) {
  (void)fetch(c); (void)fetch(c); (void)fetch(c);
  uint32_t low = (base + c->x) & 0xffffffu;
  uint16_t value = c->read(c->mem, low); sample_interrupt(c);
  c->a = value | ((uint16_t)c->read(c->mem, (low + 1) & 0xffffffu) << 8);
  zn(c, c->a, false);
}
static void store_a_abs_y16(Cpu *c, uint16_t base) {
  (void)fetch(c); (void)fetch(c); idle(c);
  uint32_t address = (((uint32_t)c->db << 16) + base + c->y) & 0xffffffu;
  c->write(c->mem, address, (uint8_t)c->a); sample_interrupt(c);
  c->write(c->mem, (address + 1) & 0xffffffu, (uint8_t)(c->a >> 8));
}
static void cpx16_constant(Cpu *c, uint16_t value) {
  uint16_t low = c->pc++, high = c->pc++;
  (void)c->read(c->mem, ((uint32_t)c->k << 16) | low); sample_interrupt(c);
  (void)c->read(c->mem, ((uint32_t)c->k << 16) | high);
  c->c = c->x >= value; zn(c, (uint16_t)(c->x - value), false);
}
static void cpy_dp16(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  uint16_t value = read16(c, address);
  c->c = c->y >= value; zn(c, (uint16_t)(c->y - value), false);
}
static void dec_dp8(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  uint8_t value = c->read(c->mem, address); idle(c); sample_interrupt(c);
  value--; c->write(c->mem, address, value); zn(c, value, true);
}
static void jsl_to(Cpu *c, uint16_t dest) {
  uint16_t low = c->pc++, high = c->pc++;
  (void)c->read(c->mem, ((uint32_t)c->k << 16) | low); sample_interrupt(c);
  (void)c->read(c->mem, ((uint32_t)c->k << 16) | high);
  push_byte_native(c, c->k); idle(c);
  uint8_t bank = fetch(c);
  push_byte_native(c, (uint8_t)((c->pc - 1) >> 8)); sample_interrupt(c);
  push_byte_native(c, (uint8_t)(c->pc - 1));
  c->pc = dest; c->k = bank;
}

static void jsr_abs_native(Cpu *c, uint16_t dest) {
  uint16_t low = c->pc++, high = c->pc++;
  (void)c->read(c->mem, ((uint32_t)c->k << 16) | low); sample_interrupt(c);
  (void)c->read(c->mem, ((uint32_t)c->k << 16) | high);
  idle(c);
  push_byte_native(c, (uint8_t)((c->pc - 1) >> 8)); sample_interrupt(c);
  push_byte_native(c, (uint8_t)(c->pc - 1));
  c->pc = dest;
}



/* $0088D8: set long pointer $06F945 at DP+$06 and resolve via $0088EA. */
static bool pointer_preset_88d8_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  if((pc == 0x88da || pc == 0x88dd) && c->mf) return false;
  if((pc == 0x88df) && c->mf) return false;
  if((pc == 0x88e1 || pc == 0x88e3 || pc == 0x88e5 || pc == 0x88e9) && !c->mf) return false;
  switch(pc) {
    case 0x88d8: case 0x88da: case 0x88dd: case 0x88df: case 0x88e1: case 0x88e3: case 0x88e5: case 0x88e9: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x88d8: set_m(c, false); break;
    case 0x88da: lda16_constant(c, 0xf945); break;
    case 0x88dd: store_dp(c, 0x06, false); break;
    case 0x88df: set_m(c, true); break;
    case 0x88e1: byte_constant(c, 0x06); break;
    case 0x88e3: store_dp(c, 0x08, true); break;
    case 0x88e5: jsl_to(c, 0x88ea); break;
    case 0x88e9: return_long(c); break;
  }
  return true;
}

/* $0088AE: set long pointer $06F876, resolve, load X/Y, fall into $0088C5. */
static bool pointer_preset_88ae_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  if((pc == 0x88b0 || pc == 0x88b3) && c->mf) return false;
  if((pc == 0x88b5) && c->mf) return false;
  if((pc == 0x88b7 || pc == 0x88b9 || pc == 0x88bb) && !c->mf) return false;
  switch(pc) {
    case 0x88ae: case 0x88b0: case 0x88b3: case 0x88b5: case 0x88b7: case 0x88b9: case 0x88bb: case 0x88bf: case 0x88c2: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x88ae: set_m(c, false); break;
    case 0x88b0: lda16_constant(c, 0xf876); break;
    case 0x88b3: store_dp(c, 0x06, false); break;
    case 0x88b5: set_m(c, true); break;
    case 0x88b7: byte_constant(c, 0x06); break;
    case 0x88b9: store_dp(c, 0x08, true); break;
    case 0x88bb: jsl_to(c, 0x88ea); break;
    case 0x88bf: index_constant(c, false, 0x00be); break;
    case 0x88c2: index_constant(c, true, 0x007e); break;
  }
  return true;
}

/* $008908: copy 0x40 bytes from $09D200 into palette shadow $02C0 and dirty it. */
static bool palette_rom_copy_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  if((pc == 0x890a || pc == 0x890d || pc == 0x8911 || pc == 0x8914 || pc == 0x8915 || pc == 0x8916 || pc == 0x8919) && c->mf) return false;
  if((pc == 0x891b) && c->mf) return false;
  if((pc == 0x891d || pc == 0x8920) && !c->mf) return false;
  switch(pc) {
    case 0x8908: case 0x890a: case 0x890d: case 0x8911: case 0x8914: case 0x8915:
    case 0x8916: case 0x8919: case 0x891b: case 0x891d: case 0x8920: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8908: set_m(c, false); break;
    case 0x890a: index_constant(c, false, 0); break;
    case 0x890d: lda_long_x16(c, 0x09d200); break;
    case 0x8911: store_a_abs_x16(c, 0x02c0); break;
    case 0x8914: case 0x8915: implied(c); c->x++; zn(c, c->x, false); break;
    case 0x8916: cpx16_constant(c, 0x0040); break;
    case 0x8919: branch_to(c, !c->c, 0x890d); break;
    case 0x891b: set_m(c, true); break;
    case 0x891d: inc_abs8_native(c, 0x0715); break;
    case 0x8920: return_long(c); break;
  }
  return true;
}

/* $0086C8: pick actor attribute, table-lookup nibble into $0D85. */
static bool attr_nibble_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x86c8: case 0x86ca: case 0x86cb: case 0x86ce: case 0x86d0: case 0x86d3:
    case 0x86d5: case 0x86d8: case 0x86d9: case 0x86db: case 0x86dc: case 0x86dd:
    case 0x86e1: case 0x86e4: case 0x86e6: case 0x86e9: case 0x86eb: case 0x86ee:
    case 0x86ef: case 0x86f2: case 0x86f3: case 0x86f4: case 0x86f5: case 0x86f6:
    case 0x86f8: case 0x86fb: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x86c8: byte_constant(c, 0); break;
    case 0x86ca: xba_native(c); break;
    case 0x86cb: video_load(c, 0x01a1, false); break;
    case 0x86ce: branch_to(c, c->n, 0x86d5); break;
    case 0x86d0: video_load(c, 0x01a0, false); break;
    case 0x86d3: branch_to(c, true, 0x86db); break;
    case 0x86d5: video_load(c, 0x01bd, false); break;
    case 0x86d8: implied(c); c->c = false; break;
    case 0x86d9: adc8_constant(c, 0x30); break;
    case 0x86db: lsr_a8(c); break;
    case 0x86dc: tax8(c); break;
    case 0x86dd: load_a_long_x8(c, 0x028584); break;
    case 0x86e1: store_register8(c, 0x0d85); break;
    case 0x86e4: branch_to(c, c->c, 0x86ef); break;
    case 0x86e6: video_load(c, 0x0d85, false); break;
    case 0x86e9: and_constant8(c, 0x07); break;
    case 0x86eb: store_register8(c, 0x0d85); break;
    case 0x86ee: return_long(c); break;
    case 0x86ef: video_load(c, 0x0d85, false); break;
    case 0x86f2: case 0x86f3: case 0x86f4: case 0x86f5: lsr_a8(c); break;
    case 0x86f6: and_constant8(c, 0x07); break;
    case 0x86f8: store_register8(c, 0x0d85); break;
    case 0x86fb: return_long(c); break;
  }
  return true;
}

/* $008732: CPU-copy words from [DP+$00] into VRAM at DP+$06/$07. */
static bool vram_cpu_copy_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x8732: case 0x8734: case 0x8737: case 0x8739: case 0x873c: case 0x873f:
    case 0x8741: case 0x8743: case 0x8746: case 0x8748: case 0x874b: case 0x874c:
    case 0x874e: case 0x8751: case 0x8752: case 0x8753: case 0x8755: case 0x8757:
    case 0x8758: case 0x875a: case 0x875c: case 0x875e: case 0x8760: case 0x8762:
    case 0x8764: case 0x8766: case 0x8768: case 0x876a: case 0x876d: case 0x876f:
    case 0x8772: case 0x8773: case 0x8775: case 0x8778: case 0x8779: case 0x877b:
    case 0x877d: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8732: load_dp8(c, 0x06); break;
    case 0x8734: store_register8(c, 0x2116); break;
    case 0x8737: load_dp8(c, 0x07); break;
    case 0x8739: store_register8(c, 0x2117); break;
    case 0x873c: case 0x8743: case 0x876a: index_constant(c, true, 0); break;
    case 0x873f: load_dp8(c, 0x05); break;
    case 0x8741: branch_to(c, c->z, 0x876d); break;
    case 0x8746: case 0x874c: case 0x876d: case 0x8773: lda_indirect_long_y8(c, 0x00); break;
    case 0x8748: case 0x876f: store_register8(c, 0x2118); break;
    case 0x874b: case 0x8751: case 0x8772: case 0x8778: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x874e: case 0x8775: store_register8(c, 0x2119); break;
    case 0x8752: implied(c); set_a8(c, (uint8_t)c->y); break;
    case 0x8753: branch_to(c, !c->z, 0x8746); break;
    case 0x8755: load_dp8(c, 0x01); break;
    case 0x8757: implied(c); c->c = false; break;
    case 0x8758: adc8_constant(c, 0x01); break;
    case 0x875a: store_dp(c, 0x01, true); break;
    case 0x875c: load_dp8(c, 0x02); break;
    case 0x875e: adc8_constant(c, 0x00); break;
    case 0x8760: store_dp(c, 0x02, true); break;
    case 0x8762: dec_dp8(c, 0x05); break;
    case 0x8764: branch_to(c, !c->z, 0x8743); break;
    case 0x8766: load_dp8(c, 0x04); break;
    case 0x8768: branch_to(c, c->z, 0x877d); break;
    case 0x8779: cpy_dp16(c, 0x04); break;
    case 0x877b: branch_to(c, !c->c, 0x876d); break;
    case 0x877d: return_long(c); break;
  }
  return true;
}

/* $008921: resolve two palette indices via $0088D8 and copy into $0300. */
static bool dual_palette_load_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  if((pc == 0x892f || pc == 0x8931 || pc == 0x8934 || pc == 0x8935 || pc == 0x8936 ||
      pc == 0x8948 || pc == 0x894a || pc == 0x894d || pc == 0x894e || pc == 0x894f) && c->mf) return false;
  if((pc == 0x8938 || pc == 0x8951) && c->mf) return false;
  if((pc == 0x8921 || pc == 0x8924 || pc == 0x8926 || pc == 0x893a || pc == 0x893d ||
      pc == 0x893f || pc == 0x8953) && !c->mf) return false;
  switch(pc) {
    case 0x8921: case 0x8924: case 0x8926: case 0x892a: case 0x892d: case 0x892f: case 0x8931:
    case 0x8934: case 0x8935: case 0x8936: case 0x8938: case 0x893a: case 0x893d: case 0x893f:
    case 0x8943: case 0x8945: case 0x8948: case 0x894a: case 0x894d: case 0x894e: case 0x894f:
    case 0x8951: case 0x8953: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8921: video_load(c, 0x01d2, false); break;
    case 0x8924: store_dp(c, 0x03, true); break;
    case 0x8926: case 0x893f: jsl_to(c, 0x88d8); break;
    case 0x892a: index_constant(c, true, 0x00fe); break;
    case 0x892d: case 0x8943: set_m(c, false); break;
    case 0x892f: case 0x8948: lda_indirect_long_y16(c, 0x00); break;
    case 0x8931: case 0x894a: store_a_abs_y16(c, 0x0300); break;
    case 0x8934: case 0x8935: case 0x894d: case 0x894e: implied(c); c->y--; zn(c, c->y, false); break;
    case 0x8936: branch_to(c, !c->n, 0x892f); break;
    case 0x8938: case 0x8951: set_m(c, true); break;
    case 0x893a: video_load(c, 0x01d3, false); break;
    case 0x893d: store_dp(c, 0x03, true); break;
    case 0x8945: index_constant(c, true, 0x007e); break;
    case 0x894f: branch_to(c, !c->n, 0x8948); break;
    case 0x8953: return_long(c); break;
  }
  return true;
}

static bool frame_upload_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf) return false;
  bool word = (pc >= 0x8380 && pc <= 0x8398) || (pc >= 0x83c2 && pc <= 0x83c9);
  if(c->mf == word) return false;
  static const uint16_t starts[] = {
    0x8324,0x8326,0x8328,0x832b,0x832e,0x8331,0x8334,0x8337,0x833a,0x833d,
    0x8340,0x8343,0x8346,0x8348,0x834b,0x834e,0x8350,0x8353,0x8356,0x8359,
    0x835c,0x835f,0x8362,0x8365,0x8368,0x836a,0x836d,0x8370,0x8373,0x8376,
    0x8378,0x837b,0x837e,0x8380,0x8383,0x8386,0x8389,0x838c,0x838f,0x8392,
    0x8395,0x8398,0x839a,0x839d,0x83a0,0x83a2,0x83a5,0x83a6,0x83a7,0x83a8,
    0x83a9,0x83aa,0x83ab,0x83ac,0x83ad,0x83af,0x83b2,0x83b5,0x83b8,0x83bb,
    0x83bd,0x83c0,0x83c2,0x83c4,0x83c7,0x83c9,0x83cb
  };
  bool valid = false;
  for(unsigned i = 0; i < sizeof(starts)/sizeof(*starts); i++) valid |= pc == starts[i];
  if(!valid) return false;
  (void)fetch(c);
  switch(pc) {
    // Disable HDMA if requested; transfer the complete sprite shadow via DMA1.
    case 0x8324: load_dp8(c, 0xba); break;
    case 0x8326: branch_to(c, c->n, 0x832b); break;
    case 0x8328: video_store(c, 0x420c, false, 0, true); break;
    case 0x832b: video_store(c, 0x2102, false, 0, true); break;
    case 0x832e: video_store(c, 0x2103, false, 0, true); break;
    case 0x8331: case 0x8337: index_constant(c, false, 0x0400); break;
    case 0x8334: video_store(c, 0x4310, false, c->x, false); break;
    case 0x833a: video_store(c, 0x4312, false, c->x, false); break;
    case 0x833d: video_store(c, 0x4314, false, 0, true); break;
    case 0x8340: index_constant(c, false, 0x0220); break;
    case 0x8343: video_store(c, 0x4315, false, c->x, false); break;
    case 0x8346: byte_constant(c, 2); break;
    case 0x8348: store_register8(c, 0x420b); break;
    // Transfer a dirty palette using DMA0, then clear the dirty flag.
    case 0x834b: video_load(c, 0x0715, false); break;
    case 0x834e: branch_to(c, c->z, 0x8370); break;
    case 0x8350: index_constant(c, true, 0x2200); break;
    case 0x8353: video_store(c, 0x4300, false, c->y, false); break;
    case 0x8356: video_store(c, 0x2121, false, 0, true); break;
    case 0x8359: case 0x8362: index_constant(c, true, 0x0200); break;
    case 0x835c: video_store(c, 0x4302, false, c->y, false); break;
    case 0x835f: video_store(c, 0x4304, false, 0, true); break;
    case 0x8365: video_store(c, 0x4305, false, c->y, false); break;
    case 0x8368: byte_constant(c, 1); break;
    case 0x836a: store_register8(c, 0x420b); break;
    case 0x836d: video_store(c, 0x0715, false, 0, true); break;
    // Consume occupied VRAM queue entries, using DMA2 for each transfer.
    case 0x8370: case 0x83af: index_constant(c, false, 0); break;
    case 0x8373: video_load(c, 0x0800, true); break;
    case 0x8376: branch_to(c, !c->n, 0x83af); break;
    case 0x8378: store_register8(c, 0x2115); break;
    case 0x837b: video_store(c, 0x0800, true, 0, true); break;
    case 0x837e: case 0x83c0: set_m(c, false); break;
    case 0x8380: video_load(c, 0x0801, true); break;
    case 0x8383: video_store(c, 0x2116, false, c->a, false); break;
    case 0x8386: {
      uint16_t low = c->pc++, high = c->pc++;
      (void)c->read(c->mem, ((uint32_t)c->k << 16) | low); sample_interrupt(c);
      (void)c->read(c->mem, ((uint32_t)c->k << 16) | high);
      c->a = 0x1801; zn(c, c->a, false); break;
    }
    case 0x8389: video_store(c, 0x4320, false, c->a, false); break;
    case 0x838c: video_load(c, 0x0803, true); break;
    case 0x838f: video_store(c, 0x4322, false, c->a, false); break;
    case 0x8392: video_load(c, 0x0805, true); break;
    case 0x8395: video_store(c, 0x4324, false, c->a, false); break;
    case 0x8398: case 0x83c9: set_m(c, true); break;
    case 0x839a: video_load(c, 0x0807, true); break;
    case 0x839d: store_register8(c, 0x4326); break;
    case 0x83a0: byte_constant(c, 4); break;
    case 0x83a2: store_register8(c, 0x420b); break;
    case 0x83a5: case 0x83a6: case 0x83a7: case 0x83a8:
    case 0x83a9: case 0x83aa: case 0x83ab: case 0x83ac:
      implied(c); c->x++; zn(c, c->x, false); break;
    case 0x83ad: branch_to(c, true, 0x8373); break;
    // Reset transfer bookkeeping, restore HDMA mask and OAM address.
    case 0x83b2: video_store(c, 0x070f, false, c->x, false); break;
    case 0x83b5: video_store(c, 0x0711, false, c->x, false); break;
    case 0x83b8: video_store(c, 0x0713, false, c->x, false); break;
    case 0x83bb: load_dp8(c, 0x62); break;
    case 0x83bd: store_register8(c, 0x420c); break;
    case 0x83c2: {
      uint16_t address = dp_address(c, 0xa8); c->a = read16(c, address); zn(c, c->a, false); break;
    }
    case 0x83c4: video_store(c, 0x2102, false, c->a, false); break;
    case 0x83c7: {
      uint16_t address = dp_address(c, 0xa8);
      c->write(c->mem, address, 0); sample_interrupt(c);
      c->write(c->mem, (uint16_t)(address + 1), 0); break;
    }
    case 0x83cb: return_long(c); break;
  }
  return true;
}

/* ---- Shared helpers for newly recovered routines ---- */
static void pha8(Cpu *c) {
  idle(c); sample_interrupt(c); push_byte_native(c, (uint8_t)c->a);
}
static void pla8(Cpu *c) {
  idle(c); idle(c); sample_interrupt(c); set_a8(c, pull(c));
}
static void pha16(Cpu *c) {
  idle(c);
  c->write(c->mem, c->sp, (uint8_t)(c->a >> 8)); c->sp--;
  sample_interrupt(c);
  c->write(c->mem, c->sp, (uint8_t)c->a); c->sp--;
}
static void pla16(Cpu *c) {
  idle(c); idle(c);
  c->sp++; uint8_t low = c->read(c->mem, c->sp);
  sample_interrupt(c);
  c->sp++; uint8_t high = c->read(c->mem, c->sp);
  c->a = (uint16_t)(low | ((uint16_t)high << 8)); zn(c, c->a, false);
}
static void phy16(Cpu *c) {
  idle(c);
  c->write(c->mem, c->sp, (uint8_t)(c->y >> 8)); c->sp--;
  sample_interrupt(c);
  c->write(c->mem, c->sp, (uint8_t)c->y); c->sp--;
}
static void ply16(Cpu *c) {
  idle(c); idle(c);
  c->sp++; uint8_t low = c->read(c->mem, c->sp);
  sample_interrupt(c);
  c->sp++; uint8_t high = c->read(c->mem, c->sp);
  c->y = (uint16_t)(low | ((uint16_t)high << 8)); zn(c, c->y, false);
}
static void store_a_abs_x8(Cpu *c, uint16_t base) {
  uint32_t address = video_address(c, base, true);
  sample_interrupt(c); c->write(c->mem, address, (uint8_t)c->a);
}
static void stz_abs_x8(Cpu *c, uint16_t base) {
  uint32_t address = video_address(c, base, true);
  sample_interrupt(c); c->write(c->mem, address, 0);
}
static void stz_abs_x16(Cpu *c, uint16_t base) {
  uint32_t address = video_address(c, base, true);
  c->write(c->mem, address, 0); sample_interrupt(c);
  c->write(c->mem, (address + 1) & 0xffffffu, 0);
}
static void store_a_long_x16(Cpu *c, uint32_t base) {
  (void)fetch(c); (void)fetch(c); (void)fetch(c);
  uint32_t low = (base + c->x) & 0xffffffu;
  c->write(c->mem, low, (uint8_t)c->a); sample_interrupt(c);
  c->write(c->mem, (low + 1) & 0xffffffu, (uint8_t)(c->a >> 8));
}
static void lda_long8(Cpu *c, uint32_t address) {
  (void)fetch(c); (void)fetch(c); (void)fetch(c); sample_interrupt(c);
  set_a8(c, c->read(c->mem, address & 0xffffffu));
}
static void sta_long8(Cpu *c, uint32_t address) {
  (void)fetch(c); (void)fetch(c); (void)fetch(c); sample_interrupt(c);
  c->write(c->mem, address & 0xffffffu, (uint8_t)c->a);
}
static void lda_long16(Cpu *c, uint32_t address) {
  (void)fetch(c); (void)fetch(c); (void)fetch(c);
  uint32_t low = address & 0xffffffu;
  uint16_t value = c->read(c->mem, low); sample_interrupt(c);
  c->a = value | ((uint16_t)c->read(c->mem, (low + 1) & 0xffffffu) << 8);
  zn(c, c->a, false);
}
static void sta_long16(Cpu *c, uint32_t address) {
  (void)fetch(c); (void)fetch(c); (void)fetch(c);
  uint32_t low = address & 0xffffffu;
  c->write(c->mem, low, (uint8_t)c->a); sample_interrupt(c);
  c->write(c->mem, (low + 1) & 0xffffffu, (uint8_t)(c->a >> 8));
}
static void lda_indirect_long16(Cpu *c, uint8_t offset) {
  uint16_t dp = dp_address(c, offset);
  uint32_t pointer = c->read(c->mem, dp);
  pointer |= (uint32_t)c->read(c->mem, (uint16_t)(dp + 1)) << 8;
  pointer |= (uint32_t)c->read(c->mem, (uint16_t)(dp + 2)) << 16;
  uint16_t value = c->read(c->mem, pointer); sample_interrupt(c);
  c->a = value | ((uint16_t)c->read(c->mem, (pointer + 1) & 0xffffffu) << 8);
  zn(c, c->a, false);
}
static void cpx_dp16(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  uint16_t value = read16(c, address);
  c->c = c->x >= value; zn(c, (uint16_t)(c->x - value), false);
}
static void adc_abs16(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c);
  uint32_t low = ((uint32_t)c->db << 16) | address;
  uint16_t value = c->read(c->mem, low); sample_interrupt(c);
  value |= (uint16_t)c->read(c->mem, (low + 1) & 0xffffffu) << 8;
  uint16_t old = c->a;
  unsigned result = (unsigned)old + value + (c->c ? 1u : 0u);
  c->v = (uint16_t)(~(old ^ value) & (old ^ (uint16_t)result) & 0x8000u) != 0;
  c->c = result > 0xffffu;
  c->a = (uint16_t)result; zn(c, c->a, false);
}
static void adc_abs8(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c); sample_interrupt(c);
  uint8_t value = c->read(c->mem, ((uint32_t)c->db << 16) | address);
  uint8_t old = (uint8_t)c->a;
  unsigned sum = (unsigned)old + value + (c->c ? 1u : 0u);
  c->v = ((~(old ^ value) & (old ^ (uint8_t)sum)) & 0x80u) != 0;
  c->c = sum > 0xffu;
  set_a8(c, (uint8_t)sum);
}

static void sbc_abs8(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c); sample_interrupt(c);
  sbc8_value(c, c->read(c->mem, ((uint32_t)c->db << 16) | address));
}
static void jmp_indirect_abs_x(Cpu *c, uint16_t base) {
  (void)fetch(c); (void)fetch(c); idle(c);
  uint16_t table = (uint16_t)(base + c->x);
  uint16_t lo = c->read(c->mem, ((uint32_t)c->k << 16) | table); sample_interrupt(c);
  c->pc = (uint16_t)(lo | ((uint16_t)c->read(c->mem,
    ((uint32_t)c->k << 16) | ((table + 1) & 0xffffu)) << 8));
}

/* $008B28: clear HDMA enable bits in DP+$62 / $420C, wipe $0800..$087F, then
 * call external setup and dispatch via ($8B54,X) from DP+$24. */
static bool hdma_queue_wipe_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x8b28: case 0x8b2a: case 0x8b2c: case 0x8b2e: case 0x8b31: case 0x8b34:
    case 0x8b36: case 0x8b39: case 0x8b3a: case 0x8b3c: case 0x8b3e: case 0x8b42:
    case 0x8b46: case 0x8b48: case 0x8b49: case 0x8b4b: case 0x8b4d: case 0x8b4f:
    case 0x8b50: case 0x8b51: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8b28: load_dp8(c, 0x62); break;
    case 0x8b2a: and_constant8(c, 0xe7); break;
    case 0x8b2c: store_dp(c, 0x62, true); break;
    case 0x8b2e: store_register8(c, 0x420c); break;
    case 0x8b31: index_constant(c, false, 0x007f); break;
    case 0x8b34: byte_constant(c, 0); break;
    case 0x8b36: store_a_abs_x8(c, 0x0800); break;
    case 0x8b39: implied(c); c->x--; zn(c, c->x, false); break;
    case 0x8b3a: branch_to(c, !c->n, 0x8b34); break;
    case 0x8b3c: byte_constant(c, 0x80); break;
    case 0x8b3e: jsl_to(c, 0x85b6); break;
    case 0x8b42: jsl_to(c, 0xfb8d); break;
    case 0x8b46: byte_constant(c, 0); break;
    case 0x8b48: xba_native(c); break;
    case 0x8b49: load_dp8(c, 0x24); break;
    case 0x8b4b: and_constant8(c, 0x7f); break;
    case 0x8b4d: store_dp(c, 0x24, true); break;
    case 0x8b4f: asl_a8(c); break;
    case 0x8b50: implied(c); c->x = c->a; zn(c, c->x, false); break;
    case 0x8b51: jmp_indirect_abs_x(c, 0x8b54); break;
  }
  return true;
}

/* $008996: fill $7E4800..$7E4BFF with $FFFF, clear $0D7D word. */
static bool wram_fill_ffff_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  if((pc == 0x8998 || pc == 0x899b || pc == 0x899e || pc == 0x89a2 || pc == 0x89a3 ||
      pc == 0x89a4 || pc == 0x89a6) && c->mf) return false;
  if((pc == 0x89ab) && !c->mf) return false;
  switch(pc) {
    case 0x8996: case 0x8998: case 0x899b: case 0x899e: case 0x89a2: case 0x89a3:
    case 0x89a4: case 0x89a6: case 0x89a9: case 0x89ab: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8996: set_m(c, false); break;
    case 0x8998: index_constant(c, false, 0x03fe); break;
    case 0x899b: lda16_constant(c, 0xffff); break;
    case 0x899e: store_a_long_x16(c, 0x7e4800); break;
    case 0x89a2: case 0x89a3: implied(c); c->x--; zn(c, c->x, false); break;
    case 0x89a4: branch_to(c, !c->n, 0x899e); break;
    case 0x89a6: stz_abs16(c, 0x0d7d); break;
    case 0x89a9: set_m(c, true); break;
    case 0x89ab: return_long(c); break;
  }
  return true;
}

/* $009B36: enqueue a VRAM transfer described by [DP+$04] into the $0800 queue,
 * then copy payload bytes into $0880. */
static bool vram_queue_enqueue_desc_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x9b36: case 0x9b3a: case 0x9b3e: case 0x9b41: case 0x9b43: case 0x9b46:
    case 0x9b48: case 0x9b4b: case 0x9b4e: case 0x9b50: case 0x9b51: case 0x9b53:
    case 0x9b56: case 0x9b57: case 0x9b59: case 0x9b5c: case 0x9b5d: case 0x9b5f:
    case 0x9b62: case 0x9b64: case 0x9b67: case 0x9b6a: case 0x9b6d: case 0x9b6f:
    case 0x9b72: case 0x9b73: case 0x9b74: case 0x9b76: case 0x9b78: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x9b36: jsl_to(c, 0x8711); break;
    case 0x9b3a: jsl_to(c, 0x8724); break;
    case 0x9b3e: index_constant(c, true, 0); break;
    case 0x9b41: case 0x9b5d: byte_constant(c, 0x80); break;
    case 0x9b43: store_a_abs_x8(c, 0x0800); break;
    case 0x9b46: case 0x9b51: case 0x9b57: case 0x9b6d: lda_indirect_long_y8(c, 0x04); break;
    case 0x9b48: store_a_abs_x8(c, 0x0806); break;
    case 0x9b4b: stz_abs_x8(c, 0x0807); break;
    case 0x9b4e: store_dp(c, 0x07, true); break;
    case 0x9b50: case 0x9b56: case 0x9b5c: case 0x9b73:
      implied(c); c->y++; zn(c, c->y, false); break;
    case 0x9b53: store_a_abs_x8(c, 0x0801); break;
    case 0x9b59: store_a_abs_x8(c, 0x0802); break;
    case 0x9b5f: store_a_abs_x8(c, 0x0803); break;
    case 0x9b62: byte_constant(c, 0x08); break;
    case 0x9b64: store_a_abs_x8(c, 0x0804); break;
    case 0x9b67: stz_abs_x8(c, 0x0805); break;
    case 0x9b6a: index_constant(c, false, 0); break;
    case 0x9b6f: store_a_abs_x8(c, 0x0880); break;
    case 0x9b72: implied(c); c->x++; zn(c, c->x, false); break;
    case 0x9b74: cpx_dp16(c, 0x07); break;
    case 0x9b76: branch_to(c, !c->c, 0x9b6d); break;
    case 0x9b78: return_long(c); break;
  }
  return true;
}

/* $009B79: enqueue a compact VRAM DMA from DP+$00/+02, advancing $0713 bookkeeping. */
static bool vram_queue_enqueue_dp_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  if((pc == 0x9b90 || pc == 0x9b92 || pc == 0x9b95 || pc == 0x9b98 || pc == 0x9b99 ||
      pc == 0x9b9c || pc == 0x9b9f || pc == 0x9ba2) && c->mf) return false;
  if((pc == 0x9b79 || pc == 0x9b7d || pc == 0x9b81 || pc == 0x9b83 || pc == 0x9b85 ||
      pc == 0x9b87 || pc == 0x9b89 || pc == 0x9b8b || pc == 0x9ba7 || pc == 0x9ba9 ||
      pc == 0x9bab || pc == 0x9bac || pc == 0x9baf || pc == 0x9bb0 || pc == 0x9bb3 ||
      pc == 0x9bb6 || pc == 0x9bb8 || pc == 0x9bbb || pc == 0x9bbe) && !c->mf) return false;
  switch(pc) {
    case 0x9b79: case 0x9b7d: case 0x9b81: case 0x9b83: case 0x9b85: case 0x9b87:
    case 0x9b89: case 0x9b8b: case 0x9b8e: case 0x9b90: case 0x9b92: case 0x9b95:
    case 0x9b98: case 0x9b99: case 0x9b9c: case 0x9b9f: case 0x9ba2: case 0x9ba5:
    case 0x9ba7: case 0x9ba9: case 0x9bab: case 0x9bac: case 0x9baf: case 0x9bb0:
    case 0x9bb3: case 0x9bb6: case 0x9bb8: case 0x9bbb: case 0x9bbe: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x9b79: jsl_to(c, 0x8711); break;
    case 0x9b7d: jsl_to(c, 0x8724); break;
    case 0x9b81: case 0x9ba7: load_dp8(c, 0x02); break;
    case 0x9b83: and_constant8(c, 0x80); break;
    case 0x9b85: branch_to(c, !c->n, 0x9b89); break;
    case 0x9b87: byte_constant(c, 0x01); break;
    case 0x9b89: ora_constant8(c, 0x80); break;
    case 0x9b8b: store_a_abs_x8(c, 0x0800); break;
    case 0x9b8e: set_m(c, false); break;
    case 0x9b90: load_dp16_to_a(c, 0x00); break;
    case 0x9b92: store_a_abs_x16(c, 0x0801); break;
    case 0x9b95: lda16_constant(c, 0x0980); break;
    case 0x9b98: case 0x9baf: implied(c); c->c = false; break;
    case 0x9b99: adc_abs16(c, 0x0713); break;
    case 0x9b9c: store_a_abs_x16(c, 0x0803); break;
    case 0x9b9f: stz_abs_x16(c, 0x0805); break;
    case 0x9ba2: stz_abs_x16(c, 0x0807); break;
    case 0x9ba5: set_m(c, true); break;
    case 0x9ba9: and_constant8(c, 0x7f); break;
    case 0x9bab: asl_a8(c); break;
    case 0x9bac: store_a_abs_x8(c, 0x0806); break;
    case 0x9bb0: adc_abs8(c, 0x0713); break;
    case 0x9bb3: store_register8(c, 0x0713); break;
    case 0x9bb6: byte_constant(c, 0); break;
    case 0x9bb8: adc_abs8(c, 0x0714); break;
    case 0x9bbb: store_register8(c, 0x0714); break;
    case 0x9bbe: return_long(c); break;
  }
  return true;
}

/* $008954: index *$06F945 by A, resolve long pointer to DP+$10, bump Y via $8AF1,
 * copy 16 words into palette shadow $0300 and dirty $0715. */
static bool palette_index_load_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  if((pc == 0x8967 || pc == 0x896a || pc == 0x896c || pc == 0x896e || pc == 0x8986 ||
      pc == 0x8988 || pc == 0x898b || pc == 0x898c || pc == 0x898d || pc == 0x898e) && c->mf)
    return false;
  if((pc == 0x8954 || pc == 0x8955 || pc == 0x8957 || pc == 0x8958 || pc == 0x895a ||
      pc == 0x895b || pc == 0x895c || pc == 0x895e || pc == 0x895f || pc == 0x8961 ||
      pc == 0x8963 || pc == 0x8972 || pc == 0x8973 || pc == 0x8976 || pc == 0x8978 ||
      pc == 0x897a || pc == 0x897b || pc == 0x897f || pc == 0x8980 || pc == 0x8981 ||
      pc == 0x8992 || pc == 0x8995) && !c->mf) return false;
  switch(pc) {
    case 0x8954: case 0x8955: case 0x8957: case 0x8958: case 0x895a: case 0x895b:
    case 0x895c: case 0x895e: case 0x895f: case 0x8961: case 0x8963: case 0x8964:
    case 0x8966: case 0x8967: case 0x896a: case 0x896c: case 0x896e: case 0x8970:
    case 0x8972: case 0x8973: case 0x8976: case 0x8978: case 0x897a: case 0x897b:
    case 0x897f: case 0x8980: case 0x8981: case 0x8984: case 0x8986: case 0x8988:
    case 0x898b: case 0x898c: case 0x898d: case 0x898e: case 0x8990: case 0x8992:
    case 0x8995: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8954: case 0x8957: xba_native(c); break;
    case 0x8955: byte_constant(c, 0); break;
    case 0x8958: store_dp(c, 0x00, true); break;
    case 0x895a: asl_a8(c); break;
    case 0x895b: case 0x8966: implied(c); c->c = false; break;
    case 0x895c: adc8_dp(c, 0x00); break;
    case 0x895e: pha8(c); break;
    case 0x895f: byte_constant(c, 0x06); break;
    case 0x8961: store_dp(c, 0x02, true); break;
    case 0x8963: pla8(c); break;
    case 0x8964: set_m(c, false); break;
    case 0x8967: adc16_constant(c, 0xf945); break;
    case 0x896a: store_dp(c, 0x00, false); break;
    case 0x896c: lda_indirect_long16(c, 0x00); break;
    case 0x896e: store_dp(c, 0x10, false); break;
    case 0x8970: set_m(c, true); break;
    case 0x8972: phy16(c); break;
    case 0x8973: index_constant(c, true, 0x0002); break;
    case 0x8976: lda_indirect_long_y8(c, 0x00); break;
    case 0x8978: store_dp(c, 0x12, true); break;
    case 0x897a: ply16(c); break;
    case 0x897b: jsl_to(c, 0x8af1); break;
    case 0x897f: case 0x8980: case 0x898b: case 0x898c:
      implied(c); c->y--; zn(c, c->y, false); break;
    case 0x8981: index_constant(c, false, 0x000f); break;
    case 0x8984: set_m(c, false); break;
    case 0x8986: lda_indirect_long_y16(c, 0x10); break;
    case 0x8988: store_a_abs_y16(c, 0x0300); break;
    case 0x898d: implied(c); c->x--; zn(c, c->x, false); break;
    case 0x898e: branch_to(c, !c->n, 0x8986); break;
    case 0x8990: set_m(c, true); break;
    case 0x8992: inc_abs8_native(c, 0x0715); break;
    case 0x8995: return_long(c); break;
  }
  return true;
}

/* $008E26: map $01A0 through optional $02E41B table, build long pointer at
 * DP+$00 from $06F2DE, then pull nested pointer fields into $0702..$0706. */
static bool scene_pointer_build_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  if((pc == 0x8e3d || pc == 0x8e3f || pc == 0x8e40 || pc == 0x8e43) && c->mf) return false;
  if((pc == 0x8e26 || pc == 0x8e28 || pc == 0x8e29 || pc == 0x8e2c || pc == 0x8e2d ||
      pc == 0x8e2e || pc == 0x8e30 || pc == 0x8e32 || pc == 0x8e36 || pc == 0x8e39 ||
      pc == 0x8e3a || pc == 0x8e47 || pc == 0x8e49 || pc == 0x8e4b || pc == 0x8e4d ||
      pc == 0x8e50 || pc == 0x8e53 || pc == 0x8e55 || pc == 0x8e57 || pc == 0x8e58 ||
      pc == 0x8e5a || pc == 0x8e5d || pc == 0x8e5e || pc == 0x8e60 || pc == 0x8e62 ||
      pc == 0x8e64 || pc == 0x8e67 || pc == 0x8e6a || pc == 0x8e6c || pc == 0x8e6f ||
      pc == 0x8e70 || pc == 0x8e72 || pc == 0x8e75) && !c->mf) return false;
  switch(pc) {
    case 0x8e26: case 0x8e28: case 0x8e29: case 0x8e2c: case 0x8e2d: case 0x8e2e:
    case 0x8e30: case 0x8e32: case 0x8e36: case 0x8e39: case 0x8e3a: case 0x8e3b:
    case 0x8e3d: case 0x8e3f: case 0x8e40: case 0x8e43: case 0x8e45: case 0x8e47:
    case 0x8e49: case 0x8e4b: case 0x8e4d: case 0x8e50: case 0x8e53: case 0x8e55:
    case 0x8e57: case 0x8e58: case 0x8e5a: case 0x8e5d: case 0x8e5e: case 0x8e60:
    case 0x8e62: case 0x8e64: case 0x8e67: case 0x8e6a: case 0x8e6c: case 0x8e6f:
    case 0x8e70: case 0x8e72: case 0x8e75: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8e26: byte_constant(c, 0); break;
    case 0x8e28: xba_native(c); break;
    case 0x8e29: video_load(c, 0x01a0, false); break;
    case 0x8e2c: pha8(c); break;
    case 0x8e2d: implied(c); c->x = c->a; zn(c, c->x, false); break;
    case 0x8e2e: cmp_constant8(c, 0x30); break;
    case 0x8e30: branch_to(c, c->c, 0x8e39); break;
    case 0x8e32: load_a_long_x8(c, 0x02e41b); break;
    case 0x8e36: store_register8(c, 0x01e7); break;
    case 0x8e39: pla8(c); break;
    case 0x8e3a: asl_a8(c); break;
    case 0x8e3b: set_m(c, false); break;
    case 0x8e3d: store_dp(c, 0x03, false); break;
    case 0x8e3f: implied(c); c->c = false; break;
    case 0x8e40: adc16_constant(c, 0xf2de); break;
    case 0x8e43: store_dp(c, 0x00, false); break;
    case 0x8e45: set_m(c, true); break;
    case 0x8e47: byte_constant(c, 0x06); break;
    case 0x8e49: store_dp(c, 0x02, true); break;
    case 0x8e4b: store_dp(c, 0x05, true); break;
    case 0x8e4d: store_register8(c, 0x0706); break;
    case 0x8e50: case 0x8e67: index_constant(c, true, 0); break;
    case 0x8e53: case 0x8e5e: lda_indirect_long_y8(c, 0x00); break;
    case 0x8e55: store_dp(c, 0x03, true); break;
    case 0x8e57: implied(c); c->c = false; break;
    case 0x8e58: adc8_constant(c, 0x02); break;
    case 0x8e5a: store_register8(c, 0x0704); break;
    case 0x8e5d: case 0x8e6f: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x8e60: store_dp(c, 0x04, true); break;
    case 0x8e62: adc8_constant(c, 0x00); break;
    case 0x8e64: store_register8(c, 0x0705); break;
    case 0x8e6a: case 0x8e70: lda_indirect_long_y8(c, 0x03); break;
    case 0x8e6c: store_register8(c, 0x0703); break;
    case 0x8e72: store_register8(c, 0x0702); break;
    case 0x8e75: return_long(c); break;
  }
  return true;
}

/* $008DFA–$8E25: JSL $008E26 trampoline then scene setup — load $01A0 into
 * DP+$03, chain palette/attr helpers, store $20 to $0700, then JSL $06F14D. */
static bool scene_setup_8dfe_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x8dfa: case 0x8dfe: case 0x8e01: case 0x8e03: case 0x8e07: case 0x8e0a:
    case 0x8e0c: case 0x8e10: case 0x8e14: case 0x8e18: case 0x8e1c: case 0x8e1e:
    case 0x8e21: case 0x8e25: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8dfa: jsl_to(c, 0x8e26); break;
    case 0x8dfe: case 0x8e07: video_load(c, 0x01a0, false); break;
    case 0x8e01: case 0x8e0a: store_dp(c, 0x03, true); break;
    case 0x8e03: jsl_to(c, 0x88ae); break;
    case 0x8e0c: jsl_to(c, 0x8921); break;
    case 0x8e10: jsl_to(c, 0x8908); break;
    case 0x8e14: jsl_to(c, 0x90c1); break;
    case 0x8e18: jsl_to(c, 0x86c8); break;
    case 0x8e1c: byte_constant(c, 0x20); break;
    case 0x8e1e: store_register8(c, 0x0700); break;
    case 0x8e21: jsl_to(c, 0xf14d); break;
    case 0x8e25: return_long(c); break;
  }
  return true;
}

/* ---- Helpers for $90C1 / $C559 / $C68C / $8E76 / $8D2B / $04:85B6 ---- */
static uint8_t get_flags_native(Cpu *c) {
  return (uint8_t)((c->n ? 0x80u : 0) | (c->v ? 0x40u : 0) | (c->mf ? 0x20u : 0) |
                   (c->xf ? 0x10u : 0) | (c->d ? 0x08u : 0) | (c->i ? 0x04u : 0) |
                   (c->z ? 0x02u : 0) | (c->c ? 0x01u : 0));
}
static void set_flags_native(Cpu *c, uint8_t val) {
  c->n = (val & 0x80u) != 0; c->v = (val & 0x40u) != 0;
  c->mf = (val & 0x20u) != 0; c->xf = (val & 0x10u) != 0;
  c->d = (val & 0x08u) != 0; c->i = (val & 0x04u) != 0;
  c->z = (val & 0x02u) != 0; c->c = (val & 0x01u) != 0;
  if(c->e) { c->mf = true; c->xf = true; c->sp = 0x100u | (c->sp & 255u); }
  if(c->xf) { c->x &= 255u; c->y &= 255u; }
}
static void sep_rep_native(Cpu *c, uint8_t mask, bool sep) {
  (void)fetch(c); sample_interrupt(c);
  uint8_t flags = get_flags_native(c);
  flags = sep ? (uint8_t)(flags | mask) : (uint8_t)(flags & (uint8_t)~mask);
  set_flags_native(c, flags); idle(c);
}
static void php_native(Cpu *c) {
  idle(c); sample_interrupt(c); push_byte_native(c, get_flags_native(c));
}
static void phb_native(Cpu *c) {
  idle(c); sample_interrupt(c); push_byte_native(c, c->db);
}
static void plb_native(Cpu *c) {
  idle(c); idle(c); sample_interrupt(c); c->db = pull(c); zn(c, c->db, true);
}
static void plp_native(Cpu *c) {
  idle(c); idle(c); sample_interrupt(c); set_flags_native(c, pull(c));
}
static void store_dp_x16(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  c->write(c->mem, address, (uint8_t)c->x); sample_interrupt(c);
  c->write(c->mem, (uint16_t)(address + 1), (uint8_t)(c->x >> 8));
}
static void load_dp_x16(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  c->x = read16(c, address); zn(c, c->x, false);
}
static void stz_dp8(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset); sample_interrupt(c);
  c->write(c->mem, address, 0);
}
static void lsr_dp8(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  uint8_t value = c->read(c->mem, address); idle(c);
  c->c = (value & 1u) != 0; value >>= 1;
  sample_interrupt(c); c->write(c->mem, address, value); zn(c, value, true);
}
static void lsr_a16(Cpu *c) {
  implied(c); c->c = (c->a & 1u) != 0; c->a >>= 1; zn(c, c->a, false);
}
static void eor_constant8(Cpu *c, uint8_t value) {
  sample_interrupt(c); (void)fetch(c); set_a8(c, (uint8_t)((uint8_t)c->a ^ value));
}
static void sbc_dp16(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  uint16_t value = read16(c, address), old = c->a;
  unsigned result = (unsigned)old - value - (c->c ? 0u : 1u);
  uint16_t out = (uint16_t)result;
  c->v = ((old ^ out) & (old ^ value) & 0x8000u) != 0;
  c->c = result < 0x10000u; c->a = out; zn(c, out, false);
}
static void adc_dp16(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  uint16_t value = read16(c, address), old = c->a;
  unsigned result = (unsigned)old + value + (c->c ? 1u : 0u);
  uint16_t out = (uint16_t)result;
  c->v = (~(old ^ value) & (old ^ out) & 0x8000u) != 0;
  c->c = result > 0xffffu; c->a = out; zn(c, out, false);
}
static void ldx_abs_y16(Cpu *c, uint16_t base) {
  (void)fetch(c); (void)fetch(c); idle(c);
  uint32_t low = (((uint32_t)c->db << 16) + base + c->y) & 0xffffffu;
  uint16_t value = c->read(c->mem, low); sample_interrupt(c);
  c->x = value | ((uint16_t)c->read(c->mem, (low + 1) & 0xffffffu) << 8);
  zn(c, c->x, false);
}
static void lda_abs_y16(Cpu *c, uint16_t base) {
  (void)fetch(c); (void)fetch(c); idle(c);
  uint32_t low = (((uint32_t)c->db << 16) + base + c->y) & 0xffffffu;
  uint16_t value = c->read(c->mem, low); sample_interrupt(c);
  c->a = value | ((uint16_t)c->read(c->mem, (low + 1) & 0xffffffu) << 8);
  zn(c, c->a, false);
}
static void load_a_abs_x8(Cpu *c, uint16_t base) {
  (void)fetch(c); (void)fetch(c); idle(c); sample_interrupt(c);
  set_a8(c, c->read(c->mem, (((uint32_t)c->db << 16) + base + c->x) & 0xffffffu));
}

/* $00:98D5-$990E: walk the eight actor slots and dispatch occupied entries.
 * The two JSL callees remain ordinary dispatch targets; this slice only
 * reconstructs the loop and its selector/stack behavior. */
static bool actor_slot_dispatch_98d5_step(Cpu *c, uint16_t pc) {
  static const uint16_t sites[] = {
    0x98d5,0x98d8,0x98db,0x98dd,0x98de,0x98e1,0x98e3,0x98e5,
    0x98e7,0x98ea,0x98ec,0x98ef,0x98f0,0x98f2,0x98f5,0x98f7,
    0x98f9,0x98fd,0x9900,0x9901,0x9905,0x9906,0x9907,0x9908,
    0x9909,0x990c,0x990e
  };
  if((c->k & 0x7fu) != 0 || c->e || c->xf || c->d || !c->mf) return false;
  bool known = false;
  for(unsigned i=0;i<sizeof(sites)/sizeof(sites[0]);i++) known |= pc == sites[i];
  if(!known) return false;
  (void)fetch(c);
  switch(pc) {
    case 0x98d5: index_constant(c,false,0); break;
    case 0x98d8: load_a_abs_x8(c,0x0d00); break;
    case 0x98db: branch_to(c,!c->n,0x9905); break;
    case 0x98dd: phx16(c); break;
    case 0x98de: load_a_abs_x8(c,0x0d01); break;
    case 0x98e1: store_dp(c,0x04,true); break;
    case 0x98e3: load_dp8(c,0x61); break;
    case 0x98e5: branch_to(c,!c->z,0x98f2); break;
    case 0x98e7: video_load(c,0x0d86,false); break;
    case 0x98ea: branch_to(c,c->n,0x98f2); break;
    case 0x98ec: video_load(c,0x0d85,false); break;
    case 0x98ef: asl_a8(c); break;
    case 0x98f0: branch_to(c,true,0x98f5); break;
    case 0x98f2: load_a_abs_x8(c,0x0d00); break;
    case 0x98f5: and_constant8(c,0x0f); break;
    case 0x98f7: store_dp(c,0x03,true); break;
    case 0x98f9: jsl_to(c,0xc95a); break;
    case 0x98fd: inc_abs8_native(c,0x0715); break;
    case 0x9900: plx16(c); break;
    case 0x9901: jsl_to(c,0xc91f); break;
    case 0x9905: case 0x9906: case 0x9907: case 0x9908:
      implied(c); c->x++; zn(c,c->x,false); break;
    case 0x9909: cpx16_constant(c,0x0020); break;
    case 0x990c: branch_to(c,!c->c,0x98d8); break;
    case 0x990e: return_long(c); break;
  }
  return true;
}
static void store_a_long_x8(Cpu *c, uint32_t base) {
  (void)fetch(c); (void)fetch(c); (void)fetch(c); sample_interrupt(c);
  c->write(c->mem, (base + c->x) & 0xffffffu, (uint8_t)c->a);
}
static void ldx_abs16(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c);
  uint32_t low = ((uint32_t)c->db << 16) | address;
  uint16_t value = c->read(c->mem, low); sample_interrupt(c);
  c->x = value | ((uint16_t)c->read(c->mem, (low + 1) & 0xffffffu) << 8);
  zn(c, c->x, false);
}
static void nop_native(Cpu *c) { implied(c); }
static void jmp_abs_native(Cpu *c, uint16_t dest) {
  (void)fetch(c); sample_interrupt(c); (void)fetch(c); c->pc = dest;
}
static void mvn_7e_00(Cpu *c) {
  (void)fetch(c); (void)fetch(c);
  c->db = 0x7e;
  uint8_t v = c->read(c->mem, c->x);
  c->write(c->mem, ((uint32_t)0x7e << 16) | c->y, v);
  c->a--; c->x++; c->y++;
  if(c->xf) { c->x &= 255u; c->y &= 255u; }
  if(c->a != 0xffffu) c->pc = (uint16_t)(c->pc - 3u);
  idle(c); sample_interrupt(c); idle(c);
}

static void mvn_7f_00(Cpu *c) {
  (void)fetch(c); (void)fetch(c);
  c->db = 0x7f;
  uint8_t v = c->read(c->mem, c->x);
  c->write(c->mem, ((uint32_t)0x7f << 16) | c->y, v);
  c->a--; c->x++; c->y++;
  if(c->xf) { c->x &= 255u; c->y &= 255u; }
  if(c->a != 0xffffu) c->pc = (uint16_t)(c->pc - 3u);
  idle(c); sample_interrupt(c); idle(c);
}
static void stz_abs_x8_only(Cpu *c, uint16_t base) {
  /* STZ abs,X — always pays index cycle when x16 */
  (void)fetch(c); (void)fetch(c); idle(c); sample_interrupt(c);
  c->write(c->mem, (((uint32_t)c->db << 16) + base + c->x) & 0xffffffu, 0);
}
static void tax16_from_a8(Cpu *c) {
  /* TAX while m=8,x=16: transfers 16-bit A? Actually TAX uses xf for width of X,
   * and transfers A (full 16 if x16). LakeSnes: if xf, x=a&ff; else x=a. */
  implied(c); c->x = c->a; zn(c, c->x, false);
}
/* $008D2B: hardware multiply $01BD * 5, index tables $02E629 / $02E3FB, A += $30. */
static bool mul_table_8d2b_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x8d2b: case 0x8d2e: case 0x8d31: case 0x8d33: case 0x8d36: case 0x8d37:
    case 0x8d38: case 0x8d39: case 0x8d3c: case 0x8d3e: case 0x8d3f: case 0x8d43:
    case 0x8d45: case 0x8d46: case 0x8d4a: case 0x8d4b: case 0x8d4d: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8d2b: video_load(c, 0x01bd, false); break;
    case 0x8d2e: store_register8(c, 0x4202); break;
    case 0x8d31: byte_constant(c, 0x05); break;
    case 0x8d33: store_register8(c, 0x4203); break;
    case 0x8d36: case 0x8d37: case 0x8d3e: xba_native(c); break;
    case 0x8d38: nop_native(c); break;
    case 0x8d39: ldx_abs16(c, 0x4216); break;
    case 0x8d3c: byte_constant(c, 0x00); break;
    case 0x8d3f: load_a_long_x8(c, 0x02e629); break;
    case 0x8d43: and_constant8(c, 0x1f); break;
    case 0x8d45: tax16_from_a8(c); break;
    case 0x8d46: load_a_long_x8(c, 0x02e3fb); break;
    case 0x8d4a: implied(c); c->c = false; break;
    case 0x8d4b: adc8_constant(c, 0x30); break;
    case 0x8d4d: return_long(c); break;
  }
  return true;
}

/* $008E76: copy scroll words to $01DE/$01E0, set $01A1=$80, call $8D2B/$02D812, JMP $8B7A. */
static bool scene_path_8e76_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  if((pc == 0x8e76 || pc == 0x8e78 || pc == 0x8e7b || pc == 0x8e7e || pc == 0x8e81) && c->mf)
    return false;
  if((pc == 0x8e84 || pc == 0x8e86 || pc == 0x8e88 || pc == 0x8e8b || pc == 0x8e8f ||
      pc == 0x8e93) && !c->mf) return false;
  switch(pc) {
    case 0x8e76: case 0x8e78: case 0x8e7b: case 0x8e7e: case 0x8e81: case 0x8e84:
    case 0x8e86: case 0x8e88: case 0x8e8b: case 0x8e8f: case 0x8e93: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8e76: set_m(c, false); break;
    case 0x8e78: video_load(c, 0x01a2, false); break;
    case 0x8e7b: store_a_abs16(c, 0x01de); break;
    case 0x8e7e: video_load(c, 0x01a4, false); break;
    case 0x8e81: store_a_abs16(c, 0x01e0); break;
    case 0x8e84: set_m(c, true); break;
    case 0x8e86: byte_constant(c, 0x80); break;
    case 0x8e88: store_register8(c, 0x01a1); break;
    case 0x8e8b: jsl_to(c, 0x8d2b); break;
    case 0x8e8f: jsl_to(c, 0xd812); break;
    case 0x8e93: jmp_abs_native(c, 0x8b7a); break;
  }
  return true;
}

/* $0090E6: program DMA0 from $7E9000 / length DP+$89 and fire. */
static bool dma_vram_from_7e9000_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x90e6: case 0x90e9: case 0x90ec: case 0x90ef: case 0x90f2: case 0x90f4:
    case 0x90f7: case 0x90f9: case 0x90fc: case 0x90fe: case 0x9101: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x90e6: index_constant(c, true, 0x9000); break;
    case 0x90e9: store_y_abs16(c, 0x4302); break;
    case 0x90ec: index_constant(c, true, 0x1801); break;
    case 0x90ef: store_y_abs16(c, 0x4300); break;
    case 0x90f2: byte_constant(c, 0x7e); break;
    case 0x90f4: store_register8(c, 0x4304); break;
    case 0x90f7: load_dp_y16(c, 0x89); break;
    case 0x90f9: store_y_abs16(c, 0x4305); break;
    case 0x90fc: byte_constant(c, 0x01); break;
    case 0x90fe: store_register8(c, 0x420b); break;
    case 0x9101: return_long(c); break;
  }
  return true;
}

/* $00946F: DP+$00 = $08DD44; copy 0x3E words through $88C5. */
static bool palette_dd44_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  if((pc == 0x946f || pc == 0x9471 || pc == 0x9473 || pc == 0x9480 || pc == 0x9484) && !c->mf)
    return false;
  switch(pc) {
    case 0x946f: case 0x9471: case 0x9473: case 0x9475: case 0x9478: case 0x947a:
    case 0x947d: case 0x9480: case 0x9484: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x946f: sep_rep_native(c, 0x20, true); break;
    case 0x9471: byte_constant(c, 0x08); break;
    case 0x9473: store_dp(c, 0x02, true); break;
    case 0x9475: index_constant(c, true, 0xdd44); break;
    case 0x9478: store_dp_y16(c, 0x00); break;
    case 0x947a: index_constant(c, false, 0x003e); break;
    case 0x947d: index_constant(c, true, 0x003e); break;
    case 0x9480: jsl_to(c, 0x88c5); break;
    case 0x9484: return_long(c); break;
  }
  return true;
}

/* $0090C1: set decompress source $08:8000, VMAIN, call C559/C68C, DMA to $2000, palette. */
static bool scene_gfx_90c1_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x90c1: case 0x90c4: case 0x90c6: case 0x90c8: case 0x90ca: case 0x90cc:
    case 0x90cf: case 0x90d3: case 0x90d7: case 0x90da: case 0x90dd: case 0x90e1:
    case 0x90e5: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x90c1: index_constant(c, true, 0x8000); break;
    case 0x90c4: store_dp_y16(c, 0x83); break;
    case 0x90c6: byte_constant(c, 0x08); break;
    case 0x90c8: store_dp(c, 0x85, true); break;
    case 0x90ca: byte_constant(c, 0x80); break;
    case 0x90cc: store_register8(c, 0x2115); break;
    case 0x90cf: jsl_to(c, 0xc559); break;
    case 0x90d3: jsl_to(c, 0xc68c); break;
    case 0x90d7: index_constant(c, true, 0x2000); break;
    case 0x90da: store_y_abs16(c, 0x2116); break;
    case 0x90dd: jsl_to(c, 0x90e6); break;
    case 0x90e1: jsl_to(c, 0x946f); break;
    case 0x90e5: return_long(c); break;
  }
  return true;
}
/* $00C68C: rearrange 16-byte tiles in $7E9000 via DP scratch $95..$A4 and MVN. */
static bool tile_rearrange_c68c_step(Cpu *c, uint16_t pc) {
  if(c->e || c->d) return false;
  const bool m16 = (pc == 0xc690 || pc == 0xc692 || pc == 0xc693 || pc == 0xc695 ||
                    pc == 0xc6f3 || pc == 0xc6f4 || pc == 0xc6f7 || pc == 0xc6fa);
  const bool sep_from_m16 = (pc == 0xc697 || pc == 0xc6fd);
  const bool rep_to_m16 = (pc == 0xc68e || pc == 0xc6f1);
  if(m16 && c->mf) return false;
  if(sep_from_m16 && c->mf) return false; /* SEP #$20 enters in m16 */
  if(rep_to_m16 && !c->mf) return false; /* REP #$20 enters in m8 */
  if(!m16 && !sep_from_m16 && !rep_to_m16 && pc != 0xc68c && pc != 0xc68d && !c->mf)
    return false;
  if(pc > 0xc699 && c->xf) return false;
  switch(pc) {

    case 0xc68c: case 0xc68d: case 0xc68e: case 0xc690: case 0xc692: case 0xc693:
    case 0xc695: case 0xc697: case 0xc699: case 0xc69b: case 0xc69d: case 0xc69f:
    case 0xc6a0: case 0xc6a1: case 0xc6a4: case 0xc6a6: case 0xc6a9: case 0xc6ab:
    case 0xc6ae: case 0xc6b0: case 0xc6b3: case 0xc6b5: case 0xc6b8: case 0xc6ba:
    case 0xc6bd: case 0xc6bf: case 0xc6c2: case 0xc6c4: case 0xc6c7: case 0xc6c9:
    case 0xc6cc: case 0xc6ce: case 0xc6d1: case 0xc6d3: case 0xc6d6: case 0xc6d8:
    case 0xc6db: case 0xc6dd: case 0xc6e0: case 0xc6e2: case 0xc6e5: case 0xc6e7:
    case 0xc6ea: case 0xc6ec: case 0xc6ef: case 0xc6f1: case 0xc6f3: case 0xc6f4:
    case 0xc6f7: case 0xc6fa: case 0xc6fd: case 0xc6ff: case 0xc700: case 0xc702:
    case 0xc704: case 0xc705: case 0xc706: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xc68c: php_native(c); break;
    case 0xc68d: phb_native(c); break;
    case 0xc68e: set_m(c, false); break;
    case 0xc690: load_dp16_to_a(c, 0x89); break;
    case 0xc692: implied(c); c->c = false; break;
    case 0xc693: adc_dp16(c, 0x86); break;
    case 0xc695: store_dp(c, 0x8d, false); break;
    case 0xc697: set_m(c, true); break;
    case 0xc699: sep_rep_native(c, 0x10, false); break;
    case 0xc69b: load_dp_x16(c, 0x86); break;
    case 0xc69d: load_dp8(c, 0x88); break;
    case 0xc69f: pha8(c); break;
    case 0xc6a0: plb_native(c); break;
    case 0xc6a1: load_a_abs_x8(c, 0x0000); break;
    case 0xc6a4: store_dp(c, 0x95, true); break;
    case 0xc6a6: load_a_abs_x8(c, 0x0001); break;
    case 0xc6a9: store_dp(c, 0x97, true); break;
    case 0xc6ab: load_a_abs_x8(c, 0x0002); break;
    case 0xc6ae: store_dp(c, 0x99, true); break;
    case 0xc6b0: load_a_abs_x8(c, 0x0003); break;
    case 0xc6b3: store_dp(c, 0x9b, true); break;
    case 0xc6b5: load_a_abs_x8(c, 0x0004); break;
    case 0xc6b8: store_dp(c, 0x9d, true); break;
    case 0xc6ba: load_a_abs_x8(c, 0x0005); break;
    case 0xc6bd: store_dp(c, 0x9f, true); break;
    case 0xc6bf: load_a_abs_x8(c, 0x0006); break;
    case 0xc6c2: store_dp(c, 0xa1, true); break;
    case 0xc6c4: load_a_abs_x8(c, 0x0007); break;
    case 0xc6c7: store_dp(c, 0xa3, true); break;
    case 0xc6c9: load_a_abs_x8(c, 0x0008); break;
    case 0xc6cc: store_dp(c, 0x96, true); break;
    case 0xc6ce: load_a_abs_x8(c, 0x0009); break;
    case 0xc6d1: store_dp(c, 0x98, true); break;
    case 0xc6d3: load_a_abs_x8(c, 0x000a); break;
    case 0xc6d6: store_dp(c, 0x9a, true); break;
    case 0xc6d8: load_a_abs_x8(c, 0x000b); break;
    case 0xc6db: store_dp(c, 0x9c, true); break;
    case 0xc6dd: load_a_abs_x8(c, 0x000c); break;
    case 0xc6e0: store_dp(c, 0x9e, true); break;
    case 0xc6e2: load_a_abs_x8(c, 0x000d); break;
    case 0xc6e5: store_dp(c, 0xa0, true); break;
    case 0xc6e7: load_a_abs_x8(c, 0x000e); break;
    case 0xc6ea: store_dp(c, 0xa2, true); break;
    case 0xc6ec: load_a_abs_x8(c, 0x000f); break;
    case 0xc6ef: store_dp(c, 0xa4, true); break;
    case 0xc6f1: set_m(c, false); break;
    case 0xc6f3: implied(c); c->y = c->x; zn(c, c->y, false); break;
    case 0xc6f4: index_constant(c, false, 0x0095); break;
    case 0xc6f7: lda16_constant(c, 0x000f); break;
    case 0xc6fa: mvn_7e_00(c); break;
    case 0xc6fd: set_m(c, true); break;
    case 0xc6ff: implied(c); c->x = c->y; zn(c, c->x, false); break;
    case 0xc700: cpx_dp16(c, 0x8d); break;
    case 0xc702: branch_to(c, !c->c, 0xc6a1); break;
    case 0xc704: plb_native(c); break;
    case 0xc705: plp_native(c); break;
    case 0xc706: return_long(c); break;
  }
  return true;
}

/* $00C707: rearrange 16-byte tiles via DP scratch $95..$A4 and MVN into bank $7F
 * (sibling of $C68C which MVNs into bank $7E). */
static bool tile_rearrange_c707_step(Cpu *c, uint16_t pc) {
  if(c->e || c->d) return false;
  const bool m16 = (pc == 0xc70b || pc == 0xc70d || pc == 0xc70e || pc == 0xc710 ||
                    pc == 0xc76e || pc == 0xc76f || pc == 0xc772 || pc == 0xc775);
  const bool sep_from_m16 = (pc == 0xc712 || pc == 0xc778);
  const bool rep_to_m16 = (pc == 0xc709 || pc == 0xc76c);
  if(m16 && c->mf) return false;
  if(sep_from_m16 && c->mf) return false;
  if(rep_to_m16 && !c->mf) return false;
  if(!m16 && !sep_from_m16 && !rep_to_m16 && pc != 0xc707 && pc != 0xc708 && !c->mf)
    return false;
  if(pc > 0xc714 && c->xf) return false;
  switch(pc) {
    case 0xc707: case 0xc708: case 0xc709: case 0xc70b: case 0xc70d: case 0xc70e:
    case 0xc710: case 0xc712: case 0xc714: case 0xc716: case 0xc718: case 0xc71a:
    case 0xc71b: case 0xc71c: case 0xc71f: case 0xc721: case 0xc724: case 0xc726:
    case 0xc729: case 0xc72b: case 0xc72e: case 0xc730: case 0xc733: case 0xc735:
    case 0xc738: case 0xc73a: case 0xc73d: case 0xc73f: case 0xc742: case 0xc744:
    case 0xc747: case 0xc749: case 0xc74c: case 0xc74e: case 0xc751: case 0xc753:
    case 0xc756: case 0xc758: case 0xc75b: case 0xc75d: case 0xc760: case 0xc762:
    case 0xc765: case 0xc767: case 0xc76a: case 0xc76c: case 0xc76e: case 0xc76f:
    case 0xc772: case 0xc775: case 0xc778: case 0xc77a: case 0xc77b: case 0xc77d:
    case 0xc77f: case 0xc780: case 0xc781: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xc707: php_native(c); break;
    case 0xc708: phb_native(c); break;
    case 0xc709: set_m(c, false); break;
    case 0xc70b: load_dp16_to_a(c, 0x89); break;
    case 0xc70d: implied(c); c->c = false; break;
    case 0xc70e: adc_dp16(c, 0x86); break;
    case 0xc710: store_dp(c, 0x8d, false); break;
    case 0xc712: set_m(c, true); break;
    case 0xc714: sep_rep_native(c, 0x10, false); break;
    case 0xc716: load_dp_x16(c, 0x86); break;
    case 0xc718: load_dp8(c, 0x88); break;
    case 0xc71a: pha8(c); break;
    case 0xc71b: plb_native(c); break;
    case 0xc71c: load_a_abs_x8(c, 0x0000); break;
    case 0xc71f: store_dp(c, 0x95, true); break;
    case 0xc721: load_a_abs_x8(c, 0x0001); break;
    case 0xc724: store_dp(c, 0x97, true); break;
    case 0xc726: load_a_abs_x8(c, 0x0002); break;
    case 0xc729: store_dp(c, 0x99, true); break;
    case 0xc72b: load_a_abs_x8(c, 0x0003); break;
    case 0xc72e: store_dp(c, 0x9b, true); break;
    case 0xc730: load_a_abs_x8(c, 0x0004); break;
    case 0xc733: store_dp(c, 0x9d, true); break;
    case 0xc735: load_a_abs_x8(c, 0x0005); break;
    case 0xc738: store_dp(c, 0x9f, true); break;
    case 0xc73a: load_a_abs_x8(c, 0x0006); break;
    case 0xc73d: store_dp(c, 0xa1, true); break;
    case 0xc73f: load_a_abs_x8(c, 0x0007); break;
    case 0xc742: store_dp(c, 0xa3, true); break;
    case 0xc744: load_a_abs_x8(c, 0x0008); break;
    case 0xc747: store_dp(c, 0x96, true); break;
    case 0xc749: load_a_abs_x8(c, 0x0009); break;
    case 0xc74c: store_dp(c, 0x98, true); break;
    case 0xc74e: load_a_abs_x8(c, 0x000a); break;
    case 0xc751: store_dp(c, 0x9a, true); break;
    case 0xc753: load_a_abs_x8(c, 0x000b); break;
    case 0xc756: store_dp(c, 0x9c, true); break;
    case 0xc758: load_a_abs_x8(c, 0x000c); break;
    case 0xc75b: store_dp(c, 0x9e, true); break;
    case 0xc75d: load_a_abs_x8(c, 0x000d); break;
    case 0xc760: store_dp(c, 0xa0, true); break;
    case 0xc762: load_a_abs_x8(c, 0x000e); break;
    case 0xc765: store_dp(c, 0xa2, true); break;
    case 0xc767: load_a_abs_x8(c, 0x000f); break;
    case 0xc76a: store_dp(c, 0xa4, true); break;
    case 0xc76c: set_m(c, false); break;
    case 0xc76e: implied(c); c->y = c->x; zn(c, c->y, false); break;
    case 0xc76f: index_constant(c, false, 0x0095); break;
    case 0xc772: lda16_constant(c, 0x000f); break;
    case 0xc775: mvn_7f_00(c); break;
    case 0xc778: set_m(c, true); break;
    case 0xc77a: implied(c); c->x = c->y; zn(c, c->x, false); break;
    case 0xc77b: cpx_dp16(c, 0x8d); break;
    case 0xc77d: branch_to(c, !c->c, 0xc71c); break;
    case 0xc77f: plb_native(c); break;
    case 0xc780: plp_native(c); break;
    case 0xc781: return_long(c); break;
  }
  return true;
}
/* $00C559: bit-packed decompress from DB:(DP+$83) into $7E9000; length at stream head. */
static bool decompress_c559_step(Cpu *c, uint16_t pc) {
  if(c->e || c->d) return false;
  const bool m16 = (pc == 0xc5a4 || pc == 0xc5a7 || pc == 0xc5a8 ||
                    pc == 0xc5a9 || pc == 0xc5aa || pc == 0xc5ab || pc == 0xc5ac ||
                    pc == 0xc5ad || pc == 0xc5ae || pc == 0xc5b0 || pc == 0xc5b1 ||
                    pc == 0xc5b2 || pc == 0xc5b4 || pc == 0xc5d0);
  if(m16 && c->mf) return false;
  /* SEP/REP at C5A2/C5B6/C5D1 flip M; enter with the pre-instruction width. */
  if(!m16 && pc != 0xc559 && pc != 0xc55a && pc != 0xc55b && pc != 0xc55d &&
     pc != 0xc5b6 && pc != 0xc5d1 && !c->mf)
    return false;
  if(pc > 0xc55d && c->xf) return false;
  switch(pc) {
    case 0xc559: case 0xc55a: case 0xc55b: case 0xc55d: case 0xc55f: case 0xc562:
    case 0xc564: case 0xc566: case 0xc568: case 0xc56a: case 0xc56c: case 0xc56d:
    case 0xc56e: case 0xc571: case 0xc573: case 0xc574: case 0xc575: case 0xc578:
    case 0xc57a: case 0xc57c: case 0xc57e: case 0xc581: case 0xc583: case 0xc584:
    case 0xc586: case 0xc588: case 0xc58a: case 0xc58c: case 0xc58e: case 0xc591:
    case 0xc595: case 0xc596: case 0xc597: case 0xc599: case 0xc59c: case 0xc59e:
    case 0xc59f: case 0xc5a0: case 0xc5a2: case 0xc5a4: case 0xc5a7: case 0xc5a8:
    case 0xc5a9: case 0xc5aa: case 0xc5ab: case 0xc5ac: case 0xc5ad: case 0xc5ae:
    case 0xc5b0: case 0xc5b1: case 0xc5b2: case 0xc5b4: case 0xc5b6: case 0xc5b8:
    case 0xc5ba: case 0xc5bc: case 0xc5be: case 0xc5c2: case 0xc5c3: case 0xc5c5:
    case 0xc5c7: case 0xc5c9: case 0xc5cb: case 0xc5ce: case 0xc5d0: case 0xc5d1:
    case 0xc5d3: case 0xc5d4: case 0xc5d6: case 0xc5d7: case 0xc5d8: case 0xc5db:
    case 0xc5de: case 0xc5df: case 0xc5e0: case 0xc5e2: case 0xc5e4: case 0xc5e5:
    case 0xc5e6: case 0xc5e8: case 0xc5ea: case 0xc5eb: case 0xc5ec: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xc559: php_native(c); break;
    case 0xc55a: phb_native(c); break;
    case 0xc55b: sep_rep_native(c, 0x20, true); break;
    case 0xc55d: sep_rep_native(c, 0x10, false); break;
    case 0xc55f: index_constant(c, false, 0x9000); break;
    case 0xc562: store_dp_x16(c, 0x86); break;
    case 0xc564: byte_constant(c, 0x7e); break;
    case 0xc566: store_dp(c, 0x88, true); break;
    case 0xc568: load_dp_y16(c, 0x83); break;
    case 0xc56a: load_dp8(c, 0x85); break;
    case 0xc56c: pha8(c); break;
    case 0xc56d: plb_native(c); break;
    case 0xc56e: ldx_abs_y16(c, 0x0000); break;
    case 0xc571: store_dp_x16(c, 0x89); break;
    case 0xc573: case 0xc574: case 0xc583: case 0xc595: case 0xc5a7: case 0xc5a8:
    case 0xc5df: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xc575: index_constant(c, false, 0x0000); break;
    case 0xc578: stz_dp8(c, 0x8c); break;
    case 0xc57a: load_dp8(c, 0x8c); break;
    case 0xc57c: branch_to(c, !c->z, 0xc588); break;
    case 0xc57e: case 0xc58e: case 0xc599: load_a_abs_y8(c, 0x0000); break;
    case 0xc581: store_dp(c, 0x8b, true); break;
    case 0xc584: byte_constant(c, 0x08); break;
    case 0xc586: store_dp(c, 0x8c, true); break;
    case 0xc588: dec_dp8(c, 0x8c); break;
    case 0xc5c3: case 0xc5e0: dec_dp8(c, 0x8d); break;
    case 0xc5c7: dec_dp8(c, 0x8f); break;
    case 0xc58a: lsr_dp8(c, 0x8b); break;
    case 0xc58c: branch_to(c, !c->c, 0xc599); break;
    case 0xc591: case 0xc5be: store_a_long_x8(c, 0x7e9000); break;
    case 0xc596: case 0xc5c2: case 0xc5de: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xc597: branch_to(c, true, 0xc5e6); break;
    case 0xc59c: and_constant8(c, 0x0f); break;
    case 0xc59e: case 0xc59f: inc_a8(c); break;
    case 0xc5a0: store_dp(c, 0x8d, true); break;
    case 0xc5a2: set_m(c, false); break;
    case 0xc5a4: lda_abs_y16(c, 0x0000); break;
    case 0xc5a9: phy16(c); break;
    case 0xc5aa: case 0xc5ab: case 0xc5ac: case 0xc5ad: lsr_a16(c); break;
    case 0xc5ae: store_dp(c, 0x8f, false); break;
    case 0xc5b0: implied(c); c->a = c->x; zn(c, c->a, false); break;
    case 0xc5b1: implied(c); c->c = true; break;
    case 0xc5b2: sbc_dp16(c, 0x8f); break;
    case 0xc5b4: branch_to(c, !c->n, 0xc5d0); break;
    case 0xc5b6: case 0xc5d1: set_m(c, true); break;
    case 0xc5b8: eor_constant8(c, 0xff); break;
    case 0xc5ba: store_dp(c, 0x8f, true); break;
    case 0xc5bc: byte_constant(c, 0x00); break;
    case 0xc5c5: branch_to(c, c->n, 0xc5e5); break;
    case 0xc5c9: branch_to(c, !c->n, 0xc5be); break;
    case 0xc5cb: index_constant(c, true, 0x0000); break;
    case 0xc5ce: branch_to(c, true, 0xc5d3); break;
    case 0xc5d0: implied(c); c->y = c->a; zn(c, c->y, false); break;
    case 0xc5d3: phb_native(c); break;
    case 0xc5d4: byte_constant(c, 0x7e); break;
    case 0xc5d6: pha8(c); break;
    case 0xc5d7: case 0xc5e4: case 0xc5ea: plb_native(c); break;
    case 0xc5d8: load_a_abs_y8(c, 0x9000); break;
    case 0xc5db: store_a_abs_x8(c, 0x9000); break;
    case 0xc5e2: branch_to(c, !c->n, 0xc5d8); break;
    case 0xc5e5: ply16(c); break;
    case 0xc5e6: cpx_dp16(c, 0x89); break;
    case 0xc5e8: branch_to(c, !c->c, 0xc57a); break;
    case 0xc5eb: plp_native(c); break;
    case 0xc5ec: return_long(c); break;
  }
  return true;
}

/* $00C5ED: bit-packed decompress like $C559, but seeds WMADD from DP+$86/$88 and
 * also streams each output byte through $2180 (WRAM port) while filling $7E9000.
 * Open callee of $05:96F3 (with $C707). */
static bool decompress_c5ed_step(Cpu *c, uint16_t pc) {
  if(c->e || c->d) return false;
  const bool m16 = (pc == 0xc63c || pc == 0xc63f || pc == 0xc640 ||
                    pc == 0xc641 || pc == 0xc642 || pc == 0xc643 || pc == 0xc644 ||
                    pc == 0xc645 || pc == 0xc646 || pc == 0xc648 || pc == 0xc649 ||
                    pc == 0xc64a || pc == 0xc64c || pc == 0xc66b);
  if(m16 && c->mf) return false;
  if(!m16 && pc != 0xc5ed && pc != 0xc5ee && pc != 0xc5ef && pc != 0xc5f1 &&
     pc != 0xc64e && pc != 0xc66c && !c->mf)
    return false;
  if(pc > 0xc5f1 && c->xf) return false;
  switch(pc) {
    case 0xc5ed: case 0xc5ee: case 0xc5ef: case 0xc5f1: case 0xc5f3: case 0xc5f5:
    case 0xc5f8: case 0xc5fa: case 0xc5fd: case 0xc5ff: case 0xc601: case 0xc602:
    case 0xc603: case 0xc606: case 0xc608: case 0xc609: case 0xc60a: case 0xc60d:
    case 0xc60f: case 0xc611: case 0xc613: case 0xc616: case 0xc618: case 0xc619:
    case 0xc61b: case 0xc61d: case 0xc61f: case 0xc621: case 0xc623: case 0xc626:
    case 0xc62a: case 0xc62d: case 0xc62e: case 0xc62f: case 0xc631: case 0xc634:
    case 0xc636: case 0xc637: case 0xc638: case 0xc63a: case 0xc63c: case 0xc63f:
    case 0xc640: case 0xc641: case 0xc642: case 0xc643: case 0xc644: case 0xc645:
    case 0xc646: case 0xc648: case 0xc649: case 0xc64a: case 0xc64c: case 0xc64e:
    case 0xc650: case 0xc652: case 0xc654: case 0xc656: case 0xc65a: case 0xc65d:
    case 0xc65e: case 0xc660: case 0xc662: case 0xc664: case 0xc666: case 0xc669:
    case 0xc66b: case 0xc66c: case 0xc66e: case 0xc66f: case 0xc671: case 0xc672:
    case 0xc673: case 0xc676: case 0xc679: case 0xc67d: case 0xc67e: case 0xc67f:
    case 0xc681: case 0xc683: case 0xc684: case 0xc685: case 0xc687: case 0xc689:
    case 0xc68a: case 0xc68b: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xc5ed: php_native(c); break;
    case 0xc5ee: phb_native(c); break;
    case 0xc5ef: sep_rep_native(c, 0x20, true); break;
    case 0xc5f1: sep_rep_native(c, 0x10, false); break;
    case 0xc5f3: load_dp_x16(c, 0x86); break;
    case 0xc5f5: stx_abs16(c, 0x2181); break;
    case 0xc5f8: load_dp8(c, 0x88); break;
    case 0xc5fa: store_register8(c, 0x2183); break;
    case 0xc5fd: load_dp_y16(c, 0x83); break;
    case 0xc5ff: load_dp8(c, 0x85); break;
    case 0xc601: pha8(c); break;
    case 0xc602: plb_native(c); break;
    case 0xc603: ldx_abs_y16(c, 0x0000); break;
    case 0xc606: store_dp_x16(c, 0x89); break;
    case 0xc608: case 0xc609: case 0xc618: case 0xc62d: case 0xc63f: case 0xc640:
    case 0xc67e: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xc60a: index_constant(c, false, 0x0000); break;
    case 0xc60d: stz_dp8(c, 0x8c); break;
    case 0xc60f: load_dp8(c, 0x8c); break;
    case 0xc611: branch_to(c, !c->z, 0xc61d); break;
    case 0xc613: case 0xc623: case 0xc631: load_a_abs_y8(c, 0x0000); break;
    case 0xc616: store_dp(c, 0x8b, true); break;
    case 0xc619: byte_constant(c, 0x08); break;
    case 0xc61b: store_dp(c, 0x8c, true); break;
    case 0xc61d: dec_dp8(c, 0x8c); break;
    case 0xc65e: case 0xc67f: dec_dp8(c, 0x8d); break;
    case 0xc662: dec_dp8(c, 0x8f); break;
    case 0xc61f: lsr_dp8(c, 0x8b); break;
    case 0xc621: branch_to(c, !c->c, 0xc631); break;
    case 0xc626: case 0xc656: store_a_long_x8(c, 0x7e9000); break;
    case 0xc62a: case 0xc65a: store_register8(c, 0x2180); break;
    case 0xc62e: case 0xc65d: case 0xc67d: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xc62f: branch_to(c, true, 0xc685); break;
    case 0xc634: and_constant8(c, 0x0f); break;
    case 0xc636: case 0xc637: inc_a8(c); break;
    case 0xc638: store_dp(c, 0x8d, true); break;
    case 0xc63a: set_m(c, false); break;
    case 0xc63c: lda_abs_y16(c, 0x0000); break;
    case 0xc641: phy16(c); break;
    case 0xc642: case 0xc643: case 0xc644: case 0xc645: lsr_a16(c); break;
    case 0xc646: store_dp(c, 0x8f, false); break;
    case 0xc648: implied(c); c->a = c->x; zn(c, c->a, false); break;
    case 0xc649: implied(c); c->c = true; break;
    case 0xc64a: sbc_dp16(c, 0x8f); break;
    case 0xc64c: branch_to(c, !c->n, 0xc66b); break;
    case 0xc64e: case 0xc66c: set_m(c, true); break;
    case 0xc650: eor_constant8(c, 0xff); break;
    case 0xc652: store_dp(c, 0x8f, true); break;
    case 0xc654: byte_constant(c, 0x00); break;
    case 0xc660: branch_to(c, c->n, 0xc684); break;
    case 0xc664: branch_to(c, !c->n, 0xc656); break;
    case 0xc666: index_constant(c, true, 0x0000); break;
    case 0xc669: branch_to(c, true, 0xc66e); break;
    case 0xc66b: implied(c); c->y = c->a; zn(c, c->y, false); break;
    case 0xc66e: phb_native(c); break;
    case 0xc66f: byte_constant(c, 0x7e); break;
    case 0xc671: pha8(c); break;
    case 0xc672: case 0xc683: case 0xc689: plb_native(c); break;
    case 0xc673: load_a_abs_y8(c, 0x9000); break;
    case 0xc676: store_a_abs_x8(c, 0x9000); break;
    case 0xc679: sta_long8(c, 0x002180); break;
    case 0xc681: branch_to(c, !c->n, 0xc673); break;
    case 0xc684: ply16(c); break;
    case 0xc685: cpx_dp16(c, 0x89); break;
    case 0xc687: branch_to(c, !c->c, 0xc60f); break;
    case 0xc68a: plp_native(c); break;
    case 0xc68b: return_long(c); break;
  }
  return true;
}
/* $0485B6: allocate / clear an 8-slot ID table at $1309; A=$80 clears all. */
static bool slot_alloc_85b6_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x04 || c->e || c->d) return false;
  if(pc != 0x85b6 && pc != 0x85b7 && pc != 0x85b9 && c->xf) return false;
  if((pc == 0x85b7 || (pc >= 0x85bb && pc <= 0x85f0)) && pc != 0x85b9) {
    /* after SEP, m8 except we don't have m16 sites */
  }
  if(pc > 0x85b9 && !c->mf && pc != 0x85b9) return false;
  switch(pc) {
    case 0x85b6: case 0x85b7: case 0x85b9: case 0x85bb: case 0x85bc: case 0x85bd:
    case 0x85c0: case 0x85c3: case 0x85c5: case 0x85c6: case 0x85c9: case 0x85cb:
    case 0x85cc: case 0x85cd: case 0x85ce: case 0x85cf: case 0x85d0: case 0x85d2:
    case 0x85d4: case 0x85d7: case 0x85d8: case 0x85d9: case 0x85da: case 0x85dd:
    case 0x85e0: case 0x85e1: case 0x85e4: case 0x85e6: case 0x85e8: case 0x85eb:
    case 0x85ee: case 0x85ef: case 0x85f0: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x85b6: php_native(c); break;
    case 0x85b7: sep_rep_native(c, 0x20, true); break;
    case 0x85b9: sep_rep_native(c, 0x10, false); break;
    case 0x85bb: phx16(c); break;
    case 0x85bc: pha8(c); break;
    case 0x85bd: case 0x85da: index_constant(c, false, 0x0000); break;
    case 0x85c0: load_a_abs_x8(c, 0x1309); break;
    case 0x85c3: branch_to(c, c->z, 0x85cf); break;
    case 0x85c5: case 0x85e0: implied(c); c->x++; zn(c, c->x, false); break;
    case 0x85c6: case 0x85e1: cpx16_constant(c, 0x0008); break;
    case 0x85c9: branch_to(c, !c->c, 0x85c0); break;
    case 0x85cb: case 0x85cf: pla8(c); break;
    case 0x85cc: case 0x85d7: case 0x85ee: plx16(c); break;
    case 0x85cd: case 0x85d8: case 0x85ef: plp_native(c); break;
    case 0x85ce: case 0x85d9: case 0x85f0: return_long(c); break;
    case 0x85d0: cmp_constant8(c, 0x80); break;
    case 0x85d2: branch_to(c, c->z, 0x85da); break;
    case 0x85d4: store_a_abs_x8(c, 0x1309); break;
    case 0x85dd: stz_abs_x8_only(c, 0x1309); break;
    case 0x85e4: branch_to(c, !c->c, 0x85dd); break;
    case 0x85e6: byte_constant(c, 0x80); break;
    case 0x85e8: store_register8(c, 0x1309); break;
    case 0x85eb: stz_abs8(c, 0x1313); break;
  }
  return true;
}

/* $02D812: if A is $33/$35/$37, snapshot scroll words into $01C0..$01CE pairs; else RTL. */
static bool scroll_snapshot_d812_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x02 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xd812: case 0xd814: case 0xd816: case 0xd818: case 0xd81a: case 0xd81c:
    case 0xd81e: case 0xd81f: case 0xd822: case 0xd825: case 0xd828: case 0xd82b:
    case 0xd82e: case 0xd831: case 0xd834: case 0xd837: case 0xd83a: case 0xd83d:
    case 0xd840: case 0xd843: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xd812: cmp_constant8(c, 0x35); break;
    case 0xd814: branch_to(c, c->z, 0xd81f); break;
    case 0xd816: cmp_constant8(c, 0x37); break;
    case 0xd818: branch_to(c, c->z, 0xd81f); break;
    case 0xd81a: cmp_constant8(c, 0x33); break;
    case 0xd81c: branch_to(c, c->z, 0xd81f); break;
    case 0xd81e: case 0xd843: return_long(c); break;
    case 0xd81f: load_y_abs16(c, 0x01a2); break;
    case 0xd822: store_y_abs16(c, 0x01c0); break;
    case 0xd825: store_y_abs16(c, 0x01c4); break;
    case 0xd828: load_y_abs16(c, 0x01a4); break;
    case 0xd82b: store_y_abs16(c, 0x01c2); break;
    case 0xd82e: store_y_abs16(c, 0x01c6); break;
    case 0xd831: load_y_abs16(c, 0x0033); break;
    case 0xd834: store_y_abs16(c, 0x01c8); break;
    case 0xd837: store_y_abs16(c, 0x01cc); break;
    case 0xd83a: load_y_abs16(c, 0x0035); break;
    case 0xd83d: store_y_abs16(c, 0x01ca); break;
    case 0xd840: store_y_abs16(c, 0x01ce); break;
  }
  return true;
}

/* $009BBF: enqueue tiled VRAM transfers from [DP+$00] descriptor into $0800,
 * copying word pairs through $0A00 via $0711; advances dest DP+$03 by $20/row. */
static bool vram_queue_enqueue_tiled_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  if((pc == 0x9be9 || pc == 0x9beb || pc == 0x9bee || pc == 0x9bf0) && c->mf) return false;
  /* REP/SEP sites (9be7/9bf3/9c24) accept either M width, matching 9B79 style. */
  if((pc == 0x9bbf || pc == 0x9bc2 || pc == 0x9bc4 || pc == 0x9bc6 || pc == 0x9bc7 ||
      pc == 0x9bc9 || pc == 0x9bcb || pc == 0x9bcd || pc == 0x9bce || pc == 0x9bd0 ||
      pc == 0x9bd2 || pc == 0x9bd4 || pc == 0x9bd6 || pc == 0x9bd8 || pc == 0x9bda ||
      pc == 0x9bde || pc == 0x9be2 || pc == 0x9be4 ||
      pc == 0x9bf5 || pc == 0x9bf8 || pc == 0x9bfa || pc == 0x9bfb || pc == 0x9bfc ||
      pc == 0x9bff || pc == 0x9c02 || pc == 0x9c04 || pc == 0x9c06 || pc == 0x9c07 ||
      pc == 0x9c09 || pc == 0x9c0b || pc == 0x9c0c || pc == 0x9c0d || pc == 0x9c10 ||
      pc == 0x9c12 || pc == 0x9c15 || pc == 0x9c16 || pc == 0x9c18 || pc == 0x9c1b ||
      pc == 0x9c1c || pc == 0x9c1f || pc == 0x9c20 || pc == 0x9c22 ||
      pc == 0x9c26 || pc == 0x9c28 || pc == 0x9c2a || pc == 0x9c2c || pc == 0x9c2d ||
      pc == 0x9c2f || pc == 0x9c31 || pc == 0x9c33 || pc == 0x9c35 || pc == 0x9c37 ||
      pc == 0x9c39 || pc == 0x9c3a || pc == 0x9c3b || pc == 0x9c3c || pc == 0x9c3e ||
      pc == 0x9c40 || pc == 0x9c42 || pc == 0x9c44 || pc == 0x9c46 || pc == 0x9c48 ||
      pc == 0x9c4a) && !c->mf) return false;
  switch(pc) {
    case 0x9bbf: case 0x9bc2: case 0x9bc4: case 0x9bc6: case 0x9bc7: case 0x9bc9:
    case 0x9bcb: case 0x9bcd: case 0x9bce: case 0x9bd0: case 0x9bd2: case 0x9bd4:
    case 0x9bd6: case 0x9bd8: case 0x9bda: case 0x9bde: case 0x9be2: case 0x9be4:
    case 0x9be7: case 0x9be9: case 0x9beb: case 0x9bee: case 0x9bf0: case 0x9bf3:
    case 0x9bf5: case 0x9bf8: case 0x9bfa: case 0x9bfb: case 0x9bfc: case 0x9bff:
    case 0x9c02: case 0x9c04: case 0x9c06: case 0x9c07: case 0x9c09: case 0x9c0b:
    case 0x9c0c: case 0x9c0d: case 0x9c10: case 0x9c12: case 0x9c15: case 0x9c16:
    case 0x9c18: case 0x9c1b: case 0x9c1c: case 0x9c1f: case 0x9c20: case 0x9c22:
    case 0x9c24: case 0x9c26: case 0x9c28: case 0x9c2a: case 0x9c2c: case 0x9c2d:
    case 0x9c2f: case 0x9c31: case 0x9c33: case 0x9c35: case 0x9c37: case 0x9c39:
    case 0x9c3a: case 0x9c3b: case 0x9c3c: case 0x9c3e: case 0x9c40: case 0x9c42:
    case 0x9c44: case 0x9c46: case 0x9c48: case 0x9c4a: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x9bbf: index_constant(c, true, 0); break;
    case 0x9bc2: case 0x9bc7: case 0x9c02: case 0x9c07: lda_indirect_long_y8(c, 0x00); break;
    case 0x9bc4: store_dp(c, 0x05, true); break;
    case 0x9bc6: case 0x9bcd: case 0x9c06: case 0x9c0b: case 0x9c15: case 0x9c1b:
      implied(c); c->y++; zn(c, c->y, false); break;
    case 0x9bc9: store_dp(c, 0x06, true); break;
    case 0x9bcb: store_dp(c, 0x08, true); break;
    case 0x9bce: case 0x9c26: load_dp8(c, 0x05); break;
    case 0x9bd0: case 0x9c28: store_dp(c, 0x07, true); break;
    case 0x9bd2: byte_constant(c, 0x00); break;
    case 0x9bd4: store_dp(c, 0x09, true); break;
    case 0x9bd6: byte_constant(c, 0x0a); break;
    case 0x9bd8: store_dp(c, 0x0a, true); break;
    case 0x9bda: jsl_to(c, 0x8711); break;
    case 0x9bde: jsl_to(c, 0x8724); break;
    case 0x9be2: byte_constant(c, 0x80); break;
    case 0x9be4: store_a_abs_x8(c, 0x0800); break;
    case 0x9be7: set_m(c, false); break;
    case 0x9be9: load_dp16_to_a(c, 0x03); break;
    case 0x9beb: store_a_abs_x16(c, 0x0801); break;
    case 0x9bee: load_dp16_to_a(c, 0x09); break;
    case 0x9bf0: store_a_abs_x16(c, 0x0803); break;
    case 0x9bf3: case 0x9c24: set_m(c, true); break;
    case 0x9bf5: stz_abs_x8(c, 0x0805); break;
    case 0x9bf8: load_dp8(c, 0x05); break;
    case 0x9bfa: case 0x9c39: inc_a8(c); break;
    case 0x9bfb: case 0x9c3a: asl_a8(c); break;
    case 0x9bfc: store_a_abs_x8(c, 0x0806); break;
    case 0x9bff: stz_abs_x8(c, 0x0807); break;
    case 0x9c04: store_dp(c, 0x0b, true); break;
    case 0x9c09: store_dp(c, 0x0c, true); break;
    case 0x9c0c: phy16(c); break;
    case 0x9c0d: load_y_abs16(c, 0x0711); break;
    case 0x9c10: load_dp8(c, 0x0b); break;
    case 0x9c12: case 0x9c18: store_a_abs_y8(c, 0x0a00); break;
    case 0x9c16: load_dp8(c, 0x0c); break;
    case 0x9c1c: store_y_abs16(c, 0x0711); break;
    case 0x9c1f: ply16(c); break;
    case 0x9c20: dec_dp8(c, 0x07); break;
    case 0x9c22: branch_to(c, !c->n, 0x9c02); break;
    case 0x9c2a: load_dp8(c, 0x03); break;
    case 0x9c2c: case 0x9c3b: implied(c); c->c = false; break;
    case 0x9c2d: adc8_constant(c, 0x20); break;
    case 0x9c2f: store_dp(c, 0x03, true); break;
    case 0x9c31: load_dp8(c, 0x04); break;
    case 0x9c33: case 0x9c42: adc8_constant(c, 0x00); break;
    case 0x9c35: store_dp(c, 0x04, true); break;
    case 0x9c37: load_dp8(c, 0x05); break;
    case 0x9c3c: adc8_dp(c, 0x09); break;
    case 0x9c3e: store_dp(c, 0x09, true); break;
    case 0x9c40: load_dp8(c, 0x0a); break;
    case 0x9c44: store_dp(c, 0x0a, true); break;
    case 0x9c46: dec_dp8(c, 0x08); break;
    case 0x9c48: branch_to(c, !c->n, 0x9bda); break;
    case 0x9c4a: return_long(c); break;
  }
  return true;
}


/* ---- $06F14D scroll clamp + $0089B8 actor field accumulate ---- */
static void dec_a8(Cpu *c) {
  implied(c); set_a8(c, (uint8_t)((uint8_t)c->a - 1u));
}
static void sbc8_constant(Cpu *c, uint8_t value) {
  sample_interrupt(c); (void)fetch(c); sbc8_value(c, value);
}
static void cmp_dp8(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset); sample_interrupt(c);
  uint8_t value = c->read(c->mem, address);
  uint8_t a = (uint8_t)c->a;
  c->c = a >= value; zn(c, (uint8_t)(a - value), true);
}
static void adc_abs_x16(Cpu *c, uint16_t base) {
  uint32_t address = video_address(c, base, true);
  uint16_t value = c->read(c->mem, address); sample_interrupt(c);
  value |= (uint16_t)c->read(c->mem, (address + 1) & 0xffffffu) << 8;
  uint16_t old = c->a;
  unsigned result = (unsigned)old + value + (c->c ? 1u : 0u);
  c->v = (~(old ^ value) & (old ^ (uint16_t)result) & 0x8000u) != 0;
  c->c = result > 0xffffu;
  c->a = (uint16_t)result; zn(c, c->a, false);
}
static void adc_abs_x8(Cpu *c, uint16_t base) {
  uint32_t address = video_address(c, base, true);
  sample_interrupt(c);
  uint8_t value = c->read(c->mem, address);
  uint8_t old = (uint8_t)c->a;
  unsigned sum = (unsigned)old + value + (c->c ? 1u : 0u);
  c->v = ((~(old ^ value) & (old ^ (uint8_t)sum)) & 0x80u) != 0;
  c->c = sum > 0xffu;
  set_a8(c, (uint8_t)sum);
}

static void adc_abs_y8(Cpu *c, uint16_t base) {
  (void)fetch(c); (void)fetch(c); idle(c);
  sample_interrupt(c);
  uint8_t value = c->read(c->mem, (((uint32_t)c->db << 16) + base + c->y) & 0xffffffu);
  uint8_t old = (uint8_t)c->a;
  unsigned sum = (unsigned)old + value + (c->c ? 1u : 0u);
  c->v = ((~(old ^ value) & (old ^ (uint8_t)sum)) & 0x80u) != 0;
  c->c = sum > 0xffu;
  set_a8(c, (uint8_t)sum);
}
/* $06F14D: clamp BG scroll from $01A2/$01A4 using $0702/$0703 extents, then
 * walk X=$22..0 calling $06EF11 + frame upload $008324; clear V scroll if $01A1
 * negative. */
static bool scroll_clamp_f14d_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x06 || c->e || c->xf || c->d) return false;
  if((pc == 0xf1c0 || pc == 0xf1c3 || pc == 0xf1d4 || pc == 0xf1d7) && c->mf) return false;
  if((pc == 0xf14d || pc == 0xf150 || pc == 0xf151 || pc == 0xf153 || pc == 0xf156 ||
      pc == 0xf157 || pc == 0xf159 || pc == 0xf15b || pc == 0xf15e || pc == 0xf160 ||
      pc == 0xf162 || pc == 0xf164 || pc == 0xf166 || pc == 0xf168 || pc == 0xf16a ||
      pc == 0xf16c || pc == 0xf16d || pc == 0xf16f || pc == 0xf171 || pc == 0xf173 ||
      pc == 0xf175 || pc == 0xf178 || pc == 0xf179 || pc == 0xf17b || pc == 0xf17e ||
      pc == 0xf17f || pc == 0xf181 || pc == 0xf183 || pc == 0xf186 || pc == 0xf188 ||
      pc == 0xf18a || pc == 0xf18c || pc == 0xf18e || pc == 0xf190 || pc == 0xf192 ||
      pc == 0xf194 || pc == 0xf195 || pc == 0xf197 || pc == 0xf199 || pc == 0xf19b ||
      pc == 0xf19d || pc == 0xf19f || pc == 0xf1a1 || pc == 0xf1a3 || pc == 0xf1a5 ||
      pc == 0xf1a7 || pc == 0xf1aa || pc == 0xf1ad || pc == 0xf1ae || pc == 0xf1b0 ||
      pc == 0xf1b2 || pc == 0xf1b4 || pc == 0xf1b6 || pc == 0xf1b8 || pc == 0xf1ba ||
      pc == 0xf1bc || pc == 0xf1be || pc == 0xf1ca || pc == 0xf1ce ||
      pc == 0xf1d2 || pc == 0xf1de || pc == 0xf1df || pc == 0xf1e0 ||
      pc == 0xf1e2 || pc == 0xf1e5 || pc == 0xf1e7 || pc == 0xf1e9 || pc == 0xf1eb) &&
     !c->mf)
    return false;
  switch(pc) {
    case 0xf14d: case 0xf150: case 0xf151: case 0xf153: case 0xf156: case 0xf157:
    case 0xf159: case 0xf15b: case 0xf15e: case 0xf160: case 0xf162: case 0xf164:
    case 0xf166: case 0xf168: case 0xf16a: case 0xf16c: case 0xf16d: case 0xf16f:
    case 0xf171: case 0xf173: case 0xf175: case 0xf178: case 0xf179: case 0xf17b:
    case 0xf17e: case 0xf17f: case 0xf181: case 0xf183: case 0xf186: case 0xf188:
    case 0xf18a: case 0xf18c: case 0xf18e: case 0xf190: case 0xf192: case 0xf194:
    case 0xf195: case 0xf197: case 0xf199: case 0xf19b: case 0xf19d: case 0xf19f:
    case 0xf1a1: case 0xf1a3: case 0xf1a5: case 0xf1a7: case 0xf1aa: case 0xf1ad:
    case 0xf1ae: case 0xf1b0: case 0xf1b2: case 0xf1b4: case 0xf1b6: case 0xf1b8:
    case 0xf1ba: case 0xf1bc: case 0xf1be: case 0xf1c0: case 0xf1c2: case 0xf1c3:
    case 0xf1c6: case 0xf1c8: case 0xf1ca: case 0xf1ce: case 0xf1d2: case 0xf1d4:
    case 0xf1d6: case 0xf1d7: case 0xf1da: case 0xf1dc: case 0xf1de: case 0xf1df:
    case 0xf1e0: case 0xf1e2: case 0xf1e5: case 0xf1e7: case 0xf1e9: case 0xf1eb:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf14d: video_load(c, 0x0702, false); break;
    case 0xf150: case 0xf16c: case 0xf178: case 0xf194: dec_a8(c); break;
    case 0xf151: case 0xf179: case 0xf1a1: store_dp(c, 0x00, true); break;
    case 0xf153: video_load(c, 0x01a4, false); break;
    case 0xf156: case 0xf17e: implied(c); c->c = true; break;
    case 0xf157: sbc8_constant(c, 0x80); break;
    case 0xf159: case 0xf168: case 0xf171: store_dp(c, 0x35, true); break;
    case 0xf15b: video_load(c, 0x01a5, false); break;
    case 0xf15e: case 0xf186: sbc8_constant(c, 0x00); break;
    case 0xf160: branch_to(c, !c->c, 0xf16f); break;
    case 0xf162: case 0xf18a: cmp_dp8(c, 0x00); break;
    case 0xf164: branch_to(c, !c->c, 0xf173); break;
    case 0xf166: case 0xf18e: byte_constant(c, 0xf8); break;
    case 0xf16a: case 0xf192: load_dp8(c, 0x00); break;
    case 0xf1b4: load_dp8(c, 0x36); break;
    case 0xf16d: branch_to(c, true, 0xf173); break;
    case 0xf16f: case 0xf197: byte_constant(c, 0x00); break;
    case 0xf173: store_dp(c, 0x36, true); break;
    case 0xf175: video_load(c, 0x0703, false); break;
    case 0xf17b: video_load(c, 0x01a2, false); break;
    case 0xf17f: sbc8_constant(c, 0x88); break;
    case 0xf181: case 0xf190: case 0xf199: store_dp(c, 0x33, true); break;
    case 0xf183: video_load(c, 0x01a3, false); break;
    case 0xf188: branch_to(c, !c->c, 0xf197); break;
    case 0xf18c: branch_to(c, !c->c, 0xf19b); break;
    case 0xf195: branch_to(c, true, 0xf19b); break;
    case 0xf19b: store_dp(c, 0x34, true); break;
    case 0xf19d: case 0xf1b8: load_dp8(c, 0x33); break;
    case 0xf19f: case 0xf1b0: and_constant8(c, 0xf8); break;
    case 0xf1a3: load_dp8(c, 0x34); break;
    case 0xf1a5: store_dp(c, 0x01, true); break;
    case 0xf1a7: stz_abs8(c, 0x0800); break;
    case 0xf1aa: index_constant(c, false, 0x0022); break;
    case 0xf1ad: phx16(c); break;
    case 0xf1ae: load_dp8(c, 0x35); break;
    case 0xf1b2: store_dp(c, 0x02, true); break;
    case 0xf1b6: store_dp(c, 0x03, true); break;
    case 0xf1ba: and_constant8(c, 0x04); break;
    case 0xf1bc: branch_to(c, c->z, 0xf1ca); break;
    case 0xf1be: case 0xf1d2: set_m(c, false); break;
    case 0xf1c0: load_dp16_to_a(c, 0x02); break;
    case 0xf1c2: case 0xf1d6: implied(c); c->c = false; break;
    case 0xf1c3: case 0xf1d7: adc16_constant(c, 0x0008); break;
    case 0xf1c6: store_dp(c, 0x02, false); break;
    case 0xf1c8: case 0xf1dc: set_m(c, true); break;
    case 0xf1ca: jsl_to(c, 0xef11); break;
    case 0xf1ce: jsl_to(c, 0x8324); break;
    case 0xf1d4: load_dp16_to_a(c, 0x00); break;
    case 0xf1da: store_dp(c, 0x00, false); break;
    case 0xf1de: plx16(c); break;
    case 0xf1df: implied(c); c->x--; zn(c, c->x, false); break;
    case 0xf1e0: branch_to(c, !c->n, 0xf1ad); break;
    case 0xf1e2: video_load(c, 0x01a1, false); break;
    case 0xf1e5: branch_to(c, !c->n, 0xf1eb); break;
    case 0xf1e7: stz_dp8(c, 0x35); break;
    case 0xf1e9: stz_dp8(c, 0x36); break;
    case 0xf1eb: return_long(c); break;
  }
  return true;
}

/* $0089B8: accumulate actor fields from [DP+$00],Y into $0B07,X stride; call
 * $8A28 then $8A69. */
static bool actor_accum_89b8_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  if((pc == 0x89be || pc == 0x89c0 || pc == 0x89c1 || pc == 0x89c4 || pc == 0x89d6 ||
      pc == 0x89d8 || pc == 0x89e7 || pc == 0x89e9 || pc == 0x89ee || pc == 0x89f0 ||
      pc == 0x89f1 || pc == 0x89f4) && c->mf)
    return false;
  if((pc == 0x89b8 || pc == 0x89c9 || pc == 0x89cb || pc == 0x89cd || pc == 0x89d0 ||
      pc == 0x89dd || pc == 0x89df || pc == 0x89e1 || pc == 0x89f9 || pc == 0x89fb ||
      pc == 0x89fd || pc == 0x8a00 || pc == 0x8a01 || pc == 0x8a05 || pc == 0x8a06) &&
     !c->mf)
    return false;
  switch(pc) {
    case 0x89b8: case 0x89bc: case 0x89be: case 0x89c0: case 0x89c1: case 0x89c4:
    case 0x89c7: case 0x89c8: case 0x89c9: case 0x89cb: case 0x89cd: case 0x89d0:
    case 0x89d3: case 0x89d4: case 0x89d6: case 0x89d8: case 0x89db: case 0x89dc:
    case 0x89dd: case 0x89df: case 0x89e1: case 0x89e4: case 0x89e5: case 0x89e7:
    case 0x89e9: case 0x89ec: case 0x89ed: case 0x89ee: case 0x89f0: case 0x89f1:
    case 0x89f4: case 0x89f7: case 0x89f8: case 0x89f9: case 0x89fb: case 0x89fd:
    case 0x8a00: case 0x8a01: case 0x8a05: case 0x8a06: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x89b8: jsl_to(c, 0x8a28); break;
    case 0x89bc: case 0x89d4: case 0x89e5: set_m(c, false); break;
    case 0x89be: case 0x89d6: case 0x89e7: case 0x89ee: lda_indirect_long_y16(c, 0x00); break;
    case 0x89c0: case 0x89f0: implied(c); c->c = false; break;
    case 0x89c1: adc_abs_x16(c, 0x0b07); break;
    case 0x89c4: store_a_abs_x16(c, 0x0b07); break;
    case 0x89c7: case 0x89c8: case 0x89d3: case 0x89db: case 0x89dc: case 0x89e4:
    case 0x89ec: case 0x89ed: case 0x89f7: case 0x89f8:
      implied(c); c->y++; zn(c, c->y, false); break;
    case 0x89c9: case 0x89dd: case 0x89f9: set_m(c, true); break;
    case 0x89cb: case 0x89df: case 0x89fb: lda_indirect_long_y8(c, 0x00); break;
    case 0x89cd: adc_abs_x8(c, 0x0b09); break;
    case 0x89d0: store_a_abs_x8(c, 0x0b09); break;
    case 0x89d8: store_a_abs_x16(c, 0x0b0f); break;
    case 0x89e1: store_a_abs_x8(c, 0x0b11); break;
    case 0x89e9: store_a_abs_x16(c, 0x0b0a); break;
    case 0x89f1: adc_abs_x16(c, 0x0b0c); break;
    case 0x89f4: store_a_abs_x16(c, 0x0b0c); break;
    case 0x89fd: store_a_abs_x8(c, 0x0b0e); break;
    case 0x8a00: phx16(c); break;
    case 0x8a01: jsl_to(c, 0x8a69); break;
    case 0x8a05: plx16(c); break;
    case 0x8a06: return_long(c); break;
  }
  return true;
}

/* ---- $06EF11 queue fill + $06F089 tile-word fetch (F082 early-exit) ---- */
static void lda_indirect_long8(Cpu *c, uint8_t offset) {
  uint16_t dp = dp_address(c, offset);
  uint32_t pointer = c->read(c->mem, dp);
  pointer |= (uint32_t)c->read(c->mem, (uint16_t)(dp + 1)) << 8;
  pointer |= (uint32_t)c->read(c->mem, (uint16_t)(dp + 2)) << 16;
  sample_interrupt(c);
  set_a8(c, c->read(c->mem, pointer));
}
static void cmp_abs8(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c); sample_interrupt(c);
  uint8_t value = c->read(c->mem, ((uint32_t)c->db << 16) | address);
  uint8_t a = (uint8_t)c->a;
  c->c = a >= value; zn(c, (uint8_t)(a - value), true);
}
static void and_constant16(Cpu *c, uint16_t value) {
  (void)fetch(c); sample_interrupt(c); (void)fetch(c);
  c->a &= value; zn(c, c->a, false);
}
static void cmp_constant16(Cpu *c, uint16_t value) {
  uint16_t low = c->pc++, high = c->pc++;
  (void)c->read(c->mem, ((uint32_t)c->k << 16) | low); sample_interrupt(c);
  (void)c->read(c->mem, ((uint32_t)c->k << 16) | high);
  c->c = c->a >= value; zn(c, (uint16_t)(c->a - value), false);
}
static void asl_a16(Cpu *c) {
  implied(c); c->c = (c->a & 0x8000u) != 0;
  c->a = (uint16_t)(c->a << 1); zn(c, c->a, false);
}
static void tay16(Cpu *c) {
  implied(c); c->y = c->a; zn(c, c->y, false);
}

/* $06EF11: find/mark VRAM queue slot, build 8-byte descriptor from DP+$00/$01
 * (offset by ±8 when $0700 bit4 set), then loop X=0..$1F calling $06F089 and
 * storing the resulting word into $0880,Y. */
static bool queue_fill_ef11_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x06 || c->e || c->xf || c->d) return false;
  if((pc == 0xef76 || pc == 0xef78 || pc == 0xef7b || pc == 0xef7c || pc == 0xef7d ||
      pc == 0xef7e || pc == 0xef80 || pc == 0xef83) && c->mf)
    return false;
  if((pc == 0xef11 || pc == 0xef15 || pc == 0xef19 || pc == 0xef1c || pc == 0xef1e ||
      pc == 0xef20 || pc == 0xef22 || pc == 0xef23 || pc == 0xef25 || pc == 0xef27 ||
      pc == 0xef29 || pc == 0xef2b || pc == 0xef2d || pc == 0xef2f || pc == 0xef31 ||
      pc == 0xef32 || pc == 0xef34 || pc == 0xef36 || pc == 0xef38 || pc == 0xef3a ||
      pc == 0xef3c || pc == 0xef3e || pc == 0xef41 || pc == 0xef43 || pc == 0xef44 ||
      pc == 0xef45 || pc == 0xef46 || pc == 0xef49 || pc == 0xef4b || pc == 0xef4d ||
      pc == 0xef4e || pc == 0xef4f || pc == 0xef51 || pc == 0xef54 || pc == 0xef56 ||
      pc == 0xef59 || pc == 0xef5b || pc == 0xef5e || pc == 0xef60 || pc == 0xef63 ||
      pc == 0xef65 || pc == 0xef68 || pc == 0xef6a || pc == 0xef6d || pc == 0xef70 ||
      pc == 0xef74 || pc == 0xef85 || pc == 0xef86 || pc == 0xef89 || pc == 0xef8b ||
      pc == 0xef8d || pc == 0xef8e || pc == 0xef90 || pc == 0xef92 || pc == 0xef94 ||
      pc == 0xef96 || pc == 0xef98 || pc == 0xef9a) && !c->mf)
    return false;
  switch(pc) {
    case 0xef11: case 0xef15: case 0xef19: case 0xef1c: case 0xef1e: case 0xef20:
    case 0xef22: case 0xef23: case 0xef25: case 0xef27: case 0xef29: case 0xef2b:
    case 0xef2d: case 0xef2f: case 0xef31: case 0xef32: case 0xef34: case 0xef36:
    case 0xef38: case 0xef3a: case 0xef3c: case 0xef3e: case 0xef41: case 0xef43:
    case 0xef44: case 0xef45: case 0xef46: case 0xef49: case 0xef4b: case 0xef4d:
    case 0xef4e: case 0xef4f: case 0xef51: case 0xef54: case 0xef56: case 0xef59:
    case 0xef5b: case 0xef5e: case 0xef60: case 0xef63: case 0xef65: case 0xef68:
    case 0xef6a: case 0xef6d: case 0xef70: case 0xef74: case 0xef76: case 0xef78:
    case 0xef7b: case 0xef7c: case 0xef7d: case 0xef7e: case 0xef80: case 0xef83:
    case 0xef85: case 0xef86: case 0xef89: case 0xef8b: case 0xef8d: case 0xef8e:
    case 0xef90: case 0xef92: case 0xef94: case 0xef96: case 0xef98: case 0xef9a:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xef11: jsl_to(c, 0x8711); break;
    case 0xef15: jsl_to(c, 0x8724); break;
    case 0xef19: video_load(c, 0x0700, false); break;
    case 0xef1c: and_constant8(c, 0x10); break;
    case 0xef1e: branch_to(c, !c->z, 0xef2f); break;
    case 0xef20: case 0xef2f: load_dp8(c, 0x00); break;
    case 0xef22: implied(c); c->c = false; break;
    case 0xef23: adc8_constant(c, 0x08); break;
    case 0xef25: case 0xef34: store_dp(c, 0x0a, true); break;
    case 0xef27: case 0xef36: load_dp8(c, 0x01); break;
    case 0xef29: adc8_constant(c, 0x00); break;
    case 0xef2b: case 0xef3a: store_dp(c, 0x0b, true); break;
    case 0xef2d: branch_to(c, true, 0xef3c); break;
    case 0xef31: implied(c); c->c = true; break;
    case 0xef32: sbc8_constant(c, 0x08); break;
    case 0xef38: sbc8_constant(c, 0x00); break;
    case 0xef3c: byte_constant(c, 0x81); break;
    case 0xef3e: store_a_abs_x8(c, 0x0800); break;
    case 0xef41: load_dp8(c, 0x00); break;
    case 0xef43: case 0xef44: case 0xef45: lsr_a8(c); break;
    case 0xef46: store_a_abs_x8(c, 0x0801); break;
    case 0xef49: load_dp8(c, 0x01); break;
    case 0xef4b: and_constant8(c, 0x01); break;
    case 0xef4d: case 0xef4e: asl_a8(c); break;
    case 0xef4f: ora_constant8(c, 0x60); break;
    case 0xef51: store_a_abs_x8(c, 0x0802); break;
    case 0xef54: byte_constant(c, 0x80); break;
    case 0xef56: store_a_abs_x8(c, 0x0803); break;
    case 0xef59: byte_constant(c, 0x08); break;
    case 0xef5b: store_a_abs_x8(c, 0x0804); break;
    case 0xef5e: case 0xef68: byte_constant(c, 0x00); break;
    case 0xef60: store_a_abs_x8(c, 0x0805); break;
    case 0xef63: byte_constant(c, 0x40); break;
    case 0xef65: store_a_abs_x8(c, 0x0806); break;
    case 0xef6a: store_a_abs_x8(c, 0x0807); break;
    case 0xef6d: index_constant(c, false, 0x0000); break;
    case 0xef70: jsl_to(c, 0xf089); break;
    case 0xef74: set_m(c, false); break;
    case 0xef76: load_dp16_to_a(c, 0x02); break;
    case 0xef78: and_constant16(c, 0x00f8); break;
    case 0xef7b: case 0xef7c: lsr_a16(c); break;
    case 0xef7d: tay16(c); break;
    case 0xef7e: load_dp16_to_a(c, 0x04); break;
    case 0xef80: store_a_abs_y16(c, 0x0880); break;
    case 0xef83: set_m(c, true); break;
    case 0xef85: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xef86: cpx16_constant(c, 0x0020); break;
    case 0xef89: branch_to(c, c->c, 0xef9a); break;
    case 0xef8b: load_dp8(c, 0x02); break;
    case 0xef8d: implied(c); c->c = false; break;
    case 0xef8e: adc8_constant(c, 0x08); break;
    case 0xef90: store_dp(c, 0x02, true); break;
    case 0xef92: load_dp8(c, 0x03); break;
    case 0xef94: adc8_constant(c, 0x00); break;
    case 0xef96: store_dp(c, 0x03, true); break;
    case 0xef98: branch_to(c, true, 0xef70); break;
    case 0xef9a: return_long(c); break;
  }
  return true;
}

/* $06F089: bounds-check DP+$01/$03 against $0703/$0702 (early-exit at $F082
 * clears DP+$04/$05); hardware-multiply row, resolve map pointer via $06F1EE
 * and return a little-endian word in DP+$04/$05. */
static bool tile_fetch_f089_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x06 || c->e || c->xf || c->d) return false;
  if((pc == 0xf0c5 || pc == 0xf0c7 || pc == 0xf0c8 || pc == 0xf0cb || pc == 0xf0cd ||
      pc == 0xf0e2 || pc == 0xf0e6 || pc == 0xf0e8 || pc == 0xf0fb || pc == 0xf0fe ||
      pc == 0xf0ff || pc == 0xf100 || pc == 0xf102 || pc == 0xf104 || pc == 0xf111 ||
      pc == 0xf113 || pc == 0xf116 || pc == 0xf119 || pc == 0xf11a || pc == 0xf11b ||
      pc == 0xf11e || pc == 0xf120 || pc == 0xf13c || pc == 0xf13f || pc == 0xf140) &&
     c->mf)
    return false;
  if((pc == 0xf082 || pc == 0xf084 || pc == 0xf086 || pc == 0xf088 || pc == 0xf089 ||
      pc == 0xf08b || pc == 0xf08d || pc == 0xf090 || pc == 0xf092 || pc == 0xf094 ||
      pc == 0xf096 || pc == 0xf099 || pc == 0xf09b || pc == 0xf09c || pc == 0xf09f ||
      pc == 0xf0a2 || pc == 0xf0a5 || pc == 0xf0a6 || pc == 0xf0a7 || pc == 0xf0a8 ||
      pc == 0xf0ab || pc == 0xf0ad || pc == 0xf0b0 || pc == 0xf0b2 || pc == 0xf0b4 ||
      pc == 0xf0b5 || pc == 0xf0b7 || pc == 0xf0b9 || pc == 0xf0bb || pc == 0xf0bd ||
      pc == 0xf0bf || pc == 0xf0c1 || pc == 0xf0c3 || pc == 0xf0cf || pc == 0xf0d2 ||
      pc == 0xf0d4 || pc == 0xf0d6 || pc == 0xf0d8 || pc == 0xf0d9 || pc == 0xf0db ||
      pc == 0xf0dd || pc == 0xf0de || pc == 0xf0df || pc == 0xf0e0 || pc == 0xf0ea ||
      pc == 0xf0eb || pc == 0xf0ed || pc == 0xf0ee || pc == 0xf0ef || pc == 0xf0f0 ||
      pc == 0xf0f1 || pc == 0xf0f3 || pc == 0xf0f5 || pc == 0xf0f7 || pc == 0xf0f9 ||
      pc == 0xf106 || pc == 0xf108 || pc == 0xf10a || pc == 0xf10c || pc == 0xf10f ||
      pc == 0xf122 || pc == 0xf125 || pc == 0xf127 || pc == 0xf129 || pc == 0xf12b ||
      pc == 0xf12d || pc == 0xf12e || pc == 0xf12f || pc == 0xf131 || pc == 0xf133 ||
      pc == 0xf135 || pc == 0xf136 || pc == 0xf137 || pc == 0xf138 || pc == 0xf13a ||
      pc == 0xf142 || pc == 0xf144 || pc == 0xf146 || pc == 0xf147 || pc == 0xf149 ||
      pc == 0xf14b || pc == 0xf14c) && !c->mf)
    return false;
  switch(pc) {
    case 0xf082: case 0xf084: case 0xf086: case 0xf088: case 0xf089: case 0xf08b:
    case 0xf08d: case 0xf090: case 0xf092: case 0xf094: case 0xf096: case 0xf099:
    case 0xf09b: case 0xf09c: case 0xf09f: case 0xf0a2: case 0xf0a5: case 0xf0a6:
    case 0xf0a7: case 0xf0a8: case 0xf0ab: case 0xf0ad: case 0xf0b0: case 0xf0b2:
    case 0xf0b4: case 0xf0b5: case 0xf0b7: case 0xf0b9: case 0xf0bb: case 0xf0bd:
    case 0xf0bf: case 0xf0c1: case 0xf0c3: case 0xf0c5: case 0xf0c7: case 0xf0c8:
    case 0xf0cb: case 0xf0cd: case 0xf0cf: case 0xf0d2: case 0xf0d4: case 0xf0d6:
    case 0xf0d8: case 0xf0d9: case 0xf0db: case 0xf0dd: case 0xf0de: case 0xf0df:
    case 0xf0e0: case 0xf0e2: case 0xf0e6: case 0xf0e8: case 0xf0ea: case 0xf0eb:
    case 0xf0ed: case 0xf0ee: case 0xf0ef: case 0xf0f0: case 0xf0f1: case 0xf0f3:
    case 0xf0f5: case 0xf0f7: case 0xf0f9: case 0xf0fb: case 0xf0fe: case 0xf0ff:
    case 0xf100: case 0xf102: case 0xf104: case 0xf106: case 0xf108: case 0xf10a:
    case 0xf10c: case 0xf10f: case 0xf111: case 0xf113: case 0xf116: case 0xf119:
    case 0xf11a: case 0xf11b: case 0xf11e: case 0xf120: case 0xf122: case 0xf125:
    case 0xf127: case 0xf129: case 0xf12b: case 0xf12d: case 0xf12e: case 0xf12f:
    case 0xf131: case 0xf133: case 0xf135: case 0xf136: case 0xf137: case 0xf138:
    case 0xf13a: case 0xf13c: case 0xf13f: case 0xf140: case 0xf142: case 0xf144:
    case 0xf146: case 0xf147: case 0xf149: case 0xf14b: case 0xf14c:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf082: byte_constant(c, 0x00); break;
    case 0xf084: store_dp(c, 0x04, true); break;
    case 0xf086: store_dp(c, 0x05, true); break;
    case 0xf088: case 0xf14c: return_long(c); break;
    case 0xf089: load_dp8(c, 0x01); break;
    case 0xf08b: branch_to(c, c->n, 0xf082); break;
    case 0xf08d: cmp_abs8(c, 0x0703); break;
    case 0xf090: branch_to(c, c->z, 0xf082); break;
    case 0xf092: load_dp8(c, 0x03); break;
    case 0xf094: branch_to(c, c->n, 0xf082); break;
    case 0xf096: cmp_abs8(c, 0x0702); break;
    case 0xf099: branch_to(c, c->z, 0xf082); break;
    case 0xf09b: phy16(c); break;
    case 0xf09c: store_register8(c, 0x4202); break;
    case 0xf09f: video_load(c, 0x0703, false); break;
    case 0xf0a2: store_register8(c, 0x4203); break;
    case 0xf0a5: case 0xf0a6: case 0xf0d8: xba_native(c); break;
    case 0xf0a7: nop_native(c); break;
    case 0xf0a8: video_load(c, 0x4216, false); break;
    case 0xf0ab: store_dp(c, 0x04, true); break;
    case 0xf0ad: video_load(c, 0x4217, false); break;
    case 0xf0b0: store_dp(c, 0x05, true); break;
    case 0xf0b2: load_dp8(c, 0x04); break;
    case 0xf0b4: implied(c); c->c = false; break;
    case 0xf0b5: adc8_dp(c, 0x01); break;
    case 0xf0b7: store_dp(c, 0x04, true); break;
    case 0xf0b9: store_dp(c, 0x07, true); break;
    case 0xf0bb: load_dp8(c, 0x05); break;
    case 0xf0bd: adc8_constant(c, 0x00); break;
    case 0xf0bf: store_dp(c, 0x05, true); break;
    case 0xf0c1: store_dp(c, 0x08, true); break;
    case 0xf0c3: set_m(c, false); break;
    case 0xf0c5: load_dp16_to_a(c, 0x04); break;
    case 0xf0c7: implied(c); c->c = false; break;
    case 0xf0c8: adc_abs16(c, 0x0704); break;
    case 0xf0cb: store_dp(c, 0x04, false); break;
    case 0xf0cd: case 0xf0e8: case 0xf104: case 0xf120: case 0xf140: set_m(c, true); break;
    case 0xf0cf: video_load(c, 0x0706, false); break;
    case 0xf0d2: store_dp(c, 0x06, true); break;
    case 0xf0d4: byte_constant(c, 0x00); break;
    case 0xf0d6: store_dp(c, 0x07, true); break;
    case 0xf0d9: lda_indirect_long8(c, 0x04); break;
    case 0xf0db: store_dp(c, 0x71, true); break;
    case 0xf0dd: asl_a8(c); break;
    case 0xf0de: phx16(c); break;
    case 0xf0df: tax16_from_a8(c); break;
    case 0xf0e0: set_m(c, false); break;
    case 0xf0e2: lda_long_x16(c, 0x06f1ee); break;
    case 0xf0e6: store_dp(c, 0x08, false); break;
    case 0xf0ea: plx16(c); break;
    case 0xf0eb: load_dp8(c, 0x00); break;
    case 0xf0ed: case 0xf0ee: case 0xf0ef: case 0xf0f0: lsr_a8(c); break;
    case 0xf0f1: store_dp(c, 0x04, true); break;
    case 0xf0f3: load_dp8(c, 0x02); break;
    case 0xf0f5: and_constant8(c, 0xf0); break;
    case 0xf0f7: ora_dp8(c, 0x04); break;
    case 0xf0f9: set_m(c, false); break;
    case 0xf0fb: and_constant16(c, 0x00ff); break;
    case 0xf0fe: asl_a16(c); break;
    case 0xf0ff: implied(c); c->c = false; break;
    case 0xf100: adc_dp16(c, 0x07); break;
    case 0xf102: store_dp(c, 0x07, false); break;
    case 0xf106: load_dp8(c, 0x09); break;
    case 0xf108: adc8_constant(c, 0x00); break;
    case 0xf10a: store_dp(c, 0x09, true); break;
    case 0xf10c: index_constant(c, true, 0x0000); break;
    case 0xf10f: set_m(c, false); break;
    case 0xf111: lda_indirect_long_y16(c, 0x07); break;
    case 0xf113: and_constant16(c, 0x3fde); break;
    case 0xf116: store_a_abs16(c, 0x0736); break;
    case 0xf119: asl_a16(c); break;
    case 0xf11a: implied(c); c->c = false; break;
    case 0xf11b: adc_abs16(c, 0x0707); break;
    case 0xf11e: store_dp(c, 0x07, false); break;
    case 0xf122: video_load(c, 0x0709, false); break;
    case 0xf125: adc8_constant(c, 0x00); break;
    case 0xf127: store_dp(c, 0x09, true); break;
    case 0xf129: load_dp8(c, 0x00); break;
    case 0xf12b: and_constant8(c, 0x08); break;
    case 0xf12d: case 0xf12e: lsr_a8(c); break;
    case 0xf12f: store_dp(c, 0x04, true); break;
    case 0xf131: load_dp8(c, 0x02); break;
    case 0xf133: and_constant8(c, 0x08); break;
    case 0xf135: case 0xf136: case 0xf137: asl_a8(c); break;
    case 0xf138: ora_dp8(c, 0x04); break;
    case 0xf13a: set_m(c, false); break;
    case 0xf13c: and_constant16(c, 0x0042); break;
    case 0xf13f: tay16(c); break;
    case 0xf142: lda_indirect_long_y8(c, 0x07); break;
    case 0xf144: store_dp(c, 0x05, true); break;
    case 0xf146: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xf147: lda_indirect_long_y8(c, 0x07); break;
    case 0xf149: store_dp(c, 0x04, true); break;
    case 0xf14b: ply16(c); break;
  }
  return true;
}


/* $03FB8D: scene-entry early-exit / dispatch (prefix through both early RTLs).
 * Reads $1648 / $01D8 / $01D6 / $01A0 / $0024; either RTL early or JMP/branch
 * into deeper bank-3 handlers outside this native range (interpreter continues).
 * Unblocks native ownership of the $008B42 JSL from HDMA/queue wipe. */
static bool early_exit_fb8d_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x03 || c->e || c->d || !c->mf) return false;
  switch(pc) {
    case 0xfb8d: case 0xfb90: case 0xfb92: case 0xfb95: case 0xfb97: case 0xfb99:
    case 0xfb9b: case 0xfb9d: case 0xfba0: case 0xfba2: case 0xfba4: case 0xfba7:
    case 0xfba9: case 0xfbab: case 0xfbae: case 0xfbb1: case 0xfbb3: case 0xfbb5:
    case 0xfbb8: case 0xfbbb: case 0xfbbd: case 0xfbbf: case 0xfbc1: case 0xfbc3:
    case 0xfbc6: case 0xfbc7: case 0xfbca: case 0xfbcc: case 0xfbce: case 0xfbd0:
    case 0xfbd2: case 0xfbd4: case 0xfbd6: case 0xfbd9: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xfb8d: video_load(c, 0x1648, false); break;
    case 0xfb90: branch_to(c, !c->n, 0xfbae); break;
    case 0xfb92: video_load(c, 0x01d8, false); break;
    case 0xfb95: cmp_constant8(c, 0x48); break;
    case 0xfb97: branch_to(c, c->z, 0xfbc6); break;
    case 0xfb99: cmp_constant8(c, 0x32); break;
    case 0xfb9b: branch_to(c, !c->z, 0xfba0); break;
    case 0xfb9d: jmp_abs_native(c, 0xfcfd); break;
    case 0xfba0: cmp_constant8(c, 0x36); break;
    case 0xfba2: branch_to(c, !c->z, 0xfba7); break;
    case 0xfba4: jmp_abs_native(c, 0xfd25); break;
    case 0xfba7: cmp_constant8(c, 0x42); break;
    case 0xfba9: branch_to(c, !c->z, 0xfbae); break;
    case 0xfbab: jmp_abs_native(c, 0xfd25); break;
    case 0xfbae: video_load(c, 0x01d6, false); break;
    case 0xfbb1: and_constant8(c, 0x40); break;
    case 0xfbb3: branch_to(c, c->z, 0xfbb8); break;
    case 0xfbb5: jmp_abs_native(c, 0xfd3d); break;
    case 0xfbb8: video_load(c, 0x01a0, false); break;
    case 0xfbbb: cmp_constant8(c, 0x05); break;
    case 0xfbbd: branch_to(c, c->z, 0xfbc7); break;
    case 0xfbbf: cmp_constant8(c, 0x18); break;
    case 0xfbc1: branch_to(c, !c->z, 0xfbc6); break;
    case 0xfbc3: jmp_abs_native(c, 0xfc93); break;
    case 0xfbc6: case 0xfbd9: return_long(c); break;
    case 0xfbc7: video_load(c, 0x0024, false); break;
    case 0xfbca: and_constant8(c, 0x7f); break;
    case 0xfbcc: branch_to(c, c->z, 0xfbda); break;
    case 0xfbce: cmp_constant8(c, 0x02); break;
    case 0xfbd0: branch_to(c, c->z, 0xfc1e); break;
    case 0xfbd2: cmp_constant8(c, 0x07); break;
    case 0xfbd4: branch_to(c, !c->z, 0xfbd9); break;
    case 0xfbd6: jmp_abs_native(c, 0xfced); break;
  }
  return true;
}


/* $008B7A: scene-mode[0] — JSLs, VRAM wipe, scroll snap, palette clear,
 * JSR $94E7/$9485, bank-1 JSLs, $01A0 specialty, then BMI paths through
 * pointer setup / JMP $8C6D into native $C559/$C68C/$90E6. */
static bool scene_mode0_8b7a_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  if((pc == 0x8ba5 || pc == 0x8ba8 || pc == 0x8bab ||
      pc == 0x8bad || pc == 0x8bb0 || pc == 0x8bb3 || pc == 0x8bb5 ||
      pc == 0x8bb8 || pc == 0x8bbb || pc == 0x8bbe || pc == 0x8bc1 ||
      pc == 0x8bc4) && c->mf) return false;
  if((pc == 0x8b7a || pc == 0x8b7e || pc == 0x8b80 || pc == 0x8b82 ||
      pc == 0x8b85 || pc == 0x8b87 || pc == 0x8b8a || pc == 0x8b8d ||
      pc == 0x8b91 || pc == 0x8b94 || pc == 0x8b97 || pc == 0x8b9b ||
      pc == 0x8b9f || pc == 0x8bc9 || pc == 0x8bcd || pc == 0x8bd0 ||
      pc == 0x8bd2 || pc == 0x8bd5 || pc == 0x8bd8 || pc == 0x8bdc ||
      pc == 0x8be0 || pc == 0x8be4 || pc == 0x8be8 || pc == 0x8bec ||
      pc == 0x8bef || pc == 0x8bf1 || pc == 0x8bf3 || pc == 0x8bf5 ||
      pc == 0x8bf7 || pc == 0x8bf9 || pc == 0x8bfb || pc == 0x8bfd ||
      pc == 0x8c00 || pc == 0x8c03 || pc == 0x8c05 || pc == 0x8c08 ||
      pc == 0x8c0a || pc == 0x8c0d || pc == 0x8c10 ||
      pc == 0x8c12 || pc == 0x8c16 || pc == 0x8c1a || pc == 0x8c1e ||
      pc == 0x8c22 || pc == 0x8c25 || pc == 0x8c28 || pc == 0x8c2b ||
      pc == 0x8c2d || pc == 0x8c30 || pc == 0x8c33 || pc == 0x8c35 ||
      pc == 0x8c37 || pc == 0x8c39 || pc == 0x8c3c || pc == 0x8c3f ||
      pc == 0x8c42 || pc == 0x8c46 || pc == 0x8c49 || pc == 0x8c4b ||
      pc == 0x8c4e || pc == 0x8c52 || pc == 0x8c56 || pc == 0x8c59 ||
      pc == 0x8c5c || pc == 0x8c5f || pc == 0x8c61 || pc == 0x8c64 ||
      pc == 0x8c67 || pc == 0x8c69 || pc == 0x8c6b || pc == 0x8c6d ||
      pc == 0x8c6f || pc == 0x8c72 || pc == 0x8c76 || pc == 0x8c7a ||
      pc == 0x8c7d || pc == 0x8c80) && !c->mf) return false;
  switch(pc) {
    case 0x8b7a: case 0x8b7e: case 0x8b80: case 0x8b82: case 0x8b85: case 0x8b87:
    case 0x8b8a: case 0x8b8d: case 0x8b91: case 0x8b94: case 0x8b97: case 0x8b9b:
    case 0x8b9f: case 0x8ba3: case 0x8ba5: case 0x8ba8: case 0x8bab: case 0x8bad:
    case 0x8bb0: case 0x8bb3: case 0x8bb5: case 0x8bb8: case 0x8bbb: case 0x8bbe:
    case 0x8bc1: case 0x8bc4: case 0x8bc7: case 0x8bc9: case 0x8bcd: case 0x8bd0:
    case 0x8bd2: case 0x8bd5: case 0x8bd8: case 0x8bdc: case 0x8be0: case 0x8be4:
    case 0x8be8: case 0x8bec: case 0x8bef: case 0x8bf1: case 0x8bf3: case 0x8bf5:
    case 0x8bf7: case 0x8bf9: case 0x8bfb: case 0x8bfd: case 0x8c00: case 0x8c03:
    case 0x8c05: case 0x8c08: case 0x8c0a: case 0x8c0d: case 0x8c10:
    case 0x8c12: case 0x8c16: case 0x8c1a: case 0x8c1e: case 0x8c22: case 0x8c25:
    case 0x8c28: case 0x8c2b: case 0x8c2d: case 0x8c30: case 0x8c33: case 0x8c35:
    case 0x8c37: case 0x8c39: case 0x8c3c: case 0x8c3f: case 0x8c42: case 0x8c46:
    case 0x8c49: case 0x8c4b: case 0x8c4e: case 0x8c52: case 0x8c56: case 0x8c59:
    case 0x8c5c: case 0x8c5f: case 0x8c61: case 0x8c64: case 0x8c67: case 0x8c69:
    case 0x8c6b: case 0x8c6d: case 0x8c6f: case 0x8c72: case 0x8c76: case 0x8c7a:
    case 0x8c7d: case 0x8c80:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8b7a: jsl_to(c, 0xa6cb); break;
    case 0x8b7e: stz_dp8(c, 0x00); break;
    case 0x8b80: stz_dp8(c, 0x01); break;
    case 0x8b82: index_constant(c, true, 0x2000); break;
    case 0x8b85: store_dp_y16(c, 0x02); break;
    case 0x8b87: index_constant(c, true, 0x5000); break;
    case 0x8b8a: store_y_abs16(c, 0x2116); break;
    case 0x8b8d: jsl_to(c, 0x8887); break;
    case 0x8b91: stz_abs8(c, 0x0d66); break;
    case 0x8b94: stz_abs8(c, 0x0725); break;
    case 0x8b97: jsl_to(c, 0xe837); break;
    case 0x8b9b: jsl_to(c, 0x8b15); break;
    case 0x8b9f: jsl_to(c, 0x9255); break;
    case 0x8ba3: set_m(c, false); break;
    case 0x8ba5: video_load(c, 0x01a6, false); break;
    case 0x8ba8: and_constant16(c, 0xfff8); break;
    case 0x8bab: store_dp(c, 0x33, false); break;
    case 0x8bad: video_load(c, 0x01a8, false); break;
    case 0x8bb0: and_constant16(c, 0xfff8); break;
    case 0x8bb3: store_dp(c, 0x35, false); break;
    case 0x8bb5: video_load(c, 0x01aa, false); break;
    case 0x8bb8: and_constant16(c, 0xfff8); break;
    case 0x8bbb: store_a_abs16(c, 0x01a2); break;
    case 0x8bbe: video_load(c, 0x01ac, false); break;
    case 0x8bc1: and_constant16(c, 0xfff8); break;
    case 0x8bc4: store_a_abs16(c, 0x01a4); break;
    case 0x8bc7: set_m(c, true); break;
    case 0x8bc9: jsl_to(c, 0x86b6); break;
    case 0x8bcd: stz_abs8(c, 0x0d00); break;
    case 0x8bd0: stz_dp8(c, 0x61); break;
    case 0x8bd2: jsr_abs_native(c, 0x94e7); break;
    case 0x8bd5: jsr_abs_native(c, 0x9485); break;
    case 0x8bd8: jsl_to(c, 0xb61e); break;
    case 0x8bdc: jsl_to(c, 0xb67a); break;
    case 0x8be0: jsl_to(c, 0xb6ac); break;
    case 0x8be4: jsl_to(c, 0xb6bf); break;
    case 0x8be8: jsl_to(c, 0x8324); break;
    case 0x8bec: video_load(c, 0x01a0, false); break;
    case 0x8bef: cmp_constant8(c, 0x15); break;
    case 0x8bf1: branch_to(c, !c->z, 0x8bf7); break;
    case 0x8bf3: byte_constant(c, 0x9e); break;
    case 0x8bf5: branch_to(c, true, 0x8bfd); break;
    case 0x8bf7: cmp_constant8(c, 0x14); break;
    case 0x8bf9: branch_to(c, !c->z, 0x8c0d); break;
    case 0x8bfb: byte_constant(c, 0x9f); break;
    case 0x8bfd: store_register8(c, 0x01bd); break;
    case 0x8c00: stz_abs8(c, 0x0d87); break;
    case 0x8c03: byte_constant(c, 0x10); break;
    case 0x8c05: store_register8(c, 0x01e7); break;
    case 0x8c08: byte_constant(c, 0x80); break;
    case 0x8c0a: store_register8(c, 0x01a1); break;
    case 0x8c0d: video_load(c, 0x01a1, false); break;
    case 0x8c10: branch_to(c, c->n, 0x8c3c); break;
    case 0x8c12: jsl_to(c, 0xd37d); break;
    case 0x8c16: jsl_to(c, 0x99b3); break;
    case 0x8c1a: jsl_to(c, 0x93fa); break;
    case 0x8c1e: jsl_to(c, 0xec12); break;
    case 0x8c22: stz_abs8(c, 0x01a1); break;
    case 0x8c25: index_constant(c, true, 0xefb0); break;
    case 0x8c28: store_y_abs16(c, 0x0707); break;
    case 0x8c2b: byte_constant(c, 0x09); break;
    case 0x8c2d: store_register8(c, 0x0709); break;
    case 0x8c30: index_constant(c, true, 0x8d4c); break;
    case 0x8c33: store_dp_y16(c, 0x83); break;
    case 0x8c35: byte_constant(c, 0x08); break;
    case 0x8c37: store_dp(c, 0x85, true); break;
    case 0x8c39: jmp_abs_native(c, 0x8c6d); break;
    case 0x8c3c: index_constant(c, true, 0xffff); break;
    case 0x8c3f: store_y_abs16(c, 0x074c); break;
    case 0x8c42: jsl_to(c, 0xd97d); break;
    case 0x8c46: video_load(c, 0x01bc, false); break;
    case 0x8c49: and_constant8(c, 0xf0); break;
    case 0x8c4b: store_register8(c, 0x01bc); break;
    case 0x8c4e: jsl_to(c, 0x9336); break;
    case 0x8c52: jsl_to(c, 0x8d2b); break;
    case 0x8c56: store_register8(c, 0x01a0); break;
    case 0x8c59: index_constant(c, true, 0xfa00); break;
    case 0x8c5c: store_y_abs16(c, 0x0707); break;
    case 0x8c5f: byte_constant(c, 0x08); break;
    case 0x8c61: store_register8(c, 0x0709); break;
    case 0x8c64: index_constant(c, true, 0xbf20); break;
    case 0x8c67: store_dp_y16(c, 0x83); break;
    case 0x8c69: byte_constant(c, 0x08); break;
    case 0x8c6b: store_dp(c, 0x85, true); break;
    case 0x8c6d: byte_constant(c, 0x80); break;
    case 0x8c6f: store_register8(c, 0x2115); break;
    case 0x8c72: jsl_to(c, 0xc559); break;
    case 0x8c76: jsl_to(c, 0xc68c); break;
    case 0x8c7a: index_constant(c, true, 0x3000); break;
    case 0x8c7d: store_y_abs16(c, 0x2116); break;
    case 0x8c80: jsl_to(c, 0x90e6); break;
  }
  return true;
}

/* $008E96: scene-mode[2] — HDMA mask, $94B2, scroll snap when $0C40==0,
 * PPU preset, JSL $8887, then $01E7 pointer select into native $C559/$C68C/$90E6. */
static bool scene_mode2_8e96_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  if((pc == 0x8ebc || pc == 0x8ebe || pc == 0x8ec1 ||
      pc == 0x8ec3 || pc == 0x8ec6 || pc == 0x8ec9 || pc == 0x8ecc ||
      pc == 0x8ecf) && c->mf) return false;
  if((pc == 0x8e96 || pc == 0x8e9a || pc == 0x8e9c || pc == 0x8e9e ||
      pc == 0x8ea0 || pc == 0x8ea3 || pc == 0x8ea6 || pc == 0x8ea8 ||
      pc == 0x8eaa || pc == 0x8eac || pc == 0x8eaf || pc == 0x8eb2 ||
      pc == 0x8eb5 || pc == 0x8eb8 || pc == 0x8ed4 || pc == 0x8ed7 ||
      pc == 0x8ed9 || pc == 0x8edb || pc == 0x8edd || pc == 0x8ee0 ||
      pc == 0x8ee2 || pc == 0x8ee5 || pc == 0x8ee7 || pc == 0x8eea ||
      pc == 0x8eec || pc == 0x8eef || pc == 0x8ef1 || pc == 0x8ef4 ||
      pc == 0x8ef6 || pc == 0x8ef9 || pc == 0x8efb || pc == 0x8efe ||
      pc == 0x8f00 || pc == 0x8f03 || pc == 0x8f05 || pc == 0x8f07 ||
      pc == 0x8f0a || pc == 0x8f0c || pc == 0x8f0f || pc == 0x8f12 ||
      pc == 0x8f16 || pc == 0x8f19 || pc == 0x8f1b || pc == 0x8f1d ||
      pc == 0x8f20 || pc == 0x8f22 || pc == 0x8f24 || pc == 0x8f26 ||
      pc == 0x8f29 || pc == 0x8f2b || pc == 0x8f2d || pc == 0x8f2f ||
      pc == 0x8f31 || pc == 0x8f34 || pc == 0x8f38 || pc == 0x8f3c ||
      pc == 0x8f3f || pc == 0x8f42) && !c->mf)
    return false;
  switch(pc) {
    case 0x8e96: case 0x8e9a: case 0x8e9c: case 0x8e9e: case 0x8ea0: case 0x8ea3:
    case 0x8ea6: case 0x8ea8: case 0x8eaa: case 0x8eac: case 0x8eaf: case 0x8eb2:
    case 0x8eb5: case 0x8eb8: case 0x8eba: case 0x8ebc: case 0x8ebe: case 0x8ec1:
    case 0x8ec3: case 0x8ec6: case 0x8ec9: case 0x8ecc: case 0x8ecf: case 0x8ed2:
    case 0x8ed4: case 0x8ed7: case 0x8ed9: case 0x8edb: case 0x8edd: case 0x8ee0:
    case 0x8ee2: case 0x8ee5: case 0x8ee7: case 0x8eea: case 0x8eec: case 0x8eef:
    case 0x8ef1: case 0x8ef4: case 0x8ef6: case 0x8ef9: case 0x8efb: case 0x8efe:
    case 0x8f00: case 0x8f03: case 0x8f05: case 0x8f07: case 0x8f0a: case 0x8f0c:
    case 0x8f0f: case 0x8f12: case 0x8f16: case 0x8f19: case 0x8f1b: case 0x8f1d:
    case 0x8f20: case 0x8f22: case 0x8f24: case 0x8f26: case 0x8f29: case 0x8f2b:
    case 0x8f2d: case 0x8f2f: case 0x8f31: case 0x8f34: case 0x8f38: case 0x8f3c:
    case 0x8f3f: case 0x8f42:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8e96: jsl_to(c, 0xef29); break;
    case 0x8e9a: load_dp8(c, 0x62); break;
    case 0x8e9c: and_constant8(c, 0xe7); break;
    case 0x8e9e: store_dp(c, 0x62, true); break;
    case 0x8ea0: store_register8(c, 0x420c); break;
    case 0x8ea3: jsr_abs_native(c, 0x94b2); break;
    case 0x8ea6: load_dp8(c, 0x62); break;
    case 0x8ea8: and_constant8(c, 0x07); break;
    case 0x8eaa: store_dp(c, 0x62, true); break;
    case 0x8eac: stz_abs8(c, 0x0d00); break;
    case 0x8eaf: stz_abs8(c, 0x0d04); break;
    case 0x8eb2: stz_abs8(c, 0x0ea1); break;
    case 0x8eb5: video_load(c, 0x0c40, false); break;
    case 0x8eb8: branch_to(c, !c->z, 0x8ed4); break;
    case 0x8eba: set_m(c, false); break;
    case 0x8ebc: load_dp16_to_a(c, 0x33); break;
    case 0x8ebe: store_a_abs16(c, 0x01a6); break;
    case 0x8ec1: load_dp16_to_a(c, 0x35); break;
    case 0x8ec3: store_a_abs16(c, 0x01a8); break;
    case 0x8ec6: video_load(c, 0x01a2, false); break;
    case 0x8ec9: store_a_abs16(c, 0x01aa); break;
    case 0x8ecc: video_load(c, 0x01a4, false); break;
    case 0x8ecf: store_a_abs16(c, 0x01ac); break;
    case 0x8ed2: set_m(c, true); break;
    case 0x8ed4: index_constant(c, true, 0x0000); break;
    case 0x8ed7: store_dp_y16(c, 0x33); break;
    case 0x8ed9: store_dp_y16(c, 0x35); break;
    case 0x8edb: byte_constant(c, 0x68); break;
    case 0x8edd: store_register8(c, 0x2107); break;
    case 0x8ee0: byte_constant(c, 0x60); break;
    case 0x8ee2: store_register8(c, 0x2108); break;
    case 0x8ee5: byte_constant(c, 0x2c); break;
    case 0x8ee7: store_register8(c, 0x2109); break;
    case 0x8eea: byte_constant(c, 0x35); break;
    case 0x8eec: store_register8(c, 0x210b); break;
    case 0x8eef: byte_constant(c, 0x02); break;
    case 0x8ef1: store_register8(c, 0x210c); break;
    case 0x8ef4: byte_constant(c, 0x17); break;
    case 0x8ef6: store_register8(c, 0x212c); break;
    case 0x8ef9: byte_constant(c, 0x09); break;
    case 0x8efb: store_register8(c, 0x2105); break;
    case 0x8efe: byte_constant(c, 0x00); break;
    case 0x8f00: store_register8(c, 0x2101); break;
    case 0x8f03: store_dp(c, 0x00, true); break;
    case 0x8f05: store_dp(c, 0x01, true); break;
    case 0x8f07: index_constant(c, true, 0x2000); break;
    case 0x8f0a: store_dp_y16(c, 0x02); break;
    case 0x8f0c: index_constant(c, true, 0x5000); break;
    case 0x8f0f: store_y_abs16(c, 0x2116); break;
    case 0x8f12: jsl_to(c, 0x8887); break;
    case 0x8f16: video_load(c, 0x01e7, false); break;
    case 0x8f19: and_constant8(c, 0x10); break;
    case 0x8f1b: branch_to(c, c->z, 0x8f26); break;
    case 0x8f1d: index_constant(c, true, 0xf233); break;
    case 0x8f20: store_dp_y16(c, 0x83); break;
    case 0x8f22: byte_constant(c, 0x0c); break;
    case 0x8f24: branch_to(c, true, 0x8f2d); break;
    case 0x8f26: index_constant(c, true, 0x83fa); break;
    case 0x8f29: store_dp_y16(c, 0x83); break;
    case 0x8f2b: byte_constant(c, 0x09); break;
    case 0x8f2d: store_dp(c, 0x85, true); break;
    case 0x8f2f: byte_constant(c, 0x80); break;
    case 0x8f31: store_register8(c, 0x2115); break;
    case 0x8f34: jsl_to(c, 0xc559); break;
    case 0x8f38: jsl_to(c, 0xc68c); break;
    case 0x8f3c: index_constant(c, true, 0x3000); break;
    case 0x8f3f: store_y_abs16(c, 0x2116); break;
    case 0x8f42: jsl_to(c, 0x90e6); break;
  }
  return true;
}

/* $009485–$94E6: mode-0 PPU register preset; $94B2 entry clears scroll/window
 * regs and DP+$37..$3A then RTS. Shared leaf for JSR $9485 / JSR $94B2. */
static bool ppu_preset_9485_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x9485: case 0x9487: case 0x948a: case 0x948c: case 0x948f: case 0x9491:
    case 0x9494: case 0x9496: case 0x9499: case 0x949b: case 0x949e: case 0x94a0:
    case 0x94a3: case 0x94a5: case 0x94a8: case 0x94aa: case 0x94ad: case 0x94af:
    case 0x94b2: case 0x94b4: case 0x94b7: case 0x94ba: case 0x94bd: case 0x94c0:
    case 0x94c3: case 0x94c6: case 0x94c9: case 0x94cc: case 0x94cf: case 0x94d2:
    case 0x94d5: case 0x94d8: case 0x94db: case 0x94de: case 0x94e0: case 0x94e2:
    case 0x94e4: case 0x94e6:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x9485: byte_constant(c, 0x09); break;
    case 0x9487: store_register8(c, 0x2105); break;
    case 0x948a: byte_constant(c, 0x68); break;
    case 0x948c: store_register8(c, 0x2107); break;
    case 0x948f: byte_constant(c, 0x61); break;
    case 0x9491: store_register8(c, 0x2108); break;
    case 0x9494: byte_constant(c, 0x2c); break;
    case 0x9496: store_register8(c, 0x2109); break;
    case 0x9499: byte_constant(c, 0x35); break;
    case 0x949b: store_register8(c, 0x210b); break;
    case 0x949e: byte_constant(c, 0x02); break;
    case 0x94a0: store_register8(c, 0x210c); break;
    case 0x94a3: byte_constant(c, 0x00); break;
    case 0x94a5: store_register8(c, 0x2101); break;
    case 0x94a8: byte_constant(c, 0x04); break;
    case 0x94aa: store_register8(c, 0x2133); break;
    case 0x94ad: byte_constant(c, 0x17); break;
    case 0x94af: store_register8(c, 0x212c); break;
    case 0x94b2: byte_constant(c, 0x00); break;
    case 0x94b4: store_register8(c, 0x420c); break;
    case 0x94b7: store_register8(c, 0x212d); break;
    case 0x94ba: case 0x94bd: store_register8(c, 0x210d); break;
    case 0x94c0: case 0x94c3: store_register8(c, 0x210e); break;
    case 0x94c6: case 0x94c9: store_register8(c, 0x210f); break;
    case 0x94cc: case 0x94cf: store_register8(c, 0x2110); break;
    case 0x94d2: case 0x94d5: store_register8(c, 0x2111); break;
    case 0x94d8: case 0x94db: store_register8(c, 0x2112); break;
    case 0x94de: store_dp(c, 0x37, true); break;
    case 0x94e0: store_dp(c, 0x38, true); break;
    case 0x94e2: store_dp(c, 0x39, true); break;
    case 0x94e4: store_dp(c, 0x3a, true); break;
    case 0x94e6: return_short(c); break;
  }
  return true;
}

/* $0094E7: actor-slot clear loop — X = $01BB<<5, wipe $0B00,X / $7E3800,X
 * unless type at $0B01,X is $54; stride +$20 until X>=$0120. */
static bool actor_clear_94e7_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  /* 94ED/950C REP and 94F5/9516 SEP omit M rejects so both entry and loop modes work. */
  if((pc == 0x94ef || pc == 0x94f0 || pc == 0x94f1 || pc == 0x94f2 || pc == 0x94f3 ||
      pc == 0x94f4 || pc == 0x950e || pc == 0x950f || pc == 0x9510 ||
      pc == 0x9513 || pc == 0x9514) && c->mf) return false;
  if((pc == 0x94e7 || pc == 0x94e9 || pc == 0x94ea || pc == 0x94f7 ||
      pc == 0x94fa || pc == 0x94fc || pc == 0x94ff || pc == 0x9501 || pc == 0x9503 ||
      pc == 0x9505 || pc == 0x9508 || pc == 0x9518) && !c->mf)
    return false;
  switch(pc) {
    case 0x94e7: case 0x94e9: case 0x94ea: case 0x94ed: case 0x94ef: case 0x94f0:
    case 0x94f1: case 0x94f2: case 0x94f3: case 0x94f4: case 0x94f5: case 0x94f7:
    case 0x94fa: case 0x94fc: case 0x94ff: case 0x9501: case 0x9503: case 0x9505:
    case 0x9508: case 0x950c: case 0x950e: case 0x950f: case 0x9510: case 0x9513:
    case 0x9514: case 0x9516: case 0x9518:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x94e7: byte_constant(c, 0x00); break;
    case 0x94e9: xba_native(c); break;
    case 0x94ea: video_load(c, 0x01bb, false); break;
    case 0x94ed: set_m(c, false); break;
    case 0x94ef: case 0x94f0: case 0x94f1: case 0x94f2: case 0x94f3: asl_a16(c); break;
    case 0x94f4: implied(c); c->x = c->a; zn(c, c->x, false); break;
    case 0x94f5: set_m(c, true); break;
    case 0x94f7: cpx16_constant(c, 0x0120); break;
    case 0x94fa: branch_to(c, c->c, 0x9518); break;
    case 0x94fc: load_a_abs_x8(c, 0x0b01); break;
    case 0x94ff: cmp_constant8(c, 0x54); break;
    case 0x9501: branch_to(c, c->z, 0x950c); break;
    case 0x9503: byte_constant(c, 0x00); break;
    case 0x9505: store_a_abs_x8(c, 0x0b00); break;
    case 0x9508: store_a_long_x8(c, 0x7e3800); break;
    case 0x950c: set_m(c, false); break;
    case 0x950e: implied(c); c->a = c->x; zn(c, c->a, false); break;
    case 0x950f: implied(c); c->c = false; break;
    case 0x9510: adc16_constant(c, 0x0020); break;
    case 0x9513: implied(c); c->x = c->a; zn(c, c->x, false); break;
    case 0x9514: branch_to(c, true, 0x94f5); break;
    case 0x9516: set_m(c, true); break;
    case 0x9518: return_short(c); break;
  }
  return true;
}

/* $03FBDA: deeper scene-entry handlers past FB8D early-exit.
 * Mode-0 path ($FBDA) and mode-2 path ($FC1E) through RTL at $FC1D/$FC32.
 * Specialty JMP $FC47 and nested JSLs leave native range for the interpreter. */
static bool scene_entry_fbda_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x03 || c->e || c->d || !c->mf) return false;
  switch(pc) {
    case 0xfbda: case 0xfbdd: case 0xfbdf: case 0xfbe1: case 0xfbe3: case 0xfbe6:
    case 0xfbe8: case 0xfbeb: case 0xfbed: case 0xfbef: case 0xfbf1: case 0xfbf4:
    case 0xfbf6: case 0xfbf8: case 0xfbfb: case 0xfbfd: case 0xfc00: case 0xfc02:
    case 0xfc04: case 0xfc06: case 0xfc09: case 0xfc0d: case 0xfc0f: case 0xfc13:
    case 0xfc15: case 0xfc17: case 0xfc19: case 0xfc1d: case 0xfc1e: case 0xfc21:
    case 0xfc23: case 0xfc27: case 0xfc29: case 0xfc2d: case 0xfc2f: case 0xfc32:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xfbda: video_load(c, 0x01d7, false); break;
    case 0xfbdd: branch_to(c, !c->n, 0xfbf1); break;
    case 0xfbdf: and_constant8(c, 0x40); break;
    case 0xfbe1: branch_to(c, !c->z, 0xfbf8); break;
    case 0xfbe3: video_load(c, 0x1648, false); break;
    case 0xfbe6: branch_to(c, !c->n, 0xfc1d); break;
    case 0xfbe8: video_load(c, 0x15fa, false); break;
    case 0xfbeb: cmp_constant8(c, 0x80); break;
    case 0xfbed: branch_to(c, !c->z, 0xfc1d); break;
    case 0xfbef: branch_to(c, true, 0xfc04); break;
    case 0xfbf1: video_load(c, 0x1661, false); break;
    case 0xfbf4: cmp_constant8(c, 0x0b); break;
    case 0xfbf6: branch_to(c, !c->z, 0xfc1d); break;
    case 0xfbf8: video_load(c, 0x1648, false); break;
    case 0xfbfb: branch_to(c, !c->n, 0xfc09); break;
    case 0xfbfd: video_load(c, 0x15fa, false); break;
    case 0xfc00: cmp_constant8(c, 0x80); break;
    case 0xfc02: branch_to(c, !c->z, 0xfc09); break;
    case 0xfc04: byte_constant(c, 0x07); break;
    case 0xfc06: jmp_abs_native(c, 0xfc47); break;
    case 0xfc09: jsl_to(c, 0xe340); break;
    case 0xfc0d: branch_to(c, !c->c, 0xfc1d); break;
    case 0xfc0f: jsl_to(c, 0x8996); break;
    case 0xfc13: byte_constant(c, 0x02); break;
    case 0xfc15: store_dp(c, 0x24, true); break;
    case 0xfc17: byte_constant(c, 0x01); break;
    case 0xfc19: jsl_to(c, 0x85a1); break;
    case 0xfc1d: case 0xfc32: return_long(c); break;
    case 0xfc1e: video_load(c, 0x01d7, false); break;
    case 0xfc21: branch_to(c, !c->n, 0xfc1d); break;
    case 0xfc23: jsl_to(c, 0xfc74); break;
    case 0xfc27: branch_to(c, c->c, 0xfc2f); break;
    case 0xfc29: jsl_to(c, 0xe25c); break;
    case 0xfc2d: branch_to(c, !c->c, 0xfc1d); break;
    case 0xfc2f: stz_abs8(c, 0x0024); break;
  }
  return true;
}

/* $03FC33: specialty prologue — gates on $1661/$1648/$15FA, PLA, fall into $FC47.
 * $03FC47: set $121F/$24, mask HDMA, clear flags, JSL helpers, RTL. */
static void pla8_native(Cpu *c) {
  idle(c); idle(c); sample_interrupt(c); set_a8(c, pull(c));
}
static bool scene_specialty_fc47_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x03 || c->e || c->d || !c->mf) return false;
  switch(pc) {
    case 0xfc33: case 0xfc36: case 0xfc38: case 0xfc3a: case 0xfc3d: case 0xfc3f:
    case 0xfc42: case 0xfc44: case 0xfc46:
    case 0xfc47: case 0xfc49: case 0xfc4c: case 0xfc4f: case 0xfc52: case 0xfc54:
    case 0xfc57: case 0xfc5a: case 0xfc5d: case 0xfc61: case 0xfc65: case 0xfc69:
    case 0xfc6c: case 0xfc6f: case 0xfc71: case 0xfc73:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xfc33: video_load(c, 0x1661, false); break;
    case 0xfc36: cmp_constant8(c, 0x0a); break;
    case 0xfc38: branch_to(c, !c->z, 0xfbc7); break;
    case 0xfc3a: video_load(c, 0x1648, false); break;
    case 0xfc3d: branch_to(c, !c->n, 0xfbc7); break;
    case 0xfc3f: video_load(c, 0x15fa, false); break;
    case 0xfc42: cmp_constant8(c, 0x80); break;
    case 0xfc44: branch_to(c, !c->z, 0xfbc7); break;
    case 0xfc46: pla8_native(c); break;
    case 0xfc47: byte_constant(c, 0x07); break;
    case 0xfc49: store_register8(c, 0x121f); break;
    case 0xfc4c: stz_abs8(c, 0x0061); break;
    case 0xfc4f: video_load(c, 0x0062, false); break;
    case 0xfc52: and_constant8(c, 0xe7); break;
    case 0xfc54: store_register8(c, 0x0062); break;
    case 0xfc57: store_register8(c, 0x420c); break;
    case 0xfc5a: stz_abs8(c, 0x0d66); break;
    case 0xfc5d: jsl_to(c, 0xb61e); break;
    case 0xfc61: jsl_to(c, 0xb67a); break;
    case 0xfc65: jsl_to(c, 0x8324); break;
    case 0xfc69: stz_abs8(c, 0x1648); break;
    case 0xfc6c: stz_abs8(c, 0x01d7); break;
    case 0xfc6f: byte_constant(c, 0x07); break;
    case 0xfc71: store_dp(c, 0x24, true); break;
    case 0xfc73: return_long(c); break;
  }
  return true;
}

/* $03FC74: scene-entry predicate — CLC/RTL unless $1648<0, $01D8==$11, $15FA<0;
 * then clear $01E2/$01D2 and SEC/RTL. */
static bool scene_pred_fc74_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x03 || c->e || c->d || !c->mf) return false;
  switch(pc) {
    case 0xfc74: case 0xfc77: case 0xfc79: case 0xfc7c: case 0xfc7e: case 0xfc80:
    case 0xfc81: case 0xfc82: case 0xfc83: case 0xfc84: case 0xfc87: case 0xfc89:
    case 0xfc8b: case 0xfc8e: case 0xfc91: case 0xfc92:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xfc74: video_load(c, 0x1648, false); break;
    case 0xfc77: branch_to(c, !c->n, 0xfc80); break;
    case 0xfc79: video_load(c, 0x01d8, false); break;
    case 0xfc7c: cmp_constant8(c, 0x11); break;
    case 0xfc7e: branch_to(c, c->z, 0xfc84); break;
    case 0xfc80: implied(c); c->c = false; break;
    case 0xfc81: return_long(c); break;
    case 0xfc82: implied(c); c->c = true; break;
    case 0xfc83: return_long(c); break;
    case 0xfc84: video_load(c, 0x15fa, false); break;
    case 0xfc87: branch_to(c, !c->n, 0xfc80); break;
    case 0xfc89: byte_constant(c, 0x00); break;
    case 0xfc8b: store_register8(c, 0x01e2); break;
    case 0xfc8e: store_register8(c, 0x01d2); break;
    case 0xfc91: implied(c); c->c = true; break;
    case 0xfc92: return_long(c); break;
  }
  return true;
}

/* $03A6CB: if $01D6==$60 clear it, JSL $E419/$E4F4, clear $01BC; RTL. */
static bool scene_gate_a6cb_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x03 || c->e || c->d || !c->mf) return false;
  switch(pc) {
    case 0xa6cb: case 0xa6ce: case 0xa6d0: case 0xa6d2: case 0xa6d5: case 0xa6d9:
    case 0xa6dd: case 0xa6e0:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xa6cb: video_load(c, 0x01d6, false); break;
    case 0xa6ce: cmp_constant8(c, 0x60); break;
    case 0xa6d0: branch_to(c, !c->z, 0xa6e0); break;
    case 0xa6d2: stz_abs8(c, 0x01d6); break;
    case 0xa6d5: jsl_to(c, 0xe419); break;
    case 0xa6d9: jsl_to(c, 0xe4f4); break;
    case 0xa6dd: stz_abs8(c, 0x01bc); break;
    case 0xa6e0: return_long(c); break;
  }
  return true;
}

/* $039255: clear $1000.. via STA $1000,Y + JSL $8B07 until Y==$00C0; RTL. */
static bool clear_1000_9255_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x03 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x9255: case 0x9258: case 0x925a: case 0x925d: case 0x9261: case 0x9264:
    case 0x9266:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x9255: index_constant(c, true, 0x0000); break;
    case 0x9258: byte_constant(c, 0x00); break;
    case 0x925a: store_a_abs_y8(c, 0x1000); break;
    case 0x925d: jsl_to(c, 0x8b07); break;
    case 0x9261: cpy16_constant(c, 0x00c0); break;
    case 0x9264: branch_to(c, !c->c, 0x9258); break;
    case 0x9266: return_long(c); break;
  }
  return true;
}

/* $06E837: scene-state wipe — STZ cascade, helper JSLs, actor/slot clear loops,
 * then JSL $03EF29 and RTL. */
static bool scene_wipe_e837_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x06 || c->e || c->xf || c->d) return false;
  if((pc == 0xe8a8 || pc == 0xe8aa) && c->mf) return false;
  if((pc == 0xe837 || pc == 0xe83a || pc == 0xe83d || pc == 0xe840 ||
      pc == 0xe843 || pc == 0xe846 || pc == 0xe849 || pc == 0xe84c ||
      pc == 0xe84f || pc == 0xe852 || pc == 0xe855 || pc == 0xe858 ||
      pc == 0xe85c || pc == 0xe860 || pc == 0xe863 || pc == 0xe866 ||
      pc == 0xe868 || pc == 0xe86b || pc == 0xe86d || pc == 0xe870 ||
      pc == 0xe873 || pc == 0xe876 || pc == 0xe879 || pc == 0xe87b ||
      pc == 0xe87d || pc == 0xe87f || pc == 0xe882 || pc == 0xe886 ||
      pc == 0xe889 || pc == 0xe88b || pc == 0xe88e || pc == 0xe891 ||
      pc == 0xe893 || pc == 0xe896 || pc == 0xe898 || pc == 0xe89a ||
      pc == 0xe89d || pc == 0xe8a1 || pc == 0xe8a3 || pc == 0xe8a6 ||
      pc == 0xe8ad || pc == 0xe8af || pc == 0xe8b3 || pc == 0xe8b6 ||
      pc == 0xe8b8 || pc == 0xe8bb || pc == 0xe8bd || pc == 0xe8c0 ||
      pc == 0xe8c3 || pc == 0xe8c6 || pc == 0xe8ca || pc == 0xe8cd ||
      pc == 0xe8cf || pc == 0xe8d2 || pc == 0xe8d5 || pc == 0xe8d7 ||
      pc == 0xe8d9 || pc == 0xe8db || pc == 0xe8de || pc == 0xe8e0 ||
      pc == 0xe8e4 || pc == 0xe8e7 || pc == 0xe8e9 || pc == 0xe8ed) && !c->mf)
    return false;
  switch(pc) {
    case 0xe837: case 0xe83a: case 0xe83d: case 0xe840: case 0xe843: case 0xe846:
    case 0xe849: case 0xe84c: case 0xe84f: case 0xe852: case 0xe855: case 0xe858:
    case 0xe85c: case 0xe860: case 0xe863: case 0xe866: case 0xe868: case 0xe86b:
    case 0xe86d: case 0xe870: case 0xe873: case 0xe876: case 0xe879: case 0xe87b:
    case 0xe87d: case 0xe87f: case 0xe882: case 0xe886: case 0xe889: case 0xe88b:
    case 0xe88e: case 0xe891: case 0xe893: case 0xe896: case 0xe898: case 0xe89a:
    case 0xe89d: case 0xe8a1: case 0xe8a3: case 0xe8a6: case 0xe8a8: case 0xe8aa:
    case 0xe8ad: case 0xe8af: case 0xe8b3: case 0xe8b6: case 0xe8b8: case 0xe8bb:
    case 0xe8bd: case 0xe8c0: case 0xe8c3: case 0xe8c6: case 0xe8ca: case 0xe8cd:
    case 0xe8cf: case 0xe8d2: case 0xe8d5: case 0xe8d7: case 0xe8d9: case 0xe8db:
    case 0xe8de: case 0xe8e0: case 0xe8e4: case 0xe8e7: case 0xe8e9: case 0xe8ed:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe837: stz_abs8(c, 0x0c40); break;
    case 0xe83a: stz_abs8(c, 0x0c36); break;
    case 0xe83d: stz_abs8(c, 0x0c37); break;
    case 0xe840: stz_abs8(c, 0x0c42); break;
    case 0xe843: stz_abs8(c, 0x0c35); break;
    case 0xe846: stz_abs8(c, 0x10c1); break;
    case 0xe849: stz_abs8(c, 0x10c2); break;
    case 0xe84c: stz_abs8(c, 0x0726); break;
    case 0xe84f: stz_abs8(c, 0x0727); break;
    case 0xe852: stz_abs8(c, 0x0ea1); break;
    case 0xe855: stz_abs8(c, 0x01ee); break;
    case 0xe858: jsl_to(c, 0xe942); break;
    case 0xe85c: jsl_to(c, 0xe8ee); break;
    case 0xe860: index_constant(c, false, 0x0000); break;
    case 0xe863: load_a_abs_x8(c, 0x0b00); break;
    case 0xe866: and_constant8(c, 0xc7); break;
    case 0xe868: store_a_abs_x8(c, 0x0b00); break;
    case 0xe86b: byte_constant(c, 0x00); break;
    case 0xe86d: store_a_abs_x8(c, 0x0b19); break;
    case 0xe870: store_a_abs_x8(c, 0x0b12); break;
    case 0xe873: store_a_abs_x8(c, 0x0b13); break;
    case 0xe876: load_a_abs_x8(c, 0x0b01); break;
    case 0xe879: cmp_constant8(c, 0x3a); break;
    case 0xe87b: branch_to(c, !c->z, 0xe882); break;
    case 0xe87d: byte_constant(c, 0x39); break;
    case 0xe87f: store_a_abs_x8(c, 0x0b01); break;
    case 0xe882: jsl_to(c, 0x8afc); break;
    case 0xe886: cpx16_constant(c, 0x0120); break;
    case 0xe889: branch_to(c, !c->c, 0xe863); break;
    case 0xe88b: index_constant(c, true, 0x0000); break;
    case 0xe88e: load_a_abs_y8(c, 0x0140); break;
    case 0xe891: and_constant8(c, 0xdf); break;
    case 0xe893: store_a_abs_y8(c, 0x0140); break;
    case 0xe896: branch_to(c, !c->n, 0xe8af); break;
    case 0xe898: byte_constant(c, 0x00); break;
    case 0xe89a: store_a_abs_y8(c, 0x0140); break;
    case 0xe89d: jsl_to(c, 0xc257); break;
    case 0xe8a1: load_dp8(c, 0x12); break;
    case 0xe8a3: store_a_abs_y8(c, 0x0145); break;
    case 0xe8a6: set_m(c, false); break;
    case 0xe8a8: load_dp16_to_a(c, 0x10); break;
    case 0xe8aa: store_a_abs_y16(c, 0x0143); break;
    case 0xe8ad: set_m(c, true); break;
    case 0xe8af: jsl_to(c, 0x8b07); break;
    case 0xe8b3: cpy16_constant(c, 0x001e); break;
    case 0xe8b6: branch_to(c, !c->c, 0xe88e); break;
    case 0xe8b8: index_constant(c, false, 0x0000); break;
    case 0xe8bb: byte_constant(c, 0x00); break;
    case 0xe8bd: store_a_abs_x8(c, 0x0e19); break;
    case 0xe8c0: store_a_abs_x8(c, 0x0e12); break;
    case 0xe8c3: store_a_abs_x8(c, 0x0e13); break;
    case 0xe8c6: jsl_to(c, 0x8afc); break;
    case 0xe8ca: cpx16_constant(c, 0x00a0); break;
    case 0xe8cd: branch_to(c, !c->c, 0xe8bb); break;
    case 0xe8cf: index_constant(c, false, 0x0000); break;
    case 0xe8d2: load_a_abs_x8(c, 0x0b01); break;
    case 0xe8d5: cmp_constant8(c, 0x4a); break;
    case 0xe8d7: branch_to(c, !c->z, 0xe8e0); break;
    case 0xe8d9: byte_constant(c, 0x00); break;
    case 0xe8db: store_a_abs_x8(c, 0x0b00); break;
    case 0xe8de: branch_to(c, true, 0xe8e9); break;
    case 0xe8e0: jsl_to(c, 0x8afc); break;
    case 0xe8e4: cpx16_constant(c, 0x0120); break;
    case 0xe8e7: branch_to(c, !c->c, 0xe8d2); break;
    case 0xe8e9: jsl_to(c, 0xef29); break;
    case 0xe8ed: return_long(c); break;
  }
  return true;
}

/* $03E340: early gate — REP, CMP $166C vs #$0800, SEP; RTL when below;
 * else JSL helpers and seed $0EA3/$0EA0 / clear $01E7 bit2 through $E36A. */
static bool early_gate_e340_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x03 || c->e || c->xf || c->d) return false;
  if((pc == 0xe342 || pc == 0xe345) && c->mf) return false;
  if((pc == 0xe34a || pc == 0xe34c ||
      pc == 0xe34d || pc == 0xe351 || pc == 0xe355 || pc == 0xe359 ||
      pc == 0xe35b || pc == 0xe35e || pc == 0xe360 || pc == 0xe363 ||
      pc == 0xe366 || pc == 0xe368) && !c->mf) return false;
  switch(pc) {
    case 0xe340: case 0xe342: case 0xe345: case 0xe348: case 0xe34a: case 0xe34c:
    case 0xe34d: case 0xe351: case 0xe355: case 0xe359: case 0xe35b: case 0xe35e:
    case 0xe360: case 0xe363: case 0xe366: case 0xe368:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe340: set_m(c, false); break;
    case 0xe342: video_load(c, 0x166c, false); break;
    case 0xe345: cmp_constant16(c, 0x0800); break;
    case 0xe348: set_m(c, true); break;
    case 0xe34a: branch_to(c, c->c, 0xe34d); break;
    case 0xe34c: return_long(c); break;
    case 0xe34d: jsl_to(c, 0xe837); break;
    case 0xe351: jsl_to(c, 0xe419); break;
    case 0xe355: jsl_to(c, 0xe4d1); break;
    case 0xe359: byte_constant(c, 0x15); break;
    case 0xe35b: store_register8(c, 0x0ea3); break;
    case 0xe35e: byte_constant(c, 0x01); break;
    case 0xe360: store_register8(c, 0x0ea0); break;
    case 0xe363: video_load(c, 0x01e7, false); break;
    case 0xe366: and_constant8(c, 0xfb); break;
    case 0xe368: store_register8(c, 0x01e7); break;
  }
  return true;
}

static void bit_dp8(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset); sample_interrupt(c);
  uint8_t value = c->read(c->mem, address);
  c->n = (value & 0x80u) != 0;
  c->v = (value & 0x40u) != 0;
  c->z = (((uint8_t)c->a) & value) == 0;
}
static void eor_indirect_long_y8(Cpu *c, uint8_t offset) {
  uint16_t dp = dp_address(c, offset);
  uint32_t pointer = c->read(c->mem, dp);
  pointer |= (uint32_t)c->read(c->mem, (uint16_t)(dp + 1)) << 8;
  pointer |= (uint32_t)c->read(c->mem, (uint16_t)(dp + 2)) << 16;
  sample_interrupt(c);
  set_a8(c, (uint8_t)((uint8_t)c->a ^ c->read(c->mem, (pointer + c->y) & 0xffffffu)));
}
static void sbc_indirect_long_y8(Cpu *c, uint8_t offset) {
  uint16_t dp = dp_address(c, offset);
  uint32_t pointer = c->read(c->mem, dp);
  pointer |= (uint32_t)c->read(c->mem, (uint16_t)(dp + 1)) << 8;
  pointer |= (uint32_t)c->read(c->mem, (uint16_t)(dp + 2)) << 16;
  sample_interrupt(c);
  sbc8_value(c, c->read(c->mem, (pointer + c->y) & 0xffffffu));
}
static void and_abs_x8(Cpu *c, uint16_t base) {
  uint32_t address = video_address(c, base, true); sample_interrupt(c);
  set_a8(c, (uint8_t)((uint8_t)c->a & c->read(c->mem, address)));
}
static void ora_abs_x8(Cpu *c, uint16_t base) {
  uint32_t address = video_address(c, base, true); sample_interrupt(c);
  set_a8(c, (uint8_t)((uint8_t)c->a | c->read(c->mem, address)));
}
static void ora_dp16(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  uint16_t value = read16(c, address);
  c->a |= value; zn(c, c->a, false);
}
static void and_dp16(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  uint16_t value = read16(c, address);
  c->a &= value; zn(c, c->a, false);
}
static void inc_dp8(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  uint8_t value = c->read(c->mem, address); idle(c); sample_interrupt(c);
  value++; c->write(c->mem, address, value); zn(c, value, true);
}
static void txa16(Cpu *c) { implied(c); c->a = c->x; zn(c, c->a, false); }
static void tax16(Cpu *c) { implied(c); c->x = c->a; zn(c, c->x, false); }

/* $00849C: actor→OAM writeback. Computes camera-relative DP+$12/$14 from
 * DP+$0A/$0C vs scroll DP+$35/$33, walks [DP+$00] records into $0400,X and
 * updates the $0600 high-table nibbles via ROM masks at $8589/$8599.
 * Alternate entries $85B9 (skip F0/STZ init) and $85CF (preset DP+$0E=$F0)
 * fall into the shared $84B5 walker. */
static bool oam_writeback_849c_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  if((pc == 0x849e || pc == 0x84a0 || pc == 0x84a1 || pc == 0x84a3 ||
      pc == 0x84a5 || pc == 0x84a7 || pc == 0x84a8 || pc == 0x84aa ||
      pc == 0x84ac || pc == 0x8563 || pc == 0x8564 ||
      pc == 0x8565 || pc == 0x8566 || pc == 0x8567 || pc == 0x8568 ||
      pc == 0x8569 || pc == 0x856a || pc == 0x856c || pc == 0x856e ||
      pc == 0x8571 || pc == 0x8572 || pc == 0x85bb || pc == 0x85bd ||
      pc == 0x85be || pc == 0x85c0 || pc == 0x85c2 || pc == 0x85c4 ||
      pc == 0x85c5 || pc == 0x85c7 || pc == 0x85c9 || pc == 0x85d1 ||
      pc == 0x85d3 || pc == 0x85d4 || pc == 0x85d6 || pc == 0x85d8 ||
      pc == 0x85da || pc == 0x85db || pc == 0x85dd || pc == 0x85df) && c->mf) return false;
  if((pc == 0x84ae || pc == 0x84af || pc == 0x84b1 || pc == 0x84b3 ||
      pc == 0x84b5 || pc == 0x84b8 || pc == 0x84ba || pc == 0x84bc ||
      pc == 0x84bd || pc == 0x84bf || pc == 0x84c1 || pc == 0x84c3 ||
      pc == 0x84c4 || pc == 0x84c5 || pc == 0x84c6 || pc == 0x84c7 ||
      pc == 0x84ca || pc == 0x84cc || pc == 0x84ce || pc == 0x84d0 ||
      pc == 0x84d2 || pc == 0x84d4 || pc == 0x84d5 || pc == 0x84d6 ||
      pc == 0x84d8 || pc == 0x84da || pc == 0x84dc || pc == 0x84de ||
      pc == 0x84df || pc == 0x84e1 || pc == 0x84e3 || pc == 0x84e5 ||
      pc == 0x84e7 || pc == 0x84e9 || pc == 0x84eb || pc == 0x84ed ||
      pc == 0x84ef || pc == 0x84f1 || pc == 0x84f3 || pc == 0x84f4 ||
      pc == 0x84f6 || pc == 0x84f8 || pc == 0x84f9 || pc == 0x84fb ||
      pc == 0x84fd || pc == 0x84ff || pc == 0x8501 || pc == 0x8503 ||
      pc == 0x8505 || pc == 0x8507 || pc == 0x8509 || pc == 0x850c ||
      pc == 0x850d || pc == 0x850f || pc == 0x8512 || pc == 0x8513 ||
      pc == 0x8515 || pc == 0x8517 || pc == 0x8519 || pc == 0x851b ||
      pc == 0x851e || pc == 0x851f || pc == 0x8521 || pc == 0x8523 ||
      pc == 0x8525 || pc == 0x8527 || pc == 0x8529 || pc == 0x852a ||
      pc == 0x852c || pc == 0x852d || pc == 0x852f || pc == 0x8531 ||
      pc == 0x8533 || pc == 0x8535 || pc == 0x8537 || pc == 0x8539 ||
      pc == 0x853b || pc == 0x853c || pc == 0x853e || pc == 0x8540 ||
      pc == 0x8541 || pc == 0x8543 || pc == 0x8545 || pc == 0x8547 ||
      pc == 0x8549 || pc == 0x854b || pc == 0x854d || pc == 0x854f ||
      pc == 0x8551 || pc == 0x8554 || pc == 0x8557 || pc == 0x8559 ||
      pc == 0x855b || pc == 0x855e || pc == 0x855f || pc == 0x8560 ||
      pc == 0x8574 || pc == 0x8577 || pc == 0x857a || pc == 0x857d ||
      pc == 0x8580 || pc == 0x8581 || pc == 0x8582 || pc == 0x8583 ||
      pc == 0x8584 || pc == 0x8585 || pc == 0x8586 || pc == 0x85cb ||
      pc == 0x85cc || pc == 0x85e1 || pc == 0x85e3 || pc == 0x85e5 ||
      pc == 0x85e6) && !c->mf) return false;
  switch(pc) {
    case 0x849c: case 0x849e: case 0x84a0: case 0x84a1: case 0x84a3: case 0x84a5:
    case 0x84a7: case 0x84a8: case 0x84aa: case 0x84ac: case 0x84ae: case 0x84af:
    case 0x84b1: case 0x84b3: case 0x84b5: case 0x84b8: case 0x84ba: case 0x84bc:
    case 0x84bd: case 0x84bf: case 0x84c1: case 0x84c3: case 0x84c4: case 0x84c5:
    case 0x84c6: case 0x84c7: case 0x84ca: case 0x84cc: case 0x84ce: case 0x84d0:
    case 0x84d2: case 0x84d4: case 0x84d5: case 0x84d6: case 0x84d8: case 0x84da:
    case 0x84dc: case 0x84de: case 0x84df: case 0x84e1: case 0x84e3: case 0x84e5:
    case 0x84e7: case 0x84e9: case 0x84eb: case 0x84ed: case 0x84ef: case 0x84f1:
    case 0x84f3: case 0x84f4: case 0x84f6: case 0x84f8: case 0x84f9: case 0x84fb:
    case 0x84fd: case 0x84ff: case 0x8501: case 0x8503: case 0x8505: case 0x8507:
    case 0x8509: case 0x850c: case 0x850d: case 0x850f: case 0x8512: case 0x8513:
    case 0x8515: case 0x8517: case 0x8519: case 0x851b: case 0x851e: case 0x851f:
    case 0x8521: case 0x8523: case 0x8525: case 0x8527: case 0x8529: case 0x852a:
    case 0x852c: case 0x852d: case 0x852f: case 0x8531: case 0x8533: case 0x8535:
    case 0x8537: case 0x8539: case 0x853b: case 0x853c: case 0x853e: case 0x8540:
    case 0x8541: case 0x8543: case 0x8545: case 0x8547: case 0x8549: case 0x854b:
    case 0x854d: case 0x854f: case 0x8551: case 0x8554: case 0x8557: case 0x8559:
    case 0x855b: case 0x855e: case 0x855f: case 0x8560: case 0x8561: case 0x8563:
    case 0x8564: case 0x8565: case 0x8566: case 0x8567: case 0x8568: case 0x8569:
    case 0x856a: case 0x856c: case 0x856e: case 0x8571: case 0x8572: case 0x8574:
    case 0x8577: case 0x857a: case 0x857d: case 0x8580: case 0x8581: case 0x8582:
    case 0x8583: case 0x8584: case 0x8585: case 0x8586: case 0x85b9: case 0x85bb:
    case 0x85bd: case 0x85be: case 0x85c0: case 0x85c2: case 0x85c4: case 0x85c5:
    case 0x85c7: case 0x85c9: case 0x85cb: case 0x85cc: case 0x85cf: case 0x85d1:
    case 0x85d3: case 0x85d4: case 0x85d6: case 0x85d8: case 0x85da: case 0x85db:
    case 0x85dd: case 0x85df: case 0x85e1: case 0x85e3: case 0x85e5: case 0x85e6:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x849c: sep_rep_native(c, 0x20, false); break;
    case 0x849e: load_dp16_to_a(c, 0x0a); break;
    case 0x84a0: implied(c); c->c = true; break;
    case 0x84a1: sbc_dp16(c, 0x35); break;
    case 0x84a3: store_dp(c, 0x12, false); break;
    case 0x84a5: load_dp16_to_a(c, 0x0c); break;
    case 0x84a7: implied(c); c->c = true; break;
    case 0x84a8: sbc_dp16(c, 0x33); break;
    case 0x84aa: store_dp(c, 0x14, false); break;
    case 0x84ac: sep_rep_native(c, 0x20, true); break;
    case 0x84ae: phx16(c); break;
    case 0x84af: stz_dp8(c, 0x09); break;
    case 0x84b1: byte_constant(c, 0xf0); break;
    case 0x84b3: store_dp(c, 0x0e, true); break;
    case 0x84b5: index_constant(c, true, 0x0000); break;
    case 0x84b8: lda_indirect_long_y8(c, 0x00); break;
    case 0x84ba: and_constant8(c, 0x01); break;
    case 0x84bc: asl_a8(c); break;
    case 0x84bd: store_dp(c, 0x04, true); break;
    case 0x84bf: load_dp_x16(c, 0x55); break;
    case 0x84c1: branch_to(c, true, 0x84c6); break;
    case 0x84c3: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x84c4: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x84c5: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x84c6: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x84c7: cpx16_constant(c, 0x0200); break;
    case 0x84ca: branch_to(c, c->c, 0x84d2); break;
    case 0x84cc: lda_indirect_long_y8(c, 0x00); break;
    case 0x84ce: cmp_constant8(c, 0x80); break;
    case 0x84d0: branch_to(c, !c->z, 0x84d6); break;
    case 0x84d2: store_dp_x16(c, 0x55); break;
    case 0x84d4: plx16(c); break;
    case 0x84d5: return_long(c); break;
    case 0x84d6: store_dp(c, 0x08, true); break;
    case 0x84d8: bit_dp8(c, 0x03); break;
    case 0x84da: branch_to(c, !c->n, 0x84e3); break;
    case 0x84dc: byte_constant(c, 0xf8); break;
    case 0x84de: implied(c); c->c = true; break;
    case 0x84df: sbc_dp8(c, 0x08); break;
    case 0x84e1: store_dp(c, 0x08, true); break;
    case 0x84e3: cmp_constant8(c, 0x80); break;
    case 0x84e5: branch_to(c, c->c, 0x84f1); break;
    case 0x84e7: adc8_dp(c, 0x12); break;
    case 0x84e9: store_dp(c, 0x10, true); break;
    case 0x84eb: load_dp8(c, 0x13); break;
    case 0x84ed: adc8_constant(c, 0x00); break;
    case 0x84ef: branch_to(c, true, 0x8501); break;
    case 0x84f1: eor_constant8(c, 0xff); break;
    case 0x84f3: inc_a8(c); break;
    case 0x84f4: store_dp(c, 0x05, true); break;
    case 0x84f6: load_dp8(c, 0x12); break;
    case 0x84f8: implied(c); c->c = true; break;
    case 0x84f9: sbc_dp8(c, 0x05); break;
    case 0x84fb: store_dp(c, 0x10, true); break;
    case 0x84fd: load_dp8(c, 0x13); break;
    case 0x84ff: sbc8_constant(c, 0x00); break;
    case 0x8501: branch_to(c, !c->z, 0x84c3); break;
    case 0x8503: load_dp8(c, 0x10); break;
    case 0x8505: cmp_dp8(c, 0x0e); break;
    case 0x8507: branch_to(c, c->c, 0x84c3); break;
    case 0x8509: store_a_abs_x8(c, 0x0401); break;
    case 0x850c: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x850d: lda_indirect_long_y8(c, 0x00); break;
    case 0x850f: store_a_abs_x8(c, 0x0402); break;
    case 0x8512: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x8513: load_dp8(c, 0x03); break;
    case 0x8515: and_constant8(c, 0xfe); break;
    case 0x8517: eor_indirect_long_y8(c, 0x00); break;
    case 0x8519: ora_dp8(c, 0x09); break;
    case 0x851b: store_a_abs_x8(c, 0x0403); break;
    case 0x851e: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x851f: stz_dp8(c, 0x06); break;
    case 0x8521: bit_dp8(c, 0x03); break;
    case 0x8523: lda_indirect_long_y8(c, 0x00); break;
    case 0x8525: branch_to(c, !c->v, 0x852c); break;
    case 0x8527: byte_constant(c, 0xf8); break;
    case 0x8529: implied(c); c->c = true; break;
    case 0x852a: sbc_indirect_long_y8(c, 0x00); break;
    case 0x852c: implied(c); c->c = false; break;
    case 0x852d: branch_to(c, c->n, 0x8539); break;
    case 0x852f: adc8_dp(c, 0x14); break;
    case 0x8531: store_dp(c, 0x10, true); break;
    case 0x8533: load_dp8(c, 0x15); break;
    case 0x8535: adc8_constant(c, 0x00); break;
    case 0x8537: branch_to(c, true, 0x8549); break;
    case 0x8539: eor_constant8(c, 0xff); break;
    case 0x853b: inc_a8(c); break;
    case 0x853c: store_dp(c, 0x05, true); break;
    case 0x853e: load_dp8(c, 0x14); break;
    case 0x8540: implied(c); c->c = true; break;
    case 0x8541: sbc_dp8(c, 0x05); break;
    case 0x8543: store_dp(c, 0x10, true); break;
    case 0x8545: load_dp8(c, 0x15); break;
    case 0x8547: sbc8_constant(c, 0x00); break;
    case 0x8549: branch_to(c, c->z, 0x8559); break;
    case 0x854b: cmp_constant8(c, 0xff); break;
    case 0x854d: branch_to(c, c->z, 0x8557); break;
    case 0x854f: byte_constant(c, 0xf0); break;
    case 0x8551: store_a_abs_x8(c, 0x0401); break;
    case 0x8554: jmp_abs_native(c, 0x84c6); break;
    case 0x8557: inc_dp8(c, 0x06); break;
    case 0x8559: load_dp8(c, 0x10); break;
    case 0x855b: store_a_abs_x8(c, 0x0400); break;
    case 0x855e: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x855f: phx16(c); break;
    case 0x8560: phy16(c); break;
    case 0x8561: sep_rep_native(c, 0x20, false); break;
    case 0x8563: txa16(c); break;
    case 0x8564: lsr_a16(c); break;
    case 0x8565: lsr_a16(c); break;
    case 0x8566: lsr_a16(c); break;
    case 0x8567: lsr_a16(c); break;
    case 0x8568: tay16(c); break;
    case 0x8569: txa16(c); break;
    case 0x856a: ora_dp16(c, 0x06); break;
    case 0x856c: ora_dp16(c, 0x04); break;
    case 0x856e: and_constant16(c, 0x000f); break;
    case 0x8571: tax16(c); break;
    case 0x8572: sep_rep_native(c, 0x20, true); break;
    case 0x8574: load_a_abs_y8(c, 0x0600); break;
    case 0x8577: and_abs_x8(c, 0x8599); break;
    case 0x857a: ora_abs_x8(c, 0x8589); break;
    case 0x857d: store_a_abs_y8(c, 0x0600); break;
    case 0x8580: ply16(c); break;
    case 0x8581: plx16(c); break;
    case 0x8582: implied(c); c->x++; zn(c, c->x, false); break;
    case 0x8583: implied(c); c->x++; zn(c, c->x, false); break;
    case 0x8584: implied(c); c->x++; zn(c, c->x, false); break;
    case 0x8585: implied(c); c->x++; zn(c, c->x, false); break;
    case 0x8586: jmp_abs_native(c, 0x84c7); break;
    case 0x85b9: sep_rep_native(c, 0x20, false); break;
    case 0x85bb: load_dp16_to_a(c, 0x0a); break;
    case 0x85bd: implied(c); c->c = true; break;
    case 0x85be: sbc_dp16(c, 0x35); break;
    case 0x85c0: store_dp(c, 0x12, false); break;
    case 0x85c2: load_dp16_to_a(c, 0x0c); break;
    case 0x85c4: implied(c); c->c = true; break;
    case 0x85c5: sbc_dp16(c, 0x33); break;
    case 0x85c7: store_dp(c, 0x14, false); break;
    case 0x85c9: sep_rep_native(c, 0x20, true); break;
    case 0x85cb: phx16(c); break;
    case 0x85cc: jmp_abs_native(c, 0x84b5); break;
    case 0x85cf: sep_rep_native(c, 0x20, false); break;
    case 0x85d1: load_dp16_to_a(c, 0x0a); break;
    case 0x85d3: implied(c); c->c = true; break;
    case 0x85d4: sbc_dp16(c, 0x35); break;
    case 0x85d6: store_dp(c, 0x12, false); break;
    case 0x85d8: load_dp16_to_a(c, 0x0c); break;
    case 0x85da: implied(c); c->c = true; break;
    case 0x85db: sbc_dp16(c, 0x33); break;
    case 0x85dd: store_dp(c, 0x14, false); break;
    case 0x85df: sep_rep_native(c, 0x20, true); break;
    case 0x85e1: byte_constant(c, 0xf0); break;
    case 0x85e3: store_dp(c, 0x0e, true); break;
    case 0x85e5: phx16(c); break;
    case 0x85e6: jmp_abs_native(c, 0x84b5); break;
  }
  return true;
}

static void eor_dp8(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset); sample_interrupt(c);
  set_a8(c, (uint8_t)((uint8_t)c->a ^ c->read(c->mem, address)));
}

/* $0085E9: parallel actor→OAM writeback (screen-space). Entries $85E9 (clear
 * DP+$12/$10) and $85EF (preset DP+$10=$01, clear $12) share the $85F5 walker.
 * Uses DP+$0A/$0C as 8-bit Y/X bases (no scroll subtract), walks [DP+$00]
 * records into $0400,X, and updates $0600 via the same $8589/$8599 masks. */
static bool oam_writeback_85e9_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  if((pc == 0x867a || pc == 0x867b || pc == 0x867c || pc == 0x867d ||
      pc == 0x867e || pc == 0x867f || pc == 0x8680 || pc == 0x8681 ||
      pc == 0x8683 || pc == 0x8685 || pc == 0x8688) && c->mf) return false;
  if((pc == 0x85e9 || pc == 0x85eb || pc == 0x85ed || pc == 0x85ef ||
      pc == 0x85f1 || pc == 0x85f3 || pc == 0x85f5 || pc == 0x85f6 ||
      pc == 0x85f9 || pc == 0x85fb || pc == 0x85fd || pc == 0x85fe ||
      pc == 0x8600 || pc == 0x8602 || pc == 0x8604 || pc == 0x8605 ||
      pc == 0x8606 || pc == 0x8607 || pc == 0x8608 || pc == 0x860b ||
      pc == 0x860d || pc == 0x860f || pc == 0x8611 || pc == 0x8613 ||
      pc == 0x8615 || pc == 0x8616 || pc == 0x8617 || pc == 0x8619 ||
      pc == 0x861b || pc == 0x861d || pc == 0x861f || pc == 0x8620 ||
      pc == 0x8622 || pc == 0x8624 || pc == 0x8626 || pc == 0x8628 ||
      pc == 0x862b || pc == 0x862c || pc == 0x862e || pc == 0x862f ||
      pc == 0x8631 || pc == 0x8634 || pc == 0x8635 || pc == 0x8637 ||
      pc == 0x8639 || pc == 0x863b || pc == 0x863d || pc == 0x863f ||
      pc == 0x8641 || pc == 0x8643 || pc == 0x8645 || pc == 0x8648 ||
      pc == 0x8649 || pc == 0x864b || pc == 0x864d || pc == 0x864f ||
      pc == 0x8651 || pc == 0x8653 || pc == 0x8654 || pc == 0x8656 ||
      pc == 0x8657 || pc == 0x8659 || pc == 0x865b || pc == 0x865d ||
      pc == 0x865f || pc == 0x8662 || pc == 0x8664 || pc == 0x8666 ||
      pc == 0x8667 || pc == 0x8669 || pc == 0x866b || pc == 0x866c ||
      pc == 0x866e || pc == 0x8670 || pc == 0x8672 || pc == 0x8675 ||
      pc == 0x8676 || pc == 0x8677 || pc == 0x868b ||
      pc == 0x868e || pc == 0x8691 || pc == 0x8694 || pc == 0x8697 ||
      pc == 0x8698 || pc == 0x8699 || pc == 0x869a || pc == 0x869b ||
      pc == 0x869c || pc == 0x869d) && !c->mf) return false;
  switch(pc) {
    case 0x85e9: case 0x85eb: case 0x85ed: case 0x85ef: case 0x85f1: case 0x85f3:
    case 0x85f5: case 0x85f6: case 0x85f9: case 0x85fb: case 0x85fd: case 0x85fe:
    case 0x8600: case 0x8602: case 0x8604: case 0x8605: case 0x8606: case 0x8607:
    case 0x8608: case 0x860b: case 0x860d: case 0x860f: case 0x8611: case 0x8613:
    case 0x8615: case 0x8616: case 0x8617: case 0x8619: case 0x861b: case 0x861d:
    case 0x861f: case 0x8620: case 0x8622: case 0x8624: case 0x8626: case 0x8628:
    case 0x862b: case 0x862c: case 0x862e: case 0x862f: case 0x8631: case 0x8634:
    case 0x8635: case 0x8637: case 0x8639: case 0x863b: case 0x863d: case 0x863f:
    case 0x8641: case 0x8643: case 0x8645: case 0x8648: case 0x8649: case 0x864b:
    case 0x864d: case 0x864f: case 0x8651: case 0x8653: case 0x8654: case 0x8656:
    case 0x8657: case 0x8659: case 0x865b: case 0x865d: case 0x865f: case 0x8662:
    case 0x8664: case 0x8666: case 0x8667: case 0x8669: case 0x866b: case 0x866c:
    case 0x866e: case 0x8670: case 0x8672: case 0x8675: case 0x8676: case 0x8677:
    case 0x8678: case 0x867a: case 0x867b: case 0x867c: case 0x867d: case 0x867e:
    case 0x867f: case 0x8680: case 0x8681: case 0x8683: case 0x8685: case 0x8688:
    case 0x8689: case 0x868b: case 0x868e: case 0x8691: case 0x8694: case 0x8697:
    case 0x8698: case 0x8699: case 0x869a: case 0x869b: case 0x869c: case 0x869d:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x85e9: stz_dp8(c, 0x12); break;
    case 0x85eb: stz_dp8(c, 0x10); break;
    case 0x85ed: branch_to(c, true, 0x85f5); break;
    case 0x85ef: byte_constant(c, 0x01); break;
    case 0x85f1: store_dp(c, 0x10, true); break;
    case 0x85f3: stz_dp8(c, 0x12); break;
    case 0x85f5: phx16(c); break;
    case 0x85f6: index_constant(c, true, 0x0000); break;
    case 0x85f9: lda_indirect_long_y8(c, 0x00); break;
    case 0x85fb: and_constant8(c, 0x01); break;
    case 0x85fd: asl_a8(c); break;
    case 0x85fe: store_dp(c, 0x04, true); break;
    case 0x8600: load_dp_x16(c, 0x55); break;
    case 0x8602: branch_to(c, true, 0x8607); break;
    case 0x8604: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x8605: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x8606: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x8607: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x8608: cpx16_constant(c, 0x0200); break;
    case 0x860b: branch_to(c, c->c, 0x8613); break;
    case 0x860d: lda_indirect_long_y8(c, 0x00); break;
    case 0x860f: cmp_constant8(c, 0x80); break;
    case 0x8611: branch_to(c, !c->z, 0x8617); break;
    case 0x8613: store_dp_x16(c, 0x55); break;
    case 0x8615: plx16(c); break;
    case 0x8616: return_long(c); break;
    case 0x8617: branch_to(c, c->c, 0x861f); break;
    case 0x8619: adc8_dp(c, 0x0a); break;
    case 0x861b: branch_to(c, c->c, 0x8604); break;
    case 0x861d: branch_to(c, !c->c, 0x8624); break;
    case 0x861f: implied(c); c->c = false; break;
    case 0x8620: adc8_dp(c, 0x0a); break;
    case 0x8622: branch_to(c, !c->c, 0x8604); break;
    case 0x8624: cmp_constant8(c, 0xf0); break;
    case 0x8626: branch_to(c, c->c, 0x8604); break;
    case 0x8628: store_a_abs_x8(c, 0x0401); break;
    case 0x862b: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x862c: lda_indirect_long_y8(c, 0x00); break;
    case 0x862e: implied(c); c->c = false; break;
    case 0x862f: adc8_dp(c, 0x12); break;
    case 0x8631: store_a_abs_x8(c, 0x0402); break;
    case 0x8634: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x8635: lda_indirect_long_y8(c, 0x00); break;
    case 0x8637: and_constant8(c, 0x01); break;
    case 0x8639: store_dp(c, 0x05, true); break;
    case 0x863b: lda_indirect_long_y8(c, 0x00); break;
    case 0x863d: eor_dp8(c, 0x03); break;
    case 0x863f: and_constant8(c, 0xfe); break;
    case 0x8641: ora_dp8(c, 0x05); break;
    case 0x8643: ora_dp8(c, 0x10); break;
    case 0x8645: store_a_abs_x8(c, 0x0403); break;
    case 0x8648: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x8649: stz_dp8(c, 0x06); break;
    case 0x864b: bit_dp8(c, 0x03); break;
    case 0x864d: lda_indirect_long_y8(c, 0x00); break;
    case 0x864f: branch_to(c, !c->v, 0x8656); break;
    case 0x8651: byte_constant(c, 0xf8); break;
    case 0x8653: implied(c); c->c = true; break;
    case 0x8654: sbc_indirect_long_y8(c, 0x00); break;
    case 0x8656: implied(c); c->c = false; break;
    case 0x8657: branch_to(c, c->n, 0x8664); break;
    case 0x8659: adc8_dp(c, 0x0c); break;
    case 0x865b: branch_to(c, !c->c, 0x8672); break;
    case 0x865d: byte_constant(c, 0xf0); break;
    case 0x865f: store_a_abs_x8(c, 0x0401); break;
    case 0x8662: branch_to(c, c->c, 0x8607); break;
    case 0x8664: eor_constant8(c, 0xff); break;
    case 0x8666: inc_a8(c); break;
    case 0x8667: store_dp(c, 0x05, true); break;
    case 0x8669: load_dp8(c, 0x0c); break;
    case 0x866b: implied(c); c->c = true; break;
    case 0x866c: sbc_dp8(c, 0x05); break;
    case 0x866e: branch_to(c, c->c, 0x8672); break;
    case 0x8670: inc_dp8(c, 0x06); break;
    case 0x8672: store_a_abs_x8(c, 0x0400); break;
    case 0x8675: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x8676: phx16(c); break;
    case 0x8677: phy16(c); break;
    case 0x8678: sep_rep_native(c, 0x20, false); break;
    case 0x867a: txa16(c); break;
    case 0x867b: lsr_a16(c); break;
    case 0x867c: lsr_a16(c); break;
    case 0x867d: lsr_a16(c); break;
    case 0x867e: lsr_a16(c); break;
    case 0x867f: tay16(c); break;
    case 0x8680: txa16(c); break;
    case 0x8681: ora_dp16(c, 0x06); break;
    case 0x8683: ora_dp16(c, 0x04); break;
    case 0x8685: and_constant16(c, 0x000f); break;
    case 0x8688: tax16(c); break;
    case 0x8689: sep_rep_native(c, 0x20, true); break;
    case 0x868b: load_a_abs_y8(c, 0x0600); break;
    case 0x868e: and_abs_x8(c, 0x8599); break;
    case 0x8691: ora_abs_x8(c, 0x8589); break;
    case 0x8694: store_a_abs_y8(c, 0x0600); break;
    case 0x8697: ply16(c); break;
    case 0x8698: plx16(c); break;
    case 0x8699: implied(c); c->x++; zn(c, c->x, false); break;
    case 0x869a: implied(c); c->x++; zn(c, c->x, false); break;
    case 0x869b: implied(c); c->x++; zn(c, c->x, false); break;
    case 0x869c: implied(c); c->x++; zn(c, c->x, false); break;
    case 0x869d: jmp_abs_native(c, 0x8608); break;
  }
  return true;
}

static void sty_dp16(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  c->write(c->mem, address, (uint8_t)c->y); sample_interrupt(c);
  c->write(c->mem, (uint16_t)(address + 1), (uint8_t)(c->y >> 8));
}
static void tya16(Cpu *c) { implied(c); c->a = c->y; zn(c, c->a, false); }
static void mvn_00_7e(Cpu *c) {
  (void)fetch(c); (void)fetch(c);
  c->db = 0x00;
  uint8_t v = c->read(c->mem, ((uint32_t)0x7e << 16) | c->x);
  c->write(c->mem, ((uint32_t)0x00 << 16) | c->y, v);
  c->a--; c->x++; c->y++;
  if(c->xf) { c->x &= 255u; c->y &= 255u; }
  if(c->a != 0xffffu) c->pc = (uint16_t)(c->pc - 3u);
  idle(c); sample_interrupt(c); idle(c);
}

/* $03EF29–$EFD6: scene actor backup/restore. Gate on $1648/$01D8; clear weak
 * actor fields via $E42A; wipe $7E4D00; rebuild occupied slots via $EFA5 MVN;
 * finally MVN $7E4D00→$0B00 and restore DB. Helpers $EFA5 (slot→backup) and
 * $EFC4 (clear backup) share this region. */
static bool scene_actor_ef29_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x03 || c->e || c->xf || c->d) return false;
  /* m16 body (between REP and SEP) */
  if((pc == 0xef92 || pc == 0xef95 || pc == 0xef98 || pc == 0xef9b ||
      pc == 0xefa9 || pc == 0xefaa || pc == 0xefab || pc == 0xefae ||
      pc == 0xefaf || pc == 0xefb0 || pc == 0xefb1 || pc == 0xefb4 ||
      pc == 0xefb5 || pc == 0xefb8 || pc == 0xefc9 || pc == 0xefcc ||
      pc == 0xefd0 || pc == 0xefd1 || pc == 0xefd2) && c->mf) return false;
  /* SEP enters in m16 */
  if((pc == 0xef9e || pc == 0xefbb || pc == 0xefd4) && c->mf) return false;
  /* REP enters in m8; remaining sites are m8 */
  if((pc == 0xef90 || pc == 0xefa7 || pc == 0xefc7 ||
      pc == 0xef29 || pc == 0xef2c || pc == 0xef2e || pc == 0xef31 ||
      pc == 0xef33 || pc == 0xef35 || pc == 0xef36 || pc == 0xef39 ||
      pc == 0xef3c || pc == 0xef3e || pc == 0xef40 || pc == 0xef42 ||
      pc == 0xef44 || pc == 0xef47 || pc == 0xef4a || pc == 0xef4d ||
      pc == 0xef50 || pc == 0xef54 || pc == 0xef58 || pc == 0xef5b ||
      pc == 0xef5d || pc == 0xef61 || pc == 0xef63 || pc == 0xef66 ||
      pc == 0xef69 || pc == 0xef6c || pc == 0xef6e || pc == 0xef70 ||
      pc == 0xef72 || pc == 0xef74 || pc == 0xef76 || pc == 0xef7a ||
      pc == 0xef7e || pc == 0xef82 || pc == 0xef85 || pc == 0xef87 ||
      pc == 0xef8a || pc == 0xef8b || pc == 0xef8d ||
      pc == 0xefa0 || pc == 0xefa2 || pc == 0xefa3 || pc == 0xefa4 ||
      pc == 0xefa5 || pc == 0xefa6 || pc == 0xefbd ||
      pc == 0xefbe || pc == 0xefbf || pc == 0xefc1 || pc == 0xefc2 ||
      pc == 0xefc3 || pc == 0xefc4 || pc == 0xefd6) && !c->mf) return false;
  switch(pc) {
    case 0xef29: case 0xef2c: case 0xef2e: case 0xef31: case 0xef33: case 0xef35:
    case 0xef36: case 0xef39: case 0xef3c: case 0xef3e: case 0xef40: case 0xef42:
    case 0xef44: case 0xef47: case 0xef4a: case 0xef4d: case 0xef50: case 0xef54:
    case 0xef58: case 0xef5b: case 0xef5d: case 0xef61: case 0xef63: case 0xef66:
    case 0xef69: case 0xef6c: case 0xef6e: case 0xef70: case 0xef72: case 0xef74:
    case 0xef76: case 0xef7a: case 0xef7e: case 0xef82: case 0xef85: case 0xef87:
    case 0xef8a: case 0xef8b: case 0xef8d: case 0xef90: case 0xef92: case 0xef95:
    case 0xef98: case 0xef9b: case 0xef9e: case 0xefa0: case 0xefa2: case 0xefa3:
    case 0xefa4: case 0xefa5: case 0xefa6: case 0xefa7: case 0xefa9: case 0xefaa:
    case 0xefab: case 0xefae: case 0xefaf: case 0xefb0: case 0xefb1: case 0xefb4:
    case 0xefb5: case 0xefb8: case 0xefbb: case 0xefbd: case 0xefbe: case 0xefbf:
    case 0xefc1: case 0xefc2: case 0xefc3: case 0xefc4: case 0xefc7: case 0xefc9:
    case 0xefcc: case 0xefd0: case 0xefd1: case 0xefd2: case 0xefd4: case 0xefd6:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xef29: video_load(c, 0x1648, false); break;
    case 0xef2c: branch_to(c, !c->n, 0xef36); break;
    case 0xef2e: video_load(c, 0x01d8, false); break;
    case 0xef31: cmp_constant8(c, 0x04); break;
    case 0xef33: branch_to(c, !c->z, 0xef36); break;
    case 0xef35: return_long(c); break;
    case 0xef36: index_constant(c, true, 0x0000); break;
    case 0xef39: load_a_abs_y8(c, 0x0b00); break;
    case 0xef3c: branch_to(c, !c->n, 0xef54); break;
    case 0xef3e: and_constant8(c, 0x40); break;
    case 0xef40: branch_to(c, !c->z, 0xef54); break;
    case 0xef42: byte_constant(c, 0x00); break;
    case 0xef44: store_a_abs_y8(c, 0x0b18); break;
    case 0xef47: store_a_abs_y8(c, 0x0b19); break;
    case 0xef4a: store_a_abs_y8(c, 0x0b12); break;
    case 0xef4d: store_a_abs_y8(c, 0x0b13); break;
    case 0xef50: jsl_to(c, 0xe42a); break;
    case 0xef54: jsl_to(c, 0x8af1); break;
    case 0xef58: cpy16_constant(c, 0x0120); break;
    case 0xef5b: branch_to(c, !c->c, 0xef39); break;
    case 0xef5d: jsl_to(c, 0xefc4); break;
    case 0xef61: stz_dp8(c, 0x10); break;
    case 0xef63: index_constant(c, false, 0x0000); break;
    case 0xef66: index_constant(c, true, 0x0000); break;
    case 0xef69: load_a_abs_x8(c, 0x0b00); break;
    case 0xef6c: branch_to(c, !c->n, 0xef76); break;
    case 0xef6e: and_constant8(c, 0x40); break;
    case 0xef70: branch_to(c, !c->z, 0xef76); break;
    case 0xef72: inc_dp8(c, 0x10); break;
    case 0xef74: branch_to(c, true, 0xef7e); break;
    case 0xef76: jsl_to(c, 0xefa5); break;
    case 0xef7a: jsl_to(c, 0x8af1); break;
    case 0xef7e: jsl_to(c, 0x8afc); break;
    case 0xef82: cpx16_constant(c, 0x0120); break;
    case 0xef85: branch_to(c, !c->c, 0xef69); break;
    case 0xef87: video_load(c, 0x01bb, false); break;
    case 0xef8a: implied(c); c->c = true; break;
    case 0xef8b: sbc_dp8(c, 0x10); break;
    case 0xef8d: store_register8(c, 0x01bb); break;
    case 0xef90: set_m(c, false); break;
    case 0xef92: lda16_constant(c, 0x011f); break;
    case 0xef95: index_constant(c, true, 0x0b00); break;
    case 0xef98: index_constant(c, false, 0x4d00); break;
    case 0xef9b: mvn_00_7e(c); break;
    case 0xef9e: set_m(c, true); break;
    case 0xefa0: byte_constant(c, 0x00); break;
    case 0xefa2: pha8(c); break;
    case 0xefa3: plb_native(c); break;
    case 0xefa4: return_long(c); break;
    case 0xefa5: phx16(c); break;
    case 0xefa6: phy16(c); break;
    case 0xefa7: set_m(c, false); break;
    case 0xefa9: tya16(c); break;
    case 0xefaa: implied(c); c->c = false; break;
    case 0xefab: adc16_constant(c, 0x4d00); break;
    case 0xefae: tay16(c); break;
    case 0xefaf: txa16(c); break;
    case 0xefb0: implied(c); c->c = false; break;
    case 0xefb1: adc16_constant(c, 0x0b00); break;
    case 0xefb4: tax16(c); break;
    case 0xefb5: lda16_constant(c, 0x001f); break;
    case 0xefb8: mvn_7e_00(c); break;
    case 0xefbb: set_m(c, true); break;
    case 0xefbd: ply16(c); break;
    case 0xefbe: plx16(c); break;
    case 0xefbf: byte_constant(c, 0x00); break;
    case 0xefc1: pha8(c); break;
    case 0xefc2: plb_native(c); break;
    case 0xefc3: return_long(c); break;
    case 0xefc4: index_constant(c, false, 0x011e); break;
    case 0xefc7: set_m(c, false); break;
    case 0xefc9: lda16_constant(c, 0x0000); break;
    case 0xefcc: store_a_long_x16(c, 0x7e4d00); break;
    case 0xefd0: implied(c); c->x--; zn(c, c->x, false); break;
    case 0xefd1: implied(c); c->x--; zn(c, c->x, false); break;
    case 0xefd2: branch_to(c, !c->n, 0xefcc); break;
    case 0xefd4: set_m(c, true); break;
    case 0xefd6: return_long(c); break;
  }
  return true;
}

/* $03E419–$E460: walk actor slots calling $E42A (MVN slot→$7E2800+type<<5)
 * then $8AF1; $E42A is the shared leaf also used by $EF29. */
static bool actor_backup_e419_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x03 || c->e || c->xf || c->d) return false;
  if((pc == 0xe43b || pc == 0xe43c || pc == 0xe43d || pc == 0xe43e ||
      pc == 0xe43f || pc == 0xe440 || pc == 0xe442 || pc == 0xe444 ||
      pc == 0xe447 || pc == 0xe448 || pc == 0xe44a || pc == 0xe44b ||
      pc == 0xe44e || pc == 0xe44f || pc == 0xe451 || pc == 0xe452 ||
      pc == 0xe455) && c->mf) return false;
  if((pc == 0xe458) && c->mf) return false; /* SEP enters m16 */
  if((pc == 0xe439 || /* REP enters m8 */
      pc == 0xe419 || pc == 0xe41c || pc == 0xe420 || pc == 0xe424 ||
      pc == 0xe427 || pc == 0xe429 || pc == 0xe42a || pc == 0xe42b ||
      pc == 0xe42c || pc == 0xe42f || pc == 0xe431 || pc == 0xe433 ||
      pc == 0xe435 || pc == 0xe436 || pc == 0xe45a ||
      pc == 0xe45c || pc == 0xe45d || pc == 0xe45e || pc == 0xe45f ||
      pc == 0xe460) && !c->mf) return false;
  switch(pc) {
    case 0xe419: case 0xe41c: case 0xe420: case 0xe424: case 0xe427: case 0xe429:
    case 0xe42a: case 0xe42b: case 0xe42c: case 0xe42f: case 0xe431: case 0xe433:
    case 0xe435: case 0xe436: case 0xe439: case 0xe43b: case 0xe43c: case 0xe43d:
    case 0xe43e: case 0xe43f: case 0xe440: case 0xe442: case 0xe444: case 0xe447:
    case 0xe448: case 0xe44a: case 0xe44b: case 0xe44e: case 0xe44f: case 0xe451:
    case 0xe452: case 0xe455: case 0xe458: case 0xe45a: case 0xe45c: case 0xe45d:
    case 0xe45e: case 0xe45f: case 0xe460:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe419: index_constant(c, true, 0x0000); break;
    case 0xe41c: jsl_to(c, 0xe42a); break;
    case 0xe420: jsl_to(c, 0x8af1); break;
    case 0xe424: cpy16_constant(c, 0x0120); break;
    case 0xe427: branch_to(c, !c->c, 0xe41c); break;
    case 0xe429: return_long(c); break;
    case 0xe42a: phy16(c); break;
    case 0xe42b: phx16(c); break;
    case 0xe42c: load_a_abs_y8(c, 0x0b00); break;
    case 0xe42f: and_constant8(c, 0xc0); break;
    case 0xe431: branch_to(c, c->z, 0xe45e); break;
    case 0xe433: byte_constant(c, 0x00); break;
    case 0xe435: xba_native(c); break;
    case 0xe436: load_a_abs_y8(c, 0x0b1c); break;
    case 0xe439: set_m(c, false); break;
    case 0xe43b: case 0xe43c: case 0xe43d: case 0xe43e: case 0xe43f: asl_a16(c); break;
    case 0xe440: store_dp(c, 0x00, false); break;
    case 0xe442: sty_dp16(c, 0x02); break;
    case 0xe444: lda16_constant(c, 0x2800); break;
    case 0xe447: implied(c); c->c = false; break;
    case 0xe448: adc_dp16(c, 0x00); break;
    case 0xe44a: tay16(c); break;
    case 0xe44b: lda16_constant(c, 0x0b00); break;
    case 0xe44e: implied(c); c->c = false; break;
    case 0xe44f: adc_dp16(c, 0x02); break;
    case 0xe451: tax16(c); break;
    case 0xe452: lda16_constant(c, 0x001f); break;
    case 0xe455: mvn_7e_00(c); break;
    case 0xe458: set_m(c, true); break;
    case 0xe45a: byte_constant(c, 0x00); break;
    case 0xe45c: pha8(c); break;
    case 0xe45d: plb_native(c); break;
    case 0xe45e: plx16(c); break;
    case 0xe45f: ply16(c); break;
    case 0xe460: return_long(c); break;
  }
  return true;
}

/* $03E461–$E48F: restore one actor slot from $7E2800+type<<5 via MVN $00,$7E.
 * Callee of $E4F4 (A holds type byte on entry). */
static bool actor_restore_e461_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x03 || c->e || c->xf || c->d) return false;
  if((pc == 0xe46c || pc == 0xe46d || pc == 0xe46e || pc == 0xe46f ||
      pc == 0xe470 || pc == 0xe471 || pc == 0xe473 || pc == 0xe476 ||
      pc == 0xe477 || pc == 0xe479 || pc == 0xe47a || pc == 0xe47d ||
      pc == 0xe47e || pc == 0xe480 || pc == 0xe481 || pc == 0xe484) && c->mf)
    return false;
  if((pc == 0xe487) && c->mf) return false; /* SEP enters m16 */
  if((pc == 0xe46a || /* REP enters m8 */
      pc == 0xe461 || pc == 0xe462 || pc == 0xe463 || pc == 0xe465 ||
      pc == 0xe466 || pc == 0xe468 || pc == 0xe469 ||
      pc == 0xe489 || pc == 0xe48b || pc == 0xe48c || pc == 0xe48d ||
      pc == 0xe48e || pc == 0xe48f) && !c->mf) return false;
  switch(pc) {
    case 0xe461: case 0xe462: case 0xe463: case 0xe465: case 0xe466: case 0xe468:
    case 0xe469: case 0xe46a: case 0xe46c: case 0xe46d: case 0xe46e: case 0xe46f:
    case 0xe470: case 0xe471: case 0xe473: case 0xe476: case 0xe477: case 0xe479:
    case 0xe47a: case 0xe47d: case 0xe47e: case 0xe480: case 0xe481: case 0xe484:
    case 0xe487: case 0xe489: case 0xe48b: case 0xe48c: case 0xe48d: case 0xe48e:
    case 0xe48f:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe461: phy16(c); break;
    case 0xe462: phx16(c); break;
    case 0xe463: sty_dp16(c, 0x02); break;
    case 0xe465: pha8(c); break;
    case 0xe466: byte_constant(c, 0x00); break;
    case 0xe468: xba_native(c); break;
    case 0xe469: pla8(c); break;
    case 0xe46a: set_m(c, false); break;
    case 0xe46c: case 0xe46d: case 0xe46e: case 0xe46f: case 0xe470: asl_a16(c); break;
    case 0xe471: store_dp(c, 0x00, false); break;
    case 0xe473: lda16_constant(c, 0x0b00); break;
    case 0xe476: implied(c); c->c = false; break;
    case 0xe477: adc_dp16(c, 0x02); break;
    case 0xe479: tay16(c); break;
    case 0xe47a: lda16_constant(c, 0x2800); break;
    case 0xe47d: implied(c); c->c = false; break;
    case 0xe47e: adc_dp16(c, 0x00); break;
    case 0xe480: tax16(c); break;
    case 0xe481: lda16_constant(c, 0x001f); break;
    case 0xe484: mvn_00_7e(c); break;
    case 0xe487: set_m(c, true); break;
    case 0xe489: byte_constant(c, 0x00); break;
    case 0xe48b: pha8(c); break;
    case 0xe48c: plb_native(c); break;
    case 0xe48d: plx16(c); break;
    case 0xe48e: ply16(c); break;
    case 0xe48f: return_long(c); break;
  }
  return true;
}

/* $03E4D1–$E513: collect $0B1C types for actors with $0B00>=$C0 into $0190
 * (terminated by $FF); $E4F4 restores each via $E461 and counts into DP+$10. */
static bool actor_type_list_e4d1_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x03 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xe4d1: case 0xe4d4: case 0xe4d7: case 0xe4da: case 0xe4dc: case 0xe4de:
    case 0xe4e1: case 0xe4e4: case 0xe4e5: case 0xe4e9: case 0xe4ec: case 0xe4ee:
    case 0xe4f0: case 0xe4f3: case 0xe4f4: case 0xe4f6: case 0xe4f9: case 0xe4fc:
    case 0xe4ff: case 0xe501: case 0xe503: case 0xe507: case 0xe509: case 0xe50a:
    case 0xe50e: case 0xe511: case 0xe513:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe4d1: index_constant(c, false, 0x0000); break;
    case 0xe4d4: index_constant(c, true, 0x0000); break;
    case 0xe4d7: load_a_abs_y8(c, 0x0b00); break;
    case 0xe4da: cmp_constant8(c, 0xc0); break;
    case 0xe4dc: branch_to(c, !c->c, 0xe4e5); break;
    case 0xe4de: load_a_abs_y8(c, 0x0b1c); break;
    case 0xe4e1: store_a_abs_x8(c, 0x0190); break;
    case 0xe4e4: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xe4e5: jsl_to(c, 0x8af1); break;
    case 0xe4e9: cpy16_constant(c, 0x0120); break;
    case 0xe4ec: branch_to(c, !c->c, 0xe4d7); break;
    case 0xe4ee: byte_constant(c, 0xff); break;
    case 0xe4f0: store_a_abs_x8(c, 0x0190); break;
    case 0xe4f3: return_long(c); break;
    case 0xe4f4: stz_dp8(c, 0x10); break;
    case 0xe4f6: index_constant(c, false, 0x0000); break;
    case 0xe4f9: index_constant(c, true, 0x0000); break;
    case 0xe4fc: load_a_abs_x8(c, 0x0190); break;
    case 0xe4ff: cmp_constant8(c, 0xff); break;
    case 0xe501: branch_to(c, c->z, 0xe513); break;
    case 0xe503: jsl_to(c, 0xe461); break;
    case 0xe507: inc_dp8(c, 0x10); break;
    case 0xe509: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xe50a: jsl_to(c, 0x8af1); break;
    case 0xe50e: cpy16_constant(c, 0x0120); break;
    case 0xe511: branch_to(c, !c->c, 0xe4fc); break;
    case 0xe513: return_long(c); break;
  }
  return true;
}

/* $06E942–$E968: conditional scene-wipe gate — when $0D9B bit6 set and map/
 * actor predicates hold, JSL $03BB46; otherwise RTL immediately. */
static bool wipe_gate_e942_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x06 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xe942: case 0xe945: case 0xe947: case 0xe949: case 0xe94c: case 0xe94f:
    case 0xe951: case 0xe953: case 0xe956: case 0xe958: case 0xe95a: case 0xe95d:
    case 0xe95f: case 0xe961: case 0xe964: case 0xe968:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe942: video_load(c, 0x0d9b, false); break;
    case 0xe945: and_constant8(c, 0x40); break;
    case 0xe947: branch_to(c, c->z, 0xe968); break;
    case 0xe949: stz_abs8(c, 0x0d9b); break;
    case 0xe94c: video_load(c, 0x01a0, false); break;
    case 0xe94f: cmp_constant8(c, 0x09); break;
    case 0xe951: branch_to(c, !c->z, 0xe968); break;
    case 0xe953: video_load(c, 0x0170, false); break;
    case 0xe956: and_constant8(c, 0x08); break;
    case 0xe958: branch_to(c, c->z, 0xe968); break;
    case 0xe95a: video_load(c, 0x01bb, false); break;
    case 0xe95d: cmp_constant8(c, 0x08); break;
    case 0xe95f: branch_to(c, !c->c, 0xe968); break;
    case 0xe961: index_constant(c, true, 0x0100); break;
    case 0xe964: jsl_to(c, 0xbb46); break;
    case 0xe968: return_long(c); break;
  }
  return true;
}

/* $06E8EE–$E941: actor attr fill via $4202×$0E; tables $02B21D/$02B214 into
 * $0B17/$0B1B; type $54 seeds $0B0E=$01, else [DP+$00],Y→$0B0E,X via $8A28/$8B07. */
static bool actor_attr_e8ee_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x06 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xe8ee: case 0xe8f1: case 0xe8f4: case 0xe8f6: case 0xe8f8: case 0xe8fb:
    case 0xe8fe: case 0xe900: case 0xe903: case 0xe904: case 0xe905: case 0xe906:
    case 0xe909: case 0xe90d: case 0xe910: case 0xe914: case 0xe917: case 0xe91a:
    case 0xe91c: case 0xe91e: case 0xe91f: case 0xe923: case 0xe927: case 0xe928:
    case 0xe929: case 0xe92a: case 0xe92b: case 0xe92d: case 0xe930: case 0xe931:
    case 0xe935: case 0xe938: case 0xe93a: case 0xe93b: case 0xe93d: case 0xe940:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe8ee: index_constant(c, true, 0x0000); break;
    case 0xe8f1: load_a_abs_y8(c, 0x0b00); break;
    case 0xe8f4: and_constant8(c, 0x40); break;
    case 0xe8f6: branch_to(c, c->z, 0xe931); break;
    case 0xe8f8: load_a_abs_y8(c, 0x0b01); break;
    case 0xe8fb: store_register8(c, 0x4202); break;
    case 0xe8fe: byte_constant(c, 0x0e); break;
    case 0xe900: store_register8(c, 0x4203); break;
    case 0xe903: case 0xe904: xba_native(c); break;
    case 0xe905: nop_native(c); break;
    case 0xe906: ldx_abs16(c, 0x4216); break;
    case 0xe909: load_a_long_x8(c, 0x02b21d); break;
    case 0xe90d: store_a_abs_y8(c, 0x0b17); break;
    case 0xe910: load_a_long_x8(c, 0x02b214); break;
    case 0xe914: store_a_abs_y8(c, 0x0b1b); break;
    case 0xe917: load_a_abs_y8(c, 0x0b01); break;
    case 0xe91a: cmp_constant8(c, 0x54); break;
    case 0xe91c: branch_to(c, c->z, 0xe93b); break;
    case 0xe91e: phy16(c); break;
    case 0xe91f: jsl_to(c, 0x8a28); break;
    case 0xe923: jsl_to(c, 0x8b07); break;
    case 0xe927: case 0xe928: case 0xe929: case 0xe92a:
      implied(c); c->y++; zn(c, c->y, false); break;
    case 0xe92b: lda_indirect_long_y8(c, 0x00); break;
    case 0xe92d: store_a_abs_x8(c, 0x0b0e); break;
    case 0xe930: ply16(c); break;
    case 0xe931: jsl_to(c, 0x8af1); break;
    case 0xe935: cpy16_constant(c, 0x0120); break;
    case 0xe938: branch_to(c, !c->c, 0xe8f1); break;
    case 0xe93a: return_long(c); break;
    case 0xe93b: byte_constant(c, 0x01); break;
    case 0xe93d: store_a_abs_y8(c, 0x0b0e); break;
    case 0xe940: branch_to(c, true, 0xe931); break;
  }
  return true;
}

/* $03BB46–$BB86: seed wipe-actor slot at Y ($0B00=$C0, type $54, coords). */
static bool wipe_actor_bb46_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x03 || c->e || c->xf || c->d) return false;
  if((pc == 0xbb61 || pc == 0xbb64 || pc == 0xbb67 || pc == 0xbb6a ||
      pc == 0xbb6d || pc == 0xbb70 || pc == 0xbb73 || pc == 0xbb76 ||
      pc == 0xbb79) && c->mf) return false;
  if(pc == 0xbb7c && c->mf) return false; /* SEP enters m16 */
  if((pc == 0xbb5f || /* REP enters m8 */
      pc == 0xbb46 || pc == 0xbb48 || pc == 0xbb4b || pc == 0xbb4d ||
      pc == 0xbb50 || pc == 0xbb52 || pc == 0xbb55 || pc == 0xbb57 ||
      pc == 0xbb5a || pc == 0xbb5c || pc == 0xbb7e || pc == 0xbb80 ||
      pc == 0xbb83 || pc == 0xbb86) && !c->mf) return false;
  switch(pc) {
    case 0xbb46: case 0xbb48: case 0xbb4b: case 0xbb4d: case 0xbb50: case 0xbb52:
    case 0xbb55: case 0xbb57: case 0xbb5a: case 0xbb5c: case 0xbb5f: case 0xbb61:
    case 0xbb64: case 0xbb67: case 0xbb6a: case 0xbb6d: case 0xbb70: case 0xbb73:
    case 0xbb76: case 0xbb79: case 0xbb7c: case 0xbb7e: case 0xbb80: case 0xbb83:
    case 0xbb86:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xbb46: byte_constant(c, 0xc0); break;
    case 0xbb48: store_a_abs_y8(c, 0x0b00); break;
    case 0xbb4b: byte_constant(c, 0x01); break;
    case 0xbb4d: store_a_abs_y8(c, 0x0b0e); break;
    case 0xbb50: byte_constant(c, 0x0c); break;
    case 0xbb52: store_a_abs_y8(c, 0x0b1c); break;
    case 0xbb55: byte_constant(c, 0x54); break;
    case 0xbb57: store_a_abs_y8(c, 0x0b01); break;
    case 0xbb5a: byte_constant(c, 0x00); break;
    case 0xbb5c: store_a_abs_y8(c, 0x0b02); break;
    case 0xbb5f: set_m(c, false); break;
    case 0xbb61: lda16_constant(c, 0x0033); break;
    case 0xbb64: store_a_abs_y16(c, 0x0b03); break;
    case 0xbb67: store_a_abs_y16(c, 0x0b0a); break;
    case 0xbb6a: lda16_constant(c, 0x0006); break;
    case 0xbb6d: store_a_abs_y16(c, 0x0b05); break;
    case 0xbb70: store_a_abs_y16(c, 0x0b0c); break;
    case 0xbb73: lda16_constant(c, 0x0000); break;
    case 0xbb76: store_a_abs_y16(c, 0x0b07); break;
    case 0xbb79: store_a_abs_y16(c, 0x0b0f); break;
    case 0xbb7c: set_m(c, true); break;
    case 0xbb7e: byte_constant(c, 0x00); break;
    case 0xbb80: store_a_abs_y8(c, 0x0b09); break;
    case 0xbb83: store_a_abs_y8(c, 0x0b11); break;
    case 0xbb86: return_long(c); break;
  }
  return true;
}

/* $03EFD7–$F020: clear-siblings after $EFC4 — wipe $7E29A0 / $7E3A00 /
 * $0B00 / $0E00 word loops (X or Y from $011E/$009E). */
static bool clear_siblings_efd7_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x03 || c->e || c->xf || c->d) return false;
  if((pc == 0xefdc || pc == 0xefdf || pc == 0xefe3 || pc == 0xefe4 ||
      pc == 0xefe5 || pc == 0xefef || pc == 0xeff2 || pc == 0xeff6 ||
      pc == 0xeff7 || pc == 0xeff8 || pc == 0xf002 || pc == 0xf005 ||
      pc == 0xf008 || pc == 0xf009 || pc == 0xf00a || pc == 0xf014 ||
      pc == 0xf017 || pc == 0xf01a || pc == 0xf01b || pc == 0xf01c) && c->mf)
    return false;
  if((pc == 0xefe7 || pc == 0xeffa || pc == 0xf00c || pc == 0xf01e) && c->mf)
    return false; /* SEP enters m16 */
  if((pc == 0xefda || pc == 0xefed || pc == 0xf000 || pc == 0xf012 ||
      pc == 0xefd7 || pc == 0xefe9 || pc == 0xefea || pc == 0xeffc ||
      pc == 0xeffd || pc == 0xf00e || pc == 0xf00f || pc == 0xf020) && !c->mf)
    return false;
  switch(pc) {
    case 0xefd7: case 0xefda: case 0xefdc: case 0xefdf: case 0xefe3: case 0xefe4:
    case 0xefe5: case 0xefe7: case 0xefe9: case 0xefea: case 0xefed: case 0xefef:
    case 0xeff2: case 0xeff6: case 0xeff7: case 0xeff8: case 0xeffa: case 0xeffc:
    case 0xeffd: case 0xf000: case 0xf002: case 0xf005: case 0xf008: case 0xf009:
    case 0xf00a: case 0xf00c: case 0xf00e: case 0xf00f: case 0xf012: case 0xf014:
    case 0xf017: case 0xf01a: case 0xf01b: case 0xf01c: case 0xf01e: case 0xf020:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xefd7: index_constant(c, false, 0x011e); break;
    case 0xefda: set_m(c, false); break;
    case 0xefdc: lda16_constant(c, 0x0000); break;
    case 0xefdf: store_a_long_x16(c, 0x7e29a0); break;
    case 0xefe3: implied(c); c->x--; zn(c, c->x, false); break;
    case 0xefe4: implied(c); c->x--; zn(c, c->x, false); break;
    case 0xefe5: branch_to(c, !c->n, 0xefdf); break;
    case 0xefe7: set_m(c, true); break;
    case 0xefe9: return_long(c); break;
    case 0xefea: index_constant(c, false, 0x011e); break;
    case 0xefed: set_m(c, false); break;
    case 0xefef: lda16_constant(c, 0x0000); break;
    case 0xeff2: store_a_long_x16(c, 0x7e3a00); break;
    case 0xeff6: implied(c); c->x--; zn(c, c->x, false); break;
    case 0xeff7: implied(c); c->x--; zn(c, c->x, false); break;
    case 0xeff8: branch_to(c, !c->n, 0xeff2); break;
    case 0xeffa: set_m(c, true); break;
    case 0xeffc: return_long(c); break;
    case 0xeffd: index_constant(c, true, 0x011e); break;
    case 0xf000: set_m(c, false); break;
    case 0xf002: lda16_constant(c, 0x0000); break;
    case 0xf005: store_a_abs_y16(c, 0x0b00); break;
    case 0xf008: implied(c); c->y--; zn(c, c->y, false); break;
    case 0xf009: implied(c); c->y--; zn(c, c->y, false); break;
    case 0xf00a: branch_to(c, !c->n, 0xf005); break;
    case 0xf00c: set_m(c, true); break;
    case 0xf00e: return_long(c); break;
    case 0xf00f: index_constant(c, true, 0x009e); break;
    case 0xf012: set_m(c, false); break;
    case 0xf014: lda16_constant(c, 0x0000); break;
    case 0xf017: store_a_abs_y16(c, 0x0e00); break;
    case 0xf01a: implied(c); c->y--; zn(c, c->y, false); break;
    case 0xf01b: implied(c); c->y--; zn(c, c->y, false); break;
    case 0xf01c: branch_to(c, !c->n, 0xf017); break;
    case 0xf01e: set_m(c, true); break;
    case 0xf020: return_long(c); break;
  }
  return true;
}

/* $03E32E–$E33F: swap $01BC↔$018F via DP+$00, then SEC/RTL (E36B epilogue). */
static bool swap_epilogue_e32e_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x03 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xe32e: case 0xe331: case 0xe333: case 0xe336: case 0xe339: case 0xe33b:
    case 0xe33e: case 0xe33f:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe32e: video_load(c, 0x01bc, false); break;
    case 0xe331: store_dp(c, 0x00, true); break;
    case 0xe333: video_load(c, 0x018f, false); break;
    case 0xe336: store_register8(c, 0x01bc); break;
    case 0xe339: load_dp8(c, 0x00); break;
    case 0xe33b: store_register8(c, 0x018f); break;
    case 0xe33e: implied(c); c->c = true; break;
    case 0xe33f: return_long(c); break;
  }
  return true;
}

/* $03E36B–$E418: post-$E340 MVN copies $0B00↔$7E3A00/$7E29A0 and $0E00←$7E2B00,
 * then long-abs state restore; JMP $E32E. */
static bool mvn_restore_e36b_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x03 || c->e || c->xf || c->d) return false;
  if((pc == 0xe36d || pc == 0xe370 || pc == 0xe373 || pc == 0xe376 ||
      pc == 0xe37d || pc == 0xe380 || pc == 0xe383 || pc == 0xe386 ||
      pc == 0xe389 || pc == 0xe38c || pc == 0xe38f || pc == 0xe392 ||
      pc == 0xe3c0 || pc == 0xe3c3 || pc == 0xe3c6 || pc == 0xe3ca ||
      pc == 0xe3cd || pc == 0xe3d1 || pc == 0xe3d4 || pc == 0xe3d8 ||
      pc == 0xe3db || pc == 0xe3df || pc == 0xe3e3 || pc == 0xe3e6 ||
      pc == 0xe3ea || pc == 0xe3ed || pc == 0xe3f1 || pc == 0xe3f4 ||
      pc == 0xe3f8 || pc == 0xe3fb || pc == 0xe3ff) && c->mf) return false;
  if((pc == 0xe379 || pc == 0xe395 || pc == 0xe402) && c->mf) return false; /* SEP */
  if((pc == 0xe36b || pc == 0xe37b || pc == 0xe3be || /* REP */
      pc == 0xe397 || pc == 0xe399 || pc == 0xe39a || pc == 0xe39b ||
      pc == 0xe39e || pc == 0xe3a2 || pc == 0xe3a6 || pc == 0xe3a9 ||
      pc == 0xe3ad || pc == 0xe3b0 || pc == 0xe3b4 || pc == 0xe3b7 ||
      pc == 0xe3bb || pc == 0xe404 || pc == 0xe406 || pc == 0xe409 ||
      pc == 0xe40b || pc == 0xe40e || pc == 0xe411 || pc == 0xe413 ||
      pc == 0xe416) && !c->mf) return false;
  switch(pc) {
    case 0xe36b: case 0xe36d: case 0xe370: case 0xe373: case 0xe376: case 0xe379:
    case 0xe37b: case 0xe37d: case 0xe380: case 0xe383: case 0xe386: case 0xe389:
    case 0xe38c: case 0xe38f: case 0xe392: case 0xe395: case 0xe397: case 0xe399:
    case 0xe39a: case 0xe39b: case 0xe39e: case 0xe3a2: case 0xe3a6: case 0xe3a9:
    case 0xe3ad: case 0xe3b0: case 0xe3b4: case 0xe3b7: case 0xe3bb: case 0xe3be:
    case 0xe3c0: case 0xe3c3: case 0xe3c6: case 0xe3ca: case 0xe3cd: case 0xe3d1:
    case 0xe3d4: case 0xe3d8: case 0xe3db: case 0xe3df: case 0xe3e3: case 0xe3e6:
    case 0xe3ea: case 0xe3ed: case 0xe3f1: case 0xe3f4: case 0xe3f8: case 0xe3fb:
    case 0xe3ff: case 0xe402: case 0xe404: case 0xe406: case 0xe409: case 0xe40b:
    case 0xe40e: case 0xe411: case 0xe413: case 0xe416:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe36b: set_m(c, false); break;
    case 0xe36d: lda16_constant(c, 0x011f); break;
    case 0xe370: index_constant(c, true, 0x3a00); break;
    case 0xe373: index_constant(c, false, 0x0b00); break;
    case 0xe376: mvn_7e_00(c); break;
    case 0xe379: set_m(c, true); break;
    case 0xe37b: set_m(c, false); break;
    case 0xe37d: lda16_constant(c, 0x011f); break;
    case 0xe380: index_constant(c, true, 0x0b00); break;
    case 0xe383: index_constant(c, false, 0x29a0); break;
    case 0xe386: mvn_00_7e(c); break;
    case 0xe389: lda16_constant(c, 0x009f); break;
    case 0xe38c: index_constant(c, true, 0x0e00); break;
    case 0xe38f: index_constant(c, false, 0x2b00); break;
    case 0xe392: mvn_00_7e(c); break;
    case 0xe395: set_m(c, true); break;
    case 0xe397: byte_constant(c, 0x00); break;
    case 0xe399: pha8(c); break;
    case 0xe39a: plb_native(c); break;
    case 0xe39b: video_load(c, 0x01bb, false); break;
    case 0xe39e: sta_long8(c, 0x7e3b20); break;
    case 0xe3a2: lda_long8(c, 0x7e2ac0); break;
    case 0xe3a6: store_register8(c, 0x01bb); break;
    case 0xe3a9: lda_long8(c, 0x7e2ae0); break;
    case 0xe3ad: store_register8(c, 0x1648); break;
    case 0xe3b0: lda_long8(c, 0x7e2aec); break;
    case 0xe3b4: store_register8(c, 0x0c36); break;
    case 0xe3b7: lda_long8(c, 0x7e2aed); break;
    case 0xe3bb: store_register8(c, 0x0c37); break;
    case 0xe3be: set_m(c, false); break;
    case 0xe3c0: stz_abs16(c, 0x166c); break;
    case 0xe3c3: video_load(c, 0x01a2, false); break;
    case 0xe3c6: sta_long16(c, 0x7e2af0); break;
    case 0xe3ca: video_load(c, 0x01a4, false); break;
    case 0xe3cd: sta_long16(c, 0x7e2af2); break;
    case 0xe3d1: video_load(c, 0x01aa, false); break;
    case 0xe3d4: sta_long16(c, 0x7e2af4); break;
    case 0xe3d8: video_load(c, 0x01ac, false); break;
    case 0xe3db: sta_long16(c, 0x7e2af6); break;
    case 0xe3df: lda_long16(c, 0x7e2ae2); break;
    case 0xe3e3: store_a_abs16(c, 0x01a2); break;
    case 0xe3e6: lda_long16(c, 0x7e2ae4); break;
    case 0xe3ea: store_a_abs16(c, 0x01a4); break;
    case 0xe3ed: lda_long16(c, 0x7e2ae6); break;
    case 0xe3f1: store_a_abs16(c, 0x01aa); break;
    case 0xe3f4: lda_long16(c, 0x7e2ae8); break;
    case 0xe3f8: store_a_abs16(c, 0x01ac); break;
    case 0xe3fb: lda_long16(c, 0x7e2aea); break;
    case 0xe3ff: store_a_abs16(c, 0x10c1); break;
    case 0xe402: set_m(c, true); break;
    case 0xe404: byte_constant(c, 0x49); break;
    case 0xe406: store_register8(c, 0x0c41); break;
    case 0xe409: byte_constant(c, 0x20); break;
    case 0xe40b: store_register8(c, 0x0ea2); break;
    case 0xe40e: video_load(c, 0x01d7, false); break;
    case 0xe411: and_constant8(c, 0xbf); break;
    case 0xe413: store_register8(c, 0x01d7); break;
    case 0xe416: jmp_abs_native(c, 0xe32e); break;
  }
  return true;
}

/* $008DAC–$8DE3: walk actor slots Y=0..$100 step $20. For each: type at $0B01,Y
 * → ×3 into X; load word $008DE4[Y>>4] into DP+$00; JSL $01C9F9 / $8324 / $8AF1. */
static bool actor_slot_walk_8dac_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  /* m16 body between REP #$20 and SEP #$20 */
  const bool m16 = (pc == 0x8dbb || pc == 0x8dbc || pc == 0x8dbd || pc == 0x8dbf ||
                    pc == 0x8dc0 || pc == 0x8dc1 || pc == 0x8dc2 || pc == 0x8dc3 ||
                    pc == 0x8dc4 || pc == 0x8dc5 || pc == 0x8dc6 || pc == 0x8dc7 ||
                    pc == 0x8dcb || pc == 0x8dcd);
  const bool sep_from_m16 = (pc == 0x8dce);
  const bool rep_to_m16 = (pc == 0x8db9);
  if(m16 && c->mf) return false;
  if(sep_from_m16 && c->mf) return false;
  if(rep_to_m16 && !c->mf) return false;
  if(!m16 && !sep_from_m16 && !rep_to_m16 && !c->mf) return false;
  switch(pc) {
    case 0x8dac: case 0x8daf: case 0x8db1: case 0x8db2: case 0x8db5: case 0x8db7:
    case 0x8db9: case 0x8dbb: case 0x8dbc: case 0x8dbd: case 0x8dbf: case 0x8dc0:
    case 0x8dc1: case 0x8dc2: case 0x8dc3: case 0x8dc4: case 0x8dc5: case 0x8dc6:
    case 0x8dc7: case 0x8dcb: case 0x8dcd: case 0x8dce: case 0x8dd0: case 0x8dd1:
    case 0x8dd5: case 0x8dd9: case 0x8dda: case 0x8dde: case 0x8de1: case 0x8de3:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8dac: index_constant(c, true, 0x0000); break;
    case 0x8daf: byte_constant(c, 0x00); break;
    case 0x8db1: xba_native(c); break;
    case 0x8db2: load_a_abs_y8(c, 0x0b01); break;
    case 0x8db5: store_dp(c, 0x12, true); break;
    case 0x8db7: stz_dp8(c, 0x13); break;
    case 0x8db9: sep_rep_native(c, 0x20, false); break;
    case 0x8dbb: asl_a16(c); break;
    case 0x8dbc: implied(c); c->c = false; break;
    case 0x8dbd: adc_dp16(c, 0x12); break;
    case 0x8dbf: tax16(c); break;
    case 0x8dc0: phx16(c); break;
    case 0x8dc1: tya16(c); break;
    case 0x8dc2: case 0x8dc3: case 0x8dc4: case 0x8dc5: lsr_a16(c); break;
    case 0x8dc6: tax16(c); break;
    case 0x8dc7: lda_long_x16(c, 0x008de4); break;
    case 0x8dcb: store_dp(c, 0x00, false); break;
    case 0x8dcd: plx16(c); break;
    case 0x8dce: sep_rep_native(c, 0x20, true); break;
    case 0x8dd0: phy16(c); break;
    case 0x8dd1: jsl_to(c, 0xc9f9); break;
    case 0x8dd5: jsl_to(c, 0x8324); break;
    case 0x8dd9: ply16(c); break;
    case 0x8dda: jsl_to(c, 0x8af1); break;
    case 0x8dde: cpy16_constant(c, 0x0120); break;
    case 0x8de1: branch_to(c, !c->c, 0x8daf); break;
    case 0x8de3: return_long(c); break;
  }
  return true;
}

/* $06EAD4–$EB51: palette-index select from $01BC/$01E7/$1648/$01A0, then
 * resolve via $88EA and copy via $88C5 (alt base $06F876 when index==3). */
static bool palette_select_ead4_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x06 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xeb17 || pc == 0xeb1a || pc == 0xeb38 || pc == 0xeb3b);
  const bool sep_from_m16 = (pc == 0xeb1c || pc == 0xeb3d);
  const bool rep_to_m16 = (pc == 0xeb15 || pc == 0xeb36);
  if(m16 && c->mf) return false;
  if(sep_from_m16 && c->mf) return false;
  if(rep_to_m16 && !c->mf) return false;
  if(!m16 && !sep_from_m16 && !rep_to_m16 && !c->mf) return false;
  switch(pc) {
    case 0xead4: case 0xead6: case 0xead7: case 0xeada: case 0xeadc: case 0xeade:
    case 0xeae1: case 0xeae2: case 0xeae4: case 0xeae6: case 0xeae8: case 0xeaea:
    case 0xeaec: case 0xeaef: case 0xeaf1: case 0xeaf3: case 0xeaf5: case 0xeaf7:
    case 0xeafa: case 0xeafc: case 0xeaff: case 0xeb01: case 0xeb04: case 0xeb05:
    case 0xeb09: case 0xeb0b: case 0xeb0d: case 0xeb0e: case 0xeb0f: case 0xeb13:
    case 0xeb15: case 0xeb17: case 0xeb1a: case 0xeb1c: case 0xeb1e: case 0xeb20:
    case 0xeb22: case 0xeb26: case 0xeb29: case 0xeb2c: case 0xeb30: case 0xeb31:
    case 0xeb34: case 0xeb36: case 0xeb38: case 0xeb3b: case 0xeb3d: case 0xeb3f:
    case 0xeb41: case 0xeb43: case 0xeb47: case 0xeb4a: case 0xeb4d: case 0xeb51:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xead4: byte_constant(c, 0x00); break;
    case 0xead6: xba_native(c); break;
    case 0xead7: video_load(c, 0x01bc, false); break;
    case 0xeada: and_constant8(c, 0x0f); break;
    case 0xeadc: branch_to(c, c->z, 0xeaec); break;
    case 0xeade: video_load(c, 0x01e7, false); break;
    case 0xeae1: lsr_a8(c); break;
    case 0xeae2: branch_to(c, !c->c, 0xeae8); break;
    case 0xeae4: byte_constant(c, 0x05); break;
    case 0xeae6: branch_to(c, true, 0xeb09); break;
    case 0xeae8: byte_constant(c, 0x02); break;
    case 0xeaea: branch_to(c, true, 0xeb09); break;
    case 0xeaec: video_load(c, 0x01e7, false); break;
    case 0xeaef: and_constant8(c, 0x10); break;
    case 0xeaf1: branch_to(c, c->z, 0xeaf7); break;
    case 0xeaf3: byte_constant(c, 0x06); break;
    case 0xeaf5: branch_to(c, true, 0xeb09); break;
    case 0xeaf7: video_load(c, 0x1648, false); break;
    case 0xeafa: branch_to(c, !c->n, 0xeb01); break;
    case 0xeafc: video_load(c, 0x164b, false); break;
    case 0xeaff: branch_to(c, true, 0xeb09); break;
    case 0xeb01: video_load(c, 0x01a0, false); break;
    case 0xeb04: tax16_from_a8(c); break;
    case 0xeb05: load_a_long_x8(c, 0x02e44b); break;
    case 0xeb09: cmp_constant8(c, 0x03); break;
    case 0xeb0b: branch_to(c, c->z, 0xeb31); break;
    case 0xeb0d: asl_a8(c); break;
    case 0xeb0e: tax16_from_a8(c); break;
    case 0xeb0f: load_a_long_x8(c, 0x02e47b); break;
    case 0xeb13: store_dp(c, 0x03, true); break;
    case 0xeb15: sep_rep_native(c, 0x20, false); break;
    case 0xeb17: lda16_constant(c, 0xe4aa); break;
    case 0xeb1a: store_dp(c, 0x06, false); break;
    case 0xeb1c: sep_rep_native(c, 0x20, true); break;
    case 0xeb1e: byte_constant(c, 0x02); break;
    case 0xeb20: store_dp(c, 0x08, true); break;
    case 0xeb22: jsl_to(c, 0x88ea); break;
    case 0xeb26: index_constant(c, false, 0x00fe); break;
    case 0xeb29: index_constant(c, true, 0x007e); break;
    case 0xeb2c: jsl_to(c, 0x88c5); break;
    case 0xeb30: return_long(c); break;
    case 0xeb31: video_load(c, 0x01a0, false); break;
    case 0xeb34: store_dp(c, 0x03, true); break;
    case 0xeb36: sep_rep_native(c, 0x20, false); break;
    case 0xeb38: lda16_constant(c, 0xf876); break;
    case 0xeb3b: store_dp(c, 0x06, false); break;
    case 0xeb3d: sep_rep_native(c, 0x20, true); break;
    case 0xeb3f: byte_constant(c, 0x06); break;
    case 0xeb41: store_dp(c, 0x08, true); break;
    case 0xeb43: jsl_to(c, 0x88ea); break;
    case 0xeb47: index_constant(c, false, 0x00fe); break;
    case 0xeb4a: index_constant(c, true, 0x007e); break;
    case 0xeb4d: jsl_to(c, 0x88c5); break;
    case 0xeb51: return_long(c); break;
  }
  return true;
}

/* $06EB52–$EBC0: sibling of EAD4 — same index select, then long-pointer
 * $02E47B/$02E489 into DP+$00/$83/$85, decompress via C559, DMA via 90E6. */
static bool gfx_select_eb52_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x06 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xeb8b || pc == 0xeb8f || pc == 0xeba5 || pc == 0xeba9);
  const bool sep_from_m16 = (pc == 0xeb91 || pc == 0xebab);
  const bool rep_to_m16 = (pc == 0xeb89 || pc == 0xeba3);
  if(m16 && c->mf) return false;
  if(sep_from_m16 && c->mf) return false;
  if(rep_to_m16 && !c->mf) return false;
  if(!m16 && !sep_from_m16 && !rep_to_m16 && !c->mf) return false;
  switch(pc) {
    case 0xeb52: case 0xeb54: case 0xeb55: case 0xeb58: case 0xeb5a: case 0xeb5c:
    case 0xeb5f: case 0xeb60: case 0xeb62: case 0xeb64: case 0xeb66: case 0xeb68:
    case 0xeb6a: case 0xeb6d: case 0xeb6f: case 0xeb71: case 0xeb73: case 0xeb75:
    case 0xeb78: case 0xeb7a: case 0xeb7d: case 0xeb7f: case 0xeb82: case 0xeb83:
    case 0xeb87: case 0xeb88: case 0xeb89: case 0xeb8b: case 0xeb8f: case 0xeb91:
    case 0xeb93: case 0xeb95: case 0xeb96: case 0xeb98: case 0xeb99: case 0xeb9a:
    case 0xeb9c: case 0xeb9d: case 0xeba1: case 0xeba3: case 0xeba5: case 0xeba9:
    case 0xebab: case 0xebad: case 0xebaf: case 0xebb2: case 0xebb6: case 0xebb9:
    case 0xebbc: case 0xebc0:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xeb52: byte_constant(c, 0x00); break;
    case 0xeb54: xba_native(c); break;
    case 0xeb55: video_load(c, 0x01bc, false); break;
    case 0xeb58: and_constant8(c, 0x0f); break;
    case 0xeb5a: branch_to(c, c->z, 0xeb6a); break;
    case 0xeb5c: video_load(c, 0x01e7, false); break;
    case 0xeb5f: lsr_a8(c); break;
    case 0xeb60: branch_to(c, !c->c, 0xeb66); break;
    case 0xeb62: byte_constant(c, 0x05); break;
    case 0xeb64: branch_to(c, true, 0xeb87); break;
    case 0xeb66: byte_constant(c, 0x02); break;
    case 0xeb68: branch_to(c, true, 0xeb87); break;
    case 0xeb6a: video_load(c, 0x01e7, false); break;
    case 0xeb6d: and_constant8(c, 0x10); break;
    case 0xeb6f: branch_to(c, c->z, 0xeb75); break;
    case 0xeb71: byte_constant(c, 0x06); break;
    case 0xeb73: branch_to(c, true, 0xeb87); break;
    case 0xeb75: video_load(c, 0x1648, false); break;
    case 0xeb78: branch_to(c, !c->n, 0xeb7f); break;
    case 0xeb7a: video_load(c, 0x164b, false); break;
    case 0xeb7d: branch_to(c, true, 0xeb87); break;
    case 0xeb7f: video_load(c, 0x01a0, false); break;
    case 0xeb82: tax16_from_a8(c); break;
    case 0xeb83: load_a_long_x8(c, 0x02e44b); break;
    case 0xeb87: asl_a8(c); break;
    case 0xeb88: tax16_from_a8(c); break;
    case 0xeb89: sep_rep_native(c, 0x20, false); break;
    case 0xeb8b: lda_long_x16(c, 0x02e47b); break;
    case 0xeb8f: store_dp(c, 0x00, false); break;
    case 0xeb91: sep_rep_native(c, 0x20, true); break;
    case 0xeb93: byte_constant(c, 0x00); break;
    case 0xeb95: xba_native(c); break;
    case 0xeb96: load_dp8(c, 0x01); break;
    case 0xeb98: asl_a8(c); break;
    case 0xeb99: implied(c); c->c = false; break;
    case 0xeb9a: adc8_dp(c, 0x01); break;
    case 0xeb9c: tax16_from_a8(c); break;
    case 0xeb9d: load_a_long_x8(c, 0x02e48b); break;
    case 0xeba1: store_dp(c, 0x85, true); break;
    case 0xeba3: sep_rep_native(c, 0x20, false); break;
    case 0xeba5: lda_long_x16(c, 0x02e489); break;
    case 0xeba9: store_dp(c, 0x83, false); break;
    case 0xebab: sep_rep_native(c, 0x20, true); break;
    case 0xebad: byte_constant(c, 0x80); break;
    case 0xebaf: store_register8(c, 0x2115); break;
    case 0xebb2: jsl_to(c, 0xc559); break;
    case 0xebb6: index_constant(c, true, 0x6000); break;
    case 0xebb9: store_y_abs16(c, 0x2116); break;
    case 0xebbc: jsl_to(c, 0x90e6); break;
    case 0xebc0: return_long(c); break;
  }
  return true;
}

/* $008F46–$8FB1: scene-mode body after $90C1 — pull EAD4/EB52, seed PPU/scene
 * flags, clear $0B12 stride-$20, optional $03D4BF / $07BC→$0C40. */
static bool scene_body_8f46_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x8f46: case 0x8f4a: case 0x8f4e: case 0x8f52: case 0x8f54: case 0x8f56:
    case 0x8f58: case 0x8f5a: case 0x8f5c: case 0x8f5e: case 0x8f61: case 0x8f63:
    case 0x8f65: case 0x8f67: case 0x8f6a: case 0x8f6d: case 0x8f70: case 0x8f72:
    case 0x8f74: case 0x8f76: case 0x8f79: case 0x8f7d: case 0x8f81: case 0x8f85:
    case 0x8f89: case 0x8f8c: case 0x8f8e: case 0x8f91: case 0x8f95: case 0x8f98:
    case 0x8f9a: case 0x8f9d: case 0x8f9f: case 0x8fa3: case 0x8fa6: case 0x8fa8:
    case 0x8fab: case 0x8fae: case 0x8fb1:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8f46: jsl_to(c, 0x90c1); break;
    case 0x8f4a: jsl_to(c, 0xead4); break;
    case 0x8f4e: jsl_to(c, 0xeb52); break;
    case 0x8f52: stz_dp8(c, 0x26); break;
    case 0x8f54: byte_constant(c, 0x03); break;
    case 0x8f56: store_dp(c, 0x25, true); break;
    case 0x8f58: byte_constant(c, 0x02); break;
    case 0x8f5a: store_dp(c, 0x27, true); break;
    case 0x8f5c: byte_constant(c, 0x61); break;
    case 0x8f5e: store_register8(c, 0x0716); break;
    case 0x8f61: load_dp8(c, 0x29); break;
    case 0x8f63: and_constant8(c, 0xf0); break;
    case 0x8f65: store_dp(c, 0x29, true); break;
    case 0x8f67: stz_abs8(c, 0x0d68); break;
    case 0x8f6a: video_load(c, 0x0700, false); break;
    case 0x8f6d: store_register8(c, 0x0701); break;
    case 0x8f70: byte_constant(c, 0x01); break;
    case 0x8f72: store_dp(c, 0x61, true); break;
    case 0x8f74: byte_constant(c, 0x23); break;
    case 0x8f76: store_register8(c, 0x0d66); break;
    case 0x8f79: jsl_to(c, 0xb61e); break;
    case 0x8f7d: jsl_to(c, 0xf06f); break;
    case 0x8f81: jsl_to(c, 0x8fb2); break;
    case 0x8f85: jsl_to(c, 0xc535); break;
    case 0x8f89: index_constant(c, false, 0x0000); break;
    case 0x8f8c: byte_constant(c, 0x00); break;
    case 0x8f8e: store_a_abs_x8(c, 0x0b12); break;
    case 0x8f91: jsl_to(c, 0x8afc); break;
    case 0x8f95: cpx16_constant(c, 0x0120); break;
    case 0x8f98: branch_to(c, !c->c, 0x8f8c); break;
    case 0x8f9a: video_load(c, 0x1648, false); break;
    case 0x8f9d: branch_to(c, !c->n, 0x8fa3); break;
    case 0x8f9f: jsl_to(c, 0xd4bf); break;
    case 0x8fa3: video_load(c, 0x07bc, false); break;
    case 0x8fa6: branch_to(c, !c->n, 0x8fb1); break;
    case 0x8fa8: stz_abs8(c, 0x07bc); break;
    case 0x8fab: video_load(c, 0x07bb, false); break;
    case 0x8fae: store_register8(c, 0x0c40); break;
    case 0x8fb1: return_long(c); break;
  }
  return true;
}

/* $00877E–$87E8: scene specialty table fill from $028484/$0284E4 based on
 * $1648/$01D8/$01A1/$01D7/$01D4, then JSL $87E9/$87F1; BMI alts via $87D9. */
static bool specialty_table_877e_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x877e: case 0x8781: case 0x8783: case 0x8786: case 0x8788: case 0x878a:
    case 0x878d: case 0x878f: case 0x8792: case 0x8794: case 0x8796: case 0x8797:
    case 0x879a: case 0x879b: case 0x879c: case 0x87a0: case 0x87a3: case 0x87a6:
    case 0x87a7: case 0x87ab: case 0x87ae: case 0x87b1: case 0x87b3: case 0x87b5:
    case 0x87b8: case 0x87bb: case 0x87be: case 0x87c0: case 0x87c2: case 0x87c3:
    case 0x87c6: case 0x87c7: case 0x87c8: case 0x87c9: case 0x87cd: case 0x87d0:
    case 0x87d4: case 0x87d8: case 0x87d9: case 0x87db: case 0x87dc: case 0x87df:
    case 0x87e0: case 0x87e4: case 0x87e7:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x877e: video_load(c, 0x1648, false); break;
    case 0x8781: branch_to(c, !c->n, 0x878a); break;
    case 0x8783: video_load(c, 0x01d8, false); break;
    case 0x8786: cmp_constant8(c, 0x48); break;
    case 0x8788: branch_to(c, !c->z, 0x87ae); break;
    case 0x878a: case 0x87bb: video_load(c, 0x01a1, false); break;
    case 0x878d: case 0x87be: branch_to(c, c->n, 0x87d9); break;
    case 0x878f: video_load(c, 0x01d7, false); break;
    case 0x8792: branch_to(c, c->n, 0x87c0); break;
    case 0x8794: case 0x87c0: case 0x87d9: byte_constant(c, 0x00); break;
    case 0x8796: case 0x87c2: case 0x87db: xba_native(c); break;
    case 0x8797: case 0x87c3: video_load(c, 0x01a0, false); break;
    case 0x879a: case 0x87c6: asl_a8(c); break;
    case 0x879b: case 0x87c7: tax16_from_a8(c); break;
    case 0x879c: case 0x87a7: load_a_long_x8(c, 0x028484); break;
    case 0x87a0: case 0x87b5: store_register8(c, 0x01d2); break;
    case 0x87a3: case 0x87b8: store_register8(c, 0x01e2); break;
    case 0x87a6: case 0x87c8: implied(c); c->x++; zn(c, c->x, false); break;
    case 0x87ab: case 0x87cd: case 0x87e4: store_register8(c, 0x01d3); break;
    case 0x87ae: video_load(c, 0x01d4, false); break;
    case 0x87b1: branch_to(c, !c->n, 0x87d0); break;
    case 0x87b3: and_constant8(c, 0x07); break;
    case 0x87c9: load_a_long_x8(c, 0x028484); break;
    case 0x87d0: jsl_to(c, 0x87e9); break;
    case 0x87d4: jsl_to(c, 0x87f1); break;
    case 0x87d8: return_long(c); break;
    case 0x87dc: video_load(c, 0x01bd, false); break;
    case 0x87df: tax16_from_a8(c); break;
    case 0x87e0: load_a_long_x8(c, 0x0284e4); break;
    case 0x87e7: branch_to(c, true, 0x87d0); break;
  }
  return true;
}

/* $008D4E–$8DAB: specialty select — sync $1303 from $01E7 bit3 via $04844A,
 * then map $01A1/$01E7 flag bits to a specialty index and JSL $0485A1. */
static bool specialty_select_8d4e_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x8d4e: case 0x8d51: case 0x8d53: case 0x8d55: case 0x8d57: case 0x8d59:
    case 0x8d5b: case 0x8d5e: case 0x8d60: case 0x8d62: case 0x8d65: case 0x8d69:
    case 0x8d6c: case 0x8d6e: case 0x8d71: case 0x8d72: case 0x8d74: case 0x8d76:
    case 0x8d78: case 0x8d7a: case 0x8d7c: case 0x8d7f: case 0x8d80: case 0x8d82:
    case 0x8d83: case 0x8d84: case 0x8d85: case 0x8d87: case 0x8d88: case 0x8d8a:
    case 0x8d8b: case 0x8d8c: case 0x8d8e: case 0x8d8f: case 0x8d91: case 0x8d93:
    case 0x8d97: case 0x8d98: case 0x8d9a: case 0x8d9c: case 0x8d9e: case 0x8da0:
    case 0x8da2: case 0x8da4: case 0x8da6: case 0x8da8: case 0x8daa:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8d4e: case 0x8d6e: case 0x8d7c: video_load(c, 0x01e7, false); break;
    case 0x8d51: and_constant8(c, 0x08); break;
    case 0x8d53: branch_to(c, !c->z, 0x8d59); break;
    case 0x8d55: byte_constant(c, 0x00); break;
    case 0x8d57: branch_to(c, true, 0x8d5b); break;
    case 0x8d59: byte_constant(c, 0x02); break;
    case 0x8d5b: cmp_abs8(c, 0x1303); break;
    case 0x8d5e: branch_to(c, c->z, 0x8d69); break;
    case 0x8d60: ora_constant8(c, 0x80); break;
    case 0x8d62: store_register8(c, 0x1303); break;
    case 0x8d65: jsl_to(c, 0x844a); break;
    case 0x8d69: video_load(c, 0x01a1, false); break;
    case 0x8d6c: branch_to(c, !c->n, 0x8d7c); break;
    case 0x8d71: lsr_a8(c); break;
    case 0x8d72: branch_to(c, c->c, 0x8d78); break;
    case 0x8d74: byte_constant(c, 0x04); break;
    case 0x8d76: case 0x8d7a: branch_to(c, true, 0x8d93); break;
    case 0x8d78: byte_constant(c, 0x0a); break;
    case 0x8d7f: case 0x8d82: case 0x8d83: case 0x8d84: case 0x8d87:
    case 0x8d8a: case 0x8d8b: case 0x8d8e: asl_a8(c); break;
    case 0x8d80: branch_to(c, c->c, 0x8d98); break;
    case 0x8d85: branch_to(c, c->c, 0x8da8); break;
    case 0x8d88: branch_to(c, c->c, 0x8da4); break;
    case 0x8d8c: branch_to(c, c->c, 0x8d9c); break;
    case 0x8d8f: branch_to(c, c->c, 0x8da0); break;
    case 0x8d91: byte_constant(c, 0x03); break;
    case 0x8d93: jsl_to(c, 0x85a1); break;
    case 0x8d97: return_long(c); break;
    case 0x8d98: byte_constant(c, 0x07); break;
    case 0x8d9a: branch_to(c, true, 0x8d93); break;
    case 0x8d9c: byte_constant(c, 0x09); break;
    case 0x8d9e: branch_to(c, true, 0x8d93); break;
    case 0x8da0: byte_constant(c, 0x0c); break;
    case 0x8da2: branch_to(c, true, 0x8d93); break;
    case 0x8da4: byte_constant(c, 0x06); break;
    case 0x8da6: branch_to(c, true, 0x8d93); break;
    case 0x8da8: byte_constant(c, 0x05); break;
    case 0x8daa: branch_to(c, true, 0x8d93); break;
  }
  return true;
}

/* $008C84–$8D2A: post-$8C6D scene-mode[0] continuation — slot walk, specialty
 * JSLs, scroll snapshot, $008DF6 index, $85B6 slot seeds; RTL. */
static bool scene_cont_8c84_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x8c9d || pc == 0x8ca0 || pc == 0x8ca3 ||
                    pc == 0x8cd9 || pc == 0x8cdc);
  const bool sep_entry = (pc == 0x8ca6 || pc == 0x8cde);
  const bool rep_entry = (pc == 0x8c9b || pc == 0x8cd7);
  /* 8CA6 SEP is a merge: may enter already m8 (BCS) or m16 (fallthrough) */
  if(m16 && c->mf) return false;
  if(pc == 0x8cde && c->mf) return false; /* SEP after REP block only */
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0x8c84: case 0x8c88: case 0x8c8c: case 0x8c90: case 0x8c93: case 0x8c94:
    case 0x8c96: case 0x8c98: case 0x8c9b: case 0x8c9d: case 0x8ca0: case 0x8ca3:
    case 0x8ca6: case 0x8ca8: case 0x8cac: case 0x8cb0: case 0x8cb3: case 0x8cb6:
    case 0x8cb8: case 0x8cbb: case 0x8cbd: case 0x8cc0: case 0x8cc2: case 0x8cc5:
    case 0x8cc7: case 0x8cca: case 0x8ccd: case 0x8cd0: case 0x8cd3: case 0x8cd5:
    case 0x8cd7: case 0x8cd9: case 0x8cdc: case 0x8cde: case 0x8ce0: case 0x8ce2:
    case 0x8ce3: case 0x8ce6: case 0x8ce7: case 0x8ceb: case 0x8cee: case 0x8cf2:
    case 0x8cf4: case 0x8cf7: case 0x8cf9: case 0x8cfb: case 0x8cff: case 0x8d03:
    case 0x8d07: case 0x8d09: case 0x8d0d: case 0x8d0f: case 0x8d13: case 0x8d16:
    case 0x8d17: case 0x8d19: case 0x8d1c: case 0x8d1e: case 0x8d20: case 0x8d22:
    case 0x8d24: case 0x8d26: case 0x8d2a:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8c84: jsl_to(c, 0x8dac); break;
    case 0x8c88: jsl_to(c, 0x877e); break;
    case 0x8c8c: jsl_to(c, 0x91fc); break;
    case 0x8c90: video_load(c, 0x01bc, false); break;
    case 0x8c93: lsr_a8(c); break;
    case 0x8c94: branch_to(c, c->c, 0x8ca6); break;
    case 0x8c96: byte_constant(c, 0x04); break;
    case 0x8c98: store_register8(c, 0x01e3); break;
    case 0x8c9b: sep_rep_native(c, 0x20, false); break;
    case 0x8c9d: lda16_constant(c, 0x0001); break;
    case 0x8ca0: store_a_abs16(c, 0x0d55); break;
    case 0x8ca3: store_a_abs16(c, 0x0d57); break;
    case 0x8ca6: sep_rep_native(c, 0x20, true); break;
    case 0x8ca8: jsl_to(c, 0xf7a4); break;
    case 0x8cac: jsl_to(c, 0x8dfa); break;
    case 0x8cb0: video_load(c, 0x0701, false); break;
    case 0x8cb3: store_register8(c, 0x0700); break;
    case 0x8cb6: stz_dp8(c, 0x27); break;
    case 0x8cb8: video_load(c, 0x01a1, false); break;
    case 0x8cbb: branch_to(c, c->n, 0x8cd3); break;
    case 0x8cbd: load_y_abs16(c, 0x01a6); break;
    case 0x8cc0: store_dp_y16(c, 0x33); break;
    case 0x8cc2: load_y_abs16(c, 0x01a8); break;
    case 0x8cc5: store_dp_y16(c, 0x35); break;
    case 0x8cc7: load_y_abs16(c, 0x01aa); break;
    case 0x8cca: store_y_abs16(c, 0x01a2); break;
    case 0x8ccd: load_y_abs16(c, 0x01ac); break;
    case 0x8cd0: store_y_abs16(c, 0x01a4); break;
    case 0x8cd3: byte_constant(c, 0x08); break;
    case 0x8cd5: store_dp(c, 0x02, true); break;
    case 0x8cd7: sep_rep_native(c, 0x20, false); break;
    case 0x8cd9: lda16_constant(c, 0xc000); break;
    case 0x8cdc: store_dp(c, 0x00, false); break;
    case 0x8cde: sep_rep_native(c, 0x20, true); break;
    case 0x8ce0: byte_constant(c, 0x00); break;
    case 0x8ce2: xba_native(c); break;
    case 0x8ce3: video_load(c, 0x0d59, false); break;
    case 0x8ce6: tax16_from_a8(c); break;
    case 0x8ce7: load_a_long_x8(c, 0x008df6); break;
    case 0x8ceb: store_register8(c, 0x01e3); break;
    case 0x8cee: jsl_to(c, 0xc535); break;
    case 0x8cf2: byte_constant(c, 0x61); break;
    case 0x8cf4: store_register8(c, 0x0716); break;
    case 0x8cf7: byte_constant(c, 0x00); break;
    case 0x8cf9: store_dp(c, 0x29, true); break;
    case 0x8cfb: jsl_to(c, 0x8d4e); break;
    case 0x8cff: jsl_to(c, 0xd2f2); break;
    case 0x8d03: jsl_to(c, 0xd844); break;
    case 0x8d07: stz_dp8(c, 0x4f); break;
    case 0x8d09: jsl_to(c, 0xebc1); break;
    case 0x8d0d: byte_constant(c, 0x80); break;
    case 0x8d0f: jsl_to(c, 0x85b6); break;
    case 0x8d13: video_load(c, 0x01bc, false); break;
    case 0x8d16: lsr_a8(c); break;
    case 0x8d17: branch_to(c, !c->c, 0x8d2a); break;
    case 0x8d19: video_load(c, 0x01bc, false); break;
    case 0x8d1c: and_constant8(c, 0x40); break;
    case 0x8d1e: branch_to(c, !c->z, 0x8d24); break;
    case 0x8d20: byte_constant(c, 0x0d); break;
    case 0x8d22: branch_to(c, true, 0x8d26); break;
    case 0x8d24: byte_constant(c, 0x08); break;
    case 0x8d26: jsl_to(c, 0x85b6); break;
    case 0x8d2a: return_long(c); break;
  }
  return true;
}

/* $06EBC1–$EC09: remaining $8C84 callee — if $01A1/$01E7 allow and DP+$4F
 * low3 clear, index (DP+$4F>>2)&6 into $06EC0A word table + optional +$100 from
 * $01E7 bit0, copy $20 bytes from [$00] (bank #$0C) into $0280, set $0715=1. */
static bool scroll_blit_ebc1_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x06 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xebea || pc == 0xebee || pc == 0xebef || pc == 0xebf1 ||
                    pc == 0xebf3 || pc == 0xebf6 || pc == 0xebf8 || pc == 0xebfb ||
                    pc == 0xebfc || pc == 0xebfd || pc == 0xec00);
  const bool sep_from_m16 = (pc == 0xec02);
  const bool rep_to_m16 = (pc == 0xebe8);
  if(m16 && c->mf) return false;
  if(sep_from_m16 && c->mf) return false;
  if(rep_to_m16 && !c->mf) return false;
  if(!m16 && !sep_from_m16 && !rep_to_m16 && !c->mf) return false;
  switch(pc) {
    case 0xebc1: case 0xebc4: case 0xebc6: case 0xebc8: case 0xebc9: case 0xebcb:
    case 0xebcd: case 0xebcf: case 0xebd1: case 0xebd2: case 0xebd3: case 0xebd5:
    case 0xebd6: case 0xebd8: case 0xebda: case 0xebdc: case 0xebde: case 0xebe1:
    case 0xebe3: case 0xebe4: case 0xebe6: case 0xebe8: case 0xebea: case 0xebee:
    case 0xebef: case 0xebf1: case 0xebf3: case 0xebf6: case 0xebf8: case 0xebfb:
    case 0xebfc: case 0xebfd: case 0xec00: case 0xec02: case 0xec04: case 0xec06:
    case 0xec09:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xebc1: video_load(c, 0x01a1, false); break;
    case 0xebc4: branch_to(c, c->n, 0xec09); break;
    case 0xebc6: byte_constant(c, 0x00); break;
    case 0xebc8: xba_native(c); break;
    case 0xebc9: case 0xebcf: load_dp8(c, 0x4f); break;
    case 0xebcb: and_constant8(c, 0x07); break;
    case 0xebcd: branch_to(c, !c->z, 0xec09); break;
    case 0xebd1: case 0xebd2: case 0xebe3: lsr_a8(c); break;
    case 0xebd3: and_constant8(c, 0x06); break;
    case 0xebd5: tax16_from_a8(c); break;
    case 0xebd6: byte_constant(c, 0x0c); break;
    case 0xebd8: store_dp(c, 0x02, true); break;
    case 0xebda: stz_dp8(c, 0x00); break;
    case 0xebdc: stz_dp8(c, 0x01); break;
    case 0xebde: video_load(c, 0x01e7, false); break;
    case 0xebe1: branch_to(c, c->n, 0xec09); break;
    case 0xebe4: branch_to(c, !c->c, 0xebe8); break;
    case 0xebe6: inc_dp8(c, 0x01); break;
    case 0xebe8: sep_rep_native(c, 0x20, false); break;
    case 0xebea: lda_long_x16(c, 0x06ec0a); break;
    case 0xebee: implied(c); c->c = false; break;
    case 0xebef: adc_dp16(c, 0x00); break;
    case 0xebf1: store_dp(c, 0x00, false); break;
    case 0xebf3: index_constant(c, true, 0x0000); break;
    case 0xebf6: lda_indirect_long_y16(c, 0x00); break;
    case 0xebf8: store_a_abs_y16(c, 0x0280); break;
    case 0xebfb: case 0xebfc: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xebfd: cpy16_constant(c, 0x0020); break;
    case 0xec00: branch_to(c, !c->c, 0xebf6); break;
    case 0xec02: sep_rep_native(c, 0x20, true); break;
    case 0xec04: byte_constant(c, 0x01); break;
    case 0xec06: store_register8(c, 0x0715); break;
    case 0xec09: return_long(c); break;
  }
  return true;
}

/* $0485A1–$85B5: specialty command leaf — STA $1306, STZ $1308; if A==$88
 * also XBA→$1318; PLP/RTL. Abuts native $85B6. */
static bool specialty_cmd_85a1_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x04 || c->e || c->d) return false;
  /* PHP/SEP entry and PLP/RTL exits may see any M; body after SEP is m8. */
  if(pc != 0x85a1 && pc != 0x85a2 && pc != 0x85ae && pc != 0x85af &&
     pc != 0x85b4 && pc != 0x85b5 && !c->mf)
    return false;
  switch(pc) {
    case 0x85a1: case 0x85a2: case 0x85a4: case 0x85a7: case 0x85aa: case 0x85ac:
    case 0x85ae: case 0x85af: case 0x85b0: case 0x85b1: case 0x85b4: case 0x85b5:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x85a1: php_native(c); break;
    case 0x85a2: sep_rep_native(c, 0x20, true); break;
    case 0x85a4: store_register8(c, 0x1306); break;
    case 0x85a7: stz_abs8(c, 0x1308); break;
    case 0x85aa: cmp_constant8(c, 0x88); break;
    case 0x85ac: branch_to(c, c->z, 0x85b0); break;
    case 0x85ae: case 0x85b4: plp_native(c); break;
    case 0x85af: case 0x85b5: return_long(c); break;
    case 0x85b0: xba_native(c); break;
    case 0x85b1: store_register8(c, 0x1318); break;
  }
  return true;
}

/* $04844A–$848F: specialty sync from $1303 bit7 — PHB bank0, clear $1304/05,
 * JSL $8490 or $84B9 by (A&$7F)==2, clear bit7, JSL $85A1 with #$86/#$87 from
 * $1302, PLB/PLP/RTL. Nested $8490/$84B9/$84E2/$85F1 are native. */
static bool specialty_sync_844a_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x04 || c->e || c->d) return false;
  /* After REP #$10, X is 16-bit; PHP/SEP/PLP edges may see any M/X. */
  if(pc >= 0x8454 && pc <= 0x848d && c->xf) return false;
  if(pc != 0x844a && pc != 0x844b && pc != 0x848e && pc != 0x848f && !c->mf)
    return false;
  switch(pc) {
    case 0x844a: case 0x844b: case 0x844d: case 0x8450: case 0x8452: case 0x8454:
    case 0x8455: case 0x8457: case 0x8458: case 0x8459: case 0x845c: case 0x845e:
    case 0x8461: case 0x8464: case 0x8466: case 0x8468: case 0x846c: case 0x846e:
    case 0x8472: case 0x8475: case 0x8477: case 0x847a: case 0x847d: case 0x847f:
    case 0x8481: case 0x8485: case 0x8487: case 0x8489: case 0x848d: case 0x848e:
    case 0x848f:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x844a: php_native(c); break;
    case 0x844b: sep_rep_native(c, 0x20, true); break;
    case 0x844d: case 0x8459: case 0x8472: video_load(c, 0x1303, false); break;
    case 0x8450: branch_to(c, !c->n, 0x848e); break;
    case 0x8452: sep_rep_native(c, 0x10, false); break;
    case 0x8454: phb_native(c); break;
    case 0x8455: byte_constant(c, 0x00); break;
    case 0x8457: pha8(c); break;
    case 0x8458: plb_native(c); break;
    case 0x845c: case 0x8475: and_constant8(c, 0x7f); break;
    case 0x845e: stz_abs8(c, 0x1304); break;
    case 0x8461: stz_abs8(c, 0x1305); break;
    case 0x8464: cmp_constant8(c, 0x02); break;
    case 0x8466: branch_to(c, c->z, 0x846e); break;
    case 0x8468: jsl_to(c, 0x8490); break;
    case 0x846c: branch_to(c, true, 0x8472); break;
    case 0x846e: jsl_to(c, 0x84b9); break;
    case 0x8477: store_register8(c, 0x1303); break;
    case 0x847a: video_load(c, 0x1302, false); break;
    case 0x847d: branch_to(c, !c->z, 0x8487); break;
    case 0x847f: byte_constant(c, 0x86); break;
    case 0x8481: case 0x8489: jsl_to(c, 0x85a1); break;
    case 0x8485: branch_to(c, true, 0x848d); break;
    case 0x8487: byte_constant(c, 0x87); break;
    case 0x848d: plb_native(c); break;
    case 0x848e: plp_native(c); break;
    case 0x848f: return_long(c); break;
  }
  return true;
}

/* $0087E9–$87F8: thin wrappers — LDA $01D2 / $01D3 then JSL $87F9; RTL. */
static bool specialty_wrap_87e9_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x87e9: case 0x87ec: case 0x87f0: case 0x87f1: case 0x87f4: case 0x87f8:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x87e9: video_load(c, 0x01d2, false); break;
    case 0x87ec: case 0x87f4: jsl_to(c, 0x87f9); break;
    case 0x87f0: case 0x87f8: return_long(c); break;
    case 0x87f1: video_load(c, 0x01d3, false); break;
  }
  return true;
}

/* $0087F9–$8886: specialty graphics stream — index A → $028302 word as long
 * bank2 pointer; walk bytes until $FF; each index ×4 into $0283C4 → DP+$83/$20,
 * bank $0F, C559/C68C, then DMA via $90E6 (or $90EC+$028470 when idx≥$26). */
static bool specialty_gfx_87f9_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x8808 || pc == 0x8809 || pc == 0x880c || pc == 0x880e ||
                    pc == 0x8810 || pc == 0x8828 || pc == 0x8829 || pc == 0x882a ||
                    pc == 0x882b || pc == 0x882f || pc == 0x8831 || pc == 0x8832 ||
                    pc == 0x8833 || pc == 0x8837 || pc == 0x885f || pc == 0x8860 ||
                    pc == 0x8861 || pc == 0x8862 || pc == 0x8866 || pc == 0x8868 ||
                    pc == 0x8869 || pc == 0x886a || pc == 0x886e || pc == 0x886f ||
                    pc == 0x8872);
  const bool sep_from_m16 = (pc == 0x8812 || pc == 0x8839 || pc == 0x8875);
  const bool rep_to_m16 = (pc == 0x8806 || pc == 0x8826 || pc == 0x885d);
  if(m16 && c->mf) return false;
  if(sep_from_m16 && c->mf) return false;
  if(rep_to_m16 && !c->mf) return false;
  if(!m16 && !sep_from_m16 && !rep_to_m16 && !c->mf) return false;
  switch(pc) {
    case 0x87f9: case 0x87fa: case 0x87fc: case 0x87fe: case 0x87ff: case 0x8801:
    case 0x8802: case 0x8804: case 0x8805: case 0x8806: case 0x8808: case 0x8809:
    case 0x880c: case 0x880e: case 0x8810: case 0x8812: case 0x8814: case 0x8817:
    case 0x8819: case 0x881a: case 0x881c: case 0x881e: case 0x8820: case 0x8822:
    case 0x8823: case 0x8824: case 0x8826: case 0x8828: case 0x8829: case 0x882a:
    case 0x882b: case 0x882f: case 0x8831: case 0x8832: case 0x8833: case 0x8837:
    case 0x8839: case 0x883b: case 0x883d: case 0x883f: case 0x8841: case 0x8844:
    case 0x8848: case 0x884c: case 0x884e: case 0x8851: case 0x8853: case 0x8854:
    case 0x8856: case 0x8858: case 0x885a: case 0x885b: case 0x885d: case 0x885f:
    case 0x8860: case 0x8861: case 0x8862: case 0x8866: case 0x8868: case 0x8869:
    case 0x886a: case 0x886e: case 0x886f: case 0x8872: case 0x8875: case 0x8877:
    case 0x887b: case 0x887d: case 0x887f: case 0x8883: case 0x8885:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x87f9: pha8(c); break;
    case 0x87fa: byte_constant(c, 0x02); break;
    case 0x87fc: store_dp(c, 0x02, true); break;
    case 0x87fe: pla8(c); break;
    case 0x87ff: store_dp(c, 0x1f, true); break;
    case 0x8801: case 0x8804: case 0x8819: case 0x8853: xba_native(c); break;
    case 0x8802: case 0x8817: case 0x8851: byte_constant(c, 0x00); break;
    case 0x8805: asl_a8(c); break;
    case 0x8806: case 0x8826: case 0x885d: sep_rep_native(c, 0x20, false); break;
    case 0x8808: case 0x886e: implied(c); c->c = false; break;
    case 0x8809: adc16_constant(c, 0x8302); break;
    case 0x880c: case 0x8810: store_dp(c, 0x00, false); break;
    case 0x880e: lda_indirect_long16(c, 0x00); break;
    case 0x8812: case 0x8839: case 0x8875: sep_rep_native(c, 0x20, true); break;
    case 0x8814: index_constant(c, true, 0x0000); break;
    case 0x881a: lda_indirect_long_y8(c, 0x00); break;
    case 0x881c: store_dp(c, 0x1e, true); break;
    case 0x881e: cmp_constant8(c, 0xff); break;
    case 0x8820: branch_to(c, !c->z, 0x8823); break;
    case 0x8822: return_long(c); break;
    case 0x8823: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x8824: sty_dp16(c, 0x22); break;
    case 0x8828: case 0x8829: case 0x885f: case 0x8860: asl_a16(c); break;
    case 0x882a: case 0x8861: tax16(c); break;
    case 0x882b: case 0x8833: lda_long_x16(c, 0x0283c4); break;
    case 0x882f: store_dp(c, 0x83, false); break;
    case 0x8831: case 0x8832: case 0x8868: case 0x8869:
      implied(c); c->x++; zn(c, c->x, false); break;
    case 0x8837: store_dp(c, 0x20, false); break;
    case 0x883b: byte_constant(c, 0x0f); break;
    case 0x883d: store_dp(c, 0x85, true); break;
    case 0x883f: byte_constant(c, 0x80); break;
    case 0x8841: store_register8(c, 0x2115); break;
    case 0x8844: jsl_to(c, 0xc559); break;
    case 0x8848: jsl_to(c, 0xc68c); break;
    case 0x884c: load_dp_y16(c, 0x20); break;
    case 0x884e: store_y_abs16(c, 0x2116); break;
    case 0x8854: load_dp8(c, 0x1e); break;
    case 0x8856: cmp_constant8(c, 0x26); break;
    case 0x8858: branch_to(c, !c->c, 0x887f); break;
    case 0x885a: implied(c); c->c = true; break;
    case 0x885b: sbc8_constant(c, 0x26); break;
    case 0x8862: case 0x886a: lda_long_x16(c, 0x028470); break;
    case 0x8866: store_dp(c, 0x89, false); break;
    case 0x886f: adc16_constant(c, 0x9000); break;
    case 0x8872: store_a_abs16(c, 0x4302); break;
    case 0x8877: jsl_to(c, 0x90ec); break;
    case 0x887b: case 0x8883: load_dp_y16(c, 0x22); break;
    case 0x887d: branch_to(c, true, 0x8817); break;
    case 0x887f: jsl_to(c, 0x90e6); break;
    case 0x8885: branch_to(c, true, 0x8817); break;
  }
  return true;
}

/* $048490–$84B8 / $0484B9–$84E1: APU boot wrappers — JSL $84E2, seed DP+$73/$75
 * and $1300, disable NMIs ($4200), JSL $85F1 transfer, clear $2140–$2143; RTL.
 * $8490 uses source $188000; $84B9 uses $19B8A4. */
static bool apu_boot_8490_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x04 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x8490: case 0x8494: case 0x8497: case 0x849a: case 0x849c: case 0x849f:
    case 0x84a2: case 0x84a5: case 0x84a8: case 0x84ac: case 0x84af: case 0x84b2:
    case 0x84b5: case 0x84b8:
    case 0x84b9: case 0x84bd: case 0x84c0: case 0x84c3: case 0x84c5: case 0x84c8:
    case 0x84cb: case 0x84ce: case 0x84d1: case 0x84d5: case 0x84d8: case 0x84db:
    case 0x84de: case 0x84e1:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8490: case 0x84b9: jsl_to(c, 0x84e2); break;
    case 0x8494: index_constant(c, false, 0x8000); break;
    case 0x84bd: index_constant(c, false, 0xb8a4); break;
    case 0x8497: case 0x84c0: stx_abs16(c, 0x0073); break;
    case 0x849a: byte_constant(c, 0x18); break;
    case 0x84c3: byte_constant(c, 0x19); break;
    case 0x849c: case 0x84c5: store_register8(c, 0x0075); break;
    case 0x849f: case 0x84c8: index_constant(c, true, 0x0000); break;
    case 0x84a2: case 0x84cb: store_y_abs16(c, 0x1300); break;
    case 0x84a5: case 0x84ce: stz_abs8(c, 0x4200); break;
    case 0x84a8: case 0x84d1: jsl_to(c, 0x85f1); break;
    case 0x84ac: case 0x84d5: stz_abs8(c, 0x2140); break;
    case 0x84af: case 0x84d8: stz_abs8(c, 0x2141); break;
    case 0x84b2: case 0x84db: stz_abs8(c, 0x2142); break;
    case 0x84b5: case 0x84de: stz_abs8(c, 0x2143); break;
    case 0x84b8: case 0x84e1: return_long(c); break;
  }
  return true;
}

/* $0484E2–$8513: APU handshake — if $2140 already $BBAA skip to final NMI wait;
 * else NMI-sync, poke $80/$FF through $2140/$2141 until $2140 reads $BBAA; RTL. */
static bool apu_handshake_84e2_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x04 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x84e2: case 0x84e5: case 0x84e8: case 0x84ea: case 0x84ed: case 0x84ef:
    case 0x84f1: case 0x84f4: case 0x84f7: case 0x84fa: case 0x84fc: case 0x84ff:
    case 0x8501: case 0x8504: case 0x8506: case 0x8509: case 0x850c: case 0x850e:
    case 0x8511: case 0x8513:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x84e2: case 0x8501: index_constant(c, false, 0xbbaa); break;
    case 0x84e5: case 0x8509: cpx_abs16(c, 0x2140); break;
    case 0x84e8: branch_to(c, c->z, 0x850e); break;
    case 0x84ea: case 0x84fc: case 0x850e: bit_abs8(c, 0x4210); break;
    case 0x84ed: branch_to(c, !c->n, 0x84ea); break;
    case 0x84ef: byte_constant(c, 0x80); break;
    case 0x84f1: store_register8(c, 0x2140); break;
    case 0x84f4: store_register8(c, 0x2141); break;
    case 0x84f7: cmp_abs8(c, 0x2140); break;
    case 0x84fa: branch_to(c, !c->z, 0x84f7); break;
    case 0x84ff: branch_to(c, !c->n, 0x84fc); break;
    case 0x8504: byte_constant(c, 0xff); break;
    case 0x8506: store_register8(c, 0x2140); break;
    case 0x850c: branch_to(c, !c->z, 0x8504); break;
    case 0x8511: branch_to(c, !c->n, 0x850e); break;
    case 0x8513: return_long(c); break;
  }
  return true;
}

/* $0485F1–$8673: APU block transfer from long pointer DP+$73. Wait for $BBAA,
 * send $CC, then for each block (size/addr from [DP+$73],Y) stream bytes via
 * $2140–$2142; size 0 with $1300==0 advances bank at DP+$75 and retries from
 * $8000; BVS continues, else PLP/RTL. */
static bool apu_transfer_85f1_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x04 || c->e || c->d) return false;
  /* X stays 16-bit after REP #$30; reject x8 once past PHP. */
  if(pc != 0x85f1 && pc != 0x85f2 && c->xf) return false;
  const bool m16 =
      (pc == 0x85f4 || pc == 0x85f7 || pc == 0x85fa || pc == 0x85fd ||
       pc == 0x861a || pc == 0x862e || pc == 0x8630 || pc == 0x8631 ||
       pc == 0x8632 || pc == 0x8633 || pc == 0x8636 || pc == 0x8638 ||
       pc == 0x863b || pc == 0x8646 || pc == 0x8649 || pc == 0x864b ||
       pc == 0x864e || pc == 0x8650 || pc == 0x8651 || pc == 0x8652 ||
       pc == 0x8653 || pc == 0x8655 || pc == 0x8656 || pc == 0x8657);
  const bool sep_from_m16 =
      (pc == 0x85ff || pc == 0x861d || pc == 0x863d || pc == 0x865a);
  const bool rep_to_m16 =
      (pc == 0x85f2 || pc == 0x8618 || pc == 0x862c || pc == 0x8644);
  if(m16 && c->mf) return false;
  if(sep_from_m16 && c->mf) return false;
  if(rep_to_m16 && pc != 0x85f2 && !c->mf) return false;
  if(!m16 && !sep_from_m16 && !rep_to_m16 &&
     pc != 0x85f1 && pc != 0x8672 && pc != 0x8673 && !c->mf)
    return false;
  switch(pc) {
    case 0x85f1: case 0x85f2: case 0x85f4: case 0x85f7: case 0x85fa: case 0x85fd:
    case 0x85ff: case 0x8601: case 0x8603: case 0x8605: case 0x8607: case 0x8608:
    case 0x8609: case 0x860b: case 0x860d: case 0x860e: case 0x8610: case 0x8611:
    case 0x8612: case 0x8615: case 0x8617: case 0x8618: case 0x861a: case 0x861d:
    case 0x861f: case 0x8620: case 0x8622: case 0x8625: case 0x8627: case 0x8629:
    case 0x862b: case 0x862c: case 0x862e: case 0x8630: case 0x8631: case 0x8632:
    case 0x8633: case 0x8636: case 0x8638: case 0x863b: case 0x863d: case 0x863f:
    case 0x8642: case 0x8644: case 0x8646: case 0x8649: case 0x864b: case 0x864e:
    case 0x8650: case 0x8651: case 0x8652: case 0x8653: case 0x8655: case 0x8656:
    case 0x8657: case 0x865a: case 0x865c: case 0x865f: case 0x8661: case 0x8662:
    case 0x8665: case 0x8667: case 0x8668: case 0x866b: case 0x866e: case 0x8670:
    case 0x8672: case 0x8673:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x85f1: php_native(c); break;
    case 0x85f2: sep_rep_native(c, 0x30, false); break;
    case 0x85f4: case 0x864b: index_constant(c, true, 0x0000); break;
    case 0x85f7: lda16_constant(c, 0xbbaa); break;
    case 0x85fa: cmp_abs16(c, 0x2140); break;
    case 0x85fd: branch_to(c, !c->z, 0x85fa); break;
    case 0x85ff: case 0x861d: case 0x863d: case 0x865a:
      sep_rep_native(c, 0x20, true); break;
    case 0x8601: byte_constant(c, 0xcc); break;
    case 0x8603: branch_to(c, true, 0x862b); break;
    case 0x8605: case 0x860e: lda_indirect_long_y8(c, 0x73); break;
    case 0x8607: case 0x8610: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x8608: case 0x860d: case 0x8611: xba_native(c); break;
    case 0x8609: byte_constant(c, 0x00); break;
    case 0x860b: branch_to(c, true, 0x8618); break;
    case 0x8612: case 0x8622: case 0x866b: cmp_abs8(c, 0x2140); break;
    case 0x8615: branch_to(c, !c->z, 0x8612); break;
    case 0x8617: inc_a8(c); break;
    case 0x8618: case 0x862c: case 0x8644: sep_rep_native(c, 0x20, false); break;
    case 0x861a: store_a_abs16(c, 0x2140); break;
    case 0x861f: implied(c); c->x--; zn(c, c->x, false); break;
    case 0x8620: branch_to(c, !c->z, 0x860d); break;
    case 0x8625: branch_to(c, !c->z, 0x8622); break;
    case 0x8627: adc8_constant(c, 0x03); break;
    case 0x8629: branch_to(c, c->z, 0x8627); break;
    case 0x862b: pha8(c); break;
    case 0x862e: case 0x864e: case 0x8653: lda_indirect_long_y16(c, 0x73); break;
    case 0x8630: case 0x8631: case 0x8650: case 0x8651: case 0x8655: case 0x8656:
      implied(c); c->y++; zn(c, c->y, false); break;
    case 0x8632: case 0x8652: tax16(c); break;
    case 0x8633: cpx16_constant(c, 0x0000); break;
    case 0x8636: branch_to(c, !c->z, 0x8653); break;
    case 0x8638: video_load(c, 0x1300, false); break;
    case 0x863b: branch_to(c, !c->z, 0x8653); break;
    case 0x863f: inc_abs8_native(c, 0x1300); break;
    case 0x8642: inc_dp8(c, 0x75); break;
    case 0x8646: index_constant(c, true, 0x8000); break;
    case 0x8649: sty_dp16(c, 0x73); break;
    case 0x8657: store_a_abs16(c, 0x2142); break;
    case 0x865c: cpx16_constant(c, 0x0001); break;
    case 0x865f: byte_constant(c, 0x00); break;
    case 0x8661: rol_a8(c); break;
    case 0x8662: store_register8(c, 0x2141); break;
    case 0x8665: adc8_constant(c, 0x7f); break;
    case 0x8667: pla8(c); break;
    case 0x8668: store_register8(c, 0x2140); break;
    case 0x866e: branch_to(c, !c->z, 0x866b); break;
    case 0x8670: branch_to(c, c->v, 0x8605); break;
    case 0x8672: plp_native(c); break;
    case 0x8673: return_long(c); break;
  }
  return true;
}

/* $03F021–$F03F: scan actor slots for type>=$C0 whose $0B1C,Y matches A (saved
 * at DP+$00). Match → CLC/RTL with Y at slot; else advance via $8AF1 until Y>=$120
 * then RTL (C unchanged / set from last CPY). */
static bool actor_scan_f021_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x03 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xf021: case 0xf023: case 0xf026: case 0xf029: case 0xf02b: case 0xf02d:
    case 0xf030: case 0xf032: case 0xf034: case 0xf038: case 0xf03b: case 0xf03d:
    case 0xf03e: case 0xf03f:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf021: store_dp(c, 0x00, true); break;
    case 0xf023: index_constant(c, true, 0x0000); break;
    case 0xf026: load_a_abs_y8(c, 0x0b00); break;
    case 0xf029: cmp_constant8(c, 0xc0); break;
    case 0xf02b: branch_to(c, !c->c, 0xf034); break;
    case 0xf02d: load_a_abs_y8(c, 0x0b1c); break;
    case 0xf030: cmp_dp8(c, 0x00); break;
    case 0xf032: branch_to(c, c->z, 0xf03e); break;
    case 0xf034: jsl_to(c, 0x8af1); break;
    case 0xf038: cpy16_constant(c, 0x0120); break;
    case 0xf03b: branch_to(c, !c->c, 0xf026); break;
    case 0xf03d: return_long(c); break;
    case 0xf03e: implied(c); c->c = false; break;
    case 0xf03f: return_long(c); break;
  }
  return true;
}

/* $03F06F–$F08C: if $F021 finds a slot (C clear) whose type==$3A and $0C36
 * positive/zero (BPL), STZ $0C37 and rewrite type to #$39; else RTL. */
static bool actor_type_f06f_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x03 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xf06f: case 0xf071: case 0xf075: case 0xf077: case 0xf07a: case 0xf07c:
    case 0xf07e: case 0xf081: case 0xf083: case 0xf084: case 0xf087: case 0xf089:
    case 0xf08c:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf06f: byte_constant(c, 0x00); break;
    case 0xf071: jsl_to(c, 0xf021); break;
    case 0xf075: branch_to(c, c->c, 0xf083); break;
    case 0xf077: load_a_abs_y8(c, 0x0b01); break;
    case 0xf07a: cmp_constant8(c, 0x3a); break;
    case 0xf07c: branch_to(c, !c->z, 0xf083); break;
    case 0xf07e: video_load(c, 0x0c36, false); break;
    case 0xf081: branch_to(c, !c->n, 0xf084); break;
    case 0xf083: return_long(c); break;
    case 0xf084: stz_abs8(c, 0x0c37); break;
    case 0xf087: byte_constant(c, 0x39); break;
    case 0xf089: store_a_abs_y8(c, 0x0b01); break;
    case 0xf08c: return_long(c); break;
  }
  return true;
}

/* $008FB2–$9082: scene continuation after $8F46 — clear $0C35 bit7, gate on
 * $01D6/$1648/$01AE/$01D7/$0C40, then APU/specialty/JSLs through RTL (early
 * JMP $95B7 / $9083 leave this range). */
static bool scene_cont_8fb2_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x8fb2: case 0x8fb6: case 0x8fb9: case 0x8fbb: case 0x8fbe: case 0x8fc1:
    case 0x8fc3: case 0x8fc6: case 0x8fc9: case 0x8fcb: case 0x8fce: case 0x8fd0:
    case 0x8fd2: case 0x8fd5: case 0x8fd9: case 0x8fdc: case 0x8fde: case 0x8fe0:
    case 0x8fe2: case 0x8fe5: case 0x8fe8: case 0x8fea: case 0x8fed: case 0x8ff1:
    case 0x8ff4: case 0x8ff5: case 0x8ff7: case 0x8ffa: case 0x8ffc: case 0x8fff:
    case 0x9001: case 0x9004: case 0x9008: case 0x900c: case 0x900e: case 0x9011:
    case 0x9013: case 0x9017: case 0x901a: case 0x901e: case 0x9021: case 0x9023:
    case 0x9027: case 0x902b: case 0x902e: case 0x9030: case 0x9033: case 0x9035:
    case 0x9037: case 0x9039: case 0x903c: case 0x9040: case 0x9043: case 0x9045:
    case 0x9048: case 0x904a: case 0x904c: case 0x904e: case 0x9050: case 0x9053:
    case 0x9055: case 0x9057: case 0x9059: case 0x905b: case 0x905d: case 0x9061:
    case 0x9064: case 0x9066: case 0x9068: case 0x906b: case 0x906d: case 0x906f:
    case 0x9072: case 0x9076: case 0x907a: case 0x907e: case 0x9082:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8fb2: jsl_to(c, 0xfd64); break;
    case 0x8fb6: video_load(c, 0x0c35, false); break;
    case 0x8fb9: and_constant8(c, 0x7f); break;
    case 0x8fbb: store_register8(c, 0x0c35); break;
    case 0x8fbe: video_load(c, 0x01d6, false); break;
    case 0x8fc1: branch_to(c, !c->n, 0x8fc6); break;
    case 0x8fc3: jmp_abs_native(c, 0x95b7); break;
    case 0x8fc6: video_load(c, 0x1648, false); break;
    case 0x8fc9: branch_to(c, c->n, 0x8fd5); break;
    case 0x8fcb: video_load(c, 0x01ae, false); break;
    case 0x8fce: branch_to(c, !c->n, 0x8fd5); break;
    case 0x8fd0: byte_constant(c, 0x20); break;
    case 0x8fd2: store_register8(c, 0x0c41); break;
    case 0x8fd5: jsl_to(c, 0xb6ac); break;
    case 0x8fd9: video_load(c, 0x01d7, false); break;
    case 0x8fdc: branch_to(c, !c->n, 0x8fe5); break;
    case 0x8fde: and_constant8(c, 0x40); break;
    case 0x8fe0: branch_to(c, !c->z, 0x8fe5); break;
    case 0x8fe2: jmp_abs_native(c, 0x9083); break;
    case 0x8fe5: video_load(c, 0x0c40, false); break;
    case 0x8fe8: branch_to(c, c->z, 0x8fed); break;
    case 0x8fea: jmp_abs_native(c, 0x9083); break;
    case 0x8fed: jsl_to(c, 0xd8a5); break;
    case 0x8ff1: video_load(c, 0x01bc, false); break;
    case 0x8ff4: lsr_a8(c); break;
    case 0x8ff5: branch_to(c, !c->c, 0x8fff); break;
    case 0x8ff7: video_load(c, 0x01bc, false); break;
    case 0x8ffa: ora_constant8(c, 0x20); break;
    case 0x8ffc: store_register8(c, 0x01bc); break;
    case 0x8fff: byte_constant(c, 0xe0); break;
    case 0x9001: store_register8(c, 0x2132); break;
    case 0x9004: jsl_to(c, 0xfe4b); break;
    case 0x9008: jsl_to(c, 0x982c); break;
    case 0x900c: byte_constant(c, 0x10); break;
    case 0x900e: store_register8(c, 0x0d5f); break;
    case 0x9011: byte_constant(c, 0x12); break;
    case 0x9013: jsl_to(c, 0xbe70); break;
    case 0x9017: stz_abs8(c, 0x0d67); break;
    case 0x901a: jsl_to(c, 0xa48f); break;
    case 0x901e: video_load(c, 0x0d67, false); break;
    case 0x9021: branch_to(c, !c->n, 0x901a); break;
    case 0x9023: jsl_to(c, 0xb6bf); break;
    case 0x9027: jsl_to(c, 0x8324); break;
    case 0x902b: video_load(c, 0x1648, false); break;
    case 0x902e: branch_to(c, c->n, 0x9037); break;
    case 0x9030: video_load(c, 0x2140, false); break;
    case 0x9033: cmp_constant8(c, 0x80); break;
    case 0x9035: branch_to(c, !c->z, 0x9030); break;
    case 0x9037: byte_constant(c, 0x82); break;
    case 0x9039: store_register8(c, 0x1303); break;
    case 0x903c: jsl_to(c, 0x844a); break;
    case 0x9040: video_load(c, 0x1648, false); break;
    case 0x9043: branch_to(c, !c->n, 0x9050); break;
    case 0x9045: video_load(c, 0x01d8, false); break;
    case 0x9048: cmp_constant8(c, 0x40); break;
    case 0x904a: branch_to(c, !c->z, 0x9050); break;
    case 0x904c: byte_constant(c, 0x06); break;
    case 0x904e: branch_to(c, true, 0x905d); break;
    case 0x9050: video_load(c, 0x01e7, false); break;
    case 0x9053: and_constant8(c, 0x02); break;
    case 0x9055: branch_to(c, c->z, 0x905b); break;
    case 0x9057: byte_constant(c, 0x07); break;
    case 0x9059: branch_to(c, true, 0x905d); break;
    case 0x905b: byte_constant(c, 0x01); break;
    case 0x905d: jsl_to(c, 0x85a1); break;
    case 0x9061: video_load(c, 0x01d6, false); break;
    case 0x9064: and_constant8(c, 0x40); break;
    case 0x9066: branch_to(c, !c->z, 0x9076); break;
    case 0x9068: video_load(c, 0x01ae, false); break;
    case 0x906b: branch_to(c, !c->n, 0x9076); break;
    case 0x906d: byte_constant(c, 0x02); break;
    case 0x906f: store_register8(c, 0x073c); break;
    case 0x9072: jsl_to(c, 0xf455); break;
    case 0x9076: jsl_to(c, 0xf4a2); break;
    case 0x907a: jsl_to(c, 0xfcdd); break;
    case 0x907e: jsl_to(c, 0x8324); break;
    case 0x9082: return_long(c); break;
  }
  return true;
}

/* $048514–$85A0: APU command drain between handshake ($84E2) and specialty
 * leaf ($85A1) — echo $1306→$2140 when changed, then queue $1309 vs $1311. */
static bool apu_command_8514_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x04 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x8514: case 0x8517: case 0x8519: case 0x851c: case 0x851e: case 0x8521:
    case 0x8524: case 0x8526: case 0x8528: case 0x852a: case 0x852c: case 0x852f:
    case 0x8532: case 0x8535: case 0x8537: case 0x8539: case 0x853c: case 0x853f:
    case 0x8541: case 0x8544: case 0x8547: case 0x8549: case 0x854b: case 0x854e:
    case 0x8550: case 0x8553: case 0x8555: case 0x8558: case 0x855b: case 0x855d:
    case 0x855f: case 0x8561: case 0x8563: case 0x8566: case 0x8569: case 0x856c:
    case 0x856d: case 0x8570: case 0x8573: case 0x8576: case 0x8579: case 0x857c:
    case 0x857f: case 0x8580: case 0x8583: case 0x8585: case 0x8588: case 0x8589:
    case 0x858c: case 0x858f: case 0x8591: case 0x8594: case 0x8596: case 0x8599:
    case 0x859c: case 0x859e: case 0x85a0:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8514: case 0x852c: video_load(c, 0x1306, false); break;
    case 0x8517: branch_to(c, c->z, 0x8550); break;
    case 0x8519: cmp_abs8(c, 0x1307); break;
    case 0x851c: branch_to(c, !c->z, 0x852c); break;
    case 0x851e: inc_abs8_native(c, 0x1308); break;
    case 0x8521: video_load(c, 0x1308, false); break;
    case 0x8524: cmp_constant8(c, 0x02); break;
    case 0x8526: branch_to(c, c->z, 0x8549); break;
    case 0x8528: cmp_constant8(c, 0x04); break;
    case 0x852a: branch_to(c, !c->z, 0x8550); break;
    case 0x852f: store_register8(c, 0x2140); break;
    case 0x8532: store_register8(c, 0x1307); break;
    case 0x8535: cmp_constant8(c, 0x88); break;
    case 0x8537: branch_to(c, c->z, 0x8541); break;
    case 0x8539: stz_abs8(c, 0x1306); break;
    case 0x853c: stz_abs8(c, 0x1308); break;
    case 0x853f: branch_to(c, true, 0x8550); break;
    case 0x8541: video_load(c, 0x1318, false); break;
    case 0x8544: store_register8(c, 0x2143); break;
    case 0x8547: branch_to(c, true, 0x8539); break;
    case 0x8549: byte_constant(c, 0x00); break;
    case 0x854b: store_register8(c, 0x2140); break;
    case 0x854e: branch_to(c, true, 0x8550); break;
    case 0x8550: video_load(c, 0x1313, false); break;
    case 0x8553: branch_to(c, !c->z, 0x8589); break;
    case 0x8555: video_load(c, 0x1309, false); break;
    case 0x8558: cmp_abs8(c, 0x1311); break;
    case 0x855b: branch_to(c, !c->z, 0x856d); break;
    case 0x855d: cmp_constant8(c, 0x00); break;
    case 0x855f: branch_to(c, c->z, 0x8588); break;
    case 0x8561: byte_constant(c, 0x00); break;
    case 0x8563: store_register8(c, 0x2141); break;
    case 0x8566: store_register8(c, 0x1311); break;
    case 0x8569: case 0x8573: case 0x8596: inc_abs8_native(c, 0x1313); break;
    case 0x856c: case 0x8588: case 0x85a0: return_long(c); break;
    case 0x856d: store_register8(c, 0x2141); break;
    case 0x8570: store_register8(c, 0x1311); break;
    case 0x8576: index_constant(c, false, 0x0000); break;
    case 0x8579: load_a_abs_x8(c, 0x130a); break;
    case 0x857c: store_a_abs_x8(c, 0x1309); break;
    case 0x857f: implied(c); c->x++; zn(c, c->x, false); break;
    case 0x8580: cpx16_constant(c, 0x0007); break;
    case 0x8583: branch_to(c, !c->c, 0x8579); break;
    case 0x8585: stz_abs_x8(c, 0x1309); break;
    case 0x8589: video_load(c, 0x1311, false); break;
    case 0x858c: cmp_abs8(c, 0x2141); break;
    case 0x858f: branch_to(c, !c->z, 0x8596); break;
    case 0x8591: stz_abs8(c, 0x1313); break;
    case 0x8594: branch_to(c, true, 0x8550); break;
    case 0x8599: video_load(c, 0x1313, false); break;
    case 0x859c: cmp_constant8(c, 0x03); break;
    case 0x859e: branch_to(c, c->c, 0x8591); break;
  }
  return true;
}

/* $01C535–$C583: rewrite $0101,Y flag bytes from $01C584[X] (or #$84 when
 * X==$1A and $0EA3==5), OR bit0 when high bits match DP+$10 from $0061. */
static bool flag_rewrite_c535_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xc535: case 0xc538: case 0xc53b: case 0xc53e: case 0xc53f: case 0xc541:
    case 0xc543: case 0xc544: case 0xc547: case 0xc548: case 0xc54b: case 0xc54d:
    case 0xc550: case 0xc552: case 0xc554: case 0xc556: case 0xc558: case 0xc55c:
    case 0xc55f: case 0xc562: case 0xc564: case 0xc566: case 0xc568: case 0xc56a:
    case 0xc56c: case 0xc56f: case 0xc571: case 0xc573: case 0xc575: case 0xc578:
    case 0xc57a: case 0xc57d: case 0xc57e: case 0xc57f: case 0xc581: case 0xc583:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xc535: index_constant(c, false, 0x0000); break;
    case 0xc538: index_constant(c, true, 0x0000); break;
    case 0xc53b: video_load(c, 0x01b2, false); break;
    case 0xc53e: dec_a8(c); break;
    case 0xc53f: store_dp(c, 0x00, true); break;
    case 0xc541: byte_constant(c, 0x00); break;
    case 0xc543: xba_native(c); break;
    case 0xc544: load_a_abs_y8(c, 0x0100); break;
    case 0xc547: tax16_from_a8(c); break;
    case 0xc548: cpx16_constant(c, 0x001a); break;
    case 0xc54b: branch_to(c, !c->z, 0xc558); break;
    case 0xc54d: video_load(c, 0x0ea3, false); break;
    case 0xc550: cmp_constant8(c, 0x05); break;
    case 0xc552: branch_to(c, !c->z, 0xc558); break;
    case 0xc554: byte_constant(c, 0x84); break;
    case 0xc556: branch_to(c, true, 0xc55c); break;
    case 0xc558: load_a_long_x8(c, 0x01c584); break;
    case 0xc55c: store_a_abs_y8(c, 0x0101); break;
    case 0xc55f: video_load(c, 0x0061, false); break;
    case 0xc562: branch_to(c, !c->z, 0xc568); break;
    case 0xc564: byte_constant(c, 0x80); break;
    case 0xc566: branch_to(c, true, 0xc56a); break;
    case 0xc568: byte_constant(c, 0x40); break;
    case 0xc56a: store_dp(c, 0x10, true); break;
    case 0xc56c: load_a_abs_y8(c, 0x0101); break;
    case 0xc56f: and_constant8(c, 0xc0); break;
    case 0xc571: cmp_dp8(c, 0x10); break;
    case 0xc573: branch_to(c, !c->z, 0xc57d); break;
    case 0xc575: load_a_abs_y8(c, 0x0101); break;
    case 0xc578: ora_constant8(c, 0x01); break;
    case 0xc57a: store_a_abs_y8(c, 0x0101); break;
    case 0xc57d: case 0xc57e: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xc57f: dec_dp8(c, 0x00); break;
    case 0xc581: branch_to(c, !c->n, 0xc541); break;
    case 0xc583: return_long(c); break;
  }
  return true;
}

/* $01B61E–$B65C: clear/fill $7E3000.. when $0D66!=$1D; $0061==1 → JSL $B647
 * (fill #$3D40 down to X=$0580) then zero-fill remainder via shared loop. */
static bool wram_fill_b61e_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xb62e || pc == 0xb631 || pc == 0xb634 || pc == 0xb638 ||
                    pc == 0xb639 || pc == 0xb63a || pc == 0xb645 ||
                    pc == 0xb649 || pc == 0xb64c || pc == 0xb64f || pc == 0xb653 ||
                    pc == 0xb654 || pc == 0xb655 || pc == 0xb658);
  const bool sep_entry = (pc == 0xb63c || pc == 0xb65a);
  const bool rep_entry = (pc == 0xb62c || pc == 0xb643 || pc == 0xb647);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xb61e: case 0xb621: case 0xb623: case 0xb625: case 0xb628: case 0xb62a:
    case 0xb62c: case 0xb62e: case 0xb631: case 0xb634: case 0xb638: case 0xb639:
    case 0xb63a: case 0xb63c: case 0xb63e: case 0xb63f: case 0xb643: case 0xb645:
    case 0xb647: case 0xb649: case 0xb64c: case 0xb64f: case 0xb653: case 0xb654:
    case 0xb655: case 0xb658: case 0xb65a: case 0xb65c:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb61e: video_load(c, 0x0d66, false); break;
    case 0xb621: cmp_constant8(c, 0x1d); break;
    case 0xb623: branch_to(c, c->z, 0xb63e); break;
    case 0xb625: video_load(c, 0x0061, false); break;
    case 0xb628: cmp_constant8(c, 0x01); break;
    case 0xb62a: branch_to(c, c->z, 0xb63f); break;
    case 0xb62c: case 0xb643: case 0xb647: sep_rep_native(c, 0x20, false); break;
    case 0xb62e: case 0xb649: index_constant(c, false, 0x07fe); break;
    case 0xb631: lda16_constant(c, 0x0000); break;
    case 0xb634: case 0xb64f: store_a_long_x16(c, 0x7e3000); break;
    case 0xb638: case 0xb639: case 0xb653: case 0xb654:
      implied(c); c->x--; zn(c, c->x, false); break;
    case 0xb63a: branch_to(c, !c->n, 0xb634); break;
    case 0xb63c: case 0xb65a: sep_rep_native(c, 0x20, true); break;
    case 0xb63e: case 0xb65c: return_long(c); break;
    case 0xb63f: jsl_to(c, 0xb647); break;
    case 0xb645: branch_to(c, true, 0xb631); break;
    case 0xb64c: lda16_constant(c, 0x3d40); break;
    case 0xb655: cpx16_constant(c, 0x0580); break;
    case 0xb658: branch_to(c, c->c, 0xb64f); break;
  }
  return true;
}

/* $01B65D–$B674: after $B647 fill, zero $7E3000,X down until X==DP+$0E. */
static bool wram_tail_b65d_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xb665 || pc == 0xb668 || pc == 0xb66c || pc == 0xb66d ||
                    pc == 0xb66e || pc == 0xb670);
  const bool sep_entry = (pc == 0xb672);
  const bool rep_entry = (pc == 0xb663);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xb65d: case 0xb65f: case 0xb663: case 0xb665: case 0xb668: case 0xb66c:
    case 0xb66d: case 0xb66e: case 0xb670: case 0xb672: case 0xb674:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb65d: store_dp_x16(c, 0x0e); break;
    case 0xb65f: jsl_to(c, 0xb647); break;
    case 0xb663: sep_rep_native(c, 0x20, false); break;
    case 0xb665: lda16_constant(c, 0x0000); break;
    case 0xb668: store_a_long_x16(c, 0x7e3000); break;
    case 0xb66c: case 0xb66d: implied(c); c->x--; zn(c, c->x, false); break;
    case 0xb66e: cpx_dp16(c, 0x0e); break;
    case 0xb670: branch_to(c, c->c, 0xb668); break;
    case 0xb672: sep_rep_native(c, 0x20, true); break;
    case 0xb674: return_long(c); break;
  }
  return true;
}

/* $01B675–$B6AB: alt LDX#0→$B689; $B67A gate on $0D66 then VRAM queue seed
 * $2C00→$3000 len #$0800 bank $7E (via $8711/$8724). */
static bool vram_seed_b67a_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xb692 || pc == 0xb695 || pc == 0xb698 || pc == 0xb69b ||
                    pc == 0xb69e || pc == 0xb6a1);
  const bool sep_from_m16 = (pc == 0xb6a4);
  const bool sep_m8 = (pc == 0xb689); /* after $8711/$8724 (M already 8) */
  const bool rep_entry = (pc == 0xb690);
  if(m16 && c->mf) return false;
  if(sep_from_m16 && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_from_m16 && !sep_m8 && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xb675: case 0xb678: case 0xb67a: case 0xb67d: case 0xb67f: case 0xb681:
    case 0xb685: case 0xb689: case 0xb68b: case 0xb68d: case 0xb690: case 0xb692:
    case 0xb695: case 0xb698: case 0xb69b: case 0xb69e: case 0xb6a1: case 0xb6a4:
    case 0xb6a6: case 0xb6a8: case 0xb6ab:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb675: index_constant(c, false, 0x0000); break;
    case 0xb678: branch_to(c, true, 0xb689); break;
    case 0xb67a: video_load(c, 0x0d66, false); break;
    case 0xb67d: cmp_constant8(c, 0x1d); break;
    case 0xb67f: branch_to(c, c->z, 0xb6ab); break;
    case 0xb681: jsl_to(c, 0x8711); break;
    case 0xb685: jsl_to(c, 0x8724); break;
    case 0xb689: sep_rep_native(c, 0x20, true); break;
    case 0xb68b: byte_constant(c, 0x80); break;
    case 0xb68d: store_a_abs_x8(c, 0x0800); break;
    case 0xb690: sep_rep_native(c, 0x20, false); break;
    case 0xb692: lda16_constant(c, 0x2c00); break;
    case 0xb695: store_a_abs_x16(c, 0x0801); break;
    case 0xb698: lda16_constant(c, 0x3000); break;
    case 0xb69b: store_a_abs_x16(c, 0x0803); break;
    case 0xb69e: lda16_constant(c, 0x0800); break;
    case 0xb6a1: store_a_abs_x16(c, 0x0806); break;
    case 0xb6a4: sep_rep_native(c, 0x20, true); break;
    case 0xb6a6: byte_constant(c, 0x7e); break;
    case 0xb6a8: store_a_abs_x8(c, 0x0805); break;
    case 0xb6ab: return_long(c); break;
  }
  return true;
}

/* $01B6AC–$B6BE: fill $7E4000.. with #$00FF words, X=$07FE..0. */
static bool wram_fill_b6ac_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xb6ae || pc == 0xb6b1 || pc == 0xb6b4 || pc == 0xb6b8 ||
                    pc == 0xb6b9 || pc == 0xb6ba);
  const bool sep_entry = (pc == 0xb6bc);
  const bool rep_entry = (pc == 0xb6ac);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xb6ac: case 0xb6ae: case 0xb6b1: case 0xb6b4: case 0xb6b8: case 0xb6b9:
    case 0xb6ba: case 0xb6bc: case 0xb6be:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb6ac: sep_rep_native(c, 0x20, false); break;
    case 0xb6ae: index_constant(c, false, 0x07fe); break;
    case 0xb6b1: lda16_constant(c, 0x00ff); break;
    case 0xb6b4: store_a_long_x16(c, 0x7e4000); break;
    case 0xb6b8: case 0xb6b9: implied(c); c->x--; zn(c, c->x, false); break;
    case 0xb6ba: branch_to(c, !c->n, 0xb6b4); break;
    case 0xb6bc: sep_rep_native(c, 0x20, true); break;
    case 0xb6be: return_long(c); break;
  }
  return true;
}

/* $01B6BF–$B6EC: VRAM queue seed $6800→$4000 len #$0800 bank $7E; STZ $0D74. */
static bool vram_seed_b6bf_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xb6d0 || pc == 0xb6d3 || pc == 0xb6d6 || pc == 0xb6d9 ||
                    pc == 0xb6dc || pc == 0xb6df);
  const bool sep_from_m16 = (pc == 0xb6e2);
  const bool sep_m8 = (pc == 0xb6c7);
  const bool rep_entry = (pc == 0xb6ce);
  if(m16 && c->mf) return false;
  if(sep_from_m16 && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_from_m16 && !sep_m8 && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xb6bf: case 0xb6c3: case 0xb6c7: case 0xb6c9: case 0xb6cb: case 0xb6ce:
    case 0xb6d0: case 0xb6d3: case 0xb6d6: case 0xb6d9: case 0xb6dc: case 0xb6df:
    case 0xb6e2: case 0xb6e4: case 0xb6e6: case 0xb6e9: case 0xb6ec:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb6bf: jsl_to(c, 0x8711); break;
    case 0xb6c3: jsl_to(c, 0x8724); break;
    case 0xb6c7: sep_rep_native(c, 0x20, true); break;
    case 0xb6c9: byte_constant(c, 0x80); break;
    case 0xb6cb: store_a_abs_x8(c, 0x0800); break;
    case 0xb6ce: sep_rep_native(c, 0x20, false); break;
    case 0xb6d0: lda16_constant(c, 0x6800); break;
    case 0xb6d3: store_a_abs_x16(c, 0x0801); break;
    case 0xb6d6: lda16_constant(c, 0x4000); break;
    case 0xb6d9: store_a_abs_x16(c, 0x0803); break;
    case 0xb6dc: lda16_constant(c, 0x0800); break;
    case 0xb6df: store_a_abs_x16(c, 0x0806); break;
    case 0xb6e2: sep_rep_native(c, 0x20, true); break;
    case 0xb6e4: byte_constant(c, 0x7e); break;
    case 0xb6e6: store_a_abs_x8(c, 0x0805); break;
    case 0xb6e9: stz_abs8(c, 0x0d74); break;
    case 0xb6ec: return_long(c); break;
  }
  return true;
}

/* $009083–$90C0: path after $8FB2 early JMPs — JSLs, optional $7E4800 seed,
 * wait on $0D67, BRA into $907E. */
static bool scene_cont_9083_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x90a7 || pc == 0x90aa || pc == 0x90ae);
  const bool sep_entry = (pc == 0x90b1);
  const bool rep_entry = (pc == 0x90a5);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0x9083: case 0x9087: case 0x908b: case 0x908f: case 0x9093: case 0x9095:
    case 0x9098: case 0x909c: case 0x909f: case 0x90a1: case 0x90a5: case 0x90a7:
    case 0x90aa: case 0x90ae: case 0x90b1: case 0x90b3: case 0x90b6: case 0x90ba:
    case 0x90bd: case 0x90bf:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x9083: jsl_to(c, 0xf802); break;
    case 0x9087: jsl_to(c, 0xf4b2); break;
    case 0x908b: jsl_to(c, 0xb6bf); break;
    case 0x908f: jsl_to(c, 0x8324); break;
    case 0x9093: byte_constant(c, 0x80); break;
    case 0x9095: store_register8(c, 0x0ea1); break;
    case 0x9098: jsl_to(c, 0xf375); break;
    case 0x909c: video_load(c, 0x0c41, false); break;
    case 0x909f: branch_to(c, !c->n, 0x90b3); break;
    case 0x90a1: jsl_to(c, 0x8996); break;
    case 0x90a5: sep_rep_native(c, 0x20, false); break;
    case 0x90a7: lda16_constant(c, 0x2137); break;
    case 0x90aa: sta_long16(c, 0x7e4800); break;
    case 0x90ae: stz_abs16(c, 0x0d7d); break;
    case 0x90b1: sep_rep_native(c, 0x20, true); break;
    case 0x90b3: stz_abs8(c, 0x0d67); break;
    case 0x90b6: jsl_to(c, 0xa48f); break;
    case 0x90ba: video_load(c, 0x0d67, false); break;
    case 0x90bd: branch_to(c, !c->n, 0x90b6); break;
    case 0x90bf: branch_to(c, true, 0x907e); break;
  }
  return true;
}

/* $03FD64–$FE0D: first JSL from $8FB2 — gate on $1648/$01D8/$0171/$0EA3/$15FA,
 * seed scroll/type state, compact $0190 list, JSL $E4F4. */
static bool scene_gate_fd64_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x03 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xfdc0 || pc == 0xfdc3 || pc == 0xfdc6 || pc == 0xfdc7 ||
                    pc == 0xfdca || pc == 0xfdcd || pc == 0xfdd0 || pc == 0xfdd3 ||
                    pc == 0xfdd4 || pc == 0xfdd7);
  const bool sep_entry = (pc == 0xfdda);
  const bool rep_entry = (pc == 0xfdbe);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xfd64: case 0xfd67: case 0xfd69: case 0xfd6c: case 0xfd6e: case 0xfd70:
    case 0xfd73: case 0xfd75: case 0xfd77: case 0xfd7a: case 0xfd7c: case 0xfd7e:
    case 0xfd81: case 0xfd83: case 0xfd84: case 0xfd87: case 0xfd89: case 0xfd8c:
    case 0xfd8f: case 0xfd91: case 0xfd94: case 0xfd96: case 0xfd98: case 0xfd9a:
    case 0xfd9c: case 0xfd9e: case 0xfda0: case 0xfda3: case 0xfda5: case 0xfda7:
    case 0xfda9: case 0xfdab: case 0xfdad: case 0xfdb0: case 0xfdb3: case 0xfdb6:
    case 0xfdb8: case 0xfdbb: case 0xfdbe: case 0xfdc0: case 0xfdc3: case 0xfdc6:
    case 0xfdc7: case 0xfdca: case 0xfdcd: case 0xfdd0: case 0xfdd3: case 0xfdd4:
    case 0xfdd7: case 0xfdda: case 0xfddc: case 0xfddf: case 0xfde2: case 0xfde4:
    case 0xfde6: case 0xfde8: case 0xfde9: case 0xfdeb: case 0xfdee: case 0xfdf1:
    case 0xfdf2: case 0xfdf4: case 0xfdf6: case 0xfdf9: case 0xfdfc: case 0xfdff:
    case 0xfe00: case 0xfe02: case 0xfe04: case 0xfe07: case 0xfe09: case 0xfe0d:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xfd64: video_load(c, 0x1648, false); break;
    case 0xfd67: branch_to(c, !c->n, 0xfd83); break;
    case 0xfd69: video_load(c, 0x01d8, false); break;
    case 0xfd6c: cmp_constant8(c, 0x32); break;
    case 0xfd6e: branch_to(c, !c->z, 0xfd83); break;
    case 0xfd70: video_load(c, 0x0171, false); break;
    case 0xfd73: and_constant8(c, 0x08); break;
    case 0xfd75: branch_to(c, !c->z, 0xfd83); break;
    case 0xfd77: video_load(c, 0x0ea3, false); break;
    case 0xfd7a: cmp_constant8(c, 0x51); break;
    case 0xfd7c: branch_to(c, !c->z, 0xfd83); break;
    case 0xfd7e: video_load(c, 0x15fa, false); break;
    case 0xfd81: branch_to(c, c->n, 0xfd84); break;
    case 0xfd83: return_long(c); break;
    case 0xfd84: video_load(c, 0x0171, false); break;
    case 0xfd87: ora_constant8(c, 0x08); break;
    case 0xfd89: store_register8(c, 0x0171); break;
    case 0xfd8c: video_load(c, 0x15f1, false); break;
    case 0xfd8f: branch_to(c, c->z, 0xfda0); break;
    case 0xfd91: video_load(c, 0x0173, false); break;
    case 0xfd94: and_constant8(c, 0x10); break;
    case 0xfd96: branch_to(c, c->z, 0xfd9c); break;
    case 0xfd98: byte_constant(c, 0x4f); break;
    case 0xfd9a: branch_to(c, true, 0xfdad); break;
    case 0xfd9c: byte_constant(c, 0x50); break;
    case 0xfd9e: branch_to(c, true, 0xfdad); break;
    case 0xfda0: video_load(c, 0x0173, false); break;
    case 0xfda3: and_constant8(c, 0x10); break;
    case 0xfda5: branch_to(c, c->z, 0xfdab); break;
    case 0xfda7: byte_constant(c, 0x68); break;
    case 0xfda9: branch_to(c, true, 0xfdad); break;
    case 0xfdab: byte_constant(c, 0x69); break;
    case 0xfdad: store_register8(c, 0x0ea3); break;
    case 0xfdb0: stz_abs8(c, 0x15fa); break;
    case 0xfdb3: video_load(c, 0x0173, false); break;
    case 0xfdb6: ora_constant8(c, 0x30); break;
    case 0xfdb8: store_register8(c, 0x0173); break;
    case 0xfdbb: stz_abs8(c, 0x0c40); break;
    case 0xfdbe: sep_rep_native(c, 0x20, false); break;
    case 0xfdc0: lda16_constant(c, 0x07b8); break;
    case 0xfdc3: store_a_abs16(c, 0x01aa); break;
    case 0xfdc6: implied(c); c->c = true; break;
    case 0xfdc7: sbc16_constant(c, 0x0080); break;
    case 0xfdca: store_a_abs16(c, 0x01a6); break;
    case 0xfdcd: lda16_constant(c, 0x0358); break;
    case 0xfdd0: store_a_abs16(c, 0x01ac); break;
    case 0xfdd3: implied(c); c->c = true; break;
    case 0xfdd4: sbc16_constant(c, 0x0078); break;
    case 0xfdd7: store_a_abs16(c, 0x01a8); break;
    case 0xfdda: sep_rep_native(c, 0x20, true); break;
    case 0xfddc: index_constant(c, false, 0x0000); break;
    case 0xfddf: load_a_abs_x8(c, 0x0190); break;
    case 0xfde2: branch_to(c, c->z, 0xfdeb); break;
    case 0xfde4: cmp_constant8(c, 0xff); break;
    case 0xfde6: branch_to(c, c->z, 0xfe0d); break;
    case 0xfde8: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xfde9: branch_to(c, true, 0xfddf); break;
    case 0xfdeb: load_a_abs_x8(c, 0x0191); break;
    case 0xfdee: store_a_abs_x8(c, 0x0190); break;
    case 0xfdf1: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xfdf2: cmp_constant8(c, 0xff); break;
    case 0xfdf4: branch_to(c, !c->z, 0xfdeb); break;
    case 0xfdf6: stz_abs8(c, 0x01bb); break;
    case 0xfdf9: index_constant(c, false, 0x0000); break;
    case 0xfdfc: load_a_abs_x8(c, 0x0190); break;
    case 0xfdff: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xfe00: cmp_constant8(c, 0xff); break;
    case 0xfe02: branch_to(c, c->z, 0xfe09); break;
    case 0xfe04: inc_abs8_native(c, 0x01bb); break;
    case 0xfe07: branch_to(c, true, 0xfdfc); break;
    case 0xfe09: jsl_to(c, 0xe4f4); break;
    case 0xfe0d: return_long(c); break;
  }
  return true;
}

/* $00A956–$A960: wait $4212 bit7 clear then set; RTS. $A95B is mid-entry. */
static bool hblank_wait_a95b_step(Cpu *c, uint16_t pc) {
  if(c->e || c->d || !c->mf) return false;
  switch(pc) {
    case 0xa956: case 0xa959: case 0xa95b: case 0xa95e: case 0xa960: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xa956: bit_abs8(c, 0x4212); break;
    case 0xa959: branch_to(c, c->n, 0xa956); break;
    case 0xa95b: bit_abs8(c, 0x4212); break;
    case 0xa95e: branch_to(c, !c->n, 0xa95b); break;
    case 0xa960: return_short(c); break;
  }
  return true;
}

/* $00A961–$A99A: after hblank wait, JSR $B960 then load Mode-7 matrix
 * from DP+$76..$7B into $211B–$2120 (identity-scale + center). */
static bool mode7_matrix_a961_step(Cpu *c, uint16_t pc) {
  if(c->e || c->d || !c->mf) return false;
  switch(pc) {
    case 0xa961: case 0xa964: case 0xa967: case 0xa969: case 0xa96c: case 0xa96e:
    case 0xa971: case 0xa974: case 0xa977: case 0xa97a: case 0xa97d: case 0xa97f:
    case 0xa982: case 0xa984: case 0xa987: case 0xa989: case 0xa98c: case 0xa98e:
    case 0xa991: case 0xa993: case 0xa996: case 0xa998:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xa961: jsr_abs_native(c, 0xa956); break;
    case 0xa964: jsr_abs_native(c, 0xb960); break;
    case 0xa967: load_dp8(c, 0x76); break;
    case 0xa969: store_register8(c, 0x211b); break;
    case 0xa96c: load_dp8(c, 0x77); break;
    case 0xa96e: store_register8(c, 0x211b); break;
    case 0xa971: stz_abs8(c, 0x211c); break;
    case 0xa974: stz_abs8(c, 0x211c); break;
    case 0xa977: stz_abs8(c, 0x211d); break;
    case 0xa97a: stz_abs8(c, 0x211d); break;
    case 0xa97d: load_dp8(c, 0x76); break;
    case 0xa97f: store_register8(c, 0x211e); break;
    case 0xa982: load_dp8(c, 0x77); break;
    case 0xa984: store_register8(c, 0x211e); break;
    case 0xa987: load_dp8(c, 0x78); break;
    case 0xa989: store_register8(c, 0x211f); break;
    case 0xa98c: load_dp8(c, 0x79); break;
    case 0xa98e: store_register8(c, 0x211f); break;
    case 0xa991: load_dp8(c, 0x7a); break;
    case 0xa993: store_register8(c, 0x2120); break;
    case 0xa996: load_dp8(c, 0x7b); break;
    case 0xa998: store_register8(c, 0x2120); break;
  }
  return true;
}

/* $01B6ED–$B6FF: fill $7E8000.. with #$00FF words, X=$0FFE..0. */
static bool wram_fill_b6ed_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xb6ef || pc == 0xb6f2 || pc == 0xb6f5 || pc == 0xb6f9 ||
                    pc == 0xb6fa || pc == 0xb6fb);
  const bool sep_entry = (pc == 0xb6fd);
  const bool rep_entry = (pc == 0xb6ed);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xb6ed: case 0xb6ef: case 0xb6f2: case 0xb6f5: case 0xb6f9: case 0xb6fa:
    case 0xb6fb: case 0xb6fd: case 0xb6ff:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb6ed: sep_rep_native(c, 0x20, false); break;
    case 0xb6ef: index_constant(c, false, 0x0ffe); break;
    case 0xb6f2: lda16_constant(c, 0x00ff); break;
    case 0xb6f5: store_a_long_x16(c, 0x7e8000); break;
    case 0xb6f9: case 0xb6fa: implied(c); c->x--; zn(c, c->x, false); break;
    case 0xb6fb: branch_to(c, !c->n, 0xb6f5); break;
    case 0xb6fd: sep_rep_native(c, 0x20, true); break;
    case 0xb6ff: return_long(c); break;
  }
  return true;
}

/* $01B700–$B72A: VRAM queue seed $6000→$8000 len #$1000 bank $7E. */
static bool vram_seed_b700_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xb711 || pc == 0xb714 || pc == 0xb717 || pc == 0xb71a ||
                    pc == 0xb71d || pc == 0xb720);
  const bool sep_from_m16 = (pc == 0xb723);
  const bool sep_m8 = (pc == 0xb708);
  const bool rep_entry = (pc == 0xb70f);
  if(m16 && c->mf) return false;
  if(sep_from_m16 && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_from_m16 && !sep_m8 && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xb700: case 0xb704: case 0xb708: case 0xb70a: case 0xb70c: case 0xb70f:
    case 0xb711: case 0xb714: case 0xb717: case 0xb71a: case 0xb71d: case 0xb720:
    case 0xb723: case 0xb725: case 0xb727: case 0xb72a:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb700: jsl_to(c, 0x8711); break;
    case 0xb704: jsl_to(c, 0x8724); break;
    case 0xb708: sep_rep_native(c, 0x20, true); break;
    case 0xb70a: byte_constant(c, 0x80); break;
    case 0xb70c: store_a_abs_x8(c, 0x0800); break;
    case 0xb70f: sep_rep_native(c, 0x20, false); break;
    case 0xb711: lda16_constant(c, 0x6000); break;
    case 0xb714: store_a_abs_x16(c, 0x0801); break;
    case 0xb717: lda16_constant(c, 0x8000); break;
    case 0xb71a: store_a_abs_x16(c, 0x0803); break;
    case 0xb71d: lda16_constant(c, 0x1000); break;
    case 0xb720: store_a_abs_x16(c, 0x0806); break;
    case 0xb723: sep_rep_native(c, 0x20, true); break;
    case 0xb725: byte_constant(c, 0x7e); break;
    case 0xb727: store_a_abs_x8(c, 0x0805); break;
    case 0xb72a: return_long(c); break;
  }
  return true;
}

/* $01F802–$F83C: if $0D9B negative rewrite #$40+RTL; else walk Y=0..4 slots
 * via $0594A1 into $0E14/$0E16. */
static bool actor_prep_f802_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xf815 || pc == 0xf816 || pc == 0xf817 ||
                    pc == 0xf818 || pc == 0xf819 || pc == 0xf81a || pc == 0xf81b ||
                    pc == 0xf82e || pc == 0xf830);
  const bool sep_entry = (pc == 0xf81c || pc == 0xf833);
  const bool rep_entry = (pc == 0xf813 || pc == 0xf82c);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xf802: case 0xf805: case 0xf807: case 0xf809: case 0xf80c: case 0xf80d:
    case 0xf80f: case 0xf812: case 0xf813: case 0xf815: case 0xf816: case 0xf817:
    case 0xf818: case 0xf819: case 0xf81a: case 0xf81b: case 0xf81c: case 0xf81e:
    case 0xf821: case 0xf822: case 0xf826: case 0xf827: case 0xf829: case 0xf82c:
    case 0xf82e: case 0xf830: case 0xf833: case 0xf835: case 0xf836: case 0xf837:
    case 0xf83a: case 0xf83c:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf802: video_load(c, 0x0d9b, false); break;
    case 0xf805: branch_to(c, !c->n, 0xf80d); break;
    case 0xf807: byte_constant(c, 0x40); break;
    case 0xf809: store_register8(c, 0x0d9b); break;
    case 0xf80c: return_long(c); break;
    case 0xf80d: stz_dp8(c, 0x01); break;
    case 0xf80f: index_constant(c, true, 0x0000); break;
    case 0xf812: phy16(c); break;
    case 0xf813: sep_rep_native(c, 0x20, false); break;
    case 0xf815: tya16(c); break;
    case 0xf816: case 0xf817: case 0xf818: case 0xf819: case 0xf81a: asl_a16(c); break;
    case 0xf81b: tay16(c); break;
    case 0xf81c: sep_rep_native(c, 0x20, true); break;
    case 0xf81e: load_a_abs_y8(c, 0x0e01); break;
    case 0xf821: phy16(c); break;
    case 0xf822: jsl_to(c, 0x94a1); break;
    case 0xf826: ply16(c); break;
    case 0xf827: load_dp8(c, 0x02); break;
    case 0xf829: store_a_abs_y8(c, 0x0e16); break;
    case 0xf82c: sep_rep_native(c, 0x20, false); break;
    case 0xf82e: load_dp16_to_a(c, 0x00); break;
    case 0xf830: store_a_abs_y16(c, 0x0e14); break;
    case 0xf833: sep_rep_native(c, 0x20, true); break;
    case 0xf835: ply16(c); break;
    case 0xf836: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xf837: cpy16_constant(c, 0x0005); break;
    case 0xf83a: branch_to(c, !c->c, 0xf812); break;
    case 0xf83c: return_long(c); break;
  }
  return true;
}

/* $03F375–$F39D: gate on $1648/$01D8/$0173; clear $0173 bit7, $1303=#$82,
 * JSL $04844A then $0485A1(#$01). */
/* $00:FCE2/FCE6: scene call/return wrapper (M/X variants). */
static bool scene_wrap_fce2_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->d || !c->mf) return false;
  if(pc != 0xfce2 && pc != 0xfce6) return false;
  (void)fetch(c);
  if(pc == 0xfce2) jsl_to(c,0x9390); else return_long(c);
  return true;
}

/* $00:9AEC-$9AF4: scene call/return wrapper. */
static bool scene_wrap_9aec_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) { case 0x9aec: case 0x9af0: case 0x9af4: break; default: return false; }
  (void)fetch(c);
  if(pc == 0x9aec) jsl_to(c,0x9735); else if(pc == 0x9af0) jsl_to(c,0xf463); else return_long(c);
  return true;
}

/* $00:9AE3-$9AE7: scene call wrapper. */
static bool scene_wrap_9ae3_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->xf || c->d || !c->mf) return false;
  if(pc != 0x9ae3 && pc != 0x9ae7) return false;
  (void)fetch(c); if(pc == 0x9ae3) jsl_to(c,0x9735); else jsl_to(c,0xf0bc); return true;
}

/* $00:9AFF-$9B03: scene wrapper chaining the common return leaf and bank-3 actor setup. */
static bool scene_wrap_9aff_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->xf || c->d || !c->mf) return false;
  if(pc != 0x9aff && pc != 0x9b03) return false;
  (void)fetch(c);
  if(pc == 0x9aff) jsl_to(c,0x9735); else jsl_to(c,0xe8aa);
  return true;
}

/* $02:F463: actor leaf return. */
static bool actor_return_2f463_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 2 || c->e || c->xf || c->d || !c->mf || pc != 0xf463) return false;
  (void)fetch(c); return_long(c); return true;
}

/* $03:F246/$F39E/$F39F/$F3A0/$F4AD/$F560/$F582/$F5A2: leaf returns. */
static bool scene_return_bank3_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 3 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) { case 0xf246: case 0xf39e: case 0xf39f: case 0xf3a0: case 0xf4ad: case 0xf560: case 0xf582: case 0xf5a2: break; default: return false; }
  (void)fetch(c); return_long(c); return true;
}

/* $03:E8AA: load the actor selector before the shared bank-3 setup call. */
static bool scene_actor_e8aa_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 3 || c->e || c->xf || c->d || !c->mf || pc != 0xe8aa) return false;
  (void)fetch(c); video_load(c, 0x0d8d, false); return true;
}

static bool scene_gate_f375_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x03 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xf375: case 0xf378: case 0xf37a: case 0xf37d: case 0xf37f: case 0xf381:
    case 0xf384: case 0xf386: case 0xf389: case 0xf38b: case 0xf38e: case 0xf390:
    case 0xf393: case 0xf397: case 0xf399: case 0xf39d:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf375: video_load(c, 0x1648, false); break;
    case 0xf378: branch_to(c, !c->n, 0xf39d); break;
    case 0xf37a: video_load(c, 0x01d8, false); break;
    case 0xf37d: cmp_constant8(c, 0x11); break;
    case 0xf37f: branch_to(c, !c->z, 0xf39d); break;
    case 0xf381: video_load(c, 0x0173, false); break;
    case 0xf384: branch_to(c, !c->n, 0xf39d); break;
    case 0xf386: video_load(c, 0x0173, false); break;
    case 0xf389: and_constant8(c, 0x7f); break;
    case 0xf38b: store_register8(c, 0x0173); break;
    case 0xf38e: byte_constant(c, 0x82); break;
    case 0xf390: store_register8(c, 0x1303); break;
    case 0xf393: jsl_to(c, 0x844a); break;
    case 0xf397: byte_constant(c, 0x01); break;
    case 0xf399: jsl_to(c, 0x85a1); break;
    case 0xf39d: return_long(c); break;
  }
  return true;
}

/* $03FE0E–$FE29: snapshot $1648/$01D8/$01D9/$01DC into $07BD..$07C0; STZ $01D9. */
static bool scene_snap_fe0e_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x03 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xfe0e: case 0xfe11: case 0xfe14: case 0xfe17: case 0xfe1a: case 0xfe1d:
    case 0xfe20: case 0xfe23: case 0xfe26: case 0xfe29:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xfe0e: video_load(c, 0x1648, false); break;
    case 0xfe11: store_register8(c, 0x07bd); break;
    case 0xfe14: video_load(c, 0x01d8, false); break;
    case 0xfe17: store_register8(c, 0x07be); break;
    case 0xfe1a: video_load(c, 0x01d9, false); break;
    case 0xfe1d: store_register8(c, 0x07bf); break;
    case 0xfe20: load_y_abs16(c, 0x01dc); break;
    case 0xfe23: store_y_abs16(c, 0x07c0); break;
    case 0xfe26: stz_abs8(c, 0x01d9); break;
    case 0xfe29: return_long(c); break;
  }
  return true;
}

/* $04:8674–$869D: frame-tail clears + JSLs $0086A0 / $04871E / $048938 / $0489E2;
 * copy DP+$24→$25. */
static bool frame_tail_8674_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x04 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x8674: case 0x8677: case 0x867a: case 0x867d: case 0x8680: case 0x8684:
    case 0x8688: case 0x868c: case 0x8690: case 0x8693: case 0x8696: case 0x8699:
    case 0x869b: case 0x869d:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8674: stz_abs8(c, 0x420c); break;
    case 0x8677: stz_abs8(c, 0x0062); break;
    case 0x867a: index_constant(c, false, 0x0000); break;
    case 0x867d: stx_abs16(c, 0x0055); break;
    case 0x8680: jsl_to(c, 0x86a0); break;
    case 0x8684: jsl_to(c, 0x871e); break;
    case 0x8688: jsl_to(c, 0x8938); break;
    case 0x868c: jsl_to(c, 0x89e2); break;
    case 0x8690: stz_abs8(c, 0x133d); break;
    case 0x8693: stz_abs8(c, 0x0062); break;
    case 0x8696: stz_abs8(c, 0x12dc); break;
    case 0x8699: load_dp8(c, 0x24); break;
    case 0x869b: store_dp(c, 0x25, true); break;
    case 0x869d: return_long(c); break;
  }
  return true;
}


static void stx_dp16(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  c->write(c->mem, address, (uint8_t)c->x); sample_interrupt(c);
  c->write(c->mem, (uint16_t)(address + 1), (uint8_t)(c->x >> 8));
}
static void ldx_dp16(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  uint16_t low = c->read(c->mem, address); sample_interrupt(c);
  c->x = low | ((uint16_t)c->read(c->mem, (uint16_t)(address + 1)) << 8);
  zn(c, c->x, false);
}
static void stz_dp16(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  c->write(c->mem, address, 0); sample_interrupt(c);
  c->write(c->mem, (uint16_t)(address + 1), 0);
}
static void jsr_indirect_abs_x(Cpu *c, uint16_t base) {
  /* LakeSnes 0xFC: fetch low, push PC (at high-byte), fetch high, idle, vector. */
  (void)fetch(c);
  push_byte_native(c, (uint8_t)(c->pc >> 8));
  push_byte_native(c, (uint8_t)c->pc);
  (void)fetch(c);
  idle(c);
  uint16_t table = (uint16_t)(base + c->x);
  uint16_t lo = c->read(c->mem, ((uint32_t)c->k << 16) | table); sample_interrupt(c);
  c->pc = (uint16_t)(lo | ((uint16_t)c->read(c->mem,
      ((uint32_t)c->k << 16) | ((table + 1) & 0xffffu)) << 8));
}

/* $01F983–$F99F: $0E01,Y×2 + $02A597 long-ptr word → DP+$00. */
static bool actor_ptr_f983_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xf991 || pc == 0xf994 || pc == 0xf995 || pc == 0xf997 ||
                    pc == 0xf999 || pc == 0xf99b);
  const bool sep_entry = (pc == 0xf99d);
  const bool rep_entry = (pc == 0xf98f);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xf983: case 0xf986: case 0xf987: case 0xf989: case 0xf98b: case 0xf98d:
    case 0xf98f: case 0xf991: case 0xf994: case 0xf995: case 0xf997: case 0xf999:
    case 0xf99b: case 0xf99d: case 0xf99f:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf983: load_a_abs_y8(c, 0x0e01); break;
    case 0xf986: asl_a8(c); break;
    case 0xf987: store_dp(c, 0x00, true); break;
    case 0xf989: stz_dp8(c, 0x01); break;
    case 0xf98b: byte_constant(c, 0x02); break;
    case 0xf98d: store_dp(c, 0x02, true); break;
    case 0xf98f: sep_rep_native(c, 0x20, false); break;
    case 0xf991: lda16_constant(c, 0xa597); break;
    case 0xf994: implied(c); c->c = false; break;
    case 0xf995: adc_dp16(c, 0x00); break;
    case 0xf997: store_dp(c, 0x00, false); break;
    case 0xf999: lda_indirect_long16(c, 0x00); break;
    case 0xf99b: store_dp(c, 0x00, false); break;
    case 0xf99d: sep_rep_native(c, 0x20, true); break;
    case 0xf99f: return_long(c); break;
  }
  return true;
}

/* $00AB00–$AB2B: after $82FB, latch DP+$3B..$42 into BG3/BG4 scroll regs. */
static bool scroll_latch_ab00_step(Cpu *c, uint16_t pc) {
  if(c->e || c->d || !c->mf) return false;
  switch(pc) {
    case 0xab00: case 0xab03: case 0xab05: case 0xab08: case 0xab0a: case 0xab0d:
    case 0xab0f: case 0xab12: case 0xab14: case 0xab17: case 0xab19: case 0xab1c:
    case 0xab1e: case 0xab21: case 0xab23: case 0xab26: case 0xab28: case 0xab2b:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xab00: jsr_abs_native(c, 0x82fb); break;
    case 0xab03: load_dp8(c, 0x3b); break;
    case 0xab05: store_register8(c, 0x2111); break;
    case 0xab08: load_dp8(c, 0x3c); break;
    case 0xab0a: store_register8(c, 0x2111); break;
    case 0xab0d: load_dp8(c, 0x3d); break;
    case 0xab0f: store_register8(c, 0x2112); break;
    case 0xab12: load_dp8(c, 0x3e); break;
    case 0xab14: store_register8(c, 0x2112); break;
    case 0xab17: load_dp8(c, 0x3f); break;
    case 0xab19: store_register8(c, 0x2113); break;
    case 0xab1c: load_dp8(c, 0x40); break;
    case 0xab1e: store_register8(c, 0x2113); break;
    case 0xab21: load_dp8(c, 0x41); break;
    case 0xab23: store_register8(c, 0x2114); break;
    case 0xab26: load_dp8(c, 0x42); break;
    case 0xab28: store_register8(c, 0x2114); break;
    case 0xab2b: return_short(c); break;
  }
  return true;
}

/* $00B960–$B9A4: Mode-7 HDMA 6/7 setup + CGRAM DMA; RTS. */
static bool mode7_hdma_b960_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xb960: case 0xb963: case 0xb965: case 0xb968: case 0xb96b: case 0xb96d:
    case 0xb970: case 0xb973: case 0xb976: case 0xb979: case 0xb97c: case 0xb97e:
    case 0xb981: case 0xb984: case 0xb986: case 0xb989: case 0xb98c: case 0xb98e:
    case 0xb991: case 0xb994: case 0xb997: case 0xb99a: case 0xb99c: case 0xb99f:
    case 0xb9a1: case 0xb9a4:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb960: stz_abs8(c, 0x4360); break;
    case 0xb963: byte_constant(c, 0x04); break;
    case 0xb965: store_register8(c, 0x4361); break;
    case 0xb968: index_constant(c, false, 0x0000); break;
    case 0xb96b: stx_dp16(c, 0x55); break;
    case 0xb96d: stx_abs16(c, 0x2102); break;
    case 0xb970: index_constant(c, false, 0x0220); break;
    case 0xb973: stx_abs16(c, 0x4365); break;
    case 0xb976: index_constant(c, false, 0x0400); break;
    case 0xb979: stx_abs16(c, 0x4362); break;
    case 0xb97c: byte_constant(c, 0x00); break;
    case 0xb97e: store_register8(c, 0x4364); break;
    case 0xb981: stz_abs8(c, 0x4370); break;
    case 0xb984: byte_constant(c, 0x22); break;
    case 0xb986: store_register8(c, 0x4371); break;
    case 0xb989: index_constant(c, false, 0x0200); break;
    case 0xb98c: byte_constant(c, 0x00); break;
    case 0xb98e: stx_abs16(c, 0x4372); break;
    case 0xb991: store_register8(c, 0x4374); break;
    case 0xb994: index_constant(c, false, 0x0200); break;
    case 0xb997: stx_abs16(c, 0x4375); break;
    case 0xb99a: byte_constant(c, 0x00); break;
    case 0xb99c: store_register8(c, 0x2121); break;
    case 0xb99f: byte_constant(c, 0xc0); break;
    case 0xb9a1: store_register8(c, 0x420b); break;
    case 0xb9a4: return_short(c); break;
  }
  return true;
}

/* $04:8938–$8973: queue seed $0000→$0000/#$8000/$7F; INC $0715; $8324; $049EF2. */
static bool vram_seed_8938_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x04 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x8947 || pc == 0x894a || pc == 0x894d || pc == 0x8950 ||
                    pc == 0x8953 || pc == 0x8956);
  const bool sep_entry = (pc == 0x8959);
  const bool rep_entry = (pc == 0x8945);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0x8938: case 0x893c: case 0x8940: case 0x8942: case 0x8945: case 0x8947:
    case 0x894a: case 0x894d: case 0x8950: case 0x8953: case 0x8956: case 0x8959:
    case 0x895b: case 0x895d: case 0x8960: case 0x8963: case 0x8967: case 0x896a:
    case 0x896c: case 0x896f: case 0x8973:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8938: jsl_to(c, 0x8711); break;
    case 0x893c: jsl_to(c, 0x8724); break;
    case 0x8940: byte_constant(c, 0x80); break;
    case 0x8942: store_a_abs_x8(c, 0x0800); break;
    case 0x8945: sep_rep_native(c, 0x20, false); break;
    case 0x8947: lda16_constant(c, 0x0000); break;
    case 0x894a: store_a_abs_x16(c, 0x0801); break;
    case 0x894d: lda16_constant(c, 0x0000); break;
    case 0x8950: store_a_abs_x16(c, 0x0803); break;
    case 0x8953: lda16_constant(c, 0x8000); break;
    case 0x8956: store_a_abs_x16(c, 0x0806); break;
    case 0x8959: sep_rep_native(c, 0x20, true); break;
    case 0x895b: byte_constant(c, 0x7f); break;
    case 0x895d: store_a_abs_x8(c, 0x0805); break;
    case 0x8960: inc_abs8_native(c, 0x0715); break;
    case 0x8963: jsl_to(c, 0x8324); break;
    case 0x8967: index_constant(c, true, 0xea49); break;
    case 0x896a: byte_constant(c, 0x13); break;
    case 0x896c: index_constant(c, false, 0x7000); break;
    case 0x896f: jsl_to(c, 0x9ef2); break;
    case 0x8973: return_long(c); break;
  }
  return true;
}

/* $04:89E2–$8A4E: Mode-7 PPU preset + $12xx seeds; JSR ($8A4F,X); JSLs. */
static bool mode7_ppu_89e2_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x04 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x8a0d || pc == 0x8a10 || pc == 0x8a13 || pc == 0x8a16 ||
                    pc == 0x8a19 || pc == 0x8a1c || pc == 0x8a1f || pc == 0x8a22 ||
                    pc == 0x8a25 || pc == 0x8a28 || pc == 0x8a2b || pc == 0x8a2e ||
                    pc == 0x8a31 || pc == 0x8a33 || pc == 0x8a36 || pc == 0x8a39 ||
                    pc == 0x8a3a || pc == 0x8a3b);
  const bool sep_entry = (pc == 0x8a3e);
  const bool rep_entry = (pc == 0x8a0b);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0x89e2: case 0x89e5: case 0x89e7: case 0x89ea: case 0x89ec: case 0x89ef:
    case 0x89f1: case 0x89f4: case 0x89f7: case 0x89fa: case 0x89fd: case 0x8a00:
    case 0x8a02: case 0x8a05: case 0x8a08: case 0x8a0b: case 0x8a0d: case 0x8a10:
    case 0x8a13: case 0x8a16: case 0x8a19: case 0x8a1c: case 0x8a1f: case 0x8a22:
    case 0x8a25: case 0x8a28: case 0x8a2b: case 0x8a2e: case 0x8a31: case 0x8a33:
    case 0x8a36: case 0x8a39: case 0x8a3a: case 0x8a3b: case 0x8a3e: case 0x8a40:
    case 0x8a44: case 0x8a48: case 0x8a4b: case 0x8a4e:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x89e2: stz_abs8(c, 0x420c); break;
    case 0x89e5: byte_constant(c, 0x07); break;
    case 0x89e7: store_register8(c, 0x2105); break;
    case 0x89ea: byte_constant(c, 0x03); break;
    case 0x89ec: store_register8(c, 0x2101); break;
    case 0x89ef: byte_constant(c, 0x11); break;
    case 0x89f1: store_register8(c, 0x212c); break;
    case 0x89f4: stz_abs8(c, 0x212d); break;
    case 0x89f7: stz_abs8(c, 0x212f); break;
    case 0x89fa: stz_abs8(c, 0x2124); break;
    case 0x89fd: stz_abs8(c, 0x2131); break;
    case 0x8a00: byte_constant(c, 0xe0); break;
    case 0x8a02: store_register8(c, 0x2132); break;
    case 0x8a05: stz_abs8(c, 0x2126); break;
    case 0x8a08: stz_abs8(c, 0x2127); break;
    case 0x8a0b: sep_rep_native(c, 0x20, false); break;
    case 0x8a0d: stz_abs16(c, 0x120e); break;
    case 0x8a10: stz_abs16(c, 0x1212); break;
    case 0x8a13: stz_abs16(c, 0x1210); break;
    case 0x8a16: stz_abs16(c, 0x1214); break;
    case 0x8a19: stz_abs16(c, 0x1200); break;
    case 0x8a1c: lda16_constant(c, 0x0100); break;
    case 0x8a1f: store_a_abs16(c, 0x1204); break;
    case 0x8a22: store_a_abs16(c, 0x1202); break;
    case 0x8a25: lda16_constant(c, 0x0080); break;
    case 0x8a28: store_a_abs16(c, 0x1216); break;
    case 0x8a2b: lda16_constant(c, 0x0080); break;
    case 0x8a2e: store_a_abs16(c, 0x1218); break;
    case 0x8a31: stz_dp16(c, 0x4f); break;
    case 0x8a33: video_load(c, 0x121c, false); break;
    case 0x8a36: and_constant16(c, 0x000f); break;
    case 0x8a39: asl_a16(c); break;
    case 0x8a3a: tax16(c); break;
    case 0x8a3b: jsr_indirect_abs_x(c, 0x8a4f); break;
    case 0x8a3e: sep_rep_native(c, 0x20, true); break;
    case 0x8a40: jsl_to(c, 0x80a9); break;
    case 0x8a44: jsl_to(c, 0x8000); break;
    case 0x8a48: stz_abs8(c, 0x121d); break;
    case 0x8a4b: jsr_abs_native(c, 0x8ef7); break;
    case 0x8a4e: return_long(c); break;
  }
  return true;
}

/* $01F4B2–$F53A: after $F802 — prep via $F53B, walk slots calling $F983/$FB2A /
 * $9D2A, optional $FBBC, then $FA1A. */
static bool actor_prep_f4b2_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xf4d3 || pc == 0xf4d5 || pc == 0xf4d6 || pc == 0xf4d9 ||
                    pc == 0xf4db || pc == 0xf4dc || pc == 0xf4dd || pc == 0xf4e0 ||
                    pc == 0xf4f9 || pc == 0xf4fc);
  const bool sep_entry = (pc == 0xf4e1 || pc == 0xf4fe);
  const bool rep_entry = (pc == 0xf4d1 || pc == 0xf4f7);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xf4b2: case 0xf4b6: case 0xf4b9: case 0xf4bb: case 0xf4be: case 0xf4c1:
    case 0xf4c3: case 0xf4c5: case 0xf4c6: case 0xf4ca: case 0xf4cc: case 0xf4d0:
    case 0xf4d1: case 0xf4d3: case 0xf4d5: case 0xf4d6: case 0xf4d9: case 0xf4db:
    case 0xf4dc: case 0xf4dd: case 0xf4e0: case 0xf4e1: case 0xf4e3: case 0xf4e6:
    case 0xf4e8: case 0xf4eb: case 0xf4ee: case 0xf4f0: case 0xf4f2: case 0xf4f5:
    case 0xf4f7: case 0xf4f9: case 0xf4fc: case 0xf4fe: case 0xf500: case 0xf503:
    case 0xf505: case 0xf507: case 0xf509: case 0xf50b: case 0xf50d: case 0xf50f:
    case 0xf511: case 0xf513: case 0xf515: case 0xf516: case 0xf51a: case 0xf51b:
    case 0xf51e: case 0xf520: case 0xf523: case 0xf525: case 0xf528: case 0xf52a:
    case 0xf52c: case 0xf530: case 0xf533: case 0xf536: case 0xf53a:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf4b2: jsl_to(c, 0xf53b); break;
    case 0xf4b6: index_constant(c, false, 0x0000); break;
    case 0xf4b9: stx_dp16(c, 0x20); break;
    case 0xf4bb: index_constant(c, true, 0x0000); break;
    case 0xf4be: load_a_abs_y8(c, 0x0e00); break;
    case 0xf4c1: and_constant8(c, 0x40); break;
    case 0xf4c3: branch_to(c, c->z, 0xf4d1); break;
    case 0xf4c5: phy16(c); break;
    case 0xf4c6: jsl_to(c, 0xf983); break;
    case 0xf4ca: ldx_dp16(c, 0x20); break;
    case 0xf4cc: jsl_to(c, 0xfb2a); break;
    case 0xf4d0: ply16(c); break;
    case 0xf4d1: sep_rep_native(c, 0x20, false); break;
    case 0xf4d3: load_dp16_to_a(c, 0x20); break;
    case 0xf4d5: implied(c); c->c = false; break;
    case 0xf4d6: adc16_constant(c, 0x0100); break;
    case 0xf4d9: store_dp(c, 0x20, false); break;
    case 0xf4db: tya16(c); break;
    case 0xf4dc: implied(c); c->c = false; break;
    case 0xf4dd: adc16_constant(c, 0x0020); break;
    case 0xf4e0: tay16(c); break;
    case 0xf4e1: sep_rep_native(c, 0x20, true); break;
    case 0xf4e3: cpy16_constant(c, 0x0060); break;
    case 0xf4e6: branch_to(c, !c->c, 0xf4be); break;
    case 0xf4e8: index_constant(c, true, 0x0060); break;
    case 0xf4eb: load_a_abs_y8(c, 0x0e00); break;
    case 0xf4ee: and_constant8(c, 0x40); break;
    case 0xf4f0: branch_to(c, c->z, 0xf51b); break;
    case 0xf4f2: load_a_abs_y8(c, 0x0e01); break;
    case 0xf4f5: store_dp(c, 0x00, true); break;
    case 0xf4f7: sep_rep_native(c, 0x20, false); break;
    case 0xf4f9: lda_abs_y16(c, 0x0e0f); break;
    case 0xf4fc: store_dp(c, 0x10, false); break;
    case 0xf4fe: sep_rep_native(c, 0x20, true); break;
    case 0xf500: cpy16_constant(c, 0x0060); break;
    case 0xf503: branch_to(c, c->z, 0xf50d); break;
    case 0xf505: byte_constant(c, 0x80); break;
    case 0xf507: store_dp(c, 0x0e, true); break;
    case 0xf509: byte_constant(c, 0x0c); break;
    case 0xf50b: branch_to(c, true, 0xf513); break;
    case 0xf50d: byte_constant(c, 0x40); break;
    case 0xf50f: store_dp(c, 0x0e, true); break;
    case 0xf511: byte_constant(c, 0x08); break;
    case 0xf513: store_dp(c, 0x0f, true); break;
    case 0xf515: phy16(c); break;
    case 0xf516: jsl_to(c, 0x9d2a); break;
    case 0xf51a: ply16(c); break;
    case 0xf51b: cpy16_constant(c, 0x0080); break;
    case 0xf51e: branch_to(c, c->z, 0xf525); break;
    case 0xf520: index_constant(c, true, 0x0080); break;
    case 0xf523: branch_to(c, true, 0xf4eb); break;
    case 0xf525: video_load(c, 0x01bc, false); break;
    case 0xf528: and_constant8(c, 0x0f); break;
    case 0xf52a: branch_to(c, !c->z, 0xf536); break;
    case 0xf52c: jsl_to(c, 0xfbbc); break;
    case 0xf530: stz_abs8(c, 0x0242); break;
    case 0xf533: stz_abs8(c, 0x0243); break;
    case 0xf536: jsl_to(c, 0xfa1a); break;
    case 0xf53a: return_long(c); break;
  }
  return true;
}


/* $01FB2A–$FBB2: expand [DP+$00] records into $7E5000,X OAM-like words via
 * $01FBB3 nibble table; terminate with #$80. */
static bool actor_expand_fb2a_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xfb36 || pc == 0xfb38 || pc == 0xfb3a || pc == 0xfb3c ||
                    pc == 0xfb3d);
  const bool sep_entry = (pc == 0xfb3e);
  const bool rep_entry = (pc == 0xfb34);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xfb2a: case 0xfb2d: case 0xfb2f: case 0xfb33: case 0xfb34: case 0xfb36:
    case 0xfb38: case 0xfb3a: case 0xfb3c: case 0xfb3d: case 0xfb3e: case 0xfb40:
    case 0xfb41: case 0xfb43: case 0xfb44: case 0xfb46: case 0xfb47: case 0xfb48:
    case 0xfb4c: case 0xfb4e: case 0xfb50: case 0xfb52: case 0xfb53: case 0xfb55:
    case 0xfb56: case 0xfb57: case 0xfb5b: case 0xfb5d: case 0xfb5f: case 0xfb60:
    case 0xfb62: case 0xfb66: case 0xfb68: case 0xfb6a: case 0xfb6c: case 0xfb70:
    case 0xfb72: case 0xfb76: case 0xfb78: case 0xfb7c: case 0xfb7d: case 0xfb7e:
    case 0xfb7f: case 0xfb80: case 0xfb81: case 0xfb83: case 0xfb84: case 0xfb86:
    case 0xfb88: case 0xfb8a: case 0xfb8c: case 0xfb8e: case 0xfb90: case 0xfb91:
    case 0xfb93: case 0xfb94: case 0xfb96: case 0xfb97: case 0xfb98: case 0xfb9c:
    case 0xfb9e: case 0xfba0: case 0xfba1: case 0xfba3: case 0xfba4: case 0xfba6:
    case 0xfba8: case 0xfbaa: case 0xfbac: case 0xfbae: case 0xfbb2:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xfb2a: index_constant(c, true, 0x0000); break;
    case 0xfb2d: byte_constant(c, 0x00); break;
    case 0xfb2f: store_a_long_x8(c, 0x7e5000); break;
    case 0xfb33: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xfb34: sep_rep_native(c, 0x20, false); break;
    case 0xfb36: lda_indirect_long_y16(c, 0x00); break;
    case 0xfb38: store_dp(c, 0x03, false); break;
    case 0xfb3a: store_dp(c, 0x05, false); break;
    case 0xfb3c: case 0xfb3d: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xfb3e: sep_rep_native(c, 0x20, true); break;
    case 0xfb40: phx16(c); break;
    case 0xfb41: byte_constant(c, 0x00); break;
    case 0xfb43: xba_native(c); break;
    case 0xfb44: load_dp8(c, 0x04); break;
    case 0xfb46: lsr_a8(c); break;
    case 0xfb47: tax16_from_a8(c); break;
    case 0xfb48: load_a_long_x8(c, 0x01fbb3); break;
    case 0xfb4c: store_dp(c, 0x08, true); break;
    case 0xfb4e: dec_dp8(c, 0x04); break;
    case 0xfb50: byte_constant(c, 0x00); break;
    case 0xfb52: xba_native(c); break;
    case 0xfb53: load_dp8(c, 0x03); break;
    case 0xfb55: lsr_a8(c); break;
    case 0xfb56: tax16_from_a8(c); break;
    case 0xfb57: load_a_long_x8(c, 0x01fbb3); break;
    case 0xfb5b: store_dp(c, 0x07, true); break;
    case 0xfb5d: dec_dp8(c, 0x03); break;
    case 0xfb5f: plx16(c); break;
    case 0xfb60: load_dp8(c, 0x08); break;
    case 0xfb62: store_a_long_x8(c, 0x7e5000); break;
    case 0xfb66: lda_indirect_long_y8(c, 0x00); break;
    case 0xfb68: cmp_constant8(c, 0xff); break;
    case 0xfb6a: branch_to(c, c->z, 0xfb80); break;
    case 0xfb6c: store_a_long_x8(c, 0x7e5001); break;
    case 0xfb70: byte_constant(c, 0x30); break;
    case 0xfb72: store_a_long_x8(c, 0x7e5002); break;
    case 0xfb76: load_dp8(c, 0x07); break;
    case 0xfb78: store_a_long_x8(c, 0x7e5003); break;
    case 0xfb7c: case 0xfb7d: case 0xfb7e: case 0xfb7f:
      implied(c); c->x++; zn(c, c->x, false); break;
    case 0xfb80: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xfb81: load_dp8(c, 0x07); break;
    case 0xfb83: implied(c); c->c = false; break;
    case 0xfb84: adc8_constant(c, 0x08); break;
    case 0xfb86: store_dp(c, 0x07, true); break;
    case 0xfb88: dec_dp8(c, 0x03); break;
    case 0xfb8a: branch_to(c, !c->n, 0xfb60); break;
    case 0xfb8c: load_dp8(c, 0x05); break;
    case 0xfb8e: store_dp(c, 0x03, true); break;
    case 0xfb90: phx16(c); break;
    case 0xfb91: byte_constant(c, 0x00); break;
    case 0xfb93: xba_native(c); break;
    case 0xfb94: load_dp8(c, 0x03); break;
    case 0xfb96: lsr_a8(c); break;
    case 0xfb97: tax16_from_a8(c); break;
    case 0xfb98: load_a_long_x8(c, 0x01fbb3); break;
    case 0xfb9c: store_dp(c, 0x07, true); break;
    case 0xfb9e: dec_dp8(c, 0x03); break;
    case 0xfba0: plx16(c); break;
    case 0xfba1: load_dp8(c, 0x08); break;
    case 0xfba3: implied(c); c->c = false; break;
    case 0xfba4: adc8_constant(c, 0x08); break;
    case 0xfba6: store_dp(c, 0x08, true); break;
    case 0xfba8: dec_dp8(c, 0x04); break;
    case 0xfbaa: branch_to(c, !c->n, 0xfb60); break;
    case 0xfbac: byte_constant(c, 0x80); break;
    case 0xfbae: store_a_long_x8(c, 0x7e5000); break;
    case 0xfbb2: return_long(c); break;
  }
  return true;
}

/* $01B72B–$B7B9: dispatch on $0D66 via $02E0F2 table into $B55A / typed seeds. */
static bool table_dispatch_b72b_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xb737 || pc == 0xb738 || pc == 0xb739 || pc == 0xb73a ||
                    pc == 0xb73e || pc == 0xb740 || pc == 0xb742 || pc == 0xb743 ||
                    pc == 0xb744 || pc == 0xb748 || pc == 0xb751 || pc == 0xb753 ||
                    pc == 0xb754 || pc == 0xb755 || pc == 0xb759);
  const bool sep_entry = (pc == 0xb74a || pc == 0xb75b);
  const bool rep_entry = (pc == 0xb735);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xb72b: case 0xb72d: case 0xb72e: case 0xb731: case 0xb733: case 0xb735:
    case 0xb737: case 0xb738: case 0xb739: case 0xb73a: case 0xb73e: case 0xb740:
    case 0xb742: case 0xb743: case 0xb744: case 0xb748: case 0xb74a: case 0xb74c:
    case 0xb750: case 0xb751: case 0xb753: case 0xb754: case 0xb755: case 0xb759:
    case 0xb75b: case 0xb75d: case 0xb760: case 0xb762: case 0xb764: case 0xb766:
    case 0xb768: case 0xb76a: case 0xb76c: case 0xb76e: case 0xb770: case 0xb771:
    case 0xb773: case 0xb775: case 0xb778: case 0xb779: case 0xb77a: case 0xb77b:
    case 0xb77d: case 0xb781: case 0xb782: case 0xb784: case 0xb786: case 0xb789:
    case 0xb78b: case 0xb78d: case 0xb78f: case 0xb792: case 0xb793: case 0xb795:
    case 0xb797: case 0xb799: case 0xb79c: case 0xb79e: case 0xb7a1: case 0xb7a3:
    case 0xb7a5: case 0xb7a7: case 0xb7a9: case 0xb7ac: case 0xb7ad: case 0xb7af:
    case 0xb7b1: case 0xb7b3: case 0xb7b5: case 0xb7b7:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb72b: byte_constant(c, 0x00); break;
    case 0xb72d: xba_native(c); break;
    case 0xb72e: video_load(c, 0x0d66, false); break;
    case 0xb731: cmp_constant8(c, 0x29); break;
    case 0xb733: branch_to(c, c->z, 0xb781); break;
    case 0xb735: sep_rep_native(c, 0x20, false); break;
    case 0xb737: case 0xb738: asl_a16(c); break;
    case 0xb739: tax16(c); break;
    case 0xb73a: lda_long_x16(c, 0x02e0f2); break;
    case 0xb73e: branch_to(c, c->n, 0xb751); break;
    case 0xb740: store_dp(c, 0x00, false); break;
    case 0xb742: case 0xb743: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xb744: lda_long_x16(c, 0x02e0f2); break;
    case 0xb748: store_dp(c, 0x02, false); break;
    case 0xb74a: sep_rep_native(c, 0x20, true); break;
    case 0xb74c: jsl_to(c, 0xb55a); break;
    case 0xb750: return_long(c); break;
    case 0xb751: sep_rep_native(c, 0x20, false); break;
    case 0xb753: case 0xb754: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xb755: lda_long_x16(c, 0x02e0f2); break;
    case 0xb759: store_dp(c, 0x02, false); break;
    case 0xb75b: sep_rep_native(c, 0x20, true); break;
    case 0xb75d: video_load(c, 0x0d66, false); break;
    case 0xb760: cmp_constant8(c, 0x02); break;
    case 0xb762: branch_to(c, c->z, 0xb771); break;
    case 0xb764: cmp_constant8(c, 0x03); break;
    case 0xb766: branch_to(c, c->z, 0xb771); break;
    case 0xb768: cmp_constant8(c, 0x0b); break;
    case 0xb76a: branch_to(c, c->z, 0xb782); break;
    case 0xb76c: cmp_constant8(c, 0x14); break;
    case 0xb76e: branch_to(c, c->z, 0xb7b5); break;
    case 0xb770: return_long(c); break;
    case 0xb771: byte_constant(c, 0x09); break;
    case 0xb773: store_dp(c, 0x00, true); break;
    case 0xb775: video_load(c, 0x01bb, false); break;
    case 0xb778: asl_a8(c); break;
    case 0xb779: case 0xb77a: inc_a8(c); break;
    case 0xb77b: store_dp(c, 0x01, true); break;
    case 0xb77d: jsl_to(c, 0xb55a); break;
    case 0xb781: return_long(c); break;
    case 0xb782: byte_constant(c, 0x0a); break;
    case 0xb784: store_dp(c, 0x00, true); break;
    case 0xb786: video_load(c, 0x0d63, false); break;
    case 0xb789: branch_to(c, c->z, 0xb79e); break;
    case 0xb78b: cmp_constant8(c, 0x01); break;
    case 0xb78d: branch_to(c, c->z, 0xb7a9); break;
    case 0xb78f: video_load(c, 0x01b2, false); break;
    case 0xb792: implied(c); c->c = true; break;
    case 0xb793: sbc8_constant(c, 0x18); break;
    case 0xb795: branch_to(c, c->c, 0xb7a5); break;
    case 0xb797: adc8_constant(c, 0x08); break;
    case 0xb799: store_register8(c, 0x01b3); break;
    case 0xb79c: branch_to(c, true, 0xb778); break;
    case 0xb79e: video_load(c, 0x01b2, false); break;
    case 0xb7a1: cmp_constant8(c, 0x08); break;
    case 0xb7a3: branch_to(c, !c->c, 0xb799); break;
    case 0xb7a5: byte_constant(c, 0x08); break;
    case 0xb7a7: branch_to(c, true, 0xb799); break;
    case 0xb7a9: video_load(c, 0x01b2, false); break;
    case 0xb7ac: implied(c); c->c = true; break;
    case 0xb7ad: sbc8_constant(c, 0x10); break;
    case 0xb7af: branch_to(c, c->c, 0xb7a5); break;
    case 0xb7b1: adc8_constant(c, 0x08); break;
    case 0xb7b3: branch_to(c, true, 0xb799); break;
    case 0xb7b5: byte_constant(c, 0x12); break;
    case 0xb7b7: jmp_abs_native(c, 0xb773); break;
  }
  return true;
}

static void and_dp8(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  sample_interrupt(c);
  set_a8(c, (uint8_t)((uint8_t)c->a & c->read(c->mem, address)));
}

/* $01C9F9-CA97 actor gfx stream; alt CA34; helpers CA98-CAAA and CAAB-CACA. */
static bool actor_gfx_c9f9_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 =
      (pc == 0xc9fb || pc == 0xc9ff ||
       pc == 0xca0d || pc == 0xca0f || pc == 0xca10 || pc == 0xca11 ||
       pc == 0xca12 || pc == 0xca13 || pc == 0xca17 || pc == 0xca19 ||
       pc == 0xca1b || pc == 0xca1e ||
       pc == 0xca36 || pc == 0xca3a ||
       pc == 0xca48 || pc == 0xca4a || pc == 0xca4b || pc == 0xca4c ||
       pc == 0xca4d || pc == 0xca4e || pc == 0xca52 ||
       pc == 0xca7c || pc == 0xca7f || pc == 0xca80 || pc == 0xca83 ||
       pc == 0xca86 || pc == 0xca88 || pc == 0xca89 || pc == 0xca8c ||
       pc == 0xcab2 || pc == 0xcab4 || pc == 0xcab7 || pc == 0xcaba ||
       pc == 0xcabd || pc == 0xcac0);
  const bool sep_entry =
      (pc == 0xca02 || pc == 0xca1f || pc == 0xca3d || pc == 0xca54 ||
       pc == 0xca8e || pc == 0xcac3);
  const bool rep_entry =
      (pc == 0xc9f9 || pc == 0xca0b || pc == 0xca34 || pc == 0xca46 ||
       pc == 0xca7a || pc == 0xcab0);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xc9f9: case 0xc9fb: case 0xc9ff: case 0xca02: case 0xca04: case 0xca08:
    case 0xca0b: case 0xca0d: case 0xca0f: case 0xca10: case 0xca11: case 0xca12:
    case 0xca13: case 0xca17: case 0xca19: case 0xca1b: case 0xca1e: case 0xca1f:
    case 0xca21: case 0xca25: case 0xca27: case 0xca29: case 0xca2b: case 0xca2d:
    case 0xca2f: case 0xca31: case 0xca34: case 0xca36: case 0xca3a: case 0xca3d:
    case 0xca3f: case 0xca43: case 0xca46: case 0xca48: case 0xca4a: case 0xca4b:
    case 0xca4c: case 0xca4d: case 0xca4e: case 0xca52: case 0xca54: case 0xca56:
    case 0xca59: case 0xca5c: case 0xca60: case 0xca63: case 0xca65: case 0xca67:
    case 0xca6a: case 0xca6c: case 0xca6f: case 0xca72: case 0xca76: case 0xca7a:
    case 0xca7c: case 0xca7f: case 0xca80: case 0xca83: case 0xca86: case 0xca88:
    case 0xca89: case 0xca8c: case 0xca8e: case 0xca90: case 0xca94: case 0xca97:
    case 0xca98: case 0xca9b: case 0xca9d: case 0xcaa0: case 0xcaa2: case 0xcaa6:
    case 0xcaaa: case 0xcaab: case 0xcaad: case 0xcab0: case 0xcab2: case 0xcab4:
    case 0xcab7: case 0xcaba: case 0xcabd: case 0xcac0: case 0xcac3: case 0xcac5:
    case 0xcac7: case 0xcaca:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xc9f9: case 0xca0b: case 0xca34: case 0xca46: case 0xca7a: case 0xcab0:
      sep_rep_native(c, 0x20, false); break;
    case 0xca02: case 0xca1f: case 0xca3d: case 0xca54: case 0xca8e: case 0xcac3:
      sep_rep_native(c, 0x20, true); break;
    case 0xc9fb: lda_long_x16(c, 0x02a3b4); break;
    case 0xc9ff: case 0xca3a: store_a_abs16(c, 0x0d77); break;
    case 0xca04: load_a_long_x8(c, 0x02a3b6); break;
    case 0xca08: case 0xca43: store_register8(c, 0x0d79); break;
    case 0xca0d: case 0xca19: case 0xca48: load_dp16_to_a(c, 0x12); break;
    case 0xca0f: case 0xca10: case 0xca11: case 0xca4a: case 0xca4b: case 0xca4c:
      lsr_a16(c); break;
    case 0xca12: case 0xca1e: case 0xca4d: tax16(c); break;
    case 0xca13: lda_long_x16(c, 0x02a57c); break;
    case 0xca17: store_dp(c, 0x10, false); break;
    case 0xca1b: and_constant16(c, 0x0007); break;
    case 0xca21: load_a_long_x8(c, 0x039b9b); break;
    case 0xca25: store_dp(c, 0x14, true); break;
    case 0xca27: load_dp8(c, 0x10); break;
    case 0xca29: and_dp8(c, 0x14); break;
    case 0xca2b: branch_to(c, c->z, 0xca2f); break;
    case 0xca2d: byte_constant(c, 0x01); break;
    case 0xca2f: store_dp(c, 0x03, true); break;
    case 0xca31: jmp_abs_native(c, 0xca56); break;
    case 0xca36: lda_long_x16(c, 0x02a552); break;
    case 0xca3f: load_a_long_x8(c, 0x02a554); break;
    case 0xca4e: lda_long_x16(c, 0x02a595); break;
    case 0xca52: branch_to(c, true, 0xca17); break;
    case 0xca56: stz_abs8(c, 0x0800); break;
    case 0xca59: stz_abs8(c, 0x0808); break;
    case 0xca5c: jsl_to(c, 0xca98); break;
    case 0xca60: index_constant(c, false, 0x0000); break;
    case 0xca63: load_dp8(c, 0x03); break;
    case 0xca65: branch_to(c, c->z, 0xca6c); break;
    case 0xca67: index_constant(c, true, 0x9100); break;
    case 0xca6a: branch_to(c, true, 0xca6f); break;
    case 0xca6c: index_constant(c, true, 0x9000); break;
    case 0xca6f: store_y_abs16(c, 0x0d77); break;
    case 0xca72: case 0xca90: jsl_to(c, 0xcaab); break;
    case 0xca76: jsl_to(c, 0x8711); break;
    case 0xca7c: case 0xcab7: video_load(c, 0x0d77, false); break;
    case 0xca7f: case 0xca88: implied(c); c->c = false; break;
    case 0xca80: adc16_constant(c, 0x0200); break;
    case 0xca83: store_a_abs16(c, 0x0d77); break;
    case 0xca86: case 0xcab2: load_dp16_to_a(c, 0x00); break;
    case 0xca89: adc16_constant(c, 0x0100); break;
    case 0xca8c: store_dp(c, 0x00, false); break;
    case 0xca94: stz_abs8(c, 0x0d75); break;
    case 0xca97: case 0xcaaa: case 0xcaca: return_long(c); break;
    case 0xca98: load_y_abs16(c, 0x0d77); break;
    case 0xca9b: sty_dp16(c, 0x83); break;
    case 0xca9d: video_load(c, 0x0d79, false); break;
    case 0xcaa0: store_dp(c, 0x85, true); break;
    case 0xcaa2: jsl_to(c, 0xc559); break;
    case 0xcaa6: jsl_to(c, 0xc68c); break;
    case 0xcaab: byte_constant(c, 0x80); break;
    case 0xcaad: store_a_abs_x8(c, 0x0800); break;
    case 0xcab4: store_a_abs_x16(c, 0x0801); break;
    case 0xcaba: store_a_abs_x16(c, 0x0803); break;
    case 0xcabd: lda16_constant(c, 0x0100); break;
    case 0xcac0: store_a_abs_x16(c, 0x0806); break;
    case 0xcac5: byte_constant(c, 0x7e); break;
    case 0xcac7: store_a_abs_x8(c, 0x0805); break;
  }
  return true;
}

/* $01F660–$F67A: type gate — SEC if $0E01,Y ∈ {$10,$11,$4C,$4D,$4F}, else CLC. */
static bool type_gate_f660_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xf660: case 0xf663: case 0xf665: case 0xf667: case 0xf669: case 0xf66b:
    case 0xf66d: case 0xf66f: case 0xf671: case 0xf673: case 0xf675: case 0xf677:
    case 0xf678: case 0xf679: case 0xf67a:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf660: load_a_abs_y8(c, 0x0e01); break;
    case 0xf663: cmp_constant8(c, 0x10); break;
    case 0xf665: branch_to(c, c->z, 0xf679); break;
    case 0xf667: cmp_constant8(c, 0x11); break;
    case 0xf669: branch_to(c, c->z, 0xf679); break;
    case 0xf66b: cmp_constant8(c, 0x4c); break;
    case 0xf66d: branch_to(c, c->z, 0xf679); break;
    case 0xf66f: cmp_constant8(c, 0x4d); break;
    case 0xf671: branch_to(c, c->z, 0xf679); break;
    case 0xf673: cmp_constant8(c, 0x4f); break;
    case 0xf675: branch_to(c, c->z, 0xf679); break;
    case 0xf677: implied(c); c->c = false; break;
    case 0xf678: case 0xf67a: return_long(c); break;
    case 0xf679: implied(c); c->c = true; break;
  }
  return true;
}

/* $01F67B–$F691: seed DP+$83=#$EB84 / +$85=#$08; VMAIN=$80; JSL $C559/$C68C. */
static bool decompress_setup_f67b_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xf67b: case 0xf67e: case 0xf680: case 0xf682: case 0xf684: case 0xf686:
    case 0xf689: case 0xf68d: case 0xf691:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf67b: index_constant(c, true, 0xeb84); break;
    case 0xf67e: sty_dp16(c, 0x83); break;
    case 0xf680: byte_constant(c, 0x08); break;
    case 0xf682: store_dp(c, 0x85, true); break;
    case 0xf684: byte_constant(c, 0x80); break;
    case 0xf686: store_register8(c, 0x2115); break;
    case 0xf689: jsl_to(c, 0xc559); break;
    case 0xf68d: jsl_to(c, 0xc68c); break;
    case 0xf691: return_long(c); break;
  }
  return true;
}

/* $04:8974–$89E1: VRAM-queue sibling after $8938 — gate on $122A, seed queue from
 * counter, optional $01B67A remap when $122A==$10. */
static bool vram_queue_8974_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x04 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x8994 || pc == 0x8997 || pc == 0x899a || pc == 0x899b ||
                    pc == 0x899c || pc == 0x899d || pc == 0x89a0 || pc == 0x89a1 ||
                    pc == 0x89a4 || pc == 0x89a7 || pc == 0x89ce || pc == 0x89d1 ||
                    pc == 0x89d4 || pc == 0x89d6 || pc == 0x89d9);
  const bool sep_entry = (pc == 0x89aa || pc == 0x89dc);
  const bool rep_entry = (pc == 0x8992 || pc == 0x89cc);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0x8974: case 0x8977: case 0x8979: case 0x897b: case 0x897d: case 0x897f:
    case 0x8981: case 0x8984: case 0x8985: case 0x8989: case 0x898d: case 0x898f:
    case 0x8992: case 0x8994: case 0x8997: case 0x899a: case 0x899b: case 0x899c:
    case 0x899d: case 0x89a0: case 0x89a1: case 0x89a4: case 0x89a7: case 0x89aa:
    case 0x89ac: case 0x89ae: case 0x89b1: case 0x89b4: case 0x89b7: case 0x89b9:
    case 0x89bb: case 0x89be: case 0x89c0: case 0x89c2: case 0x89c4: case 0x89c7:
    case 0x89c8: case 0x89cc: case 0x89ce: case 0x89d1: case 0x89d4: case 0x89d6:
    case 0x89d9: case 0x89dc: case 0x89de: case 0x89e1:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8974: video_load(c, 0x122a, false); break;
    case 0x8977: branch_to(c, c->n, 0x8984); break;
    case 0x8979: cmp_constant8(c, 0x10); break;
    case 0x897b: branch_to(c, !c->c, 0x8985); break;
    case 0x897d: branch_to(c, c->z, 0x89c8); break;
    case 0x897f: byte_constant(c, 0x80); break;
    case 0x8981: store_register8(c, 0x122a); break;
    case 0x8984: case 0x89c7: case 0x89e1: return_long(c); break;
    case 0x8985: jsl_to(c, 0x8711); break;
    case 0x8989: jsl_to(c, 0x8724); break;
    case 0x898d: byte_constant(c, 0x80); break;
    case 0x898f: store_a_abs_x8(c, 0x0800); break;
    case 0x8992: case 0x89cc: sep_rep_native(c, 0x20, false); break;
    case 0x8994: video_load(c, 0x122a, false); break;
    case 0x8997: and_constant16(c, 0x001f); break;
    case 0x899a: xba_native(c); break;
    case 0x899b: case 0x899c: case 0x89a0: asl_a16(c); break;
    case 0x899d: store_a_abs_x16(c, 0x0801); break;
    case 0x89a1: store_a_abs_x16(c, 0x0803); break;
    case 0x89a4: lda16_constant(c, 0x0800); break;
    case 0x89a7: store_a_abs_x16(c, 0x0806); break;
    case 0x89aa: case 0x89dc: sep_rep_native(c, 0x20, true); break;
    case 0x89ac: byte_constant(c, 0x7f); break;
    case 0x89ae: store_a_abs_x8(c, 0x0805); break;
    case 0x89b1: case 0x89de: inc_abs8_native(c, 0x122a); break;
    case 0x89b4: video_load(c, 0x122a, false); break;
    case 0x89b7: cmp_constant8(c, 0x02); break;
    case 0x89b9: branch_to(c, !c->z, 0x89be); break;
    case 0x89bb: inc_abs8_native(c, 0x0715); break;
    case 0x89be: cmp_constant8(c, 0x04); break;
    case 0x89c0: branch_to(c, !c->z, 0x89c7); break;
    case 0x89c2: byte_constant(c, 0x11); break;
    case 0x89c4: store_register8(c, 0x212c); break;
    case 0x89c8: jsl_to(c, 0xb67a); break;
    case 0x89ce: video_load(c, 0x0801, true); break;
    case 0x89d1: cmp_constant16(c, 0x2c00); break;
    case 0x89d4: branch_to(c, !c->z, 0x8985); break;
    case 0x89d6: lda16_constant(c, 0x5c00); break;
    case 0x89d9: store_a_abs_x16(c, 0x0801); break;
  }
  return true;
}

/* $01F53B–$F65F: first JSL from $F4B2 — dual $F67B DMA, optional $F660 type path,
 * else bulk $7F0000 copy + three DMA bursts, then $0E0F table fill via multiply. */
static bool actor_prep_f53b_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xf64d || pc == 0xf651);
  const bool sep_entry = false; /* F654 accepts either M width */
  const bool rep_entry = (pc == 0xf64b);
  const bool either = (pc == 0xf654);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !either && !c->mf) return false;
  switch(pc) {
    case 0xf53b: case 0xf53f: case 0xf542: case 0xf545: case 0xf549: case 0xf54d:
    case 0xf550: case 0xf553: case 0xf557: case 0xf55a: case 0xf55e: case 0xf560:
    case 0xf563: case 0xf566: case 0xf569: case 0xf56b: case 0xf56e: case 0xf572:
    case 0xf575: case 0xf577: case 0xf579: case 0xf57b: case 0xf57f: case 0xf583:
    case 0xf586: case 0xf589: case 0xf58c: case 0xf58f: case 0xf591: case 0xf593:
    case 0xf596: case 0xf59a: case 0xf59b: case 0xf59f: case 0xf5a1: case 0xf5a3:
    case 0xf5a7: case 0xf5aa: case 0xf5ad: case 0xf5b0: case 0xf5b3: case 0xf5b6:
    case 0xf5b9: case 0xf5bb: case 0xf5be: case 0xf5c1: case 0xf5c4: case 0xf5c6:
    case 0xf5c9: case 0xf5cc: case 0xf5cf: case 0xf5d2: case 0xf5d5: case 0xf5d8:
    case 0xf5db: case 0xf5dd: case 0xf5e0: case 0xf5e3: case 0xf5e6: case 0xf5e8:
    case 0xf5eb: case 0xf5ee: case 0xf5f1: case 0xf5f4: case 0xf5f7: case 0xf5fa:
    case 0xf5fd: case 0xf5ff: case 0xf602: case 0xf605: case 0xf608: case 0xf60a:
    case 0xf60d: case 0xf611: case 0xf614: case 0xf615: case 0xf616: case 0xf617:
    case 0xf618: case 0xf619: case 0xf61a: case 0xf61c: case 0xf61e: case 0xf621:
    case 0xf623: case 0xf625: case 0xf627: case 0xf628: case 0xf62b: case 0xf62c:
    case 0xf630: case 0xf631: case 0xf632: case 0xf633: case 0xf635: case 0xf638:
    case 0xf63a: case 0xf63d: case 0xf63e: case 0xf63f: case 0xf640: case 0xf643:
    case 0xf645: case 0xf646: case 0xf647: case 0xf648: case 0xf64a: case 0xf64b:
    case 0xf64d: case 0xf651: case 0xf654: case 0xf656: case 0xf65a: case 0xf65d:
    case 0xf65f:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf53b: case 0xf549: jsl_to(c, 0xf67b); break;
    case 0xf53f: index_constant(c, true, 0x0000); break;
    case 0xf542: case 0xf550: case 0xf5aa: case 0xf5cc: case 0xf5ee:
      store_y_abs16(c, 0x2116); break;
    case 0xf545: case 0xf553: jsl_to(c, 0x90e6); break;
    case 0xf54d: index_constant(c, true, 0x5000); break;
    case 0xf557: case 0xf589: case 0xf5b3: case 0xf611:
      index_constant(c, true, 0x0000); break;
    case 0xf55a: jsl_to(c, 0xf660); break;
    case 0xf55e: branch_to(c, !c->c, 0xf586); break;
    case 0xf560: store_register8(c, 0x0d01); break;
    case 0xf563: index_constant(c, true, 0x0600); break;
    case 0xf566: store_y_abs16(c, 0x0d7a); break;
    case 0xf569: byte_constant(c, 0xa0); break;
    case 0xf56b: store_register8(c, 0x0d75); break;
    case 0xf56e: jsl_to(c, 0xc987); break;
    case 0xf572: video_load(c, 0x0d01, false); break;
    case 0xf575: store_dp(c, 0x04, true); break;
    case 0xf577: byte_constant(c, 0x02); break;
    case 0xf579: store_dp(c, 0x03, true); break;
    case 0xf57b: jsl_to(c, 0xc8c8); break;
    case 0xf57f: jsl_to(c, 0x8324); break;
    case 0xf583: jmp_abs_native(c, 0xf611); break;
    case 0xf586: index_constant(c, false, 0x0000); break;
    case 0xf58c: case 0xf61e: load_a_abs_y8(c, 0x0e00); break;
    case 0xf58f: case 0xf621: and_constant8(c, 0x40); break;
    case 0xf591: branch_to(c, c->z, 0xf596); break;
    case 0xf593: case 0xf628: load_a_abs_y8(c, 0x0e01); break;
    case 0xf596: store_a_long_x8(c, 0x7f0000); break;
    case 0xf59a: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xf59b: case 0xf656: jsl_to(c, 0x8af1); break;
    case 0xf59f: cmp_constant8(c, 0xa0); break;
    case 0xf5a1: branch_to(c, !c->c, 0xf58c); break;
    case 0xf5a3: jsl_to(c, 0xbe54); break; /* bank 5 — native BE54 */
    case 0xf5a7: index_constant(c, true, 0x0410); break;
    case 0xf5ad: case 0xf5cf: case 0xf5f1: index_constant(c, true, 0x1801); break;
    case 0xf5b0: case 0xf5d2: case 0xf5f4: store_y_abs16(c, 0x4300); break;
    case 0xf5b6: store_y_abs16(c, 0x4302); break;
    case 0xf5b9: case 0xf5db: case 0xf5fd: byte_constant(c, 0x7f); break;
    case 0xf5bb: case 0xf5dd: case 0xf5ff: store_register8(c, 0x4304); break;
    case 0xf5be: index_constant(c, true, 0x0800); break;
    case 0xf5c1: case 0xf5e3: case 0xf605: store_y_abs16(c, 0x4305); break;
    case 0xf5c4: case 0xf5e6: case 0xf608: byte_constant(c, 0x01); break;
    case 0xf5c6: case 0xf5e8: case 0xf60a: store_register8(c, 0x420b); break;
    case 0xf5c9: index_constant(c, true, 0x0800); break;
    case 0xf5d5: index_constant(c, true, 0x0800); break;
    case 0xf5d8: store_y_abs16(c, 0x4302); break;
    case 0xf5e0: index_constant(c, true, 0x1000); break;
    case 0xf5eb: index_constant(c, true, 0x5400); break;
    case 0xf5f7: index_constant(c, true, 0x1800); break;
    case 0xf5fa: store_y_abs16(c, 0x4302); break;
    case 0xf602: index_constant(c, true, 0x1000); break;
    case 0xf60d: jsl_to(c, 0xfa86); break;
    case 0xf614: tya16(c); break;
    case 0xf615: case 0xf616: case 0xf617: case 0xf618: case 0xf619: lsr_a8(c); break;
    case 0xf61a: store_dp(c, 0x10, true); break;
    case 0xf61c: stz_dp8(c, 0x11); break;
    case 0xf623: branch_to(c, c->z, 0xf654); break;
    case 0xf625: byte_constant(c, 0x00); break;
    case 0xf627: case 0xf63d: case 0xf63e: xba_native(c); break;
    case 0xf62b: case 0xf631: case 0xf64a: tax16(c); break;
    case 0xf62c: load_a_long_x8(c, 0x01fca7); break;
    case 0xf630: asl_a8(c); break;
    case 0xf632: phy16(c); break;
    case 0xf633: load_dp8(c, 0x10); break;
    case 0xf635: store_register8(c, 0x4202); break;
    case 0xf638: byte_constant(c, 0x12); break;
    case 0xf63a: store_register8(c, 0x4203); break;
    case 0xf63f: nop_native(c); break;
    case 0xf640: load_y_abs16(c, 0x4216); break;
    case 0xf643: sty_dp16(c, 0x00); break;
    case 0xf645: ply16(c); break;
    case 0xf646: txa16(c); break;
    case 0xf647: implied(c); c->c = false; break;
    case 0xf648: adc8_dp(c, 0x00); break;
    case 0xf64b: sep_rep_native(c, 0x20, false); break;
    case 0xf64d: lda_long_x16(c, 0x01fc2d); break;
    case 0xf651: store_a_abs_y16(c, 0x0e0f); break;
    case 0xf654: sep_rep_native(c, 0x20, true); break;
    case 0xf65a: cpy16_constant(c, 0x00a0); break;
    case 0xf65d: branch_to(c, !c->c, 0xf614); break;
    case 0xf65f: return_long(c); break;
  }
  return true;
}

/* $01B55A–$B61D: WRAM $7E3000 pattern fill from DP+$00/$01/$02; clears $07B4. */
static bool wram_pattern_b55a_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xb560 || pc == 0xb562 || pc == 0xb565 || pc == 0xb569 ||
                    pc == 0xb56a || pc == 0xb578 || pc == 0xb57b || pc == 0xb57f ||
                    pc == 0xb580 || pc == 0xb589 || pc == 0xb58c || pc == 0xb590 ||
                    pc == 0xb592 || pc == 0xb593 || pc == 0xb596 || pc == 0xb598 ||
                    pc == 0xb5a9 || pc == 0xb5ac || pc == 0xb5b0 || pc == 0xb5b1 ||
                    pc == 0xb5bd || pc == 0xb5c0 || pc == 0xb5d3 || pc == 0xb5d6 ||
                    pc == 0xb5da || pc == 0xb5dc || pc == 0xb5dd || pc == 0xb5e0 ||
                    pc == 0xb5e2 || pc == 0xb5ea || pc == 0xb5ed || pc == 0xb5f1 ||
                    pc == 0xb5f2 || pc == 0xb600 || pc == 0xb603 || pc == 0xb607 ||
                    pc == 0xb608 || pc == 0xb611 || pc == 0xb614);
  const bool sep_entry = (pc == 0xb56b || pc == 0xb581 || pc == 0xb59a ||
                          pc == 0xb5b2 || pc == 0xb5e4 || pc == 0xb5f3 ||
                          pc == 0xb609 || pc == 0xb618);
  const bool rep_entry = (pc == 0xb55e || pc == 0xb576 || pc == 0xb587 ||
                          pc == 0xb5a7 || pc == 0xb5bb || pc == 0xb5d1 ||
                          pc == 0xb5e8 || pc == 0xb5fe || pc == 0xb60f);
  const bool either = (pc == 0xb5c4); /* SEP reachable from M8 BNE or M16 STA */
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !either && !c->mf) return false;
  switch(pc) {
    case 0xb55a: case 0xb55c: case 0xb55e: case 0xb560: case 0xb562: case 0xb565:
    case 0xb569: case 0xb56a: case 0xb56b: case 0xb56d: case 0xb56f: case 0xb571:
    case 0xb572: case 0xb574: case 0xb576: case 0xb578: case 0xb57b: case 0xb57f:
    case 0xb580: case 0xb581: case 0xb583: case 0xb585: case 0xb587: case 0xb589:
    case 0xb58c: case 0xb590: case 0xb592: case 0xb593: case 0xb596: case 0xb598:
    case 0xb59a: case 0xb59c: case 0xb59e: case 0xb5a0: case 0xb5a2: case 0xb5a3:
    case 0xb5a5: case 0xb5a7: case 0xb5a9: case 0xb5ac: case 0xb5b0: case 0xb5b1:
    case 0xb5b2: case 0xb5b4: case 0xb5b6: case 0xb5b9: case 0xb5bb: case 0xb5bd:
    case 0xb5c0: case 0xb5c4: case 0xb5c6: case 0xb5c7: case 0xb5c8: case 0xb5ca:
    case 0xb5cc: case 0xb5cd: case 0xb5cf: case 0xb5d1: case 0xb5d3: case 0xb5d6:
    case 0xb5da: case 0xb5dc: case 0xb5dd: case 0xb5e0: case 0xb5e2: case 0xb5e4:
    case 0xb5e6: case 0xb5e8: case 0xb5ea: case 0xb5ed: case 0xb5f1: case 0xb5f2:
    case 0xb5f3: case 0xb5f5: case 0xb5f7: case 0xb5f9: case 0xb5fa: case 0xb5fc:
    case 0xb5fe: case 0xb600: case 0xb603: case 0xb607: case 0xb608: case 0xb609:
    case 0xb60b: case 0xb60d: case 0xb60f: case 0xb611: case 0xb614: case 0xb618:
    case 0xb61a: case 0xb61d:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb55a: case 0xb59c: stz_dp8(c, 0x10); break;
    case 0xb55c: stz_dp8(c, 0x11); break;
    case 0xb55e: case 0xb576: case 0xb587: case 0xb5a7: case 0xb5bb: case 0xb5d1:
    case 0xb5e8: case 0xb5fe: case 0xb60f:
      sep_rep_native(c, 0x20, false); break;
    case 0xb560: case 0xb598: case 0xb5e2: ldx_dp16(c, 0x02); break;
    case 0xb562: lda16_constant(c, 0x2832); break;
    case 0xb565: case 0xb57b: case 0xb58c: case 0xb5ac: case 0xb5c0: case 0xb5d6:
    case 0xb5ed: case 0xb603: case 0xb614:
      store_a_long_x16(c, 0x7e3000); break;
    case 0xb569: case 0xb56a: case 0xb57f: case 0xb580: case 0xb5b0: case 0xb5b1:
    case 0xb5c6: case 0xb5c7: case 0xb5f1: case 0xb5f2: case 0xb607: case 0xb608:
      implied(c); c->x++; zn(c, c->x, false); break;
    case 0xb56b: case 0xb581: case 0xb59a: case 0xb5b2: case 0xb5c4: case 0xb5e4:
    case 0xb5f3: case 0xb609: case 0xb618:
      sep_rep_native(c, 0x20, true); break;
    case 0xb56d: case 0xb583: case 0xb5b4: case 0xb5c8: case 0xb5f5: case 0xb60b:
      inc_dp8(c, 0x10); break;
    case 0xb56f: case 0xb5ca: case 0xb5f7: load_dp8(c, 0x10); break;
    case 0xb571: case 0xb5a2: case 0xb5cc: case 0xb5f9: inc_a8(c); break;
    case 0xb572: case 0xb5cd: case 0xb5fa: cmp_dp8(c, 0x00); break;
    case 0xb574: branch_to(c, c->z, 0xb587); break;
    case 0xb578: lda16_constant(c, 0x2833); break;
    case 0xb585: branch_to(c, true, 0xb56f); break;
    case 0xb589: lda16_constant(c, 0x2834); break;
    case 0xb590: case 0xb5da: load_dp16_to_a(c, 0x02); break;
    case 0xb592: case 0xb5dc: implied(c); c->c = false; break;
    case 0xb593: case 0xb5dd: adc16_constant(c, 0x0040); break;
    case 0xb596: case 0xb5e0: store_dp(c, 0x02, false); break;
    case 0xb59e: inc_dp8(c, 0x11); break;
    case 0xb5a0: load_dp8(c, 0x11); break;
    case 0xb5a3: cmp_dp8(c, 0x01); break;
    case 0xb5a5: branch_to(c, c->z, 0xb5e8); break;
    case 0xb5a9: lda16_constant(c, 0x2835); break;
    case 0xb5b6: video_load(c, 0x07b4, false); break;
    case 0xb5b9: branch_to(c, !c->z, 0xb5c4); break;
    case 0xb5bd: lda16_constant(c, 0x2801); break;
    case 0xb5cf: branch_to(c, !c->z, 0xb5b6); break;
    case 0xb5d3: lda16_constant(c, 0x2836); break;
    case 0xb5e6: branch_to(c, true, 0xb59c); break;
    case 0xb5ea: lda16_constant(c, 0x283b); break;
    case 0xb5fc: branch_to(c, c->z, 0xb60f); break;
    case 0xb600: lda16_constant(c, 0x283c); break;
    case 0xb60d: branch_to(c, true, 0xb5f7); break;
    case 0xb611: lda16_constant(c, 0x283d); break;
    case 0xb61a: stz_abs8(c, 0x07b4); break;
    case 0xb61d: return_long(c); break;
  }
  return true;
}

/* $04:871E–$8802: open $8674 callee — PHP, index via $121B into bank-4 pointer
 * tables, decompress/rearrange via $C559/$C68C, optional APU/stream JSLs, copy
 * palette words into $0200, PLP/RTL. */
static bool frame_gfx_871e_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x04 || c->e || c->xf || c->d) return false;
  /* PHP/PLP/RTL: any M. REP entries expect m8; SEP entries expect m16.
   * Remaining m8 body after SEP; else m16. */
  const bool either = (pc == 0x871e || pc == 0x8801 || pc == 0x8802);
  const bool rep_entry = (pc == 0x871f || pc == 0x876c || pc == 0x87c2);
  const bool sep_entry = (pc == 0x875b || pc == 0x87b8);
  const bool m8 = (pc == 0x875d || pc == 0x8761 || pc == 0x8765 || pc == 0x8768 ||
                   pc == 0x87ba || pc == 0x87be);
  if(rep_entry && !c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(!either && !rep_entry && !sep_entry && m8 && !c->mf) return false;
  if(!either && !rep_entry && !sep_entry && !m8 && c->mf) return false;
  switch(pc) {
    case 0x871e: case 0x871f: case 0x8721: case 0x8724: case 0x8727: case 0x8728:
    case 0x872b: case 0x872d: case 0x8730: case 0x8732: case 0x8735: case 0x8737:
    case 0x8739: case 0x873c: case 0x873e: case 0x8741: case 0x8743: case 0x8744:
    case 0x8745: case 0x8748: case 0x8749: case 0x874a: case 0x874c: case 0x874e:
    case 0x8750: case 0x8753: case 0x8756: case 0x8758: case 0x875b: case 0x875d:
    case 0x8761: case 0x8765: case 0x8768: case 0x876c: case 0x876e: case 0x8772:
    case 0x8773: case 0x8774: case 0x8777: case 0x8779: case 0x877a: case 0x877b:
    case 0x877e: case 0x8780: case 0x8783: case 0x8785: case 0x8787: case 0x8789:
    case 0x878b: case 0x878d: case 0x8790: case 0x8792: case 0x8796: case 0x8798:
    case 0x879b: case 0x879c: case 0x879d: case 0x87a0: case 0x87a2: case 0x87a5:
    case 0x87a7: case 0x87a9: case 0x87ab: case 0x87ad: case 0x87b0: case 0x87b3:
    case 0x87b5: case 0x87b8: case 0x87ba: case 0x87be: case 0x87c2: case 0x87c4:
    case 0x87c7: case 0x87c9: case 0x87cc: case 0x87ce: case 0x87d1: case 0x87d2:
    case 0x87d3: case 0x87d6: case 0x87d8: case 0x87db: case 0x87dd: case 0x87e0:
    case 0x87e2: case 0x87e3: case 0x87e6: case 0x87e7: case 0x87e8: case 0x87ea:
    case 0x87ec: case 0x87ee: case 0x87f0: case 0x87f3: case 0x87f5: case 0x87f7:
    case 0x87f8: case 0x87fa: case 0x87fd: case 0x87fe: case 0x87ff: case 0x8801:
    case 0x8802:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x871e: php_native(c); break;
    case 0x871f: sep_rep_native(c, 0x20, false); break;
    case 0x8721: video_load(c, 0x121b, false); break;
    case 0x8724: and_constant16(c, 0x00ff); break;
    case 0x8727: case 0x8744: case 0x8773: case 0x87d2: pha16(c); break;
    case 0x8728: case 0x8774: case 0x87d3: cmp_constant16(c, 0x0080); break;
    case 0x872b: branch_to(c, !c->c, 0x8739); break;
    case 0x872d: lda16_constant(c, 0x90fd); break;
    case 0x8730: case 0x873c: case 0x874c: case 0x877e: case 0x87a0: case 0x87a9:
    case 0x87c7: case 0x87db: case 0x87ea: store_dp(c, 0x00, false); break;
    case 0x8732: case 0x873e: case 0x8780: case 0x87a2: case 0x87c9: case 0x87dd:
      lda16_constant(c, 0x0004); break;
    case 0x8735: case 0x8741: case 0x8783: case 0x87a5: case 0x87cc: case 0x87e0:
      store_dp(c, 0x02, false); break;
    case 0x8737: branch_to(c, true, 0x8743); break;
    case 0x8739: lda16_constant(c, 0x8fef); break;
    case 0x8743: case 0x8772: case 0x87d1: pla16(c); break;
    case 0x8745: and_constant16(c, 0x003f); break;
    case 0x8748: case 0x8779: case 0x879b: case 0x87e6: asl_a16(c); break;
    case 0x8749: case 0x877a: case 0x879c: case 0x87e7: tay16(c); break;
    case 0x874a: case 0x8756: case 0x8787: case 0x87a7: case 0x87b3: case 0x87e8:
    case 0x87f3: lda_indirect_long_y16(c, 0x00); break;
    case 0x87f8: lda_indirect_long_y16(c, 0x73); break;
    case 0x874e: case 0x87ab: case 0x87ec: lda_indirect_long16(c, 0x00); break;
    case 0x8750: case 0x87ad: store_a_abs16(c, 0x0083); break;
    case 0x8753: case 0x87b0: case 0x87f0: index_constant(c, true, 0x0002); break;
    case 0x8758: case 0x87b5: store_a_abs16(c, 0x0085); break;
    case 0x875b: case 0x87b8: sep_rep_native(c, 0x20, true); break;
    case 0x875d: case 0x87ba: jsl_to(c, 0xc559); break;
    case 0x8761: jsl_to(c, 0xc68c); break;
    case 0x8765: index_constant(c, false, 0x0000); break;
    case 0x8768: jsl_to(c, 0x83b2); break;
    case 0x876c: case 0x87c2: sep_rep_native(c, 0x20, false); break;
    case 0x876e: jsl_to(c, 0x889e); break;
    case 0x8777: branch_to(c, c->c, 0x8798); break;
    case 0x877b: lda16_constant(c, 0x9049); break;
    case 0x8785: store_dp(c, 0x75, false); break;
    case 0x8789: case 0x87ee: store_dp(c, 0x73, false); break;
    case 0x878b: stz_dp16(c, 0x76); break;
    case 0x878d: lda16_constant(c, 0x007f); break;
    case 0x8790: store_dp(c, 0x78, false); break;
    case 0x8792: jsl_to(c, 0x8282); break;
    case 0x8796: branch_to(c, true, 0x87c4); break;
    case 0x8798: and_constant16(c, 0x000f); break;
    case 0x879d: lda16_constant(c, 0x9113); break;
    case 0x87be: jsl_to(c, 0x8411); break;
    case 0x87c4: lda16_constant(c, 0x90a3); break;
    case 0x87ce: index_constant(c, false, 0x001e); break;
    case 0x87d6: branch_to(c, !c->c, 0x87e2); break;
    case 0x87d8: lda16_constant(c, 0x9129); break;
    case 0x87e2: pla16(c); break;
    case 0x87e3: and_constant16(c, 0x003f); break;
    case 0x87f5: store_dp(c, 0x75, false); break;
    case 0x87f7: implied(c); c->y = c->x; zn(c, c->y, false); break;
    case 0x87fa: store_a_abs_y16(c, 0x0200); break;
    case 0x87fd: case 0x87fe: implied(c); c->y--; zn(c, c->y, false); break;
    case 0x87ff: branch_to(c, !c->n, 0x87f8); break;
    case 0x8801: plp_native(c); break;
    case 0x8802: return_long(c); break;
  }
  return true;
}

/* $01:FA1A–$FA85: F4B2 tail — adjust $0E0F words from $01FD79, then fill
 * $0E0F,$60.. from $01FC87 via $01FCA7; RTL. */
static bool actor_adjust_fa1a_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xfa2e || pc == 0xfa32 || pc == 0xfa33 || pc == 0xfa36 ||
                    pc == 0xfa67 || pc == 0xfa68 || pc == 0xfa69 || pc == 0xfa6c ||
                    pc == 0xfa6d || pc == 0xfa6e || pc == 0xfa70 || pc == 0xfa71 ||
                    pc == 0xfa75 || pc == 0xfa78 || pc == 0xfa79 || pc == 0xfa7a ||
                    pc == 0xfa7d);
  const bool sep_entry = (pc == 0xfa38 || pc == 0xfa7e);
  const bool rep_entry = (pc == 0xfa2c || pc == 0xfa65);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xfa1a: case 0xfa1d: case 0xfa20: case 0xfa22: case 0xfa24: case 0xfa26:
    case 0xfa27: case 0xfa2a: case 0xfa2b: case 0xfa2c: case 0xfa2e: case 0xfa32:
    case 0xfa33: case 0xfa36: case 0xfa38: case 0xfa3a: case 0xfa3d: case 0xfa3e:
    case 0xfa40: case 0xfa43: case 0xfa46: case 0xfa47: case 0xfa49: case 0xfa4c:
    case 0xfa50: case 0xfa53: case 0xfa55: case 0xfa57: case 0xfa58: case 0xfa5b:
    case 0xfa5c: case 0xfa60: case 0xfa61: case 0xfa63: case 0xfa65: case 0xfa67:
    case 0xfa68: case 0xfa69: case 0xfa6c: case 0xfa6d: case 0xfa6e: case 0xfa70:
    case 0xfa71: case 0xfa75: case 0xfa78: case 0xfa79: case 0xfa7a: case 0xfa7d:
    case 0xfa7e: case 0xfa80: case 0xfa83: case 0xfa85:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xfa1a: index_constant(c, true, 0x0000); break;
    case 0xfa1d: load_a_abs_y8(c, 0x0e00); break;
    case 0xfa20: and_constant8(c, 0x40); break;
    case 0xfa22: branch_to(c, c->z, 0xfa4c); break;
    case 0xfa24: case 0xfa55: byte_constant(c, 0x00); break;
    case 0xfa26: case 0xfa57: xba_native(c); break;
    case 0xfa27: case 0xfa58: load_a_abs_y8(c, 0x0e01); break;
    case 0xfa2a: case 0xfa60: asl_a8(c); break;
    case 0xfa2b: case 0xfa5b: tax16(c); break;
    case 0xfa2c: case 0xfa65: sep_rep_native(c, 0x20, false); break;
    case 0xfa2e: lda_long_x16(c, 0x01fd79); break;
    case 0xfa32: case 0xfa3d: case 0xfa46: case 0xfa68: implied(c); c->c = true; break;
    case 0xfa33: sbc16_constant(c, 0x0200); break;
    case 0xfa36: store_dp(c, 0x10, false); break;
    case 0xfa38: case 0xfa7e: sep_rep_native(c, 0x20, true); break;
    case 0xfa3a: load_a_abs_y8(c, 0x0e0f); break;
    case 0xfa3e: sbc_dp8(c, 0x10); break;
    case 0xfa40: store_a_abs_y8(c, 0x0e0f); break;
    case 0xfa43: load_a_abs_y8(c, 0x0e10); break;
    case 0xfa47: sbc_dp8(c, 0x11); break;
    case 0xfa49: store_a_abs_y8(c, 0x0e10); break;
    case 0xfa4c: jsl_to(c, 0x8af1); break;
    case 0xfa50: cpy16_constant(c, 0x0060); break;
    case 0xfa53: branch_to(c, !c->c, 0xfa1d); break;
    case 0xfa5c: load_a_long_x8(c, 0x01fca7); break;
    case 0xfa61: store_dp(c, 0x00, true); break;
    case 0xfa63: stz_dp8(c, 0x01); break;
    case 0xfa67: case 0xfa78: tya16(c); break;
    case 0xfa69: sbc16_constant(c, 0x0060); break;
    case 0xfa6c: lsr_a16(c); break;
    case 0xfa6d: case 0xfa79: implied(c); c->c = false; break;
    case 0xfa6e: adc_dp16(c, 0x00); break;
    case 0xfa70: tax16(c); break;
    case 0xfa71: lda_long_x16(c, 0x01fc87); break;
    case 0xfa75: store_a_abs_y16(c, 0x0e0f); break;
    case 0xfa7a: adc16_constant(c, 0x0020); break;
    case 0xfa7d: tay16(c); break;
    case 0xfa80: cpy16_constant(c, 0x00a0); break;
    case 0xfa83: branch_to(c, !c->c, 0xfa55); break;
    case 0xfa85: return_long(c); break;
  }
  return true;
}

/* $01:C885–$C8AA: copy 16 words from [DP+$00] into palette shadow $0300+(DP+$03&$FE)<<4. */
static bool palette_copy_c885_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xc895 || pc == 0xc896 || pc == 0xc897 || pc == 0xc898 ||
                    pc == 0xc899 || pc == 0xc89a || pc == 0xc89c || pc == 0xc89f ||
                    pc == 0xc8a0 || pc == 0xc8a1 || pc == 0xc8a2 || pc == 0xc8a3 ||
                    pc == 0xc8a6);
  const bool sep_entry = (pc == 0xc8a8);
  const bool rep_entry = (pc == 0xc893);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xc885: case 0xc887: case 0xc889: case 0xc88c: case 0xc88e: case 0xc88f:
    case 0xc891: case 0xc893: case 0xc895: case 0xc896: case 0xc897: case 0xc898:
    case 0xc899: case 0xc89a: case 0xc89c: case 0xc89f: case 0xc8a0: case 0xc8a1:
    case 0xc8a2: case 0xc8a3: case 0xc8a6: case 0xc8a8: case 0xc8aa:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xc885: byte_constant(c, 0x09); break;
    case 0xc887: store_dp(c, 0x02, true); break;
    case 0xc889: index_constant(c, true, 0x0000); break;
    case 0xc88c: byte_constant(c, 0x00); break;
    case 0xc88e: xba_native(c); break;
    case 0xc88f: load_dp8(c, 0x03); break;
    case 0xc891: and_constant8(c, 0xfe); break;
    case 0xc893: sep_rep_native(c, 0x20, false); break;
    case 0xc895: case 0xc896: case 0xc897: case 0xc898: asl_a16(c); break;
    case 0xc899: tax16(c); break;
    case 0xc89a: lda_indirect_long_y16(c, 0x00); break;
    case 0xc89c: store_a_abs_x16(c, 0x0300); break;
    case 0xc89f: case 0xc8a0: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xc8a1: case 0xc8a2: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xc8a3: cpy16_constant(c, 0x0020); break;
    case 0xc8a6: branch_to(c, !c->c, 0xc89a); break;
    case 0xc8a8: sep_rep_native(c, 0x20, true); break;
    case 0xc8aa: return_long(c); break;
  }
  return true;
}

/* $01:C8C8–$C8E4: F53B SEC-path callee — bank $02 table $A284[DP+$04] → DP+$00,
 * then JSL $C885. */
static bool palette_select_c8c8_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xc8d3 || pc == 0xc8d4 || pc == 0xc8d5 || pc == 0xc8d8 ||
                    pc == 0xc8da || pc == 0xc8dc);
  const bool sep_entry = (pc == 0xc8de);
  const bool rep_entry = (pc == 0xc8d1);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xc8c8: case 0xc8ca: case 0xc8cc: case 0xc8ce: case 0xc8cf: case 0xc8d1:
    case 0xc8d3: case 0xc8d4: case 0xc8d5: case 0xc8d8: case 0xc8da: case 0xc8dc:
    case 0xc8de: case 0xc8e0: case 0xc8e4:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xc8c8: byte_constant(c, 0x02); break;
    case 0xc8ca: store_dp(c, 0x02, true); break;
    case 0xc8cc: byte_constant(c, 0x00); break;
    case 0xc8ce: xba_native(c); break;
    case 0xc8cf: load_dp8(c, 0x04); break;
    case 0xc8d1: sep_rep_native(c, 0x20, false); break;
    case 0xc8d3: asl_a16(c); break;
    case 0xc8d4: tay16(c); break;
    case 0xc8d5: lda16_constant(c, 0xa284); break;
    case 0xc8d8: store_dp(c, 0x00, false); break;
    case 0xc8da: lda_indirect_long_y16(c, 0x00); break;
    case 0xc8dc: store_dp(c, 0x00, false); break;
    case 0xc8de: sep_rep_native(c, 0x20, true); break;
    case 0xc8e0: jsl_to(c, 0xc885); break;
    case 0xc8e4: return_long(c); break;
  }
  return true;
}

/* $01:FC00–$FC22: compute DP+$00 from $0E0F,Y / $0E10,Y (M8 in, M8 out; mid REP). */
static bool actor_scale_fc00_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xfc18 || pc == 0xfc19 || pc == 0xfc1a || pc == 0xfc1b ||
                    pc == 0xfc1c || pc == 0xfc1e);
  const bool sep_entry = (pc == 0xfc20);
  const bool rep_entry = (pc == 0xfc16);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xfc00: case 0xfc03: case 0xfc04: case 0xfc06: case 0xfc07: case 0xfc08:
    case 0xfc0a: case 0xfc0c: case 0xfc0e: case 0xfc10: case 0xfc11: case 0xfc14:
    case 0xfc16: case 0xfc18: case 0xfc19: case 0xfc1a: case 0xfc1b: case 0xfc1c:
    case 0xfc1e: case 0xfc20: case 0xfc22:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xfc00: load_a_abs_y8(c, 0x0e0f); break;
    case 0xfc03: implied(c); c->c = false; break;
    case 0xfc04: adc8_constant(c, 0x04); break;
    case 0xfc06: case 0xfc07: lsr_a8(c); break;
    case 0xfc08: and_constant8(c, 0xfe); break;
    case 0xfc0a: store_dp(c, 0x00, true); break;
    case 0xfc0c: stz_dp8(c, 0x01); break;
    case 0xfc0e: byte_constant(c, 0x00); break;
    case 0xfc10: xba_native(c); break;
    case 0xfc11: load_a_abs_y8(c, 0x0e10); break;
    case 0xfc14: and_constant8(c, 0xf8); break;
    case 0xfc16: sep_rep_native(c, 0x20, false); break;
    case 0xfc18: case 0xfc19: case 0xfc1a: asl_a16(c); break;
    case 0xfc1b: implied(c); c->c = false; break;
    case 0xfc1c: adc_dp16(c, 0x00); break;
    case 0xfc1e: store_dp(c, 0x00, false); break;
    case 0xfc20: sep_rep_native(c, 0x20, true); break;
    case 0xfc22: return_long(c); break;
  }
  return true;
}

/* $01:FAE9–$FB29: A8 table idx → [ $02:($AB20+A*2) ] words into $7E4000,X;
 * length (A+4)*2. M8 in/out; careful REP/SEP around long loads. */
static bool actor_blit_fae9_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xfaf2 || pc == 0xfaf5 || pc == 0xfaf6 || pc == 0xfaf8 ||
                    pc == 0xfb07 || pc == 0xfb09 || pc == 0xfb19 || pc == 0xfb1b ||
                    pc == 0xfb1f || pc == 0xfb20 || pc == 0xfb21 || pc == 0xfb22 ||
                    pc == 0xfb23 || pc == 0xfb25);
  const bool sep_entry = (pc == 0xfafa || pc == 0xfb0b || pc == 0xfb27);
  const bool rep_entry = (pc == 0xfaf0 || pc == 0xfb05); /* require M8 */
  const bool rep_loop = (pc == 0xfb17); /* first entry M8; loop-back already M16 */
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !rep_loop && !c->mf) return false;
  switch(pc) {
    case 0xfae9: case 0xfaeb: case 0xfaec: case 0xfaee: case 0xfaf0: case 0xfaf2:
    case 0xfaf5: case 0xfaf6: case 0xfaf8: case 0xfafa: case 0xfafc: case 0xfafe:
    case 0xfb00: case 0xfb02: case 0xfb05: case 0xfb07: case 0xfb09: case 0xfb0b:
    case 0xfb0d: case 0xfb0f: case 0xfb10: case 0xfb12: case 0xfb13: case 0xfb15:
    case 0xfb17: case 0xfb19: case 0xfb1b: case 0xfb1f: case 0xfb20: case 0xfb21:
    case 0xfb22: case 0xfb23: case 0xfb25: case 0xfb27: case 0xfb29:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xfae9: store_dp(c, 0x10, true); break;
    case 0xfaeb: asl_a8(c); break;
    case 0xfaec: store_dp(c, 0x00, true); break;
    case 0xfaee: stz_dp8(c, 0x01); break;
    case 0xfaf0: case 0xfb05: case 0xfb17: sep_rep_native(c, 0x20, false); break;
    case 0xfaf2: lda16_constant(c, 0xab20); break;
    case 0xfaf5: implied(c); c->c = false; break;
    case 0xfaf6: adc_dp16(c, 0x00); break;
    case 0xfaf8: store_dp(c, 0x03, false); break;
    case 0xfafa: case 0xfb0b: case 0xfb27: sep_rep_native(c, 0x20, true); break;
    case 0xfafc: byte_constant(c, 0x02); break;
    case 0xfafe: store_dp(c, 0x02, true); break;
    case 0xfb00: store_dp(c, 0x05, true); break;
    case 0xfb02: index_constant(c, true, 0x0000); break;
    case 0xfb07: lda_indirect_long_y16(c, 0x03); break;
    case 0xfb09: store_dp(c, 0x00, false); break;
    case 0xfb0d: load_dp8(c, 0x10); break;
    case 0xfb0f: implied(c); c->c = false; break;
    case 0xfb10: adc8_constant(c, 0x04); break;
    case 0xfb12: asl_a8(c); break;
    case 0xfb13: store_dp(c, 0x05, true); break;
    case 0xfb15: stz_dp8(c, 0x06); break;
    case 0xfb19: lda_indirect_long_y16(c, 0x00); break;
    case 0xfb1b: store_a_long_x16(c, 0x7e4000); break;
    case 0xfb1f: case 0xfb20: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xfb21: case 0xfb22: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xfb23: cpy_dp16(c, 0x05); break;
    case 0xfb25: branch_to(c, !c->c, 0xfb17); break;
    case 0xfb29: return_long(c); break;
  }
  return true;
}

/* $01:FBBC–$FBFF: F4B2 optional path — occupied slots via $F660/$FC00/$FAE9,
 * then $8AF1 walk Y=0..$40; empty slots skip to $8AF1. */
static bool actor_optional_fbbc_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xfbbc: case 0xfbbf: case 0xfbc2: case 0xfbc4: case 0xfbc6: case 0xfbca:
    case 0xfbcc: case 0xfbce: case 0xfbcf: case 0xfbd2: case 0xfbd3: case 0xfbd7:
    case 0xfbd8: case 0xfbd9: case 0xfbdb: case 0xfbdd: case 0xfbdf: case 0xfbe1:
    case 0xfbe3: case 0xfbe7: case 0xfbe9: case 0xfbea: case 0xfbec: case 0xfbed:
    case 0xfbef: case 0xfbf0: case 0xfbf1: case 0xfbf5: case 0xfbf6: case 0xfbfa:
    case 0xfbfd: case 0xfbff:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xfbbc: index_constant(c, true, 0x0000); break;
    case 0xfbbf: load_a_abs_y8(c, 0x0e00); break;
    case 0xfbc2: and_constant8(c, 0x40); break;
    case 0xfbc4: branch_to(c, c->z, 0xfbf6); break;
    case 0xfbc6: jsl_to(c, 0xf660); break;
    case 0xfbca: branch_to(c, c->c, 0xfbf6); break;
    case 0xfbcc: byte_constant(c, 0x00); break;
    case 0xfbce: xba_native(c); break;
    case 0xfbcf: load_a_abs_y8(c, 0x0e01); break;
    case 0xfbd2: tax16(c); break;
    case 0xfbd3: load_a_long_x8(c, 0x01fd10); break;
    case 0xfbd7: pha8(c); break;
    case 0xfbd8: implied(c); c->c = false; break;
    case 0xfbd9: adc8_constant(c, 0x04); break;
    case 0xfbdb: and_constant8(c, 0xfe); break;
    case 0xfbdd: store_dp(c, 0x02, true); break;
    case 0xfbdf: stz_dp8(c, 0x03); break;
    case 0xfbe1: case 0xfbed: sep_rep_native(c, 0x20, true); break;
    case 0xfbe3: jsl_to(c, 0xfc00); break;
    case 0xfbe7: load_dp8(c, 0x00); break;
    case 0xfbe9: implied(c); c->c = true; break;
    case 0xfbea: sbc_dp8(c, 0x02); break;
    case 0xfbec: tax16(c); break;
    case 0xfbef: pla8(c); break;
    case 0xfbf0: phy16(c); break;
    case 0xfbf1: jsl_to(c, 0xfae9); break;
    case 0xfbf5: ply16(c); break;
    case 0xfbf6: jsl_to(c, 0x8af1); break;
    case 0xfbfa: cpy16_constant(c, 0x0060); break;
    case 0xfbfd: branch_to(c, !c->c, 0xfbbf); break;
    case 0xfbff: return_long(c); break;
  }
  return true;
}

/* $01:C987–$C9F8: F53B SEC-path sibling — BPL RTL; else seed from $0D75/$0D01/
 * $0D7A, ASL-dispatch to $C9F9 / $C9D9 / $CA34; $C9D9 loads $02A4EF stream. */
static bool actor_sec_c987_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xc9a1 || pc == 0xc9a2 || pc == 0xc9a3 || pc == 0xc9a5 ||
                    pc == 0xc9a6 || pc == 0xc9a9 || pc == 0xc9ab ||
                    pc == 0xc9db || pc == 0xc9df ||
                    pc == 0xc9ed || pc == 0xc9ef || pc == 0xc9f0 || pc == 0xc9f1 ||
                    pc == 0xc9f2 || pc == 0xc9f3);
  const bool sep_entry = (pc == 0xc9ae || pc == 0xc9e2);
  const bool rep_entry = (pc == 0xc99f || pc == 0xc9d9 || pc == 0xc9eb);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xc987: case 0xc98a: case 0xc98c: case 0xc98e: case 0xc98f: case 0xc992:
    case 0xc994: case 0xc995: case 0xc996: case 0xc997: case 0xc998: case 0xc99b:
    case 0xc99d: case 0xc99f: case 0xc9a1: case 0xc9a2: case 0xc9a3: case 0xc9a5:
    case 0xc9a6: case 0xc9a9: case 0xc9ab: case 0xc9ae: case 0xc9b0: case 0xc9b2:
    case 0xc9b4: case 0xc9b6: case 0xc9b8: case 0xc9b9: case 0xc9ba: case 0xc9bc:
    case 0xc9bf: case 0xc9c2: case 0xc9c3: case 0xc9c4: case 0xc9c6: case 0xc9c9:
    case 0xc9ca: case 0xc9cc: case 0xc9cf: case 0xc9d0: case 0xc9d2: case 0xc9d5:
    case 0xc9d8: case 0xc9d9: case 0xc9db: case 0xc9df: case 0xc9e2: case 0xc9e4:
    case 0xc9e8: case 0xc9eb: case 0xc9ed: case 0xc9ef: case 0xc9f0: case 0xc9f1:
    case 0xc9f2: case 0xc9f3: case 0xc9f7:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xc987: case 0xc98f: case 0xc9bf: video_load(c, 0x0d75, false); break;
    case 0xc98a: branch_to(c, !c->n, 0xc9d8); break;
    case 0xc98c: byte_constant(c, 0x00); break;
    case 0xc98e: case 0xc9b8: xba_native(c); break;
    case 0xc992: and_constant8(c, 0x0f); break;
    case 0xc994: tay16(c); break;
    case 0xc995: case 0xc996: case 0xc9c2: case 0xc9c3: case 0xc9c9: case 0xc9cf:
      asl_a8(c); break;
    case 0xc997: tax16(c); break;
    case 0xc998: load_a_abs_x8(c, 0x0d01); break;
    case 0xc99b: store_dp(c, 0x12, true); break;
    case 0xc99d: stz_dp8(c, 0x13); break;
    case 0xc99f: case 0xc9d9: case 0xc9eb: sep_rep_native(c, 0x20, false); break;
    case 0xc9a1: asl_a16(c); break;
    case 0xc9a2: implied(c); c->c = false; break;
    case 0xc9a3: adc_dp16(c, 0x12); break;
    case 0xc9a5: case 0xc9f2: tax16(c); break;
    case 0xc9a6: video_load(c, 0x0d7a, false); break;
    case 0xc9a9: store_dp(c, 0x00, false); break;
    case 0xc9ab: and_constant16(c, 0x0ff0); break;
    case 0xc9ae: case 0xc9e2: sep_rep_native(c, 0x20, true); break;
    case 0xc9b0: and_constant8(c, 0x80); break;
    case 0xc9b2: branch_to(c, c->z, 0xc9b6); break;
    case 0xc9b4: byte_constant(c, 0x01); break;
    case 0xc9b6: store_dp(c, 0x10, true); break;
    case 0xc9b9: implied(c); c->c = false; break;
    case 0xc9ba: adc8_dp(c, 0x10); break;
    case 0xc9bc: store_a_abs_y8(c, 0x0d40); break;
    case 0xc9c4: branch_to(c, !c->c, 0xc9c9); break;
    case 0xc9c6: case 0xc9cc: jmp_abs_native(c, 0xc9f9); break;
    case 0xc9ca: branch_to(c, !c->c, 0xc9cf); break;
    case 0xc9d0: branch_to(c, !c->c, 0xc9d5); break;
    case 0xc9d2: jmp_abs_native(c, 0xc9d9); break;
    case 0xc9d5: jmp_abs_native(c, 0xca34); break;
    case 0xc9d8: return_long(c); break;
    case 0xc9db: lda_long_x16(c, 0x02a4ef); break;
    case 0xc9df: store_a_abs16(c, 0x0d77); break;
    case 0xc9e4: load_a_long_x8(c, 0x02a4f1); break;
    case 0xc9e8: store_register8(c, 0x0d79); break;
    case 0xc9ed: load_dp16_to_a(c, 0x12); break;
    case 0xc9ef: case 0xc9f0: case 0xc9f1: lsr_a16(c); break;
    case 0xc9f3: lda_long_x16(c, 0x02a58f); break;
    case 0xc9f7: branch_to(c, true, 0xca17); break;
  }
  return true;
}

/* ---- helpers for $04:8282 / $83B2 / $8411 / $889E ---- */
static void sta_indirect_long_y8(Cpu *c, uint8_t offset) {
  uint16_t dp = dp_address(c, offset);
  uint32_t pointer = c->read(c->mem, dp);
  pointer |= (uint32_t)c->read(c->mem, (uint16_t)(dp + 1)) << 8;
  pointer |= (uint32_t)c->read(c->mem, (uint16_t)(dp + 2)) << 16;
  sample_interrupt(c);
  c->write(c->mem, (pointer + c->y) & 0xffffffu, (uint8_t)c->a);
}
static void sta_indirect_long_y16(Cpu *c, uint8_t offset) {
  uint16_t dp = dp_address(c, offset);
  uint32_t pointer = c->read(c->mem, dp);
  pointer |= (uint32_t)c->read(c->mem, (uint16_t)(dp + 1)) << 8;
  pointer |= (uint32_t)c->read(c->mem, (uint16_t)(dp + 2)) << 16;
  uint32_t low = (pointer + c->y) & 0xffffffu;
  c->write(c->mem, low, (uint8_t)c->a); sample_interrupt(c);
  c->write(c->mem, (low + 1) & 0xffffffu, (uint8_t)(c->a >> 8));
}
static void asl_dp8(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  uint8_t value = c->read(c->mem, address); idle(c);
  c->c = (value & 0x80u) != 0; value = (uint8_t)(value << 1);
  sample_interrupt(c); c->write(c->mem, address, value); zn(c, value, true);
}
static void asl_dp16(Cpu *c, uint8_t offset) {
  /* LakeSnes RMW word: read low/high (no int), idle, write high then low with int. */
  uint16_t address = dp_address(c, offset);
  uint16_t value = c->read(c->mem, address);
  value |= (uint16_t)c->read(c->mem, (uint16_t)(address + 1)) << 8;
  idle(c);
  unsigned result = (unsigned)value << 1;
  c->c = (result & 0x10000u) != 0;
  c->write(c->mem, (uint16_t)(address + 1), (uint8_t)(result >> 8)); sample_interrupt(c);
  c->write(c->mem, address, (uint8_t)result);
  zn(c, (uint16_t)result, false);
}
static void ror_dp16(Cpu *c, uint8_t offset) {
  /* LakeSnes RMW word: read low/high (no int), idle, write high then low with int. */
  uint16_t address = dp_address(c, offset);
  uint16_t value = c->read(c->mem, address);
  value |= (uint16_t)c->read(c->mem, (uint16_t)(address + 1)) << 8;
  idle(c);
  bool carry_in = c->c;
  c->c = (value & 1u) != 0;
  uint16_t result = (uint16_t)((value >> 1) | (carry_in ? 0x8000u : 0));
  c->write(c->mem, (uint16_t)(address + 1), (uint8_t)(result >> 8)); sample_interrupt(c);
  c->write(c->mem, address, (uint8_t)result);
  zn(c, result, false);
}
static void inc_dp16(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  uint16_t value = c->read(c->mem, address);
  value |= (uint16_t)c->read(c->mem, (uint16_t)(address + 1)) << 8;
  idle(c);
  value++;
  c->write(c->mem, (uint16_t)(address + 1), (uint8_t)(value >> 8)); sample_interrupt(c);
  c->write(c->mem, address, (uint8_t)value);
  zn(c, value, false);
}
static void dec_dp16(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  uint16_t value = c->read(c->mem, address);
  value |= (uint16_t)c->read(c->mem, (uint16_t)(address + 1)) << 8;
  idle(c);
  value--;
  c->write(c->mem, (uint16_t)(address + 1), (uint8_t)(value >> 8)); sample_interrupt(c);
  c->write(c->mem, address, (uint8_t)value);
  zn(c, value, false);
}
static void inc_a16(Cpu *c) {
  implied(c); c->a++; zn(c, c->a, false);
}

/* $04:889E–$88C5: fill $7F0000.. with $FF/$00FF pattern via [DP+$73]. */
static bool wram_fill_889e_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x04 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x889e || pc == 0x88a1 || pc == 0x88b7 || pc == 0x88b9 ||
                    pc == 0x88bc || pc == 0x88be || pc == 0x88bf || pc == 0x88c0 ||
                    pc == 0x88c3 || pc == 0x88c5);
  const bool sep_entry = (pc == 0x88a3);
  const bool rep_entry = (pc == 0x88b7);
  if(m16 && !sep_entry && !rep_entry && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0x889e: case 0x88a1: case 0x88a3: case 0x88a5: case 0x88a7: case 0x88a9:
    case 0x88ac: case 0x88ae: case 0x88b0: case 0x88b1: case 0x88b2: case 0x88b5:
    case 0x88b7: case 0x88b9: case 0x88bc: case 0x88be: case 0x88bf: case 0x88c0:
    case 0x88c3: case 0x88c5:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x889e: lda16_constant(c, 0x0000); break;
    case 0x88a1: store_dp(c, 0x73, false); break;
    case 0x88a3: sep_rep_native(c, 0x20, true); break;
    case 0x88a5: byte_constant(c, 0x7f); break;
    case 0x88a7: store_dp(c, 0x75, true); break;
    case 0x88a9: index_constant(c, true, 0x0000); break;
    case 0x88ac: byte_constant(c, 0xff); break;
    case 0x88ae: sta_indirect_long_y8(c, 0x73); break;
    case 0x88b0: case 0x88b1: case 0x88be: case 0x88bf:
      implied(c); c->y++; zn(c, c->y, false); break;
    case 0x88b2: cpy16_constant(c, 0x7f80); break;
    case 0x88b5: branch_to(c, !c->c, 0x88ae); break;
    case 0x88b7: sep_rep_native(c, 0x20, false); break;
    case 0x88b9: lda16_constant(c, 0x00ff); break;
    case 0x88bc: sta_indirect_long_y16(c, 0x73); break;
    case 0x88c0: cpy16_constant(c, 0x8000); break;
    case 0x88c3: branch_to(c, !c->c, 0x88bc); break;
    case 0x88c5: return_long(c); break;
  }
  return true;
}

/* $04:8411–$8449: copy $7E9000 words into $7F0000 with 64-byte row skip +$C0. */
static bool wram_copy_8411_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x04 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x8431 || pc == 0x8433 || pc == 0x8434 || pc == 0x8437 ||
                    pc == 0x8439 || pc == 0x843a || pc == 0x843b || pc == 0x843e);
  const bool sep_from_m16 = (pc == 0x843f); /* after m16 TXA/AND/ADC body */
  const bool rep_entry = (pc == 0x8414 || pc == 0x8431);
  const bool either = (pc == 0x8411 || pc == 0x8412 || pc == 0x8446 || pc == 0x8447 ||
                       pc == 0x8448 || pc == 0x8449);
  if(m16 && !rep_entry && c->mf) return false;
  if(sep_from_m16 && c->mf) return false;
  if(rep_entry && pc == 0x8414 && !c->mf) return false; /* REP #$10 after SEP #$20 */
  if(rep_entry && pc == 0x8431 && !c->mf) return false; /* REP #$20 after m8 copy */
  if(!either && !m16 && !sep_from_m16 && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0x8411: case 0x8412: case 0x8414: case 0x8416: case 0x8417: case 0x8418:
    case 0x841b: case 0x841d: case 0x841f: case 0x8421: case 0x8424: case 0x8427:
    case 0x8429: case 0x842d: case 0x842e: case 0x842f: case 0x8430: case 0x8431:
    case 0x8433: case 0x8434: case 0x8437: case 0x8439: case 0x843a: case 0x843b:
    case 0x843e: case 0x843f: case 0x8441: case 0x8444: case 0x8446: case 0x8447:
    case 0x8448: case 0x8449:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8411: php_native(c); break;
    case 0x8412: case 0x843f: sep_rep_native(c, 0x20, true); break;
    case 0x8414: sep_rep_native(c, 0x10, false); break;
    case 0x8416: phx16(c); break;
    case 0x8417: phy16(c); break;
    case 0x8418: index_constant(c, true, 0x9000); break;
    case 0x841b: sty_dp16(c, 0x73); break;
    case 0x841d: byte_constant(c, 0x7e); break;
    case 0x841f: store_dp(c, 0x75, true); break;
    case 0x8421: index_constant(c, true, 0x0000); break;
    case 0x8424: index_constant(c, false, 0x0000); break;
    case 0x8427: lda_indirect_long_y8(c, 0x73); break;
    case 0x8429: store_a_long_x8(c, 0x7f0000); break;
    case 0x842d: case 0x842e: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x842f: case 0x8430: implied(c); c->x++; zn(c, c->x, false); break;
    case 0x8431: sep_rep_native(c, 0x20, false); break;
    case 0x8433: case 0x8439: txa16(c); break;
    case 0x8434: and_constant16(c, 0x003f); break;
    case 0x8437: branch_to(c, !c->z, 0x843f); break;
    case 0x843a: implied(c); c->c = false; break;
    case 0x843b: adc16_constant(c, 0x00c0); break;
    case 0x843e: tax16(c); break;
    case 0x8441: cpy16_constant(c, 0x0800); break;
    case 0x8444: branch_to(c, !c->c, 0x8427); break;
    case 0x8446: ply16(c); break;
    case 0x8447: plx16(c); break;
    case 0x8448: plp_native(c); break;
    case 0x8449: return_long(c); break;
  }
  return true;
}

/* $04:83B2–$8410: bitplane unpack from DB:(DP+$86) into $7F0001,X. */
static bool bitplane_83b2_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x04 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x83c0 || pc == 0x83c2 || pc == 0x83c3 ||
                    pc == 0x83c5 || pc == 0x83c7 || pc == 0x83ca || pc == 0x83cc ||
                    pc == 0x83cf || pc == 0x83d1 || pc == 0x83d4 || pc == 0x83d6 ||
                    pc == 0x83d9 || pc == 0x83fb || pc == 0x83fc ||
                    pc == 0x83fd || pc == 0x83fe || pc == 0x8401 || pc == 0x8403 ||
                    pc == 0x8404 || pc == 0x8405 || pc == 0x8407 || pc == 0x8408 ||
                    pc == 0x840a || pc == 0x840c);
  const bool sep_from_m16 = (pc == 0x83db);
  const bool rep_m8 = (pc == 0x83b6 || pc == 0x83be || pc == 0x83f9); /* enter m8 */
  const bool either = (pc == 0x83b2 || pc == 0x83b3 || pc == 0x83b4 || pc == 0x83bd ||
                       pc == 0x840e || pc == 0x840f || pc == 0x8410);
  if(m16 && c->mf) return false;
  if(sep_from_m16 && c->mf) return false;
  if(rep_m8 && !c->mf) return false;
  if(!either && !m16 && !sep_from_m16 && !rep_m8 && !c->mf) return false;
  switch(pc) {
    case 0x83b2: case 0x83b3: case 0x83b4: case 0x83b6: case 0x83b8: case 0x83ba:
    case 0x83bc: case 0x83bd: case 0x83be: case 0x83c0: case 0x83c2: case 0x83c3:
    case 0x83c5: case 0x83c7: case 0x83ca: case 0x83cc: case 0x83cf: case 0x83d1:
    case 0x83d4: case 0x83d6: case 0x83d9: case 0x83db: case 0x83dd: case 0x83df:
    case 0x83e1: case 0x83e3: case 0x83e4: case 0x83e6: case 0x83e7: case 0x83e9:
    case 0x83ea: case 0x83ec: case 0x83ed: case 0x83ef: case 0x83f3: case 0x83f4:
    case 0x83f5: case 0x83f7: case 0x83f9: case 0x83fb: case 0x83fc: case 0x83fd:
    case 0x83fe: case 0x8401: case 0x8403: case 0x8404: case 0x8405: case 0x8407:
    case 0x8408: case 0x840a: case 0x840c: case 0x840e: case 0x840f: case 0x8410:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x83b2: php_native(c); break;
    case 0x83b3: phb_native(c); break;
    case 0x83b4: case 0x83db: sep_rep_native(c, 0x20, true); break;
    case 0x83b6: sep_rep_native(c, 0x10, false); break;
    case 0x83b8: load_dp_y16(c, 0x86); break;
    case 0x83ba: load_dp8(c, 0x88); break;
    case 0x83bc: pha8(c); break;
    case 0x83bd: plb_native(c); break;
    case 0x83be: case 0x83f9: sep_rep_native(c, 0x20, false); break;
    case 0x83c0: load_dp16_to_a(c, 0x89); break;
    case 0x83c2: case 0x8404: implied(c); c->c = false; break;
    case 0x83c3: adc_dp16(c, 0x86); break;
    case 0x83c5: store_dp(c, 0x8d, false); break;
    case 0x83c7: lda16_constant(c, 0x000f); break;
    case 0x83ca: store_dp(c, 0x73, false); break;
    case 0x83cc: lda16_constant(c, 0x0010); break;
    case 0x83cf: store_dp(c, 0x75, false); break;
    case 0x83d1: lda_abs_y16(c, 0x0000); break;
    case 0x83d4: store_dp(c, 0x91, false); break;
    case 0x83d6: lda_abs_y16(c, 0x0010); break;
    case 0x83d9: store_dp(c, 0x93, false); break;
    case 0x83dd: byte_constant(c, 0x07); break;
    case 0x83df: store_dp(c, 0x8f, true); break;
    case 0x83e1: asl_dp8(c, 0x94); break;
    case 0x83e3: case 0x83e6: case 0x83e9: case 0x83ec: rol_a8(c); break;
    case 0x83e4: asl_dp8(c, 0x93); break;
    case 0x83e7: asl_dp8(c, 0x92); break;
    case 0x83ea: asl_dp8(c, 0x91); break;
    case 0x83ed: and_dp8(c, 0x73); break;
    case 0x83ef: store_a_long_x8(c, 0x7f0001); break;
    case 0x83f3: case 0x83f4: implied(c); c->x++; zn(c, c->x, false); break;
    case 0x83f5: dec_dp8(c, 0x8f); break;
    case 0x83f7: branch_to(c, !c->n, 0x83e1); break;
    case 0x83fb: case 0x83fc: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x83fd: case 0x8403: tya16(c); break;
    case 0x83fe: and_constant16(c, 0x000f); break;
    case 0x8401: branch_to(c, !c->z, 0x83d1); break;
    case 0x8405: adc_dp16(c, 0x75); break;
    case 0x8407: tay16(c); break;
    case 0x8408: cpy_dp16(c, 0x8d); break;
    case 0x840a: branch_to(c, !c->c, 0x83d1); break;
    case 0x840c: asl_dp16(c, 0x89); break;
    case 0x840e: plb_native(c); break;
    case 0x840f: plp_native(c); break;
    case 0x8410: return_long(c); break;
  }
  return true;
}

/* $04:8282–$82EB: stream unpack from [DP+$73] into [DP+$76],Y via ($82A3,X). */
static bool stream_unpack_8282_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x04 || c->e || c->xf || c->d) return false;
  const bool m8 = (pc == 0x82ab || pc == 0x82ad || pc == 0x82af || pc == 0x82b0 ||
                   pc == 0x82b2 || pc == 0x82c7 || pc == 0x82c9 || pc == 0x82dc ||
                   pc == 0x82de);
  const bool sep_entry = (pc == 0x82ab || pc == 0x82c7 || pc == 0x82dc);
  const bool rep_entry = (pc == 0x82b4 || pc == 0x82cb || pc == 0x82e0);
  if(m8 && !sep_entry && !c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m8 && !sep_entry && !rep_entry && c->mf) return false;
  switch(pc) {
    case 0x8282: case 0x8284: case 0x8286: case 0x8288: case 0x8289: case 0x828b:
    case 0x828d: case 0x828f: case 0x8291: case 0x8293: case 0x8294: case 0x8297:
    case 0x8299: case 0x829a: case 0x829b: case 0x829e: case 0x829f: case 0x82a0:
    case 0x82ab: case 0x82ad: case 0x82af: case 0x82b0: case 0x82b2: case 0x82b4:
    case 0x82b6: case 0x82b7: case 0x82b8: case 0x82ba: case 0x82bc: case 0x82be:
    case 0x82c0: case 0x82c2: case 0x82c3: case 0x82c5: case 0x82c7: case 0x82c9:
    case 0x82cb: case 0x82cd: case 0x82ce: case 0x82cf: case 0x82d1: case 0x82d3:
    case 0x82d5: case 0x82d7: case 0x82d8: case 0x82da: case 0x82dc: case 0x82de:
    case 0x82e0: case 0x82e2: case 0x82e3: case 0x82e4: case 0x82e5: case 0x82e7:
    case 0x82e9: case 0x82eb:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8282: stz_dp16(c, 0x7b); break;
    case 0x8284: case 0x828d: case 0x82c0: case 0x82d5: lda_indirect_long16(c, 0x73); break;
    case 0x8286: branch_to(c, c->n, 0x828f); break;
    case 0x8288: tay16(c); break;
    case 0x8289: case 0x828b: case 0x828f: case 0x8291: case 0x82b8: case 0x82c5:
    case 0x82da: inc_dp16(c, 0x73); break;
    case 0x8293: pha16(c); break;
    case 0x8294: and_constant16(c, 0x00ff); break;
    case 0x8297: store_dp(c, 0x79, false); break;
    case 0x8299: pla16(c); break;
    case 0x829a: xba_native(c); break;
    case 0x829b: and_constant16(c, 0x0003); break;
    case 0x829e: asl_a16(c); break;
    case 0x829f: tax16(c); break;
    case 0x82a0: jmp_indirect_abs_x(c, 0x82a3); break;
    case 0x82ab: case 0x82c7: case 0x82dc: sep_rep_native(c, 0x20, true); break;
    case 0x82ad: lda_indirect_long8(c, 0x73); break;
    case 0x82af: case 0x82c2: case 0x82d7: implied(c); c->c = false; break;
    case 0x82b0: adc8_dp(c, 0x7b); break;
    case 0x82b2: case 0x82c9: case 0x82de: sta_indirect_long_y8(c, 0x76); break;
    case 0x82b4: case 0x82cb: case 0x82e0: sep_rep_native(c, 0x20, false); break;
    case 0x82b6: case 0x82b7: case 0x82cd: case 0x82ce: case 0x82e3: case 0x82e4:
      implied(c); c->y++; zn(c, c->y, false); break;
    case 0x82ba: case 0x82cf: case 0x82e5: dec_dp16(c, 0x79); break;
    case 0x82bc: branch_to(c, !c->z, 0x82ab); break;
    case 0x82be: case 0x82d3: case 0x82e9: branch_to(c, true, 0x8284); break;
    case 0x82c3: case 0x82d8: adc_dp16(c, 0x7b); break;
    case 0x82d1: branch_to(c, !c->z, 0x82c7); break;
    case 0x82e2: inc_a16(c); break;
    case 0x82e7: branch_to(c, !c->z, 0x82dc); break;
    case 0x82eb: return_long(c); break;
  }
  return true;
}

/* ---- Helpers for A99B / FA86 / BE54 ---- */
static void bit_imm8(Cpu *c, uint8_t value) {
  sample_interrupt(c); (void)fetch(c);
  c->z = ((uint8_t)c->a & value) == 0;
}
static void cmp_abs_y8(Cpu *c, uint16_t base) {
  (void)fetch(c); (void)fetch(c);
  if(!c->xf) idle(c); /* x16 always pays Aby idle */
  sample_interrupt(c);
  uint8_t value = c->read(c->mem, (((uint32_t)c->db << 16) + base + c->y) & 0xffffffu);
  uint8_t a = (uint8_t)c->a;
  c->c = a >= value; zn(c, (uint8_t)(a - value), true);
}

static void cmp_abs_y16(Cpu *c, uint16_t base) {
  (void)fetch(c); (void)fetch(c); idle(c);
  uint32_t low = (((uint32_t)c->db << 16) + base + c->y) & 0xffffffu;
  uint16_t value = c->read(c->mem, low); sample_interrupt(c);
  value |= (uint16_t)c->read(c->mem, (low + 1) & 0xffffffu) << 8;
  c->c = c->a >= value; zn(c, (uint16_t)(c->a - value), false);
}
static void sbc_abs_y16(Cpu *c, uint16_t base) {
  (void)fetch(c); (void)fetch(c); idle(c);
  uint32_t low = (((uint32_t)c->db << 16) + base + c->y) & 0xffffffu;
  uint16_t value = c->read(c->mem, low); sample_interrupt(c);
  value |= (uint16_t)c->read(c->mem, (low + 1) & 0xffffffu) << 8;
  uint16_t old = c->a;
  unsigned result = (unsigned)old - value - (c->c ? 0u : 1u);
  uint16_t out = (uint16_t)result;
  c->v = ((old ^ out) & (old ^ value) & 0x8000u) != 0;
  c->c = result < 0x10000u; c->a = out; zn(c, out, false);
}
/* $00A99B–$AAD8: post-$AB00 Mode-7 epilogue after $A961 matrix load.
 * JSR $AB00 / blank / controllers; BMI/$48:bit4 gate — RTS, or $A9B1 path
 * (discard return, clear $2131, JMP $A153), or $7C!=0 compare loop vs
 * $7004E0 / $AAD9; on full match $A9E9 body fills WRAM, JSR $AE38, HDMA
 * via $420C/$A956, then controller-driven VRAM poke loop through RTS. */
static bool mode7_epilogue_a99b_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xa9da || pc == 0xa9db || pc == 0xa9dc || pc == 0xa9df ||
                    pc == 0xaa46 || pc == 0xaa49 || pc == 0xaa4c || pc == 0xaa4f ||
                    pc == 0xaa50 || pc == 0xaa53);
  const bool sep_entry = (pc == 0xa9e0 || pc == 0xaa54);
  const bool rep_entry = (pc == 0xa9d8 || pc == 0xaa44);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xa99b: case 0xa99e: case 0xa9a2: case 0xa9a5: case 0xa9a8: case 0xa9aa:
    case 0xa9ac: case 0xa9ae: case 0xa9b0: case 0xa9b1: case 0xa9b3: case 0xa9b5:
    case 0xa9b6: case 0xa9b7: case 0xa9bb: case 0xa9be: case 0xa9c1: case 0xa9c4:
    case 0xa9c7: case 0xa9c8: case 0xa9cc: case 0xa9cf: case 0xa9d1: case 0xa9d2:
    case 0xa9d3: case 0xa9d5: case 0xa9d7: case 0xa9d8: case 0xa9da: case 0xa9db:
    case 0xa9dc: case 0xa9df: case 0xa9e0: case 0xa9e2: case 0xa9e5: case 0xa9e7:
    case 0xa9e8: case 0xa9e9: case 0xa9ea: case 0xa9ec: case 0xa9ef: case 0xa9f1:
    case 0xa9f2: case 0xa9f5: case 0xa9f7: case 0xa9fb: case 0xa9fc: case 0xa9fe:
    case 0xaa01: case 0xaa03: case 0xaa07: case 0xaa08: case 0xaa0a: case 0xaa0e:
    case 0xaa0f: case 0xaa11: case 0xaa14: case 0xaa15: case 0xaa18: case 0xaa1c:
    case 0xaa1e: case 0xaa20: case 0xaa21: case 0xaa23: case 0xaa27: case 0xaa28:
    case 0xaa2a: case 0xaa2e: case 0xaa32: case 0xaa33: case 0xaa34: case 0xaa37:
    case 0xaa39: case 0xaa3b: case 0xaa3e: case 0xaa41: case 0xaa44: case 0xaa46:
    case 0xaa49: case 0xaa4c: case 0xaa4f: case 0xaa50: case 0xaa53: case 0xaa54:
    case 0xaa56: case 0xaa58: case 0xaa5b: case 0xaa5d: case 0xaa60: case 0xaa63:
    case 0xaa66: case 0xaa68: case 0xaa6b: case 0xaa6d: case 0xaa70: case 0xaa73:
    case 0xaa77: case 0xaa78: case 0xaa7c: case 0xaa80: case 0xaa81: case 0xaa82:
    case 0xaa86: case 0xaa88: case 0xaa8a: case 0xaa8d: case 0xaa8f: case 0xaa92:
    case 0xaa95: case 0xaa98: case 0xaa9a: case 0xaa9d: case 0xaa9f: case 0xaaa1:
    case 0xaaa4: case 0xaaa7: case 0xaaaa: case 0xaaac: case 0xaaae: case 0xaab0:
    case 0xaab2: case 0xaab4: case 0xaab6: case 0xaab9: case 0xaabc: case 0xaabe:
    case 0xaac0: case 0xaac2: case 0xaac4: case 0xaac5: case 0xaac6: case 0xaac7:
    case 0xaac9: case 0xaaca: case 0xaacc: case 0xaacf: case 0xaad1: case 0xaad4:
    case 0xaad6: case 0xaad8:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xa99b: jsr_abs_native(c, 0xab00); break;
    case 0xa99e: jsl_to(c, 0x828f); break;
    case 0xa9a2: jsr_abs_native(c, 0x82d4); break;
    case 0xa9a5: video_load(c, 0x0d91, false); break;
    case 0xa9a8: branch_to(c, c->n, 0xa9b1); break;
    case 0xa9aa: load_dp8(c, 0x48); break;
    case 0xa9ac: bit_imm8(c, 0x10); break;
    case 0xa9ae: branch_to(c, !c->z, 0xa9b1); break;
    case 0xa9b0: case 0xa9e8: case 0xaad8: return_short(c); break;
    case 0xa9b1: load_dp8(c, 0x7c); break;
    case 0xa9b3: branch_to(c, !c->z, 0xa9c1); break;
    case 0xa9b5: case 0xa9b6: pla8(c); break;
    case 0xa9b7: jsl_to(c, 0x8299); break;
    case 0xa9bb: stz_abs8(c, 0x2131); break;
    case 0xa9be: jmp_abs_native(c, 0xa153); break;
    case 0xa9c1: index_constant(c, false, 0x0007); break;
    case 0xa9c4: index_constant(c, true, 0x0007); break;
    case 0xa9c7: phx16(c); break;
    case 0xa9c8: load_a_long_x8(c, 0x7004e0); break;
    case 0xa9cc: cmp_abs_y8(c, 0xaad9); break;
    case 0xa9cf: branch_to(c, !c->z, 0xa9d7); break;
    case 0xa9d1: implied(c); c->x--; zn(c, c->x, false); break;
    case 0xa9d2: implied(c); c->y--; zn(c, c->y, false); break;
    case 0xa9d3: branch_to(c, !c->n, 0xa9c8); break;
    case 0xa9d5: branch_to(c, true, 0xa9e9); break;
    case 0xa9d7: case 0xa9e7: case 0xa9e9: case 0xa9f1: plx16(c); break;
    case 0xa9d8: case 0xaa44: sep_rep_native(c, 0x20, false); break;
    case 0xa9da: txa16(c); break;
    case 0xa9db: implied(c); c->c = false; break;
    case 0xa9dc: adc16_constant(c, 0x0500); break;
    case 0xa9df: tax16(c); break;
    case 0xa9e0: case 0xaa54: sep_rep_native(c, 0x20, true); break;
    case 0xa9e2: cpx16_constant(c, 0x0f00); break;
    case 0xa9e5: branch_to(c, !c->c, 0xa9c4); break;
    /* $A9E9 body */
    case 0xa9ea: ldx_dp16(c, 0x3d); break;
    case 0xa9ec: cpx16_constant(c, 0x0008); break;
    case 0xa9ef: branch_to(c, !c->z, 0xa9b0); break;
    case 0xa9f2: index_constant(c, false, 0x07ff); break;
    case 0xa9f5: byte_constant(c, 0x00); break;
    case 0xa9f7: store_a_long_x8(c, 0x7e9000); break;
    case 0xa9fb: implied(c); c->x--; zn(c, c->x, false); break;
    case 0xa9fc: branch_to(c, !c->n, 0xa9f7); break;
    case 0xa9fe: index_constant(c, false, 0x00ff); break;
    case 0xaa01: byte_constant(c, 0x04); break;
    case 0xaa03: store_a_long_x8(c, 0x7e9640); break;
    case 0xaa07: case 0xaa0e: implied(c); c->x--; zn(c, c->x, false); break;
    case 0xaa08: byte_constant(c, 0x01); break;
    case 0xaa0a: store_a_long_x8(c, 0x7e9640); break;
    case 0xaa0f: branch_to(c, !c->n, 0xaa01); break;
    case 0xaa11: index_constant(c, false, 0x0000); break;
    case 0xaa14: implied(c); c->y = c->x; zn(c, c->y, false); break;
    case 0xaa15: load_a_abs_y8(c, 0xaae1); break;
    case 0xaa18: store_a_long_x8(c, 0x7e9688); break;
    case 0xaa1c: cmp_constant8(c, 0x01); break;
    case 0xaa1e: branch_to(c, c->z, 0xaa23); break;
    case 0xaa20: implied(c); c->c = false; break;
    case 0xaa21: adc8_constant(c, 0x10); break;
    case 0xaa23: store_a_long_x8(c, 0x7e96c8); break;
    case 0xaa27: case 0xaa32: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xaa28: byte_constant(c, 0x04); break;
    case 0xaa2a: store_a_long_x8(c, 0x7e9688); break;
    case 0xaa2e: store_a_long_x8(c, 0x7e96c8); break;
    case 0xaa33: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xaa34: cpy16_constant(c, 0x0016); break;
    case 0xaa37: branch_to(c, !c->c, 0xaa15); break;
    case 0xaa39: stz_dp8(c, 0x82); break;
    case 0xaa3b: index_constant(c, false, 0x6c00); break;
    case 0xaa3e: index_constant(c, true, 0x0800); break;
    case 0xaa41: jsr_abs_native(c, 0xae38); break;
    case 0xaa46: index_constant(c, false, 0xaaf7); break;
    case 0xaa49: index_constant(c, true, 0x6000); break;
    case 0xaa4c: lda16_constant(c, 0x0008); break;
    case 0xaa4f: phb_native(c); break;
    case 0xaa50: mvn_7e_00(c); break;
    case 0xaa53: plb_native(c); break;
    case 0xaa56: byte_constant(c, 0x00); break;
    case 0xaa58: store_register8(c, 0x4300); break;
    case 0xaa5b: byte_constant(c, 0x2c); break;
    case 0xaa5d: store_register8(c, 0x4301); break;
    case 0xaa60: index_constant(c, false, 0x6000); break;
    case 0xaa63: stx_abs16(c, 0x4302); break;
    case 0xaa66: byte_constant(c, 0x7e); break;
    case 0xaa68: store_register8(c, 0x4304); break;
    case 0xaa6b: byte_constant(c, 0x01); break;
    case 0xaa6d: store_register8(c, 0x420c); break;
    case 0xaa70: case 0xaa92: jsr_abs_native(c, 0xa956); break;
    case 0xaa73: lda_long8(c, 0x7e6002); break;
    case 0xaa77: dec_a8(c); break;
    case 0xaa78: sta_long8(c, 0x7e6002); break;
    case 0xaa7c: lda_long8(c, 0x7e6004); break;
    case 0xaa80: case 0xaa81: inc_a8(c); break;
    case 0xaa82: sta_long8(c, 0x7e6004); break;
    case 0xaa86: cmp_constant8(c, 0x20); break;
    case 0xaa88: branch_to(c, !c->c, 0xaa70); break;
    case 0xaa8a: stz_abs8(c, 0x420c); break;
    case 0xaa8d: byte_constant(c, 0x17); break;
    case 0xaa8f: store_register8(c, 0x212c); break;
    case 0xaa95: jsr_abs_native(c, 0xb960); break;
    case 0xaa98: inc_dp8(c, 0x73); break;
    case 0xaa9a: index_constant(c, false, 0x6f43); break;
    case 0xaa9d: case 0xaab0: case 0xaad6: load_dp8(c, 0x82); break;
    case 0xaa9f: branch_to(c, c->z, 0xaaa4); break;
    case 0xaaa1: index_constant(c, false, 0x6f51); break;
    case 0xaaa4: stx_abs16(c, 0x2116); break;
    case 0xaaa7: jsr_abs_native(c, 0x82d4); break;
    case 0xaaaa: load_dp8(c, 0x48); break;
    case 0xaaac: bit_imm8(c, 0x23); break;
    case 0xaaae: branch_to(c, c->z, 0xaabe); break;
    case 0xaab2: eor_constant8(c, 0xff); break;
    case 0xaab4: store_dp(c, 0x82, true); break;
    case 0xaab6: index_constant(c, false, 0x0401); break;
    case 0xaab9: stx_abs16(c, 0x2118); break;
    case 0xaabc: branch_to(c, true, 0xaa92); break;
    case 0xaabe: bit_imm8(c, 0x10); break;
    case 0xaac0: branch_to(c, !c->z, 0xaad6); break;
    case 0xaac2: load_dp8(c, 0x73); break;
    case 0xaac4: case 0xaac5: case 0xaac6: lsr_a8(c); break;
    case 0xaac7: and_constant8(c, 0x01); break;
    case 0xaac9: implied(c); c->c = false; break;
    case 0xaaca: adc8_constant(c, 0x33); break;
    case 0xaacc: store_register8(c, 0x2118); break;
    case 0xaacf: byte_constant(c, 0x10); break;
    case 0xaad1: store_register8(c, 0x2119); break;
    case 0xaad4: branch_to(c, true, 0xaa92); break;
  }
  return true;
}

/* $01:FA86–$FAE8: clear palette shadow $0300, then up to five long-pointer
 * copies from $7F2800 slots into $0200+($01FADF table); INC $0715; RTL. */
static bool actor_copy_fa86_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xfa8b || pc == 0xfa8e || pc == 0xfa91 || pc == 0xfa92 ||
                    pc == 0xfa93 || pc == 0xfaab || pc == 0xfaaf ||
                    pc == 0xfab1 || pc == 0xfab3 || pc == 0xfab4 || pc == 0xfac5 ||
                    pc == 0xfac9 || pc == 0xfaca || pc == 0xfacd || pc == 0xfacf ||
                    pc == 0xfad2 || pc == 0xfad3 || pc == 0xfad4 || pc == 0xfad5 ||
                    pc == 0xfad6 || pc == 0xfad9);
  const bool sep_entry = (pc == 0xfa95 || pc == 0xfab5 || pc == 0xfadb);
  const bool rep_entry = (pc == 0xfa89 || pc == 0xfaa9 || pc == 0xfac3);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xfa86: case 0xfa89: case 0xfa8b: case 0xfa8e: case 0xfa91: case 0xfa92:
    case 0xfa93: case 0xfa95: case 0xfa97: case 0xfa99: case 0xfa9b: case 0xfa9d:
    case 0xfa9e: case 0xfaa0: case 0xfaa1: case 0xfaa2: case 0xfaa3: case 0xfaa7:
    case 0xfaa9: case 0xfaab: case 0xfaaf: case 0xfab1: case 0xfab3: case 0xfab4:
    case 0xfab5: case 0xfab7: case 0xfab9: case 0xfabb: case 0xfabd: case 0xfabf:
    case 0xfac2: case 0xfac3: case 0xfac5: case 0xfac9: case 0xfaca: case 0xfacd:
    case 0xfacf: case 0xfad2: case 0xfad3: case 0xfad4: case 0xfad5: case 0xfad6:
    case 0xfad9: case 0xfadb: case 0xfadd:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xfa86: index_constant(c, false, 0x001e); break;
    case 0xfa89: case 0xfaa9: case 0xfac3: sep_rep_native(c, 0x20, false); break;
    case 0xfa8b: lda16_constant(c, 0x0000); break;
    case 0xfa8e: store_a_abs_x16(c, 0x0300); break;
    case 0xfa91: case 0xfa92: implied(c); c->x--; zn(c, c->x, false); break;
    case 0xfa93: branch_to(c, !c->n, 0xfa8e); break;
    case 0xfa95: case 0xfab5: case 0xfadb: sep_rep_native(c, 0x20, true); break;
    case 0xfa97: stz_dp8(c, 0x10); break;
    case 0xfa99: stz_dp8(c, 0x11); break;
    case 0xfa9b: byte_constant(c, 0x00); break;
    case 0xfa9d: xba_native(c); break;
    case 0xfa9e: load_dp8(c, 0x10); break;
    case 0xfaa0: case 0xfaa1: asl_a8(c); break;
    case 0xfaa2: tax16(c); break;
    case 0xfaa3: load_a_long_x8(c, 0x7f2802); break;
    case 0xfaa7: store_dp(c, 0x02, true); break;
    case 0xfaab: lda_long_x16(c, 0x7f2800); break;
    case 0xfaaf: store_dp(c, 0x00, false); break;
    case 0xfab1: load_dp16_to_a(c, 0x10); break;
    case 0xfab3: asl_a16(c); break;
    case 0xfab4: tax16(c); break;
    case 0xfab7: inc_dp8(c, 0x10); break;
    case 0xfab9: load_dp8(c, 0x10); break;
    case 0xfabb: cmp_constant8(c, 0x06); break;
    case 0xfabd: branch_to(c, !c->c, 0xfac3); break;
    case 0xfabf: inc_abs8_native(c, 0x0715); break;
    case 0xfac2: return_long(c); break;
    case 0xfac5: lda_long_x16(c, 0x01fadf); break;
    case 0xfac9: tax16(c); break;
    case 0xfaca: index_constant(c, true, 0x0000); break;
    case 0xfacd: lda_indirect_long_y16(c, 0x00); break;
    case 0xfacf: store_a_abs_x16(c, 0x0200); break;
    case 0xfad2: case 0xfad3: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xfad4: case 0xfad5: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xfad6: cpy16_constant(c, 0x0020); break;
    case 0xfad9: branch_to(c, !c->c, 0xfacd); break;
    case 0xfadd: branch_to(c, true, 0xfa9b); break;
  }
  return true;
}

/* $05:BE54–$BECA: five-slot walk via $BE61; pointer scale helper $BEAB.
 * Enables bank-5 map. Nested $C029 is native (with $96F3). */
static bool actor_pack_be54_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xbe6a || pc == 0xbe6b || pc == 0xbe6c || pc == 0xbe6d ||
                    pc == 0xbe71 || pc == 0xbe88 || pc == 0xbe89 || pc == 0xbe8a ||
                    pc == 0xbe8b || pc == 0xbe8c || pc == 0xbe8e || pc == 0xbe92 ||
                    pc == 0xbeb4 || pc == 0xbeb5 || pc == 0xbeb6 ||
                    pc == 0xbeb8 || pc == 0xbeb9 || pc == 0xbebd || pc == 0xbec0 ||
                    pc == 0xbec1 || pc == 0xbec3 || pc == 0xbec4 || pc == 0xbec5 ||
                    pc == 0xbec7);
  const bool sep_entry = (pc == 0xbe73 || pc == 0xbe95 || pc == 0xbec8);
  const bool rep_entry = (pc == 0xbe68 || pc == 0xbe86 || pc == 0xbeb2);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xbe54: case 0xbe57: case 0xbe5a: case 0xbe5d: case 0xbe5e: case 0xbe60:
    case 0xbe61: case 0xbe62: case 0xbe63: case 0xbe67: case 0xbe68: case 0xbe6a:
    case 0xbe6b: case 0xbe6c: case 0xbe6d: case 0xbe71: case 0xbe73: case 0xbe75:
    case 0xbe77: case 0xbe79: case 0xbe7a: case 0xbe7d: case 0xbe80: case 0xbe81:
    case 0xbe82: case 0xbe84: case 0xbe85: case 0xbe86: case 0xbe88: case 0xbe89:
    case 0xbe8a: case 0xbe8b: case 0xbe8c: case 0xbe8e: case 0xbe92: case 0xbe95:
    case 0xbe97: case 0xbe9b: case 0xbe9e: case 0xbe9f: case 0xbea0: case 0xbeab:
    case 0xbead: case 0xbeae: case 0xbeb0: case 0xbeb1: case 0xbeb2: case 0xbeb4:
    case 0xbeb5: case 0xbeb6: case 0xbeb8: case 0xbeb9: case 0xbebd: case 0xbec0:
    case 0xbec1: case 0xbec3: case 0xbec4: case 0xbec5: case 0xbec7: case 0xbec8:
    case 0xbeca:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xbe54: index_constant(c, false, 0x0004); break;
    case 0xbe57: index_constant(c, true, 0x0045); break;
    case 0xbe5a: jsr_abs_native(c, 0xbe61); break;
    case 0xbe5d: implied(c); c->x--; zn(c, c->x, false); break;
    case 0xbe5e: branch_to(c, !c->n, 0xbe57); break;
    case 0xbe60: return_long(c); break;
    case 0xbe61: phb_native(c); break;
    case 0xbe62: case 0xbe81: phx16(c); break;
    case 0xbe63: load_a_long_x8(c, 0x7f0000); break;
    case 0xbe67: case 0xbead: pha8(c); break;
    case 0xbe68: case 0xbe86: case 0xbeb2: sep_rep_native(c, 0x20, false); break;
    case 0xbe6a: case 0xbe88: txa16(c); break;
    case 0xbe6b: case 0xbeb4: case 0xbeb5: case 0xbe89: asl_a16(c); break;
    case 0xbe6c: case 0xbeb8: case 0xbec7: tax16(c); break;
    case 0xbe6d: lda_long_x16(c, 0x05bea1); break;
    case 0xbe71: store_dp(c, 0x86, false); break;
    case 0xbe73: case 0xbe95: case 0xbec8: sep_rep_native(c, 0x20, true); break;
    case 0xbe75: case 0xbe82: byte_constant(c, 0x7f); break;
    case 0xbe77: store_dp(c, 0x88, true); break;
    case 0xbe79: case 0xbeb1: pla8(c); break;
    case 0xbe7a: jsr_abs_native(c, 0xbeab); break;
    case 0xbeab: and_constant8(c, 0x7f); break;
    case 0xbe7d: jsr_abs_native(c, 0xc029); break;
    case 0xbe80: case 0xbe9e: plx16(c); break;
    case 0xbe84: pha8(c); break;
    case 0xbe85: case 0xbe9f: plb_native(c); break;
    case 0xbe8a: asl_a16(c); break;
    case 0xbe8b: tay16(c); break;
    case 0xbe8c: ldx_dp16(c, 0x12); break;
    case 0xbe8e: lda_long_x16(c, 0x1ecb06); break;
    case 0xbe92: store_a_abs_y16(c, 0x2800); break;
    case 0xbe97: load_a_long_x8(c, 0x1ecb08); break;
    case 0xbe9b: store_a_abs_y8(c, 0x2802); break;
    case 0xbea0: case 0xbeca: return_short(c); break;
    case 0xbeae: byte_constant(c, 0x00); break;
    case 0xbeb0: xba_native(c); break;
    case 0xbeb6: store_dp(c, 0x12, false); break;
    case 0xbeb9: lda_long_x16(c, 0x1ecb09); break;
    case 0xbebd: and_constant16(c, 0x00ff); break;
    case 0xbec0: asl_a16(c); break;
    case 0xbec1: store_dp(c, 0x00, false); break;
    case 0xbec3: asl_a16(c); break;
    case 0xbec4: implied(c); c->c = false; break;
    case 0xbec5: adc_dp16(c, 0x00); break;
  }
  return true;
}


/* $00AE38–$AE69: Mode-7 VRAM DMA helper — program ch4 from $7E9000 / Y length,
 * wait via $A956, then fire $420B. Mid-entries $AE5A/$AE61 select mask. */
static bool mode7_dma_ae38_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xae38: case 0xae3b: case 0xae3d: case 0xae40: case 0xae42: case 0xae45:
    case 0xae48: case 0xae4a: case 0xae4d: case 0xae50: case 0xae53: case 0xae56:
    case 0xae58: case 0xae5a: case 0xae5d: case 0xae5f: case 0xae61: case 0xae64:
    case 0xae66: case 0xae69:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xae38: stx_abs16(c, 0x2116); break;
    case 0xae3b: byte_constant(c, 0x01); break;
    case 0xae3d: store_register8(c, 0x4340); break;
    case 0xae40: byte_constant(c, 0x18); break;
    case 0xae42: store_register8(c, 0x4341); break;
    case 0xae45: index_constant(c, false, 0x9000); break;
    case 0xae48: byte_constant(c, 0x7e); break;
    case 0xae4a: stx_abs16(c, 0x4342); break;
    case 0xae4d: store_register8(c, 0x4344); break;
    case 0xae50: store_y_abs16(c, 0x4345); break;
    case 0xae53: case 0xae5a: case 0xae61: jsr_abs_native(c, 0xa956); break;
    case 0xae56: byte_constant(c, 0x10); break;
    case 0xae58: branch_to(c, true, 0xae66); break;
    case 0xae5d: byte_constant(c, 0x30); break;
    case 0xae5f: branch_to(c, true, 0xae66); break;
    case 0xae64: byte_constant(c, 0x80); break;
    case 0xae66: store_register8(c, 0x420b); break;
    case 0xae69: return_short(c); break;
  }
  return true;
}

/* $05:96ED–$9700: optional STY $86 / LDA #$7F / STA $88 then VMAIN=#$80,
 * JSL $00C5ED / $00C707 (native); RTS. Mid-entry at $96F3 remains valid. */
static bool gfx_vmain_96f3_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x96ed: case 0x96ef: case 0x96f1: case 0x96f3: case 0x96f5: case 0x96f8:
    case 0x96fc: case 0x9700:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x96ed: sty_dp16(c, 0x86); break;
    case 0x96ef: byte_constant(c, 0x7f); break;
    case 0x96f1: store_dp(c, 0x88, true); break;
    case 0x96f3: byte_constant(c, 0x80); break;
    case 0x96f5: store_register8(c, 0x2115); break;
    case 0x96f8: jsl_to(c, 0xc5ed); break;
    case 0x96fc: jsl_to(c, 0xc707); break;
    case 0x9700: return_short(c); break;
  }
  return true;
}

/* $05:C029–$C07A: build long ptr from $158060,X; walk [DP+$04] words —
 * BMI → $96F3 accumulate into DP+$86; zero → RTS; else one-shot $96F3. */
static bool actor_stream_c029_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xc031 || pc == 0xc035 || pc == 0xc037 || pc == 0xc039 ||
                    pc == 0xc03b || pc == 0xc04c || pc == 0xc04e || pc == 0xc059 ||
                    pc == 0xc05b || pc == 0xc05d || pc == 0xc062 || pc == 0xc06c ||
                    pc == 0xc06e || pc == 0xc06f || pc == 0xc071);
  const bool sep_entry = (pc == 0xc03d || pc == 0xc050 || pc == 0xc05f ||
                          pc == 0xc064 || pc == 0xc073);
  const bool rep_entry = (pc == 0xc02f || pc == 0xc04a || pc == 0xc057 || pc == 0xc06a);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xc029: case 0xc02d: case 0xc02f: case 0xc031: case 0xc035: case 0xc037:
    case 0xc039: case 0xc03b: case 0xc03d: case 0xc03f: case 0xc040: case 0xc041:
    case 0xc043: case 0xc045: case 0xc047: case 0xc04a: case 0xc04c: case 0xc04e:
    case 0xc050: case 0xc052: case 0xc054: case 0xc057: case 0xc059: case 0xc05b:
    case 0xc05d: case 0xc05f: case 0xc061: case 0xc062: case 0xc064: case 0xc066:
    case 0xc067: case 0xc06a: case 0xc06c: case 0xc06e: case 0xc06f: case 0xc071:
    case 0xc073: case 0xc075: case 0xc076: case 0xc077: case 0xc078: case 0xc07a:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xc029: load_a_long_x8(c, 0x158062); break;
    case 0xc02d: store_dp(c, 0x02, true); break;
    case 0xc02f: case 0xc04a: case 0xc057: case 0xc06a: sep_rep_native(c, 0x20, false); break;
    case 0xc031: lda_long_x16(c, 0x158060); break;
    case 0xc035: store_dp(c, 0x00, false); break;
    case 0xc037: lda_indirect_long_y16(c, 0x00); break;
    case 0xc039: store_dp(c, 0x04, false); break;
    case 0xc03b: store_dp(c, 0x83, false); break;
    case 0xc03d: case 0xc050: case 0xc05f: case 0xc064: case 0xc073:
      sep_rep_native(c, 0x20, true); break;
    case 0xc03f: case 0xc040: case 0xc076: case 0xc077:
      implied(c); c->y++; zn(c, c->y, false); break;
    case 0xc041: lda_indirect_long_y8(c, 0x00); break;
    case 0xc043: store_dp(c, 0x06, true); break;
    case 0xc045: store_dp(c, 0x85, true); break;
    case 0xc047: index_constant(c, true, 0x0000); break;
    case 0xc04c: lda_indirect_long_y16(c, 0x04); break;
    case 0xc04e: branch_to(c, c->n, 0xc062); break;
    case 0xc052: branch_to(c, c->z, 0xc07a); break;
    case 0xc054: case 0xc067: jsr_abs_native(c, 0x96f3); break;
    case 0xc059: load_dp16_to_a(c, 0x89); break;
    case 0xc05b: store_dp(c, 0x86, false); break;
    case 0xc05d: stz_dp16(c, 0x16); break;
    case 0xc061: case 0xc07a: return_short(c); break;
    case 0xc062: store_dp(c, 0x83, false); break;
    case 0xc066: phy16(c); break;
    case 0xc06c: load_dp16_to_a(c, 0x86); break;
    case 0xc06e: implied(c); c->c = false; break;
    case 0xc06f: adc_dp16(c, 0x89); break;
    case 0xc071: store_dp(c, 0x86, false); break;
    case 0xc075: ply16(c); break;
    case 0xc078: branch_to(c, true, 0xc04a); break;
  }
  return true;
}

/* $05:BECB–$C010: post-$BE54 actor stream — scale via $BEAB, $C029 fill,
 * VRAM-queue seed, then WMDATA opcode walk ($80..$84) with $C00A helper. */
static bool actor_stream_becb_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xbecf || pc == 0xbed1 || pc == 0xbed5 || pc == 0xbed7 ||
                    pc == 0xbf03 || pc == 0xbf04 || pc == 0xbf06 || pc == 0xbf0a ||
                    pc == 0xbf0d || pc == 0xbf0f || pc == 0xbf10 || pc == 0xbf12 ||
                    pc == 0xbf15 || pc == 0xbf19 || pc == 0xbf1c || pc == 0xbf1d ||
                    pc == 0xbf36 || pc == 0xbf3a || pc == 0xbf5a || pc == 0xbf5c ||
                    pc == 0xbf5e || pc == 0xbf60 || pc == 0xbf64);
  const bool sep_entry = (pc == 0xbed9 || pc == 0xbf20 || pc == 0xbf3c || pc == 0xbf67);
  const bool rep_entry = (pc == 0xbecd || pc == 0xbf01 || pc == 0xbf34 || pc == 0xbf58);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xbecb: case 0xbecd: case 0xbecf: case 0xbed1: case 0xbed5: case 0xbed7:
    case 0xbed9: case 0xbedb: case 0xbedd: case 0xbedf: case 0xbee2: case 0xbee5:
    case 0xbee7: case 0xbee8: case 0xbeeb: case 0xbeed: case 0xbef0: case 0xbef1:
    case 0xbef2: case 0xbef5: case 0xbef6: case 0xbef9: case 0xbefd: case 0xbf01:
    case 0xbf03: case 0xbf04: case 0xbf06: case 0xbf0a: case 0xbf0d: case 0xbf0f:
    case 0xbf10: case 0xbf12: case 0xbf15: case 0xbf19: case 0xbf1c: case 0xbf1d:
    case 0xbf20: case 0xbf22: case 0xbf24: case 0xbf27: case 0xbf29: case 0xbf2c:
    case 0xbf2e: case 0xbf31: case 0xbf34: case 0xbf36: case 0xbf3a: case 0xbf3c:
    case 0xbf3e: case 0xbf42: case 0xbf44: case 0xbf46: case 0xbf48: case 0xbf4a:
    case 0xbf4b: case 0xbf4e: case 0xbf50: case 0xbf53: case 0xbf56: case 0xbf57:
    case 0xbf58: case 0xbf5a: case 0xbf5c: case 0xbf5e: case 0xbf60: case 0xbf64:
    case 0xbf67: case 0xbf69: case 0xbf6b: case 0xbf6e: case 0xbf70: case 0xbf72:
    case 0xbf74: case 0xbf77: case 0xbf7a: case 0xbf7c: case 0xbf7e: case 0xbf80:
    case 0xbf82: case 0xbf84: case 0xbf86: case 0xbf88: case 0xbf8a: case 0xbf8c:
    case 0xbf8e: case 0xbf90: case 0xbf92: case 0xbf94: case 0xbf96: case 0xbf98:
    case 0xbf9a: case 0xbf9c: case 0xbf9e: case 0xbf9f: case 0xbfa1: case 0xbfa4:
    case 0xbfa7: case 0xbfa9: case 0xbfab: case 0xbfad: case 0xbfaf: case 0xbfb0:
    case 0xbfb3: case 0xbfb6: case 0xbfb8: case 0xbfbb: case 0xbfbd: case 0xbfc0:
    case 0xbfc2: case 0xbfc5: case 0xbfc8: case 0xbfca: case 0xbfcd: case 0xbfd1:
    case 0xbfd3: case 0xbfd5: case 0xbfd9: case 0xbfdb: case 0xbfdf: case 0xbfe0:
    case 0xbfe1: case 0xbfe2: case 0xbfe3: case 0xbfe5: case 0xbfe6: case 0xbfe7:
    case 0xbfe9: case 0xbfeb: case 0xbfec: case 0xbfef: case 0xbff1: case 0xbff4:
    case 0xbff6: case 0xbff8: case 0xbffa: case 0xbffc: case 0xbffd: case 0xc000:
    case 0xc003: case 0xc005: case 0xc007: case 0xc00a: case 0xc00c: case 0xc00f:
    case 0xc010:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xbecb: sty_dp16(c, 0x10); break;
    case 0xbecd: sep_rep_native(c, 0x20, false); break;
    case 0xbecf: ldx_dp16(c, 0x14); break;
    case 0xbed1: lda_long_x16(c, 0x05c019); break;
    case 0xbed5: store_dp(c, 0x86, false); break;
    case 0xbed7: store_dp(c, 0x16, false); break;
    case 0xbed9: sep_rep_native(c, 0x20, true); break;
    case 0xbedb: byte_constant(c, 0x7f); break;
    case 0xbedd: store_dp(c, 0x88, true); break;
    case 0xbedf: load_a_abs_y8(c, 0x0020); break;
    case 0xbee2: jsr_abs_native(c, 0xbeab); break;
    case 0xbee5: byte_constant(c, 0x00); break;
    case 0xbee7: xba_native(c); break;
    case 0xbee8: load_a_abs_y8(c, 0x0021); break;
    case 0xbeeb: and_constant8(c, 0x7f); break;
    case 0xbeed: store_a_abs_y8(c, 0x0021); break;
    case 0xbef0: asl_a8(c); break;
    case 0xbef1: implied(c); c->c = false; break;
    case 0xbef2: adc_abs_y8(c, 0x0021); break;
    case 0xbef5: tay16(c); break;
    case 0xbef6: jsr_abs_native(c, 0xc029); break;
    case 0xbef9: jsl_to(c, 0x8711); break;
    case 0xbefd: jsl_to(c, 0x8724); break;
    case 0xbf01: sep_rep_native(c, 0x20, false); break;
    case 0xbf03: implied(c); c->y = c->x; zn(c, c->y, false); break;
    case 0xbf04: ldx_dp16(c, 0x14); break;
    case 0xbf06: lda_long_x16(c, 0x05c011); break;
    case 0xbf0a: store_a_abs_y16(c, 0x0801); break;
    case 0xbf0d: load_dp16_to_a(c, 0x86); break;
    case 0xbf0f: implied(c); c->c = true; break;
    case 0xbf10: sbc_dp16(c, 0x16); break;
    case 0xbf12: store_a_abs_y16(c, 0x0806); break;
    case 0xbf15: lda_long_x16(c, 0x05c019); break;
    case 0xbf19: store_a_abs_y16(c, 0x0803); break;
    case 0xbf1c: tya16(c); break;
    case 0xbf1d: store_a_abs_x16(c, 0x15e9); break;
    case 0xbf20: sep_rep_native(c, 0x20, true); break;
    case 0xbf22: byte_constant(c, 0x7f); break;
    case 0xbf24: store_a_abs_y8(c, 0x0805); break;
    case 0xbf27: byte_constant(c, 0x80); break;
    case 0xbf29: store_a_abs_y8(c, 0x0800); break;
    case 0xbf2c: load_dp_y16(c, 0x10); break;
    case 0xbf2e: load_a_abs_y8(c, 0x0020); break;
    case 0xbf31: jsr_abs_native(c, 0xbeab); break;
    case 0xbf34: sep_rep_native(c, 0x20, false); break;
    case 0xbf36: lda_long_x16(c, 0x158063); break;
    case 0xbf3a: store_dp(c, 0x04, false); break;
    case 0xbf3c: sep_rep_native(c, 0x20, true); break;
    case 0xbf3e: load_a_long_x8(c, 0x158065); break;
    case 0xbf42: store_dp(c, 0x06, true); break;
    case 0xbf44: store_dp(c, 0x02, true); break;
    case 0xbf46: load_dp_y16(c, 0x10); break;
    case 0xbf48: byte_constant(c, 0x00); break;
    case 0xbf4a: xba_native(c); break;
    case 0xbf4b: load_a_abs_y8(c, 0x0021); break;
    case 0xbf4e: branch_to(c, !c->n, 0xbf53); break;
    case 0xbf50: load_a_abs_y8(c, 0x0028); break;
    case 0xbf53: store_a_abs_y8(c, 0x0028); break;
    case 0xbf56: asl_a8(c); break;
    case 0xbf57: tay16(c); break;
    case 0xbf58: sep_rep_native(c, 0x20, false); break;
    case 0xbf5a: lda_indirect_long_y16(c, 0x04); break;
    case 0xbf5c: store_dp(c, 0x00, false); break;
    case 0xbf5e: ldx_dp16(c, 0x14); break;
    case 0xbf60: lda_long_x16(c, 0x05c021); break;
    case 0xbf64: store_a_abs16(c, 0x2181); break;
    case 0xbf67: sep_rep_native(c, 0x20, true); break;
    case 0xbf69: byte_constant(c, 0x7f); break;
    case 0xbf6b: store_register8(c, 0x2183); break;
    case 0xbf6e: byte_constant(c, 0x30); break;
    case 0xbf70: store_dp(c, 0x10, true); break;
    case 0xbf72: byte_constant(c, 0x00); break;
    case 0xbf74: store_register8(c, 0x2180); break;
    case 0xbf77: index_constant(c, true, 0x0000); break;
    case 0xbf7a: lda_indirect_long_y8(c, 0x00); break;
    case 0xbf7c: cmp_constant8(c, 0x80); break;
    case 0xbf7e: branch_to(c, c->z, 0xbfb8); break;
    case 0xbf80: cmp_constant8(c, 0x81); break;
    case 0xbf82: branch_to(c, c->z, 0xbf9a); break;
    case 0xbf84: cmp_constant8(c, 0x82); break;
    case 0xbf86: branch_to(c, c->z, 0xbfe6); break;
    case 0xbf88: cmp_constant8(c, 0x83); break;
    case 0xbf8a: branch_to(c, c->z, 0xbf92); break;
    case 0xbf8c: cmp_constant8(c, 0x84); break;
    case 0xbf8e: branch_to(c, c->z, 0xbf96); break;
    case 0xbf90: branch_to(c, !c->z, 0xbfa1); break;
    case 0xbf92: byte_constant(c, 0xff); break;
    case 0xbf94: branch_to(c, true, 0xbf9c); break;
    case 0xbf96: byte_constant(c, 0x30); break;
    case 0xbf98: branch_to(c, true, 0xbf9c); break;
    case 0xbf9a: byte_constant(c, 0x70); break;
    case 0xbf9c: store_dp(c, 0x10, true); break;
    case 0xbf9e: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xbf9f: branch_to(c, true, 0xbf7a); break;
    case 0xbfa1: jsr_abs_native(c, 0xc00a); break;
    case 0xbfa4: jsr_abs_native(c, 0xc00a); break;
    case 0xbfa7: load_dp8(c, 0x10); break;
    case 0xbfa9: cmp_constant8(c, 0xff); break;
    case 0xbfab: branch_to(c, !c->z, 0xbfb0); break;
    case 0xbfad: lda_indirect_long_y8(c, 0x00); break;
    case 0xbfaf: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xbfb0: store_register8(c, 0x2180); break;
    case 0xbfb3: jsr_abs_native(c, 0xc00a); break;
    case 0xbfb6: branch_to(c, true, 0xbf7a); break;
    case 0xbfb8: store_register8(c, 0x2180); break;
    case 0xbfbb: load_dp_y16(c, 0x14); break;
    case 0xbfbd: cpy16_constant(c, 0x0004); break;
    case 0xbfc0: branch_to(c, !c->c, 0xbfe5); break;
    case 0xbfc2: index_constant(c, false, 0x0000); break;
    case 0xbfc5: cpy16_constant(c, 0x0004); break;
    case 0xbfc8: branch_to(c, c->z, 0xbfcd); break;
    case 0xbfca: index_constant(c, false, 0x0400); break;
    case 0xbfcd: load_a_long_x8(c, 0x7f6801); break;
    case 0xbfd1: cmp_constant8(c, 0x80); break;
    case 0xbfd3: branch_to(c, c->z, 0xbfe5); break;
    case 0xbfd5: load_a_long_x8(c, 0x7f6802); break;
    case 0xbfd9: ora_constant8(c, 0x80); break;
    case 0xbfdb: store_a_long_x8(c, 0x7f6802); break;
    case 0xbfdf: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xbfe0: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xbfe1: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xbfe2: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xbfe3: branch_to(c, true, 0xbfcd); break;
    case 0xbfe5: return_long(c); break;
    case 0xbfe6: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xbfe7: lda_indirect_long_y8(c, 0x00); break;
    case 0xbfe9: store_dp(c, 0x11, true); break;
    case 0xbfeb: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xbfec: jsr_abs_native(c, 0xc00a); break;
    case 0xbfef: load_dp8(c, 0x11); break;
    case 0xbff1: store_register8(c, 0x2180); break;
    case 0xbff4: load_dp8(c, 0x10); break;
    case 0xbff6: cmp_constant8(c, 0xff); break;
    case 0xbff8: branch_to(c, !c->z, 0xbffd); break;
    case 0xbffa: lda_indirect_long_y8(c, 0x00); break;
    case 0xbffc: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xbffd: store_register8(c, 0x2180); break;
    case 0xc000: jsr_abs_native(c, 0xc00a); break;
    case 0xc003: dec_dp8(c, 0x11); break;
    case 0xc005: branch_to(c, !c->n, 0xbfec); break;
    case 0xc007: jmp_abs_native(c, 0xbf7a); break;
    case 0xc00a: lda_indirect_long_y8(c, 0x00); break;
    case 0xc00c: store_register8(c, 0x2180); break;
    case 0xc00f: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xc010: return_short(c); break;
  }
  return true;
}

/* $01:F9A0–$F9AB: thin wrapper — JSL $F983, LDX #0, JSL $FB2A, RTL.
 * Closes $03:C246 after native $FA86. */
static bool actor_wrap_f9a0_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xf9a0: case 0xf9a4: case 0xf9a7: case 0xf9ab:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf9a0: jsl_to(c, 0xf983); break;
    case 0xf9a4: index_constant(c, false, 0x0000); break;
    case 0xf9a7: jsl_to(c, 0xfb2a); break;
    case 0xf9ab: return_long(c); break;
  }
  return true;
}

/* $05:C07B–$C0EC: actor palette from $1ECB06/08/09 tables via type at $0020,Y;
 * optional $CE4E override; copy 16 words into $0300+(attr<<4) and $16E1 mirror;
 * INC $0715; RTS. Continues bank-5 frontier after native $C07A. */
static bool actor_palette_c07b_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xc087 || pc == 0xc088 || pc == 0xc089 || pc == 0xc08a ||
                    pc == 0xc08e || pc == 0xc0cc || pc == 0xc0ce || pc == 0xc0d1 ||
                    pc == 0xc0d4 || pc == 0xc0d6 || pc == 0xc0d9 || pc == 0xc0db ||
                    pc == 0xc0de || pc == 0xc0df || pc == 0xc0e0 || pc == 0xc0e1 ||
                    pc == 0xc0e2 || pc == 0xc0e5);
  const bool sep_entry = (pc == 0xc090 || pc == 0xc0e7);
  const bool rep_entry = (pc == 0xc085 || pc == 0xc0ca);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xc07b: case 0xc07d: case 0xc07f: case 0xc080: case 0xc083: case 0xc085:
    case 0xc087: case 0xc088: case 0xc089: case 0xc08a: case 0xc08e: case 0xc090:
    case 0xc092: case 0xc095: case 0xc097: case 0xc099: case 0xc09b: case 0xc09d:
    case 0xc0a0: case 0xc0a2: case 0xc0a4: case 0xc0a6: case 0xc0a9: case 0xc0ab:
    case 0xc0af: case 0xc0b1: case 0xc0b5: case 0xc0b7: case 0xc0ba: case 0xc0bc:
    case 0xc0bd: case 0xc0c0: case 0xc0c2: case 0xc0c3: case 0xc0c4: case 0xc0c5:
    case 0xc0c6: case 0xc0c7: case 0xc0ca: case 0xc0cc: case 0xc0ce: case 0xc0d1:
    case 0xc0d4: case 0xc0d6: case 0xc0d9: case 0xc0db: case 0xc0de: case 0xc0df:
    case 0xc0e0: case 0xc0e1: case 0xc0e2: case 0xc0e5: case 0xc0e7: case 0xc0e9:
    case 0xc0ec:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xc07b: sty_dp16(c, 0x10); break;
    case 0xc07d: byte_constant(c, 0x00); break;
    case 0xc07f: xba_native(c); break;
    case 0xc080: load_a_abs_y8(c, 0x0020); break;
    case 0xc083: and_constant8(c, 0x7f); break;
    case 0xc085: case 0xc0ca: sep_rep_native(c, 0x20, false); break;
    case 0xc087: case 0xc088: asl_a16(c); break;
    case 0xc089: tax16(c); break;
    case 0xc08a: lda_long_x16(c, 0x1ecb06); break;
    case 0xc08e: store_dp(c, 0x00, false); break;
    case 0xc090: case 0xc0e7: sep_rep_native(c, 0x20, true); break;
    case 0xc092: cpx16_constant(c, 0x0024); break;
    case 0xc095: branch_to(c, !c->z, 0xc0ab); break;
    case 0xc097: load_dp8(c, 0xb7); break;
    case 0xc099: cmp_constant8(c, 0x5c); break;
    case 0xc09b: branch_to(c, c->z, 0xc0a6); break;
    case 0xc09d: load_a_abs_y8(c, 0x0021); break;
    case 0xc0a0: and_constant8(c, 0x7f); break;
    case 0xc0a2: cmp_constant8(c, 0x1e); break;
    case 0xc0a4: branch_to(c, !c->z, 0xc0ab); break;
    case 0xc0a6: index_constant(c, true, 0xce4e); break;
    case 0xc0a9: sty_dp16(c, 0x00); break;
    case 0xc0ab: load_a_long_x8(c, 0x1ecb08); break;
    case 0xc0af: store_dp(c, 0x02, true); break;
    case 0xc0b1: load_a_long_x8(c, 0x1ecb09); break;
    case 0xc0b5: load_dp_y16(c, 0x10); break;
    case 0xc0b7: store_a_abs_y8(c, 0x0034); break;
    case 0xc0ba: byte_constant(c, 0x00); break;
    case 0xc0bc: xba_native(c); break;
    case 0xc0bd: load_a_abs_y8(c, 0x0022); break;
    case 0xc0c0: and_constant8(c, 0x0e); break;
    case 0xc0c2: case 0xc0c3: case 0xc0c4: case 0xc0c5: asl_a8(c); break;
    case 0xc0c6: tax16(c); break;
    case 0xc0c7: index_constant(c, true, 0x0000); break;
    case 0xc0cc: lda_indirect_long_y16(c, 0x00); break;
    case 0xc0ce: store_a_abs_x16(c, 0x0300); break;
    case 0xc0d1: cpx16_constant(c, 0x0020); break;
    case 0xc0d4: branch_to(c, !c->c, 0xc0de); break;
    case 0xc0d6: cpx16_constant(c, 0x0060); break;
    case 0xc0d9: branch_to(c, c->c, 0xc0de); break;
    case 0xc0db: store_a_abs_x16(c, 0x16e1); break;
    case 0xc0de: case 0xc0df:
      implied(c); c->x++; zn(c, c->x, false); break;
    case 0xc0e0: case 0xc0e1:
      implied(c); c->y++; zn(c, c->y, false); break;
    case 0xc0e2: cpy16_constant(c, 0x0020); break;
    case 0xc0e5: branch_to(c, !c->c, 0xc0cc); break;
    case 0xc0e9: inc_abs8_native(c, 0x0715); break;
    case 0xc0ec: return_short(c); break;
  }
  return true;
}

/* $03:C232–$C24A: seed $7F0000/$0E01=#$05 then JSL $BE54/$FA86/$F9A0; RTL.
 * All three callees are native. */
static bool actor_seed_c232_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x03 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xc232: case 0xc234: case 0xc238: case 0xc23b: case 0xc23f:
    case 0xc243: case 0xc246: case 0xc24a:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xc232: byte_constant(c, 0x05); break;
    case 0xc234: sta_long8(c, 0x7f0000); break;
    case 0xc238: store_register8(c, 0x0e01); break;
    case 0xc23b: jsl_to(c, 0xbe54); break;
    case 0xc23f: jsl_to(c, 0xfa86); break;
    case 0xc243: index_constant(c, true, 0x0000); break;
    case 0xc246: jsl_to(c, 0xf9a0); break;
    case 0xc24a: return_long(c); break;
  }
  return true;
}

/* $05:9AFF–$9B36: sole $BECB caller prefix — STX DP+$14, Y from $059B89,X,
 * scene-id gates, optional JSL $BECB, early RTL on type/sign. Continuation
 * at $9B37 (JSR $9B91 / JMP $9626 / JML $85B9) is native below. */
static bool actor_becb_caller_9aff_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x9b03 || pc == 0x9b07);
  const bool sep_entry = (pc == 0x9b08);
  const bool rep_entry = (pc == 0x9b01);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0x9aff: case 0x9b01: case 0x9b03: case 0x9b07: case 0x9b08: case 0x9b0a:
    case 0x9b0c: case 0x9b0e: case 0x9b10: case 0x9b12: case 0x9b14: case 0x9b16:
    case 0x9b18: case 0x9b1a: case 0x9b1c: case 0x9b1f: case 0x9b21: case 0x9b24:
    case 0x9b26: case 0x9b29: case 0x9b2b: case 0x9b2c: case 0x9b30: case 0x9b31:
    case 0x9b34: case 0x9b36:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x9aff: stx_dp16(c, 0x14); break;
    case 0x9b01: sep_rep_native(c, 0x20, false); break;
    case 0x9b03: lda_long_x16(c, 0x059b89); break;
    case 0x9b07: tay16(c); break;
    case 0x9b08: sep_rep_native(c, 0x20, true); break;
    case 0x9b0a: load_dp8(c, 0xb7); break;
    case 0x9b0c: cmp_constant8(c, 0x34); break;
    case 0x9b0e: branch_to(c, c->z, 0x9b18); break;
    case 0x9b10: cmp_constant8(c, 0x35); break;
    case 0x9b12: branch_to(c, c->z, 0x9b18); break;
    case 0x9b14: cmp_constant8(c, 0x49); break;
    case 0x9b16: branch_to(c, !c->z, 0x9b21); break;
    case 0x9b18: load_dp8(c, 0xb9); break;
    case 0x9b1a: branch_to(c, !c->n, 0x9b21); break;
    case 0x9b1c: index_constant(c, false, 0x0000); break;
    case 0x9b1f: stx_dp16(c, 0x14); break;
    case 0x9b21: load_a_abs_y8(c, 0x0020); break;
    case 0x9b24: branch_to(c, c->n, 0x9b36); break;
    case 0x9b26: load_a_abs_y8(c, 0x0021); break;
    case 0x9b29: branch_to(c, !c->n, 0x9b31); break;
    case 0x9b2b: phy16(c); break;
    case 0x9b2c: jsl_to(c, 0xbecb); break;
    case 0x9b30: ply16(c); break;
    case 0x9b31: load_a_abs_y8(c, 0x0021); break;
    case 0x9b34: branch_to(c, !c->n, 0x9b37); break;
    case 0x9b36: return_long(c); break;
  }
  return true;
}

/* ---- Helpers for 9B37 / 9B91 / C0ED / C116 ---- */
static void jml_to(Cpu *c, uint16_t dest, uint8_t bank) {
  (void)fetch(c); (void)fetch(c); sample_interrupt(c); (void)fetch(c);
  c->pc = dest; c->k = bank;
}
static void adc_long_x16(Cpu *c, uint32_t base) {
  (void)fetch(c); (void)fetch(c); (void)fetch(c);
  uint32_t low = (base + c->x) & 0xffffffu;
  uint16_t value = c->read(c->mem, low); sample_interrupt(c);
  value |= (uint16_t)c->read(c->mem, (low + 1) & 0xffffffu) << 8;
  uint16_t old = c->a;
  unsigned result = (unsigned)old + value + (c->c ? 1u : 0u);
  c->v = (~(old ^ value) & (old ^ (uint16_t)result) & 0x8000u) != 0;
  c->c = result > 0xffffu;
  c->a = (uint16_t)result; zn(c, c->a, false);
}
static void sbc_long_x16(Cpu *c, uint32_t base) {
  (void)fetch(c); (void)fetch(c); (void)fetch(c);
  uint32_t low = (base + c->x) & 0xffffffu;
  uint16_t value = c->read(c->mem, low); sample_interrupt(c);
  value |= (uint16_t)c->read(c->mem, (low + 1) & 0xffffffu) << 8;
  uint16_t old = c->a;
  unsigned result = (unsigned)old - value - (c->c ? 0u : 1u);
  uint16_t out = (uint16_t)result;
  c->v = ((old ^ out) & (old ^ value) & 0x8000u) != 0;
  c->c = result < 0x10000u;
  c->a = out; zn(c, out, false);
}

static void sbc_long_x8(Cpu *c, uint32_t base) {
  (void)fetch(c); (void)fetch(c); (void)fetch(c);
  sample_interrupt(c);
  sbc8_value(c, c->read(c->mem, (base + c->x) & 0xffffffu));
}
static void ora_abs8(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c); sample_interrupt(c);
  set_a8(c, (uint8_t)((uint8_t)c->a | c->read(c->mem, ((uint32_t)c->db << 16) | address)));
}
static void ora_abs_y8(Cpu *c, uint16_t base) {
  (void)fetch(c); (void)fetch(c); idle(c); sample_interrupt(c);
  uint8_t value = c->read(c->mem, (((uint32_t)c->db << 16) + base + c->y) & 0xffffffu);
  set_a8(c, (uint8_t)((uint8_t)c->a | value));
}
static void cpy_abs16(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c);
  uint32_t low = ((uint32_t)c->db << 16) | address;
  uint16_t value = c->read(c->mem, low); sample_interrupt(c);
  value |= (uint16_t)c->read(c->mem, (low + 1) & 0xffffffu) << 8;
  c->c = c->y >= value; zn(c, (uint16_t)(c->y - value), false);
}
static void inc_abs16(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c);
  uint32_t low = ((uint32_t)c->db << 16) | address;
  uint32_t high = (low + 1) & 0xffffffu;
  uint16_t value = c->read(c->mem, low);
  value |= (uint16_t)c->read(c->mem, high) << 8;
  idle(c);
  value++;
  c->write(c->mem, high, (uint8_t)(value >> 8)); sample_interrupt(c);
  c->write(c->mem, low, (uint8_t)value);
  zn(c, value, false);
}
static void dec_abs16(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c);
  uint32_t low = ((uint32_t)c->db << 16) | address;
  uint32_t high = (low + 1) & 0xffffffu;
  uint16_t value = c->read(c->mem, low);
  value |= (uint16_t)c->read(c->mem, high) << 8;
  idle(c);
  value--;
  c->write(c->mem, high, (uint8_t)(value >> 8)); sample_interrupt(c);
  c->write(c->mem, low, (uint8_t)value);
  zn(c, value, false);
}



/* $05:9B37–$9B88: post-$9AFF actor prep — copy fields, pointer from $05C021,
 * JSR $9B91 adjust, then screen-bounds gate → JMP $9626 or JML $0085B9. */
static bool actor_prep_9b37_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x9b50 || pc == 0x9b53 || pc == 0x9b55 || pc == 0x9b58 ||
                    pc == 0x9b5a || pc == 0x9b5c || pc == 0x9b60);
  const bool sep_entry = (pc == 0x9b62);
  const bool rep_entry = (pc == 0x9b4e);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0x9b37: case 0x9b3a: case 0x9b3c: case 0x9b3f: case 0x9b41: case 0x9b43:
    case 0x9b45: case 0x9b47: case 0x9b49: case 0x9b4c: case 0x9b4e: case 0x9b50:
    case 0x9b53: case 0x9b55: case 0x9b58: case 0x9b5a: case 0x9b5c: case 0x9b60:
    case 0x9b62: case 0x9b64: case 0x9b66: case 0x9b68: case 0x9b6b: case 0x9b6d:
    case 0x9b6f: case 0x9b71: case 0x9b74: case 0x9b76: case 0x9b79: case 0x9b7b:
    case 0x9b7d: case 0x9b80: case 0x9b82: case 0x9b85:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x9b37: load_a_abs_y8(c, 0x0022); break;
    case 0x9b3a: store_dp(c, 0x03, true); break;
    case 0x9b3c: load_a_abs_y8(c, 0x0028); break;
    case 0x9b3f: cmp_constant8(c, 0x19); break;
    case 0x9b41: branch_to(c, !c->z, 0x9b49); break;
    case 0x9b43: load_dp8(c, 0x03); break;
    case 0x9b45: eor_constant8(c, 0x40); break;
    case 0x9b47: store_dp(c, 0x03, true); break;
    case 0x9b49: load_a_abs_y8(c, 0x0023); break;
    case 0x9b4c: store_dp(c, 0x09, true); break;
    case 0x9b4e: sep_rep_native(c, 0x20, false); break;
    case 0x9b50: lda_abs_y16(c, 0x0024); break;
    case 0x9b53: store_dp(c, 0x0c, false); break;
    case 0x9b55: lda_abs_y16(c, 0x0026); break;
    case 0x9b58: store_dp(c, 0x0a, false); break;
    case 0x9b5a: ldx_dp16(c, 0x14); break;
    case 0x9b5c: lda_long_x16(c, 0x05c021); break;
    case 0x9b60: store_dp(c, 0x00, false); break;
    case 0x9b62: sep_rep_native(c, 0x20, true); break;
    case 0x9b64: byte_constant(c, 0x7f); break;
    case 0x9b66: store_dp(c, 0x02, true); break;
    case 0x9b68: jsr_abs_native(c, 0x9b91); break;
    case 0x9b6b: byte_constant(c, 0xa8); break;
    case 0x9b6d: store_dp(c, 0x0e, true); break;
    case 0x9b6f: load_dp_y16(c, 0x0c); break;
    case 0x9b71: cpy16_constant(c, 0x00c8); break;
    case 0x9b74: branch_to(c, c->c, 0x9b85); break;
    case 0x9b76: cpy16_constant(c, 0x0038); break;
    case 0x9b79: branch_to(c, !c->c, 0x9b85); break;
    case 0x9b7b: load_dp_y16(c, 0x0a); break;
    case 0x9b7d: cpy16_constant(c, 0x00c0); break;
    case 0x9b80: branch_to(c, c->c, 0x9b85); break;
    case 0x9b82: jmp_abs_native(c, 0x9626); break;
    case 0x9b85: jml_to(c, 0x85b9, 0x00); break;
  }
  return true;
}

/* $05:9B91–$9C0A: leaf — scale $0021,Y via $059C0B/$059C17 into DP+$0A/$0C; RTS. */
static bool actor_adjust_9b91_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x9bb6 || pc == 0x9bb8 || pc == 0x9bb9 || pc == 0x9bbd ||
                    pc == 0x9bc9 || pc == 0x9bcb || pc == 0x9bcc || pc == 0x9bd0 ||
                    pc == 0x9be5 || pc == 0x9be7 || pc == 0x9be8 || pc == 0x9bec ||
                    pc == 0x9bff || pc == 0x9c01 || pc == 0x9c02 || pc == 0x9c06);
  const bool sep_entry = (pc == 0x9bbf || pc == 0x9bd2 || pc == 0x9bee || pc == 0x9c08);
  const bool rep_entry = (pc == 0x9bb4 || pc == 0x9bc7 || pc == 0x9be3 || pc == 0x9bfd);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0x9b91: case 0x9b93: case 0x9b94: case 0x9b97: case 0x9b99: case 0x9b9b:
    case 0x9b9c: case 0x9b9e: case 0x9ba0: case 0x9ba1: case 0x9ba5: case 0x9ba7:
    case 0x9baa: case 0x9bac: case 0x9bad: case 0x9bae: case 0x9bb0: case 0x9bb2:
    case 0x9bb4: case 0x9bb6: case 0x9bb8: case 0x9bb9: case 0x9bbd: case 0x9bbf:
    case 0x9bc1: case 0x9bc3: case 0x9bc5: case 0x9bc7: case 0x9bc9: case 0x9bcb:
    case 0x9bcc: case 0x9bd0: case 0x9bd2: case 0x9bd4: case 0x9bd6: case 0x9bd8:
    case 0x9bda: case 0x9bdc: case 0x9bdf: case 0x9be1: case 0x9be3: case 0x9be5:
    case 0x9be7: case 0x9be8: case 0x9bec: case 0x9bee: case 0x9bf0: case 0x9bf2:
    case 0x9bf4: case 0x9bf6: case 0x9bf9: case 0x9bfb: case 0x9bfd: case 0x9bff:
    case 0x9c01: case 0x9c02: case 0x9c06: case 0x9c08: case 0x9c0a:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x9b91: byte_constant(c, 0x00); break;
    case 0x9b93: xba_native(c); break;
    case 0x9b94: load_a_abs_y8(c, 0x0021); break;
    case 0x9b97: cmp_constant8(c, 0x12); break;
    case 0x9b99: branch_to(c, c->c, 0x9c0a); break;
    case 0x9b9b: implied(c); c->c = true; break;
    case 0x9b9c: sbc8_constant(c, 0x06); break;
    case 0x9b9e: branch_to(c, !c->c, 0x9c0a); break;
    case 0x9ba0: tax16(c); break;
    case 0x9ba1: load_a_long_x8(c, 0x059c0b); break;
    case 0x9ba5: store_dp(c, 0x20, true); break;
    case 0x9ba7: load_a_abs_y8(c, 0x0034); break;
    case 0x9baa: and_constant8(c, 0x1f); break;
    case 0x9bac: asl_a8(c); break;
    case 0x9bad: tax16(c); break;
    case 0x9bae: load_dp8(c, 0x20); break;
    case 0x9bb0: bit_imm8(c, 0x04); break;
    case 0x9bb2: branch_to(c, c->z, 0x9bc3); break;
    case 0x9bb4: case 0x9bc7: case 0x9be3: case 0x9bfd: sep_rep_native(c, 0x20, false); break;
    case 0x9bb6: case 0x9bc9: load_dp16_to_a(c, 0x0a); break;
    case 0x9bb8: case 0x9be7: implied(c); c->c = true; break;
    case 0x9bb9: case 0x9be8: sbc_long_x16(c, 0x059c17); break;
    case 0x9bbd: store_dp(c, 0x0a, false); break;
    case 0x9bbf: case 0x9bd2: case 0x9bee: case 0x9c08: sep_rep_native(c, 0x20, true); break;
    case 0x9bc1: branch_to(c, true, 0x9c0a); break;
    case 0x9bc3: bit_imm8(c, 0x08); break;
    case 0x9bc5: branch_to(c, c->z, 0x9bd6); break;
    case 0x9bcb: case 0x9c01: implied(c); c->c = false; break;
    case 0x9bcc: case 0x9c02: adc_long_x16(c, 0x059c17); break;
    case 0x9bd0: store_dp(c, 0x0a, false); break;
    case 0x9bd4: branch_to(c, true, 0x9c0a); break;
    case 0x9bd6: load_dp8(c, 0x20); break;
    case 0x9bd8: bit_imm8(c, 0x01); break;
    case 0x9bda: branch_to(c, c->z, 0x9bf2); break;
    case 0x9bdc: case 0x9bf6: load_a_abs_y8(c, 0x0022); break;
    case 0x9bdf: case 0x9bf9: bit_imm8(c, 0x40); break;
    case 0x9be1: branch_to(c, !c->z, 0x9bfd); break;
    case 0x9be5: case 0x9bff: load_dp16_to_a(c, 0x0c); break;
    case 0x9bec: store_dp(c, 0x0c, false); break;
    case 0x9bf0: branch_to(c, true, 0x9c0a); break;
    case 0x9bf2: bit_imm8(c, 0x02); break;
    case 0x9bf4: branch_to(c, c->z, 0x9c0a); break;
    case 0x9bfb: branch_to(c, !c->z, 0x9be3); break;
    case 0x9c06: store_dp(c, 0x0c, false); break;
    case 0x9c0a: return_short(c); break;
  }
  return true;
}

/* $05:C0ED–$C115: dual-slot palette push via $153B/$153C → JSR $C116; RTL. */
static bool palette_push_c0ed_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xc0ed: case 0xc0f0: case 0xc0f2: case 0xc0f4: case 0xc0f6: case 0xc0f8:
    case 0xc0fb: case 0xc0fe: case 0xc101: case 0xc104: case 0xc106: case 0xc108:
    case 0xc10a: case 0xc10c: case 0xc10f: case 0xc112: case 0xc115:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xc0ed: video_load(c, 0x153b, false); break;
    case 0xc0f0: branch_to(c, c->z, 0xc101); break;
    case 0xc0f2: cmp_constant8(c, 0x80); break;
    case 0xc0f4: branch_to(c, !c->z, 0xc0fb); break;
    case 0xc0f6: byte_constant(c, 0x00); break;
    case 0xc0f8: store_register8(c, 0x153b); break;
    case 0xc0fb: index_constant(c, false, 0x0120); break;
    case 0xc0fe: jsr_abs_native(c, 0xc116); break;
    case 0xc101: video_load(c, 0x153c, false); break;
    case 0xc104: branch_to(c, c->z, 0xc115); break;
    case 0xc106: cmp_constant8(c, 0x80); break;
    case 0xc108: branch_to(c, !c->z, 0xc10f); break;
    case 0xc10a: byte_constant(c, 0x00); break;
    case 0xc10c: store_register8(c, 0x153c); break;
    case 0xc10f: index_constant(c, false, 0x0140); break;
    case 0xc112: jsr_abs_native(c, 0xc116); break;
    case 0xc115: return_long(c); break;
  }
  return true;
}

/* $05:C116–$C1BB: copy/blend 32 bytes $15E1,X → $0200,X (or $05C1B8 color add);
 * INC $0715; RTS. Epilogue bytes at $C1B8 also seed the blend table. */
static bool palette_slot_c116_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xc140 || pc == 0xc142 || pc == 0xc144 || pc == 0xc145 ||
                    pc == 0xc147 || pc == 0xc14a || pc == 0xc14c || pc == 0xc14e ||
                    pc == 0xc151 || pc == 0xc153 || pc == 0xc157 || pc == 0xc159 ||
                    pc == 0xc15d || pc == 0xc15f || pc == 0xc162 || pc == 0xc164 ||
                    pc == 0xc167 || pc == 0xc16a || pc == 0xc16b || pc == 0xc16d ||
                    pc == 0xc170 || pc == 0xc172 || pc == 0xc175 || pc == 0xc177 ||
                    pc == 0xc179 || pc == 0xc17c || pc == 0xc17e || pc == 0xc181 ||
                    pc == 0xc184 || pc == 0xc185 || pc == 0xc187 || pc == 0xc18a ||
                    pc == 0xc18c || pc == 0xc18f || pc == 0xc191 || pc == 0xc193 ||
                    pc == 0xc195 || pc == 0xc198 || pc == 0xc19a || pc == 0xc19d ||
                    pc == 0xc1a0 || pc == 0xc1a1 || pc == 0xc1a3 || pc == 0xc1a6 ||
                    pc == 0xc1a8 || pc == 0xc1ab || pc == 0xc1ad || pc == 0xc1b0 ||
                    pc == 0xc1b1 || pc == 0xc1b2 || pc == 0xc1b4);
  const bool sep_entry = (pc == 0xc1b6);
  const bool rep_entry = (pc == 0xc13e); /* $C14C is redundant REP while already m16 */
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xc116: case 0xc118: case 0xc11a: case 0xc11c: case 0xc11e: case 0xc120:
    case 0xc122: case 0xc125: case 0xc128: case 0xc12b: case 0xc12c: case 0xc12d:
    case 0xc130: case 0xc132: case 0xc135: case 0xc136: case 0xc138: case 0xc13a:
    case 0xc13c: case 0xc13e: case 0xc140: case 0xc142: case 0xc144: case 0xc145:
    case 0xc147: case 0xc14a: case 0xc14c: case 0xc14e: case 0xc151: case 0xc153:
    case 0xc157: case 0xc159: case 0xc15d: case 0xc15f: case 0xc162: case 0xc164:
    case 0xc167: case 0xc16a: case 0xc16b: case 0xc16d: case 0xc170: case 0xc172:
    case 0xc175: case 0xc177: case 0xc179: case 0xc17c: case 0xc17e: case 0xc181:
    case 0xc184: case 0xc185: case 0xc187: case 0xc18a: case 0xc18c: case 0xc18f:
    case 0xc191: case 0xc193: case 0xc195: case 0xc198: case 0xc19a: case 0xc19d:
    case 0xc1a0: case 0xc1a1: case 0xc1a3: case 0xc1a6: case 0xc1a8: case 0xc1ab:
    case 0xc1ad: case 0xc1b0: case 0xc1b1: case 0xc1b2: case 0xc1b4: case 0xc1b6:
    case 0xc1b8: case 0xc1bb:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xc116: and_constant8(c, 0x7f); break;
    case 0xc118: store_dp(c, 0x00, true); break;
    case 0xc11a: branch_to(c, c->z, 0xc122); break;
    case 0xc11c: load_dp8(c, 0x4f); break;
    case 0xc11e: and_constant8(c, 0x04); break;
    case 0xc120: branch_to(c, !c->z, 0xc136); break;
    case 0xc122: index_constant(c, true, 0x0000); break;
    case 0xc125: video_load(c, 0x15e1, true); break;
    case 0xc128: video_store(c, 0x0200, true, c->a, true); break;
    case 0xc12b: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xc12c: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xc12d: cpy16_constant(c, 0x0020); break;
    case 0xc130: branch_to(c, !c->c, 0xc125); break;
    case 0xc132: case 0xc1b8: inc_abs8_native(c, 0x0715); break;
    case 0xc135: case 0xc1bb: return_short(c); break;
    case 0xc136: load_dp8(c, 0x4f); break;
    case 0xc138: and_constant8(c, 0x03); break;
    case 0xc13a: branch_to(c, !c->z, 0xc135); break;
    case 0xc13c: stz_dp8(c, 0x01); break;
    case 0xc13e: case 0xc14c: sep_rep_native(c, 0x20, false); break;
    case 0xc140: case 0xc142: asl_dp16(c, 0x00); break;
    case 0xc144: implied(c); c->y = c->x; zn(c, c->y, false); break;
    case 0xc145: ldx_dp16(c, 0x00); break;
    case 0xc147: lda16_constant(c, 0x000f); break;
    case 0xc14a: store_dp(c, 0x02, false); break;
    case 0xc14e: lda_abs_y16(c, 0x0200); break;
    case 0xc151: branch_to(c, c->z, 0xc159); break;
    case 0xc153: lda_long_x16(c, 0x05c1b8); break;
    case 0xc157: branch_to(c, true, 0xc15d); break;
    case 0xc159: lda_long_x16(c, 0x05c1ba); break;
    case 0xc15d: store_dp(c, 0x04, false); break;
    case 0xc15f: and_constant16(c, 0x001f); break;
    case 0xc162: store_dp(c, 0x06, false); break;
    case 0xc164: case 0xc17e: case 0xc19a: lda_abs_y16(c, 0x0200); break;
    case 0xc167: and_constant16(c, 0x001f); break;
    case 0xc16a: case 0xc184: case 0xc1a0: implied(c); c->c = false; break;
    case 0xc16b: case 0xc185: case 0xc1a1: adc_dp16(c, 0x06); break;
    case 0xc16d: cmp_constant16(c, 0x0020); break;
    case 0xc170: branch_to(c, !c->c, 0xc175); break;
    case 0xc172: lda16_constant(c, 0x001f); break;
    case 0xc175: store_dp(c, 0x10, false); break;
    case 0xc177: case 0xc193: load_dp16_to_a(c, 0x04); break;
    case 0xc179: and_constant16(c, 0x03e0); break;
    case 0xc17c: case 0xc198: store_dp(c, 0x06, false); break;
    case 0xc181: and_constant16(c, 0x03e0); break;
    case 0xc187: cmp_constant16(c, 0x0400); break;
    case 0xc18a: branch_to(c, !c->c, 0xc18f); break;
    case 0xc18c: lda16_constant(c, 0x03e0); break;
    case 0xc18f: case 0xc1ab: ora_dp16(c, 0x10); break;
    case 0xc191: store_dp(c, 0x10, false); break;
    case 0xc195: and_constant16(c, 0x7c00); break;
    case 0xc19d: and_constant16(c, 0x7c00); break;
    case 0xc1a3: cmp_constant16(c, 0x8000); break;
    case 0xc1a6: branch_to(c, !c->c, 0xc1ab); break;
    case 0xc1a8: lda16_constant(c, 0x7c00); break;
    case 0xc1ad: store_a_abs_y16(c, 0x0200); break;
    case 0xc1b0: case 0xc1b1:
      implied(c); c->y++; zn(c, c->y, false); break;
    case 0xc1b2: dec_dp16(c, 0x02); break;
    case 0xc1b4: branch_to(c, !c->n, 0xc14e); break;
    case 0xc1b6: sep_rep_native(c, 0x20, true); break;
  }
  return true;
}

/* ---- Helpers for 9626 OAM walker / C1C8 / B572 ---- */
static void and_long_x16(Cpu *c, uint32_t base) {
  (void)fetch(c); (void)fetch(c); (void)fetch(c);
  uint32_t low = (base + c->x) & 0xffffffu;
  uint16_t value = c->read(c->mem, low); sample_interrupt(c);
  value |= (uint16_t)c->read(c->mem, (low + 1) & 0xffffffu) << 8;
  c->a &= value; zn(c, c->a, false);
}
static void lda_dp_x8(Cpu *c, uint8_t offset) {
  (void)fetch(c);
  if(c->dp & 255u) idle(c);
  idle(c);
  uint16_t address = (uint16_t)(c->dp + offset + c->x);
  sample_interrupt(c);
  set_a8(c, c->read(c->mem, address));
}
/* $05:9626–$96D6: OAM/placement walker (JMP target from $9B82). Reads
 * [DP+$00] records (flags,Y,tile,attr,X) into $0400,X / $0600 high-table via
 * $0085A9 masks; preserves X across RTL; cursor in DP+$55. */
static bool oam_place_9626_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x96a0 || pc == 0x96a1 || pc == 0x96a2 || pc == 0x96a3 ||
                    pc == 0x96a4 || pc == 0x96a5 || pc == 0x96a6 || pc == 0x96a9 ||
                    pc == 0x96aa || pc == 0x96ab || pc == 0x96ae || pc == 0x96af ||
                    pc == 0x96b0 || pc == 0x96b2 || pc == 0x96b5 || pc == 0x96b6 ||
                    pc == 0x96b7 || pc == 0x96b9 || pc == 0x96ba || pc == 0x96bb ||
                    pc == 0x96bd || pc == 0x96be || pc == 0x96c0 || pc == 0x96c3 ||
                    pc == 0x96c7 || pc == 0x96c9);
  const bool sep_entry = (pc == 0x96cc);
  const bool rep_entry = (pc == 0x969e);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0x9626: case 0x9627: case 0x962a: case 0x962c: case 0x962e: case 0x962f:
    case 0x9631: case 0x9633: case 0x9635: case 0x9636: case 0x9637: case 0x9638:
    case 0x9639: case 0x963c: case 0x963e: case 0x9640: case 0x9642: case 0x9644:
    case 0x9646: case 0x9647: case 0x9648: case 0x964a: case 0x964c: case 0x964e:
    case 0x9650: case 0x9651: case 0x9653: case 0x9654: case 0x9656: case 0x9658:
    case 0x965a: case 0x965c: case 0x965e: case 0x9661: case 0x9662: case 0x9664:
    case 0x9667: case 0x9668: case 0x966a: case 0x966c: case 0x966e: case 0x9670:
    case 0x9672: case 0x9674: case 0x9676: case 0x9678: case 0x967b: case 0x967c:
    case 0x967e: case 0x9680: case 0x9682: case 0x9684: case 0x9685: case 0x9687:
    case 0x9688: case 0x968a: case 0x968c: case 0x968e: case 0x9690: case 0x9691:
    case 0x9693: case 0x9695: case 0x9696: case 0x9698: case 0x969b: case 0x969c:
    case 0x969d: case 0x969e: case 0x96a0: case 0x96a1: case 0x96a2: case 0x96a3:
    case 0x96a4: case 0x96a5: case 0x96a6: case 0x96a9: case 0x96aa: case 0x96ab:
    case 0x96ae: case 0x96af: case 0x96b0: case 0x96b2: case 0x96b5: case 0x96b6:
    case 0x96b7: case 0x96b9: case 0x96ba: case 0x96bb: case 0x96bd: case 0x96be:
    case 0x96c0: case 0x96c3: case 0x96c7: case 0x96c9: case 0x96cc: case 0x96ce:
    case 0x96cf: case 0x96d0: case 0x96d1: case 0x96d2: case 0x96d3: case 0x96d4:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x9626: phx16(c); break;
    case 0x9627: index_constant(c, true, 0x0000); break;
    case 0x962a: lda_indirect_long_y8(c, 0x00); break;
    case 0x962c: and_constant8(c, 0x01); break;
    case 0x962e: asl_a8(c); break;
    case 0x962f: store_dp(c, 0x04, true); break;
    case 0x9631: load_dp_x16(c, 0x55); break;
    case 0x9633: branch_to(c, true, 0x9638); break;
    case 0x9635: case 0x9636: case 0x9637: case 0x9638:
    case 0x9661: case 0x9667: case 0x967b: case 0x969b:
      implied(c); c->y++; zn(c, c->y, false); break;
    case 0x9639: cpx16_constant(c, 0x0200); break;
    case 0x963c: branch_to(c, c->c, 0x9644); break;
    case 0x963e: case 0x9662: case 0x9668: case 0x966e: case 0x967e:
      lda_indirect_long_y8(c, 0x00); break;
    case 0x9640: cmp_constant8(c, 0x80); break;
    case 0x9642: branch_to(c, !c->z, 0x9648); break;
    case 0x9644: store_dp_x16(c, 0x55); break;
    case 0x9646: plx16(c); break;
    case 0x9647: return_long(c); break;
    case 0x9648: case 0x967c: bit_dp8(c, 0x03); break;
    case 0x964a: branch_to(c, !c->n, 0x9653); break;
    case 0x964c: store_dp(c, 0x08, true); break;
    case 0x964e: case 0x9682: byte_constant(c, 0xf8); break;
    case 0x9650: case 0x9684: case 0x9695: implied(c); c->c = true; break;
    case 0x9651: sbc_dp8(c, 0x08); break;
    case 0x9653: case 0x9687: implied(c); c->c = false; break;
    case 0x9654: adc8_dp(c, 0x0a); break;
    case 0x9656: cmp_constant8(c, 0xf0); break;
    case 0x9658: branch_to(c, c->c, 0x965e); break;
    case 0x965a: cmp_dp8(c, 0x0e); break;
    case 0x965c: branch_to(c, c->c, 0x9635); break;
    case 0x965e: store_a_abs_x8(c, 0x0401); break;
    case 0x9664: store_a_abs_x8(c, 0x0402); break;
    case 0x966a: and_constant8(c, 0x01); break;
    case 0x966c: case 0x9691: store_dp(c, 0x05, true); break;
    case 0x9670: eor_dp8(c, 0x03); break;
    case 0x9672: and_constant8(c, 0xfe); break;
    case 0x9674: ora_dp8(c, 0x05); break;
    case 0x9676: ora_dp8(c, 0x09); break;
    case 0x9678: store_a_abs_x8(c, 0x0403); break;
    case 0x9680: branch_to(c, !c->v, 0x9687); break;
    case 0x9685: sbc_indirect_long_y8(c, 0x00); break;
    case 0x9688: branch_to(c, c->n, 0x968e); break;
    case 0x968a: adc8_dp(c, 0x0c); break;
    case 0x968c: branch_to(c, true, 0x9698); break;
    case 0x968e: eor_constant8(c, 0xff); break;
    case 0x9690: inc_a8(c); break;
    case 0x9693: load_dp8(c, 0x0c); break;
    case 0x9696: sbc_dp8(c, 0x05); break;
    case 0x9698: store_a_abs_x8(c, 0x0400); break;
    case 0x969c: phx16(c); break;
    case 0x969d: phy16(c); break;
    case 0x969e: sep_rep_native(c, 0x20, false); break;
    case 0x96a0: txa16(c); break;
    case 0x96a1: case 0x96a3: case 0x96a4: case 0x96a5: lsr_a16(c); break;
    case 0x96a2: pha16(c); break;
    case 0x96a6: and_constant16(c, 0xfffe); break;
    case 0x96a9: tay16(c); break;
    case 0x96aa: pla16(c); break;
    case 0x96ab: and_constant16(c, 0x000e); break;
    case 0x96ae: tax16(c); break;
    case 0x96af: phx16(c); break;
    case 0x96b0: load_dp16_to_a(c, 0x04); break;
    case 0x96b2: and_constant16(c, 0x0003); break;
    case 0x96b5: case 0x96b6:
      implied(c); c->x--; zn(c, c->x, false); break;
    case 0x96b7: branch_to(c, c->n, 0x96bd); break;
    case 0x96b9: case 0x96ba: asl_a16(c); break;
    case 0x96bb: branch_to(c, true, 0x96b5); break;
    case 0x96bd: plx16(c); break;
    case 0x96be: store_dp(c, 0x06, false); break;
    case 0x96c0: lda_abs_y16(c, 0x0600); break;
    case 0x96c3: and_long_x16(c, 0x0085a9); break;
    case 0x96c7: ora_dp16(c, 0x06); break;
    case 0x96c9: store_a_abs_y16(c, 0x0600); break;
    case 0x96cc: sep_rep_native(c, 0x20, true); break;
    case 0x96ce: ply16(c); break;
    case 0x96cf: plx16(c); break;
    case 0x96d0: case 0x96d1: case 0x96d2: case 0x96d3:
      implied(c); c->x++; zn(c, c->x, false); break;
    case 0x96d4: jmp_abs_native(c, 0x9639); break;
  }
  return true;
}

/* $05:96D7–$96E1: X += $20 (16-bit) then RTS. */
static bool index_add20_96d7_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x96d9 || pc == 0x96da || pc == 0x96db || pc == 0x96de);
  const bool sep_entry = (pc == 0x96df);
  const bool rep_entry = (pc == 0x96d7);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  /* REP entry accepts M8 or M16: $F952 calls while already M16. */
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0x96d7: case 0x96d9: case 0x96da: case 0x96db: case 0x96de: case 0x96df:
    case 0x96e1:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x96d7: sep_rep_native(c, 0x20, false); break;
    case 0x96d9: txa16(c); break;
    case 0x96da: implied(c); c->c = false; break;
    case 0x96db: adc16_constant(c, 0x0020); break;
    case 0x96de: tax16(c); break;
    case 0x96df: sep_rep_native(c, 0x20, true); break;
    case 0x96e1: return_short(c); break;
  }
  return true;
}

/* $05:96E2–$96EC: Y += $20 (16-bit) then RTS. */
static bool index_add20_96e2_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x96e4 || pc == 0x96e5 || pc == 0x96e6 || pc == 0x96e9);
  const bool sep_entry = (pc == 0x96ea);
  const bool rep_entry = (pc == 0x96e2);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0x96e2: case 0x96e4: case 0x96e5: case 0x96e6: case 0x96e9: case 0x96ea:
    case 0x96ec:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x96e2: sep_rep_native(c, 0x20, false); break;
    case 0x96e4: tya16(c); break;
    case 0x96e5: implied(c); c->c = false; break;
    case 0x96e6: adc16_constant(c, 0x0020); break;
    case 0x96e9: tay16(c); break;
    case 0x96ea: sep_rep_native(c, 0x20, true); break;
    case 0x96ec: return_short(c); break;
  }
  return true;
}

/* $05:B572–$B581: type&$7F → $0D8200,X table byte&$7F; RTS. */
static bool type_lookup_b572_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xb572: case 0xb573: case 0xb575: case 0xb576: case 0xb578: case 0xb579:
    case 0xb57a: case 0xb57e: case 0xb580: case 0xb581:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb572: phx16(c); break;
    case 0xb573: case 0xb57e: and_constant8(c, 0x7f); break;
    case 0xb575: case 0xb578: xba_native(c); break;
    case 0xb576: byte_constant(c, 0x00); break;
    case 0xb579: tax16(c); break;
    case 0xb57a: load_a_long_x8(c, 0x0d8200); break;
    case 0xb580: plx16(c); break;
    case 0xb581: return_short(c); break;
  }
  return true;
}

/* $05:C1C8–$C1ED: actor flag predicate via $B572 → #$80/#$81/#$82/#$83; RTS. */
static bool actor_flag_c1c8_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xc1c8: case 0xc1ca: case 0xc1cd: case 0xc1cf: case 0xc1d1: case 0xc1d4:
    case 0xc1d6: case 0xc1d8: case 0xc1d9: case 0xc1db: case 0xc1dd: case 0xc1df:
    case 0xc1e1: case 0xc1e3: case 0xc1e4: case 0xc1e6: case 0xc1e8: case 0xc1ea:
    case 0xc1eb: case 0xc1ed:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xc1c8: lda_dp_x8(c, 0x20); break;
    case 0xc1ca: jsr_abs_native(c, 0xb572); break;
    case 0xc1cd: cmp_constant8(c, 0x00); break;
    case 0xc1cf: branch_to(c, !c->z, 0xc1d9); break;
    case 0xc1d1: video_load(c, 0x01ee, false); break;
    case 0xc1d4: branch_to(c, !c->n, 0xc1d9); break;
    case 0xc1d6: byte_constant(c, 0x81); break;
    case 0xc1d8: case 0xc1e3: case 0xc1ea: case 0xc1ed: return_short(c); break;
    case 0xc1d9: lda_dp_x8(c, 0x00); break;
    case 0xc1db: and_constant8(c, 0x24); break;
    case 0xc1dd: cmp_constant8(c, 0x20); break;
    case 0xc1df: branch_to(c, !c->z, 0xc1e4); break;
    case 0xc1e1: byte_constant(c, 0x82); break;
    case 0xc1e4: cmp_constant8(c, 0x24); break;
    case 0xc1e6: branch_to(c, !c->z, 0xc1eb); break;
    case 0xc1e8: byte_constant(c, 0x83); break;
    case 0xc1eb: byte_constant(c, 0x80); break;
  }
  return true;
}

/* $05:C1EE–$C272: actor timer/anim mask — Y from $059B89,X; scene $13/$14 may
 * clear $0029,Y early. Else dec timer, optional $0021 bit7, index $05C273 mask
 * into DP+$10, then AND-mask words at [$00] via $15E9,X queue length. Mid-entry
 * $C20D skips the scene clear; $C216 is the shared body. Data follows at $C273. */
static bool actor_timer_c1ee_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 =
      (pc == 0xc1f0 || pc == 0xc1f4 || pc == 0xc20f || pc == 0xc213 ||
       pc == 0xc24b || pc == 0xc24f || pc == 0xc251 || pc == 0xc252 ||
       pc == 0xc255 || pc == 0xc256 || pc == 0xc259 || pc == 0xc25b ||
       pc == 0xc25e || pc == 0xc260 || pc == 0xc263 || pc == 0xc264 ||
       pc == 0xc265 || pc == 0xc266 || pc == 0xc268 || pc == 0xc26a ||
       pc == 0xc26c || pc == 0xc26d || pc == 0xc26e);
  const bool sep_entry = (pc == 0xc1f5 || pc == 0xc214 || pc == 0xc270);
  const bool rep_entry = (pc == 0xc1ee || pc == 0xc20d || pc == 0xc249);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xc1ee: case 0xc1f0: case 0xc1f4: case 0xc1f5: case 0xc1f7: case 0xc1fa:
    case 0xc1fc: case 0xc1fe: case 0xc200: case 0xc202: case 0xc205: case 0xc207:
    case 0xc209: case 0xc20c: case 0xc20d: case 0xc20f: case 0xc213: case 0xc214:
    case 0xc216: case 0xc219: case 0xc21b: case 0xc21d: case 0xc220: case 0xc221:
    case 0xc224: case 0xc227: case 0xc229: case 0xc22c: case 0xc22e: case 0xc230:
    case 0xc233: case 0xc235: case 0xc238: case 0xc23b: case 0xc23d: case 0xc23f:
    case 0xc241: case 0xc242: case 0xc243: case 0xc245: case 0xc246: case 0xc247:
    case 0xc248: case 0xc249: case 0xc24b: case 0xc24f: case 0xc251: case 0xc252:
    case 0xc255: case 0xc256: case 0xc259: case 0xc25b: case 0xc25e: case 0xc260:
    case 0xc263: case 0xc264: case 0xc265: case 0xc266: case 0xc268: case 0xc26a:
    case 0xc26c: case 0xc26d: case 0xc26e: case 0xc270: case 0xc272:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xc1ee: case 0xc20d: case 0xc249: sep_rep_native(c, 0x20, false); break;
    case 0xc1f0: case 0xc20f: lda_long_x16(c, 0x059b89); break;
    case 0xc1f4: case 0xc213: case 0xc255: case 0xc263: tay16(c); break;
    case 0xc1f5: case 0xc214: case 0xc270: sep_rep_native(c, 0x20, true); break;
    case 0xc1f7: video_load(c, 0x1474, false); break;
    case 0xc1fa: cmp_constant8(c, 0x13); break;
    case 0xc1fc: branch_to(c, c->z, 0xc202); break;
    case 0xc1fe: cmp_constant8(c, 0x14); break;
    case 0xc200: branch_to(c, !c->z, 0xc216); break;
    case 0xc202: load_a_abs_y8(c, 0x0029); break;
    case 0xc205: branch_to(c, c->n, 0xc216); break;
    case 0xc207: byte_constant(c, 0x00); break;
    case 0xc209: store_a_abs_y8(c, 0x0029); break;
    case 0xc20c: case 0xc272: return_long(c); break;
    case 0xc216: case 0xc21d: case 0xc229: case 0xc238: load_a_abs_y8(c, 0x0029); break;
    case 0xc219: case 0xc22c: case 0xc23d: and_constant8(c, 0x7f); break;
    case 0xc21b: branch_to(c, c->z, 0xc272); break;
    case 0xc220: dec_a8(c); break;
    case 0xc221: store_a_abs_y8(c, 0x0029); break;
    case 0xc224: case 0xc230: load_a_abs_y8(c, 0x0021); break;
    case 0xc227: branch_to(c, c->n, 0xc272); break;
    case 0xc22e: branch_to(c, c->z, 0xc238); break;
    case 0xc233: ora_constant8(c, 0x80); break;
    case 0xc235: store_a_abs_y8(c, 0x0021); break;
    case 0xc23b: branch_to(c, !c->n, 0xc241); break;
    case 0xc23f: ora_constant8(c, 0x10); break;
    case 0xc241: asl_a8(c); break;
    case 0xc242: pha8(c); break;
    case 0xc243: byte_constant(c, 0x00); break;
    case 0xc245: xba_native(c); break;
    case 0xc246: pla8(c); break;
    case 0xc247: phx16(c); break;
    case 0xc248: tax16(c); break;
    case 0xc24b: lda_long_x16(c, 0x05c273); break;
    case 0xc24f: store_dp(c, 0x10, false); break;
    case 0xc251: plx16(c); break;
    case 0xc252: video_load(c, 0x15e9, true); break;
    case 0xc256: lda_abs_y16(c, 0x0803); break;
    case 0xc259: store_dp(c, 0x00, false); break;
    case 0xc25b: lda_abs_y16(c, 0x0805); break;
    case 0xc25e: store_dp(c, 0x02, false); break;
    case 0xc260: lda_abs_y16(c, 0x0806); break;
    case 0xc264: case 0xc265: case 0xc26c: case 0xc26d:
      implied(c); c->y--; zn(c, c->y, false); break;
    case 0xc266: lda_indirect_long_y16(c, 0x00); break;
    case 0xc268: and_dp16(c, 0x10); break;
    case 0xc26a: sta_indirect_long_y16(c, 0x00); break;
    case 0xc26e: branch_to(c, !c->n, 0xc266); break;
  }
  return true;
}

/* $05:E3CC–$E3F1: scale helper for $E38E — $05E400,X × DP+$03 via $4202/$4203,
 * then ± $05E3F2,X using $4216 (sign bit of factor selects ADC vs SBC). */
static bool scale_helper_e3cc_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xe3cc: case 0xe3d0: case 0xe3d2: case 0xe3d5: case 0xe3d7: case 0xe3da:
    case 0xe3de: case 0xe3e0: case 0xe3e4: case 0xe3e5: case 0xe3e8: case 0xe3e9:
    case 0xe3ed: case 0xe3ee: case 0xe3f1:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe3cc: case 0xe3da: load_a_long_x8(c, 0x05e400); break;
    case 0xe3d0: and_constant8(c, 0x7f); break;
    case 0xe3d2: store_register8(c, 0x4202); break;
    case 0xe3d5: load_dp8(c, 0x03); break;
    case 0xe3d7: store_register8(c, 0x4203); break;
    case 0xe3de: branch_to(c, c->n, 0xe3e9); break;
    case 0xe3e0: case 0xe3e9: load_a_long_x8(c, 0x05e3f2); break;
    case 0xe3e4: implied(c); c->c = false; break;
    case 0xe3e5: adc_abs8(c, 0x4216); break;
    case 0xe3e8: case 0xe3f1: return_short(c); break;
    case 0xe3ed: implied(c); c->c = true; break;
    case 0xe3ee: sbc_abs8(c, 0x4216); break;
  }
  return true;
}

/* $05:E38E–$E3CB: sole $9626 JSL caller — gate $1546<$8A, walk 7 pointer words
 * from $05E40E into [DP+$00] bank #$05, scale Y/X via $E3CC into DP+$0A/$0C,
 * clear facing/or-attr, JSL $9626; INC $1546; RTS. */
static bool oam_caller_e38e_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xe39a || pc == 0xe39e);
  const bool sep_entry = (pc == 0xe3a0);
  const bool rep_entry = (pc == 0xe398);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xe38e: case 0xe391: case 0xe393: case 0xe395: case 0xe398: case 0xe39a:
    case 0xe39e: case 0xe3a0: case 0xe3a2: case 0xe3a4: case 0xe3a6: case 0xe3a9:
    case 0xe3ab: case 0xe3ad: case 0xe3b0: case 0xe3b2: case 0xe3b3: case 0xe3b6:
    case 0xe3b8: case 0xe3ba: case 0xe3bc: case 0xe3bd: case 0xe3c1: case 0xe3c2:
    case 0xe3c3: case 0xe3c6: case 0xe3c8: case 0xe3cb:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe38e: case 0xe3a6: video_load(c, 0x1546, false); break;
    case 0xe391: cmp_constant8(c, 0x8a); break;
    case 0xe393: branch_to(c, c->c, 0xe3c8); break;
    case 0xe395: index_constant(c, false, 0x0000); break;
    case 0xe398: sep_rep_native(c, 0x20, false); break;
    case 0xe39a: lda_long_x16(c, 0x05e40e); break;
    case 0xe39e: store_dp(c, 0x00, false); break;
    case 0xe3a0: sep_rep_native(c, 0x20, true); break;
    case 0xe3a2: byte_constant(c, 0x05); break;
    case 0xe3a4: store_dp(c, 0x02, true); break;
    case 0xe3a9: and_constant8(c, 0x7f); break;
    case 0xe3ab: store_dp(c, 0x03, true); break;
    case 0xe3ad: case 0xe3b3: jsr_abs_native(c, 0xe3cc); break;
    case 0xe3b0: store_dp(c, 0x0a, true); break;
    case 0xe3b2: case 0xe3c2:
      implied(c); c->x++; zn(c, c->x, false); break;
    case 0xe3b6: store_dp(c, 0x0c, true); break;
    case 0xe3b8: stz_dp8(c, 0x03); break;
    case 0xe3ba: stz_dp8(c, 0x09); break;
    case 0xe3bc: phx16(c); break;
    case 0xe3bd: jsl_to(c, 0x9626); break;
    case 0xe3c1: plx16(c); break;
    case 0xe3c3: cpx16_constant(c, 0x000e); break;
    case 0xe3c6: branch_to(c, !c->c, 0xe398); break;
    case 0xe3c8: inc_abs8_native(c, 0x1546); break;
    case 0xe3cb: return_short(c); break;
  }
  return true;
}

/* $05:86DE–$871C: dual $B110 / $B045 prologue then dual $C1C8 into $153B/$153C;
 * tick DP+$AB; sync $1480/$14A0. Join point for several 85xx JMPs. */
static bool actor_flag_wrap_86f5_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x86de: case 0x86e1: case 0x86e4: case 0x86e7: case 0x86ea: case 0x86ec:
    case 0x86ef: case 0x86f2: case 0x86f5: case 0x86f8: case 0x86fb: case 0x86fe:
    case 0x8701: case 0x8704: case 0x8707: case 0x8709: case 0x870b: case 0x870e:
    case 0x8710: case 0x8711: case 0x8713: case 0x8714: case 0x8716: case 0x8719:
    case 0x871c:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x86de: index_constant(c, true, 0x1400); break;
    case 0x86e1: case 0x86e7: jsr_abs_native(c, 0xb110); break;
    case 0x86e4: index_constant(c, true, 0x1440); break;
    case 0x86ea: byte_constant(c, 0x0f); break;
    case 0x86ec: store_register8(c, 0x1429); break;
    case 0x86ef: store_register8(c, 0x1469); break;
    case 0x86f2: jsr_abs_native(c, 0xb045); break;
    case 0x86f5: index_constant(c, false, 0x1400); break;
    case 0x86f8: case 0x8701: jsr_abs_native(c, 0xc1c8); break;
    case 0x86fb: store_register8(c, 0x153b); break;
    case 0x86fe: index_constant(c, false, 0x1440); break;
    case 0x8704: store_register8(c, 0x153c); break;
    case 0x8707: stz_dp8(c, 0xac); break;
    case 0x8709: inc_dp8(c, 0xab); break;
    case 0x870b: video_load(c, 0x1480, false); break;
    case 0x870e: cmp_constant8(c, 0x69); break;
    case 0x8710: php_native(c); break;
    case 0x8711: byte_constant(c, 0x80); break;
    case 0x8713: plp_native(c); break;
    case 0x8714: branch_to(c, c->z, 0x8719); break;
    case 0x8716: store_register8(c, 0x1480); break;
    case 0x8719: store_register8(c, 0x14a0); break;
    case 0x871c: return_long(c); break;
  }
  return true;
}

/* $1D:8807–$8842: after JML trampoline at $8803 — if $1559,X clear, seed
 * $1558/$1559/$155C/$155A/$155B/$155D from dual RNG streams; RTL. Enables bank-1D. */
static bool slot_seed_1d8807_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x1d || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x8807: case 0x880a: case 0x880c: case 0x8810: case 0x8812: case 0x8813:
    case 0x8815: case 0x8818: case 0x881c: case 0x881e: case 0x881f: case 0x8822:
    case 0x8826: case 0x8828: case 0x8829: case 0x882b: case 0x882e: case 0x8832:
    case 0x8834: case 0x8835: case 0x8837: case 0x883a: case 0x883c: case 0x883f:
    case 0x8842:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8807: load_a_abs_x8(c, 0x1559); break;
    case 0x880a: branch_to(c, !c->z, 0x8842); break;
    case 0x880c: case 0x8822: jsl_to(c, 0x82a3); break;
    case 0x8810: and_constant8(c, 0x03); break;
    case 0x8812: case 0x8828: case 0x8834: implied(c); c->c = false; break;
    case 0x8813: adc8_constant(c, 0xa0); break;
    case 0x8815: store_a_abs_x8(c, 0x1558); break;
    case 0x8818: case 0x882e: jsl_to(c, 0x82b5); break;
    case 0x881c: and_constant8(c, 0x07); break;
    case 0x881e: inc_a8(c); break;
    case 0x881f: store_a_abs_x8(c, 0x1559); break;
    case 0x8826: case 0x8832: and_constant8(c, 0x2f); break;
    case 0x8829: adc8_constant(c, 0x68); break;
    case 0x882b: store_a_abs_x8(c, 0x155c); break;
    case 0x8835: adc8_constant(c, 0x48); break;
    case 0x8837: store_a_abs_x8(c, 0x155a); break;
    case 0x883a: byte_constant(c, 0x00); break;
    case 0x883c: store_a_abs_x8(c, 0x155b); break;
    case 0x883f: store_a_abs_x8(c, 0x155d); break;
    case 0x8842: return_long(c); break;
  }
  return true;
}

/* $05:8857–$8886: C07B caller leaf — CPY gates; optional $C98F/$CA07/$C07B;
 * seed DP+$AB/$AC/$B6; optional JSL $0593A9; RTL. */
static bool actor_c07b_caller_8857_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x8857: case 0x885a: case 0x885c: case 0x885e: case 0x8861: case 0x8864:
    case 0x8866: case 0x8869: case 0x886c: case 0x886e: case 0x8870: case 0x8873:
    case 0x8875: case 0x8877: case 0x8879: case 0x887b: case 0x887d: case 0x887f:
    case 0x8882: case 0x8886:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8857: cpy16_constant(c, 0x0042); break;
    case 0x885a: branch_to(c, !c->z, 0x8870); break;
    case 0x885c: byte_constant(c, 0x5c); break;
    case 0x885e: jsr_abs_native(c, 0xc98f); break;
    case 0x8861: jsr_abs_native(c, 0xca07); break;
    case 0x8864: stz_dp8(c, 0xb0); break;
    case 0x8866: index_constant(c, true, 0x1400); break;
    case 0x8869: jsr_abs_native(c, 0xc07b); break;
    case 0x886c: byte_constant(c, 0x18); break;
    case 0x886e: branch_to(c, true, 0x887b); break;
    case 0x8870: cpy16_constant(c, 0x00ea); break;
    case 0x8873: branch_to(c, !c->z, 0x8886); break;
    case 0x8875: byte_constant(c, 0x03); break;
    case 0x8877: store_dp(c, 0xb6, true); break;
    case 0x8879: byte_constant(c, 0x15); break;
    case 0x887b: store_dp(c, 0xab, true); break;
    case 0x887d: stz_dp8(c, 0xac); break;
    case 0x887f: load_y_abs16(c, 0x14e4); break;
    case 0x8882: jsl_to(c, 0x93a9); break;
    case 0x8886: return_long(c); break;
  }
  return true;
}

/* $05:B0B1–$B0BD: dual $C07B palette push for slots $1400/$1440; RTS. */
static bool actor_dual_c07b_b0b1_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xb0b1: case 0xb0b4: case 0xb0b7: case 0xb0ba: case 0xb0bd:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb0b1: index_constant(c, true, 0x1400); break;
    case 0xb0b4: case 0xb0ba: jsr_abs_native(c, 0xc07b); break;
    case 0xb0b7: index_constant(c, true, 0x1440); break;
    case 0xb0bd: return_short(c); break;
  }
  return true;
}

/* $1D:8843–$8905: timer/$1570 walker after $8807 seed — BMI gate, slot walk with
 * table $1D8968/69/6B, scale via ASL/ROR/×3, JSR $8906 per entry; RTL. */
static bool timer_walk_1d8843_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x1d || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x886a || pc == 0x886d || pc == 0x886f || pc == 0x8872 ||
                    pc == 0x887a || pc == 0x887e || pc == 0x8880 || pc == 0x8884 ||
                    pc == 0x8886 || pc == 0x8889 || pc == 0x888c || pc == 0x888f ||
                    pc == 0x8891 || pc == 0x8894 || pc == 0x8897 || pc == 0x889a ||
                    pc == 0x889c || pc == 0x889e || pc == 0x889f || pc == 0x88a1 ||
                    pc == 0x88a3 || pc == 0x88a4 || pc == 0x88a6 || pc == 0x88a8 ||
                    pc == 0x88ab || pc == 0x88ad || pc == 0x88b0 || pc == 0x88b2 ||
                    pc == 0x88b4 || pc == 0x88b6 || pc == 0x88b7 || pc == 0x88b8 ||
                    pc == 0x88ba || pc == 0x88bc || pc == 0x88be || pc == 0x88c0 ||
                    pc == 0x88c1 || pc == 0x88c2 || pc == 0x88c4 || pc == 0x88c6 ||
                    pc == 0x88c8 || pc == 0x88ca || pc == 0x88cc || pc == 0x88cf ||
                    pc == 0x88d0 || pc == 0x88d2 || pc == 0x88d5 || pc == 0x88d7 ||
                    pc == 0x88da || pc == 0x88db || pc == 0x88dd || pc == 0x88e0);
  const bool sep_entry = (pc == 0x8874 || pc == 0x88e2);
  const bool rep_entry = (pc == 0x8868 || pc == 0x8878);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0x8843: case 0x8846: case 0x8848: case 0x8849: case 0x884c: case 0x884f:
    case 0x8852: case 0x8854: case 0x8856: case 0x8858: case 0x885a: case 0x885c:
    case 0x885d: case 0x8860: case 0x8863: case 0x8865: case 0x8868: case 0x886a:
    case 0x886d: case 0x886f: case 0x8872: case 0x8874: case 0x8876: case 0x8878:
    case 0x887a: case 0x887e: case 0x8880: case 0x8884: case 0x8886: case 0x8889:
    case 0x888c: case 0x888f: case 0x8891: case 0x8894: case 0x8897: case 0x889a:
    case 0x889c: case 0x889e: case 0x889f: case 0x88a1: case 0x88a3: case 0x88a4:
    case 0x88a6: case 0x88a8: case 0x88ab: case 0x88ad: case 0x88b0: case 0x88b2:
    case 0x88b4: case 0x88b6: case 0x88b7: case 0x88b8: case 0x88ba: case 0x88bc:
    case 0x88be: case 0x88c0: case 0x88c1: case 0x88c2: case 0x88c4: case 0x88c6:
    case 0x88c8: case 0x88ca: case 0x88cc: case 0x88cf: case 0x88d0: case 0x88d2:
    case 0x88d5: case 0x88d7: case 0x88da: case 0x88db: case 0x88dd: case 0x88e0:
    case 0x88e2: case 0x88e4: case 0x88e6: case 0x88e7: case 0x88eb: case 0x88ec:
    case 0x88ed: case 0x88ee: case 0x88f1: case 0x88f2: case 0x88f3: case 0x88f4:
    case 0x88f5: case 0x88f6: case 0x88f7: case 0x88f8: case 0x88f9: case 0x88fa:
    case 0x88fb: case 0x88fc: case 0x88fd: case 0x8900: case 0x8902: case 0x8905:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8843: video_load(c, 0x1570, false); break;
    case 0x8846: branch_to(c, c->n, 0x8849); break;
    case 0x8848: case 0x8905: return_long(c); break;
    case 0x8849: index_constant(c, false, 0x0000); break;
    case 0x884c: index_constant(c, true, 0x0001); break;
    case 0x884f: load_a_abs_y8(c, 0x1570); break;
    case 0x8852: branch_to(c, c->z, 0x8878); break;
    case 0x8854: cmp_constant8(c, 0xff); break;
    case 0x8856: branch_to(c, c->z, 0x8865); break;
    case 0x8858: cmp_constant8(c, 0xfe); break;
    case 0x885a: branch_to(c, c->z, 0x8868); break;
    case 0x885c: dec_a8(c); break;
    case 0x885d: store_a_abs_y8(c, 0x1570); break;
    case 0x8860: cpx16_constant(c, 0x0037); break;
    case 0x8863: branch_to(c, !c->c, 0x8868); break;
    case 0x8865: jmp_abs_native(c, 0x88f3); break;
    case 0x8868: case 0x8878: sep_rep_native(c, 0x20, false); break;
    case 0x886a: lda_abs_y16(c, 0x1571); break;
    case 0x886d: case 0x887e: store_dp(c, 0x0a, false); break;
    case 0x886f: lda_abs_y16(c, 0x1573); break;
    case 0x8872: case 0x8884: store_dp(c, 0x0c, false); break;
    case 0x8874: case 0x88e2: sep_rep_native(c, 0x20, true); break;
    case 0x8876: branch_to(c, true, 0x88e4); break;
    case 0x887a: lda_long_x16(c, 0x1d8969); break;
    case 0x8880: lda_long_x16(c, 0x1d896b); break;
    case 0x8886: video_load(c, 0x01d6, false); break;
    case 0x8889: and_constant16(c, 0x0060); break;
    case 0x888c: cmp_constant16(c, 0x0060); break;
    case 0x888f: branch_to(c, !c->z, 0x88cc); break;
    case 0x8891: video_load(c, 0x14cc, false); break;
    case 0x8894: and_constant16(c, 0x00ff); break;
    case 0x8897: cmp_constant16(c, 0x0015); break;
    case 0x889a: branch_to(c, c->c, 0x88a8); break;
    case 0x889c: case 0x88b2: load_dp16_to_a(c, 0x0a); break;
    case 0x889e: case 0x88a3: case 0x88b6: case 0x88c0: asl_a16(c); break;
    case 0x889f: ror_dp16(c, 0x0a); break;
    case 0x88a1: case 0x88bc: load_dp16_to_a(c, 0x0c); break;
    case 0x88a4: ror_dp16(c, 0x0c); break;
    case 0x88a6: branch_to(c, true, 0x88cc); break;
    case 0x88a8: cmp_constant16(c, 0x0023); break;
    case 0x88ab: branch_to(c, !c->c, 0x88cc); break;
    case 0x88ad: cmp_constant16(c, 0x0031); break;
    case 0x88b0: branch_to(c, c->c, 0x88c8); break;
    case 0x88b4: case 0x88be: store_dp(c, 0x00, false); break;
    case 0x88b7: case 0x88c1: case 0x88cf: case 0x88da: implied(c); c->c = false; break;
    case 0x88b8: adc_dp16(c, 0x00); break;
    case 0x88ba: store_dp(c, 0x0a, false); break;
    case 0x88c2: adc_dp16(c, 0x00); break;
    case 0x88c4: store_dp(c, 0x0c, false); break;
    case 0x88c6: branch_to(c, true, 0x889c); break;
    case 0x88c8: asl_dp16(c, 0x0a); break;
    case 0x88ca: asl_dp16(c, 0x0c); break;
    case 0x88cc: lda_abs_y16(c, 0x1571); break;
    case 0x88d0: adc_dp16(c, 0x0a); break;
    case 0x88d2: store_a_abs_y16(c, 0x1571); break;
    case 0x88d5: store_dp(c, 0x0a, false); break;
    case 0x88d7: lda_abs_y16(c, 0x1573); break;
    case 0x88db: adc_dp16(c, 0x0c); break;
    case 0x88dd: store_a_abs_y16(c, 0x1573); break;
    case 0x88e0: store_dp(c, 0x0c, false); break;
    case 0x88e4: byte_constant(c, 0x00); break;
    case 0x88e6: xba_native(c); break;
    case 0x88e7: load_a_long_x8(c, 0x1d8968); break;
    case 0x88eb: phy16(c); break;
    case 0x88ec: phx16(c); break;
    case 0x88ed: tax16(c); break;
    case 0x88ee: jsr_abs_native(c, 0x8906); break;
    case 0x88f1: plx16(c); break;
    case 0x88f2: ply16(c); break;
    case 0x88f3: case 0x88f4: case 0x88f5: case 0x88f6: case 0x88f7:
      implied(c); c->y++; zn(c, c->y, false); break;
    case 0x88f8: case 0x88f9: case 0x88fa: case 0x88fb: case 0x88fc:
      implied(c); c->x++; zn(c, c->x, false); break;
    case 0x88fd: cpx16_constant(c, 0x0069); break;
    case 0x8900: branch_to(c, c->c, 0x8905); break;
    case 0x8902: jmp_abs_native(c, 0x884f); break;
  }
  return true;
}

/* $05:9768–$97C8: clear $80 markers via $96E2 walks; copy $17C6→$0140; seed
 * scroll/$0738; JSL $0091BD; DP+$24=#$82; RTL. */
static bool actor_clear_9768_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x9768: case 0x976b: case 0x976e: case 0x9770: case 0x9772: case 0x9774:
    case 0x9777: case 0x977a: case 0x977d: case 0x977f: case 0x9782: case 0x9785:
    case 0x9787: case 0x9789: case 0x978b: case 0x978e: case 0x9791: case 0x9794:
    case 0x9796: case 0x9799: case 0x979c: case 0x979f: case 0x97a0: case 0x97a3:
    case 0x97a5: case 0x97a8: case 0x97aa: case 0x97ad: case 0x97af: case 0x97b2:
    case 0x97b5: case 0x97b8: case 0x97ba: case 0x97bd: case 0x97c0: case 0x97c4:
    case 0x97c6: case 0x97c8:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x9768: case 0x977f: case 0x9796: index_constant(c, true, 0x0000); break;
    case 0x976b: load_a_abs_y8(c, 0x0b19); break;
    case 0x976e: case 0x9785: cmp_constant8(c, 0x80); break;
    case 0x9770: branch_to(c, !c->z, 0x9777); break;
    case 0x9772: case 0x9789: byte_constant(c, 0x00); break;
    case 0x9774: store_a_abs_y8(c, 0x0b19); break;
    case 0x9777: case 0x978e: jsr_abs_native(c, 0x96e2); break;
    case 0x977a: cpy16_constant(c, 0x0120); break;
    case 0x977d: branch_to(c, !c->c, 0x976b); break;
    case 0x9782: load_a_abs_y8(c, 0x0e19); break;
    case 0x9787: branch_to(c, !c->z, 0x978e); break;
    case 0x978b: store_a_abs_y8(c, 0x0e19); break;
    case 0x9791: cpy16_constant(c, 0x00a0); break;
    case 0x9794: branch_to(c, !c->c, 0x9782); break;
    case 0x9799: load_a_abs_y8(c, 0x17c6); break;
    case 0x979c: store_a_abs_y8(c, 0x0140); break;
    case 0x979f: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x97a0: cpy16_constant(c, 0x000c); break;
    case 0x97a3: branch_to(c, !c->c, 0x9799); break;
    case 0x97a5: load_y_abs16(c, 0x17d3); break;
    case 0x97a8: sty_dp16(c, 0x33); break;
    case 0x97aa: load_y_abs16(c, 0x17d5); break;
    case 0x97ad: sty_dp16(c, 0x35); break;
    case 0x97af: stz_abs8(c, 0x15e8); break;
    case 0x97b2: index_constant(c, true, 0x8000); break;
    case 0x97b5: store_y_abs16(c, 0x0738); break;
    case 0x97b8: byte_constant(c, 0x07); break;
    case 0x97ba: store_register8(c, 0x073a); break;
    case 0x97bd: stz_abs8(c, 0x071b); break;
    case 0x97c0: jsl_to(c, 0x91bd); break;
    case 0x97c4: byte_constant(c, 0x82); break;
    case 0x97c6: store_dp(c, 0x24, true); break;
    case 0x97c8: return_long(c); break;
  }
  return true;
}

/* $05:B110–$B11C: clear 32 bytes at $0020,Y (X=#$001F..0); RTS. */
static bool actor_clear_b110_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xb110: case 0xb113: case 0xb115: case 0xb118: case 0xb119: case 0xb11a:
    case 0xb11c:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb110: index_constant(c, false, 0x001f); break;
    case 0xb113: byte_constant(c, 0x00); break;
    case 0xb115: store_a_abs_y8(c, 0x0020); break;
    case 0xb118: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xb119: implied(c); c->x--; zn(c, c->x, false); break;
    case 0xb11a: branch_to(c, !c->n, 0xb115); break;
    case 0xb11c: return_short(c); break;
  }
  return true;
}

/* $05:B045–$B0B0: seed $1421/$1461 via $B0BE/$B0F9 + $B582 gates; clear
 * velocity/scroll bytes; falls into dual $C07B at $B0B1. */
static bool actor_seed_b045_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xb045: case 0xb048: case 0xb04b: case 0xb04d: case 0xb050: case 0xb053:
    case 0xb055: case 0xb058: case 0xb05b: case 0xb05d: case 0xb060: case 0xb062:
    case 0xb064: case 0xb066: case 0xb068: case 0xb06a: case 0xb06d: case 0xb070:
    case 0xb073: case 0xb075: case 0xb078: case 0xb07b: case 0xb07d: case 0xb080:
    case 0xb083: case 0xb086: case 0xb089: case 0xb08b: case 0xb08e: case 0xb091:
    case 0xb093: case 0xb096: case 0xb099: case 0xb09c: case 0xb09f: case 0xb0a2:
    case 0xb0a5: case 0xb0a8: case 0xb0ab: case 0xb0ae:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb045: jsr_abs_native(c, 0xb0be); break;
    case 0xb048: jsr_abs_native(c, 0xb0f9); break;
    case 0xb04b: byte_constant(c, 0x85); break;
    case 0xb04d: store_register8(c, 0x1421); break;
    case 0xb050: store_register8(c, 0x1461); break;
    case 0xb053: byte_constant(c, 0x97); break;
    case 0xb055: ldx_abs16(c, 0x1424); break;
    case 0xb058: cpx16_constant(c, 0x0080); break;
    case 0xb05b: branch_to(c, c->z, 0xb06a); break;
    case 0xb05d: video_load(c, 0x1401, false); break;
    case 0xb060: cmp_constant8(c, 0x53); break;
    case 0xb062: branch_to(c, c->z, 0xb068); break;
    case 0xb064: cmp_constant8(c, 0x6b); break;
    case 0xb066: branch_to(c, !c->z, 0xb06d); break;
    case 0xb068: byte_constant(c, 0x80); break;
    case 0xb06a: store_register8(c, 0x1421); break;
    case 0xb06d: video_load(c, 0x1420, false); break;
    case 0xb070: case 0xb086: jsr_abs_native(c, 0xb582); break;
    case 0xb073: branch_to(c, !c->z, 0xb07d); break;
    case 0xb075: stz_abs8(c, 0x1421); break;
    case 0xb078: case 0xb08e: index_constant(c, false, 0x0100); break;
    case 0xb07b: branch_to(c, true, 0xb080); break;
    case 0xb07d: case 0xb093: index_constant(c, false, 0x0070); break;
    case 0xb080: stx_abs16(c, 0x1426); break;
    case 0xb083: video_load(c, 0x1460, false); break;
    case 0xb089: branch_to(c, !c->z, 0xb093); break;
    case 0xb08b: stz_abs8(c, 0x1461); break;
    case 0xb091: branch_to(c, true, 0xb096); break;
    case 0xb096: stx_abs16(c, 0x1466); break;
    case 0xb099: stz_abs8(c, 0x142e); break;
    case 0xb09c: stz_abs8(c, 0x142f); break;
    case 0xb09f: stz_abs8(c, 0x1435); break;
    case 0xb0a2: stz_abs8(c, 0x1436); break;
    case 0xb0a5: stz_abs8(c, 0x146e); break;
    case 0xb0a8: stz_abs8(c, 0x146f); break;
    case 0xb0ab: stz_abs8(c, 0x1475); break;
    case 0xb0ae: stz_abs8(c, 0x1476); break;
  }
  return true;
}

/* $05:90CC–$9114: if $14E4==$E8 and DP+$AC, seed actor $1400 words (REP/SEP),
 * JSR $C07B, STZ $B7; always JSL $9488; $14E4==$EA → AB=#$82 else #$01; RTL. */
static bool actor_c07b_caller_90cc_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x90de || pc == 0x90e1 || pc == 0x90e4 || pc == 0x90e7 ||
                    pc == 0x90ea || pc == 0x90ed || pc == 0x90f0 || pc == 0x90f3);
  const bool sep_entry = (pc == 0x90f6);
  const bool rep_entry = (pc == 0x90dc);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0x90cc: case 0x90cf: case 0x90d1: case 0x90d3: case 0x90d5: case 0x90d7:
    case 0x90d9: case 0x90dc: case 0x90de: case 0x90e1: case 0x90e4: case 0x90e7:
    case 0x90ea: case 0x90ed: case 0x90f0: case 0x90f3: case 0x90f6: case 0x90f8:
    case 0x90fb: case 0x90fd: case 0x9101: case 0x9104: case 0x9106: case 0x9108:
    case 0x910b: case 0x910d: case 0x910f: case 0x9110: case 0x9112: case 0x9114:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x90cc: video_load(c, 0x14e4, false); break;
    case 0x90cf: cmp_constant8(c, 0xe8); break;
    case 0x90d1: branch_to(c, !c->z, 0x90fd); break;
    case 0x90d3: load_dp8(c, 0xac); break;
    case 0x90d5: branch_to(c, c->z, 0x90fd); break;
    case 0x90d7: inc_dp8(c, 0xac); break;
    case 0x90d9: index_constant(c, true, 0x1400); break;
    case 0x90dc: sep_rep_native(c, 0x20, false); break;
    case 0x90de: lda16_constant(c, 0xa40f); break;
    case 0x90e1: store_a_abs_y16(c, 0x0020); break;
    case 0x90e4: lda16_constant(c, 0x0042); break;
    case 0x90e7: store_a_abs_y16(c, 0x0022); break;
    case 0x90ea: lda16_constant(c, 0x0040); break;
    case 0x90ed: store_a_abs_y16(c, 0x0024); break;
    case 0x90f0: lda16_constant(c, 0x0070); break;
    case 0x90f3: store_a_abs_y16(c, 0x0026); break;
    case 0x90f6: sep_rep_native(c, 0x20, true); break;
    case 0x90f8: jsr_abs_native(c, 0xc07b); break;
    case 0x90fb: stz_dp8(c, 0xb7); break;
    case 0x90fd: jsl_to(c, 0x9488); break;
    case 0x9101: video_load(c, 0x14e4, false); break;
    case 0x9104: cmp_constant8(c, 0xea); break;
    case 0x9106: branch_to(c, !c->z, 0x9110); break;
    case 0x9108: stz_abs8(c, 0x14e4); break;
    case 0x910b: byte_constant(c, 0x82); break;
    case 0x910d: store_dp(c, 0xab, true); break;
    case 0x910f: case 0x9114: return_long(c); break;
    case 0x9110: byte_constant(c, 0x01); break;
    case 0x9112: store_dp(c, 0xab, true); break;
  }
  return true;
}

/* $05:8116–$81EA: dual $C07B; optional $7F wipe / $E018 / $F9B0; $15E8/$B7 gate;
 * $9391; clear $0D00; bank-1D JSL chain; palette/PPU seeds; RTL. */
static bool actor_c07b_caller_8116_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x81bd || pc == 0x81c0);
  const bool sep_entry = (pc == 0x81c3);
  const bool rep_entry = (pc == 0x81bb);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0x8116: case 0x8119: case 0x811c: case 0x811f: case 0x8122: case 0x8125:
    case 0x8127: case 0x812b: case 0x812e: case 0x8130: case 0x8134: case 0x8135:
    case 0x8137: case 0x8139: case 0x813c: case 0x813e: case 0x8141: case 0x8144:
    case 0x8147: case 0x814b: case 0x814e: case 0x8151: case 0x8154: case 0x8157:
    case 0x815a: case 0x815d: case 0x815f: case 0x8161: case 0x8163: case 0x8165:
    case 0x8167: case 0x8169: case 0x816b: case 0x816e: case 0x8170: case 0x8173:
    case 0x8176: case 0x8179: case 0x817a: case 0x817c: case 0x8180: case 0x8184:
    case 0x8188: case 0x818c: case 0x8190: case 0x8194: case 0x8198: case 0x819c:
    case 0x81a0: case 0x81a4: case 0x81a8: case 0x81ac: case 0x81af: case 0x81b1:
    case 0x81b5: case 0x81b8: case 0x81bb: case 0x81bd: case 0x81c0: case 0x81c3:
    case 0x81c5: case 0x81c7: case 0x81c9: case 0x81cc: case 0x81ce: case 0x81d0:
    case 0x81d2: case 0x81d6: case 0x81d8: case 0x81da: case 0x81dd: case 0x81e1:
    case 0x81e3: case 0x81e6: case 0x81ea:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8116: index_constant(c, true, 0x1400); break;
    case 0x8119: case 0x811f: jsr_abs_native(c, 0xc07b); break;
    case 0x811c: index_constant(c, true, 0x1440); break;
    case 0x8122: video_load(c, 0x121b, false); break;
    case 0x8125: branch_to(c, !c->n, 0x815a); break;
    case 0x8127: case 0x8147: case 0x8180: case 0x818c: case 0x8194: case 0x81a8:
    case 0x81e6:
      jsl_to(c, 0x8324); break;
    case 0x812b: index_constant(c, false, 0x1fff); break;
    case 0x812e: byte_constant(c, 0x00); break;
    case 0x8130: store_a_long_x8(c, 0x7f0000); break;
    case 0x8134: implied(c); c->x--; zn(c, c->x, false); break;
    case 0x8135: branch_to(c, !c->n, 0x8130); break;
    case 0x8137: byte_constant(c, 0x10); break;
    case 0x8139: store_register8(c, 0x1541); break;
    case 0x813c: byte_constant(c, 0x06); break;
    case 0x813e: jsr_abs_native(c, 0xe018); break;
    case 0x8141: ldx_abs16(c, 0x2000); break;
    case 0x8144: stx_abs16(c, 0x0806); break;
    case 0x814b: stz_abs8(c, 0x1503); break;
    case 0x814e: stz_abs8(c, 0x1504); break;
    case 0x8151: stz_abs8(c, 0x1505); break;
    case 0x8154: jsr_abs_native(c, 0xf9b0); break;
    case 0x8157: stz_abs8(c, 0x1502); break;
    case 0x815a: video_load(c, 0x15e8, false); break;
    case 0x815d: branch_to(c, c->z, 0x8169); break;
    case 0x815f: load_dp8(c, 0xb7); break;
    case 0x8161: cmp_constant8(c, 0x15); break;
    case 0x8163: branch_to(c, c->c, 0x8169); break;
    case 0x8165: cmp_constant8(c, 0x12); break;
    case 0x8167: branch_to(c, !c->z, 0x816b); break;
    case 0x8169: stz_dp8(c, 0xb0); break;
    case 0x816b: jsr_abs_native(c, 0x9391); break;
    case 0x816e: byte_constant(c, 0x1d); break;
    case 0x8170: store_register8(c, 0x0d66); break;
    case 0x8173: index_constant(c, false, 0x003f); break;
    case 0x8176: stz_abs_x8_only(c, 0x0d00); break;
    case 0x8179: implied(c); c->x--; zn(c, c->x, false); break;
    case 0x817a: branch_to(c, !c->n, 0x8176); break;
    case 0x817c: jsl_to(c, 0x805c); break;
    case 0x8184: jsl_to(c, 0x82b3); break;
    case 0x8188: jsl_to(c, 0x8583); break;
    case 0x8190: jsl_to(c, 0x859b); break;
    case 0x8198: jsl_to(c, 0x83ec); break;
    case 0x819c: jsl_to(c, 0x83a9); break;
    case 0x81a0: jsl_to(c, 0x8453); break;
    case 0x81a4: jsl_to(c, 0x84e8); break;
    case 0x81ac: video_load(c, 0x14fc, false); break;
    case 0x81af: branch_to(c, !c->z, 0x81b8); break;
    case 0x81b1: jsl_to(c, 0x9a4a); break;
    case 0x81b5: inc_abs8_native(c, 0x14fe); break;
    case 0x81b8: stz_abs8(c, 0x14fc); break;
    case 0x81bb: sep_rep_native(c, 0x20, false); break;
    case 0x81bd: video_load(c, 0x02c0, false); break;
    case 0x81c0: store_a_abs16(c, 0x0200); break;
    case 0x81c3: sep_rep_native(c, 0x20, true); break;
    case 0x81c5: load_dp8(c, 0xb0); break;
    case 0x81c7: store_dp(c, 0xb3, true); break;
    case 0x81c9: stz_abs8(c, 0x212c); break;
    case 0x81cc: stz_dp8(c, 0x26); break;
    case 0x81ce: byte_constant(c, 0x04); break;
    case 0x81d0: store_dp(c, 0x25, true); break;
    case 0x81d2: jsl_to(c, 0x8004); break;
    case 0x81d6: byte_constant(c, 0x0f); break;
    case 0x81d8: store_dp(c, 0x29, true); break;
    case 0x81da: jsr_abs_native(c, 0x8321); break;
    case 0x81dd: jsl_to(c, 0x8004); break;
    case 0x81e1: byte_constant(c, 0x01); break;
    case 0x81e3: store_register8(c, 0x0715); break;
    case 0x81ea: return_long(c); break;
  }
  return true;
}

/* $1D:8906–$8967: walker callee — long ptr $1092B1,X; optional negate DP+$0C;
 * seed DP+$03/$09/$0E; optional JSL $0085B9 with $0033 swap; or STZ $1570; RTS. */
static bool timer_leaf_1d8906_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x1d || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x8908 || pc == 0x890c);
  const bool sep_entry = (pc == 0x890e);
  const bool rep_entry = (pc == 0x8906);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0x8906: case 0x8908: case 0x890c: case 0x890e: case 0x8910: case 0x8912:
    case 0x8914: case 0x8917: case 0x8919: case 0x891b: case 0x891c: case 0x891e:
    case 0x891f: case 0x8921: case 0x8924: case 0x8926: case 0x8929: case 0x892b:
    case 0x892d: case 0x892f: case 0x8930: case 0x8932: case 0x8934: case 0x8936:
    case 0x8937: case 0x8939: case 0x893b: case 0x893d: case 0x893f: case 0x8941:
    case 0x8943: case 0x8945: case 0x8947: case 0x8949: case 0x894b: case 0x894d:
    case 0x894f: case 0x8951: case 0x8954: case 0x8955: case 0x8958: case 0x895b:
    case 0x895f: case 0x8960: case 0x8963: case 0x8964: case 0x8967:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8906: sep_rep_native(c, 0x20, false); break;
    case 0x8908: lda_long_x16(c, 0x1092b1); break;
    case 0x890c: store_dp(c, 0x00, false); break;
    case 0x890e: sep_rep_native(c, 0x20, true); break;
    case 0x8910: byte_constant(c, 0x10); break;
    case 0x8912: store_dp(c, 0x02, true); break;
    case 0x8914: video_load(c, 0x01d6, false); break;
    case 0x8917: and_constant8(c, 0x60); break;
    case 0x8919: cmp_constant8(c, 0x60); break;
    case 0x891b: php_native(c); break;
    case 0x891c: byte_constant(c, 0x01); break;
    case 0x891e: plp_native(c); break;
    case 0x891f: branch_to(c, c->z, 0x893b); break;
    case 0x8921: jsr_abs_native(c, 0x8a77); break;
    case 0x8924: byte_constant(c, 0x00); break;
    case 0x8926: cpy16_constant(c, 0x1400); break;
    case 0x8929: branch_to(c, c->z, 0x893b); break;
    case 0x892b: load_dp8(c, 0x0c); break;
    case 0x892d: case 0x8934: eor_constant8(c, 0xff); break;
    case 0x892f: case 0x8936: inc_a8(c); break;
    case 0x8930: store_dp(c, 0x0c, true); break;
    case 0x8932: load_dp8(c, 0x0d); break;
    case 0x8937: store_dp(c, 0x0d, true); break;
    case 0x8939: byte_constant(c, 0x41); break;
    case 0x893b: ora_constant8(c, 0x0e); break;
    case 0x893d: store_dp(c, 0x03, true); break;
    case 0x893f: and_constant8(c, 0x01); break;
    case 0x8941: store_dp(c, 0x09, true); break;
    case 0x8943: byte_constant(c, 0xa8); break;
    case 0x8945: store_dp(c, 0x0e, true); break;
    case 0x8947: load_dp8(c, 0x0b); break;
    case 0x8949: case 0x894d: cmp_constant8(c, 0x80); break;
    case 0x894b: load_dp8(c, 0x0d); break;
    case 0x894f: branch_to(c, c->z, 0x8964); break;
    case 0x8951: load_y_abs16(c, 0x0033); break;
    case 0x8954: phy16(c); break;
    case 0x8955: load_y_abs16(c, 0x17db); break;
    case 0x8958: store_y_abs16(c, 0x0033); break;
    case 0x895b: jsl_to(c, 0x85b9); break;
    case 0x895f: ply16(c); break;
    case 0x8960: store_y_abs16(c, 0x0033); break;
    case 0x8963: case 0x8967: return_short(c); break;
    case 0x8964: stz_abs8(c, 0x1570); break;
  }
  return true;
}
/* $05:B0BE–$B0F8: seed $1420/$1422/$1432/$1424 from $1401/$B7/$14D5/$01D6; optional $B00A; RTS. */
static bool actor_seed_b0be_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0xb0be: case 0xb0c1: case 0xb0c4: case 0xb0c6: case 0xb0c8: case 0xb0ca: 
    case 0xb0cd: case 0xb0d0: case 0xb0d3: case 0xb0d6: case 0xb0d7: case 0xb0da: 
    case 0xb0dd: case 0xb0e0: case 0xb0e2: case 0xb0e5: case 0xb0e8: case 0xb0eb: 
    case 0xb0ee: case 0xb0f0: case 0xb0f2: case 0xb0f5: case 0xb0f8: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb0be: video_load(c, 0x1401, false); break;
    case 0xb0c1: case 0xb0cd: store_register8(c, 0x1420); break;
    case 0xb0c4: load_dp8(c, 0xb7); break;
    case 0xb0c6: cmp_constant8(c, 0x44); break;
    case 0xb0c8: branch_to(c, !c->z, 0xb0d0); break;
    case 0xb0ca: video_load(c, 0x14d5, false); break;
    case 0xb0d0: index_constant(c, false, 0x0002); break;
    case 0xb0d3: stx_abs16(c, 0x1422); break;
    case 0xb0d6: implied(c); set_a8(c, (uint8_t)c->x); break;
    case 0xb0d7: store_register8(c, 0x1432); break;
    case 0xb0da: index_constant(c, false, 0x0040); break;
    case 0xb0dd: video_load(c, 0x01d6, false); break;
    case 0xb0e0: branch_to(c, !c->n, 0xb0f5); break;
    case 0xb0e2: stz_abs8(c, 0x1429); break;
    case 0xb0e5: jsr_abs_native(c, 0xb00a); break;
    case 0xb0e8: index_constant(c, false, 0x0080); break;
    case 0xb0eb: video_load(c, 0x17ea, false); break;
    case 0xb0ee: cmp_constant8(c, 0x07); break;
    case 0xb0f0: branch_to(c, c->c, 0xb0f5); break;
    case 0xb0f2: index_constant(c, false, 0x0030); break;
    case 0xb0f5: stx_abs16(c, 0x1424); break;
    case 0xb0f8: return_short(c); break;
  }
  return true;
}

/* $05:B0F9–$B10F: seed $1460/$1462/$1472/$1464 from $1441; RTS. */
static bool actor_seed_b0f9_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0xb0f9: case 0xb0fc: case 0xb0ff: case 0xb102: case 0xb105: case 0xb106: 
    case 0xb109: case 0xb10c: case 0xb10f: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb0f9: video_load(c, 0x1441, false); break;
    case 0xb0fc: store_register8(c, 0x1460); break;
    case 0xb0ff: index_constant(c, false, 0x0145); break;
    case 0xb102: stx_abs16(c, 0x1462); break;
    case 0xb105: implied(c); set_a8(c, (uint8_t)c->x); break;
    case 0xb106: store_register8(c, 0x1472); break;
    case 0xb109: index_constant(c, false, 0x00c0); break;
    case 0xb10c: stx_abs16(c, 0x1464); break;
    case 0xb10f: return_short(c); break;
  }
  return true;
}

/* $05:B582–$B593: type&$7F → $0D8200,X bit7 via EOR #$80; RTS (sibling of $B572). */
static bool type_lookup_b582_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0xb582: case 0xb583: case 0xb584: case 0xb586: case 0xb587: case 0xb589: 
    case 0xb58a: case 0xb58e: case 0xb590: case 0xb591: case 0xb593: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb582: phx16(c); break;
    case 0xb583: case 0xb586: xba_native(c); break;
    case 0xb584: byte_constant(c, 0x00); break;
    case 0xb587: and_constant8(c, 0x7f); break;
    case 0xb589: tax16_from_a8(c); break;
    case 0xb58a: load_a_long_x8(c, 0x0d8200); break;
    case 0xb58e: and_constant8(c, 0x80); break;
    case 0xb590: plx16(c); break;
    case 0xb591: eor_constant8(c, 0x80); break;
    case 0xb593: return_short(c); break;
  }
  return true;
}

/* $05:9488–$9490: if $071B nonzero PLA×3 then RTL; else RTL (JSL early-exit). */
static bool early_exit_9488_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0x9488: case 0x948b: case 0x948d: case 0x948e: case 0x948f: case 0x9490: 
    break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x9488: video_load(c, 0x071b, false); break;
    case 0x948b: branch_to(c, c->z, 0x9490); break;
    case 0x948d: case 0x948e: case 0x948f: pla8(c); break;
    case 0x9490: return_long(c); break;
  }
  return true;
}

/* $1D:8A77–$8A84: Y=#$1400, or #$1440 when $14C2 bit0 set; RTS. */
static bool slot_select_1d8a77_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x1d || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0x8a77: case 0x8a7a: case 0x8a7d: case 0x8a7f: case 0x8a81: case 0x8a84: 
    break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8a77: index_constant(c, true, 0x1400); break;
    case 0x8a7a: video_load(c, 0x14c2, false); break;
    case 0x8a7d: and_constant8(c, 0x01); break;
    case 0x8a7f: branch_to(c, c->z, 0x8a84); break;
    case 0x8a81: index_constant(c, true, 0x1440); break;
    case 0x8a84: return_short(c); break;
  }
  return true;
}

/* $05:97C9–$9882: JSL $9768; mask $0B00 via $96D7 walk; scene gates → DP+$24/$0716; RTL or JMP $C41F. */
static bool actor_walk_97c9_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x97e5);
  const bool sep_entry = (pc == 0x97e8);
  const bool rep_entry = (pc == 0x97e3);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0x97c9: case 0x97cd: case 0x97d0: case 0x97d3: case 0x97d5: case 0x97d8: 
    case 0x97db: case 0x97de: case 0x97e0: case 0x97e3: case 0x97e5: case 0x97e8: 
    case 0x97ea: case 0x97ed: case 0x97ef: case 0x97f1: case 0x97f3: case 0x97f6: 
    case 0x97f8: case 0x97fa: case 0x97fc: case 0x97fe: case 0x9800: case 0x9802: 
    case 0x9805: case 0x9808: case 0x980a: case 0x980d: case 0x980f: case 0x9811: 
    case 0x9814: case 0x9816: case 0x9818: case 0x981a: case 0x981d: case 0x9820: 
    case 0x9822: case 0x9824: case 0x9826: case 0x9828: case 0x9829: case 0x982b: 
    case 0x982d: case 0x982f: case 0x9831: case 0x9833: case 0x9836: case 0x9837: 
    case 0x983a: case 0x983d: case 0x9840: case 0x9842: case 0x9844: case 0x9847: 
    case 0x9849: case 0x984b: case 0x984e: case 0x984f: case 0x9852: case 0x9856: 
    case 0x985a: case 0x985c: case 0x985f: case 0x9861: case 0x9863: case 0x9865: 
    case 0x9868: case 0x986a: case 0x986c: case 0x986e: case 0x9871: case 0x9873: 
    case 0x9875: case 0x9877: case 0x987b: case 0x987e: case 0x9880: case 0x9882: 
    break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x97c9: jsl_to(c, 0x9768); break;
    case 0x97cd: index_constant(c, false, 0x0000); break;
    case 0x97d0: load_a_abs_x8(c, 0x0b00); break;
    case 0x97d3: and_constant8(c, 0xc0); break;
    case 0x97d5: store_a_abs_x8(c, 0x0b00); break;
    case 0x97d8: jsr_abs_native(c, 0x96d7); break;
    case 0x97db: cpx16_constant(c, 0x0120); break;
    case 0x97de: branch_to(c, !c->c, 0x97d0); break;
    case 0x97e0: stz_abs8(c, 0x14f3); break;
    case 0x97e3: sep_rep_native(c, 0x20, false); break;
    case 0x97e5: stz_abs16(c, 0x14f4); break;
    case 0x97e8: sep_rep_native(c, 0x20, true); break;
    case 0x97ea: video_load(c, 0x01d6, false); break;
    case 0x97ed: and_constant8(c, 0x60); break;
    case 0x97ef: cmp_constant8(c, 0x60); break;
    case 0x97f1: case 0x97fc: branch_to(c, c->z, 0x9831); break;
    case 0x97f3: case 0x981a: video_load(c, 0x0ea3, false); break;
    case 0x97f6: cmp_constant8(c, 0x51); break;
    case 0x97f8: branch_to(c, c->z, 0x985a); break;
    case 0x97fa: cmp_constant8(c, 0x57); break;
    case 0x97fe: cmp_constant8(c, 0x54); break;
    case 0x9800: branch_to(c, !c->z, 0x980a); break;
    case 0x9802: load_y_abs16(c, 0x0e03); break;
    case 0x9805: cpy16_constant(c, 0x0015); break;
    case 0x9808: branch_to(c, !c->c, 0x9831); break;
    case 0x980a: jsr_abs_native(c, 0x90a5); break;
    case 0x980d: branch_to(c, c->c, 0x9814); break;
    case 0x980f: byte_constant(c, 0x0d); break;
    case 0x9811: case 0x9833: case 0x9837: case 0x985c: jmp_abs_native(c, 0x985f); break;
    case 0x9814: load_dp8(c, 0xab); break;
    case 0x9816: cmp_constant8(c, 0x16); break;
    case 0x9818: branch_to(c, c->z, 0x983a); break;
    case 0x981d: index_constant(c, true, 0x0011); break;
    case 0x9820: cmp_constant8(c, 0x6e); break;
    case 0x9822: case 0x9826: case 0x982b: branch_to(c, c->z, 0x9836); break;
    case 0x9824: cmp_constant8(c, 0x56); break;
    case 0x9828: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x9829: cmp_constant8(c, 0x6f); break;
    case 0x982d: byte_constant(c, 0x03); break;
    case 0x982f: store_dp(c, 0x25, true); break;
    case 0x9831: byte_constant(c, 0x00); break;
    case 0x9836: case 0x984e: implied(c); set_a8(c, (uint8_t)c->y); break;
    case 0x983a: video_load(c, 0x1460, false); break;
    case 0x983d: index_constant(c, true, 0x0039); break;
    case 0x9840: cmp_constant8(c, 0x1f); break;
    case 0x9842: case 0x9849: branch_to(c, c->z, 0x984e); break;
    case 0x9844: index_constant(c, true, 0x0054); break;
    case 0x9847: cmp_constant8(c, 0x36); break;
    case 0x984b: index_constant(c, true, 0x006e); break;
    case 0x984f: store_register8(c, 0x0ea3); break;
    case 0x9852: jsl_to(c, 0xf692); break;
    case 0x9856: jsl_to(c, 0xf719); break;
    case 0x985a: byte_constant(c, 0x02); break;
    case 0x985f: and_constant8(c, 0x7f); break;
    case 0x9861: store_dp(c, 0x24, true); break;
    case 0x9863: byte_constant(c, 0x81); break;
    case 0x9865: store_register8(c, 0x0716); break;
    case 0x9868: load_dp8(c, 0x24); break;
    case 0x986a: cmp_constant8(c, 0x02); break;
    case 0x986c: case 0x9873: branch_to(c, c->z, 0x987b); break;
    case 0x986e: case 0x987b: video_load(c, 0x14c1, false); break;
    case 0x9871: cmp_constant8(c, 0x1e); break;
    case 0x9875: byte_constant(c, 0x84); break;
    case 0x9877: jsl_to(c, 0x85a1); break;
    case 0x987e: cmp_constant8(c, 0x10); break;
    case 0x9880: branch_to(c, c->c, 0x9883); break;
    case 0x9882: return_long(c); break;
  }
  return true;
}

/* $05:CA10–$CA88: actor slot update via $CA90/$DEC3/$DA64; $1440 tail $E102/$FF33/$F9B0; RTL. */
static bool actor_ca10_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0xca10: case 0xca12: case 0xca15: case 0xca17: case 0xca1a: case 0xca1c: 
    case 0xca1e: case 0xca21: case 0xca23: case 0xca26: case 0xca29: case 0xca2c: 
    case 0xca2e: case 0xca31: case 0xca33: case 0xca36: case 0xca38: case 0xca3b: 
    case 0xca3e: case 0xca40: case 0xca42: case 0xca43: case 0xca45: case 0xca48: 
    case 0xca4a: case 0xca4b: case 0xca4d: case 0xca4f: case 0xca52: case 0xca53: 
    case 0xca55: case 0xca56: case 0xca57: case 0xca5b: case 0xca5d: case 0xca60: 
    case 0xca61: case 0xca62: case 0xca64: case 0xca66: case 0xca67: case 0xca6a: 
    case 0xca6d: case 0xca6f: case 0xca72: case 0xca75: case 0xca77: case 0xca79: 
    case 0xca7b: case 0xca7d: case 0xca7f: case 0xca81: case 0xca83: case 0xca85: 
    case 0xca88: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xca10: case 0xca36: case 0xca3e: sty_dp16(c, 0x20); break;
    case 0xca12: load_a_abs_y8(c, 0x0020); break;
    case 0xca15: branch_to(c, !c->n, 0xca1e); break;
    case 0xca17: jsr_abs_native(c, 0xd521); break;
    case 0xca1a: case 0xca2c: case 0xca43: load_dp_y16(c, 0x20); break;
    case 0xca1c: branch_to(c, true, 0xca6a); break;
    case 0xca1e: video_load(c, 0x1506, false); break;
    case 0xca21: ora_constant8(c, 0x01); break;
    case 0xca23: store_register8(c, 0x1506); break;
    case 0xca26: jsr_abs_native(c, 0xca90); break;
    case 0xca29: case 0xca38: jsr_abs_native(c, 0xdec3); break;
    case 0xca2e: case 0xca6a: cpy16_constant(c, 0x1440); break;
    case 0xca31: branch_to(c, !c->z, 0xca40); break;
    case 0xca33: index_constant(c, true, 0x1460); break;
    case 0xca3b: index_constant(c, true, 0x1440); break;
    case 0xca40: byte_constant(c, 0x00); break;
    case 0xca42: xba_native(c); break;
    case 0xca45: load_a_abs_y8(c, 0x0031); break;
    case 0xca48: branch_to(c, c->z, 0xca6a); break;
    case 0xca4a: inc_a8(c); break;
    case 0xca4b: branch_to(c, !c->z, 0xca4f); break;
    case 0xca4d: ora_constant8(c, 0x80); break;
    case 0xca4f: store_a_abs_y8(c, 0x0031); break;
    case 0xca52: case 0xca61: lsr_a8(c); break;
    case 0xca53: and_constant8(c, 0x03); break;
    case 0xca55: pha8(c); break;
    case 0xca56: tax16_from_a8(c); break;
    case 0xca57: load_a_long_x8(c, 0x05ca89); break;
    case 0xca5b: store_dp(c, 0x04, true); break;
    case 0xca5d: jsr_abs_native(c, 0xda64); break;
    case 0xca60: pla8(c); break;
    case 0xca62: byte_constant(c, 0x84); break;
    case 0xca64: branch_to(c, !c->c, 0xca67); break;
    case 0xca66: dec_a8(c); break;
    case 0xca67: store_a_abs_y8(c, 0x0021); break;
    case 0xca6d: branch_to(c, !c->z, 0xca88); break;
    case 0xca6f: jsr_abs_native(c, 0xe102); break;
    case 0xca72: jsr_abs_native(c, 0xff33); break;
    case 0xca75: load_dp8(c, 0xb8); break;
    case 0xca77: and_constant8(c, 0xc0); break;
    case 0xca79: cmp_constant8(c, 0xc0); break;
    case 0xca7b: branch_to(c, !c->z, 0xca85); break;
    case 0xca7d: load_dp8(c, 0x51); break;
    case 0xca7f: and_constant8(c, 0xf9); break;
    case 0xca81: ora_constant8(c, 0x02); break;
    case 0xca83: store_dp(c, 0x51, true); break;
    case 0xca85: jsr_abs_native(c, 0xf9b0); break;
    case 0xca88: return_long(c); break;
  }
  return true;
}

/* $05:9115–$9200: DP+$AC phase machine; JSL $9488/$CA10/$DFE3; multiple RTL / JMP $97C9. */
static bool actor_cont_9115_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0x9115: case 0x9117: case 0x9119: case 0x911c: case 0x911f: case 0x9121: 
    case 0x9124: case 0x9126: case 0x9129: case 0x912b: case 0x912f: case 0x9132: 
    case 0x9135: case 0x9138: case 0x913b: case 0x913e: case 0x9142: case 0x9145: 
    case 0x9149: case 0x914d: case 0x9150: case 0x9153: case 0x9155: case 0x9159: 
    case 0x915b: case 0x915c: case 0x915e: case 0x9160: case 0x9163: case 0x9166: 
    case 0x9168: case 0x916b: case 0x916e: case 0x9172: case 0x9175: case 0x9177: 
    case 0x9179: case 0x917c: case 0x917e: case 0x9181: case 0x9183: case 0x9186: 
    case 0x9188: case 0x918a: case 0x918e: case 0x9190: case 0x9193: case 0x9197: 
    case 0x919a: case 0x919d: case 0x919f: case 0x91a2: case 0x91a4: case 0x91a7: 
    case 0x91a9: case 0x91aa: case 0x91ad: case 0x91af: case 0x91b1: case 0x91b3: 
    case 0x91b6: case 0x91b9: case 0x91bb: case 0x91bc: case 0x91be: case 0x91c0: 
    case 0x91c2: case 0x91c5: case 0x91c8: case 0x91cc: case 0x91cf: case 0x91d3: 
    case 0x91d7: case 0x91d9: case 0x91db: case 0x91dd: case 0x91df: case 0x91e2: 
    case 0x91e4: case 0x91e7: case 0x91ea: case 0x91ed: case 0x91ee: case 0x91f1: 
    case 0x91f3: case 0x91f6: case 0x91f8: case 0x91fb: case 0x91fe: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x9115: load_dp8(c, 0xac); break;
    case 0x9117: branch_to(c, !c->z, 0x915c); break;
    case 0x9119: load_y_abs16(c, 0x14e4); break;
    case 0x911c: cpy16_constant(c, 0x013a); break;
    case 0x911f: case 0x9124: branch_to(c, c->z, 0x912b); break;
    case 0x9121: cpy16_constant(c, 0x00f8); break;
    case 0x9126: cpy16_constant(c, 0x0101); break;
    case 0x9129: branch_to(c, !c->z, 0x9135); break;
    case 0x912b: case 0x916e: jsl_to(c, 0x9488); break;
    case 0x912f: index_constant(c, true, 0x003d); break;
    case 0x9132: jmp_abs_native(c, 0x93a9); break;
    case 0x9135: index_constant(c, true, 0xffff); break;
    case 0x9138: case 0x916b: store_y_abs16(c, 0x0726); break;
    case 0x913b: case 0x9197: case 0x91c5: index_constant(c, true, 0x1400); break;
    case 0x913e: case 0x9145: case 0x91c8: case 0x91cf: jsl_to(c, 0xca10); break;
    case 0x9142: case 0x91cc: index_constant(c, true, 0x1440); break;
    case 0x9149: case 0x91d3: jsl_to(c, 0xdfe3); break;
    case 0x914d: jsr_abs_native(c, 0xa435); break;
    case 0x9150: case 0x9163: cpy16_constant(c, 0xffff); break;
    case 0x9153: branch_to(c, c->z, 0x9159); break;
    case 0x9155: jsl_to(c, 0x93a9); break;
    case 0x9159: case 0x91a7: case 0x91b9: inc_dp8(c, 0xac); break;
    case 0x915b: case 0x91a9: case 0x91bb: case 0x91ed: return_long(c); break;
    case 0x915c: cmp_constant8(c, 0x01); break;
    case 0x915e: branch_to(c, !c->z, 0x91ad); break;
    case 0x9160: load_y_abs16(c, 0x0726); break;
    case 0x9166: branch_to(c, !c->z, 0x916e); break;
    case 0x9168: index_constant(c, true, 0x003c); break;
    case 0x9172: video_load(c, 0x0ea3, false); break;
    case 0x9175: cmp_constant8(c, 0x55); break;
    case 0x9177: case 0x9181: branch_to(c, !c->z, 0x91aa); break;
    case 0x9179: stz_abs8(c, 0x0c40); break;
    case 0x917c: case 0x918e: byte_constant(c, 0x61); break;
    case 0x917e: jsr_abs_native(c, 0xae95); break;
    case 0x9183: load_a_abs_y8(c, 0x0b02); break;
    case 0x9186: cmp_constant8(c, 0x03); break;
    case 0x9188: branch_to(c, !c->c, 0x91aa); break;
    case 0x918a: jsl_to(c, 0xae81); break;
    case 0x9190: store_register8(c, 0x1420); break;
    case 0x9193: jsl_to(c, 0x8583); break;
    case 0x919a: jsr_abs_native(c, 0xc07b); break;
    case 0x919d: byte_constant(c, 0x01); break;
    case 0x919f: store_register8(c, 0x14c2); break;
    case 0x91a2: case 0x91dd: case 0x91f1: byte_constant(c, 0x80); break;
    case 0x91a4: case 0x91df: store_register8(c, 0x14fc); break;
    case 0x91aa: case 0x91fe: jmp_abs_native(c, 0x97c9); break;
    case 0x91ad: cmp_constant8(c, 0x02); break;
    case 0x91af: branch_to(c, !c->z, 0x91bc); break;
    case 0x91b1: byte_constant(c, 0x1c); break;
    case 0x91b3: case 0x91e4: jsr_abs_native(c, 0xc98f); break;
    case 0x91b6: case 0x91c2: case 0x91e7: jsr_abs_native(c, 0x9886); break;
    case 0x91bc: case 0x91d7: load_dp8(c, 0xb7); break;
    case 0x91be: case 0x91d9: cmp_constant8(c, 0x1c); break;
    case 0x91c0: branch_to(c, c->z, 0x91c5); break;
    case 0x91db: branch_to(c, !c->z, 0x91ee); break;
    case 0x91e2: byte_constant(c, 0x1d); break;
    case 0x91ea: jsr_abs_native(c, 0xca07); break;
    case 0x91ee: load_y_abs16(c, 0x14c3); break;
    case 0x91f3: store_a_abs_y8(c, 0x0b00); break;
    case 0x91f6: byte_constant(c, 0x00); break;
    case 0x91f8: store_a_abs_y8(c, 0x0b03); break;
    case 0x91fb: store_a_abs_y8(c, 0x0b04); break;
  }
  return true;
}


/* $00:91B6-$91BC: scene-mode thunk. The same offset is a different bank-05
 * actor continuation above, so the dispatcher keeps this implementation bank-specific. */
static bool scene_thunk_91b6_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->xf || !c->mf || c->d) return false;
  switch(pc) {
    case 0x91b6: case 0x91b8: case 0x91ba: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x91b6: byte_constant(c, 2); break;
    case 0x91b8: store_dp(c, 0x61, true); break;
    case 0x91ba: jmp_abs_native(c, 0xfcd3); break;
  }
  return true;
}

/* $00:943F-$9443: scene table leaf that delegates the PPU preset to bank 04. */
static bool scene_thunk_943f_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->xf || c->d || !c->mf) return false;
  if(pc != 0x943f && pc != 0x9443) return false;
  (void)fetch(c);
  if(pc == 0x943f) jsl_to(c,0xd85b); else return_long(c);
  return true;
}

/* Scene-table leaves that only delegate to another bank's handler. */
static bool scene_thunk_952b_9530_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x952b: case 0x952f: case 0x9530: case 0x9534: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x952b: jsl_to(c,0xf462); break;
    case 0x952f: return_long(c); break;
    case 0x9530: jsl_to(c,0x8674); break;
    case 0x9534: return_long(c); break;
  }
  return true;
}

/* $00:95A9-$95B6: scene-mode[15] setup around the native $8E96 path. */
static bool scene_thunk_95a9_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x95a9: case 0x95ab: case 0x95ae: case 0x95b2: case 0x95b4: case 0x95b6: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x95a9: byte_constant(c,0x80); break;
    case 0x95ab: store_register8(c,0x01d6); break;
    case 0x95ae: jsl_to(c,0x8e96); break;
    case 0x95b2: byte_constant(c,0x0d); break;
    case 0x95b4: store_dp(c,0x27,true); break;
    case 0x95b6: return_long(c); break;
  }
  return true;
}

/* $00:9444-$9458: scene-table PPU preset wrapper. */
static bool scene_thunk_9444_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x9444: case 0x9446: case 0x9449: case 0x944b: case 0x944e:
    case 0x9450: case 0x9454: case 0x9456: case 0x9458: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x9444: byte_constant(c,0x0c); break;
    case 0x9446: store_register8(c,0x121b); break;
    case 0x9449: byte_constant(c,0x80); break;
    case 0x944b: store_register8(c,0x121c); break;
    case 0x944e: byte_constant(c,0); break;
    case 0x9450: jsl_to(c,0x8eba); break;
    case 0x9454: byte_constant(c,0x09); break;
    case 0x9456: store_dp(c,0x25,true); break;
    case 0x9458: return_long(c); break;
  }
  return true;
}

/* $00:9459-$946E: scene-table reset wrapper with two cross-bank calls. */
static bool scene_thunk_9459_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x9459: case 0x945c: case 0x945f: case 0x9462: case 0x9466:
    case 0x946a: case 0x946c: case 0x946e: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x9459: stz_abs8(c,0x420c); break;
    case 0x945c: stz_abs8(c,0x10c0); break;
    case 0x945f: stz_abs8(c,0x01bc); break;
    case 0x9462: jsl_to(c,0xef1a); break;
    case 0x9466: jsl_to(c,0xefe1); break;
    case 0x946a: byte_constant(c,0x0a); break;
    case 0x946c: store_dp(c,0x25,true); break;
    case 0x946e: return_long(c); break;
  }
  return true;
}

/* $00:99C3-$99CB: scene transition trampoline. */
static bool scene_trampoline_99c3_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) { case 0x99c3: case 0x99c7: case 0x99cb: break; default: return false; }
  (void)fetch(c);
  switch(pc) {
    case 0x99c3: jsl_to(c,0x8000); break;
    case 0x99c7: jsl_to(c,0x982c); break;
    case 0x99cb: return_long(c); break;
  }
  return true;
}

/* $00:99AD-$99C1: scene post-processing continuation. */
static bool scene_post_99ad_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) { case 0x99ad: case 0x99b1: case 0x99b4: case 0x99b6: case 0x99b9: case 0x99bb: case 0x99bd: case 0x99c1: break; default: return false; }
  (void)fetch(c);
  switch(pc) {
    case 0x99ad: jsl_to(c,0x98d5); break;
    case 0x99b1: video_load(c,0x0ea1,false); break;
    case 0x99b4: branch_to(c,c->n,0x99bd); break;
    case 0x99b6: video_load(c,0x0d66,false); break;
    case 0x99b9: cmp_constant8(c,0x20); break;
    case 0x99bb: branch_to(c,c->z,0x99c1); break;
    case 0x99bd: jsl_to(c,0xf91f); break;
    case 0x99c1: return_long(c); break;
  }
  return true;
}

/* $00:991D-$9926: scene continuation branch. */
static bool scene_branch_991d_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) { case 0x991d: case 0x9920: case 0x9922: case 0x9926: break; default: return false; }
  (void)fetch(c);
  switch(pc) {
    case 0x991d: video_load(c,0x0ea1,false); break;
    case 0x9920: branch_to(c,!c->n,0x9926); break;
    case 0x9922: jsl_to(c,0xf91f); break;
    case 0x9926: jsl_to(c,0xdd83); break;
  }
  return true;
}

/* $00:987A-$9883: scene flag branch. */
static bool scene_branch_987a_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) { case 0x987a: case 0x987d: case 0x987f: case 0x9883: break; default: return false; }
  (void)fetch(c);
  switch(pc) {
    case 0x987a: video_load(c,0x07af,false); break;
    case 0x987d: branch_to(c,!c->n,0x9885); break;
    case 0x987f: jsl_to(c,0xa3a8); break;
    case 0x9883: branch_to(c,true,0x9889); break;
  }
  return true;
}

/* $01:8481: actor long-jump stub. */
static bool actor_jump_8481_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 1 || c->e || c->xf || c->d || !c->mf || pc != 0x8481) return false;
  (void)fetch(c); jml_to(c,0x8627,0x01); return true;
}

/* $05:9390: actor leaf return (M/X variants). */
static bool actor_return_9390_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 5 || c->e || c->d || !c->mf || pc != 0x9390) return false;
  (void)fetch(c); return_long(c); return true;
}

/* $04:8C4F: audio leaf return. */
static bool audio_return_8c4f_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 4 || c->e || c->xf || c->d || !c->mf || pc != 0x8c4f) return false;
  (void)fetch(c); return_short(c); return true;
}

/* $01:EDF4: actor call stub. */
static bool actor_call_edf4_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 1 || c->e || c->xf || c->d || !c->mf || pc != 0xedf4) return false;
  (void)fetch(c); jsl_to(c,0xeb73); return true;
}

/* $01:EC4C: actor call stub. */
static bool actor_call_ec4c_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 1 || c->e || c->xf || c->d || !c->mf || pc != 0xec4c) return false;
  (void)fetch(c); jsl_to(c,0xeb73); return true;
}

/* $01:CD27/$CD4B/$CD8A: actor call stubs. */
static bool actor_call_cdb2_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 1 || c->e || c->xf || c->d || !c->mf) return false;
  if(pc != 0xcd27 && pc != 0xcd4b && pc != 0xcd8a) return false;
  (void)fetch(c); jsl_to(c,0xcfb2); return true;
}

/* $01:CD1E: actor call stub. */
static bool actor_call_cd1e_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 1 || c->e || c->xf || c->d || !c->mf || pc != 0xcd1e) return false;
  (void)fetch(c); jsl_to(c,0xcfb2); return true;
}

/* Bank-1 short prefixes that all load the shared status word at $073C before
 * their local continuation. The continuation remains interpreter-backed. */
static bool actor_load_073c_prefix_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 1 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xdc60: case 0xde10: case 0xde46: case 0xde7c: case 0xdfcc:
    case 0xe066: case 0xe0b7: case 0xe0f9: case 0xe124: case 0xe14f:
    case 0xe1d3: case 0xeb48: case 0xec9d: case 0xed2e: break;
    default: return false;
  }
  (void)fetch(c); video_load(c, 0x073c, false); return true;
}

/* $01:D433: load the shared map/status word before the local continuation. */
static bool actor_load_072e_prefix_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 1 || c->e || c->xf || c->d || !c->mf || pc != 0xd433) return false;
  (void)fetch(c); video_load(c, 0x072e, false); return true;
}

/* $01:A3DB–A3E2: initialize the actor upload flag and clear its first slot. */
static bool actor_init_a3db_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 1 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) { case 0xa3db: case 0xa3dd: case 0xa3df: case 0xa3e2: break; default: return false; }
  (void)fetch(c);
  switch(pc) { case 0xa3db: byte_constant(c, 1); break; case 0xa3dd: store_dp(c, 0x51, true); break; case 0xa3df: stz_abs8(c, 0x0800); break; case 0xa3e2: return_long(c); break; }
  return true;
}

/* $01:D00B–D012: advance the actor timer pair and return. */
static bool actor_timer_d00b_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 1 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) { case 0xd00b: case 0xd00c: case 0xd00f: case 0xd012: break; default: return false; }
  (void)fetch(c);
  switch(pc) { case 0xd00b: implied(c); c->x++; zn(c, c->x, false); break; case 0xd00c: inc_abs8_native(c, 0x071d); break; case 0xd00f: inc_abs8_native(c, 0x071f); break; case 0xd012: return_long(c); break; }
  return true;
}

/* $01:CEBA–CEC5: reset the actor status helpers and return. */
static bool actor_reset_ceba_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 1 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) { case 0xceba: case 0xcebe: case 0xcec2: case 0xcec5: break; default: return false; }
  (void)fetch(c);
  switch(pc) { case 0xceba: jsl_to(c,0xcec6); break; case 0xcebe: jsl_to(c,0xb67a); break; case 0xcec2: stz_abs8(c,0x0725); break; case 0xcec5: return_long(c); break; }
  return true;
}

/* $01:A4EB–A4F2: enter the shared actor table helper and load its selector. */
static bool actor_table_a4eb_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 1 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) { case 0xa4eb: case 0xa4ef: case 0xa4f2: break; default: return false; }
  (void)fetch(c);
  switch(pc) { case 0xa4eb: jsl_to(c,0xb72b); break; case 0xa4ef: video_load(c,0x0d66,false); break; case 0xa4f2: jsr_abs_native(c,0xbf44); break; }
  return true;
}

/* $00:AE29–AE37: prepare the tile decompression pointers and return. */
static bool mode7_prepare_ae29_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) { case 0xae29: case 0xae2b: case 0xae2d: case 0xae31: case 0xae35: case 0xae37: break; default: return false; }
  (void)fetch(c);
  switch(pc) { case 0xae29: stx_dp16(c,0x83); break; case 0xae2b: store_dp(c,0x85,true); break; case 0xae2d: jsl_to(c,0xc559); break; case 0xae31: jsl_to(c,0xc68c); break; case 0xae35: ldy_dp16(c,0x89); break; case 0xae37: return_short(c); break; }
  return true;
}

/* $01:8092–80A1: resolve the actor index through the bank-1 table. */
static bool actor_index_8092_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 1 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) { case 0x8092: case 0x8094: case 0x8095: case 0x8098: case 0x8099: case 0x809a: case 0x809e: case 0x80a1: break; default: return false; }
  (void)fetch(c);
  switch(pc) { case 0x8092: byte_constant(c,0); break; case 0x8094: xba_native(c); break; case 0x8095: video_load(c,0x0d59,false); break; case 0x8098: asl_a8(c); break; case 0x8099: tax16(c); break; case 0x809a: load_a_long_x8(c,0x0183a8); break; case 0x809e: store_register8(c,0x01e3); break; case 0x80a1: return_long(c); break; }
  return true;
}

/* $01:8241–824F: resolve the compact actor table entry into $0D5E. */
static bool actor_index_8241_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 1 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) { case 0x8241: case 0x8243: case 0x8244: case 0x8247: case 0x8248: case 0x824c: case 0x824f: break; default: return false; }
  (void)fetch(c);
  switch(pc) { case 0x8241: byte_constant(c,0); break; case 0x8243: xba_native(c); break; case 0x8244: video_load(c,0x0d59,false); break; case 0x8247: tax16(c); break; case 0x8248: load_a_long_x8(c,0x0183c9); break; case 0x824c: store_register8(c,0x0d5e); break; case 0x824f: return_long(c); break; }
  return true;
}

/* $01:BBF7–BC03: resolve the actor table pointer into Y and return. */
static bool actor_index_bbf7_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 1 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) { case 0xbbf7: case 0xbbf9: case 0xbbfa: case 0xbbfd: case 0xbbfe: case 0xbc02: case 0xbc03: break; default: return false; }
  (void)fetch(c);
  switch(pc) { case 0xbbf7: byte_constant(c,0); break; case 0xbbf9: xba_native(c); break; case 0xbbfa: video_load(c,0x0d62,false); break; case 0xbbfd: tax16(c); break; case 0xbbfe: load_a_long_x8(c,0x01bc04); break; case 0xbc02: tay16(c); break; case 0xbc03: return_long(c); break; }
  return true;
}

/* $03:958A–9599: copy actor coordinates and select the compact table bank. */
static bool scene_actor_958a_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 3 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) { case 0x958a: case 0x958d: case 0x958f: case 0x9592: case 0x9594: case 0x9597: case 0x9599: break; default: return false; }
  (void)fetch(c);
  switch(pc) { case 0x958a: load_y_abs16(c,0x01a2); break; case 0x958d: sty_dp16(c,0x00); break; case 0x958f: load_y_abs16(c,0x01a4); break; case 0x9592: sty_dp16(c,0x02); break; case 0x9594: video_load(c,0x0d59,false); break; case 0x9597: and_constant8(c,0x03); break; case 0x9599: jsr_abs_native(c,0x8000); break; }
  return true;
}

/* $01:BA10–BA29: update the two actor flag classes through $0485B6. */
static bool actor_flag_ba10_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 1 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) { case 0xba10: case 0xba12: case 0xba14: case 0xba16: case 0xba18: case 0xba1c:
    case 0xba1d: case 0xba1f: case 0xba21: case 0xba23: case 0xba25: case 0xba29: break; default: return false; }
  (void)fetch(c);
  switch(pc) {
    case 0xba10: case 0xba1d: load_dp8(c,0x48); break;
    case 0xba12: and_constant8(c,0x0c); break;
    case 0xba14: branch_to(c,c->z,0xba1c); break;
    case 0xba16: case 0xba23: byte_constant(c,1); break;
    case 0xba18: case 0xba25: jsl_to(c,0x85b6); break;
    case 0xba1f: and_constant8(c,0x03); break;
    case 0xba21: branch_to(c,c->z,0xba29); break;
    case 0xba1c: case 0xba29: return_long(c); break;
  }
  return true;
}

/* $01:CE57–CE6C: reset the actor status word and set its active bit. */
static bool actor_status_ce57_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 1 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) { case 0xce57: case 0xce5a: case 0xce5e: case 0xce62: case 0xce65: case 0xce67: case 0xce69: case 0xce6c: break; default: return false; }
  (void)fetch(c);
  switch(pc) { case 0xce57: stz_abs8(c,0x0725); break; case 0xce5a: jsl_to(c,0xd33a); break; case 0xce5e: jsl_to(c,0xb67a); break; case 0xce62: video_load(c,0x071b,false); break; case 0xce65: and_constant8(c,0xf0); break; case 0xce67: ora_constant8(c,1); break; case 0xce69: store_register8(c,0x071b); break; case 0xce6c: return_long(c); break; }
  return true;
}

/* $01:CCEF–CD01: advance the actor status counter and dispatch its update. */
static bool actor_status_ccef_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 1 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) { case 0xccef: case 0xccf3: case 0xccf6: case 0xccf9: case 0xccfb: case 0xccfd: case 0xcd01: break; default: return false; }
  (void)fetch(c);
  switch(pc) { case 0xccef: jsl_to(c,0xcd0d); break; case 0xccf3: inc_abs8_native(c,0x0725); break; case 0xccf6: video_load(c,0x0725,false); break; case 0xccf9: cmp_dp8(c,0x04); break; case 0xccfb: branch_to(c,!c->c,0xcd08); break; case 0xccfd: jsl_to(c,0xcfb2); break; case 0xcd01: jsl_to(c,0xb67a); break; }
  return true;
}

/* $01:EDF9: actor leaf return. */
static bool actor_return_edf9_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 1 || c->e || c->xf || c->d || !c->mf || pc != 0xedf9) return false;
  (void)fetch(c); return_long(c); return true;
}


/* $00:FCD8: scene call stub. */
static bool scene_call_fcd8_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->xf || c->d || !c->mf || pc != 0xfcd8) return false;
  (void)fetch(c); jsl_to(c,0x8206); return true;
}

/* $00:9AFA: scene call stub. */
static bool scene_call_9afa_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->xf || c->d || !c->mf || pc != 0x9afa) return false;
  (void)fetch(c); jsl_to(c,0x991d); return true;
}

/* $00:9AF5: scene call stub. */
static bool scene_call_9af5_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->xf || c->d || !c->mf || pc != 0x9af5) return false;
  (void)fetch(c); jsl_to(c,0x869e); return true;
}

/* $00:9AC8: scene call stub. */
static bool scene_call_9ac8_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->xf || c->d || !c->mf || pc != 0x9ac8) return false;
  (void)fetch(c); jsl_to(c,0xd8dd); return true;
}

/* $00:9735: scene leaf return. */
static bool scene_return_9735_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->xf || c->d || !c->mf || pc != 0x9735) return false;
  (void)fetch(c); return_long(c); return true;
}

/* $00:99C2: scene leaf return. */
static bool scene_return_99c2_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->xf || c->d || !c->mf || pc != 0x99c2) return false;
  (void)fetch(c); return_long(c); return true;
}

/* $00:97E9-$97FC: scene-state seed leaf. */
static bool scene_seed_97e9_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 0 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x97e9: case 0x97eb: case 0x97ee: case 0x97f0: case 0x97f2:
    case 0x97f4: case 0x97f6: case 0x97f8: case 0x97fa: case 0x97fc: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x97e9: byte_constant(c,0xa1); break;
    case 0x97eb: store_register8(c,0x0716); break;
    case 0x97ee: byte_constant(c,0x02); break;
    case 0x97f0: store_dp(c,0x24,true); break;
    case 0x97f2: store_dp(c,0x27,true); break;
    case 0x97f4: byte_constant(c,0x03); break;
    case 0x97f6: store_dp(c,0x25,true); break;
    case 0x97f8: byte_constant(c,0x04); break;
    case 0x97fa: store_dp(c,0x26,true); break;
    case 0x97fc: return_long(c); break;
  }
  return true;
}

/* $05:B00A–$B044: BMI-path callee of $B0BE — seed $17EA..$17F3 from $0E03/$0E0A; JSL $1C806C/$1C8136; RTS. */
static bool actor_seed_b00a_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0xb00a: case 0xb00c: case 0xb00f: case 0xb012: case 0xb015: case 0xb018:
    case 0xb01b: case 0xb01f: case 0xb022: case 0xb025: case 0xb028: case 0xb02b:
    case 0xb02e: case 0xb031: case 0xb034: case 0xb037: case 0xb03a: case 0xb03d:
    case 0xb040: case 0xb044: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb00a: byte_constant(c, 0x0a); break;
    case 0xb00c: store_register8(c, 0x17ea); break;
    case 0xb00f: index_constant(c, true, 0x0000); break;
    case 0xb012: store_y_abs16(c, 0x17eb); break;
    case 0xb015: load_y_abs16(c, 0x0e03); break;
    case 0xb018: store_y_abs16(c, 0x17ed); break;
    case 0xb01b: jsl_to(c, 0x806c); break;
    case 0xb01f: load_y_abs16(c, 0x17ef); break;
    case 0xb022: store_y_abs16(c, 0x17ea); break;
    case 0xb025: load_y_abs16(c, 0x17f1); break;
    case 0xb028: store_y_abs16(c, 0x17ec); break;
    case 0xb02b: video_load(c, 0x17f3, false); break;
    case 0xb02e: store_register8(c, 0x17ee); break;
    case 0xb031: load_y_abs16(c, 0x0e0a); break;
    case 0xb034: store_y_abs16(c, 0x17ef); break;
    case 0xb037: stz_abs8(c, 0x17f1); break;
    case 0xb03a: stz_abs8(c, 0x17f2); break;
    case 0xb03d: stz_abs8(c, 0x17f3); break;
    case 0xb040: jsl_to(c, 0x8136); break;
    case 0xb044: return_short(c); break;
  }
  return true;
}

/* $05:9883: JMP $C41F (carry-taken fallthrough from $97C9).
 * $05:9886–$98C7: seed $17DD/$17E1/$14FF/$17E5 and DP+$B0 from $01E7/$01D6/$17D2; RTS. */
static bool scene_seed_9886_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0x9883: case 0x9886: case 0x9889: case 0x988c: case 0x988f: case 0x9892:
    case 0x9895: case 0x9897: case 0x9899: case 0x989c: case 0x989f: case 0x98a1:
    case 0x98a4: case 0x98a7: case 0x98a9: case 0x98ac: case 0x98af: case 0x98b2:
    case 0x98b4: case 0x98b6: case 0x98b9: case 0x98bc: case 0x98be: case 0x98c1:
    case 0x98c4: case 0x98c5: case 0x98c7: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x9883: jmp_abs_native(c, 0xc41f); break;
    case 0x9886: index_constant(c, false, 0x0000); break;
    case 0x9889: stx_abs16(c, 0x17dd); break;
    case 0x988c: stx_abs16(c, 0x17e1); break;
    case 0x988f: stx_abs16(c, 0x14ff); break;
    case 0x9892: video_load(c, 0x01e7, false); break;
    case 0x9895: and_constant8(c, 0x02); break;
    case 0x9897: branch_to(c, !c->z, 0x98a1); break;
    case 0x9899: index_constant(c, true, 0x0014); break;
    case 0x989c: video_load(c, 0x01d6, false); break;
    case 0x989f: branch_to(c, c->n, 0x98c1); break;
    case 0x98a1: index_constant(c, true, 0x001c); break;
    case 0x98a4: video_load(c, 0x01e7, false); break;
    case 0x98a7: branch_to(c, c->n, 0x98c1); break;
    case 0x98a9: index_constant(c, false, 0x00ac); break;
    case 0x98ac: index_constant(c, true, 0x0018); break;
    case 0x98af: video_load(c, 0x17d2, false); break;
    case 0x98b2: and_constant8(c, 0x01); break;
    case 0x98b4: branch_to(c, !c->z, 0x98c1); break;
    case 0x98b6: index_constant(c, true, 0x001c); break;
    case 0x98b9: video_load(c, 0x01e7, false); break;
    case 0x98bc: branch_to(c, c->n, 0x98c1); break;
    case 0x98be: index_constant(c, false, 0x0158); break;
    case 0x98c1: stx_abs16(c, 0x17e5); break;
    case 0x98c4: implied(c); set_a8(c, (uint8_t)c->y); break;
    case 0x98c5: store_dp(c, 0xb0, true); break;
    case 0x98c7: return_short(c); break;
  }
  return true;
}

/* $05:C41F–$C44F: post-$9883 scene index from $14C1 via $05C450; optional INC $1220 / JSL $0485A1; RTL. */
static bool scene_index_c41f_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0xc41f: case 0xc421: case 0xc422: case 0xc425: case 0xc426: case 0xc428:
    case 0xc429: case 0xc42d: case 0xc42f: case 0xc431: case 0xc433: case 0xc436:
    case 0xc439: case 0xc43c: case 0xc43e: case 0xc441: case 0xc444: case 0xc446:
    case 0xc448: case 0xc44c: case 0xc44f: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xc41f: byte_constant(c, 0x00); break;
    case 0xc421: xba_native(c); break;
    case 0xc422: video_load(c, 0x14c1, false); break;
    case 0xc425: implied(c); c->c = true; break;
    case 0xc426: sbc8_constant(c, 0x10); break;
    case 0xc428: tax16_from_a8(c); break;
    case 0xc429: load_a_long_x8(c, 0x05c450); break;
    case 0xc42d: store_dp(c, 0x24, true); break;
    case 0xc42f: cmp_constant8(c, 0x07); break;
    case 0xc431: branch_to(c, !c->z, 0xc436); break;
    case 0xc433: inc_abs8_native(c, 0x1220); break;
    case 0xc436: stz_abs8(c, 0x14e8); break;
    case 0xc439: cpx16_constant(c, 0x000e); break;
    case 0xc43c: branch_to(c, !c->z, 0xc446); break;
    case 0xc43e: video_load(c, 0x14e9, false); break;
    case 0xc441: store_register8(c, 0x14e8); break;
    case 0xc444: branch_to(c, true, 0xc44c); break;
    case 0xc446: byte_constant(c, 0x84); break;
    case 0xc448: jsl_to(c, 0x85a1); break;
    case 0xc44c: stz_abs8(c, 0x14c1); break;
    case 0xc44f: return_long(c); break;
  }
  return true;
}

/* $05:90A5–$90B2: carry predicate — SEC; if $15FA==#$80 RTS else LSR $01E7 twice into C; RTS. */
static bool carry_pred_90a5_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0x90a5: case 0x90a8: case 0x90aa: case 0x90ab: case 0x90ad: case 0x90b0:
    case 0x90b1: case 0x90b2: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x90a5: video_load(c, 0x15fa, false); break;
    case 0x90a8: cmp_constant8(c, 0x80); break;
    case 0x90aa: implied(c); c->c = true; break;
    case 0x90ab: branch_to(c, c->z, 0x90b2); break;
    case 0x90ad: video_load(c, 0x01e7, false); break;
    case 0x90b0: case 0x90b1: lsr_a8(c); break;
    case 0x90b2: return_short(c); break;
  }
  return true;
}

/* $05:DA64–$DA7B: face-byte merge — slot bit from Y==$1400, AND #$3F, ORA DP+$05, EOR DP+$04 → $0022,Y; RTS. */
static bool face_merge_da64_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0xda64: case 0xda66: case 0xda69: case 0xda6b: case 0xda6d: case 0xda6f:
    case 0xda72: case 0xda74: case 0xda76: case 0xda78: case 0xda7b: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xda64: byte_constant(c, 0x00); break;
    case 0xda66: cpy16_constant(c, 0x1400); break;
    case 0xda69: branch_to(c, c->z, 0xda6d); break;
    case 0xda6b: byte_constant(c, 0x40); break;
    case 0xda6d: store_dp(c, 0x05, true); break;
    case 0xda6f: load_a_abs_y8(c, 0x0022); break;
    case 0xda72: and_constant8(c, 0x3f); break;
    case 0xda74: ora_dp8(c, 0x05); break;
    case 0xda76: eor_dp8(c, 0x04); break;
    case 0xda78: store_a_abs_y8(c, 0x0022); break;
    case 0xda7b: return_short(c); break;
  }
  return true;
}

/* $05:C9FA–$CA06: clear $1502..$1505; RTS (callee of $C98F / $DFE3). */
static bool clear_1502_c9fa_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0xc9fa: case 0xc9fd: case 0xca00: case 0xca03: case 0xca06: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xc9fa: stz_abs8(c, 0x1502); break;
    case 0xc9fd: stz_abs8(c, 0x1503); break;
    case 0xca00: stz_abs8(c, 0x1504); break;
    case 0xca03: stz_abs8(c, 0x1505); break;
    case 0xca06: return_short(c); break;
  }
  return true;
}

/* $05:DFE3–$DFF2: if DP+$B8&$C0 != $C0 then PLA×3 RTL; else JSR $C9FA; RTL. */
static bool gate_dfe3_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0xdfe3: case 0xdfe5: case 0xdfe7: case 0xdfe9: case 0xdfeb: case 0xdfec:
    case 0xdfed: case 0xdfee: case 0xdfef: case 0xdff2: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xdfe3: load_dp8(c, 0xb8); break;
    case 0xdfe5: and_constant8(c, 0xc0); break;
    case 0xdfe7: cmp_constant8(c, 0xc0); break;
    case 0xdfe9: branch_to(c, c->z, 0xdfef); break;
    case 0xdfeb: case 0xdfec: case 0xdfed: pla8(c); break;
    case 0xdfee: case 0xdff2: return_long(c); break;
    case 0xdfef: jsr_abs_native(c, 0xc9fa); break;
  }
  return true;
}

/* $05:C98F–$C9F9: store A→DP+$B7; clear battle/DP fields; optional JSL $05870B; JSR $C9FA; seed $14C8; RTS. */
static bool actor_init_c98f_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0xc98f: case 0xc991: case 0xc993: case 0xc995: case 0xc997: case 0xc999:
    case 0xc99b: case 0xc99d: case 0xc99f: case 0xc9a1: case 0xc9a3: case 0xc9a5:
    case 0xc9a7: case 0xc9a9: case 0xc9ac: case 0xc9af: case 0xc9b2: case 0xc9b5:
    case 0xc9b8: case 0xc9bb: case 0xc9be: case 0xc9c1: case 0xc9c4: case 0xc9c7:
    case 0xc9ca: case 0xc9cd: case 0xc9d0: case 0xc9d2: case 0xc9d4: case 0xc9d6:
    case 0xc9d9: case 0xc9dc: case 0xc9de: case 0xc9e1: case 0xc9e4: case 0xc9e6:
    case 0xc9e9: case 0xc9eb: case 0xc9ed: case 0xc9f1: case 0xc9f4: case 0xc9f6:
    case 0xc9f9: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xc98f: store_dp(c, 0xb7, true); break;
    case 0xc991: cmp_constant8(c, 0x1e); break;
    case 0xc993: branch_to(c, c->z, 0xc9a5); break;
    case 0xc995: load_dp8(c, 0xb7); break;
    case 0xc997: cmp_constant8(c, 0x12); break;
    case 0xc999: case 0xc99d: branch_to(c, c->z, 0xc9a3); break;
    case 0xc99b: cmp_constant8(c, 0x15); break;
    case 0xc99f: cmp_constant8(c, 0x17); break;
    case 0xc9a1: branch_to(c, !c->z, 0xc9a5); break;
    case 0xc9a3: stz_dp8(c, 0xb0); break;
    case 0xc9a5: stz_dp8(c, 0xb8); break;
    case 0xc9a7: stz_dp8(c, 0xb9); break;
    case 0xc9a9: stz_abs8(c, 0x1433); break;
    case 0xc9ac: stz_abs8(c, 0x1473); break;
    case 0xc9af: stz_abs8(c, 0x1435); break;
    case 0xc9b2: stz_abs8(c, 0x1475); break;
    case 0xc9b5: stz_abs8(c, 0x1436); break;
    case 0xc9b8: stz_abs8(c, 0x1476); break;
    case 0xc9bb: stz_abs8(c, 0x14ff); break;
    case 0xc9be: stz_abs8(c, 0x1546); break;
    case 0xc9c1: stz_abs8(c, 0x14d7); break;
    case 0xc9c4: stz_abs8(c, 0x153d); break;
    case 0xc9c7: stz_abs8(c, 0x1558); break;
    case 0xc9ca: stz_abs8(c, 0x1560); break;
    case 0xc9cd: stz_abs8(c, 0x1568); break;
    case 0xc9d0: load_dp8(c, 0xb7); break;
    case 0xc9d2: cmp_constant8(c, 0x1e); break;
    case 0xc9d4: branch_to(c, c->z, 0xc9e1); break;
    case 0xc9d6: stz_abs8(c, 0x1507); break;
    case 0xc9d9: stz_abs8(c, 0x1517); break;
    case 0xc9dc: byte_constant(c, 0x32); break;
    case 0xc9de: store_register8(c, 0x210b); break;
    case 0xc9e1: video_load(c, 0x1480, false); break;
    case 0xc9e4: branch_to(c, !c->z, 0xc9ed); break;
    case 0xc9e6: video_load(c, 0x14e8, false); break;
    case 0xc9e9: cmp_constant8(c, 0x02); break;
    case 0xc9eb: branch_to(c, c->z, 0xc9f1); break;
    case 0xc9ed: jsl_to(c, 0x870b); break;
    case 0xc9f1: jsr_abs_native(c, 0xc9fa); break;
    case 0xc9f4: byte_constant(c, 0x18); break;
    case 0xc9f6: store_register8(c, 0x14c8); break;
    case 0xc9f9: return_short(c); break;
  }
  return true;
}

/* $1D:8A85–$8AA1: scan WRAM list at $1D0100 for A; RTL #$FF if missing else matched A. */
static bool list_scan_1d8a85_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x1d || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0x8a85: case 0x8a87: case 0x8a8a: case 0x8a8c: case 0x8a8e: case 0x8a91:
    case 0x8a95: case 0x8a97: case 0x8a99: case 0x8a9a: case 0x8a9b: case 0x8a9d:
    case 0x8a9f: case 0x8aa1: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8a85: store_dp(c, 0x11, true); break;
    case 0x8a87: video_load(c, 0x01b2, false); break;
    case 0x8a8a: branch_to(c, c->z, 0x8a9f); break;
    case 0x8a8c: store_dp(c, 0x10, true); break;
    case 0x8a8e: index_constant(c, false, 0x0000); break;
    case 0x8a91: load_a_long_x8(c, 0x1d0100); break;
    case 0x8a95: cmp_dp8(c, 0x11); break;
    case 0x8a97: branch_to(c, c->z, 0x8aa1); break;
    case 0x8a99: case 0x8a9a: implied(c); c->x++; zn(c, c->x, false); break;
    case 0x8a9b: dec_dp8(c, 0x10); break;
    case 0x8a9d: branch_to(c, !c->z, 0x8a91); break;
    case 0x8a9f: byte_constant(c, 0xff); break;
    case 0x8aa1: return_long(c); break;
  }
  return true;
}

/* $1D:8AA2–$8AB2: append A into WRAM list at $0100,$01B2; INC $01B2; RTL. */
static bool list_append_1d8aa2_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x1d || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0x8aa2: case 0x8aa3: case 0x8aa5: case 0x8aa6: case 0x8aa9: case 0x8aaa:
    case 0x8aab: case 0x8aac: case 0x8aaf: case 0x8ab2: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8aa2: pha8(c); break;
    case 0x8aa3: byte_constant(c, 0x00); break;
    case 0x8aa5: xba_native(c); break;
    case 0x8aa6: video_load(c, 0x01b2, false); break;
    case 0x8aa9: asl_a8(c); break;
    case 0x8aaa: tax16_from_a8(c); break;
    case 0x8aab: pla8(c); break;
    case 0x8aac: store_a_abs_x8(c, 0x0100); break;
    case 0x8aaf: inc_abs8_native(c, 0x01b2); break;
    case 0x8ab2: return_long(c); break;
  }
  return true;
}

/* $01:F692–$F6DB: fill $0E00.. from $02BF0B[$0EA3×5]; count in $0EA0; JSL $01B8DE; RTL. */
static bool actor_fill_f692_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xf6ca || pc == 0xf6cb || pc == 0xf6cc || pc == 0xf6cf);
  const bool sep_entry = (pc == 0xf6d0);
  const bool rep_entry = (pc == 0xf6c8);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xf692: case 0xf695: case 0xf698: case 0xf69b: case 0xf69d: case 0xf6a0:
    case 0xf6a1: case 0xf6a2: case 0xf6a3: case 0xf6a6: case 0xf6a9: case 0xf6ac:
    case 0xf6ae: case 0xf6b1: case 0xf6b5: case 0xf6b7: case 0xf6b9: case 0xf6bc:
    case 0xf6bf: case 0xf6c1: case 0xf6c4: case 0xf6c7: case 0xf6c8: case 0xf6ca:
    case 0xf6cb: case 0xf6cc: case 0xf6cf: case 0xf6d0: case 0xf6d2: case 0xf6d5:
    case 0xf6d7: case 0xf6db: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf692: stz_abs8(c, 0x0ea0); break;
    case 0xf695: video_load(c, 0x0ea3, false); break;
    case 0xf698: store_register8(c, 0x4202); break;
    case 0xf69b: byte_constant(c, 0x05); break;
    case 0xf69d: store_register8(c, 0x4203); break;
    case 0xf6a0: case 0xf6a1: xba_native(c); break;
    case 0xf6a2: nop_native(c); break;
    case 0xf6a3: load_y_abs16(c, 0x4216); break;
    case 0xf6a6: ldx_abs16(c, 0x4216); break;
    case 0xf6a9: index_constant(c, true, 0x0000); break;
    case 0xf6ac: byte_constant(c, 0x00); break;
    case 0xf6ae: store_a_abs_y8(c, 0x0e00); break;
    case 0xf6b1: load_a_long_x8(c, 0x02bf0b); break;
    case 0xf6b5: cmp_constant8(c, 0xff); break;
    case 0xf6b7: branch_to(c, c->z, 0xf6c7); break;
    case 0xf6b9: store_a_abs_y8(c, 0x0e01); break;
    case 0xf6bc: store_a_abs_y8(c, 0x0e1c); break;
    case 0xf6bf: byte_constant(c, 0x40); break;
    case 0xf6c1: store_a_abs_y8(c, 0x0e00); break;
    case 0xf6c4: inc_abs8_native(c, 0x0ea0); break;
    case 0xf6c7: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xf6c8: sep_rep_native(c, 0x20, false); break;
    case 0xf6ca: tya16(c); break;
    case 0xf6cb: implied(c); c->c = false; break;
    case 0xf6cc: adc16_constant(c, 0x0020); break;
    case 0xf6cf: tay16(c); break;
    case 0xf6d0: sep_rep_native(c, 0x20, true); break;
    case 0xf6d2: cpy16_constant(c, 0x00a0); break;
    case 0xf6d5: branch_to(c, !c->c, 0xf6ac); break;
    case 0xf6d7: jsl_to(c, 0xb8de); break;
    case 0xf6db: return_long(c); break;
  }
  return true;
}

/* $05:CA8D–$CAF7: $CA8D trampoline JSR $DD51 then fall into jump-table body — select bank-$1E
 * stream via $0033,Y / $B6B2, load opcode+operands into DP+$04/$06, JMP ($CAFA,X). */
static bool actor_jt_ca90_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xcaba || pc == 0xcabc || pc == 0xcac0 || pc == 0xcac2 ||
                    pc == 0xcac6 || pc == 0xcad3 || pc == 0xcad6 || pc == 0xcad7 ||
                    pc == 0xcad9 || pc == 0xcae6 || pc == 0xcae7 || pc == 0xcae8 ||
                    pc == 0xcae9 || pc == 0xcaea || pc == 0xcaec || pc == 0xcaee ||
                    pc == 0xcaef || pc == 0xcaf0 || pc == 0xcaf2);
  const bool sep_entry = (pc == 0xcac7 || pc == 0xcadb || pc == 0xcaf4);
  const bool rep_entry = (pc == 0xcab8 || pc == 0xcad1 || pc == 0xcae4);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xca8d:
    case 0xca90: case 0xca92: case 0xca95: case 0xca98: case 0xca9a: case 0xca9c:
    case 0xca9f: case 0xcaa1: case 0xcaa3: case 0xcaa6: case 0xcaa8: case 0xcaaa:
    case 0xcaac: case 0xcaae: case 0xcaaf: case 0xcab1: case 0xcab2: case 0xcab3:
    case 0xcab6: case 0xcab8: case 0xcaba: case 0xcabc: case 0xcac0: case 0xcac2:
    case 0xcac6: case 0xcac7: case 0xcac9: case 0xcacb: case 0xcacd: case 0xcacf:
    case 0xcad1: case 0xcad3: case 0xcad6: case 0xcad7: case 0xcad9: case 0xcadb:
    case 0xcadd: case 0xcae0: case 0xcae1: case 0xcae2: case 0xcae4: case 0xcae6:
    case 0xcae7: case 0xcae8: case 0xcae9: case 0xcaea: case 0xcaec: case 0xcaee:
    case 0xcaef: case 0xcaf0: case 0xcaf2: case 0xcaf4: case 0xcaf6: case 0xcaf7:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xca8d: jsr_abs_native(c, 0xdd51); break;
    case 0xca90: case 0xcacf: load_dp_y16(c, 0x20); break;
    case 0xca92: load_a_abs_y8(c, 0x0033); break;
    case 0xca95: index_constant(c, true, 0x81f7); break;
    case 0xca98: cmp_constant8(c, 0x01); break;
    case 0xca9a: case 0xcaa1: case 0xcaaa: branch_to(c, c->z, 0xcac9); break;
    case 0xca9c: index_constant(c, true, 0x821d); break;
    case 0xca9f: cmp_constant8(c, 0x02); break;
    case 0xcaa3: index_constant(c, true, 0x8240); break;
    case 0xcaa6: cmp_constant8(c, 0x03); break;
    case 0xcaa8: branch_to(c, !c->z, 0xcaac); break;
    case 0xcaac: byte_constant(c, 0x00); break;
    case 0xcaae: case 0xcae1: xba_native(c); break;
    case 0xcaaf: load_dp8(c, 0xb7); break;
    case 0xcab1: asl_a8(c); break;
    case 0xcab2: tax16_from_a8(c); break;
    case 0xcab3: jsr_abs_native(c, 0xb6b2); break;
    case 0xcab6: cpy_dp16(c, 0x20); break;
    case 0xcab8: case 0xcad1: case 0xcae4: sep_rep_native(c, 0x20, false); break;
    case 0xcaba: branch_to(c, !c->z, 0xcac2); break;
    case 0xcabc: lda_long_x16(c, 0x1e8000); break;
    case 0xcac0: branch_to(c, true, 0xcac6); break;
    case 0xcac2: lda_long_x16(c, 0x1e80fa); break;
    case 0xcac6: tay16(c); break;
    case 0xcac7: case 0xcadb: case 0xcaf4: sep_rep_native(c, 0x20, true); break;
    case 0xcac9: sty_dp16(c, 0x00); break;
    case 0xcacb: byte_constant(c, 0x1e); break;
    case 0xcacd: store_dp(c, 0x02, true); break;
    case 0xcad3: load_a_abs_y16(c, 0x0035); break;
    case 0xcad6: implied(c); c->c = false; break;
    case 0xcad7: adc_dp16(c, 0x00); break;
    case 0xcad9: store_dp(c, 0x00, false); break;
    case 0xcadd: index_constant(c, true, 0x0000); break;
    case 0xcae0: /* TYA m8: preserve B */
      implied(c); set_a8(c, (uint8_t)c->y); break;
    case 0xcae2: lda_indirect_long_y8(c, 0x00); break;
    case 0xcae6: asl_a16(c); break;
    case 0xcae7: tax16(c); break;
    case 0xcae8: case 0xcaee: case 0xcaef: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xcae9: phy16(c); break;
    case 0xcaea: case 0xcaf0: lda_indirect_long_y16(c, 0x00); break;
    case 0xcaec: store_dp(c, 0x04, false); break;
    case 0xcaf2: store_dp(c, 0x06, false); break;
    case 0xcaf6: ply16(c); break;
    case 0xcaf7: jmp_indirect_abs_x(c, 0xcafa); break;
  }
  return true;
}

/* $05:DEC3–$DF5F: script walker on [$00] via $002C/$0030,Y; opcodes 00/03/01/22;
 * shared advance leaf $DF51. Includes advance leaf; $DF60 now native. */
static bool actor_script_dec3_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xded7 || pc == 0xded9 || pc == 0xdedb || pc == 0xdede ||
                    pc == 0xdee0 || pc == 0xdee4 || pc == 0xdee6 || pc == 0xdeea);
  const bool sep_entry = (pc == 0xdeec);
  const bool rep_entry = (pc == 0xded5);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xdec3: case 0xdec5: case 0xdec7: case 0xdec8: case 0xdecb: case 0xdecd:
    case 0xdece: case 0xdecf: case 0xded0: case 0xded3: case 0xded5: case 0xded7:
    case 0xded9: case 0xdedb: case 0xdede: case 0xdee0: case 0xdee4: case 0xdee6:
    case 0xdeea: case 0xdeec: case 0xdeee: case 0xdef0: case 0xdef2: case 0xdef4:
    case 0xdef6: case 0xdef7: case 0xdefa: case 0xdefb: case 0xdefd: case 0xdeff:
    case 0xdf01: case 0xdf03: case 0xdf05: case 0xdf08: case 0xdf0a: case 0xdf0d:
    case 0xdf0e: case 0xdf10: case 0xdf12: case 0xdf13: case 0xdf15: case 0xdf17:
    case 0xdf19: case 0xdf1c: case 0xdf1e: case 0xdf21: case 0xdf24: case 0xdf26:
    case 0xdf28: case 0xdf29: case 0xdf2b: case 0xdf2d: case 0xdf2f: case 0xdf32:
    case 0xdf33: case 0xdf36: case 0xdf38: case 0xdf3a: case 0xdf3c: case 0xdf3d:
    case 0xdf3f: case 0xdf41: case 0xdf42: case 0xdf44: case 0xdf46: case 0xdf49:
    case 0xdf4b: case 0xdf4e: case 0xdf51: case 0xdf53: case 0xdf54: case 0xdf57:
    case 0xdf5a: case 0xdf5c: case 0xdf5f: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xdec3: case 0xded9: case 0xdef2: case 0xdf01: case 0xdf15: case 0xdf2d:
    case 0xdf51: load_dp_y16(c, 0x20); break;
    case 0xdec5: case 0xdef4: case 0xdf08: case 0xdf5a: byte_constant(c, 0x00); break;
    case 0xdec7: case 0xdef6: xba_native(c); break;
    case 0xdec8: load_a_abs_y8(c, 0x002c); break;
    case 0xdecb: branch_to(c, c->n, 0xdece); break;
    case 0xdecd: case 0xdf0d: case 0xdf3c: case 0xdf5f: return_short(c); break;
    case 0xdece: asl_a8(c); break;
    case 0xdecf: tax16_from_a8(c); break;
    case 0xded0: jsr_abs_native(c, 0xb6b2); break;
    case 0xded3: cpy_dp16(c, 0x20); break;
    case 0xded5: sep_rep_native(c, 0x20, false); break;
    case 0xded7: branch_to(c, c->z, 0xdee0); break;
    case 0xdedb: cpy16_constant(c, 0x1460); break;
    case 0xdede: branch_to(c, !c->z, 0xdee6); break;
    case 0xdee0: lda_long_x16(c, 0x1d908c); break;
    case 0xdee4: branch_to(c, true, 0xdeea); break;
    case 0xdee6: lda_long_x16(c, 0x1d9183); break;
    case 0xdeea: store_dp(c, 0x00, false); break;
    case 0xdeec: sep_rep_native(c, 0x20, true); break;
    case 0xdeee: byte_constant(c, 0x1d); break;
    case 0xdef0: store_dp(c, 0x02, true); break;
    case 0xdef7: load_a_abs_y8(c, 0x0030); break;
    case 0xdefa: tay16(c); break;
    case 0xdefb: case 0xdf13: case 0xdf29: case 0xdf42: lda_indirect_long_y8(c, 0x00); break;
    case 0xdefd: cmp_constant8(c, 0x00); break;
    case 0xdeff: branch_to(c, !c->z, 0xdf0e); break;
    case 0xdf03: byte_constant(c, 0xff); break;
    case 0xdf05: store_a_abs_y8(c, 0x0030); break;
    case 0xdf0a: store_a_abs_y8(c, 0x002c); break;
    case 0xdf0e: cmp_constant8(c, 0x03); break;
    case 0xdf10: branch_to(c, !c->z, 0xdf24); break;
    case 0xdf12: case 0xdf28: case 0xdf41: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xdf17: ora_constant8(c, 0x80); break;
    case 0xdf19: store_a_abs_y8(c, 0x0021); break;
    case 0xdf1c: case 0xdf38: case 0xdf49: byte_constant(c, 0x02); break;
    case 0xdf1e: case 0xdf4b: jsr_abs_native(c, 0xdf51); break;
    case 0xdf21: case 0xdf4e: jmp_abs_native(c, 0xdec3); break;
    case 0xdf24: cmp_constant8(c, 0x01); break;
    case 0xdf26: branch_to(c, !c->z, 0xdf3d); break;
    case 0xdf2b: store_dp(c, 0x04, true); break;
    case 0xdf2f: load_a_abs_y8(c, 0x002d); break;
    case 0xdf32: inc_a8(c); break;
    case 0xdf33: store_a_abs_y8(c, 0x002d); break;
    case 0xdf36: cmp_dp8(c, 0x04); break;
    case 0xdf3a: branch_to(c, c->c, 0xdf51); break;
    case 0xdf3d: cmp_constant8(c, 0x22); break;
    case 0xdf3f: branch_to(c, !c->z, 0xdf51); break;
    case 0xdf44: store_dp(c, 0x04, true); break;
    case 0xdf46: jsr_abs_native(c, 0xdf60); break;
    case 0xdf53: implied(c); c->c = false; break;
    case 0xdf54: adc_abs_y8(c, 0x0030); break;
    case 0xdf57: store_a_abs_y8(c, 0x0030); break;
    case 0xdf5c: store_a_abs_y8(c, 0x002d); break;
  }
  return true;
}

/* $01:F719–$F7F1: sibling of $F692 — scale $0E00 slot(s) from $02B214 tables via
 * ×$0E multiply; optional RNG JSL $82A3; stride +$20 until Y≥$A0; RTL. */
static bool actor_scale_f719_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x01 || c->e || c->xf || c->d) return false;
  const bool m16b = (pc == 0xf742 || pc == 0xf746 || pc == 0xf749 || pc == 0xf74c ||
                     pc == 0xf750 || pc == 0xf753 || pc == 0xf778 || pc == 0xf77c ||
                     pc == 0xf798 || pc == 0xf79c || pc == 0xf7ac || pc == 0xf7b0 ||
                     pc == 0xf7b1 || pc == 0xf7b3 || pc == 0xf7b5 || pc == 0xf7c7 ||
                     pc == 0xf7cb || pc == 0xf7cc || pc == 0xf7ce || pc == 0xf7cf ||
                     pc == 0xf7d1 || pc == 0xf7e1 || pc == 0xf7e2 || pc == 0xf7e3 ||
                     pc == 0xf7e6);
  const bool sep_b = (pc == 0xf756 || pc == 0xf77e || pc == 0xf79e ||
                      pc == 0xf7b8 || pc == 0xf7d4 || pc == 0xf7e7);
  const bool rep_b = (pc == 0xf740 || pc == 0xf776 || pc == 0xf796 ||
                      pc == 0xf7aa || pc == 0xf7c5 || pc == 0xf7df);
  if(m16b && c->mf) return false;
  if(sep_b && c->mf) return false;
  if(rep_b && !c->mf) return false;
  if(!m16b && !sep_b && !rep_b && !c->mf) return false;
  switch(pc) {
    case 0xf719: case 0xf71c: case 0xf71d: case 0xf71f: case 0xf722: case 0xf724:
    case 0xf727: case 0xf72a: case 0xf72c: case 0xf72f: case 0xf732: case 0xf735:
    case 0xf737: case 0xf73a: case 0xf73b: case 0xf73c: case 0xf73d: case 0xf740:
    case 0xf742: case 0xf746: case 0xf749: case 0xf74c: case 0xf750: case 0xf753:
    case 0xf756: case 0xf758: case 0xf75c: case 0xf75f: case 0xf763: case 0xf766:
    case 0xf76a: case 0xf76d: case 0xf771: case 0xf774: case 0xf775: case 0xf776:
    case 0xf778: case 0xf77c: case 0xf77e: case 0xf780: case 0xf784: case 0xf786:
    case 0xf78a: case 0xf78b: case 0xf78d: case 0xf78f: case 0xf790: case 0xf792:
    case 0xf794: case 0xf795: case 0xf796: case 0xf798: case 0xf79c: case 0xf79e:
    case 0xf7a0: case 0xf7a1: case 0xf7a5: case 0xf7a7: case 0xf7a8: case 0xf7aa:
    case 0xf7ac: case 0xf7b0: case 0xf7b1: case 0xf7b3: case 0xf7b5: case 0xf7b8:
    case 0xf7ba: case 0xf7be: case 0xf7c0: case 0xf7c3: case 0xf7c5: case 0xf7c7:
    case 0xf7cb: case 0xf7cc: case 0xf7ce: case 0xf7cf: case 0xf7d1: case 0xf7d4:
    case 0xf7d6: case 0xf7da: case 0xf7dc: case 0xf7df: case 0xf7e1: case 0xf7e2:
    case 0xf7e3: case 0xf7e6: case 0xf7e7: case 0xf7e9: case 0xf7ec: case 0xf7ee:
    case 0xf7f1: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf719: video_load(c, 0x0ea0, false); break;
    case 0xf71c: lsr_a8(c); break;
    case 0xf71d: branch_to(c, !c->c, 0xf724); break;
    case 0xf71f: index_constant(c, true, 0x0000); break;
    case 0xf722: branch_to(c, true, 0xf727); break;
    case 0xf724: index_constant(c, true, 0x0020); break;
    case 0xf727: load_a_abs_y8(c, 0x0e00); break;
    case 0xf72a: branch_to(c, !c->z, 0xf72f); break;
    case 0xf72c: jmp_abs_native(c, 0xf7df); break;
    case 0xf72f: load_a_abs_y8(c, 0x0e01); break;
    case 0xf732: store_register8(c, 0x4202); break;
    case 0xf735: byte_constant(c, 0x0e); break;
    case 0xf737: store_register8(c, 0x4203); break;
    case 0xf73a: case 0xf73b: xba_native(c); break;
    case 0xf73c: nop_native(c); break;
    case 0xf73d: ldx_abs16(c, 0x4216); break;
    case 0xf740: case 0xf776: case 0xf796: case 0xf7aa: case 0xf7c5: case 0xf7df:
      sep_rep_native(c, 0x20, false); break;
    case 0xf742: lda_long_x16(c, 0x02b216); break;
    case 0xf746: store_a_abs_y16(c, 0x0e0a); break;
    case 0xf749: store_a_abs_y16(c, 0x0e03); break;
    case 0xf74c: lda_long_x16(c, 0x02b21b); break;
    case 0xf750: store_a_abs_y16(c, 0x0e0c); break;
    case 0xf753: store_a_abs_y16(c, 0x0e05); break;
    case 0xf756: case 0xf77e: case 0xf79e: case 0xf7b8: case 0xf7d4: case 0xf7e7:
      sep_rep_native(c, 0x20, true); break;
    case 0xf758: load_a_long_x8(c, 0x02b21e); break;
    case 0xf75c: store_a_abs_y8(c, 0x0e0e); break;
    case 0xf75f: load_a_long_x8(c, 0x02b21d); break;
    case 0xf763: store_a_abs_y8(c, 0x0e17); break;
    case 0xf766: load_a_long_x8(c, 0x02b215); break;
    case 0xf76a: store_a_abs_y8(c, 0x0e02); break;
    case 0xf76d: load_a_long_x8(c, 0x02b214); break;
    case 0xf771: store_a_abs_y8(c, 0x0e1b); break;
    case 0xf774: phx16(c); break;
    case 0xf775: phy16(c); break;
    case 0xf778: lda_long_x16(c, 0x02b218); break;
    case 0xf77c: store_dp(c, 0x00, false); break;
    case 0xf780: load_a_long_x8(c, 0x02b21a); break;
    case 0xf784: store_dp(c, 0x02, true); break;
    case 0xf786: jsl_to(c, 0xe969); break;
    case 0xf78a: ply16(c); break;
    case 0xf78b: byte_constant(c, 0x00); break;
    case 0xf78d: store_dp(c, 0x04, true); break;
    case 0xf78f: xba_native(c); break;
    case 0xf790: load_dp8(c, 0x03); break;
    case 0xf792: and_constant8(c, 0x07); break;
    case 0xf794: asl_a8(c); break;
    case 0xf795: tax16_from_a8(c); break;
    case 0xf798: lda_long_x16(c, 0x01f7f2); break;
    case 0xf79c: store_dp(c, 0x12, false); break;
    case 0xf7a0: plx16(c); break;
    case 0xf7a1: jsl_to(c, 0x82a3); break;
    case 0xf7a5: load_dp8(c, 0x2f); break;
    case 0xf7a7: lsr_a8(c); break;
    case 0xf7a8: branch_to(c, !c->c, 0xf7c5); break;
    case 0xf7ac: case 0xf7c7: lda_long_x16(c, 0x02b218); break;
    case 0xf7b0: case 0xf7cb: implied(c); c->c = true; break;
    case 0xf7b1: case 0xf7cc: sbc_dp16(c, 0x03); break;
    case 0xf7b3: sbc_dp16(c, 0x12); break;
    case 0xf7b5: case 0xf7d1: store_a_abs_y16(c, 0x0e07); break;
    case 0xf7ba: case 0xf7d6: load_a_long_x8(c, 0x02b21a); break;
    case 0xf7be: sbc8_constant(c, 0x00); break;
    case 0xf7c0: case 0xf7dc: store_a_abs_y8(c, 0x0e09); break;
    case 0xf7c3: branch_to(c, true, 0xf7df); break;
    case 0xf7ce: implied(c); c->c = false; break;
    case 0xf7cf: adc_dp16(c, 0x12); break;
    case 0xf7da: adc8_constant(c, 0x00); break;
    case 0xf7e1: tya16(c); break;
    case 0xf7e2: implied(c); c->c = false; break;
    case 0xf7e3: adc16_constant(c, 0x0020); break;
    case 0xf7e6: tay16(c); break;
    case 0xf7e9: cpy16_constant(c, 0x00a0); break;
    case 0xf7ec: branch_to(c, c->c, 0xf7f1); break;
    case 0xf7ee: jmp_abs_native(c, 0xf727); break;
    case 0xf7f1: return_long(c); break;
  }
  return true;
}


/* $05:B6B2–$B6CD: Y=#$1400/#$1440 from $14C2 bit0 (B6B2 same-polarity; B6C0 inverted). */
static bool slot_select_b6b2_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xb6b2: case 0xb6b5: case 0xb6b8: case 0xb6ba: case 0xb6bc: case 0xb6bf:
    case 0xb6c0: case 0xb6c3: case 0xb6c6: case 0xb6c8: case 0xb6ca: case 0xb6cd:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb6b2: case 0xb6c0: index_constant(c, true, 0x1400); break;
    case 0xb6b5: case 0xb6c3: video_load(c, 0x14c2, false); break;
    case 0xb6b8: case 0xb6c6: and_constant8(c, 0x01); break;
    case 0xb6ba: branch_to(c, c->z, 0xb6bf); break;
    case 0xb6bc: case 0xb6ca: index_constant(c, true, 0x1440); break;
    case 0xb6bf: case 0xb6cd: return_short(c); break;
    case 0xb6c8: branch_to(c, !c->z, 0xb6cd); break;
  }
  return true;
}

/* $05:DF60–$DFC1: opcode-$22 face adjust via $B6C0 / $05DFC2 / $1D8FEC tables; JSR $95F6 (native); RTS. */
static bool actor_face_df60_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xdf94 || pc == 0xdf97 || pc == 0xdf9a || pc == 0xdf9c ||
                    pc == 0xdf9d || pc == 0xdf9f || pc == 0xdfa1 || pc == 0xdfa2 ||
                    pc == 0xdfa4);
  const bool sep_entry = (pc == 0xdfa7);
  const bool rep_entry = (pc == 0xdf92);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xdf60: case 0xdf62: case 0xdf64: case 0xdf67: case 0xdf69: case 0xdf6a:
    case 0xdf6d: case 0xdf6e: case 0xdf72: case 0xdf73: case 0xdf74: case 0xdf76:
    case 0xdf77: case 0xdf78: case 0xdf79: case 0xdf7b: case 0xdf7c: case 0xdf7e:
    case 0xdf7f: case 0xdf82: case 0xdf83: case 0xdf87: case 0xdf8a: case 0xdf8e:
    case 0xdf90: case 0xdf92: case 0xdf94: case 0xdf97: case 0xdf9a: case 0xdf9c:
    case 0xdf9d: case 0xdf9f: case 0xdfa1: case 0xdfa2: case 0xdfa4: case 0xdfa7:
    case 0xdfa9: case 0xdfab: case 0xdfae: case 0xdfb0: case 0xdfb3: case 0xdfb5:
    case 0xdfb8: case 0xdfbb: case 0xdfbe: case 0xdfc1: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xdf60: dec_dp8(c, 0x04); break;
    case 0xdf62: asl_dp8(c, 0x04); break;
    case 0xdf64: jsr_abs_native(c, 0xb6c0); break;
    case 0xdf67: byte_constant(c, 0x00); break;
    case 0xdf69: xba_native(c); break;
    case 0xdf6a: load_a_abs_y8(c, 0x0034); break;
    case 0xdf6d: tax16_from_a8(c); break;
    case 0xdf6e: load_a_long_x8(c, 0x05dfc2); break;
    case 0xdf72: case 0xdf73: case 0xdf76: case 0xdf77: asl_a8(c); break;
    case 0xdf74: case 0xdf8e: store_dp(c, 0x00, true); break;
    case 0xdf78: case 0xdf7b: case 0xdf9c: implied(c); c->c = false; break;
    case 0xdf79: adc8_dp(c, 0x00); break;
    case 0xdf7c: adc8_dp(c, 0x04); break;
    case 0xdf7e: tax16_from_a8(c); break;
    case 0xdf7f: load_a_abs_y8(c, 0x0026); break;
    case 0xdf82: case 0xdfa1: implied(c); c->c = true; break;
    case 0xdf83: sbc_long_x8(c, 0x1d8fed); break;
    case 0xdf87: store_register8(c, 0x1556); break;
    case 0xdf8a: load_a_long_x8(c, 0x1d8fec); break;
    case 0xdf90: stz_dp8(c, 0x01); break;
    case 0xdf92: sep_rep_native(c, 0x20, false); break;
    case 0xdf94: load_a_abs_y16(c, 0x0024); break;
    case 0xdf97: cpy16_constant(c, 0x1400); break;
    case 0xdf9a: branch_to(c, !c->z, 0xdfa1); break;
    case 0xdf9d: adc_dp16(c, 0x00); break;
    case 0xdf9f: branch_to(c, true, 0xdfa4); break;
    case 0xdfa2: sbc_dp16(c, 0x00); break;
    case 0xdfa4: cmp_constant16(c, 0x0100); break;
    case 0xdfa7: sep_rep_native(c, 0x20, true); break;
    case 0xdfa9: branch_to(c, c->c, 0xdfbe); break;
    case 0xdfab: store_register8(c, 0x1554); break;
    case 0xdfae: byte_constant(c, 0x80); break;
    case 0xdfb0: store_register8(c, 0x1552); break;
    case 0xdfb3: byte_constant(c, 0x08); break;
    case 0xdfb5: store_register8(c, 0x1553); break;
    case 0xdfb8: stz_abs8(c, 0x1555); break;
    case 0xdfbb: stz_abs8(c, 0x1557); break;
    case 0xdfbe: jsr_abs_native(c, 0x95f6); break;
    case 0xdfc1: return_short(c); break;
  }
  return true;
}

/* $05:E102–$E116: $1546 BMI gate; $1540×2 → JMP ($E117,X); $F3AA handler now native. */
static bool actor_dispatch_e102_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xe110 || pc == 0xe111);
  const bool sep_entry = (pc == 0xe112);
  const bool rep_entry = (pc == 0xe10e);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xe102: case 0xe105: case 0xe107: case 0xe108: case 0xe10a: case 0xe10b:
    case 0xe10e: case 0xe110: case 0xe111: case 0xe112: case 0xe114: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe102: video_load(c, 0x1546, false); break;
    case 0xe105: branch_to(c, c->n, 0xe108); break;
    case 0xe107: return_short(c); break;
    case 0xe108: byte_constant(c, 0x00); break;
    case 0xe10a: xba_native(c); break;
    case 0xe10b: video_load(c, 0x1540, false); break;
    case 0xe10e: sep_rep_native(c, 0x20, false); break;
    case 0xe110: asl_a16(c); break;
    case 0xe111: tax16(c); break;
    case 0xe112: sep_rep_native(c, 0x20, true); break;
    case 0xe114: jmp_indirect_abs_x(c, 0xe117); break;
  }
  return true;
}

/* $1D:8AB3–$8ABC: clear list byte at $1D0101,X then JML $01C639. */
static bool list_clear_1d8ab3_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x1d || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x8ab3: case 0x8ab5: case 0x8ab9: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8ab3: byte_constant(c, 0x00); break;
    case 0x8ab5: store_a_long_x8(c, 0x1d0101); break;
    case 0x8ab9: jml_to(c, 0xc639, 0x01); break;
  }
  return true;
}

/* $05:93A9–$9487: seed $7E3000 grid, copy face ids into $1640.., optional $14D2 remap; JML $0089AC. */
static bool actor_seed_93a9_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x93ae || pc == 0x93b1 || pc == 0x93b3 || pc == 0x93b6 ||
                    pc == 0x93b9 || pc == 0x93bb || pc == 0x93be || pc == 0x93c2 ||
                    pc == 0x93c3 || pc == 0x93c4 || pc == 0x93c6 || pc == 0x93c8 ||
                    pc == 0x93c9 || pc == 0x93ca || pc == 0x93cd || pc == 0x93ce ||
                    pc == 0x93d0);
  const bool sep_entry = (pc == 0x93d2);
  const bool rep_entry = (pc == 0x93ac);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0x93a9: case 0x93ac: case 0x93ae: case 0x93b1: case 0x93b3: case 0x93b6:
    case 0x93b9: case 0x93bb: case 0x93be: case 0x93c2: case 0x93c3: case 0x93c4:
    case 0x93c6: case 0x93c8: case 0x93c9: case 0x93ca: case 0x93cd: case 0x93ce:
    case 0x93d0: case 0x93d2: case 0x93d4: case 0x93d7: case 0x93d9: case 0x93db:
    case 0x93de: case 0x93e1: case 0x93e3: case 0x93e6: case 0x93e8: case 0x93eb:
    case 0x93ee: case 0x93f1: case 0x93f3: case 0x93f5: case 0x93f8: case 0x93fb:
    case 0x93fd: case 0x9400: case 0x9402: case 0x9405: case 0x9407: case 0x940a:
    case 0x940c: case 0x940e: case 0x9411: case 0x9414: case 0x9417: case 0x9419:
    case 0x941b: case 0x941e: case 0x941f: case 0x9422: case 0x9425: case 0x9426:
    case 0x9429: case 0x942c: case 0x942f: case 0x9432: case 0x9434: case 0x9437:
    case 0x943a: case 0x943c: case 0x943f: case 0x9442: case 0x9445: case 0x9447:
    case 0x944a: case 0x944c: case 0x944f: case 0x9451: case 0x9453: case 0x9454:
    case 0x9456: case 0x945a: case 0x945d: case 0x9460: case 0x9461: case 0x9463:
    case 0x9465: case 0x9467: case 0x9468: case 0x946a: case 0x946e: case 0x9471:
    case 0x9474: case 0x9476: case 0x9478: case 0x947b: case 0x947e: case 0x9481:
    case 0x9484: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x93a9: store_y_abs16(c, 0x14e4); break;
    case 0x93ac: sep_rep_native(c, 0x20, false); break;
    case 0x93ae: lda16_constant(c, 0x0005); break;
    case 0x93b1: store_dp(c, 0x10, false); break;
    case 0x93b3: index_constant(c, false, 0x05ce); break;
    case 0x93b6: lda16_constant(c, 0x0011); break;
    case 0x93b9: store_dp(c, 0x12, false); break;
    case 0x93bb: lda16_constant(c, 0x0001); break;
    case 0x93be: store_a_long_x16(c, 0x7e3000); break;
    case 0x93c2: case 0x93c3: case 0x9453: case 0x9460: case 0x9467:
      implied(c); c->x++; zn(c, c->x, false); break;
    case 0x93c4: case 0x93ce: dec_dp16(c, pc == 0x93c4 ? 0x12 : 0x10); break;
    case 0x93c6: branch_to(c, !c->n, 0x93bb); break;
    case 0x93c8: txa16(c); break;
    case 0x93c9: implied(c); c->c = false; break;
    case 0x93ca: adc16_constant(c, 0x001c); break;
    case 0x93cd: tax16(c); break;
    case 0x93d0: branch_to(c, !c->n, 0x93b6); break;
    case 0x93d2: sep_rep_native(c, 0x20, true); break;
    case 0x93d4: case 0x93ee: video_load(c, 0x15f1, false); break;
    case 0x93d7: case 0x93f1: cmp_constant8(c, 0x02); break;
    case 0x93d9: branch_to(c, !c->z, 0x93e8); break;
    case 0x93db: load_y_abs16(c, 0x14c3); break;
    case 0x93de: cpy_abs16(c, 0x15f2); break;
    case 0x93e1: branch_to(c, !c->z, 0x93e8); break;
    case 0x93e3: video_load(c, 0x15f6, false); break;
    case 0x93e6: branch_to(c, true, 0x93eb); break;
    case 0x93e8: video_load(c, 0x1401, false); break;
    case 0x93eb: store_register8(c, 0x1640); break;
    case 0x93f3: branch_to(c, !c->z, 0x9402); break;
    case 0x93f5: load_y_abs16(c, 0x14c5); break;
    case 0x93f8: cpy_abs16(c, 0x15f4); break;
    case 0x93fb: branch_to(c, !c->z, 0x9402); break;
    case 0x93fd: video_load(c, 0x15f7, false); break;
    case 0x9400: branch_to(c, true, 0x9411); break;
    case 0x9402: video_load(c, 0x14ee, false); break;
    case 0x9405: branch_to(c, !c->z, 0x940e); break;
    case 0x9407: video_load(c, 0x145c, false); break;
    case 0x940a: cmp_constant8(c, 0x05); break;
    case 0x940c: branch_to(c, c->z, 0x9411); break;
    case 0x940e: video_load(c, 0x1441, false); break;
    case 0x9411: store_register8(c, 0x1642); break;
    case 0x9414: video_load(c, 0x14c2, false); break;
    case 0x9417: and_constant8(c, 0x01); break;
    case 0x9419: branch_to(c, c->z, 0x9429); break;
    case 0x941b: video_load(c, 0x1640, false); break;
    case 0x941e: pha8(c); break;
    case 0x941f: video_load(c, 0x1642, false); break;
    case 0x9422: store_register8(c, 0x1640); break;
    case 0x9425: pla8(c); break;
    case 0x9426: store_register8(c, 0x1642); break;
    case 0x9429: jsr_abs_native(c, 0xb6b2); break;
    case 0x942c: load_a_abs_y8(c, 0x0012); break;
    case 0x942f: store_register8(c, 0x1644); break;
    case 0x9432: byte_constant(c, 0x80); break;
    case 0x9434: store_register8(c, 0x1641); break;
    case 0x9437: store_register8(c, 0x1643); break;
    case 0x943a: byte_constant(c, 0x20); break;
    case 0x943c: store_register8(c, 0x1645); break;
    case 0x943f: load_y_abs16(c, 0x14e4); break;
    case 0x9442: cpy16_constant(c, 0x011e); break;
    case 0x9445: branch_to(c, !c->z, 0x947b); break;
    case 0x9447: video_load(c, 0x14d2, false); break;
    case 0x944a: store_dp(c, 0x10, true); break;
    case 0x944c: index_constant(c, false, 0x0000); break;
    case 0x944f: case 0x9461: lsr_dp8(c, 0x10); break;
    case 0x9451: branch_to(c, c->c, 0x9456); break;
    case 0x9454: branch_to(c, true, 0x944f); break;
    case 0x9456: case 0x946a: load_a_long_x8(c, 0x05aa0b); break;
    case 0x945a: store_register8(c, 0x1640); break;
    case 0x945d: case 0x9471: jsr_abs_native(c, 0x9491); break;
    case 0x9463: branch_to(c, c->c, 0x946a); break;
    case 0x9465: branch_to(c, c->z, 0x9476); break;
    case 0x9468: branch_to(c, true, 0x9461); break;
    case 0x946e: store_register8(c, 0x1642); break;
    case 0x9474: branch_to(c, true, 0x947b); break;
    case 0x9476: byte_constant(c, 0x52); break;
    case 0x9478: store_register8(c, 0x1642); break;
    case 0x947b: index_constant(c, true, 0x003c); break;
    case 0x947e: store_y_abs16(c, 0x0726); break;
    case 0x9481: index_constant(c, true, 0x0000); break;
    case 0x9484: jml_to(c, 0x89ac, 0x00); break;
  }
  return true;
}

/* $1D:8ABD–$8B45: flight/input face nudge — $01D6/$01E7/$15FA/$4F/$50/$1482..$1484; RTL. */
static bool flight_input_1d8abd_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x1d || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0x8ae8 || pc == 0x8afd || pc == 0x8b00 || pc == 0x8b03);
  const bool sep_entry = (pc == 0x8aeb || pc == 0x8b06);
  const bool rep_entry = (pc == 0x8ae6 || pc == 0x8afb);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0x8abd: case 0x8ac0: case 0x8ac2: case 0x8ac3: case 0x8ac6: case 0x8ac8:
    case 0x8aca: case 0x8acd: case 0x8acf: case 0x8ad1: case 0x8ad3: case 0x8ad4:
    case 0x8ad6: case 0x8ad8: case 0x8ada: case 0x8adc: case 0x8ade: case 0x8adf:
    case 0x8ae1: case 0x8ae4: case 0x8ae6: case 0x8ae8: case 0x8aeb: case 0x8aed:
    case 0x8af0: case 0x8af2: case 0x8af5: case 0x8af7: case 0x8af9: case 0x8afb:
    case 0x8afd: case 0x8b00: case 0x8b03: case 0x8b06: case 0x8b08: case 0x8b0a:
    case 0x8b0d: case 0x8b0f: case 0x8b12: case 0x8b14: case 0x8b17: case 0x8b19:
    case 0x8b1b: case 0x8b1d: case 0x8b1e: case 0x8b20: case 0x8b22: case 0x8b24:
    case 0x8b26: case 0x8b29: case 0x8b2b: case 0x8b2d: case 0x8b2f: case 0x8b31:
    case 0x8b33: case 0x8b35: case 0x8b37: case 0x8b3a: case 0x8b3c: case 0x8b3e:
    case 0x8b40: case 0x8b42: case 0x8b45: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8abd: video_load(c, 0x01d6, false); break;
    case 0x8ac0: branch_to(c, c->n, 0x8ac3); break;
    case 0x8ac2: case 0x8b45: return_long(c); break;
    case 0x8ac3: video_load(c, 0x01e7, false); break;
    case 0x8ac6: and_constant8(c, 0x02); break;
    case 0x8ac8: branch_to(c, c->z, 0x8b45); break;
    case 0x8aca: case 0x8b14: video_load(c, 0x15fa, false); break;
    case 0x8acd: case 0x8b17: cmp_constant8(c, 0x80); break;
    case 0x8acf: branch_to(c, c->z, 0x8b12); break;
    case 0x8ad1: case 0x8b1b: load_dp8(c, 0x50); break;
    case 0x8ad3: case 0x8ade: case 0x8b1d: lsr_a8(c); break;
    case 0x8ad4: branch_to(c, c->c, 0x8adc); break;
    case 0x8ad6: case 0x8adc: case 0x8b1e: case 0x8b31: load_dp8(c, 0x4f); break;
    case 0x8ad8: cmp_constant8(c, 0x40); break;
    case 0x8ada: branch_to(c, !c->c, 0x8b12); break;
    case 0x8adf: branch_to(c, c->c, 0x8b12); break;
    case 0x8ae1: bit_abs8(c, 0x1482); break;
    case 0x8ae4: branch_to(c, c->v, 0x8afb); break;
    case 0x8ae6: case 0x8afb: sep_rep_native(c, 0x20, false); break;
    case 0x8ae8: inc_abs16(c, 0x1484); break;
    case 0x8aeb: case 0x8b06: sep_rep_native(c, 0x20, true); break;
    case 0x8aed: video_load(c, 0x1485, false); break;
    case 0x8af0: branch_to(c, !c->z, 0x8b12); break;
    case 0x8af2: video_load(c, 0x1484, false); break;
    case 0x8af5: cmp_constant8(c, 0xc0); break;
    case 0x8af7: branch_to(c, c->c, 0x8b0a); break;
    case 0x8af9: branch_to(c, !c->c, 0x8b12); break;
    case 0x8afd: dec_abs16(c, 0x1484); break;
    case 0x8b00: video_load(c, 0x1484, false); break; /* M16 LDA */
    case 0x8b03: cmp_constant16(c, 0x0040); break;
    case 0x8b08: branch_to(c, c->c, 0x8b12); break;
    case 0x8b0a: video_load(c, 0x1482, false); break;
    case 0x8b0d: eor_constant8(c, 0x40); break;
    case 0x8b0f: store_register8(c, 0x1482); break;
    case 0x8b12: stz_dp8(c, 0x00); break;
    case 0x8b19: branch_to(c, c->z, 0x8b2d); break;
    case 0x8b20: branch_to(c, c->c, 0x8b33); break;
    case 0x8b22: branch_to(c, !c->z, 0x8b29); break;
    case 0x8b24: byte_constant(c, 0x03); break;
    case 0x8b26: store_register8(c, 0x1481); break;
    case 0x8b29: cmp_constant8(c, 0x40); break;
    case 0x8b2b: branch_to(c, c->c, 0x8b33); break;
    case 0x8b2d: case 0x8b2f: inc_dp8(c, 0x00); break;
    case 0x8b33: and_constant8(c, 0x0f); break;
    case 0x8b35: branch_to(c, !c->z, 0x8b45); break;
    case 0x8b37: video_load(c, 0x1481, false); break;
    case 0x8b3a: eor_constant8(c, 0x01); break;
    case 0x8b3c: and_constant8(c, 0x01); break;
    case 0x8b3e: ora_dp8(c, 0x00); break;
    case 0x8b40: ora_constant8(c, 0x80); break;
    case 0x8b42: store_register8(c, 0x1481); break;
  }
  return true;
}

/* $05:A435–$A52D: battle face/setup probe with nested $A4DF/$A4F5 leaves; several RTS. */
static bool actor_probe_a435_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xa49c || pc == 0xa49f);
  const bool sep_entry = (pc == 0xa4a2);
  const bool rep_entry = (pc == 0xa49a);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xa435: case 0xa438: case 0xa43b: case 0xa43e: case 0xa441: case 0xa443:
    case 0xa446: case 0xa449: case 0xa44c: case 0xa44e: case 0xa451: case 0xa453:
    case 0xa456: case 0xa458: case 0xa45b: case 0xa45d: case 0xa45f: case 0xa462:
    case 0xa465: case 0xa468: case 0xa46a: case 0xa46b: case 0xa46e: case 0xa470:
    case 0xa473: case 0xa476: case 0xa478: case 0xa47b: case 0xa47d: case 0xa480:
    case 0xa483: case 0xa484: case 0xa487: case 0xa48a: case 0xa48c: case 0xa48f:
    case 0xa491: case 0xa493: case 0xa496: case 0xa498: case 0xa49a: case 0xa49c:
    case 0xa49f: case 0xa4a2: case 0xa4a4: case 0xa4a7: case 0xa4a8: case 0xa4ac:
    case 0xa4af: case 0xa4b2: case 0xa4b4: case 0xa4b6: case 0xa4b9: case 0xa4bc:
    case 0xa4be: case 0xa4c0: case 0xa4c2: case 0xa4c4: case 0xa4c6: case 0xa4c8:
    case 0xa4ca: case 0xa4cc: case 0xa4cf: case 0xa4d1: case 0xa4d3: case 0xa4d6:
    case 0xa4d8: case 0xa4db: case 0xa4de: case 0xa4df: case 0xa4e1: case 0xa4e4:
    case 0xa4e7: case 0xa4e9: case 0xa4ec: case 0xa4ee: case 0xa4f1: case 0xa4f4:
    case 0xa4f5: case 0xa4f8: case 0xa4fb: case 0xa4fd: case 0xa4ff: case 0xa502:
    case 0xa504: case 0xa506: case 0xa508: case 0xa50b: case 0xa50d: case 0xa50f:
    case 0xa511: case 0xa514: case 0xa516: case 0xa518: case 0xa51a: case 0xa51d:
    case 0xa520: case 0xa522: case 0xa525: case 0xa528: case 0xa52c: case 0xa52d:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xa435: jsr_abs_native(c, 0xb124); break;
    case 0xa438: stz_abs8(c, 0x1527); break;
    case 0xa43b: stz_abs8(c, 0x1528); break;
    case 0xa43e: video_load(c, 0x01d6, false); break;
    case 0xa441: branch_to(c, c->n, 0xa446); break;
    case 0xa443: jsr_abs_native(c, 0xa52e); break;
    case 0xa446: index_constant(c, true, 0x0004); break;
    case 0xa449: video_load(c, 0x14cc, false); break;
    case 0xa44c: branch_to(c, c->z, 0xa46a); break;
    case 0xa44e: video_load(c, 0x14cd, false); break;
    case 0xa451: branch_to(c, !c->z, 0xa458); break;
    case 0xa453: index_constant(c, true, 0x0007); break;
    case 0xa456: branch_to(c, true, 0xa46a); break;
    case 0xa458: index_constant(c, true, 0x0003); break;
    case 0xa45b: cmp_constant8(c, 0x01); break;
    case 0xa45d: branch_to(c, c->z, 0xa46a); break;
    case 0xa45f: video_load(c, 0x14cf, false); break;
    case 0xa462: ora_abs8(c, 0x14d0); break;
    case 0xa465: ora_abs8(c, 0x14d1); break;
    case 0xa468: branch_to(c, !c->z, 0xa46b); break;
    case 0xa46a: case 0xa483: case 0xa4a7: case 0xa4de: case 0xa4f4:
    case 0xa52c: case 0xa52d: return_short(c); break;
    case 0xa46b: jsr_abs_native(c, 0xbddc); break;
    case 0xa46e: branch_to(c, !c->c, 0xa484); break;
    case 0xa470: load_a_abs_y8(c, 0x0003); break;
    case 0xa473: ora_abs_y8(c, 0x0004); break;
    case 0xa476: branch_to(c, c->z, 0xa484); break;
    case 0xa478: cpy16_constant(c, 0x1400); break;
    case 0xa47b: branch_to(c, c->z, 0xa480); break;
    case 0xa47d: jsr_abs_native(c, 0xa4f5); break;
    case 0xa480: index_constant(c, true, 0x0001); break;
    case 0xa484: jsr_abs_native(c, 0xb6c0); break;
    case 0xa487: cpy16_constant(c, 0x1440); break;
    case 0xa48a: branch_to(c, !c->z, 0xa4a8); break;
    case 0xa48c: case 0xa4b9: video_load(c, 0x0ea3, false); break;
    case 0xa48f: cmp_constant8(c, 0x51); break;
    case 0xa491: branch_to(c, !c->z, 0xa4a8); break;
    case 0xa493: video_load(c, 0x15f1, false); break;
    case 0xa496: cmp_constant8(c, 0x02); break;
    case 0xa498: branch_to(c, c->z, 0xa4a8); break;
    case 0xa49a: sep_rep_native(c, 0x20, false); break;
    case 0xa49c: lda16_constant(c, 0x0001); break;
    case 0xa49f: store_a_abs_y16(c, 0x0003); break;
    case 0xa4a2: sep_rep_native(c, 0x20, true); break;
    case 0xa4a4: index_constant(c, true, 0x0001); break;
    case 0xa4a8: jsl_to(c, 0xa9aa); break;
    case 0xa4ac: index_constant(c, true, 0x0002); break;
    case 0xa4af: video_load(c, 0x14c2, false); break;
    case 0xa4b2: and_constant8(c, 0x01); break;
    case 0xa4b4: branch_to(c, !c->z, 0xa483); break;
    case 0xa4b6: index_constant(c, true, 0x0067); break;
    case 0xa4bc: cmp_constant8(c, 0x56); break;
    case 0xa4be: branch_to(c, c->z, 0xa4cc); break;
    case 0xa4c0: cmp_constant8(c, 0x59); break;
    case 0xa4c2: branch_to(c, c->z, 0xa480); break;
    case 0xa4c4: cmp_constant8(c, 0x58); break;
    case 0xa4c6: branch_to(c, c->c, 0xa4cc); break;
    case 0xa4c8: cmp_constant8(c, 0x53); break;
    case 0xa4ca: branch_to(c, c->c, 0xa480); break;
    case 0xa4cc: video_load(c, 0x14e8, false); break;
    case 0xa4cf: cmp_constant8(c, 0x02); break;
    case 0xa4d1: branch_to(c, !c->z, 0xa483); break;
    case 0xa4d3: jsr_abs_native(c, 0xa4df); break;
    case 0xa4d6: byte_constant(c, 0x8f); break;
    case 0xa4d8: store_register8(c, 0x1489); break;
    case 0xa4db: index_constant(c, true, 0x004a); break;
    case 0xa4df: byte_constant(c, 0x00); break;
    case 0xa4e1: jsr_abs_native(c, 0xae95); break;
    case 0xa4e4: load_a_abs_y8(c, 0x0b00); break;
    case 0xa4e7: and_constant8(c, 0xbf); break;
    case 0xa4e9: store_a_abs_y8(c, 0x0b00); break;
    case 0xa4ec: byte_constant(c, 0x00); break;
    case 0xa4ee: store_a_abs_y8(c, 0x0b03); break;
    case 0xa4f1: store_a_abs_y8(c, 0x0b04); break;
    case 0xa4f5: load_y_abs16(c, 0x1443); break;
    case 0xa4f8: video_load(c, 0x1441, false); break;
    case 0xa4fb: cmp_constant8(c, 0x35); break;
    case 0xa4fd: branch_to(c, !c->z, 0xa504); break;
    case 0xa4ff: index_constant(c, true, 0x012c); break;
    case 0xa502: branch_to(c, true, 0xa51d); break;
    case 0xa504: cmp_constant8(c, 0x36); break;
    case 0xa506: branch_to(c, !c->z, 0xa50d); break;
    case 0xa508: index_constant(c, true, 0x0014); break;
    case 0xa50b: branch_to(c, true, 0xa51d); break;
    case 0xa50d: cmp_constant8(c, 0x1e); break;
    case 0xa50f: branch_to(c, !c->z, 0xa516); break;
    case 0xa511: index_constant(c, true, 0x001e); break;
    case 0xa514: branch_to(c, true, 0xa51d); break;
    case 0xa516: cmp_constant8(c, 0x37); break;
    case 0xa518: branch_to(c, !c->z, 0xa52d); break;
    case 0xa51a: index_constant(c, true, 0x0032); break;
    case 0xa51d: cpy_abs16(c, 0x1443); break;
    case 0xa520: branch_to(c, !c->c, 0xa52d); break;
    case 0xa522: store_y_abs16(c, 0x1443); break;
    case 0xa525: stz_abs8(c, 0x1440); break;
    case 0xa528: jsl_to(c, 0x8c33); break;
  }
  return true;
}

/* ---- Helpers for 95F6 / 9491 / A9F4 / BDDC / D521 / F3AA / F998 / F952 / 8B7C ---- */
static void eor_constant16(Cpu *c, uint16_t value) {
  (void)fetch(c); sample_interrupt(c); (void)fetch(c);
  c->a = (uint16_t)(c->a ^ value); zn(c, c->a, false);
}
static void tyx16(Cpu *c) {
  implied(c); c->x = c->y; zn(c, c->x, false);
}
static void cmp_indirect_long_y8(Cpu *c, uint8_t offset) {
  uint16_t dp = dp_address(c, offset);
  uint32_t pointer = c->read(c->mem, dp);
  pointer |= (uint32_t)c->read(c->mem, (uint16_t)(dp + 1)) << 8;
  pointer |= (uint32_t)c->read(c->mem, (uint16_t)(dp + 2)) << 16;
  sample_interrupt(c);
  uint8_t value = c->read(c->mem, (pointer + c->y) & 0xffffffu);
  uint8_t a = (uint8_t)c->a;
  c->c = a >= value; zn(c, (uint8_t)(a - value), true);
}

static void and_abs8(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c); sample_interrupt(c);
  set_a8(c, (uint8_t)((uint8_t)c->a & c->read(c->mem, ((uint32_t)c->db << 16) | address)));
}
static void cmp_long_x8(Cpu *c, uint32_t base) {
  (void)fetch(c); (void)fetch(c); (void)fetch(c);
  sample_interrupt(c);
  uint8_t value = c->read(c->mem, (base + c->x) & 0xffffffu);
  uint8_t a = (uint8_t)c->a;
  c->c = a >= value; zn(c, (uint8_t)(a - value), true);
}
static void sbc_abs16(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c);
  uint32_t low = ((uint32_t)c->db << 16) | address;
  uint16_t value = c->read(c->mem, low); sample_interrupt(c);
  value |= (uint16_t)c->read(c->mem, (low + 1) & 0xffffffu) << 8;
  uint16_t old = c->a;
  unsigned result = (unsigned)old - value - (c->c ? 0u : 1u);
  uint16_t out = (uint16_t)result;
  c->v = ((old ^ out) & (old ^ value) & 0x8000u) != 0;
  c->c = result < 0x10000u;
  c->a = out; zn(c, out, false);
}

/* $05:95F6–$9625: face-class → APU cmd #$1C/#$1D via $0485B6; closes $DF60. */
static bool actor_face_95f6_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x95f6: case 0x95f9: case 0x95fc: case 0x95fe: case 0x9600: case 0x9602:
    case 0x9604: case 0x9606: case 0x9608: case 0x960a: case 0x960c: case 0x960e:
    case 0x9610: case 0x9612: case 0x9614: case 0x9616: case 0x9618: case 0x9619:
    case 0x961b: case 0x961d: case 0x961f: case 0x9621: case 0x9625: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x95f6: jsr_abs_native(c, 0xb6b2); break;
    case 0x95f9: load_a_abs_y8(c, 0x0021); break;
    case 0x95fc: and_constant8(c, 0x7f); break;
    case 0x95fe: cmp_constant8(c, 0x06); break;
    case 0x9600: branch_to(c, c->z, 0x961f); break;
    case 0x9602: cmp_constant8(c, 0x07); break;
    case 0x9604: branch_to(c, c->z, 0x961f); break;
    case 0x9606: cmp_constant8(c, 0x0f); break;
    case 0x9608: branch_to(c, c->z, 0x961f); break;
    case 0x960a: cmp_constant8(c, 0x0c); break;
    case 0x960c: branch_to(c, c->z, 0x961b); break;
    case 0x960e: cmp_constant8(c, 0x0d); break;
    case 0x9610: branch_to(c, c->z, 0x961b); break;
    case 0x9612: cmp_constant8(c, 0x10); break;
    case 0x9614: branch_to(c, c->z, 0x961b); break;
    case 0x9616: load_dp8(c, 0x4f); break;
    case 0x9618: lsr_a8(c); break;
    case 0x9619: branch_to(c, !c->c, 0x961f); break;
    case 0x961b: byte_constant(c, 0x1d); break;
    case 0x961d: branch_to(c, true, 0x9621); break;
    case 0x961f: byte_constant(c, 0x1c); break;
    case 0x9621: jsl_to(c, 0x85b6); break;
    case 0x9625: return_short(c); break;
  }
  return true;
}

/* $05:B124–$B130: $14C2 bit0 invert +3 → DP+$B6; RTS. First callee of $A435. */
static bool actor_seed_b124_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xb124: case 0xb127: case 0xb129: case 0xb12b: case 0xb12c: case 0xb12e:
    case 0xb130: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb124: video_load(c, 0x14c2, false); break;
    case 0xb127: and_constant8(c, 0x01); break;
    case 0xb129: eor_constant8(c, 0x01); break;
    case 0xb12b: implied(c); c->c = false; break;
    case 0xb12c: adc8_constant(c, 0x03); break;
    case 0xb12e: store_dp(c, 0xb6, true); break;
    case 0xb130: return_short(c); break;
  }
  return true;
}

/* $05:BDDC–$BE11: $1700&3 gate via $B6C0; SEC early or subtract $14CF/$14D1 from $0003,Y. */
static bool actor_probe_bddc_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xbe00 || pc == 0xbe03 || pc == 0xbe04 || pc == 0xbe07);
  const bool sep_entry = (pc == 0xbe0a);
  const bool rep_entry = (pc == 0xbdfe);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xbddc: case 0xbddf: case 0xbde2: case 0xbde4: case 0xbde6: case 0xbde8:
    case 0xbdea: case 0xbdec: case 0xbdef: case 0xbdf1: case 0xbdf3: case 0xbdf6:
    case 0xbdf8: case 0xbdfa: case 0xbdfb: case 0xbdfc: case 0xbdfe: case 0xbe00:
    case 0xbe03: case 0xbe04: case 0xbe07: case 0xbe0a: case 0xbe0c: case 0xbe0e:
    case 0xbe11: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xbddc: jsr_abs_native(c, 0xb6c0); break;
    case 0xbddf: video_load(c, 0x1700, false); break;
    case 0xbde2: and_constant8(c, 0x03); break;
    case 0xbde4: branch_to(c, c->z, 0xbdfe); break;
    case 0xbde6: cmp_constant8(c, 0x03); break;
    case 0xbde8: branch_to(c, c->z, 0xbdfa); break;
    case 0xbdea: sty_dp16(c, 0x00); break;
    case 0xbdec: index_constant(c, true, 0x1400); break;
    case 0xbdef: cmp_constant8(c, 0x01); break;
    case 0xbdf1: branch_to(c, c->z, 0xbdf6); break;
    case 0xbdf3: index_constant(c, true, 0x1440); break;
    case 0xbdf6: cpy_dp16(c, 0x00); break;
    case 0xbdf8: branch_to(c, !c->z, 0xbdfc); break;
    case 0xbdfa: implied(c); c->c = true; break;
    case 0xbdfb: case 0xbe11: return_short(c); break;
    case 0xbdfc: load_dp_y16(c, 0x00); break;
    case 0xbdfe: sep_rep_native(c, 0x20, false); break;
    case 0xbe00: load_a_abs_y16(c, 0x0003); break;
    case 0xbe03: implied(c); c->c = true; break;
    case 0xbe04: sbc_abs16(c, 0x14cf); break;
    case 0xbe07: store_a_abs_y16(c, 0x0003); break;
    case 0xbe0a: sep_rep_native(c, 0x20, true); break;
    case 0xbe0c: byte_constant(c, 0x00); break;
    case 0xbe0e: sbc_abs8(c, 0x14d1); break;
  }
  return true;
}

/* $05:A9F4–$AA0A: scan $05AA0B table; ASL bit into DP+$00 or STZ; RTS. */
static bool actor_bit_a9f4_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xa9f4: case 0xa9f7: case 0xa9f9: case 0xa9fa: case 0xa9fe: case 0xaa00:
    case 0xaa02: case 0xaa03: case 0xaa06: case 0xaa08: case 0xaa0a: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xa9f4: index_constant(c, false, 0x0001); break;
    case 0xa9f7: stx_dp16(c, 0x00); break;
    case 0xa9f9: implied(c); c->x--; zn(c, c->x, false); break;
    case 0xa9fa: cmp_long_x8(c, 0x05aa0b); break;
    case 0xa9fe: branch_to(c, c->z, 0xaa0a); break;
    case 0xaa00: asl_dp8(c, 0x00); break;
    case 0xaa02: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xaa03: cpx16_constant(c, 0x0007); break;
    case 0xaa06: branch_to(c, !c->c, 0xa9fa); break;
    case 0xaa08: stz_dp8(c, 0x00); break;
    case 0xaa0a: return_short(c); break;
  }
  return true;
}

/* $05:9491–$94A0: PHX; $A9F4; mask-clear bit in $14D2 via ~DP+$00; PLX; RTS. */
static bool actor_remap_9491_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x9491: case 0x9492: case 0x9495: case 0x9497: case 0x9499: case 0x949c:
    case 0x949f: case 0x94a0: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x9491: phx16(c); break;
    case 0x9492: jsr_abs_native(c, 0xa9f4); break;
    case 0x9495: load_dp8(c, 0x00); break;
    case 0x9497: eor_constant8(c, 0xff); break;
    case 0x9499: and_abs8(c, 0x14d2); break;
    case 0x949c: store_register8(c, 0x14d2); break;
    case 0x949f: plx16(c); break;
    case 0x94a0: return_short(c); break;
  }
  return true;
}

/* $05:D521–$D571: ($CAFA,0)/(,6) $D521+$D530 leaf; sibling $D531; ($CAFA,2) $D55B;
 * shared $D566 helper (INC $002B,Y; CMP DP+$04 → C). */
static bool actor_op_d521_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xd521: case 0xd524: case 0xd526: case 0xd528: case 0xd52a: case 0xd52c:
    case 0xd52e: case 0xd530:
    case 0xd531: case 0xd533: case 0xd535: case 0xd537: case 0xd539: case 0xd53c:
    case 0xd53e: case 0xd53f: case 0xd541: case 0xd542: case 0xd544: case 0xd546:
    case 0xd549: case 0xd54b: case 0xd54d: case 0xd550: case 0xd553: case 0xd555:
    case 0xd556: case 0xd558:
    case 0xd55b: case 0xd55e: case 0xd560: case 0xd561: case 0xd563:
    case 0xd566: case 0xd568: case 0xd56b: case 0xd56c: case 0xd56f: case 0xd571:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xd521: jsr_abs_native(c, 0xb6b2); break;
    case 0xd524: byte_constant(c, 0x80); break;
    case 0xd526: cpy_dp16(c, 0x20); break;
    case 0xd528: branch_to(c, c->z, 0xd52c); break;
    case 0xd52a: byte_constant(c, 0x40); break;
    case 0xd52c: ora_dp8(c, 0xb8); break;
    case 0xd52e: store_dp(c, 0xb8, true); break;
    case 0xd530: case 0xd555: case 0xd560: case 0xd571: return_short(c); break;
    case 0xd531: case 0xd566: load_dp_y16(c, 0x20); break;
    case 0xd533: load_dp8(c, 0x05); break;
    case 0xd535: cmp_constant8(c, 0x25); break;
    case 0xd537: branch_to(c, !c->z, 0xd546); break;
    case 0xd539: load_a_abs_y8(c, 0x001b); break;
    case 0xd53c: cmp_constant8(c, 0x06); break;
    case 0xd53e: php_native(c); break;
    case 0xd53f: byte_constant(c, 0x25); break;
    case 0xd541: plp_native(c); break;
    case 0xd542: branch_to(c, !c->z, 0xd546); break;
    case 0xd544: byte_constant(c, 0x27); break;
    case 0xd546: cmp_abs_y8(c, 0x0021); break;
    case 0xd549: branch_to(c, c->z, 0xd550); break;
    case 0xd54b: ora_constant8(c, 0x80); break;
    case 0xd54d: store_a_abs_y8(c, 0x0021); break;
    case 0xd550: case 0xd55b: jsr_abs_native(c, 0xd566); break;
    case 0xd553: branch_to(c, c->c, 0xd556); break;
    case 0xd556: byte_constant(c, 0x02); break;
    case 0xd558: case 0xd563: jmp_abs_native(c, 0xdd51); break;
    case 0xd55e: branch_to(c, c->c, 0xd561); break;
    case 0xd561: byte_constant(c, 0x01); break;
    case 0xd568: load_a_abs_y8(c, 0x002b); break;
    case 0xd56b: inc_a8(c); break;
    case 0xd56c: store_a_abs_y8(c, 0x002b); break;
    case 0xd56f: cmp_dp8(c, 0x04); break;
  }
  return true;
}

/* $05:F3AA–$F40F: ($E117,0) handler — $F998 gate on $1546; two tails through RTS. */
static bool actor_hdl_f3aa_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xf3aa: case 0xf3ad: case 0xf3b0: case 0xf3b2: case 0xf3b4: case 0xf3b6:
    case 0xf3b8: case 0xf3ba: case 0xf3bd: case 0xf3bf: case 0xf3c2: case 0xf3c5:
    case 0xf3c9: case 0xf3cc: case 0xf3cf: case 0xf3d2: case 0xf3d4: case 0xf3d7:
    case 0xf3da: case 0xf3dd: case 0xf3e0: case 0xf3e3: case 0xf3e4: case 0xf3e7:
    case 0xf3ea: case 0xf3ed: case 0xf3f0: case 0xf3f3: case 0xf3f6: case 0xf3f9:
    case 0xf3fc: case 0xf3fd: case 0xf3ff: case 0xf402: case 0xf404: case 0xf407:
    case 0xf409: case 0xf40c: case 0xf40f: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf3aa: jsr_abs_native(c, 0xf998); break;
    case 0xf3ad: video_load(c, 0x1546, false); break;
    case 0xf3b0: cmp_constant8(c, 0x80); break;
    case 0xf3b2: branch_to(c, !c->z, 0xf3e4); break;
    case 0xf3b4: load_dp8(c, 0xb0); break;
    case 0xf3b6: and_constant8(c, 0xfd); break;
    case 0xf3b8: store_dp(c, 0xb0, true); break;
    case 0xf3ba: index_constant(c, false, 0x0162); break;
    case 0xf3bd: stx_dp16(c, 0x10); break;
    case 0xf3bf: index_constant(c, false, 0x0000); break;
    case 0xf3c2: stx_abs16(c, 0x17dd); break;
    case 0xf3c5: jsl_to(c, 0x99e7); break;
    case 0xf3c9: index_constant(c, false, 0x0050); break;
    case 0xf3cc: jsr_abs_native(c, 0xb6b2); break;
    case 0xf3cf: cpy16_constant(c, 0x1440); break;
    case 0xf3d2: branch_to(c, c->z, 0xf3da); break;
    case 0xf3d4: jsr_abs_native(c, 0xf952); break;
    case 0xf3d7: index_constant(c, false, 0xffb0); break;
    case 0xf3da: stx_abs16(c, 0x17db); break;
    case 0xf3dd: case 0xf3e4: jsr_abs_native(c, 0xf91e); break;
    case 0xf3e0: inc_abs8_native(c, 0x1546); break;
    case 0xf3e3: case 0xf40f: return_short(c); break;
    case 0xf3e7: index_constant(c, true, 0x6400); break;
    case 0xf3ea: store_y_abs16(c, 0x0801); break;
    case 0xf3ed: index_constant(c, true, 0x0800); break;
    case 0xf3f0: store_y_abs16(c, 0x0803); break;
    case 0xf3f3: index_constant(c, false, 0x003f); break;
    case 0xf3f6: load_a_abs_x8(c, 0x0280); break;
    case 0xf3f9: store_a_abs_x8(c, 0x0240); break;
    case 0xf3fc: implied(c); c->x--; zn(c, c->x, false); break;
    case 0xf3fd: branch_to(c, !c->n, 0xf3f6); break;
    case 0xf3ff: inc_abs8_native(c, 0x0715); break;
    case 0xf402: byte_constant(c, 0x42); break;
    case 0xf404: store_register8(c, 0x210b); break;
    case 0xf407: byte_constant(c, 0x61); break;
    case 0xf409: store_register8(c, 0x2108); break;
    case 0xf40c: stz_abs8(c, 0x1546); break;
  }
  return true;
}

/* $05:A52E–$A580: deepen $A435 — $B6C0/$B582 gates; optional $1C8136 seed; RTS.
 * Fail branches land at unrecovered $A581+. */
static bool actor_probe_a52e_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xa54a || pc == 0xa54d || pc == 0xa550 || pc == 0xa553 ||
                    pc == 0xa556 || pc == 0xa559 || pc == 0xa56f || pc == 0xa572);
  const bool sep_entry = (pc == 0xa55c || pc == 0xa575);
  const bool rep_entry = (pc == 0xa548 || pc == 0xa56d);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xa52e: case 0xa531: case 0xa534: case 0xa537: case 0xa539: case 0xa53c:
    case 0xa53f: case 0xa541: case 0xa542: case 0xa545: case 0xa548: case 0xa54a:
    case 0xa54d: case 0xa550: case 0xa553: case 0xa556: case 0xa559: case 0xa55c:
    case 0xa55e: case 0xa561: case 0xa564: case 0xa567: case 0xa568: case 0xa56c:
    case 0xa56d: case 0xa56f: case 0xa572: case 0xa575: case 0xa577: case 0xa57a:
    case 0xa57d: case 0xa580: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xa52e: jsr_abs_native(c, 0xb6c0); break;
    case 0xa531: load_a_abs_y8(c, 0x0001); break;
    case 0xa534: case 0xa53c: jsr_abs_native(c, 0xb582); break;
    case 0xa537: branch_to(c, !c->z, 0xa581); break;
    case 0xa539: load_a_abs_y8(c, 0x0020); break;
    case 0xa53f: branch_to(c, c->z, 0xa581); break;
    case 0xa541: plx16(c); break;
    case 0xa542: load_a_abs_y8(c, 0x0020); break;
    case 0xa545: store_a_abs_y8(c, 0x0001); break;
    case 0xa548: case 0xa56d: sep_rep_native(c, 0x20, false); break;
    case 0xa54a: load_a_abs_y16(c, 0x0007); break;
    case 0xa54d: store_a_abs16(c, 0x17ea); break;
    case 0xa550: stz_abs16(c, 0x17ed); break;
    case 0xa553: lda16_constant(c, 0x000a); break;
    case 0xa556: store_a_abs16(c, 0x17ef); break;
    case 0xa559: stz_abs16(c, 0x17f1); break;
    case 0xa55c: case 0xa575: sep_rep_native(c, 0x20, true); break;
    case 0xa55e: load_a_abs_y8(c, 0x0009); break;
    case 0xa561: store_register8(c, 0x17ec); break;
    case 0xa564: stz_abs8(c, 0x17f3); break;
    case 0xa567: phy16(c); break;
    case 0xa568: jsl_to(c, 0x8136); break; /* bank byte → $1C */
    case 0xa56c: ply16(c); break;
    case 0xa56f: video_load(c, 0x17ea, false); break; /* M16 LDA $17EA */
    case 0xa572: store_a_abs_y16(c, 0x0007); break;
    case 0xa577: video_load(c, 0x17ec, false); break;
    case 0xa57a: store_a_abs_y8(c, 0x0009); break;
    case 0xa57d: index_constant(c, true, 0x0040); break;
    case 0xa580: return_short(c); break;
  }
  return true;
}

/* $1D:8B46–$8B7B: after $8ABD — $0C35 gates via $8AA2/$0594A1; seed $14D8/$14DA; RTL. */
static bool flight_cont_1d8b46_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x1d || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x8b46: case 0x8b49: case 0x8b4b: case 0x8b4d: case 0x8b4f: case 0x8b53:
    case 0x8b56: case 0x8b59: case 0x8b5b: case 0x8b5d: case 0x8b5f: case 0x8b63:
    case 0x8b66: case 0x8b68: case 0x8b6b: case 0x8b6d: case 0x8b71: case 0x8b73:
    case 0x8b76: case 0x8b78: case 0x8b7b: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8b46: case 0x8b56: case 0x8b63: video_load(c, 0x0c35, false); break;
    case 0x8b49: and_constant8(c, 0x08); break;
    case 0x8b4b: branch_to(c, c->z, 0x8b53); break;
    case 0x8b4d: byte_constant(c, 0x14); break;
    case 0x8b4f: case 0x8b5f: jsl_to(c, 0x8aa2); break;
    case 0x8b53: stz_abs8(c, 0x14d8); break;
    case 0x8b59: and_constant8(c, 0x40); break;
    case 0x8b5b: branch_to(c, c->z, 0x8b7b); break;
    case 0x8b5d: byte_constant(c, 0x12); break;
    case 0x8b66: and_constant8(c, 0xbf); break;
    case 0x8b68: store_register8(c, 0x0c35); break;
    case 0x8b6b: byte_constant(c, 0x53); break;
    case 0x8b6d: jsl_to(c, 0x94a1); break; /* bank → $05 */
    case 0x8b71: load_dp_y16(c, 0x00); break;
    case 0x8b73: store_y_abs16(c, 0x14d8); break;
    case 0x8b76: load_dp8(c, 0x02); break;
    case 0x8b78: store_register8(c, 0x14da); break;
    case 0x8b7b: return_long(c); break;
  }
  return true;
}

/* $05:F998–$F9AF: clear $7F0000/$7F0800 word pairs for X=0..$07FE; RTS.
 * Unblocks $F3AA and ($E117,X) bodies that JSR here first. */
static bool clear_7f_f998_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xf99d || pc == 0xf99e || pc == 0xf9a2 || pc == 0xf9a6 ||
                    pc == 0xf9a7 || pc == 0xf9a8 || pc == 0xf9ab);
  const bool sep_entry = (pc == 0xf9ad);
  const bool rep_entry = (pc == 0xf99b);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xf998: case 0xf99b: case 0xf99d: case 0xf99e: case 0xf9a2: case 0xf9a6:
    case 0xf9a7: case 0xf9a8: case 0xf9ab: case 0xf9ad: case 0xf9af: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf998: index_constant(c, false, 0x0000); break;
    case 0xf99b: sep_rep_native(c, 0x20, false); break;
    case 0xf99d: txa16(c); break;
    case 0xf99e: store_a_long_x16(c, 0x7f0000); break;
    case 0xf9a2: store_a_long_x16(c, 0x7f0800); break;
    case 0xf9a6: case 0xf9a7: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xf9a8: cpx16_constant(c, 0x0800); break;
    case 0xf9ab: branch_to(c, !c->c, 0xf99e); break;
    case 0xf9ad: sep_rep_native(c, 0x20, true); break;
    case 0xf9af: return_short(c); break;
  }
  return true;
}

/* $05:F91E–$F951: VRAM-queue seed at $0800 (F91E) + $1541→$0801 length pick (F93D). */
static bool queue_seed_f91e_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xf91e: case 0xf921: case 0xf924: case 0xf927: case 0xf92a: case 0xf92d:
    case 0xf930: case 0xf932: case 0xf935: case 0xf936: case 0xf939: case 0xf93c:
    case 0xf93d: case 0xf940: case 0xf942: case 0xf944: case 0xf947: case 0xf949:
    case 0xf94b: case 0xf94e: case 0xf951: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf91e: index_constant(c, true, 0x6000); break;
    case 0xf921: store_y_abs16(c, 0x0801); break;
    case 0xf924: index_constant(c, true, 0x0000); break;
    case 0xf927: store_y_abs16(c, 0x0803); break;
    case 0xf92a: index_constant(c, true, 0x0580); break;
    case 0xf92d: store_y_abs16(c, 0x0806); break;
    case 0xf930: byte_constant(c, 0x7f); break;
    case 0xf932: store_register8(c, 0x0805); break;
    case 0xf935: inc_a8(c); break;
    case 0xf936: store_register8(c, 0x0800); break;
    case 0xf939: stz_abs8(c, 0x0808); break;
    case 0xf93c: case 0xf951: return_short(c); break;
    case 0xf93d: video_load(c, 0x1541, false); break;
    case 0xf940: cmp_constant8(c, 0x02); break;
    case 0xf942: branch_to(c, c->z, 0xf951); break;
    case 0xf944: index_constant(c, true, 0x6800); break;
    case 0xf947: cmp_constant8(c, 0x03); break;
    case 0xf949: branch_to(c, c->z, 0xf94e); break;
    case 0xf94b: index_constant(c, true, 0x7000); break;
    case 0xf94e: store_y_abs16(c, 0x0801); break;
  }
  return true;
}

/* $05:F952–$F997: mirror-XOR $7F0000 rows (EOR #$4000) with $96D7 stride; RTS.
 * Called from $F3AA when Y!=$1440 after $B6B2. */
static bool mirror_xor_f952_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xf95a || pc == 0xf95d || pc == 0xf95f || pc == 0xf963 ||
                    pc == 0xf965 || pc == 0xf966 || pc == 0xf967 || pc == 0xf96b ||
                    pc == 0xf96d || pc == 0xf970 || pc == 0xf974 || pc == 0xf975 ||
                    pc == 0xf976 || pc == 0xf979 || pc == 0xf97d || pc == 0xf97f ||
                    pc == 0xf980 || pc == 0xf981 || pc == 0xf982 || pc == 0xf983 ||
                    pc == 0xf985 || pc == 0xf987 || pc == 0xf988 || pc == 0xf989 ||
                    pc == 0xf98c || pc == 0xf98d || pc == 0xf992 || pc == 0xf995);
  const bool sep_entry = (pc == 0xf990);
  const bool rep_entry = (pc == 0xf958);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xf952: case 0xf955: case 0xf958: case 0xf95a: case 0xf95d: case 0xf95f:
    case 0xf963: case 0xf965: case 0xf966: case 0xf967: case 0xf96b: case 0xf96d:
    case 0xf970: case 0xf974: case 0xf975: case 0xf976: case 0xf979: case 0xf97d:
    case 0xf97f: case 0xf980: case 0xf981: case 0xf982: case 0xf983: case 0xf985:
    case 0xf987: case 0xf988: case 0xf989: case 0xf98c: case 0xf98d: case 0xf990:
    case 0xf992: case 0xf995: case 0xf997: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf952: index_constant(c, false, 0x0000); break;
    case 0xf955: index_constant(c, true, 0x003e); break;
    case 0xf958: sep_rep_native(c, 0x20, false); break;
    case 0xf95a: lda16_constant(c, 0x000f); break;
    case 0xf95d: store_dp(c, 0x04, false); break;
    case 0xf95f: case 0xf967: lda_long_x16(c, 0x7f0000); break;
    case 0xf963: stx_dp16(c, 0x00); break;
    case 0xf965: pha16(c); break;
    case 0xf966: case 0xf975: tyx16(c); break;
    case 0xf96b: case 0xf97d: ldx_dp16(c, 0x00); break;
    case 0xf96d: case 0xf976: eor_constant16(c, 0x4000); break;
    case 0xf970: case 0xf979: store_a_long_x16(c, 0x7f0000); break;
    case 0xf974: pla16(c); break;
    case 0xf97f: case 0xf980: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xf981: case 0xf982: implied(c); c->y--; zn(c, c->y, false); break;
    case 0xf983: dec_dp16(c, 0x04); break;
    case 0xf985: branch_to(c, !c->n, 0xf95f); break;
    case 0xf987: tya16(c); break;
    case 0xf988: implied(c); c->c = false; break;
    case 0xf989: adc16_constant(c, 0x0060); break;
    case 0xf98c: tay16(c); break;
    case 0xf98d: jsr_abs_native(c, 0x96d7); break;
    case 0xf990: sep_rep_native(c, 0x20, true); break;
    case 0xf992: cpx16_constant(c, 0x0580); break;
    case 0xf995: branch_to(c, !c->c, 0xf958); break;
    case 0xf997: return_short(c); break;
  }
  return true;
}

/* $1D:8B7C–$8BDB: post-$8B7B sibling — if $14D8 set, optional [DP] bound check,
 * copy $1D8BDC.. into $1400, seed $1414/$1416/$1418, JSL $1D8AB3, clear, RTL. */
static bool flight_post_1d8b7c_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x1d || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x8b7c: case 0x8b7f: case 0x8b81: case 0x8b83: case 0x8b85: case 0x8b88:
    case 0x8b8a: case 0x8b8c: case 0x8b8f: case 0x8b91: case 0x8b93: case 0x8b95:
    case 0x8b98: case 0x8b9a: case 0x8b9c: case 0x8b9f: case 0x8ba3: case 0x8ba6:
    case 0x8ba7: case 0x8ba9: case 0x8bac: case 0x8baf: case 0x8bb2: case 0x8bb5:
    case 0x8bb8: case 0x8bb9: case 0x8bba: case 0x8bbb: case 0x8bbc: case 0x8bbd:
    case 0x8bc0: case 0x8bc3: case 0x8bc5: case 0x8bc6: case 0x8bc9: case 0x8bca:
    case 0x8bcb: case 0x8bcc: case 0x8bd0: case 0x8bd3: case 0x8bd6: case 0x8bd9:
    case 0x8bdb: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8b7c: case 0x8b95: video_load(c, 0x14d8, false); break;
    case 0x8b7f: branch_to(c, c->z, 0x8bdb); break;
    case 0x8b81: load_dp8(c, 0x04); break;
    case 0x8b83: branch_to(c, c->z, 0x8b9c); break;
    case 0x8b85: index_constant(c, false, 0x0b14); break;
    case 0x8b88: load_dp8(c, 0x03); break;
    case 0x8b8a: branch_to(c, c->z, 0x8b8f); break;
    case 0x8b8c: index_constant(c, false, 0x0e14); break;
    case 0x8b8f: stx_dp16(c, 0x00); break;
    case 0x8b91: byte_constant(c, 0x1d); break;
    case 0x8b93: store_dp(c, 0x02, true); break;
    case 0x8b98: cmp_indirect_long_y8(c, 0x00); break;
    case 0x8b9a: branch_to(c, !c->c, 0x8bdb); break;
    case 0x8b9c: index_constant(c, false, 0x001f); break;
    case 0x8b9f: load_a_long_x8(c, 0x1d8bdc); break;
    case 0x8ba3: store_a_abs_x8(c, 0x1400); break;
    case 0x8ba6: implied(c); c->x--; zn(c, c->x, false); break;
    case 0x8ba7: branch_to(c, !c->n, 0x8b9f); break;
    case 0x8ba9: load_y_abs16(c, 0x14d8); break;
    case 0x8bac: store_y_abs16(c, 0x1414); break;
    case 0x8baf: video_load(c, 0x14da, false); break;
    case 0x8bb2: store_register8(c, 0x1416); break;
    case 0x8bb5: video_load(c, 0x0c73, false); break;
    case 0x8bb8: case 0x8bb9: case 0x8bba: case 0x8bbb: case 0x8bbc: lsr_a8(c); break;
    case 0x8bbd: ora_abs8(c, 0x1418); break;
    case 0x8bc0: store_register8(c, 0x1418); break;
    case 0x8bc3: byte_constant(c, 0x00); break;
    case 0x8bc5: xba_native(c); break;
    case 0x8bc6: video_load(c, 0x01b2, false); break;
    case 0x8bc9: dec_a8(c); break;
    case 0x8bca: asl_a8(c); break;
    case 0x8bcb: tax16_from_a8(c); break;
    case 0x8bcc: jsl_to(c, 0x8ab3); break;
    case 0x8bd0: stz_abs8(c, 0x14d8); break;
    case 0x8bd3: index_constant(c, true, 0xffff); break;
    case 0x8bd6: stz_abs8(c, 0x14c2); break;
    case 0x8bd9: stz_dp8(c, 0x03); break;
    case 0x8bdb: return_long(c); break;
  }
  return true;
}

/* ---- Helpers for DD51 / DFF3 / D743 / F410 ---- */
static void ror_dp8(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  uint8_t value = c->read(c->mem, address); idle(c);
  bool carry_in = c->c;
  c->c = (value & 1u) != 0;
  value = (uint8_t)((value >> 1) | (carry_in ? 0x80u : 0));
  sample_interrupt(c); c->write(c->mem, address, value); zn(c, value, true);
}
static void dec_a16(Cpu *c) {
  implied(c); c->a--; zn(c, c->a, false);
}

/* $05:DD51–$DD68: wrap leaf — INC A into $0035/$0036,Y; clear $002B,Y; RTS.
 * JMP target from $D55B/$D531/$D81E/$D849/$D743 tails. */
static bool actor_wrap_dd51_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xdd51: case 0xdd53: case 0xdd54: case 0xdd55: case 0xdd58: case 0xdd5b:
    case 0xdd5e: case 0xdd60: case 0xdd63: case 0xdd65: case 0xdd68: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xdd51: load_dp_y16(c, 0x20); break;
    case 0xdd53: inc_a8(c); break;
    case 0xdd54: implied(c); c->c = false; break;
    case 0xdd55: adc_abs_y8(c, 0x0035); break;
    case 0xdd58: store_a_abs_y8(c, 0x0035); break;
    case 0xdd5b: load_a_abs_y8(c, 0x0036); break;
    case 0xdd5e: adc8_constant(c, 0x00); break;
    case 0xdd60: store_a_abs_y8(c, 0x0036); break;
    case 0xdd63: byte_constant(c, 0x00); break;
    case 0xdd65: store_a_abs_y8(c, 0x002b); break;
    case 0xdd68: return_short(c); break;
  }
  return true;
}

/* $05:DD69–$DDD0: ±$14C9/$14CA into actor X/Y fields ($002E/$0024 or $002F/$0026). */
static bool actor_move_dd69_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xdd77 || pc == 0xdd7a || pc == 0xdd7d ||
                    pc == 0xdd91 || pc == 0xdd94 || pc == 0xdd97 ||
                    pc == 0xddab || pc == 0xddae || pc == 0xddb1 ||
                    pc == 0xddc5 || pc == 0xddc8 || pc == 0xddcb);
  const bool sep_entry = (pc == 0xdd80 || pc == 0xdd9a || pc == 0xddb4 || pc == 0xddce);
  const bool rep_entry = (pc == 0xdd75 || pc == 0xdd8f || pc == 0xdda9 || pc == 0xddc3);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xdd69: case 0xdd6b: case 0xdd6e: case 0xdd6f: case 0xdd72: case 0xdd75:
    case 0xdd77: case 0xdd7a: case 0xdd7d: case 0xdd80: case 0xdd82:
    case 0xdd83: case 0xdd85: case 0xdd88: case 0xdd89: case 0xdd8c: case 0xdd8f:
    case 0xdd91: case 0xdd94: case 0xdd97: case 0xdd9a: case 0xdd9c:
    case 0xdd9d: case 0xdd9f: case 0xdda2: case 0xdda3: case 0xdda6: case 0xdda9:
    case 0xddab: case 0xddae: case 0xddb1: case 0xddb4: case 0xddb6:
    case 0xddb7: case 0xddb9: case 0xddbc: case 0xddbd: case 0xddc0: case 0xddc3:
    case 0xddc5: case 0xddc8: case 0xddcb: case 0xddce: case 0xddd0: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xdd69: case 0xdd83: case 0xdd9d: case 0xddb7: load_dp_y16(c, 0x20); break;
    case 0xdd6b: case 0xdd85: load_a_abs_y8(c, 0x002e); break;
    case 0xdd6e: implied(c); c->c = false; break;
    case 0xdd6f: adc_abs8(c, 0x14c9); break;
    case 0xdd72: case 0xdd8c: store_a_abs_y8(c, 0x002e); break;
    case 0xdd75: case 0xdd8f: case 0xdda9: case 0xddc3: sep_rep_native(c, 0x20, false); break;
    case 0xdd77: case 0xdd91: load_a_abs_y16(c, 0x0024); break;
    case 0xdd7a: adc_abs16(c, 0x14ca); break;
    case 0xdd7d: case 0xdd97: store_a_abs_y16(c, 0x0024); break;
    case 0xdd80: case 0xdd9a: case 0xddb4: case 0xddce: sep_rep_native(c, 0x20, true); break;
    case 0xdd82: case 0xdd9c: case 0xddb6: case 0xddd0: return_short(c); break;
    case 0xdd88: implied(c); c->c = true; break;
    case 0xdd89: sbc_abs8(c, 0x14c9); break;
    case 0xdd94: sbc_abs16(c, 0x14ca); break;
    case 0xdd9f: case 0xddb9: load_a_abs_y8(c, 0x002f); break;
    case 0xdda2: implied(c); c->c = false; break;
    case 0xdda3: adc_abs8(c, 0x14c9); break;
    case 0xdda6: case 0xddc0: store_a_abs_y8(c, 0x002f); break;
    case 0xddab: case 0xddc5: load_a_abs_y16(c, 0x0026); break;
    case 0xddae: adc_abs16(c, 0x14ca); break;
    case 0xddb1: case 0xddcb: store_a_abs_y16(c, 0x0026); break;
    case 0xddbc: implied(c); c->c = true; break;
    case 0xddbd: sbc_abs8(c, 0x14c9); break;
    case 0xddc8: sbc_abs16(c, 0x14ca); break;
  }
  return true;
}

/* $05:DFF3–$E017: scale helper — STX DP+$00; $14C8→DP+$02; JSL $0086FC;
 * fold product into $14CB/$14C9; RTS. Callee of $D743/$D6xx. */
static bool actor_scale_dff3_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xe010 || pc == 0xe012);
  const bool sep_entry = (pc == 0xe015);
  const bool rep_entry = (pc == 0xe00e);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xdff3: case 0xdff5: case 0xdff8: case 0xdffa: case 0xdffe: case 0xe000:
    case 0xe003: case 0xe004: case 0xe006: case 0xe008: case 0xe009: case 0xe00b:
    case 0xe00e: case 0xe010: case 0xe012: case 0xe015: case 0xe017: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xdff3: stx_dp16(c, 0x00); break;
    case 0xdff5: video_load(c, 0x14c8, false); break;
    case 0xdff8: store_dp(c, 0x02, true); break;
    case 0xdffa: jsl_to(c, 0x86fc); break;
    case 0xdffe: load_dp8(c, 0x02); break;
    case 0xe000: index_constant(c, false, 0x0003); break;
    case 0xe003: lsr_a8(c); break;
    case 0xe004: ror_dp8(c, 0x01); break;
    case 0xe006: ror_dp8(c, 0x00); break;
    case 0xe008: implied(c); c->x--; zn(c, c->x, false); break;
    case 0xe009: branch_to(c, !c->n, 0xe003); break;
    case 0xe00b: store_register8(c, 0x14cb); break;
    case 0xe00e: sep_rep_native(c, 0x20, false); break;
    case 0xe010: load_dp16_to_a(c, 0x00); break;
    case 0xe012: store_a_abs16(c, 0x14c9); break;
    case 0xe015: sep_rep_native(c, 0x20, true); break;
    case 0xe017: return_short(c); break;
  }
  return true;
}

/* $05:F548–$F54E: ORA #$02 into DP+$B0; RTS. Callee of $F410. */
static bool actor_ora_f548_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xf548: case 0xf54a: case 0xf54c: case 0xf54e: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf548: load_dp8(c, 0xb0); break;
    case 0xf54a: ora_constant8(c, 0x02); break;
    case 0xf54c: store_dp(c, 0xb0, true); break;
    case 0xf54e: return_short(c); break;
  }
  return true;
}


/* $05:D7F4–$D81D: ($CAFA,6) + siblings $D802/$D817 — face ORA #$80 / CMP paths → JMP $CA8D. */
static bool actor_op_d7f4_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xd7f4: case 0xd7f6: case 0xd7f8: case 0xd7fa: case 0xd7fd: case 0xd7ff:
    case 0xd802: case 0xd804: case 0xd807: case 0xd809: case 0xd80b: case 0xd80d:
    case 0xd80f: case 0xd812: case 0xd814: case 0xd817: case 0xd819: case 0xd81c:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xd7f4: case 0xd802: case 0xd817: load_dp_y16(c, 0x20); break;
    case 0xd7f6: load_dp8(c, 0x04); break;
    case 0xd7f8: case 0xd80d: ora_constant8(c, 0x80); break;
    case 0xd7fa: case 0xd80f: store_a_abs_y8(c, 0x0021); break;
    case 0xd7fd: byte_constant(c, 0x01); break;
    case 0xd7ff: case 0xd814: jmp_abs_native(c, 0xca8d); break;
    case 0xd804: load_a_abs_y8(c, 0x0001); break;
    case 0xd807: cmp_dp8(c, 0x04); break;
    case 0xd809: branch_to(c, !c->z, 0xd812); break;
    case 0xd80b: load_dp8(c, 0x05); break;
    case 0xd812: byte_constant(c, 0x02); break;
    case 0xd819: load_a_abs_y8(c, 0x0034); break;
    case 0xd81c: branch_to(c, true, 0xd807); break;
  }
  return true;
}

/* $05:D81E–$D848: ($CAFA,8) — seed $002C/$002D/$0030,Y; optional $148C; JMP $DD51. */
static bool actor_op_d81e_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xd81e: case 0xd820: case 0xd822: case 0xd824: case 0xd827: case 0xd829:
    case 0xd82c: case 0xd82f: case 0xd831: case 0xd833: case 0xd835: case 0xd837:
    case 0xd839: case 0xd83b: case 0xd83e: case 0xd841: case 0xd844: case 0xd846:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xd81e: load_dp_y16(c, 0x20); break;
    case 0xd820: load_dp8(c, 0x04); break;
    case 0xd822: ora_constant8(c, 0x80); break;
    case 0xd824: store_a_abs_y8(c, 0x002c); break;
    case 0xd827: byte_constant(c, 0x00); break;
    case 0xd829: store_a_abs_y8(c, 0x002d); break;
    case 0xd82c: store_a_abs_y8(c, 0x0030); break;
    case 0xd82f: load_dp8(c, 0xb7); break;
    case 0xd831: cmp_constant8(c, 0x2f); break;
    case 0xd833: branch_to(c, c->z, 0xd839); break;
    case 0xd835: cmp_constant8(c, 0x43); break;
    case 0xd837: branch_to(c, !c->z, 0xd844); break;
    case 0xd839: byte_constant(c, 0x83); break;
    case 0xd83b: store_register8(c, 0x148c); break;
    case 0xd83e: stz_abs8(c, 0x148d); break;
    case 0xd841: stz_abs8(c, 0x1490); break;
    case 0xd844: byte_constant(c, 0x01); break;
    case 0xd846: jmp_abs_native(c, 0xdd51); break;
  }
  return true;
}

/* $05:D849–$D857: ($CAFA,A) — if $0030,Y!=$FF RTS else LDA #0 / JMP $DD51. */
static bool actor_op_d849_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xd849: case 0xd84b: case 0xd84e: case 0xd850: case 0xd852: case 0xd853:
    case 0xd855: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xd849: load_dp_y16(c, 0x20); break;
    case 0xd84b: load_a_abs_y8(c, 0x0030); break;
    case 0xd84e: cmp_constant8(c, 0xff); break;
    case 0xd850: branch_to(c, c->z, 0xd853); break;
    case 0xd852: return_short(c); break;
    case 0xd853: byte_constant(c, 0x00); break;
    case 0xd855: jmp_abs_native(c, 0xdd51); break;
  }
  return true;
}

/* $05:D6D9–$D6E9 + $D6EB–$D7F3: ($CAFA,4)/(,E) plus $D6F6/$D73A/$D6D9 gap cluster —
 * scale via $DFF3, ± via $DD69/$DD83/$DD9D/$DDB7, multiply probe, optional $D566
 * siblings, face/script tails → $DD51/$CA8D. */
static bool actor_op_d743_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xd750 || pc == 0xd751 || pc == 0xd752 || pc == 0xd753 ||
                    pc == 0xd754 || pc == 0xd772 || pc == 0xd775 ||
                    pc == 0xd798 || pc == 0xd79b || pc == 0xd79d || pc == 0xd7a0 ||
                    pc == 0xd7a1 || pc == 0xd7a4 ||
                    pc == 0xd7bb || pc == 0xd7be || pc == 0xd7c0 || pc == 0xd7c1 ||
                    pc == 0xd7c2 || pc == 0xd7c3 ||
                    pc == 0xd718 || pc == 0xd71b || pc == 0xd71e || pc == 0xd721 ||
                    pc == 0xd722 || pc == 0xd725);
  const bool sep_entry = (pc == 0xd755 || pc == 0xd777 || pc == 0xd7a7 || pc == 0xd7c6 ||
                          pc == 0xd728);
  const bool rep_entry = (pc == 0xd74e || pc == 0xd770 || pc == 0xd796 || pc == 0xd7b9 ||
                          pc == 0xd716);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xd6d9: case 0xd6dc: case 0xd6df:
    case 0xd6e2: case 0xd6e4: case 0xd6e7: case 0xd6e9:
    case 0xd6eb: case 0xd6ee: case 0xd6f1: case 0xd6f4: case 0xd6f6: case 0xd6f9:
    case 0xd6fa: case 0xd6fc: case 0xd6ff: case 0xd702: case 0xd705: case 0xd707:
    case 0xd70a: case 0xd70c: case 0xd70f: case 0xd711: case 0xd714: case 0xd716:
    case 0xd718: case 0xd71b: case 0xd71e: case 0xd721: case 0xd722: case 0xd725:
    case 0xd728: case 0xd72a: case 0xd72b: case 0xd72e: case 0xd731: case 0xd734:
    case 0xd736: case 0xd738: case 0xd73a: case 0xd73d: case 0xd740:
    case 0xd743: case 0xd745: case 0xd748: case 0xd749: case 0xd74b: case 0xd74c:
    case 0xd74e: case 0xd750: case 0xd751: case 0xd752: case 0xd753: case 0xd754:
    case 0xd755: case 0xd757: case 0xd75a: case 0xd75b: case 0xd75d: case 0xd75f:
    case 0xd762: case 0xd764: case 0xd767: case 0xd76a: case 0xd76b: case 0xd76e:
    case 0xd770: case 0xd772: case 0xd775: case 0xd777: case 0xd779: case 0xd77d:
    case 0xd77f: case 0xd781: case 0xd783: case 0xd785: case 0xd787: case 0xd788:
    case 0xd78a: case 0xd78d: case 0xd790: case 0xd792: case 0xd793: case 0xd795:
    case 0xd796: case 0xd798: case 0xd79b: case 0xd79d: case 0xd7a0: case 0xd7a1:
    case 0xd7a4: case 0xd7a7: case 0xd7a9: case 0xd7aa: case 0xd7ac: case 0xd7af:
    case 0xd7b2: case 0xd7b4: case 0xd7b5: case 0xd7b6: case 0xd7b8: case 0xd7b9:
    case 0xd7bb: case 0xd7be: case 0xd7c0: case 0xd7c1: case 0xd7c2: case 0xd7c3:
    case 0xd7c6: case 0xd7c8: case 0xd7c9: case 0xd7cb: case 0xd7ce: case 0xd7d0:
    case 0xd7d2: case 0xd7d5: case 0xd7d8: case 0xd7da: case 0xd7dd: case 0xd7e0:
    case 0xd7e2: case 0xd7e5: case 0xd7e7: case 0xd7e9: case 0xd7ec: case 0xd7ef:
    case 0xd7f1: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xd6d9: case 0xd6eb: case 0xd6fc: case 0xd72b: case 0xd73a:
      index_constant(c, false, 0x0100); break;
    case 0xd6dc: case 0xd6ee: case 0xd6ff: case 0xd72e: case 0xd73d:
    case 0xd757: jsr_abs_native(c, 0xdff3); break;
    case 0xd6df: case 0xd731: case 0xd740: jsr_abs_native(c, 0xddb7); break;
    case 0xd6e2: case 0xd743: case 0xd714: load_dp_y16(c, 0x20); break;
    case 0xd6e4: case 0xd745: load_a_abs_y8(c, 0x0032); break;
    case 0xd6e7: eor_constant8(c, 0x40); break;
    case 0xd6e9: branch_to(c, true, 0xd748); break;
    case 0xd6f1: case 0xd702: jsr_abs_native(c, 0xdd9d); break;
    case 0xd6f4: branch_to(c, true, 0xd743); break;
    case 0xd6f6: video_load(c, 0x17d2, false); break;
    case 0xd6f9: lsr_a8(c); break;
    case 0xd6fa: branch_to(c, c->c, 0xd73a); break;
    case 0xd705: byte_constant(c, 0x70); break;
    case 0xd707: cmp_abs_y8(c, 0x0026); break;
    case 0xd70a: branch_to(c, c->c, 0xd743); break;
    case 0xd70c: store_a_abs_y8(c, 0x0026); break;
    case 0xd70f: branch_to(c, true, 0xd743); break;
    case 0xd711: jsr_abs_native(c, 0xd73a); break;
    case 0xd716: sep_rep_native(c, 0x20, false); break;
    case 0xd718: load_a_abs_y16(c, 0x0024); break;
    case 0xd71b: store_a_abs16(c, 0x1484); break;
    case 0xd71e: lda16_constant(c, 0x00be); break;
    case 0xd721: implied(c); c->c = true; break;
    case 0xd722: sbc_abs_y16(c, 0x0026); break;
    case 0xd725: store_a_abs16(c, 0x1486); break;
    case 0xd728: sep_rep_native(c, 0x20, true); break;
    case 0xd72a: return_short(c); break;
    case 0xd734: load_dp_y16(c, 0x20); break;
    case 0xd736: byte_constant(c, 0x00); break;
    case 0xd738: branch_to(c, true, 0xd748); break;
    case 0xd748: pha8(c); break;
    case 0xd749: byte_constant(c, 0x00); break;
    case 0xd74b: xba_native(c); break;
    case 0xd74c: load_dp8(c, 0x04); break;
    case 0xd74e: case 0xd770: case 0xd796: case 0xd7b9: sep_rep_native(c, 0x20, false); break;
    case 0xd750: case 0xd751: case 0xd752: case 0xd753: asl_a16(c); break;
    case 0xd754: tax16(c); break;
    case 0xd755: case 0xd777: case 0xd7a7: case 0xd7c6: sep_rep_native(c, 0x20, true); break;
    case 0xd75a: pla8(c); break;
    case 0xd75b: and_constant8(c, 0x40); break;
    case 0xd75d: branch_to(c, !c->z, 0xd764); break;
    case 0xd75f: jsr_abs_native(c, 0xdd69); break;
    case 0xd762: branch_to(c, true, 0xd767); break;
    case 0xd764: jsr_abs_native(c, 0xdd83); break;
    case 0xd767: load_a_abs_y8(c, 0x002b); break;
    case 0xd76a: inc_a8(c); break;
    case 0xd76b: store_a_abs_y8(c, 0x002b); break;
    case 0xd76e: store_dp(c, 0x02, true); break;
    case 0xd772: video_load(c, 0x14c9, false); break; /* M16 LDA $14C9 */
    case 0xd775: store_dp(c, 0x00, false); break;
    case 0xd779: jsl_to(c, 0x86fc); break;
    case 0xd77d: load_dp8(c, 0x02); break;
    case 0xd77f: branch_to(c, !c->z, 0xd788); break;
    case 0xd781: load_dp8(c, 0x01); break;
    case 0xd783: cmp_dp8(c, 0x05); break;
    case 0xd785: branch_to(c, c->c, 0xd788); break;
    case 0xd787: case 0xd7a9: case 0xd7c8: return_short(c); break;
    case 0xd788: byte_constant(c, 0x02); break;
    case 0xd78a: case 0xd7ac: case 0xd7cb: jmp_abs_native(c, 0xdd51); break;
    case 0xd78d: case 0xd7af: jsr_abs_native(c, 0xd566); break;
    case 0xd790: branch_to(c, c->c, 0xd7aa); break;
    case 0xd792: lsr_a8(c); break;
    case 0xd793: branch_to(c, !c->c, 0xd7a9); break;
    case 0xd795: lsr_a8(c); break;
    case 0xd798: load_a_abs_y16(c, 0x0024); break;
    case 0xd79b: branch_to(c, !c->c, 0xd7a1); break;
    case 0xd79d: sbc16_constant(c, 0x0004); break;
    case 0xd7a0: implied(c); c->c = false; break;
    case 0xd7a1: adc16_constant(c, 0x0002); break;
    case 0xd7a4: store_a_abs_y16(c, 0x0024); break;
    case 0xd7aa: case 0xd7c9: byte_constant(c, 0x01); break;
    case 0xd7b2: branch_to(c, c->c, 0xd7c9); break;
    case 0xd7b4: case 0xd7b5: case 0xd7b8: lsr_a8(c); break;
    case 0xd7b6: branch_to(c, !c->c, 0xd7c8); break;
    case 0xd7bb: load_a_abs_y16(c, 0x0026); break;
    case 0xd7be: branch_to(c, !c->c, 0xd7c2); break;
    case 0xd7c0: case 0xd7c1: inc_a16(c); break;
    case 0xd7c2: dec_a16(c); break;
    case 0xd7c3: store_a_abs_y16(c, 0x0026); break;
    case 0xd7ce: load_dp_y16(c, 0x20); break;
    case 0xd7d0: load_dp8(c, 0x04); break;
    case 0xd7d2: store_a_abs_y8(c, 0x0020); break;
    case 0xd7d5: load_a_abs_y8(c, 0x0021); break;
    case 0xd7d8: ora_constant8(c, 0x80); break;
    case 0xd7da: store_a_abs_y8(c, 0x0021); break;
    case 0xd7dd: jsr_abs_native(c, 0xc07b); break;
    case 0xd7e0: byte_constant(c, 0x01); break;
    case 0xd7e2: case 0xd7f1: jmp_abs_native(c, 0xca8d); break;
    case 0xd7e5: load_dp8(c, 0x04); break;
    case 0xd7e7: ora_constant8(c, 0x80); break;
    case 0xd7e9: store_register8(c, 0x1481); break;
    case 0xd7ec: store_register8(c, 0x14a1); break;
    case 0xd7ef: byte_constant(c, 0x01); break;
  }
  return true;
}

/* $05:F410–$F47F: ($E117,2) — $F998 clear, tile stream into $7F, $F952/$F91E/$F548, JSL $1D89D1. */
static bool actor_hdl_f410_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xf435 || pc == 0xf436 || pc == 0xf437 || pc == 0xf43a);
  const bool sep_entry = (pc == 0xf43b);
  const bool rep_entry = (pc == 0xf433);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xf410: case 0xf413: case 0xf416: case 0xf418: case 0xf41a: case 0xf41c:
    case 0xf41f: case 0xf422: case 0xf424: case 0xf426: case 0xf428: case 0xf42c:
    case 0xf42d: case 0xf42e: case 0xf42f: case 0xf431: case 0xf433: case 0xf435:
    case 0xf436: case 0xf437: case 0xf43a: case 0xf43b: case 0xf43d: case 0xf440:
    case 0xf442: case 0xf445: case 0xf447: case 0xf449: case 0xf44b: case 0xf44e:
    case 0xf451: case 0xf453: case 0xf456: case 0xf458: case 0xf45b: case 0xf45e:
    case 0xf461: case 0xf463: case 0xf466: case 0xf469: case 0xf46c: case 0xf46f:
    case 0xf472: case 0xf475: case 0xf478: case 0xf47b: case 0xf47f: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf410: jsr_abs_native(c, 0xf998); break;
    case 0xf413: index_constant(c, true, 0x91b5); break;
    case 0xf416: sty_dp16(c, 0x00); break;
    case 0xf418: byte_constant(c, 0x10); break;
    case 0xf41a: store_dp(c, 0x02, true); break;
    case 0xf41c: index_constant(c, true, 0x0000); break;
    case 0xf41f: index_constant(c, false, 0x028a); break;
    case 0xf422: byte_constant(c, 0x14); break;
    case 0xf424: store_dp(c, 0x03, true); break;
    case 0xf426: lda_indirect_long_y8(c, 0x00); break;
    case 0xf428: store_a_long_x8(c, 0x7f0000); break;
    case 0xf42c: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xf42d: case 0xf42e: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xf42f: dec_dp8(c, 0x03); break;
    case 0xf431: branch_to(c, !c->n, 0xf426); break;
    case 0xf433: sep_rep_native(c, 0x20, false); break;
    case 0xf435: txa16(c); break;
    case 0xf436: implied(c); c->c = false; break;
    case 0xf437: adc16_constant(c, 0x0016); break;
    case 0xf43a: tax16(c); break;
    case 0xf43b: sep_rep_native(c, 0x20, true); break;
    case 0xf43d: cpx16_constant(c, 0x0580); break;
    case 0xf440: branch_to(c, !c->c, 0xf422); break;
    case 0xf442: video_load(c, 0x01d6, false); break;
    case 0xf445: and_constant8(c, 0x60); break;
    case 0xf447: cmp_constant8(c, 0x60); break;
    case 0xf449: branch_to(c, !c->z, 0xf458); break;
    case 0xf44b: index_constant(c, false, 0x0000); break;
    case 0xf44e: load_y_abs16(c, 0x14cd); break;
    case 0xf451: branch_to(c, c->z, 0xf469); break;
    case 0xf453: index_constant(c, false, 0xff00); break;
    case 0xf456: branch_to(c, true, 0xf469); break;
    case 0xf458: index_constant(c, false, 0xffc0); break;
    case 0xf45b: jsr_abs_native(c, 0xb6b2); break;
    case 0xf45e: cpy16_constant(c, 0x1400); break;
    case 0xf461: branch_to(c, c->z, 0xf469); break;
    case 0xf463: jsr_abs_native(c, 0xf952); break;
    case 0xf466: index_constant(c, false, 0x0038); break;
    case 0xf469: stx_abs16(c, 0x17db); break;
    case 0xf46c: index_constant(c, false, 0x0000); break;
    case 0xf46f: stx_abs16(c, 0x17dd); break;
    case 0xf472: jsr_abs_native(c, 0xf91e); break;
    case 0xf475: jsr_abs_native(c, 0xf548); break;
    case 0xf478: stz_abs8(c, 0x1546); break;
    case 0xf47b: jsl_to(c, 0x89d1); break; /* bank → $1D */
    case 0xf47f: return_short(c); break;
  }
  return true;
}

/* $05:A581–$A5F6: $A52E fail/alt — type gates via $B6B2; RTS for #$36/#$37/#$5E paths;
 * other types JMP native $A8E9/$A907; BNE $A5F7 continues into native $A5F7. */
static bool actor_probe_a581_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xa581: case 0xa584: case 0xa586: case 0xa588: case 0xa58a: case 0xa58c:
    case 0xa58e: case 0xa591: case 0xa593: case 0xa595: case 0xa597: case 0xa59a:
    case 0xa59d: case 0xa5a0: case 0xa5a3: case 0xa5a5: case 0xa5a7: case 0xa5aa:
    case 0xa5ac: case 0xa5ae: case 0xa5b1: case 0xa5b4: case 0xa5b7: case 0xa5b9:
    case 0xa5bb: case 0xa5be: case 0xa5c1: case 0xa5c3: case 0xa5c5: case 0xa5c7:
    case 0xa5c9: case 0xa5cc: case 0xa5ce: case 0xa5d1: case 0xa5d4: case 0xa5d7:
    case 0xa5da: case 0xa5dd: case 0xa5df: case 0xa5e1: case 0xa5e3: case 0xa5e5:
    case 0xa5e7: case 0xa5e9: case 0xa5ec: case 0xa5ee: case 0xa5f0: case 0xa5f3:
    case 0xa5f6: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xa581: case 0xa5ae: case 0xa5c9: jsr_abs_native(c, 0xb6b2); break;
    case 0xa584: load_dp8(c, 0xb7); break;
    case 0xa586: cmp_constant8(c, 0x23); break;
    case 0xa588: branch_to(c, c->z, 0xa58e); break;
    case 0xa58a: cmp_constant8(c, 0x3e); break;
    case 0xa58c: branch_to(c, !c->z, 0xa5a3); break;
    case 0xa58e: video_load(c, 0x152c, false); break;
    case 0xa591: branch_to(c, c->z, 0xa59a); break;
    case 0xa593: store_dp(c, 0x00, true); break;
    case 0xa595: byte_constant(c, 0x01); break;
    case 0xa597: jsr_abs_native(c, 0xa90d); break;
    case 0xa59a: index_constant(c, false, 0x0080); break;
    case 0xa59d: index_constant(c, true, 0x0b0c); break;
    case 0xa5a0: jmp_abs_native(c, 0xa8e9); break;
    case 0xa5a3: cmp_constant8(c, 0x2a); break;
    case 0xa5a5: branch_to(c, !c->z, 0xa5c1); break;
    case 0xa5a7: index_constant(c, false, 0x0040); break;
    case 0xa5aa: byte_constant(c, 0x04); break;
    case 0xa5ac: store_dp(c, 0xb6, true); break;
    case 0xa5b1: load_a_abs_y8(c, 0x0001); break;
    case 0xa5b4: index_constant(c, true, 0xffff); break;
    case 0xa5b7: cmp_constant8(c, 0x37); break;
    case 0xa5b9: branch_to(c, !c->z, 0xa5be); break;
    case 0xa5bb: index_constant(c, true, 0x003e); break;
    case 0xa5be: jmp_abs_native(c, 0xa907); break;
    case 0xa5c1: cmp_constant8(c, 0x5f); break;
    case 0xa5c3: branch_to(c, !c->z, 0xa5dd); break;
    case 0xa5c5: byte_constant(c, 0x05); break;
    case 0xa5c7: store_dp(c, 0xb6, true); break;
    case 0xa5cc: byte_constant(c, 0x00); break;
    case 0xa5ce: store_a_abs_y8(c, 0x0005); break;
    case 0xa5d1: store_a_abs_y8(c, 0x0006); break;
    case 0xa5d4: index_constant(c, false, 0x0040); break;
    case 0xa5d7: index_constant(c, true, 0x4d4d); break;
    case 0xa5da: jmp_abs_native(c, 0xa8e9); break;
    case 0xa5dd: cmp_constant8(c, 0x36); break;
    case 0xa5df: branch_to(c, c->z, 0xa5f0); break;
    case 0xa5e1: cmp_constant8(c, 0x37); break;
    case 0xa5e3: branch_to(c, c->z, 0xa5f0); break;
    case 0xa5e5: cmp_constant8(c, 0x5e); break;
    case 0xa5e7: branch_to(c, !c->z, 0xa5f7); break;
    case 0xa5e9: load_a_abs_y8(c, 0x0001); break;
    case 0xa5ec: cmp_constant8(c, 0x21); break;
    case 0xa5ee: branch_to(c, !c->z, 0xa5f6); break;
    case 0xa5f0: index_constant(c, false, 0x0020); break;
    case 0xa5f3: stx_abs16(c, 0x1527); break;
    case 0xa5f6: return_short(c); break;
  }
  return true;
}

/* $05:D572–$D60E: pre-($CAFA,12) cluster — $D572/$D57D scale+move via $DFF3/$DDB7/$DD69;
 * ($CAFA sibling) $D5B9 clamp via $DD9D → $D767/$DD51; helper $D5FB wraps $D60F. */
static bool actor_op_d5b9_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xd584 || pc == 0xd585 || pc == 0xd586 || pc == 0xd587 ||
                    pc == 0xd588 || pc == 0xd59c || pc == 0xd59f ||
                    pc == 0xd5c0 || pc == 0xd5c1 || pc == 0xd5c2 || pc == 0xd5c3 ||
                    pc == 0xd5c4 || pc == 0xd5d5 || pc == 0xd5d8 || pc == 0xd5db ||
                    pc == 0xd5dd || pc == 0xd5e0 || pc == 0xd5e2 || pc == 0xd5e5 ||
                    pc == 0xd5e8 || pc == 0xd5ea ||
                    pc == 0xd600 || pc == 0xd602 || pc == 0xd605 || pc == 0xd606 ||
                    pc == 0xd609);
  const bool sep_entry = (pc == 0xd589 || pc == 0xd5a1 || pc == 0xd5c5 ||
                          pc == 0xd5ed || pc == 0xd5f1 || pc == 0xd60c);
  const bool rep_entry = (pc == 0xd582 || pc == 0xd59a || pc == 0xd5be ||
                          pc == 0xd5d3 || pc == 0xd5fe);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xd572: case 0xd575: case 0xd578: case 0xd57b: case 0xd57d: case 0xd57f:
    case 0xd580: case 0xd582: case 0xd584: case 0xd585: case 0xd586: case 0xd587:
    case 0xd588: case 0xd589: case 0xd58b: case 0xd58e: case 0xd591: case 0xd594:
    case 0xd595: case 0xd598: case 0xd59a: case 0xd59c: case 0xd59f: case 0xd5a1:
    case 0xd5a3: case 0xd5a7: case 0xd5a9: case 0xd5ab: case 0xd5ad: case 0xd5af:
    case 0xd5b1: case 0xd5b3: case 0xd5b4: case 0xd5b6: case 0xd5b9: case 0xd5bb:
    case 0xd5bc: case 0xd5be: case 0xd5c0: case 0xd5c1: case 0xd5c2: case 0xd5c3:
    case 0xd5c4: case 0xd5c5: case 0xd5c7: case 0xd5ca: case 0xd5cd: case 0xd5cf:
    case 0xd5d1: case 0xd5d3: case 0xd5d5: case 0xd5d8: case 0xd5db: case 0xd5dd:
    case 0xd5e0: case 0xd5e2: case 0xd5e5: case 0xd5e8: case 0xd5ea: case 0xd5ed:
    case 0xd5ef: case 0xd5f1: case 0xd5f3: case 0xd5f6: case 0xd5f8: case 0xd5fb:
    case 0xd5fe: case 0xd600: case 0xd602: case 0xd605: case 0xd606: case 0xd609:
    case 0xd60c: case 0xd60e: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xd572: index_constant(c, false, 0x0100); break;
    case 0xd575: case 0xd58b: case 0xd5c7: jsr_abs_native(c, 0xdff3); break;
    case 0xd578: jsr_abs_native(c, 0xddb7); break;
    case 0xd57b: load_dp_y16(c, 0x20); break;
    case 0xd57d: case 0xd5b9: byte_constant(c, 0x00); break;
    case 0xd57f: case 0xd5bb: xba_native(c); break;
    case 0xd580: case 0xd5bc: load_dp8(c, 0x04); break;
    case 0xd582: case 0xd59a: case 0xd5be: case 0xd5d3: case 0xd5fe:
      sep_rep_native(c, 0x20, false); break;
    case 0xd584: case 0xd585: case 0xd586: case 0xd587:
    case 0xd5c0: case 0xd5c1: case 0xd5c2: case 0xd5c3: asl_a16(c); break;
    case 0xd588: case 0xd5c4: tax16(c); break;
    case 0xd589: case 0xd5a1: case 0xd5c5: case 0xd5ed: case 0xd5f1: case 0xd60c:
      sep_rep_native(c, 0x20, true); break;
    case 0xd58e: jsr_abs_native(c, 0xdd69); break;
    case 0xd591: load_a_abs_y8(c, 0x002b); break;
    case 0xd594: inc_a8(c); break;
    case 0xd595: store_a_abs_y8(c, 0x002b); break;
    case 0xd598: store_dp(c, 0x02, true); break;
    case 0xd59c: video_load(c, 0x14c9, false); break;
    case 0xd59f: store_dp(c, 0x00, false); break;
    case 0xd5a3: jsl_to(c, 0x86fc); break;
    case 0xd5a7: load_dp8(c, 0x02); break;
    case 0xd5a9: branch_to(c, !c->z, 0xd5b4); break;
    case 0xd5ab: load_dp8(c, 0x05); break;
    case 0xd5ad: cmp_dp8(c, 0x01); break;
    case 0xd5af: branch_to(c, c->z, 0xd5b4); break;
    case 0xd5b1: branch_to(c, !c->c, 0xd5b4); break;
    case 0xd5b3: return_short(c); break;
    case 0xd5b4: case 0xd5f6: byte_constant(c, 0x02); break;
    case 0xd5b6: case 0xd5f8: jmp_abs_native(c, 0xdd51); break;
    case 0xd5ca: jsr_abs_native(c, 0xdd9d); break;
    case 0xd5cd: load_dp8(c, 0xb7); break;
    case 0xd5cf: cmp_constant8(c, 0x20); break;
    case 0xd5d1: branch_to(c, c->c, 0xd5f1); break;
    case 0xd5d5: video_load(c, 0x17e5, false); break;
    case 0xd5d8: cmp_constant16(c, 0x0128); break;
    case 0xd5db: branch_to(c, !c->c, 0xd5f1); break;
    case 0xd5dd: load_a_abs_y16(c, 0x0026); break;
    case 0xd5e0: branch_to(c, c->n, 0xd5f1); break;
    case 0xd5e2: lda16_constant(c, 0x0070); break;
    case 0xd5e5: cmp_abs_y16(c, 0x0026); break;
    case 0xd5e8: branch_to(c, c->c, 0xd5ed); break;
    case 0xd5ea: store_a_abs_y16(c, 0x0026); break;
    case 0xd5ef: branch_to(c, !c->c, 0xd5f6); break;
    case 0xd5f3: jmp_abs_native(c, 0xd767); break;
    case 0xd5fb: jsr_abs_native(c, 0xd60f); break;
    case 0xd600: load_dp_y16(c, 0x20); break;
    case 0xd602: load_a_abs_y16(c, 0x0026); break;
    case 0xd605: implied(c); c->c = false; break;
    case 0xd606: adc16_constant(c, 0x0008); break;
    case 0xd609: store_a_abs16(c, 0x1486); break;
    case 0xd60e: return_short(c); break;
  }
  return true;
}

/* $05:D60F–$D6D8: ($CAFA,12) $D60F + siblings $D626/$D637/$D647/$D661/$D676/$D681/
 * $D69C/$D6C4/$D6D3 — scale/move via $DFF3/$DD9D/$DDB7; merge into $D6E2/$D748/$D6DC. */
static bool actor_op_d60f_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xd616 || pc == 0xd617 || pc == 0xd618 || pc == 0xd619 ||
                    pc == 0xd61a ||
                    pc == 0xd62d || pc == 0xd62e || pc == 0xd62f || pc == 0xd630 ||
                    pc == 0xd631 ||
                    pc == 0xd63e || pc == 0xd63f || pc == 0xd640 || pc == 0xd641 ||
                    pc == 0xd642 ||
                    pc == 0xd64e || pc == 0xd64f || pc == 0xd650 || pc == 0xd651 ||
                    pc == 0xd668 || pc == 0xd669 || pc == 0xd66a || pc == 0xd66b ||
                    pc == 0xd6ab || pc == 0xd6ae || pc == 0xd6b1 || pc == 0xd6b4 ||
                    pc == 0xd6b7 || pc == 0xd6ba || pc == 0xd6bb || pc == 0xd6be);
  const bool sep_entry = (pc == 0xd61b || pc == 0xd632 || pc == 0xd643 ||
                          pc == 0xd652 || pc == 0xd66c || pc == 0xd6c1);
  const bool rep_entry = (pc == 0xd614 || pc == 0xd62b || pc == 0xd63c ||
                          pc == 0xd64c || pc == 0xd666 || pc == 0xd6a9);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xd60f: case 0xd611: case 0xd612: case 0xd614: case 0xd616: case 0xd617:
    case 0xd618: case 0xd619: case 0xd61a: case 0xd61b: case 0xd61d: case 0xd620:
    case 0xd623: case 0xd626: case 0xd628: case 0xd629: case 0xd62b: case 0xd62d:
    case 0xd62e: case 0xd62f: case 0xd630: case 0xd631: case 0xd632: case 0xd634:
    case 0xd637: case 0xd639: case 0xd63a: case 0xd63c: case 0xd63e: case 0xd63f:
    case 0xd640: case 0xd641: case 0xd642: case 0xd643: case 0xd645: case 0xd647:
    case 0xd649: case 0xd64a: case 0xd64c: case 0xd64e: case 0xd64f: case 0xd650:
    case 0xd651: case 0xd652: case 0xd654: case 0xd657: case 0xd65a: case 0xd65c:
    case 0xd65e: case 0xd661: case 0xd663: case 0xd664: case 0xd666: case 0xd668:
    case 0xd669: case 0xd66a: case 0xd66b: case 0xd66c: case 0xd66e: case 0xd671:
    case 0xd674: case 0xd676: case 0xd679: case 0xd67c: case 0xd67f: case 0xd681:
    case 0xd684: case 0xd685: case 0xd687: case 0xd68a: case 0xd68d: case 0xd690:
    case 0xd692: case 0xd695: case 0xd697: case 0xd69a: case 0xd69c: case 0xd69e:
    case 0xd6a1: case 0xd6a4: case 0xd6a7: case 0xd6a9: case 0xd6ab: case 0xd6ae:
    case 0xd6b1: case 0xd6b4: case 0xd6b7: case 0xd6ba: case 0xd6bb: case 0xd6be:
    case 0xd6c1: case 0xd6c3: case 0xd6c4: case 0xd6c7: case 0xd6ca: case 0xd6cd:
    case 0xd6cf: case 0xd6d1: case 0xd6d3: case 0xd6d5: case 0xd6d7: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xd60f: case 0xd626: case 0xd637: case 0xd647: case 0xd661:
      byte_constant(c, 0x00); break;
    case 0xd611: case 0xd628: case 0xd639: case 0xd649: case 0xd663: xba_native(c); break;
    case 0xd612: case 0xd629: case 0xd63a: case 0xd64a: case 0xd664: load_dp8(c, 0x04); break;
    case 0xd614: case 0xd62b: case 0xd63c: case 0xd64c: case 0xd666: case 0xd6a9:
      sep_rep_native(c, 0x20, false); break;
    case 0xd616: case 0xd617: case 0xd618: case 0xd619:
    case 0xd62d: case 0xd62e: case 0xd62f: case 0xd630:
    case 0xd63e: case 0xd63f: case 0xd640: case 0xd641:
    case 0xd64e: case 0xd64f: case 0xd650:
    case 0xd668: case 0xd669: case 0xd66a: asl_a16(c); break;
    case 0xd61a: case 0xd631: case 0xd642: case 0xd651: case 0xd66b: tax16(c); break;
    case 0xd61b: case 0xd632: case 0xd643: case 0xd652: case 0xd66c: case 0xd6c1:
      sep_rep_native(c, 0x20, true); break;
    case 0xd61d: case 0xd654: case 0xd66e: case 0xd679: case 0xd68a: case 0xd6c7:
      jsr_abs_native(c, 0xdff3); break;
    case 0xd620: case 0xd6ca: jsr_abs_native(c, 0xddb7); break;
    case 0xd623: jmp_abs_native(c, 0xd767); break;
    case 0xd634: jmp_abs_native(c, 0xd6dc); break;
    case 0xd645: branch_to(c, true, 0xd68a); break;
    case 0xd657: case 0xd671: case 0xd67c: case 0xd68d: jsr_abs_native(c, 0xdd9d); break;
    case 0xd65a: case 0xd69c: case 0xd6a7: case 0xd6cd: case 0xd6d3: load_dp_y16(c, 0x20); break;
    case 0xd65c: case 0xd6cf: byte_constant(c, 0x40); break;
    case 0xd65e: jmp_abs_native(c, 0xd748); break;
    case 0xd674: case 0xd67f: case 0xd69a: branch_to(c, true, 0xd6e2); break;
    case 0xd676: case 0xd687: case 0xd6c4: index_constant(c, false, 0x0100); break;
    case 0xd681: video_load(c, 0x17d2, false); break;
    case 0xd684: lsr_a8(c); break;
    case 0xd685: branch_to(c, c->c, 0xd6d9); break;
    case 0xd690: byte_constant(c, 0x70); break;
    case 0xd692: cmp_abs_y8(c, 0x0026); break;
    case 0xd695: branch_to(c, c->c, 0xd6e2); break;
    case 0xd697: store_a_abs_y8(c, 0x0026); break;
    case 0xd69e: video_load(c, 0x1486, false); break;
    case 0xd6a1: store_a_abs_y8(c, 0x0026); break;
    case 0xd6a4: jsr_abs_native(c, 0xd6d9); break;
    case 0xd6ab: load_a_abs_y16(c, 0x0024); break;
    case 0xd6ae: store_a_abs16(c, 0x1484); break;
    case 0xd6b1: load_a_abs_y16(c, 0x0026); break;
    case 0xd6b4: store_a_abs16(c, 0x1486); break;
    case 0xd6b7: lda16_constant(c, 0x00be); break;
    case 0xd6ba: implied(c); c->c = true; break;
    case 0xd6bb: sbc_abs_y16(c, 0x0026); break;
    case 0xd6be: store_a_abs_y16(c, 0x0026); break;
    case 0xd6c3: return_short(c); break;
    case 0xd6d1: branch_to(c, true, 0xd748); break;
    case 0xd6d5: byte_constant(c, 0x00); break;
    case 0xd6d7: branch_to(c, true, 0xd748); break;
  }
  return true;
}

/* $05:D858–$D8D8: ($CAFA,10) $D873 + sibling $D858; shared $D88A stream pick → $CA8D/$CA90;
 * helper $D8A2 clears slot fields; ($CAFA,14) $D8B3 face-slot seed → JMP $CA8D. */
static bool actor_op_d873_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xd88c || pc == 0xd88d || pc == 0xd88f);
  const bool sep_entry = (pc == 0xd890);
  const bool rep_entry = (pc == 0xd88a);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xd858: case 0xd85a: case 0xd85c: case 0xd85d: case 0xd860: case 0xd862:
    case 0xd865: case 0xd867: case 0xd869: case 0xd86b: case 0xd86d: case 0xd86f:
    case 0xd871: case 0xd873: case 0xd875: case 0xd877: case 0xd878: case 0xd87b:
    case 0xd87d: case 0xd880: case 0xd882: case 0xd884: case 0xd886: case 0xd888:
    case 0xd88a: case 0xd88c: case 0xd88d: case 0xd88f: case 0xd890: case 0xd892:
    case 0xd894: case 0xd896: case 0xd897: case 0xd89a: case 0xd89c: case 0xd89f:
    case 0xd8a2: case 0xd8a4: case 0xd8a7: case 0xd8a9: case 0xd8ac: case 0xd8af:
    case 0xd8b2: case 0xd8b3: case 0xd8b5: case 0xd8b7: case 0xd8b9: case 0xd8bb:
    case 0xd8bd: case 0xd8c0: case 0xd8c2: case 0xd8c4: case 0xd8c7: case 0xd8c9:
    case 0xd8cc: case 0xd8ce: case 0xd8d0: case 0xd8d3: case 0xd8d5: case 0xd8d8:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xd858: case 0xd873: sty_dp16(c, 0x06); break;
    case 0xd85a: case 0xd875: case 0xd8a7: byte_constant(c, 0x00); break;
    case 0xd85c: case 0xd877: xba_native(c); break;
    case 0xd85d: case 0xd878: video_load(c, 0x14cc, false); break;
    case 0xd860: branch_to(c, c->z, 0xd88a); break;
    case 0xd862: case 0xd87d: video_load(c, 0x14cd, false); break;
    case 0xd865: branch_to(c, !c->z, 0xd86b); break;
    case 0xd867: byte_constant(c, 0x04); break;
    case 0xd869: case 0xd871: branch_to(c, true, 0xd88a); break;
    case 0xd86b: case 0xd884: cmp_constant8(c, 0x04); break;
    case 0xd86d: branch_to(c, !c->c, 0xd88a); break;
    case 0xd86f: case 0xd888: byte_constant(c, 0x03); break;
    case 0xd87b: branch_to(c, c->z, 0xd88a); break;
    case 0xd880: branch_to(c, !c->z, 0xd884); break;
    case 0xd882: byte_constant(c, 0x01); break;
    case 0xd886: branch_to(c, !c->c, 0xd88a); break;
    case 0xd88a: sep_rep_native(c, 0x20, false); break;
    case 0xd88c: implied(c); c->c = false; break;
    case 0xd88d: adc_dp16(c, 0x06); break;
    case 0xd88f: tay16(c); break;
    case 0xd890: sep_rep_native(c, 0x20, true); break;
    case 0xd892: lda_indirect_long_y8(c, 0x00); break;
    case 0xd894: branch_to(c, c->z, 0xd89a); break;
    case 0xd896: dec_a8(c); break;
    case 0xd897: case 0xd8d8: jmp_abs_native(c, 0xca8d); break;
    case 0xd89a: byte_constant(c, 0x01); break;
    case 0xd89c: jsr_abs_native(c, 0xd8a2); break;
    case 0xd89f: jmp_abs_native(c, 0xca90); break;
    case 0xd8a2: case 0xd8b3: load_dp_y16(c, 0x20); break;
    case 0xd8a4: store_a_abs_y8(c, 0x0033); break;
    case 0xd8a9: store_a_abs_y8(c, 0x0035); break;
    case 0xd8ac: store_a_abs_y8(c, 0x0036); break;
    case 0xd8af: store_a_abs_y8(c, 0x002b); break;
    case 0xd8b2: return_short(c); break;
    case 0xd8b5: load_dp8(c, 0x04); break;
    case 0xd8b7: branch_to(c, c->z, 0xd8d3); break;
    case 0xd8b9: cmp_constant8(c, 0x01); break;
    case 0xd8bb: branch_to(c, c->z, 0xd8c9); break;
    case 0xd8bd: index_constant(c, true, 0x1460); break;
    case 0xd8c0: cmp_constant8(c, 0x02); break;
    case 0xd8c2: branch_to(c, c->z, 0xd8d3); break;
    case 0xd8c4: index_constant(c, true, 0x1480); break;
    case 0xd8c7: branch_to(c, true, 0xd8d3); break;
    case 0xd8c9: index_constant(c, true, 0x1400); break;
    case 0xd8cc: cpy_dp16(c, 0x20); break;
    case 0xd8ce: branch_to(c, !c->z, 0xd8d3); break;
    case 0xd8d0: index_constant(c, true, 0x1440); break;
    case 0xd8d3: byte_constant(c, 0x01); break;
    case 0xd8d5: store_a_abs_y8(c, 0x0027); break;
  }
  return true;
}

/* ==== Batch: A581 tails, E117 F480 leaves, CAFA D8DB+, 1D89D1 ==== */


/* $05:B11D–$B123: alt entry into $B124 — $14C2&1 without invert, BRA $B12B. */
static void and_abs16(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c);
  uint32_t low = ((uint32_t)c->db << 16) | address;
  uint16_t value = c->read(c->mem, low); sample_interrupt(c);
  value |= (uint16_t)c->read(c->mem, (low + 1) & 0xffffffu) << 8;
  c->a &= value; zn(c, c->a, false);
}
static void ora_abs16(Cpu *c, uint16_t address) {
  (void)fetch(c); (void)fetch(c);
  uint32_t low = ((uint32_t)c->db << 16) | address;
  uint16_t value = c->read(c->mem, low); sample_interrupt(c);
  value |= (uint16_t)c->read(c->mem, (low + 1) & 0xffffffu) << 8;
  c->a |= value; zn(c, c->a, false);
}
static void ldy_dp16(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  uint16_t value = read16(c, address);
  c->y = value; zn(c, value, false);
}
static bool actor_seed_b11d_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xb11d: case 0xb120: case 0xb122: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb11d: video_load(c, 0x14c2, false); break;
    case 0xb120: and_constant8(c, 0x01); break;
    case 0xb122: branch_to(c, true, 0xb12b); break;
  }
  return true;
}

/* $05:A93D–$A960: face/slot index from $14C2/$14C3/$14C5 → X; RTS. */
static bool actor_index_a93d_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = ((pc == 0xa954 || pc == 0xa955 || pc == 0xa956 || pc == 0xa957) || (pc == 0xa958 || pc == 0xa959 || pc == 0xa95a || pc == 0xa95b) || (pc == 0xa95d));
  const bool sep_entry = (pc == 0xa95e);
  const bool rep_entry = (pc == 0xa952);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xa93d: case 0xa940: case 0xa943: case 0xa946: case 0xa948: case 0xa94a:
    case 0xa94d: case 0xa950: case 0xa952: case 0xa954: case 0xa955: case 0xa956:
    case 0xa957: case 0xa958: case 0xa959: case 0xa95a: case 0xa95b: case 0xa95d:
    case 0xa95e: case 0xa960: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xa93d: index_constant(c, false, 0x0000); break;
    case 0xa940: load_y_abs16(c, 0x14c3); break;
    case 0xa943: video_load(c, 0x14c2, false); break;
    case 0xa946: and_constant8(c, 0x01); break;
    case 0xa948: branch_to(c, !c->z, 0xa950); break;
    case 0xa94a: index_constant(c, false, 0x0009); break;
    case 0xa94d: load_y_abs16(c, 0x14c5); break;
    case 0xa950: stx_dp16(c, 0x10); break;
    case 0xa952: sep_rep_native(c, 0x20, false); break;
    case 0xa954: tya16(c); break;
    case 0xa955: case 0xa956: case 0xa957: case 0xa958: case 0xa959: implied(c); c->c = (c->a & 1u) != 0; c->a >>= 1; zn(c, c->a, false); break;
    case 0xa95a: implied(c); c->c = false; break;
    case 0xa95b: adc_dp16(c, 0x10); break;
    case 0xa95d: tax16(c); break;
    case 0xa95e: sep_rep_native(c, 0x20, true); break;
    case 0xa960: return_short(c); break;
  }
  return true;
}

/* $05:A90D–$A93C: mask bit into $152D,X / $0019,Y via $A93D/$B6C0; RTS. */
static bool actor_mask_a90d_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xa90d: case 0xa90f: case 0xa912: case 0xa915: case 0xa917: case 0xa919:
    case 0xa91c: case 0xa91e: case 0xa920: case 0xa923: case 0xa925: case 0xa928:
    case 0xa92b: case 0xa92d: case 0xa92e: case 0xa930: case 0xa932: case 0xa934:
    case 0xa937: case 0xa939: case 0xa93c: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xa90d: store_dp(c, 0x01, true); break;
    case 0xa90f: jsr_abs_native(c, 0xa93d); break;
    case 0xa912: load_a_abs_x8(c, 0x152d); break;
    case 0xa915: and_dp8(c, 0x01); break;
    case 0xa917: branch_to(c, !c->z, 0xa925); break;
    case 0xa919: jsr_abs_native(c, 0xb6c0); break;
    case 0xa91c: load_dp8(c, 0x00); break;
    case 0xa91e: ora_constant8(c, 0x80); break;
    case 0xa920: store_a_abs_y8(c, 0x0019); break;
    case 0xa923: branch_to(c, true, 0xa937); break;
    case 0xa925: jsr_abs_native(c, 0xb6c0); break;
    case 0xa928: load_a_abs_y8(c, 0x0019); break;
    case 0xa92b: ora_constant8(c, 0x80); break;
    case 0xa92d: implied(c); c->c = false; break;
    case 0xa92e: adc8_dp(c, 0x00); break;
    case 0xa930: branch_to(c, !c->c, 0xa934); break;
    case 0xa932: byte_constant(c, 0xff); break;
    case 0xa934: store_a_abs_y8(c, 0x0019); break;
    case 0xa937: load_dp8(c, 0x01); break;
    case 0xa939: store_a_abs_x8(c, 0x152d); break;
    case 0xa93c: return_short(c); break;
  }
  return true;
}

/* $05:A8E9–$A90C: Y face-shrink via $14CC/$14CD; STX $1527; PLA×2; RTS. */
static bool actor_exit_a8e9_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = ((pc == 0xa8f6 || pc == 0xa8f7 || pc == 0xa8f9 || pc == 0xa8fa) || (pc == 0xa8fb || pc == 0xa8fc || pc == 0xa8fd || pc == 0xa8fe) || (pc == 0xa8ff || pc == 0xa900 || pc == 0xa901 || pc == 0xa904));
  const bool sep_entry = (pc == 0xa905);
  const bool rep_entry = (pc == 0xa8f4);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xa8e9: case 0xa8ea: case 0xa8ed: case 0xa8ef: case 0xa8f2: case 0xa8f4:
    case 0xa8f6: case 0xa8f7: case 0xa8f9: case 0xa8fa: case 0xa8fb: case 0xa8fc:
    case 0xa8fd: case 0xa8fe: case 0xa8ff: case 0xa900: case 0xa901: case 0xa904:
    case 0xa905: case 0xa907: case 0xa90a: case 0xa90b: case 0xa90c: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xa8e9: implied(c); c->c = false; break;
    case 0xa8ea: video_load(c, 0x14cc, false); break;
    case 0xa8ed: branch_to(c, c->z, 0xa8f4); break;
    case 0xa8ef: video_load(c, 0x14cd, false); break;
    case 0xa8f2: cmp_constant8(c, 0x02); break;
    case 0xa8f4: sep_rep_native(c, 0x20, false); break;
    case 0xa8f6: tya16(c); break;
    case 0xa8f7: branch_to(c, !c->c, 0xa901); break;
    case 0xa8f9: case 0xa8fa: case 0xa8fb: case 0xa8fc: case 0xa8fd: case 0xa8fe: case 0xa8ff: case 0xa900: implied(c); c->c = (c->a & 1u) != 0; c->a >>= 1; zn(c, c->a, false); break;
    case 0xa901: and_constant16(c, 0x00ff); break;
    case 0xa904: tay16(c); break;
    case 0xa905: sep_rep_native(c, 0x20, true); break;
    case 0xa907: stx_abs16(c, 0x1527); break;
    case 0xa90a: case 0xa90b: pla8(c); break;
    case 0xa90c: return_short(c); break;
  }
  return true;
}

/* $05:F85E–$F885: clear scroll pair by $1541; STZ $1546; RTS. */
static bool actor_clear_f85e_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xf85e: case 0xf861: case 0xf864: case 0xf866: case 0xf868: case 0xf86a:
    case 0xf86c: case 0xf86f: case 0xf872: case 0xf874: case 0xf877: case 0xf87a:
    case 0xf87c: case 0xf87f: case 0xf882: case 0xf885: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf85e: index_constant(c, true, 0x0000); break;
    case 0xf861: video_load(c, 0x1541, false); break;
    case 0xf864: cmp_constant8(c, 0x02); break;
    case 0xf866: branch_to(c, c->z, 0xf87c); break;
    case 0xf868: cmp_constant8(c, 0x03); break;
    case 0xf86a: branch_to(c, c->z, 0xf874); break;
    case 0xf86c: store_y_abs16(c, 0x17e3); break;
    case 0xf86f: store_y_abs16(c, 0x17e5); break;
    case 0xf872: branch_to(c, true, 0xf882); break;
    case 0xf874: store_y_abs16(c, 0x17df); break;
    case 0xf877: store_y_abs16(c, 0x17e1); break;
    case 0xf87a: branch_to(c, true, 0xf882); break;
    case 0xf87c: store_y_abs16(c, 0x17db); break;
    case 0xf87f: store_y_abs16(c, 0x17dd); break;
    case 0xf882: stz_abs8(c, 0x1546); break;
    case 0xf885: return_short(c); break;
  }
  return true;
}

/* $05:F8A1–$F8B1: type gate #$54/#$55/#$3D keep A else LDA#0; RTS. */
static bool actor_type_f8a1_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xf8a1: case 0xf8a3: case 0xf8a5: case 0xf8a7: case 0xf8a9: case 0xf8ab:
    case 0xf8ad: case 0xf8af: case 0xf8b1: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf8a1: load_dp8(c, 0xb7); break;
    case 0xf8a3: cmp_constant8(c, 0x54); break;
    case 0xf8a5: branch_to(c, c->z, 0xf8b1); break;
    case 0xf8a7: cmp_constant8(c, 0x55); break;
    case 0xf8a9: branch_to(c, c->z, 0xf8b1); break;
    case 0xf8ab: cmp_constant8(c, 0x3d); break;
    case 0xf8ad: branch_to(c, c->z, 0xf8b1); break;
    case 0xf8af: byte_constant(c, 0x00); break;
    case 0xf8b1: return_short(c); break;
  }
  return true;
}

/* $05:F910–$F91D: JSR $F91E then STA $0806=#$0D80; RTS. */
static bool queue_len_f910_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xf915 || pc == 0xf918);
  const bool sep_entry = (pc == 0xf91b);
  const bool rep_entry = (pc == 0xf913);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xf910: case 0xf913: case 0xf915: case 0xf918: case 0xf91b: case 0xf91d: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf910: jsr_abs_native(c, 0xf91e); break;
    case 0xf913: sep_rep_native(c, 0x20, false); break;
    case 0xf915: lda16_constant(c, 0x0d80); break;
    case 0xf918: store_a_abs16(c, 0x0806); break;
    case 0xf91b: sep_rep_native(c, 0x20, true); break;
    case 0xf91d: return_short(c); break;
  }
  return true;
}

/* $05:F8B2–$F90F: expand [DP+$00] tiles into $7F0000,Y via [$04]; RTS. */
static bool tile_expand_f8b2_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xf8ca || pc == 0xf8f7 || pc == 0xf8f9 || pc == 0xf8ff);
  const bool sep_entry = (pc == 0xf8cc || pc == 0xf8fb);
  const bool rep_entry = (pc == 0xf8c8 || pc == 0xf8f5);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xf8b2: case 0xf8b3: case 0xf8b6: case 0xf8b8: case 0xf8ba: case 0xf8bc:
    case 0xf8be: case 0xf8bf: case 0xf8c1: case 0xf8c3: case 0xf8c4: case 0xf8c6:
    case 0xf8c8: case 0xf8ca: case 0xf8cc: case 0xf8ce: case 0xf8cf: case 0xf8d1:
    case 0xf8d3: case 0xf8d5: case 0xf8d7: case 0xf8d9: case 0xf8db: case 0xf8dd:
    case 0xf8df: case 0xf8e0: case 0xf8e1: case 0xf8e3: case 0xf8e5: case 0xf8e7:
    case 0xf8e8: case 0xf8ea: case 0xf8ec: case 0xf8ed: case 0xf8ee: case 0xf8f0:
    case 0xf8f2: case 0xf8f3: case 0xf8f5: case 0xf8f7: case 0xf8f9: case 0xf8fb:
    case 0xf8fd: case 0xf8ff: case 0xf901: case 0xf903: case 0xf905: case 0xf907:
    case 0xf909: case 0xf90a: case 0xf90c: case 0xf90e: case 0xf90f: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf8b2: phx16(c); break;
    case 0xf8b3: index_constant(c, false, 0x0000); break;
    case 0xf8b6: stx_dp16(c, 0x04); break;
    case 0xf8b8: byte_constant(c, 0x7f); break;
    case 0xf8ba: store_dp(c, 0x06, true); break;
    case 0xf8bc: lda_indirect_long8(c, 0x00); break;
    case 0xf8be: asl_a8(c); break;
    case 0xf8bf: store_dp(c, 0x08, true); break;
    case 0xf8c1: byte_constant(c, 0x40); break;
    case 0xf8c3: implied(c); c->c = true; break;
    case 0xf8c4: sbc_dp8(c, 0x08); break;
    case 0xf8c6: store_dp(c, 0x09, true); break;
    case 0xf8c8: sep_rep_native(c, 0x20, false); break;
    case 0xf8ca: inc_dp16(c, 0x00); break;
    case 0xf8cc: sep_rep_native(c, 0x20, true); break;
    case 0xf8ce: implied(c); set_a8(c, (uint8_t)c->y); break;
    case 0xf8cf: and_constant8(c, 0x3f); break;
    case 0xf8d1: cmp_dp8(c, 0x08); break;
    case 0xf8d3: branch_to(c, !c->c, 0xf8d9); break;
    case 0xf8d5: cmp_dp8(c, 0x09); break;
    case 0xf8d7: branch_to(c, !c->c, 0xf8e3); break;
    case 0xf8d9: byte_constant(c, 0x00); break;
    case 0xf8db: case 0xf8dd: sta_indirect_long_y8(c, 0x04); break;
    case 0xf8df: case 0xf8e0: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xf8e1: branch_to(c, true, 0xf909); break;
    case 0xf8e3: lda_indirect_long8(c, 0x00); break;
    case 0xf8e5: sta_indirect_long_y8(c, 0x04); break;
    case 0xf8e7: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xf8e8: load_dp8(c, 0x03); break;
    case 0xf8ea: sta_indirect_long_y8(c, 0x04); break;
    case 0xf8ec: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xf8ed: implied(c); set_a8(c, (uint8_t)c->y); break;
    case 0xf8ee: and_constant8(c, 0x1f); break;
    case 0xf8f0: branch_to(c, c->z, 0xf903); break;
    case 0xf8f2: implied(c); set_a8(c, (uint8_t)c->y); break;
    case 0xf8f3: bit_imm8(c, 0x20); break;
    case 0xf8f5: sep_rep_native(c, 0x20, false); break;
    case 0xf8f7: branch_to(c, !c->z, 0xf8ff); break;
    case 0xf8f9: inc_dp16(c, 0x00); break;
    case 0xf8fb: sep_rep_native(c, 0x20, true); break;
    case 0xf8fd: branch_to(c, true, 0xf909); break;
    case 0xf8ff: dec_dp16(c, 0x00); break;
    case 0xf901: sep_rep_native(c, 0x20, true); break;
    case 0xf903: load_dp8(c, 0x03); break;
    case 0xf905: ora_constant8(c, 0x40); break;
    case 0xf907: store_dp(c, 0x03, true); break;
    case 0xf909: implied(c); set_a8(c, (uint8_t)c->y); break;
    case 0xf90a: and_constant8(c, 0x3f); break;
    case 0xf90c: branch_to(c, !c->z, 0xf8ce); break;
    case 0xf90e: plx16(c); break;
    case 0xf90f: return_short(c); break;
  }
  return true;
}

/* $05:E089–$E0B0: A→X index $1D92B2; build ptr $05E0B1 → DP+$83/$85; JMP $96ED. */
static bool stream_ptr_e089_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = ((pc == 0xe094 || pc == 0xe096 || pc == 0xe097 || pc == 0xe098) || (pc == 0xe09a || pc == 0xe09b || pc == 0xe09d || pc == 0xe0a1));
  const bool sep_entry = (pc == 0xe0a3);
  const bool rep_entry = (pc == 0xe092);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xe089: case 0xe08a: case 0xe08c: case 0xe08d: case 0xe08e: case 0xe092:
    case 0xe094: case 0xe096: case 0xe097: case 0xe098: case 0xe09a: case 0xe09b:
    case 0xe09d: case 0xe0a1: case 0xe0a3: case 0xe0a5: case 0xe0a9: case 0xe0ab:
    case 0xe0ae: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe089: xba_native(c); break;
    case 0xe08a: byte_constant(c, 0x00); break;
    case 0xe08c: xba_native(c); break;
    case 0xe08d: tax16(c); break;
    case 0xe08e: load_a_long_x8(c, 0x1d92b2); break;
    case 0xe092: sep_rep_native(c, 0x20, false); break;
    case 0xe094: store_dp(c, 0x00, false); break;
    case 0xe096: asl_a16(c); break;
    case 0xe097: implied(c); c->c = false; break;
    case 0xe098: adc_dp16(c, 0x00); break;
    case 0xe09a: tax16(c); break;
    case 0xe09b: stx_dp16(c, 0x00); break;
    case 0xe09d: lda_long_x16(c, 0x05e0b1); break;
    case 0xe0a1: store_dp(c, 0x83, false); break;
    case 0xe0a3: sep_rep_native(c, 0x20, true); break;
    case 0xe0a5: load_a_long_x8(c, 0x05e0b3); break;
    case 0xe0a9: store_dp(c, 0x85, true); break;
    case 0xe0ab: index_constant(c, true, 0x0000); break;
    case 0xe0ae: jmp_abs_native(c, 0x96ed); break;
  }
  return true;
}

/* $1D:89D1–$89F7: copy $1D8A0D→$1570 (89D1) or sparse $1D89F8→$1570 (89DF); RTL. */
static bool timer_seed_1d89d1_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x1d || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x89d1: case 0x89d4: case 0x89d8: case 0x89db: case 0x89dc: case 0x89de:
    case 0x89df: case 0x89e2: case 0x89e5: case 0x89e9: case 0x89ec: case 0x89ed:
    case 0x89ee: case 0x89ef: case 0x89f0: case 0x89f1: case 0x89f2: case 0x89f5:
    case 0x89f7: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x89d1: index_constant(c, false, 0x0069); break;
    case 0x89d4: load_a_long_x8(c, 0x1d8a0d); break;
    case 0x89d8: store_a_abs_x8(c, 0x1570); break;
    case 0x89db: implied(c); c->x--; zn(c, c->x, false); break;
    case 0x89dc: branch_to(c, !c->n, 0x89d4); break;
    case 0x89de: return_long(c); break;
    case 0x89df: index_constant(c, true, 0x0001); break;
    case 0x89e2: index_constant(c, false, 0x0000); break;
    case 0x89e5: load_a_long_x8(c, 0x1d89f8); break;
    case 0x89e9: store_a_abs_y8(c, 0x1570); break;
    case 0x89ec: case 0x89ed: case 0x89ee: case 0x89ef: case 0x89f0: implied(c); c->y++; zn(c, c->y, false); break;
    case 0x89f1: implied(c); c->x++; zn(c, c->x, false); break;
    case 0x89f2: cpx16_constant(c, 0x0015); break;
    case 0x89f5: branch_to(c, !c->c, 0x89e5); break;
    case 0x89f7: return_long(c); break;
  }
  return true;
}

/* $05:D8DB–$DA40: ($CAFA,16..) $D8DB/$D955/$D970/$D9A6/$DA03 + siblings → $CA8D. */
static bool actor_op_d8db_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = ((pc == 0xd8f2 || pc == 0xd8f5 || pc == 0xd8f6 || pc == 0xd8f9) || (pc == 0xd8fc || pc == 0xd8ff || pc == 0xd900 || pc == 0xd903) || (pc == 0xd90f || pc == 0xd911 || pc == 0xd913 || pc == 0xd921) || (pc == 0xd923 || pc == 0xd925 || pc == 0xd957 || pc == 0xd959) || (pc == 0xd95b || pc == 0xd95e || pc == 0xd960 || pc == 0xd963) || (pc == 0xd966 || pc == 0xd972 || pc == 0xd974 || pc == 0xd977) || (pc == 0xd979 || pc == 0xd97b || pc == 0xd97d || pc == 0xd97f) || (pc == 0xd982 || pc == 0xd98e || pc == 0xd991 || pc == 0xd992) || (pc == 0xd994 || pc == 0xd996 || pc == 0xd998 || pc == 0xd99a) || (pc == 0xd99c));
  const bool sep_entry = ((pc == 0xd906 || pc == 0xd916 || pc == 0xd928 || pc == 0xd969) || (pc == 0xd985 || pc == 0xd99f));
  const bool rep_entry = ((pc == 0xd8f0 || pc == 0xd90d || pc == 0xd91f || pc == 0xd955) || (pc == 0xd970 || pc == 0xd98c));
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xd8db: case 0xd8dd: case 0xd8df: case 0xd8e2: case 0xd8e4: case 0xd8e7:
    case 0xd8e9: case 0xd8ec: case 0xd8ee: case 0xd8f0: case 0xd8f2: case 0xd8f5:
    case 0xd8f6: case 0xd8f9: case 0xd8fc: case 0xd8ff: case 0xd900: case 0xd903:
    case 0xd906: case 0xd908: case 0xd90a: case 0xd90d: case 0xd90f: case 0xd911:
    case 0xd913: case 0xd916: case 0xd918: case 0xd91a: case 0xd91d: case 0xd91f:
    case 0xd921: case 0xd923: case 0xd925: case 0xd928: case 0xd92a: case 0xd92c:
    case 0xd92f: case 0xd931: case 0xd934: case 0xd936: case 0xd938: case 0xd93b:
    case 0xd93d: case 0xd940: case 0xd943: case 0xd945: case 0xd947: case 0xd94a:
    case 0xd94c: case 0xd94f: case 0xd952: case 0xd955: case 0xd957: case 0xd959:
    case 0xd95b: case 0xd95e: case 0xd960: case 0xd963: case 0xd966: case 0xd969:
    case 0xd96b: case 0xd96d: case 0xd970: case 0xd972: case 0xd974: case 0xd977:
    case 0xd979: case 0xd97b: case 0xd97d: case 0xd97f: case 0xd982: case 0xd985:
    case 0xd987: case 0xd989: case 0xd98c: case 0xd98e: case 0xd991: case 0xd992:
    case 0xd994: case 0xd996: case 0xd998: case 0xd99a: case 0xd99c: case 0xd99f:
    case 0xd9a1: case 0xd9a3: case 0xd9a6: case 0xd9a9: case 0xd9ab: case 0xd9ad:
    case 0xd9af: case 0xd9b1: case 0xd9b3: case 0xd9b5: case 0xd9b7: case 0xd9b9:
    case 0xd9bb: case 0xd9bd: case 0xd9c0: case 0xd9c3: case 0xd9c5: case 0xd9c7:
    case 0xd9c9: case 0xd9cb: case 0xd9cd: case 0xd9d0: case 0xd9d1: case 0xd9d3:
    case 0xd9d5: case 0xd9d7: case 0xd9d9: case 0xd9db: case 0xd9dd: case 0xd9e0:
    case 0xd9e2: case 0xd9e4: case 0xd9e6: case 0xd9e9: case 0xd9ec: case 0xd9ef:
    case 0xd9f2: case 0xd9f5: case 0xd9f8: case 0xd9fb: case 0xd9fe: case 0xda01:
    case 0xda03: case 0xda06: case 0xda08: case 0xda09: case 0xda0b: case 0xda0e:
    case 0xda11: case 0xda13: case 0xda15: case 0xda17: case 0xda19: case 0xda1a:
    case 0xda1c: case 0xda1f: case 0xda21: case 0xda24: case 0xda26: case 0xda28:
    case 0xda2b: case 0xda2d: case 0xda30: case 0xda32: case 0xda34: case 0xda36:
    case 0xda39: case 0xda3b: case 0xda3d: case 0xda3f: case 0xda40: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xd8db: load_dp8(c, 0x04); break;
    case 0xd8dd: branch_to(c, !c->z, 0xd8e9); break;
    case 0xd8df: video_load(c, 0x17e6, false); break;
    case 0xd8e2: eor_constant8(c, 0x01); break;
    case 0xd8e4: store_register8(c, 0x17e6); break;
    case 0xd8e7: branch_to(c, true, 0xd908); break;
    case 0xd8e9: video_load(c, 0x14ff, false); break;
    case 0xd8ec: and_constant8(c, 0x0c); break;
    case 0xd8ee: branch_to(c, !c->z, 0xd908); break;
    case 0xd8f0: sep_rep_native(c, 0x20, false); break;
    case 0xd8f2: video_load(c, 0x17df, false); break;
    case 0xd8f5: implied(c); c->c = false; break;
    case 0xd8f6: adc16_constant(c, 0x0060); break;
    case 0xd8f9: store_a_abs16(c, 0x17df); break;
    case 0xd8fc: video_load(c, 0x17e3, false); break;
    case 0xd8ff: implied(c); c->c = false; break;
    case 0xd900: adc16_constant(c, 0x0060); break;
    case 0xd903: store_a_abs16(c, 0x17e3); break;
    case 0xd906: sep_rep_native(c, 0x20, true); break;
    case 0xd908: byte_constant(c, 0x01); break;
    case 0xd90a: jmp_abs_native(c, 0xca8d); break;
    case 0xd90d: sep_rep_native(c, 0x20, false); break;
    case 0xd90f: ldy_dp16(c, 0x20); break;
    case 0xd911: load_dp16_to_a(c, 0x04); break;
    case 0xd913: store_a_abs_y16(c, 0x0026); break;
    case 0xd916: sep_rep_native(c, 0x20, true); break;
    case 0xd918: byte_constant(c, 0x00); break;
    case 0xd91a: store_a_abs_y8(c, 0x002f); break;
    case 0xd91d: branch_to(c, true, 0xd92f); break;
    case 0xd91f: sep_rep_native(c, 0x20, false); break;
    case 0xd921: ldy_dp16(c, 0x20); break;
    case 0xd923: load_dp16_to_a(c, 0x04); break;
    case 0xd925: store_a_abs_y16(c, 0x0024); break;
    case 0xd928: sep_rep_native(c, 0x20, true); break;
    case 0xd92a: byte_constant(c, 0x00); break;
    case 0xd92c: store_a_abs_y8(c, 0x002e); break;
    case 0xd92f: byte_constant(c, 0x02); break;
    case 0xd931: jmp_abs_native(c, 0xca8d); break;
    case 0xd934: ldy_dp16(c, 0x20); break;
    case 0xd936: byte_constant(c, 0x80); break;
    case 0xd938: store_a_abs_y8(c, 0x0024); break;
    case 0xd93b: byte_constant(c, 0x00); break;
    case 0xd93d: store_a_abs_y8(c, 0x0025); break;
    case 0xd940: store_a_abs_y8(c, 0x002e); break;
    case 0xd943: ldy_dp16(c, 0x20); break;
    case 0xd945: byte_constant(c, 0x5f); break;
    case 0xd947: store_a_abs_y8(c, 0x0026); break;
    case 0xd94a: byte_constant(c, 0x00); break;
    case 0xd94c: store_a_abs_y8(c, 0x0027); break;
    case 0xd94f: store_a_abs_y8(c, 0x002f); break;
    case 0xd952: jmp_abs_native(c, 0xca8d); break;
    case 0xd955: sep_rep_native(c, 0x20, false); break;
    case 0xd957: ldy_dp16(c, 0x20); break;
    case 0xd959: load_dp16_to_a(c, 0x04); break;
    case 0xd95b: store_a_abs_y16(c, 0x0024); break;
    case 0xd95e: load_dp16_to_a(c, 0x06); break;
    case 0xd960: store_a_abs_y16(c, 0x0026); break;
    case 0xd963: lda16_constant(c, 0x0000); break;
    case 0xd966: store_a_abs_y16(c, 0x002e); break;
    case 0xd969: sep_rep_native(c, 0x20, true); break;
    case 0xd96b: byte_constant(c, 0x04); break;
    case 0xd96d: jmp_abs_native(c, 0xca8d); break;
    case 0xd970: sep_rep_native(c, 0x20, false); break;
    case 0xd972: ldy_dp16(c, 0x20); break;
    case 0xd974: cpy16_constant(c, 0x1400); break;
    case 0xd977: branch_to(c, !c->z, 0xd97d); break;
    case 0xd979: load_dp16_to_a(c, 0x04); break;
    case 0xd97b: branch_to(c, true, 0xd97f); break;
    case 0xd97d: load_dp16_to_a(c, 0x06); break;
    case 0xd97f: store_a_abs_y16(c, 0x0024); break;
    case 0xd982: store_a_abs_y16(c, 0x0037); break;
    case 0xd985: sep_rep_native(c, 0x20, true); break;
    case 0xd987: byte_constant(c, 0x04); break;
    case 0xd989: jmp_abs_native(c, 0xca8d); break;
    case 0xd98c: sep_rep_native(c, 0x20, false); break;
    case 0xd98e: video_load(c, 0x17d2, false); break;
    case 0xd991: implied(c); c->c = (c->a & 1u) != 0; c->a >>= 1; zn(c, c->a, false); break;
    case 0xd992: branch_to(c, c->c, 0xd998); break;
    case 0xd994: load_dp16_to_a(c, 0x04); break;
    case 0xd996: branch_to(c, true, 0xd99a); break;
    case 0xd998: load_dp16_to_a(c, 0x06); break;
    case 0xd99a: ldy_dp16(c, 0x20); break;
    case 0xd99c: store_a_abs_y16(c, 0x0026); break;
    case 0xd99f: sep_rep_native(c, 0x20, true); break;
    case 0xd9a1: byte_constant(c, 0x04); break;
    case 0xd9a3: jmp_abs_native(c, 0xca8d); break;
    case 0xd9a6: video_load(c, 0x01e7, false); break;
    case 0xd9a9: branch_to(c, !c->n, 0xd9bb); break;
    case 0xd9ab: load_dp8(c, 0xb7); break;
    case 0xd9ad: cmp_constant8(c, 0x20); break;
    case 0xd9af: branch_to(c, c->c, 0xd9bb); break;
    case 0xd9b1: cmp_constant8(c, 0x16); break;
    case 0xd9b3: branch_to(c, c->z, 0xd9bb); break;
    case 0xd9b5: load_dp8(c, 0x04); break;
    case 0xd9b7: and_constant8(c, 0xf3); break;
    case 0xd9b9: store_dp(c, 0x04, true); break;
    case 0xd9bb: ldx_dp16(c, 0x04); break;
    case 0xd9bd: stx_abs16(c, 0x14ff); break;
    case 0xd9c0: stz_abs8(c, 0x1501); break;
    case 0xd9c3: load_dp8(c, 0x04); break;
    case 0xd9c5: branch_to(c, c->z, 0xd9e4); break;
    case 0xd9c7: and_constant8(c, 0x0f); break;
    case 0xd9c9: cmp_constant8(c, 0x03); break;
    case 0xd9cb: branch_to(c, !c->z, 0xd9e0); break;
    case 0xd9cd: video_load(c, 0x14c2, false); break;
    case 0xd9d0: lsr_a8(c); break;
    case 0xd9d1: byte_constant(c, 0x01); break;
    case 0xd9d3: branch_to(c, c->c, 0xd9d7); break;
    case 0xd9d5: byte_constant(c, 0x02); break;
    case 0xd9d7: bit_dp8(c, 0x04); break;
    case 0xd9d9: branch_to(c, !c->n, 0xd9dd); break;
    case 0xd9db: eor_constant8(c, 0x03); break;
    case 0xd9dd: store_register8(c, 0x14ff); break;
    case 0xd9e0: byte_constant(c, 0x02); break;
    case 0xd9e2: branch_to(c, true, 0xd9e6); break;
    case 0xd9e4: byte_constant(c, 0x01); break;
    case 0xd9e6: jmp_abs_native(c, 0xca8d); break;
    case 0xd9e9: stz_abs8(c, 0x14ff); break;
    case 0xd9ec: index_constant(c, true, 0x0000); break;
    case 0xd9ef: store_y_abs16(c, 0x17db); break;
    case 0xd9f2: store_y_abs16(c, 0x17dd); break;
    case 0xd9f5: store_y_abs16(c, 0x17df); break;
    case 0xd9f8: store_y_abs16(c, 0x17e1); break;
    case 0xd9fb: store_y_abs16(c, 0x17e3); break;
    case 0xd9fe: store_y_abs16(c, 0x17e5); break;
    case 0xda01: branch_to(c, true, 0xda09); break;
    case 0xda03: video_load(c, 0x14ff, false); break;
    case 0xda06: branch_to(c, c->z, 0xda09); break;
    case 0xda08: return_short(c); break;
    case 0xda09: byte_constant(c, 0x00); break;
    case 0xda0b: jmp_abs_native(c, 0xca8d); break;
    case 0xda0e: video_load(c, 0x17e5, false); break;
    case 0xda11: cmp_dp8(c, 0x04); break;
    case 0xda13: branch_to(c, !c->c, 0xda19); break;
    case 0xda15: cmp_dp8(c, 0x05); break;
    case 0xda17: branch_to(c, !c->c, 0xda1a); break;
    case 0xda19: return_short(c); break;
    case 0xda1a: load_dp8(c, 0x04); break;
    case 0xda1c: store_register8(c, 0x17e5); break;
    case 0xda1f: byte_constant(c, 0x02); break;
    case 0xda21: jmp_abs_native(c, 0xca8d); break;
    case 0xda24: ldy_dp16(c, 0x20); break;
    case 0xda26: load_dp8(c, 0x04); break;
    case 0xda28: store_a_abs_y8(c, 0x0029); break;
    case 0xda2b: byte_constant(c, 0x01); break;
    case 0xda2d: jmp_abs_native(c, 0xca8d); break;
    case 0xda30: load_dp8(c, 0x04); break;
    case 0xda32: store_dp(c, 0xb9, true); break;
    case 0xda34: byte_constant(c, 0x01); break;
    case 0xda36: jmp_abs_native(c, 0xca8d); break;
    case 0xda39: load_dp8(c, 0xb9); break;
    case 0xda3b: cmp_dp8(c, 0x04); break;
    case 0xda3d: branch_to(c, c->z, 0xda40); break;
    case 0xda3f: return_short(c); break;
    case 0xda40: byte_constant(c, 0x01); break;
  }
  return true;
}

/* $05:F480–$F547: ($E117,4/6) phase via $1547; falls into native $F548. */
static bool actor_hdl_f480_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = ((pc == 0xf4af || pc == 0xf4b2 || pc == 0xf4b6 || pc == 0xf4b9) || (pc == 0xf4bd || pc == 0xf4be || pc == 0xf4bf || pc == 0xf4c2) || (pc == 0xf4cb || pc == 0xf4ce || pc == 0xf4d1 || pc == 0xf4d3) || (pc == 0xf4d4 || pc == 0xf4d7 || pc == 0xf4da || pc == 0xf4dd) || (pc == 0xf50b || pc == 0xf50f || pc == 0xf511 || pc == 0xf513) || (pc == 0xf524 || pc == 0xf528 || pc == 0xf52a || pc == 0xf52c));
  const bool sep_entry = (pc == 0xf4c4 || pc == 0xf4e0 || pc == 0xf515 || pc == 0xf52e);
  const bool rep_entry = (pc == 0xf4ad || pc == 0xf4c9 || pc == 0xf509 || pc == 0xf522);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xf480: case 0xf483: case 0xf485: case 0xf488: case 0xf48b: case 0xf48d:
    case 0xf48f: case 0xf492: case 0xf493: case 0xf495: case 0xf497: case 0xf499:
    case 0xf49c: case 0xf49f: case 0xf4a0: case 0xf4a2: case 0xf4a4: case 0xf4a7:
    case 0xf4aa: case 0xf4ad: case 0xf4af: case 0xf4b2: case 0xf4b6: case 0xf4b9:
    case 0xf4bd: case 0xf4be: case 0xf4bf: case 0xf4c2: case 0xf4c4: case 0xf4c6:
    case 0xf4c9: case 0xf4cb: case 0xf4ce: case 0xf4d1: case 0xf4d3: case 0xf4d4:
    case 0xf4d7: case 0xf4da: case 0xf4dd: case 0xf4e0: case 0xf4e2: case 0xf4e3:
    case 0xf4e6: case 0xf4e9: case 0xf4ec: case 0xf4ef: case 0xf4f0: case 0xf4f1:
    case 0xf4f2: case 0xf4f4: case 0xf4f6: case 0xf4f7: case 0xf4fa: case 0xf4fc:
    case 0xf4fd: case 0xf4fe: case 0xf500: case 0xf502: case 0xf505: case 0xf507:
    case 0xf509: case 0xf50b: case 0xf50f: case 0xf511: case 0xf513: case 0xf515:
    case 0xf517: case 0xf51a: case 0xf51c: case 0xf51e: case 0xf520: case 0xf522:
    case 0xf524: case 0xf528: case 0xf52a: case 0xf52c: case 0xf52e: case 0xf530:
    case 0xf531: case 0xf532: case 0xf535: case 0xf536: case 0xf537: case 0xf53a:
    case 0xf53c: case 0xf53f: case 0xf542: case 0xf545: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf480: video_load(c, 0x1547, false); break;
    case 0xf483: branch_to(c, c->z, 0xf4e3); break;
    case 0xf485: inc_abs8_native(c, 0x1547); break;
    case 0xf488: video_load(c, 0x1547, false); break;
    case 0xf48b: cmp_constant8(c, 0x03); break;
    case 0xf48d: branch_to(c, !c->c, 0xf4a0); break;
    case 0xf48f: video_load(c, 0x1546, false); break;
    case 0xf492: inc_a8(c); break;
    case 0xf493: cmp_constant8(c, 0x84); break;
    case 0xf495: branch_to(c, !c->c, 0xf499); break;
    case 0xf497: byte_constant(c, 0x00); break;
    case 0xf499: store_register8(c, 0x1546); break;
    case 0xf49c: stz_abs8(c, 0x1547); break;
    case 0xf49f: return_short(c); break;
    case 0xf4a0: cmp_constant8(c, 0x02); break;
    case 0xf4a2: branch_to(c, !c->z, 0xf4e2); break;
    case 0xf4a4: video_load(c, 0x1540, false); break;
    case 0xf4a7: jsr_abs_native(c, 0xe089); break;
    case 0xf4aa: index_constant(c, false, 0x0000); break;
    case 0xf4ad: sep_rep_native(c, 0x20, false); break;
    case 0xf4af: lda16_constant(c, 0x0000); break;
    case 0xf4b2: store_a_long_x16(c, 0x7f1000); break;
    case 0xf4b6: lda16_constant(c, 0xff00); break;
    case 0xf4b9: store_a_long_x16(c, 0x7f1020); break;
    case 0xf4bd: case 0xf4be: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xf4bf: cpx16_constant(c, 0x0010); break;
    case 0xf4c2: branch_to(c, !c->c, 0xf4af); break;
    case 0xf4c4: sep_rep_native(c, 0x20, true); break;
    case 0xf4c6: jsr_abs_native(c, 0xf91e); break;
    case 0xf4c9: sep_rep_native(c, 0x20, false); break;
    case 0xf4cb: lda16_constant(c, 0x3800); break;
    case 0xf4ce: store_a_abs16(c, 0x0801); break;
    case 0xf4d1: load_dp16_to_a(c, 0x89); break;
    case 0xf4d3: implied(c); c->c = true; break;
    case 0xf4d4: sbc16_constant(c, 0x1000); break;
    case 0xf4d7: store_a_abs16(c, 0x0806); break;
    case 0xf4da: lda16_constant(c, 0x1000); break;
    case 0xf4dd: store_a_abs16(c, 0x0803); break;
    case 0xf4e0: sep_rep_native(c, 0x20, true); break;
    case 0xf4e2: return_short(c); break;
    case 0xf4e3: jsr_abs_native(c, 0xf998); break;
    case 0xf4e6: inc_abs8_native(c, 0x1547); break;
    case 0xf4e9: index_constant(c, true, 0x0000); break;
    case 0xf4ec: index_constant(c, false, 0x0000); break;
    case 0xf4ef: phx16(c); break;
    case 0xf4f0: phy16(c); break;
    case 0xf4f1: implied(c); c->y = c->x; zn(c, c->y, false); break;
    case 0xf4f2: stz_dp8(c, 0x03); break;
    case 0xf4f4: byte_constant(c, 0x00); break;
    case 0xf4f6: xba_native(c); break;
    case 0xf4f7: video_load(c, 0x1546, false); break;
    case 0xf4fa: and_constant8(c, 0x03); break;
    case 0xf4fc: asl_a8(c); break;
    case 0xf4fd: tax16(c); break;
    case 0xf4fe: byte_constant(c, 0x10); break;
    case 0xf500: store_dp(c, 0x02, true); break;
    case 0xf502: video_load(c, 0x1540, false); break;
    case 0xf505: cmp_constant8(c, 0x03); break;
    case 0xf507: branch_to(c, c->z, 0xf522); break;
    case 0xf509: sep_rep_native(c, 0x20, false); break;
    case 0xf50b: lda_long_x16(c, 0x108f8c); break;
    case 0xf50f: store_dp(c, 0x00, false); break;
    case 0xf511: lda_indirect_long_y16(c, 0x00); break;
    case 0xf513: store_dp(c, 0x00, false); break;
    case 0xf515: sep_rep_native(c, 0x20, true); break;
    case 0xf517: video_load(c, 0x1546, false); break;
    case 0xf51a: cmp_constant8(c, 0x83); break;
    case 0xf51c: branch_to(c, !c->z, 0xf530); break;
    case 0xf51e: inc_dp8(c, 0x03); break;
    case 0xf520: branch_to(c, true, 0xf530); break;
    case 0xf522: sep_rep_native(c, 0x20, false); break;
    case 0xf524: lda_long_x16(c, 0x108ed4); break;
    case 0xf528: store_dp(c, 0x00, false); break;
    case 0xf52a: lda_indirect_long_y16(c, 0x00); break;
    case 0xf52c: store_dp(c, 0x00, false); break;
    case 0xf52e: sep_rep_native(c, 0x20, true); break;
    case 0xf530: ply16(c); break;
    case 0xf531: plx16(c); break;
    case 0xf532: jsr_abs_native(c, 0xf8b2); break;
    case 0xf535: case 0xf536: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xf537: cpx16_constant(c, 0x002c); break;
    case 0xf53a: branch_to(c, !c->c, 0xf4ef); break;
    case 0xf53c: jsr_abs_native(c, 0xf91e); break;
    case 0xf53f: index_constant(c, true, 0x0000); break;
    case 0xf542: store_y_abs16(c, 0x17db); break;
    case 0xf545: store_y_abs16(c, 0x17dd); break;
  }
  return true;
}

/* $05:A5F7–$A8E8: continue $A581 type gates through JMP $A8E9/$A907 / RTS. */
static bool actor_probe_a5f7_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = ((pc == 0xa622 || pc == 0xa625 || pc == 0xa628 || pc == 0xa62b) || (pc == 0xa63e || pc == 0xa641 || pc == 0xa6cf || pc == 0xa6d2) || (pc == 0xa6d3 || pc == 0xa6d4 || pc == 0xa6d7 || pc == 0xa6da) || (pc == 0xa6db || pc == 0xa6de || pc == 0xa6e8 || pc == 0xa6eb) || (pc == 0xa6ec || pc == 0xa6ef || pc == 0xa727 || pc == 0xa72a) || (pc == 0xa748 || pc == 0xa74b || pc == 0xa74c || pc == 0xa74e) || (pc == 0xa7bd || pc == 0xa7c1 || pc == 0xa7c4 || pc == 0xa7c6) || (pc == 0xa7ca || pc == 0xa7cd || pc == 0xa815 || pc == 0xa818) || (pc == 0xa81b || pc == 0xa81c || pc == 0xa81d || pc == 0xa81e) || (pc == 0xa81f || pc == 0xa820 || pc == 0xa821 || pc == 0xa822) || (pc == 0xa823 || pc == 0xa826 || pc == 0xa828 || pc == 0xa82b) || (pc == 0xa82c || pc == 0xa82e || pc == 0xa830 || pc == 0xa833) || (pc == 0xa835 || pc == 0xa838));
  const bool sep_entry = ((pc == 0xa62e || pc == 0xa644 || pc == 0xa6e1 || pc == 0xa6f2) || (pc == 0xa72c || pc == 0xa751 || pc == 0xa7d0 || pc == 0xa7e2) || (pc == 0xa83b));
  const bool rep_entry = ((pc == 0xa620 || pc == 0xa63c || pc == 0xa6cd || pc == 0xa6e6) || (pc == 0xa725 || pc == 0xa746 || pc == 0xa7bb || pc == 0xa813));
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xa5f7: case 0xa5f9: case 0xa5fb: case 0xa5fe: case 0xa600: case 0xa602:
    case 0xa604: case 0xa606: case 0xa609: case 0xa60c: case 0xa60f: case 0xa612:
    case 0xa614: case 0xa616: case 0xa618: case 0xa61a: case 0xa61d: case 0xa620:
    case 0xa622: case 0xa625: case 0xa628: case 0xa62b: case 0xa62e: case 0xa630:
    case 0xa633: case 0xa636: case 0xa637: case 0xa63b: case 0xa63c: case 0xa63e:
    case 0xa641: case 0xa644: case 0xa646: case 0xa649: case 0xa64c: case 0xa64f:
    case 0xa652: case 0xa655: case 0xa658: case 0xa65a: case 0xa65c: case 0xa65e:
    case 0xa660: case 0xa662: case 0xa664: case 0xa666: case 0xa668: case 0xa66a:
    case 0xa66d: case 0xa670: case 0xa673: case 0xa676: case 0xa678: case 0xa67a:
    case 0xa67d: case 0xa680: case 0xa683: case 0xa685: case 0xa687: case 0xa68a:
    case 0xa68c: case 0xa690: case 0xa694: case 0xa696: case 0xa698: case 0xa69b:
    case 0xa69e: case 0xa6a1: case 0xa6a4: case 0xa6a6: case 0xa6a9: case 0xa6ab:
    case 0xa6ad: case 0xa6af: case 0xa6b1: case 0xa6b4: case 0xa6b7: case 0xa6ba:
    case 0xa6bc: case 0xa6be: case 0xa6c1: case 0xa6c3: case 0xa6c6: case 0xa6c8:
    case 0xa6ca: case 0xa6cd: case 0xa6cf: case 0xa6d2: case 0xa6d3: case 0xa6d4:
    case 0xa6d7: case 0xa6da: case 0xa6db: case 0xa6de: case 0xa6e1: case 0xa6e3:
    case 0xa6e6: case 0xa6e8: case 0xa6eb: case 0xa6ec: case 0xa6ef: case 0xa6f2:
    case 0xa6f4: case 0xa6f7: case 0xa6fa: case 0xa6fd: case 0xa700: case 0xa702:
    case 0xa704: case 0xa706: case 0xa707: case 0xa70a: case 0xa70b: case 0xa70c:
    case 0xa70d: case 0xa70e: case 0xa70f: case 0xa713: case 0xa716: case 0xa718:
    case 0xa71c: case 0xa71f: case 0xa722: case 0xa725: case 0xa727: case 0xa72a:
    case 0xa72c: case 0xa72e: case 0xa731: case 0xa733: case 0xa735: case 0xa737:
    case 0xa739: case 0xa73c: case 0xa73e: case 0xa740: case 0xa742: case 0xa744:
    case 0xa746: case 0xa748: case 0xa74b: case 0xa74c: case 0xa74e: case 0xa751:
    case 0xa753: case 0xa756: case 0xa758: case 0xa75b: case 0xa75d: case 0xa75f:
    case 0xa762: case 0xa765: case 0xa768: case 0xa76a: case 0xa76d: case 0xa770:
    case 0xa773: case 0xa776: case 0xa778: case 0xa77a: case 0xa77d: case 0xa780:
    case 0xa782: case 0xa785: case 0xa788: case 0xa78b: case 0xa78d: case 0xa790:
    case 0xa793: case 0xa795: case 0xa798: case 0xa79b: case 0xa79e: case 0xa7a0:
    case 0xa7a3: case 0xa7a6: case 0xa7a8: case 0xa7ab: case 0xa7ac: case 0xa7ae:
    case 0xa7b0: case 0xa7b2: case 0xa7b3: case 0xa7b6: case 0xa7b7: case 0xa7b8:
    case 0xa7b9: case 0xa7ba: case 0xa7bb: case 0xa7bd: case 0xa7c1: case 0xa7c4:
    case 0xa7c6: case 0xa7ca: case 0xa7cd: case 0xa7d0: case 0xa7d2: case 0xa7d5:
    case 0xa7d8: case 0xa7d9: case 0xa7db: case 0xa7dc: case 0xa7dd: case 0xa7df:
    case 0xa7e2: case 0xa7e4: case 0xa7e5: case 0xa7e7: case 0xa7e9: case 0xa7ec:
    case 0xa7ee: case 0xa7f1: case 0xa7f4: case 0xa7f6: case 0xa7f8: case 0xa7fa:
    case 0xa7fd: case 0xa7ff: case 0xa801: case 0xa802: case 0xa804: case 0xa806:
    case 0xa808: case 0xa809: case 0xa80d: case 0xa810: case 0xa813: case 0xa815:
    case 0xa818: case 0xa81b: case 0xa81c: case 0xa81d: case 0xa81e: case 0xa81f:
    case 0xa820: case 0xa821: case 0xa822: case 0xa823: case 0xa826: case 0xa828:
    case 0xa82b: case 0xa82c: case 0xa82e: case 0xa830: case 0xa833: case 0xa835:
    case 0xa838: case 0xa83b: case 0xa83d: case 0xa83f: case 0xa841: case 0xa843:
    case 0xa846: case 0xa847: case 0xa849: case 0xa84d: case 0xa850: case 0xa851:
    case 0xa852: case 0xa853: case 0xa855: case 0xa857: case 0xa85a: case 0xa85c:
    case 0xa85f: case 0xa861: case 0xa863: case 0xa865: case 0xa866: case 0xa868:
    case 0xa86a: case 0xa86d: case 0xa86f: case 0xa872: case 0xa875: case 0xa878:
    case 0xa87b: case 0xa87d: case 0xa87f: case 0xa881: case 0xa883: case 0xa885:
    case 0xa888: case 0xa88a: case 0xa88d: case 0xa890: case 0xa893: case 0xa896:
    case 0xa898: case 0xa89a: case 0xa89c: case 0xa89e: case 0xa8a0: case 0xa8a2:
    case 0xa8a4: case 0xa8a7: case 0xa8a9: case 0xa8ac: case 0xa8ad: case 0xa8b0:
    case 0xa8b3: case 0xa8b5: case 0xa8b7: case 0xa8ba: case 0xa8bd: case 0xa8bf:
    case 0xa8c1: case 0xa8c3: case 0xa8c5: case 0xa8c7: case 0xa8c9: case 0xa8cb:
    case 0xa8ce: case 0xa8d0: case 0xa8d3: case 0xa8d4: case 0xa8d6: case 0xa8d8:
    case 0xa8db: case 0xa8dd: case 0xa8df: case 0xa8e2: case 0xa8e5: case 0xa8e8: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xa5f7: cmp_constant8(c, 0x39); break;
    case 0xa5f9: branch_to(c, !c->z, 0xa612); break;
    case 0xa5fb: video_load(c, 0x152c, false); break;
    case 0xa5fe: branch_to(c, c->z, 0xa609); break;
    case 0xa600: byte_constant(c, 0x01); break;
    case 0xa602: store_dp(c, 0x00, true); break;
    case 0xa604: byte_constant(c, 0x02); break;
    case 0xa606: jsr_abs_native(c, 0xa90d); break;
    case 0xa609: index_constant(c, false, 0x0010); break;
    case 0xa60c: index_constant(c, true, 0x210c); break;
    case 0xa60f: jmp_abs_native(c, 0xa8e9); break;
    case 0xa612: cmp_constant8(c, 0x5a); break;
    case 0xa614: branch_to(c, c->z, 0xa61a); break;
    case 0xa616: cmp_constant8(c, 0x3d); break;
    case 0xa618: branch_to(c, !c->z, 0xa658); break;
    case 0xa61a: load_a_abs_y8(c, 0x0020); break;
    case 0xa61d: store_a_abs_y8(c, 0x0001); break;
    case 0xa620: sep_rep_native(c, 0x20, false); break;
    case 0xa622: load_a_abs_y16(c, 0x0007); break;
    case 0xa625: store_a_abs16(c, 0x17ea); break;
    case 0xa628: lda16_constant(c, 0x000a); break;
    case 0xa62b: store_a_abs16(c, 0x17ed); break;
    case 0xa62e: sep_rep_native(c, 0x20, true); break;
    case 0xa630: load_a_abs_y8(c, 0x0009); break;
    case 0xa633: store_register8(c, 0x17ec); break;
    case 0xa636: phy16(c); break;
    case 0xa637: jsl_to(c, 0x806c); break;
    case 0xa63b: ply16(c); break;
    case 0xa63c: sep_rep_native(c, 0x20, false); break;
    case 0xa63e: video_load(c, 0x17ef, false); break;
    case 0xa641: store_a_abs_y16(c, 0x0007); break;
    case 0xa644: sep_rep_native(c, 0x20, true); break;
    case 0xa646: video_load(c, 0x17f1, false); break;
    case 0xa649: store_a_abs_y8(c, 0x0009); break;
    case 0xa64c: jsr_abs_native(c, 0xb11d); break;
    case 0xa64f: index_constant(c, false, 0x0008); break;
    case 0xa652: index_constant(c, true, 0x0026); break;
    case 0xa655: jmp_abs_native(c, 0xa907); break;
    case 0xa658: cmp_constant8(c, 0x41); break;
    case 0xa65a: branch_to(c, !c->z, 0xa676); break;
    case 0xa65c: byte_constant(c, 0x01); break;
    case 0xa65e: store_dp(c, 0x00, true); break;
    case 0xa660: load_dp8(c, 0x2f); break;
    case 0xa662: and_constant8(c, 0x03); break;
    case 0xa664: branch_to(c, !c->z, 0xa668); break;
    case 0xa666: inc_dp8(c, 0x00); break;
    case 0xa668: byte_constant(c, 0x04); break;
    case 0xa66a: jsr_abs_native(c, 0xa90d); break;
    case 0xa66d: index_constant(c, false, 0x0004); break;
    case 0xa670: index_constant(c, true, 0x0030); break;
    case 0xa673: jmp_abs_native(c, 0xa907); break;
    case 0xa676: cmp_constant8(c, 0x44); break;
    case 0xa678: branch_to(c, !c->z, 0xa683); break;
    case 0xa67a: index_constant(c, false, 0x0000); break;
    case 0xa67d: index_constant(c, true, 0x0050); break;
    case 0xa680: jmp_abs_native(c, 0xa907); break;
    case 0xa683: cmp_constant8(c, 0x4d); break;
    case 0xa685: branch_to(c, !c->z, 0xa6ba); break;
    case 0xa687: video_load(c, 0x15f1, false); break;
    case 0xa68a: branch_to(c, !c->n, 0xa6a1); break;
    case 0xa68c: jsl_to(c, 0xa9aa); break;
    case 0xa690: jsl_to(c, 0x8583); break;
    case 0xa694: byte_constant(c, 0x04); break;
    case 0xa696: store_dp(c, 0xb6, true); break;
    case 0xa698: index_constant(c, false, 0x0001); break;
    case 0xa69b: index_constant(c, true, 0x014b); break;
    case 0xa69e: jmp_abs_native(c, 0xa907); break;
    case 0xa6a1: video_load(c, 0x14cc, false); break;
    case 0xa6a4: branch_to(c, c->z, 0xa6b1); break;
    case 0xa6a6: video_load(c, 0x14cd, false); break;
    case 0xa6a9: cmp_constant8(c, 0x02); break;
    case 0xa6ab: branch_to(c, !c->c, 0xa6b1); break;
    case 0xa6ad: byte_constant(c, 0x02); break;
    case 0xa6af: store_dp(c, 0xb6, true); break;
    case 0xa6b1: index_constant(c, false, 0x0001); break;
    case 0xa6b4: index_constant(c, true, 0x3a04); break;
    case 0xa6b7: jmp_abs_native(c, 0xa8e9); break;
    case 0xa6ba: cmp_constant8(c, 0x4e); break;
    case 0xa6bc: branch_to(c, !c->z, 0xa700); break;
    case 0xa6be: video_load(c, 0x14cc, false); break;
    case 0xa6c1: branch_to(c, c->z, 0xa6f7); break;
    case 0xa6c3: video_load(c, 0x14cd, false); break;
    case 0xa6c6: cmp_constant8(c, 0x02); break;
    case 0xa6c8: branch_to(c, !c->c, 0xa6f7); break;
    case 0xa6ca: jsr_abs_native(c, 0xb6c0); break;
    case 0xa6cd: sep_rep_native(c, 0x20, false); break;
    case 0xa6cf: load_a_abs_y16(c, 0x0003); break;
    case 0xa6d2: case 0xa6d3: implied(c); c->c = (c->a & 1u) != 0; c->a >>= 1; zn(c, c->a, false); break;
    case 0xa6d4: store_a_abs16(c, 0x14cf); break;
    case 0xa6d7: load_a_abs_y16(c, 0x0003); break;
    case 0xa6da: implied(c); c->c = true; break;
    case 0xa6db: sbc_abs16(c, 0x14cf); break;
    case 0xa6de: store_a_abs_y16(c, 0x0003); break;
    case 0xa6e1: sep_rep_native(c, 0x20, true); break;
    case 0xa6e3: jsr_abs_native(c, 0xb6b2); break;
    case 0xa6e6: sep_rep_native(c, 0x20, false); break;
    case 0xa6e8: load_a_abs_y16(c, 0x0003); break;
    case 0xa6eb: implied(c); c->c = false; break;
    case 0xa6ec: adc_abs16(c, 0x14cf); break;
    case 0xa6ef: store_a_abs_y16(c, 0x0003); break;
    case 0xa6f2: sep_rep_native(c, 0x20, true); break;
    case 0xa6f4: stz_abs8(c, 0x14d1); break;
    case 0xa6f7: index_constant(c, false, 0x0000); break;
    case 0xa6fa: index_constant(c, true, 0x5404); break;
    case 0xa6fd: jmp_abs_native(c, 0xa8e9); break;
    case 0xa700: cmp_constant8(c, 0x4b); break;
    case 0xa702: branch_to(c, !c->z, 0xa776); break;
    case 0xa704: byte_constant(c, 0x00); break;
    case 0xa706: xba_native(c); break;
    case 0xa707: video_load(c, 0x14c5, false); break;
    case 0xa70a: case 0xa70b: case 0xa70c: case 0xa70d: lsr_a8(c); break;
    case 0xa70e: tax16(c); break;
    case 0xa70f: load_a_long_x8(c, 0x05a988); break;
    case 0xa713: and_abs8(c, 0x1529); break;
    case 0xa716: branch_to(c, !c->z, 0xa768); break;
    case 0xa718: load_a_long_x8(c, 0x05a988); break;
    case 0xa71c: ora_abs8(c, 0x1529); break;
    case 0xa71f: store_register8(c, 0x1529); break;
    case 0xa722: jsr_abs_native(c, 0xb6b2); break;
    case 0xa725: sep_rep_native(c, 0x20, false); break;
    case 0xa727: load_a_abs_y16(c, 0x0007); break;
    case 0xa72a: store_dp(c, 0x00, false); break;
    case 0xa72c: sep_rep_native(c, 0x20, true); break;
    case 0xa72e: load_a_abs_y8(c, 0x0009); break;
    case 0xa731: store_dp(c, 0x02, true); break;
    case 0xa733: lsr_dp8(c, 0x02); break;
    case 0xa735: ror_dp8(c, 0x01); break;
    case 0xa737: ror_dp8(c, 0x00); break;
    case 0xa739: load_a_abs_y8(c, 0x0001); break;
    case 0xa73c: cmp_constant8(c, 0x2f); break;
    case 0xa73e: branch_to(c, !c->z, 0xa746); break;
    case 0xa740: lsr_dp8(c, 0x02); break;
    case 0xa742: ror_dp8(c, 0x01); break;
    case 0xa744: ror_dp8(c, 0x00); break;
    case 0xa746: sep_rep_native(c, 0x20, false); break;
    case 0xa748: load_a_abs_y16(c, 0x0007); break;
    case 0xa74b: implied(c); c->c = false; break;
    case 0xa74c: adc_dp16(c, 0x00); break;
    case 0xa74e: store_a_abs_y16(c, 0x0007); break;
    case 0xa751: sep_rep_native(c, 0x20, true); break;
    case 0xa753: load_a_abs_y8(c, 0x0009); break;
    case 0xa756: adc8_dp(c, 0x02); break;
    case 0xa758: store_a_abs_y8(c, 0x0009); break;
    case 0xa75b: branch_to(c, !c->c, 0xa768); break;
    case 0xa75d: byte_constant(c, 0xff); break;
    case 0xa75f: store_a_abs_y8(c, 0x0007); break;
    case 0xa762: store_a_abs_y8(c, 0x0008); break;
    case 0xa765: store_a_abs_y8(c, 0x0009); break;
    case 0xa768: byte_constant(c, 0x04); break;
    case 0xa76a: store_register8(c, 0x00b6); break;
    case 0xa76d: index_constant(c, false, 0x0000); break;
    case 0xa770: index_constant(c, true, 0xffff); break;
    case 0xa773: jmp_abs_native(c, 0xa907); break;
    case 0xa776: cmp_constant8(c, 0x40); break;
    case 0xa778: branch_to(c, !c->z, 0xa7ac); break;
    case 0xa77a: index_constant(c, true, 0x0000); break;
    case 0xa77d: load_a_abs_y8(c, 0x0b18); break;
    case 0xa780: ora_constant8(c, 0x70); break;
    case 0xa782: store_a_abs_y8(c, 0x0b18); break;
    case 0xa785: jsr_abs_native(c, 0x96e2); break;
    case 0xa788: cpy16_constant(c, 0x0120); break;
    case 0xa78b: branch_to(c, !c->c, 0xa77d); break;
    case 0xa78d: index_constant(c, true, 0x0000); break;
    case 0xa790: load_a_abs_y8(c, 0x0e18); break;
    case 0xa793: ora_constant8(c, 0x70); break;
    case 0xa795: store_a_abs_y8(c, 0x0e18); break;
    case 0xa798: jsr_abs_native(c, 0x96e2); break;
    case 0xa79b: cpy16_constant(c, 0x00a0); break;
    case 0xa79e: branch_to(c, !c->c, 0xa790); break;
    case 0xa7a0: jsr_abs_native(c, 0xb6c0); break;
    case 0xa7a3: load_a_abs_y8(c, 0x0018); break;
    case 0xa7a6: ora_constant8(c, 0x70); break;
    case 0xa7a8: store_a_abs_y8(c, 0x0018); break;
    case 0xa7ab: return_short(c); break;
    case 0xa7ac: cmp_constant8(c, 0x2b); break;
    case 0xa7ae: branch_to(c, !c->z, 0xa7e5); break;
    case 0xa7b0: byte_constant(c, 0x00); break;
    case 0xa7b2: xba_native(c); break;
    case 0xa7b3: video_load(c, 0x14c3, false); break;
    case 0xa7b6: case 0xa7b7: case 0xa7b8: case 0xa7b9: lsr_a8(c); break;
    case 0xa7ba: tax16(c); break;
    case 0xa7bb: sep_rep_native(c, 0x20, false); break;
    case 0xa7bd: lda_long_x16(c, 0x05a988); break;
    case 0xa7c1: and_abs16(c, 0x152a); break;
    case 0xa7c4: branch_to(c, !c->z, 0xa7e2); break;
    case 0xa7c6: lda_long_x16(c, 0x05a988); break;
    case 0xa7ca: ora_abs16(c, 0x152a); break;
    case 0xa7cd: store_a_abs16(c, 0x152a); break;
    case 0xa7d0: sep_rep_native(c, 0x20, true); break;
    case 0xa7d2: jsr_abs_native(c, 0xb6c0); break;
    case 0xa7d5: load_a_abs_y8(c, 0x0017); break;
    case 0xa7d8: lsr_a8(c); break;
    case 0xa7d9: store_dp(c, 0x00, true); break;
    case 0xa7db: lsr_a8(c); break;
    case 0xa7dc: implied(c); c->c = false; break;
    case 0xa7dd: adc8_dp(c, 0x00); break;
    case 0xa7df: store_a_abs_y8(c, 0x0017); break;
    case 0xa7e2: sep_rep_native(c, 0x20, true); break;
    case 0xa7e4: return_short(c); break;
    case 0xa7e5: cmp_constant8(c, 0x4c); break;
    case 0xa7e7: branch_to(c, !c->z, 0xa853); break;
    case 0xa7e9: video_load(c, 0x14cc, false); break;
    case 0xa7ec: branch_to(c, c->z, 0xa852); break;
    case 0xa7ee: index_constant(c, false, 0x0000); break;
    case 0xa7f1: video_load(c, 0x14cd, false); break;
    case 0xa7f4: cmp_constant8(c, 0x02); break;
    case 0xa7f6: branch_to(c, !c->c, 0xa852); break;
    case 0xa7f8: branch_to(c, c->z, 0xa7fd); break;
    case 0xa7fa: index_constant(c, false, 0x0008); break;
    case 0xa7fd: stx_dp16(c, 0x00); break;
    case 0xa7ff: byte_constant(c, 0x00); break;
    case 0xa801: xba_native(c); break;
    case 0xa802: load_dp8(c, 0x2f); break;
    case 0xa804: and_constant8(c, 0x07); break;
    case 0xa806: ora_dp8(c, 0x00); break;
    case 0xa808: tax16(c); break;
    case 0xa809: load_a_long_x8(c, 0x05a99a); break;
    case 0xa80d: store_register8(c, 0x4206); break;
    case 0xa810: jsr_abs_native(c, 0xb6c0); break;
    case 0xa813: sep_rep_native(c, 0x20, false); break;
    case 0xa815: load_a_abs_y16(c, 0x000c); break;
    case 0xa818: store_a_abs16(c, 0x4204); break;
    case 0xa81b: case 0xa81c: case 0xa81d: case 0xa81e: case 0xa81f: case 0xa820: case 0xa821: case 0xa822: nop_native(c); break;
    case 0xa823: video_load(c, 0x4214, false); break;
    case 0xa826: store_dp(c, 0x00, false); break;
    case 0xa828: load_a_abs_y16(c, 0x0005); break;
    case 0xa82b: implied(c); c->c = true; break;
    case 0xa82c: sbc_dp8(c, 0x00); break;
    case 0xa82e: branch_to(c, c->c, 0xa838); break;
    case 0xa830: load_a_abs_y16(c, 0x0005); break;
    case 0xa833: store_dp(c, 0x00, false); break;
    case 0xa835: lda16_constant(c, 0x0000); break;
    case 0xa838: store_a_abs_y16(c, 0x0005); break;
    case 0xa83b: sep_rep_native(c, 0x20, true); break;
    case 0xa83d: load_dp8(c, 0x00); break;
    case 0xa83f: branch_to(c, !c->z, 0xa847); break;
    case 0xa841: byte_constant(c, 0x01); break;
    case 0xa843: store_register8(c, 0x14cd); break;
    case 0xa846: return_short(c); break;
    case 0xa847: stz_dp8(c, 0x02); break;
    case 0xa849: jsl_to(c, 0x8044); break;
    case 0xa84d: index_constant(c, true, 0x0053); break;
    case 0xa850: case 0xa851: pla8(c); break;
    case 0xa852: return_short(c); break;
    case 0xa853: cmp_constant8(c, 0x50); break;
    case 0xa855: branch_to(c, !c->z, 0xa87b); break;
    case 0xa857: video_load(c, 0x14cc, false); break;
    case 0xa85a: branch_to(c, !c->z, 0xa861); break;
    case 0xa85c: stz_abs8(c, 0x14cd); break;
    case 0xa85f: branch_to(c, true, 0xa872); break;
    case 0xa861: load_dp8(c, 0x2f); break;
    case 0xa863: and_constant8(c, 0x01); break;
    case 0xa865: inc_a8(c); break;
    case 0xa866: store_dp(c, 0x00, true); break;
    case 0xa868: byte_constant(c, 0x08); break;
    case 0xa86a: jsr_abs_native(c, 0xa90d); break;
    case 0xa86d: byte_constant(c, 0x02); break;
    case 0xa86f: store_register8(c, 0x14cd); break;
    case 0xa872: index_constant(c, false, 0x0002); break;
    case 0xa875: index_constant(c, true, 0x210c); break;
    case 0xa878: jmp_abs_native(c, 0xa8e9); break;
    case 0xa87b: cmp_constant8(c, 0x52); break;
    case 0xa87d: branch_to(c, !c->z, 0xa896); break;
    case 0xa87f: byte_constant(c, 0x03); break;
    case 0xa881: store_dp(c, 0xb6, true); break;
    case 0xa883: byte_constant(c, 0x3a); break;
    case 0xa885: store_a_abs_y8(c, 0x0001); break;
    case 0xa888: byte_constant(c, 0x80); break;
    case 0xa88a: store_register8(c, 0x0c36); break;
    case 0xa88d: index_constant(c, false, 0x0000); break;
    case 0xa890: index_constant(c, true, 0xffff); break;
    case 0xa893: jmp_abs_native(c, 0xa907); break;
    case 0xa896: cmp_constant8(c, 0x54); break;
    case 0xa898: branch_to(c, c->z, 0xa8a4); break;
    case 0xa89a: cmp_constant8(c, 0x55); break;
    case 0xa89c: branch_to(c, c->z, 0xa8a9); break;
    case 0xa89e: cmp_constant8(c, 0x59); break;
    case 0xa8a0: branch_to(c, c->z, 0xa8ad); break;
    case 0xa8a2: branch_to(c, !c->z, 0xa8bd); break;
    case 0xa8a4: bit_abs8(c, 0x0c37); break;
    case 0xa8a7: branch_to(c, !c->v, 0xa8ad); break;
    case 0xa8a9: stz_abs8(c, 0x0c37); break;
    case 0xa8ac: return_short(c); break;
    case 0xa8ad: index_constant(c, false, 0x0000); break;
    case 0xa8b0: index_constant(c, true, 0xffff); break;
    case 0xa8b3: byte_constant(c, 0x03); break;
    case 0xa8b5: store_dp(c, 0xb6, true); break;
    case 0xa8b7: inc_abs8_native(c, 0x0c37); break;
    case 0xa8ba: jmp_abs_native(c, 0xa907); break;
    case 0xa8bd: cmp_constant8(c, 0x32); break;
    case 0xa8bf: branch_to(c, c->z, 0xa8ce); break;
    case 0xa8c1: cmp_constant8(c, 0x56); break;
    case 0xa8c3: branch_to(c, c->z, 0xa8ce); break;
    case 0xa8c5: cmp_constant8(c, 0x51); break;
    case 0xa8c7: branch_to(c, !c->z, 0xa8d4); break;
    case 0xa8c9: byte_constant(c, 0x20); break;
    case 0xa8cb: store_register8(c, 0x1527); break;
    case 0xa8ce: byte_constant(c, 0x80); break;
    case 0xa8d0: store_register8(c, 0x1528); break;
    case 0xa8d3: return_short(c); break;
    case 0xa8d4: cmp_constant8(c, 0x5b); break;
    case 0xa8d6: branch_to(c, !c->z, 0xa8e8); break;
    case 0xa8d8: inc_abs8_native(c, 0x14e8); break;
    case 0xa8db: byte_constant(c, 0x02); break;
    case 0xa8dd: store_dp(c, 0xb6, true); break;
    case 0xa8df: index_constant(c, false, 0x0000); break;
    case 0xa8e2: index_constant(c, true, 0x0044); break;
    case 0xa8e5: jmp_abs_native(c, 0xa907); break;
    case 0xa8e8: return_short(c); break;
  }
  return true;
}
/* ==== Batch: E117 F54F/F708 cluster + F82B + CAFA DA45/DA7C ==== */

/* $05:F6A8–$F6B8: if DP+$B7 < #$12, Y += #$0040; RTS. */
static bool actor_adj_f6a8_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xf6b0 || pc == 0xf6b1 || pc == 0xf6b2 || pc == 0xf6b5)
                      || (pc == 0xf6b6);
  const bool sep_entry = pc == 0xf6b6;
  const bool rep_entry = pc == 0xf6ae;
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xf6a8: case 0xf6aa: case 0xf6ac: case 0xf6ae: case 0xf6b0: case 0xf6b1:
    case 0xf6b2: case 0xf6b5: case 0xf6b6: case 0xf6b8: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf6a8: load_dp8(c, 0xb7); break;
    case 0xf6aa: cmp_constant8(c, 0x12); break;
    case 0xf6ac: branch_to(c, c->c, 0xf6b8); break;
    case 0xf6ae: sep_rep_native(c, 0x20, false); break;
    case 0xf6b0: tya16(c); break;
    case 0xf6b1: implied(c); c->c = false; break;
    case 0xf6b2: adc16_constant(c, 0x0040); break;
    case 0xf6b5: tay16(c); break;
    case 0xf6b6: sep_rep_native(c, 0x20, true); break;
    case 0xf6b8: return_short(c); break;
  }
  return true;
}

/* $05:F6B9–$F6EE: optional Y+=#$80; stream $10:8CF0,X into [DP+$00],Y with DP+$03 attr; RTS. */
static bool actor_fill_f6b9_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xf6c1 || pc == 0xf6c2 || pc == 0xf6c3 || pc == 0xf6c6)
                      || (pc == 0xf6c7 || pc == 0xf6e2 || pc == 0xf6e3 || pc == 0xf6e6);
  const bool sep_entry = (pc == 0xf6c7 || pc == 0xf6e6);
  const bool rep_entry = (pc == 0xf6bf || pc == 0xf6e0);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xf6b9: case 0xf6bb: case 0xf6bd: case 0xf6bf: case 0xf6c1: case 0xf6c2:
    case 0xf6c3: case 0xf6c6: case 0xf6c7: case 0xf6c9: case 0xf6cb: case 0xf6cd:
    case 0xf6d1: case 0xf6d3: case 0xf6d4: case 0xf6d6: case 0xf6d7: case 0xf6d9:
    case 0xf6db: case 0xf6dc: case 0xf6de: case 0xf6e0: case 0xf6e2: case 0xf6e3:
    case 0xf6e6: case 0xf6e8: case 0xf6ea: case 0xf6ec: case 0xf6ee: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf6b9: load_dp8(c, 0xb7); break;
    case 0xf6bb: cmp_constant8(c, 0x12); break;
    case 0xf6bd: branch_to(c, c->c, 0xf6c9); break;
    case 0xf6bf: sep_rep_native(c, 0x20, false); break;
    case 0xf6c1: tya16(c); break;
    case 0xf6c2: implied(c); c->c = false; break;
    case 0xf6c3: adc16_constant(c, 0x0080); break;
    case 0xf6c6: tay16(c); break;
    case 0xf6c7: sep_rep_native(c, 0x20, true); break;
    case 0xf6c9: byte_constant(c, 0x0e); break;
    case 0xf6cb: store_dp(c, 0x04, true); break;
    case 0xf6cd: load_a_long_x8(c, 0x108cf0); break;
    case 0xf6d1: store_dp(c, 0x05, true); break;
    case 0xf6d3: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xf6d4: sta_indirect_long_y8(c, 0x00); break;
    case 0xf6d6: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xf6d7: load_dp8(c, 0x03); break;
    case 0xf6d9: sta_indirect_long_y8(c, 0x00); break;
    case 0xf6db: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xf6dc: dec_dp8(c, 0x04); break;
    case 0xf6de: branch_to(c, !c->n, 0xf6cd); break;
    case 0xf6e0: sep_rep_native(c, 0x20, false); break;
    case 0xf6e2: tya16(c); break;
    case 0xf6e3: and_constant16(c, 0x003f); break;
    case 0xf6e6: sep_rep_native(c, 0x20, true); break;
    case 0xf6e8: branch_to(c, c->z, 0xf6ee); break;
    case 0xf6ea: load_dp8(c, 0x05); break;
    case 0xf6ec: branch_to(c, true, 0xf6d4); break;
    case 0xf6ee: return_short(c); break;
  }
  return true;
}

/* $05:F685–$F6A7: seed $0808..$0810 via $F6A8-adjusted Y; RTS. */
static bool queue_seed_f685_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0xf685: case 0xf688: case 0xf68b: case 0xf68e: case 0xf691: case 0xf694:
    case 0xf697: case 0xf69a: case 0xf69c: case 0xf69f: case 0xf6a1: case 0xf6a4:
    case 0xf6a7: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf685: index_constant(c, true, 0x6480); break;
    case 0xf688: jsr_abs_native(c, 0xf6a8); break;
    case 0xf68b: store_y_abs16(c, 0x0809); break;
    case 0xf68e: index_constant(c, true, 0x0800); break;
    case 0xf691: store_y_abs16(c, 0x080b); break;
    case 0xf694: index_constant(c, true, 0x0280); break;
    case 0xf697: store_y_abs16(c, 0x080e); break;
    case 0xf69a: byte_constant(c, 0x7f); break;
    case 0xf69c: store_register8(c, 0x080d); break;
    case 0xf69f: byte_constant(c, 0x80); break;
    case 0xf6a1: store_register8(c, 0x0808); break;
    case 0xf6a4: stz_abs8(c, 0x0810); break;
    case 0xf6a7: return_short(c); break;
  }
  return true;
}

/* $05:F54F–$F684: ($E117,8) — $1546 phase #$80→$F609 / #$82 DMA / else AND $B0; RTS. */
static bool actor_hdl_f54f_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xf5ad || pc == 0xf5af || pc == 0xf5b2 || pc == 0xf5b3)
                      || (pc == 0xf5b6 || pc == 0xf5b9 || pc == 0xf5bc || pc == 0xf5bf)
                      || (pc == 0xf5c2 || pc == 0xf5c5 || pc == 0xf5c8 || pc == 0xf5f2)
                      || (pc == 0xf5f5 || pc == 0xf5f6 || pc == 0xf5f9 || pc == 0xf5fc)
                      || (pc == 0xf600 || pc == 0xf603 || pc == 0xf606);
  const bool sep_entry = (pc == 0xf5c8 || pc == 0xf5fc || pc == 0xf606);
  const bool rep_entry = (pc == 0xf5ab || pc == 0xf5f0 || pc == 0xf5fe);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xf54f: case 0xf552: case 0xf554: case 0xf556: case 0xf559: case 0xf55b:
    case 0xf55d: case 0xf560: case 0xf562: case 0xf565: case 0xf568: case 0xf56b:
    case 0xf56e: case 0xf571: case 0xf573: case 0xf575: case 0xf577: case 0xf579:
    case 0xf57b: case 0xf57d: case 0xf581: case 0xf584: case 0xf585: case 0xf588:
    case 0xf58a: case 0xf58d: case 0xf590: case 0xf592: case 0xf595: case 0xf598:
    case 0xf59a: case 0xf59c: case 0xf59e: case 0xf5a0: case 0xf5a2: case 0xf5a3:
    case 0xf5a6: case 0xf5a7: case 0xf5a8: case 0xf5a9: case 0xf5ab: case 0xf5ad:
    case 0xf5af: case 0xf5b2: case 0xf5b3: case 0xf5b6: case 0xf5b9: case 0xf5bc:
    case 0xf5bf: case 0xf5c2: case 0xf5c5: case 0xf5c8: case 0xf5ca: case 0xf5cc:
    case 0xf5cf: case 0xf5d2: case 0xf5d4: case 0xf5d7: case 0xf5da: case 0xf5dd:
    case 0xf5df: case 0xf5e1: case 0xf5e3: case 0xf5e6: case 0xf5e9: case 0xf5eb:
    case 0xf5ed: case 0xf5f0: case 0xf5f2: case 0xf5f5: case 0xf5f6: case 0xf5f9:
    case 0xf5fc: case 0xf5fe: case 0xf600: case 0xf603: case 0xf606: case 0xf608:
    case 0xf609: case 0xf60c: case 0xf60f: case 0xf611: case 0xf613: case 0xf615:
    case 0xf617: case 0xf61a: case 0xf61d: case 0xf620: case 0xf623: case 0xf626:
    case 0xf629: case 0xf62c: case 0xf62f: case 0xf632: case 0xf634: case 0xf636:
    case 0xf639: case 0xf63c: case 0xf63f: case 0xf642: case 0xf645: case 0xf648:
    case 0xf64b: case 0xf64e: case 0xf651: case 0xf654: case 0xf657: case 0xf65a:
    case 0xf65d: case 0xf65f: case 0xf662: case 0xf665: case 0xf667: case 0xf66a:
    case 0xf66d: case 0xf670: case 0xf673: case 0xf676: case 0xf679: case 0xf67c:
    case 0xf67e: case 0xf681: case 0xf684: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf54f: video_load(c, 0x1546, false); break;
    case 0xf552: cmp_constant8(c, 0x80); break;
    case 0xf554: branch_to(c, !c->z, 0xf559); break;
    case 0xf556: jmp_abs_native(c, 0xf609); break;
    case 0xf559: cmp_constant8(c, 0x82); break;
    case 0xf55b: branch_to(c, c->z, 0xf568); break;
    case 0xf55d: video_load(c, 0x17dc, false); break;
    case 0xf560: branch_to(c, !c->z, 0xf565); break;
    case 0xf562: inc_abs8_native(c, 0x1546); break;
    case 0xf565: jmp_abs_native(c, 0xf5dd); break;
    case 0xf568: jsr_abs_native(c, 0xb6b2); break;
    case 0xf56b: video_load(c, 0x17dc, false); break;
    case 0xf56e: cpy16_constant(c, 0x1440); break;
    case 0xf571: branch_to(c, c->z, 0xf579); break;
    case 0xf573: cmp_constant8(c, 0x00); break;
    case 0xf575: branch_to(c, !c->z, 0xf57d); break;
    case 0xf577: branch_to(c, true, 0xf5dd); break;
    case 0xf579: cmp_constant8(c, 0xff); break;
    case 0xf57b: branch_to(c, c->z, 0xf5dd); break;
    case 0xf57d: jsl_to(c, 0x8711); break;
    case 0xf581: video_load(c, 0x17dc, false); break;
    case 0xf584: lsr_a8(c); break;
    case 0xf585: index_constant(c, true, 0x64e0); break;
    case 0xf588: branch_to(c, !c->c, 0xf58d); break;
    case 0xf58a: index_constant(c, true, 0x60e0); break;
    case 0xf58d: jsr_abs_native(c, 0xf6a8); break;
    case 0xf590: sty_dp16(c, 0x00); break;
    case 0xf592: jsr_abs_native(c, 0xb6b2); break;
    case 0xf595: cpy16_constant(c, 0x1440); break;
    case 0xf598: branch_to(c, c->z, 0xf5a0); break;
    case 0xf59a: load_dp8(c, 0x01); break;
    case 0xf59c: eor_constant8(c, 0x04); break;
    case 0xf59e: store_dp(c, 0x01, true); break;
    case 0xf5a0: byte_constant(c, 0x00); break;
    case 0xf5a2: xba_native(c); break;
    case 0xf5a3: video_load(c, 0x17db, false); break;
    case 0xf5a6: lsr_a8(c); break;
    case 0xf5a7: lsr_a8(c); break;
    case 0xf5a8: lsr_a8(c); break;
    case 0xf5a9: and_constant8(c, 0xfe); break;
    case 0xf5ab: sep_rep_native(c, 0x20, false); break;
    case 0xf5ad: ora_dp16(c, 0x00); break;
    case 0xf5af: store_a_abs_x16(c, 0x0801); break;
    case 0xf5b2: inc_a16(c); break;
    case 0xf5b3: store_a_abs_x16(c, 0x0809); break;
    case 0xf5b6: lda16_constant(c, 0x0010); break;
    case 0xf5b9: store_a_abs_x16(c, 0x0806); break;
    case 0xf5bc: store_a_abs_x16(c, 0x080e); break;
    case 0xf5bf: lda16_constant(c, 0x8d2c); break;
    case 0xf5c2: store_a_abs_x16(c, 0x0803); break;
    case 0xf5c5: store_a_abs_x16(c, 0x080b); break;
    case 0xf5c8: sep_rep_native(c, 0x20, true); break;
    case 0xf5ca: byte_constant(c, 0x10); break;
    case 0xf5cc: store_a_abs_x8(c, 0x0805); break;
    case 0xf5cf: store_a_abs_x8(c, 0x080d); break;
    case 0xf5d2: byte_constant(c, 0x81); break;
    case 0xf5d4: store_a_abs_x8(c, 0x0800); break;
    case 0xf5d7: store_a_abs_x8(c, 0x0808); break;
    case 0xf5da: stz_abs_x8(c, 0x0810); break;
    case 0xf5dd: load_dp8(c, 0xb0); break;
    case 0xf5df: and_constant8(c, 0xfd); break;
    case 0xf5e1: store_dp(c, 0xb0, true); break;
    case 0xf5e3: jsr_abs_native(c, 0xb6b2); break;
    case 0xf5e6: load_a_abs_y8(c, 0x0021); break;
    case 0xf5e9: cmp_constant8(c, 0x02); break;
    case 0xf5eb: branch_to(c, !c->z, 0xf5fe); break;
    case 0xf5ed: jsr_abs_native(c, 0xf548); break;
    case 0xf5f0: sep_rep_native(c, 0x20, false); break;
    case 0xf5f2: lda16_constant(c, 0x0080); break;
    case 0xf5f5: implied(c); c->c = true; break;
    case 0xf5f6: sbc_abs_y16(c, 0x0037); break;
    case 0xf5f9: store_a_abs16(c, 0x17db); break;
    case 0xf5fc: sep_rep_native(c, 0x20, true); break;
    case 0xf5fe: sep_rep_native(c, 0x20, false); break;
    case 0xf600: load_a_abs_y16(c, 0x0024); break;
    case 0xf603: store_a_abs_y16(c, 0x0037); break;
    case 0xf606: sep_rep_native(c, 0x20, true); break;
    case 0xf608: return_short(c); break;
    case 0xf609: jsr_abs_native(c, 0xf998); break;
    case 0xf60c: index_constant(c, false, 0x0000); break;
    case 0xf60f: stx_dp16(c, 0x00); break;
    case 0xf611: byte_constant(c, 0x7f); break;
    case 0xf613: store_dp(c, 0x02, true); break;
    case 0xf615: stz_dp8(c, 0x03); break;
    case 0xf617: index_constant(c, false, 0x0000); break;
    case 0xf61a: index_constant(c, true, 0x01d6); break;
    case 0xf61d: jsr_abs_native(c, 0xf6b9); break;
    case 0xf620: index_constant(c, true, 0x0216); break;
    case 0xf623: jsr_abs_native(c, 0xf6b9); break;
    case 0xf626: index_constant(c, true, 0x0256); break;
    case 0xf629: jsr_abs_native(c, 0xf6b9); break;
    case 0xf62c: index_constant(c, true, 0x0296); break;
    case 0xf62f: jsr_abs_native(c, 0xf6b9); break;
    case 0xf632: byte_constant(c, 0x80); break;
    case 0xf634: store_dp(c, 0x03, true); break;
    case 0xf636: index_constant(c, false, 0x002d); break;
    case 0xf639: index_constant(c, true, 0x02d6); break;
    case 0xf63c: jsr_abs_native(c, 0xf6b9); break;
    case 0xf63f: index_constant(c, false, 0x001e); break;
    case 0xf642: index_constant(c, true, 0x0316); break;
    case 0xf645: jsr_abs_native(c, 0xf6b9); break;
    case 0xf648: index_constant(c, false, 0x000f); break;
    case 0xf64b: index_constant(c, true, 0x0356); break;
    case 0xf64e: jsr_abs_native(c, 0xf6b9); break;
    case 0xf651: index_constant(c, false, 0x0000); break;
    case 0xf654: index_constant(c, true, 0x0396); break;
    case 0xf657: jsr_abs_native(c, 0xf6b9); break;
    case 0xf65a: jsr_abs_native(c, 0xb6b2); break;
    case 0xf65d: byte_constant(c, 0x00); break;
    case 0xf65f: store_a_abs_y8(c, 0x0021); break;
    case 0xf662: cpy16_constant(c, 0x1440); break;
    case 0xf665: branch_to(c, c->z, 0xf66a); break;
    case 0xf667: jsr_abs_native(c, 0xf952); break;
    case 0xf66a: jsr_abs_native(c, 0xf91e); break;
    case 0xf66d: jsr_abs_native(c, 0xf685); break;
    case 0xf670: index_constant(c, false, 0x0100); break;
    case 0xf673: stx_abs16(c, 0x17db); break;
    case 0xf676: index_constant(c, false, 0xfff8); break;
    case 0xf679: stx_abs16(c, 0x17dd); break;
    case 0xf67c: byte_constant(c, 0x61); break;
    case 0xf67e: store_register8(c, 0x2108); break;
    case 0xf681: inc_abs8_native(c, 0x1546); break;
    case 0xf684: return_short(c); break;
  }
  return true;
}

/* $05:F6EF–$F76A: ($E117,A) + sibling X stubs → $F998/$F8B2/$F91E/$F93D/$F85E; RTS. */
static bool actor_hdl_f708_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xf724 || pc == 0xf728 || pc == 0xf72a);
  const bool sep_entry = pc == 0xf72a;
  const bool rep_entry = pc == 0xf722;
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xf6ef: case 0xf6f2: case 0xf6f4: case 0xf6f7: case 0xf6f9: case 0xf6fc:
    case 0xf6fe: case 0xf701: case 0xf703: case 0xf706: case 0xf708: case 0xf70b:
    case 0xf70d: case 0xf710: case 0xf712: case 0xf715: case 0xf717: case 0xf71a:
    case 0xf71b: case 0xf71e: case 0xf71f: case 0xf722: case 0xf724: case 0xf728:
    case 0xf72a: case 0xf72c: case 0xf72e: case 0xf730: case 0xf732: case 0xf734:
    case 0xf737: case 0xf739: case 0xf73c: case 0xf73e: case 0xf740: case 0xf742:
    case 0xf745: case 0xf746: case 0xf747: case 0xf749: case 0xf74c: case 0xf74f:
    case 0xf752: case 0xf755: case 0xf757: case 0xf759: case 0xf75b: case 0xf75e:
    case 0xf761: case 0xf763: case 0xf765: case 0xf767: case 0xf76a: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf6ef: index_constant(c, false, 0x009e); break;
    case 0xf6f2: branch_to(c, true, 0xf71a); break;
    case 0xf6f4: index_constant(c, false, 0x00c4); break;
    case 0xf6f7: branch_to(c, true, 0xf71a); break;
    case 0xf6f9: index_constant(c, false, 0x00ea); break;
    case 0xf6fc: branch_to(c, true, 0xf71a); break;
    case 0xf6fe: index_constant(c, false, 0x0112); break;
    case 0xf701: branch_to(c, true, 0xf71a); break;
    case 0xf703: index_constant(c, false, 0x013e); break;
    case 0xf706: branch_to(c, true, 0xf71a); break;
    case 0xf708: index_constant(c, false, 0x0000); break;
    case 0xf70b: branch_to(c, true, 0xf71a); break;
    case 0xf70d: index_constant(c, false, 0x002e); break;
    case 0xf710: branch_to(c, true, 0xf71a); break;
    case 0xf712: index_constant(c, false, 0x0058); break;
    case 0xf715: branch_to(c, true, 0xf71a); break;
    case 0xf717: index_constant(c, false, 0x007c); break;
    case 0xf71a: phx16(c); break;
    case 0xf71b: jsr_abs_native(c, 0xf998); break;
    case 0xf71e: plx16(c); break;
    case 0xf71f: index_constant(c, true, 0x0000); break;
    case 0xf722: sep_rep_native(c, 0x20, false); break;
    case 0xf724: lda_long_x16(c, 0x108add); break;
    case 0xf728: store_dp(c, 0x00, false); break;
    case 0xf72a: sep_rep_native(c, 0x20, true); break;
    case 0xf72c: branch_to(c, c->z, 0xf749); break;
    case 0xf72e: byte_constant(c, 0x10); break;
    case 0xf730: store_dp(c, 0x02, true); break;
    case 0xf732: byte_constant(c, 0x00); break;
    case 0xf734: cpx16_constant(c, 0x009e); break;
    case 0xf737: branch_to(c, c->c, 0xf740); break;
    case 0xf739: cpy16_constant(c, 0x0300); break;
    case 0xf73c: branch_to(c, !c->c, 0xf740); break;
    case 0xf73e: byte_constant(c, 0x80); break;
    case 0xf740: store_dp(c, 0x03, true); break;
    case 0xf742: jsr_abs_native(c, 0xf8b2); break;
    case 0xf745: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xf746: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xf747: branch_to(c, true, 0xf722); break;
    case 0xf749: jsr_abs_native(c, 0xf91e); break;
    case 0xf74c: jsr_abs_native(c, 0xf93d); break;
    case 0xf74f: jsr_abs_native(c, 0xf85e); break;
    case 0xf752: video_load(c, 0x01d6, false); break;
    case 0xf755: and_constant8(c, 0x60); break;
    case 0xf757: cmp_constant8(c, 0x60); break;
    case 0xf759: branch_to(c, !c->z, 0xf761); break;
    case 0xf75b: index_constant(c, true, 0xfff0); break;
    case 0xf75e: store_y_abs16(c, 0x17dd); break;
    case 0xf761: load_dp8(c, 0xb7); break;
    case 0xf763: cmp_constant8(c, 0x20); break;
    case 0xf765: branch_to(c, c->c, 0xf76a); break;
    case 0xf767: jsr_abs_native(c, 0xf548); break;
    case 0xf76a: return_short(c); break;
  }
  return true;
}

/* $05:F82B–$F85D: PHA/PHX; $F998; ptr $05F886,X → DP+$83/$85; JSR $E0AB; fall $F85E. */
static bool actor_setup_f82b_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xf833 || pc == 0xf837 || pc == 0xf839);
  const bool sep_entry = pc == 0xf839;
  const bool rep_entry = pc == 0xf831;
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xf82b: case 0xf82c: case 0xf82d: case 0xf830: case 0xf831: case 0xf833:
    case 0xf837: case 0xf839: case 0xf83b: case 0xf83f: case 0xf841: case 0xf844:
    case 0xf845: case 0xf847: case 0xf84a: case 0xf84d: case 0xf850: case 0xf852:
    case 0xf855: case 0xf858: case 0xf85b: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf82b: pha8(c); break;
    case 0xf82c: phx16(c); break;
    case 0xf82d: jsr_abs_native(c, 0xf998); break;
    case 0xf830: plx16(c); break;
    case 0xf831: sep_rep_native(c, 0x20, false); break;
    case 0xf833: lda_long_x16(c, 0x05f886); break;
    case 0xf837: store_dp(c, 0x83, false); break;
    case 0xf839: sep_rep_native(c, 0x20, true); break;
    case 0xf83b: load_a_long_x8(c, 0x05f888); break;
    case 0xf83f: store_dp(c, 0x85, true); break;
    case 0xf841: jsr_abs_native(c, 0xe0ab); break;
    case 0xf844: pla8(c); break;
    case 0xf845: branch_to(c, c->z, 0xf84a); break;
    case 0xf847: jsr_abs_native(c, 0xf952); break;
    case 0xf84a: jsr_abs_native(c, 0xf91e); break;
    case 0xf84d: jsr_abs_native(c, 0xf93d); break;
    case 0xf850: byte_constant(c, 0x60); break;
    case 0xf852: store_register8(c, 0x2108); break;
    case 0xf855: index_constant(c, true, 0x7068); break;
    case 0xf858: store_y_abs16(c, 0x2109); break;
    case 0xf85b: stz_abs8(c, 0x14ff); break;
  }
  return true;
}

/* $05:DA45–$DA63: face nibble gate → $DA64/$DA6D; JMP $CA8D. */
static bool actor_op_da45_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0xda45: case 0xda47: case 0xda49: case 0xda4b: case 0xda4d: case 0xda4f:
    case 0xda51: case 0xda53: case 0xda55: case 0xda57: case 0xda5a: case 0xda5c:
    case 0xda5f: case 0xda61: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xda45: ldy_dp16(c, 0x20); break;
    case 0xda47: load_dp8(c, 0x04); break;
    case 0xda49: and_constant8(c, 0x0f); break;
    case 0xda4b: cmp_constant8(c, 0x0f); break;
    case 0xda4d: branch_to(c, !c->z, 0xda5c); break;
    case 0xda4f: load_dp8(c, 0x04); break;
    case 0xda51: and_constant8(c, 0xf0); break;
    case 0xda53: store_dp(c, 0x04, true); break;
    case 0xda55: byte_constant(c, 0x00); break;
    case 0xda57: jsr_abs_native(c, 0xda6d); break;
    case 0xda5a: branch_to(c, true, 0xda5f); break;
    case 0xda5c: jsr_abs_native(c, 0xda64); break;
    case 0xda5f: byte_constant(c, 0x01); break;
    case 0xda61: jmp_abs_native(c, 0xca8d); break;
  }
  return true;
}

/* $05:DA7C–$DA8C: store DP+$04 → $0031,Y; optional $DA64; JMP $CA8D. */
static bool actor_op_da7c_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0xda7c: case 0xda7e: case 0xda80: case 0xda83: case 0xda85: case 0xda88:
    case 0xda8a: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xda7c: ldy_dp16(c, 0x20); break;
    case 0xda7e: load_dp8(c, 0x04); break;
    case 0xda80: store_a_abs_y8(c, 0x0031); break;
    case 0xda83: branch_to(c, !c->z, 0xda88); break;
    case 0xda85: jsr_abs_native(c, 0xda64); break;
    case 0xda88: byte_constant(c, 0x01); break;
    case 0xda8a: jmp_abs_native(c, 0xca8d); break;
  }
  return true;
}


/* ==== Batch: E117 F76B+ slots + F000 setup + CAFA 95C9/DA8D ==== */

/* $05:F76B–$F82A: remaining ($E117,*) handlers — stubs + $F782/$F7CA bodies;
 * call native $F82B / $F8A1 / $F000; $F820 falls into $F82B. */
static bool actor_hdl_f76b_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xf76b: case 0xf76d: case 0xf770: case 0xf773: case 0xf776: case 0xf779:
    case 0xf77a: case 0xf77c: case 0xf77f: case 0xf782: case 0xf785: case 0xf787:
    case 0xf789: case 0xf78c: case 0xf78f: case 0xf791: case 0xf795: case 0xf796:
    case 0xf798: case 0xf799: case 0xf79c: case 0xf79f: case 0xf7a1: case 0xf7a3:
    case 0xf7a6: case 0xf7a8: case 0xf7aa: case 0xf7ad: case 0xf7b0: case 0xf7b3:
    case 0xf7b5: case 0xf7b8: case 0xf7bb: case 0xf7be: case 0xf7c1: case 0xf7c2:
    case 0xf7c4: case 0xf7c7: case 0xf7ca: case 0xf7cd: case 0xf7cf: case 0xf7d1:
    case 0xf7d3: case 0xf7d6: case 0xf7d8: case 0xf7da: case 0xf7dc: case 0xf7de:
    case 0xf7e0: case 0xf7e3: case 0xf7e5: case 0xf7e7: case 0xf7e9: case 0xf7eb:
    case 0xf7ed: case 0xf7ef: case 0xf7f1: case 0xf7f3: case 0xf7f5: case 0xf7f7:
    case 0xf7f9: case 0xf7fb: case 0xf7fd: case 0xf7ff: case 0xf801: case 0xf803:
    case 0xf805: case 0xf808: case 0xf80a: case 0xf80c: case 0xf80f: case 0xf811:
    case 0xf813: case 0xf815: case 0xf817: case 0xf81a: case 0xf81d: case 0xf820:
    case 0xf822: case 0xf824: case 0xf826: case 0xf828: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf76b: byte_constant(c, 0x00); break;
    case 0xf76d: index_constant(c, false, 0x0000); break;
    case 0xf770: jsr_abs_native(c, 0xf82b); break;
    case 0xf773: index_constant(c, true, 0x0800); break;
    case 0xf776: store_y_abs16(c, 0x0806); break;
    case 0xf779: return_short(c); break;
    case 0xf77a: byte_constant(c, 0x00); break;
    case 0xf77c: index_constant(c, false, 0x0003); break;
    case 0xf77f: jmp_abs_native(c, 0xf82b); break;
    case 0xf782: video_load(c, 0x1546, false); break;
    case 0xf785: cmp_constant8(c, 0x82); break;
    case 0xf787: branch_to(c, c->z, 0xf799); break;
    case 0xf789: jsr_abs_native(c, 0xf000); break;
    case 0xf78c: index_constant(c, false, 0x000f); break;
    case 0xf78f: byte_constant(c, 0x00); break;
    case 0xf791: store_a_long_x8(c, 0x7f1760); break;
    case 0xf795: implied(c); c->x--; zn(c, c->x, false); break;
    case 0xf796: branch_to(c, !c->n, 0xf78f); break;
    case 0xf798: return_short(c); break;
    case 0xf799: index_constant(c, false, 0x0006); break;
    case 0xf79c: video_load(c, 0x1540, false); break;
    case 0xf79f: cmp_constant8(c, 0x09); break;
    case 0xf7a1: branch_to(c, c->z, 0xf7ad); break;
    case 0xf7a3: index_constant(c, false, 0x000f); break;
    case 0xf7a6: cmp_constant8(c, 0x0c); break;
    case 0xf7a8: branch_to(c, c->z, 0xf7ad); break;
    case 0xf7aa: index_constant(c, false, 0x0018); break;
    case 0xf7ad: jsr_abs_native(c, 0xf8a1); break;
    case 0xf7b0: jmp_abs_native(c, 0xf82b); break;
    case 0xf7b3: byte_constant(c, 0x00); break;
    case 0xf7b5: index_constant(c, false, 0x0009); break;
    case 0xf7b8: jsr_abs_native(c, 0xf82b); break;
    case 0xf7bb: index_constant(c, true, 0x0800); break;
    case 0xf7be: store_y_abs16(c, 0x0806); break;
    case 0xf7c1: return_short(c); break;
    case 0xf7c2: byte_constant(c, 0x00); break;
    case 0xf7c4: index_constant(c, false, 0x000c); break;
    case 0xf7c7: jmp_abs_native(c, 0xf82b); break;
    case 0xf7ca: jsr_abs_native(c, 0xb6b2); break;
    case 0xf7cd: load_dp8(c, 0xb7); break;
    case 0xf7cf: cmp_constant8(c, 0x24); break;
    case 0xf7d1: branch_to(c, !c->z, 0xf7da); break;
    case 0xf7d3: load_a_abs_y8(c, 0x0034); break;
    case 0xf7d6: cmp_constant8(c, 0x05); break;
    case 0xf7d8: branch_to(c, c->z, 0xf81a); break;
    case 0xf7da: load_dp8(c, 0xb7); break;
    case 0xf7dc: cmp_constant8(c, 0x29); break;
    case 0xf7de: branch_to(c, !c->z, 0xf7e7); break;
    case 0xf7e0: load_a_abs_y8(c, 0x0034); break;
    case 0xf7e3: cmp_constant8(c, 0x16); break;
    case 0xf7e5: branch_to(c, c->z, 0xf81a); break;
    case 0xf7e7: load_dp8(c, 0xb7); break;
    case 0xf7e9: cmp_constant8(c, 0x31); break;
    case 0xf7eb: branch_to(c, c->z, 0xf80c); break;
    case 0xf7ed: cmp_constant8(c, 0x33); break;
    case 0xf7ef: branch_to(c, c->z, 0xf813); break;
    case 0xf7f1: cmp_constant8(c, 0x36); break;
    case 0xf7f3: branch_to(c, c->z, 0xf80c); break;
    case 0xf7f5: cmp_constant8(c, 0x37); break;
    case 0xf7f7: branch_to(c, c->z, 0xf80c); break;
    case 0xf7f9: cmp_constant8(c, 0x57); break;
    case 0xf7fb: branch_to(c, c->z, 0xf80c); break;
    case 0xf7fd: cmp_constant8(c, 0x5d); break;
    case 0xf7ff: branch_to(c, c->z, 0xf80c); break;
    case 0xf801: cmp_constant8(c, 0x5e); break;
    case 0xf803: branch_to(c, !c->z, 0xf817); break;
    case 0xf805: load_a_abs_y8(c, 0x0001); break;
    case 0xf808: cmp_constant8(c, 0x30); break;
    case 0xf80a: branch_to(c, c->z, 0xf817); break;
    case 0xf80c: video_load(c, 0x14c2, false); break;
    case 0xf80f: and_constant8(c, 0x01); break;
    case 0xf811: branch_to(c, true, 0xf81a); break;
    case 0xf813: byte_constant(c, 0x00); break;
    case 0xf815: branch_to(c, true, 0xf81a); break;
    case 0xf817: jsr_abs_native(c, 0xf8a1); break;
    case 0xf81a: index_constant(c, false, 0x0012); break;
    case 0xf81d: jmp_abs_native(c, 0xf82b); break;
    case 0xf820: load_dp8(c, 0xb7); break;
    case 0xf822: cmp_constant8(c, 0x28); break;
    case 0xf824: branch_to(c, c->z, 0xf828); break;
    case 0xf826: byte_constant(c, 0x00); break;
    case 0xf828: index_constant(c, false, 0x0015); break;
  }
  return true;
}

/* $05:F000–$F069: ($E117) prep — Y seed from $1546 #$80/#$81; queue via $E089/$F91E/$B6B2; RTS. */
static bool actor_setup_f000_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xf033 || pc == 0xf037 || pc == 0xf03a || pc == 0xf03d)
                      || (pc == 0xf041 || pc == 0xf044 || pc == 0xf047 || pc == 0xf049)
                      || (pc == 0xf04c || pc == 0xf04e || pc == 0xf04f || pc == 0xf051)
                      || (pc == 0xf053 || pc == 0xf054 || pc == 0xf056 || pc == 0xf059)
                      || (pc == 0xf05c || pc == 0xf05f || pc == 0xf061);
  const bool sep_entry = pc == 0xf064;
  const bool rep_entry = (pc == 0xf031 || pc == 0xf035 || pc == 0xf03f);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xf000: case 0xf003: case 0xf006: case 0xf008: case 0xf00a: case 0xf00d:
    case 0xf00f: case 0xf011: case 0xf012: case 0xf015: case 0xf018: case 0xf01b:
    case 0xf01e: case 0xf021: case 0xf023: case 0xf025: case 0xf027: case 0xf029:
    case 0xf02a: case 0xf02b: case 0xf02c: case 0xf02d: case 0xf02e: case 0xf02f:
    case 0xf031: case 0xf033: case 0xf035: case 0xf037: case 0xf03a: case 0xf03d:
    case 0xf03f: case 0xf041: case 0xf044: case 0xf047: case 0xf049: case 0xf04c:
    case 0xf04e: case 0xf04f: case 0xf051: case 0xf053: case 0xf054: case 0xf056:
    case 0xf059: case 0xf05c: case 0xf05f: case 0xf061: case 0xf064: case 0xf066:
    case 0xf069: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf000: index_constant(c, true, 0x0400); break;
    case 0xf003: video_load(c, 0x1546, false); break;
    case 0xf006: cmp_constant8(c, 0x80); break;
    case 0xf008: branch_to(c, c->z, 0xf011); break;
    case 0xf00a: index_constant(c, true, 0x0800); break;
    case 0xf00d: cmp_constant8(c, 0x81); break;
    case 0xf00f: branch_to(c, !c->z, 0xf069); break;
    case 0xf011: phy16(c); break;
    case 0xf012: video_load(c, 0x1540, false); break;
    case 0xf015: jsr_abs_native(c, 0xe089); break;
    case 0xf018: jsr_abs_native(c, 0xf91e); break;
    case 0xf01b: jsr_abs_native(c, 0xb6b2); break;
    case 0xf01e: video_load(c, 0x1541, false); break;
    case 0xf021: cmp_constant8(c, 0x10); break;
    case 0xf023: branch_to(c, c->z, 0xf035); break;
    case 0xf025: cmp_constant8(c, 0x11); break;
    case 0xf027: branch_to(c, c->z, 0xf03f); break;
    case 0xf029: inc_a8(c); break;
    case 0xf02a: asl_a8(c); break;
    case 0xf02b: asl_a8(c); break;
    case 0xf02c: asl_a8(c); break;
    case 0xf02d: asl_a8(c); break;
    case 0xf02e: xba_native(c); break;
    case 0xf02f: byte_constant(c, 0x00); break;
    case 0xf031: sep_rep_native(c, 0x20, false); break;
    case 0xf033: branch_to(c, true, 0xf04c); break;
    case 0xf035: sep_rep_native(c, 0x20, false); break;
    case 0xf037: lda16_constant(c, 0x0000); break;
    case 0xf03a: cpy16_constant(c, 0x1400); break;
    case 0xf03d: branch_to(c, true, 0xf047); break;
    case 0xf03f: sep_rep_native(c, 0x20, false); break;
    case 0xf041: lda16_constant(c, 0x0000); break;
    case 0xf044: cpy16_constant(c, 0x1440); break;
    case 0xf047: branch_to(c, c->z, 0xf04c); break;
    case 0xf049: lda16_constant(c, 0x1000); break;
    case 0xf04c: store_dp(c, 0x12, false); break;
    case 0xf04e: pla16(c); break;
    case 0xf04f: store_dp(c, 0x10, false); break;
    case 0xf051: asl_dp16(c, 0x10); break;
    case 0xf053: implied(c); c->c = false; break;
    case 0xf054: adc_dp16(c, 0x12); break;
    case 0xf056: store_a_abs16(c, 0x0801); break;
    case 0xf059: lda16_constant(c, 0x0800); break;
    case 0xf05c: store_a_abs16(c, 0x0806); break;
    case 0xf05f: load_dp16_to_a(c, 0x10); break;
    case 0xf061: store_a_abs16(c, 0x0803); break;
    case 0xf064: sep_rep_native(c, 0x20, true); break;
    case 0xf066: inc_abs8_native(c, 0x1546); break;
    case 0xf069: return_short(c); break;
  }
  return true;
}

/* $05:95C9–$95F5: face/index via $B6B2 + $1D8C67,X → $121B..$121E / DP+$24; RTS. */
static bool actor_face_95c9_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x95c9: case 0x95cb: case 0x95ce: case 0x95d0: case 0x95d1: case 0x95d4:
    case 0x95d6: case 0x95d7: case 0x95db: case 0x95dd: case 0x95df: case 0x95e2:
    case 0x95e4: case 0x95e7: case 0x95e9: case 0x95ec: case 0x95ee: case 0x95f1:
    case 0x95f3: case 0x95f5: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x95c9: store_dp(c, 0x00, true); break;
    case 0x95cb: jsr_abs_native(c, 0xb6b2); break;
    case 0x95ce: byte_constant(c, 0x00); break;
    case 0x95d0: xba_native(c); break;
    case 0x95d1: load_a_abs_y8(c, 0x0020); break;
    case 0x95d4: and_constant8(c, 0x7f); break;
    case 0x95d6: tax16_from_a8(c); break;
    case 0x95d7: load_a_long_x8(c, 0x1d8c67); break;
    case 0x95db: cmp_constant8(c, 0xff); break;
    case 0x95dd: branch_to(c, c->z, 0x95f5); break;
    case 0x95df: store_register8(c, 0x121b); break;
    case 0x95e2: load_dp8(c, 0x00); break;
    case 0x95e4: store_register8(c, 0x121c); break;
    case 0x95e7: byte_constant(c, 0x01); break;
    case 0x95e9: store_register8(c, 0x15e8); break;
    case 0x95ec: byte_constant(c, 0x04); break;
    case 0x95ee: store_register8(c, 0x121e); break;
    case 0x95f1: byte_constant(c, 0x8c); break;
    case 0x95f3: store_dp(c, 0x24, true); break;
    case 0x95f5: return_short(c); break;
  }
  return true;
}

/* $05:DA8D–$DA9B: ($CAFA,36) LDA #$82 / JSR $95C9; $15E8=#$02; LDA #0 / JMP $DD51. */
static bool actor_op_da8d_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xda8d: case 0xda8f: case 0xda92: case 0xda94: case 0xda97: case 0xda99: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xda8d: byte_constant(c, 0x82); break;
    case 0xda8f: jsr_abs_native(c, 0x95c9); break;
    case 0xda92: byte_constant(c, 0x02); break;
    case 0xda94: store_register8(c, 0x15e8); break;
    case 0xda97: byte_constant(c, 0x00); break;
    case 0xda99: jmp_abs_native(c, 0xdd51); break;
  }
  return true;
}

/* ==== Batch: CAFA D48F/DA9C + E117 F06A + bank-05 F386 ==== */

/* CMP $xx,X (dpx) — matches LakeSnes cpu_adrDpx + cpu_cmp (m8). */
static void cmp_dp_x8(Cpu *c, uint8_t offset) {
  (void)fetch(c);
  if(c->dp & 255u) idle(c);
  idle(c);
  uint16_t address = (uint16_t)(c->dp + offset + c->x);
  sample_interrupt(c);
  uint8_t value = c->read(c->mem, address);
  uint8_t a = (uint8_t)c->a;
  c->c = a >= value; zn(c, (uint8_t)(a - value), true);
}

/* $05:D485–$D520: ($CAFA,22) $D48F INY/BIT score cluster + $D485/$D48A leaves;
 * siblings $D4A4/$D4A9/$D4BE/$D4CB/$D4D2/$D4F2/$D503/$D50D converge on $D4E7 → $CA8D.
 * Uses CMP $01,X / $34,X after $B6B2→TYX. */
static bool actor_op_d48f_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xd485: case 0xd487: case 0xd48a: case 0xd48c:
    case 0xd48f: case 0xd492: case 0xd494: case 0xd495: case 0xd497: case 0xd499:
    case 0xd49a: case 0xd49d: case 0xd49f: case 0xd4a1: case 0xd4a2: case 0xd4a4:
    case 0xd4a7: case 0xd4a9: case 0xd4ab: case 0xd4ad: case 0xd4af: case 0xd4b2:
    case 0xd4b4: case 0xd4b6: case 0xd4b8: case 0xd4ba: case 0xd4bc: case 0xd4be:
    case 0xd4bf: case 0xd4c1: case 0xd4c4: case 0xd4c5: case 0xd4c7: case 0xd4c9:
    case 0xd4cb: case 0xd4ce: case 0xd4d0: case 0xd4d2: case 0xd4d3: case 0xd4d6:
    case 0xd4d7: case 0xd4d8: case 0xd4da: case 0xd4dc: case 0xd4de: case 0xd4e0:
    case 0xd4e2: case 0xd4e4: case 0xd4e6: case 0xd4e7: case 0xd4e9: case 0xd4ea:
    case 0xd4ed: case 0xd4ef: case 0xd4f2: case 0xd4f3: case 0xd4f6: case 0xd4f9:
    case 0xd4fa: case 0xd4fc: case 0xd4fe: case 0xd500: case 0xd503: case 0xd506:
    case 0xd508: case 0xd50a: case 0xd50b: case 0xd50d: case 0xd50e: case 0xd511:
    case 0xd513: case 0xd515: case 0xd518: case 0xd51b: case 0xd51c: case 0xd51f:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xd485: load_dp8(c, 0x04); break;
    case 0xd487: store_register8(c, 0x14d4); break;
    case 0xd48a: byte_constant(c, 0x01); break;
    case 0xd48c: jmp_abs_native(c, 0xca8d); break;
    case 0xd48f: video_load(c, 0x01e7, false); break;
    case 0xd492: branch_to(c, c->n, 0xd4e7); break;
    case 0xd494: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xd495: bit_imm8(c, 0x04); break;
    case 0xd497: branch_to(c, !c->z, 0xd4e7); break;
    case 0xd499: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xd49a: video_load(c, 0x17d2, false); break;
    case 0xd49d: and_constant8(c, 0x01); break;
    case 0xd49f: branch_to(c, c->z, 0xd4e7); break;
    case 0xd4a1: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xd4a2: branch_to(c, true, 0xd4e7); break;
    case 0xd4a4: video_load(c, 0x14c1, false); break;
    case 0xd4a7: branch_to(c, true, 0xd4ce); break;
    case 0xd4a9: load_dp8(c, 0x04); break;
    case 0xd4ab: cmp_constant8(c, 0xff); break;
    case 0xd4ad: branch_to(c, !c->z, 0xd4ba); break;
    case 0xd4af: video_load(c, 0x14e8, false); break;
    case 0xd4b2: cmp_constant8(c, 0x02); break;
    case 0xd4b4: branch_to(c, !c->z, 0xd4ba); break;
    case 0xd4b6: byte_constant(c, 0x28); break;
    case 0xd4b8: store_dp(c, 0x04, true); break;
    case 0xd4ba: load_dp8(c, 0xb7); break;
    case 0xd4bc: branch_to(c, true, 0xd4ce); break;
    case 0xd4be: phy16(c); break;
    case 0xd4bf: ldy_dp16(c, 0x20); break;
    case 0xd4c1: load_a_abs_y8(c, 0x0014); break;
    case 0xd4c4: ply16(c); break;
    case 0xd4c5: cmp_constant8(c, 0x09); break;
    case 0xd4c7: branch_to(c, !c->z, 0xd48a); break;
    case 0xd4c9: branch_to(c, true, 0xd4e7); break;
    case 0xd4cb: video_load(c, 0x14d3, false); break;
    case 0xd4ce: cmp_dp8(c, 0x04); break;
    case 0xd4d0: branch_to(c, true, 0xd4e4); break;
    case 0xd4d2: phy16(c); break;
    case 0xd4d3: jsr_abs_native(c, 0xb6b2); break;
    case 0xd4d6: tyx16(c); break;
    case 0xd4d7: ply16(c); break;
    case 0xd4d8: load_dp8(c, 0x04); break;
    case 0xd4da: branch_to(c, c->n, 0xd4e0); break;
    case 0xd4dc: cmp_dp_x8(c, 0x01); break;
    case 0xd4de: branch_to(c, true, 0xd4e4); break;
    case 0xd4e0: and_constant8(c, 0x7f); break;
    case 0xd4e2: cmp_dp_x8(c, 0x34); break;
    case 0xd4e4: branch_to(c, !c->z, 0xd4ed); break;
    case 0xd4e6: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xd4e7: lda_indirect_long_y8(c, 0x00); break;
    case 0xd4e9: dec_a8(c); break;
    case 0xd4ea: jmp_abs_native(c, 0xca8d); break;
    case 0xd4ed: byte_constant(c, 0x02); break;
    case 0xd4ef: jmp_abs_native(c, 0xca8d); break;
    case 0xd4f2: phy16(c); break;
    case 0xd4f3: jsr_abs_native(c, 0xb6c0); break;
    case 0xd4f6: load_a_abs_y8(c, 0x0000); break;
    case 0xd4f9: ply16(c); break;
    case 0xd4fa: and_constant8(c, 0x40); break;
    case 0xd4fc: branch_to(c, !c->z, 0xd4e7); break;
    case 0xd4fe: byte_constant(c, 0x01); break;
    case 0xd500: jmp_abs_native(c, 0xca8d); break;
    case 0xd503: video_load(c, 0x14cc, false); break;
    case 0xd506: cmp_dp8(c, 0x04); break;
    case 0xd508: branch_to(c, !c->c, 0xd4ed); break;
    case 0xd50a: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xd50b: branch_to(c, true, 0xd4e7); break;
    case 0xd50d: phy16(c); break;
    case 0xd50e: jsr_abs_native(c, 0xb6c0); break;
    case 0xd511: load_dp8(c, 0x04); break;
    case 0xd513: branch_to(c, !c->z, 0xd518); break;
    case 0xd515: jsr_abs_native(c, 0xb6b2); break;
    case 0xd518: load_a_abs_y8(c, 0x0001); break;
    case 0xd51b: ply16(c); break;
    case 0xd51c: jsr_abs_native(c, 0xb582); break;
    case 0xd51f: branch_to(c, true, 0xd4e4); break;
  }
  return true;
}

/* $05:DA9C–$DB2A: ($CAFA,38) $14CD==4 battle-face seed via $95E4; else LDA#1→$CA8D.
 * Match paths set $121B/$15E8/$1425/$1465 then $D8A2/$DD51. */
static bool actor_op_da9c_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xda9c: case 0xda9f: case 0xdaa1: case 0xdaa3: case 0xdaa6: case 0xdaa8:
    case 0xdaaa: case 0xdaac: case 0xdaae: case 0xdab0: case 0xdab2: case 0xdab4:
    case 0xdab6: case 0xdab8: case 0xdaba: case 0xdabc: case 0xdabe: case 0xdac0:
    case 0xdac2: case 0xdac4: case 0xdac6: case 0xdac8: case 0xdaca: case 0xdacc:
    case 0xdace: case 0xdad0: case 0xdad2: case 0xdad4: case 0xdad7: case 0xdada:
    case 0xdadd: case 0xdade: case 0xdae0: case 0xdae1: case 0xdae2: case 0xdae5:
    case 0xdae7: case 0xdae9: case 0xdaec: case 0xdaef: case 0xdaf1: case 0xdaf3:
    case 0xdaf6: case 0xdaf7: case 0xdaf9: case 0xdafc: case 0xdafd: case 0xdafe:
    case 0xdb00: case 0xdb01: case 0xdb02: case 0xdb05: case 0xdb07: case 0xdb09:
    case 0xdb0a: case 0xdb0c: case 0xdb0f: case 0xdb11: case 0xdb14: case 0xdb16:
    case 0xdb19: case 0xdb1c: case 0xdb1e: case 0xdb20: case 0xdb22: case 0xdb23:
    case 0xdb24: case 0xdb27: case 0xdb28:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xda9c: video_load(c, 0x14cd, false); break;
    case 0xda9f: cmp_constant8(c, 0x04); break;
    case 0xdaa1: branch_to(c, !c->z, 0xdad2); break;
    case 0xdaa3: video_load(c, 0x01e7, false); break;
    case 0xdaa6: branch_to(c, c->n, 0xdb14); break;
    case 0xdaa8: load_dp8(c, 0xb7); break;
    case 0xdaaa: cmp_constant8(c, 0x01); break;
    case 0xdaac: branch_to(c, c->z, 0xdad7); break;
    case 0xdaae: cmp_constant8(c, 0x03); break;
    case 0xdab0: branch_to(c, c->z, 0xdae9); break;
    case 0xdab2: cmp_constant8(c, 0x04); break;
    case 0xdab4: branch_to(c, c->z, 0xdad7); break;
    case 0xdab6: cmp_constant8(c, 0x06); break;
    case 0xdab8: branch_to(c, c->z, 0xdad7); break;
    case 0xdaba: cmp_constant8(c, 0x08); break;
    case 0xdabc: branch_to(c, c->z, 0xdae9); break;
    case 0xdabe: cmp_constant8(c, 0x0a); break;
    case 0xdac0: branch_to(c, c->z, 0xdad7); break;
    case 0xdac2: cmp_constant8(c, 0x0c); break;
    case 0xdac4: branch_to(c, c->z, 0xdad7); break;
    case 0xdac6: cmp_constant8(c, 0x0d); break;
    case 0xdac8: branch_to(c, c->z, 0xdad7); break;
    case 0xdaca: cmp_constant8(c, 0x0f); break;
    case 0xdacc: branch_to(c, c->z, 0xdad7); break;
    case 0xdace: cmp_constant8(c, 0x14); break;
    case 0xdad0: branch_to(c, c->z, 0xdad7); break;
    case 0xdad2: byte_constant(c, 0x01); break;
    case 0xdad4: jmp_abs_native(c, 0xca8d); break;
    case 0xdad7: index_constant(c, true, 0x0085); break;
    case 0xdada: video_load(c, 0x01e7, false); break;
    case 0xdadd: lsr_a8(c); break;
    case 0xdade: branch_to(c, c->c, 0xdae1); break;
    case 0xdae0: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xdae1: implied(c); set_a8(c, (uint8_t)c->y); break;
    case 0xdae2: store_register8(c, 0x121b); break;
    case 0xdae5: byte_constant(c, 0x04); break;
    case 0xdae7: branch_to(c, true, 0xdb0c); break;
    case 0xdae9: index_constant(c, true, 0x0080); break;
    case 0xdaec: video_load(c, 0x01e7, false); break;
    case 0xdaef: bit_imm8(c, 0x02); break;
    case 0xdaf1: branch_to(c, !c->z, 0xdb01); break;
    case 0xdaf3: index_constant(c, true, 0x0081); break;
    case 0xdaf6: lsr_a8(c); break;
    case 0xdaf7: branch_to(c, c->c, 0xdafc); break;
    case 0xdaf9: index_constant(c, true, 0x0083); break;
    case 0xdafc: lsr_a8(c); break;
    case 0xdafd: lsr_a8(c); break;
    case 0xdafe: branch_to(c, !c->c, 0xdb01); break;
    case 0xdb00: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xdb01: implied(c); set_a8(c, (uint8_t)c->y); break;
    case 0xdb02: store_register8(c, 0x121b); break;
    case 0xdb05: load_dp8(c, 0x2f); break;
    case 0xdb07: and_constant8(c, 0x01); break;
    case 0xdb09: implied(c); c->c = false; break;
    case 0xdb0a: adc8_constant(c, 0x05); break;
    case 0xdb0c: jsr_abs_native(c, 0x95e4); break;
    case 0xdb0f: byte_constant(c, 0x02); break;
    case 0xdb11: store_register8(c, 0x15e8); break;
    case 0xdb14: byte_constant(c, 0x02); break;
    case 0xdb16: store_register8(c, 0x1425); break;
    case 0xdb19: store_register8(c, 0x1465); break;
    case 0xdb1c: load_dp8(c, 0x04); break;
    case 0xdb1e: cmp_constant8(c, 0x02); break;
    case 0xdb20: branch_to(c, c->c, 0xdb27); break;
    case 0xdb22: inc_a8(c); break;
    case 0xdb23: inc_a8(c); break;
    case 0xdb24: jmp_abs_native(c, 0xd8a2); break;
    case 0xdb27: dec_a8(c); break;
    case 0xdb28: jmp_abs_native(c, 0xdd51); break;
  }
  return true;
}

/* $05:F386–$F3A1: m16 adjust — if $14C7&2, A += $05F3A2[($14D4&$FF)<<1]; RTS.
 * Callee of $F06A (and siblings) after REP #$20. */
static bool actor_adj_f386_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || c->mf) return false;
  switch(pc) {
    case 0xf386: case 0xf387: case 0xf388: case 0xf38b: case 0xf38e: case 0xf390:
    case 0xf393: case 0xf396: case 0xf397: case 0xf398: case 0xf399: case 0xf39a:
    case 0xf39e: case 0xf39f: case 0xf3a0: case 0xf3a1:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf386: phx16(c); break;
    case 0xf387: pha16(c); break;
    case 0xf388: video_load(c, 0x14c7, false); break;
    case 0xf38b: and_constant16(c, 0x0002); break;
    case 0xf38e: branch_to(c, c->z, 0xf39f); break;
    case 0xf390: video_load(c, 0x14d4, false); break;
    case 0xf393: and_constant16(c, 0x00ff); break;
    case 0xf396: asl_a16(c); break;
    case 0xf397: tax16(c); break;
    case 0xf398: pla16(c); break;
    case 0xf399: implied(c); c->c = false; break;
    case 0xf39a: adc_long_x16(c, 0x05f3a2); break;
    case 0xf39e: pha16(c); break;
    case 0xf39f: pla16(c); break;
    case 0xf3a0: plx16(c); break;
    case 0xf3a1: return_short(c); break;
  }
  return true;
}

/* $05:F06A–$F0E9: ($E117,40/48) scroll/$155C prep — $1546==#$80 early Y seed + RTS;
 * else $1540→$1558 map, ±6 into $155C, JSR $F386, $155A>>=3, optional $155E|#$40. */
static bool actor_setup_f06a_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xf0be || pc == 0xf0c1 || pc == 0xf0c3 || pc == 0xf0c6
                      || pc == 0xf0c7 || pc == 0xf0ca || pc == 0xf0cd || pc == 0xf0d0
                      || pc == 0xf0d3 || pc == 0xf0d4 || pc == 0xf0d5 || pc == 0xf0d6);
  const bool sep_entry = (pc == 0xf0d9);
  const bool rep_entry = (pc == 0xf0bc);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xf06a: case 0xf06d: case 0xf06f: case 0xf071: case 0xf074: case 0xf075:
    case 0xf078: case 0xf07a: case 0xf07d: case 0xf080: case 0xf083: case 0xf084:
    case 0xf087: case 0xf089: case 0xf08b: case 0xf08d: case 0xf08f: case 0xf091:
    case 0xf093: case 0xf095: case 0xf097: case 0xf099: case 0xf09b: case 0xf09d:
    case 0xf09f: case 0xf0a1: case 0xf0a3: case 0xf0a5: case 0xf0a7: case 0xf0a9:
    case 0xf0ab: case 0xf0ad: case 0xf0af: case 0xf0b1: case 0xf0b2: case 0xf0b3:
    case 0xf0b5: case 0xf0b8: case 0xf0bb: case 0xf0bc: case 0xf0be: case 0xf0c1:
    case 0xf0c3: case 0xf0c6: case 0xf0c7: case 0xf0ca: case 0xf0cd: case 0xf0d0:
    case 0xf0d3: case 0xf0d4: case 0xf0d5: case 0xf0d6: case 0xf0d9: case 0xf0db:
    case 0xf0de: case 0xf0df: case 0xf0e1: case 0xf0e4: case 0xf0e6: case 0xf0e9:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf06a: video_load(c, 0x1546, false); break;
    case 0xf06d: cmp_constant8(c, 0x80); break;
    case 0xf06f: branch_to(c, !c->z, 0xf084); break;
    case 0xf071: video_load(c, 0x14c2, false); break;
    case 0xf074: lsr_a8(c); break;
    case 0xf075: index_constant(c, true, 0xffe0); break;
    case 0xf078: branch_to(c, !c->c, 0xf07d); break;
    case 0xf07a: index_constant(c, true, 0x0120); break;
    case 0xf07d: store_y_abs16(c, 0x155c); break;
    case 0xf080: inc_abs8_native(c, 0x1546); break;
    case 0xf083: return_short(c); break;
    case 0xf084: video_load(c, 0x1540, false); break;
    case 0xf087: cmp_constant8(c, 0x7f); break;
    case 0xf089: branch_to(c, !c->z, 0xf08f); break;
    case 0xf08b: byte_constant(c, 0xa6); break;
    case 0xf08d: branch_to(c, true, 0xf0b5); break;
    case 0xf08f: cmp_constant8(c, 0x6e); break;
    case 0xf091: branch_to(c, !c->z, 0xf097); break;
    case 0xf093: byte_constant(c, 0x9a); break;
    case 0xf095: branch_to(c, true, 0xf0b5); break;
    case 0xf097: cmp_constant8(c, 0x6b); break;
    case 0xf099: branch_to(c, !c->z, 0xf09f); break;
    case 0xf09b: byte_constant(c, 0x98); break;
    case 0xf09d: branch_to(c, true, 0xf0b5); break;
    case 0xf09f: cmp_constant8(c, 0x67); break;
    case 0xf0a1: branch_to(c, !c->z, 0xf0a7); break;
    case 0xf0a3: byte_constant(c, 0x85); break;
    case 0xf0a5: branch_to(c, true, 0xf0b5); break;
    case 0xf0a7: cmp_constant8(c, 0x70); break;
    case 0xf0a9: branch_to(c, !c->z, 0xf0af); break;
    case 0xf0ab: byte_constant(c, 0x84); break;
    case 0xf0ad: branch_to(c, true, 0xf0b5); break;
    case 0xf0af: and_constant8(c, 0x04); break;
    case 0xf0b1: lsr_a8(c); break;
    case 0xf0b2: lsr_a8(c); break;
    case 0xf0b3: ora_constant8(c, 0x98); break;
    case 0xf0b5: store_register8(c, 0x1558); break;
    case 0xf0b8: video_load(c, 0x14c2, false); break;
    case 0xf0bb: lsr_a8(c); break;
    case 0xf0bc: sep_rep_native(c, 0x20, false); break;
    case 0xf0be: lda16_constant(c, 0x0006); break;
    case 0xf0c1: branch_to(c, !c->c, 0xf0c6); break;
    case 0xf0c3: lda16_constant(c, 0xfffa); break;
    case 0xf0c6: implied(c); c->c = false; break;
    case 0xf0c7: adc_abs16(c, 0x155c); break;
    case 0xf0ca: store_a_abs16(c, 0x155c); break;
    case 0xf0cd: lda16_constant(c, 0x0300); break;
    case 0xf0d0: jsr_abs_native(c, 0xf386); break;
    case 0xf0d3: lsr_a16(c); break;
    case 0xf0d4: lsr_a16(c); break;
    case 0xf0d5: lsr_a16(c); break;
    case 0xf0d6: store_a_abs16(c, 0x155a); break;
    case 0xf0d9: sep_rep_native(c, 0x20, true); break;
    case 0xf0db: video_load(c, 0x14c2, false); break;
    case 0xf0de: lsr_a8(c); break;
    case 0xf0df: branch_to(c, c->c, 0xf0e9); break;
    case 0xf0e1: video_load(c, 0x155e, false); break;
    case 0xf0e4: ora_constant8(c, 0x40); break;
    case 0xf0e6: store_register8(c, 0x155e); break;
    case 0xf0e9: return_short(c); break;
  }
  return true;
}
/* ==== Batch: E117 F0EA+/F119/F167 + EAA4 stream + F353/F364 helpers ==== */

/* $05:F0EA–$F118: ($E117,E8/EA) scroll prep sibling of $F06A — LDA #$9C/#$82 → $1558;
 * $1546==#$80 early Y seed $155A/$155C else $155A+=3; RTS. */
static bool actor_setup_f0ea_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xf10c || pc == 0xf10f || pc == 0xf110 || pc == 0xf113);
  const bool sep_entry = (pc == 0xf116);
  const bool rep_entry = (pc == 0xf10a);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xf0ea: case 0xf0ec: case 0xf0ee: case 0xf0f0: case 0xf0f3: case 0xf0f6:
    case 0xf0f8: case 0xf0fa: case 0xf0fd: case 0xf100: case 0xf103: case 0xf106:
    case 0xf109: case 0xf10a: case 0xf10c: case 0xf10f: case 0xf110: case 0xf113:
    case 0xf116: case 0xf118:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf0ea: byte_constant(c, 0x9c); break;
    case 0xf0ec: branch_to(c, true, 0xf0f0); break;
    case 0xf0ee: byte_constant(c, 0x82); break;
    case 0xf0f0: store_register8(c, 0x1558); break;
    case 0xf0f3: video_load(c, 0x1546, false); break;
    case 0xf0f6: cmp_constant8(c, 0x80); break;
    case 0xf0f8: branch_to(c, !c->z, 0xf10a); break;
    case 0xf0fa: index_constant(c, true, 0xffe0); break;
    case 0xf0fd: store_y_abs16(c, 0x155a); break;
    case 0xf100: index_constant(c, true, 0x0080); break;
    case 0xf103: store_y_abs16(c, 0x155c); break;
    case 0xf106: inc_abs8_native(c, 0x1546); break;
    case 0xf109: return_short(c); break;
    case 0xf10a: sep_rep_native(c, 0x20, false); break;
    case 0xf10c: video_load(c, 0x155a, false); break;
    case 0xf10f: implied(c); c->c = false; break;
    case 0xf110: adc16_constant(c, 0x0003); break;
    case 0xf113: store_a_abs16(c, 0x155a); break;
    case 0xf116: sep_rep_native(c, 0x20, true); break;
    case 0xf118: return_short(c); break;
  }
  return true;
}

/* $05:F119–$F13A: ($E117,3E) — $F998 clear; fill $7F0280..$7F037F with #$0001;
 * JSR $F91E/$F93D/$F85E; RTS. */
static bool actor_hdl_f119_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xf11e || pc == 0xf121 || pc == 0xf124 || pc == 0xf128
                      || pc == 0xf129 || pc == 0xf12a || pc == 0xf12d);
  const bool sep_entry = (pc == 0xf12f);
  const bool rep_entry = (pc == 0xf11c);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xf119: case 0xf11c: case 0xf11e: case 0xf121: case 0xf124: case 0xf128:
    case 0xf129: case 0xf12a: case 0xf12d: case 0xf12f: case 0xf131: case 0xf134:
    case 0xf137: case 0xf13a:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf119: jsr_abs_native(c, 0xf998); break;
    case 0xf11c: sep_rep_native(c, 0x20, false); break;
    case 0xf11e: index_constant(c, false, 0x0280); break;
    case 0xf121: lda16_constant(c, 0x0001); break;
    case 0xf124: store_a_long_x16(c, 0x7f0000); break;
    case 0xf128: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xf129: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xf12a: cpx16_constant(c, 0x0380); break;
    case 0xf12d: branch_to(c, !c->c, 0xf124); break;
    case 0xf12f: sep_rep_native(c, 0x20, true); break;
    case 0xf131: jsr_abs_native(c, 0xf91e); break;
    case 0xf134: jsr_abs_native(c, 0xf93d); break;
    case 0xf137: jsr_abs_native(c, 0xf85e); break;
    case 0xf13a: return_short(c); break;
  }
  return true;
}

/* $05:EA8C–$EACF: ($E117,60+) prologue + stream expand — $F998; $1540-#$30 →
 * $17969C,X ptr; walk [DP+$00] words into $7F0000,X until #$FFFF (restart on
 * #$FFFE); then $F91E/$F93D/$F85E; RTS. Mid-entry $EAA4 keeps F13B callers. */
static bool stream_expand_eaa4_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xeaa0 || pc == 0xeaa4 || pc == 0xeaa6 || pc == 0xeaa9 || pc == 0xeaab
                      || pc == 0xeaac || pc == 0xeaad || pc == 0xeaae || pc == 0xeab0
                      || pc == 0xeab1 || pc == 0xeab2 || pc == 0xeab5 || pc == 0xeab7
                      || pc == 0xeaba || pc == 0xeabc || pc == 0xeac0 || pc == 0xeac1
                      || pc == 0xeac2);
  const bool sep_entry = (pc == 0xeac4);
  const bool rep_entry = (pc == 0xea9e);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xea8c: case 0xea8f: case 0xea91: case 0xea92: case 0xea95: case 0xea96:
    case 0xea98: case 0xea99: case 0xea9a: case 0xea9c: case 0xea9e: case 0xeaa0:
    case 0xeaa4: case 0xeaa6: case 0xeaa9: case 0xeaab: case 0xeaac: case 0xeaad:
    case 0xeaae: case 0xeab0: case 0xeab1: case 0xeab2: case 0xeab5: case 0xeab7:
    case 0xeaba: case 0xeabc: case 0xeac0: case 0xeac1: case 0xeac2: case 0xeac4:
    case 0xeac6: case 0xeac9: case 0xeacc: case 0xeacf:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xea8c: jsr_abs_native(c, 0xf998); break;
    case 0xea8f: byte_constant(c, 0x00); break;
    case 0xea91: xba_native(c); break;
    case 0xea92: video_load(c, 0x1540, false); break;
    case 0xea95: implied(c); c->c = true; break;
    case 0xea96: sbc8_constant(c, 0x30); break;
    case 0xea98: asl_a8(c); break;
    case 0xea99: tax16_from_a8(c); break;
    case 0xea9a: byte_constant(c, 0x17); break;
    case 0xea9c: store_dp(c, 0x02, true); break;
    case 0xea9e: sep_rep_native(c, 0x20, false); break;
    case 0xeaa0: lda_long_x16(c, 0x17969c); break;
    case 0xeaa4: store_dp(c, 0x00, false); break;
    case 0xeaa6: index_constant(c, true, 0x0000); break;
    case 0xeaa9: lda_indirect_long_y16(c, 0x00); break;
    case 0xeaab: tax16(c); break;
    case 0xeaac: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xeaad: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xeaae: lda_indirect_long_y16(c, 0x00); break;
    case 0xeab0: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xeab1: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xeab2: cmp_constant16(c, 0xffff); break;
    case 0xeab5: branch_to(c, c->z, 0xeac4); break;
    case 0xeab7: cmp_constant16(c, 0xfffe); break; /* ROM C9 FE FF */
    case 0xeaba: branch_to(c, c->z, 0xeaa9); break;
    case 0xeabc: store_a_long_x16(c, 0x7f0000); break;
    case 0xeac0: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xeac1: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xeac2: branch_to(c, true, 0xeaae); break;
    case 0xeac4: sep_rep_native(c, 0x20, true); break;
    case 0xeac6: jsr_abs_native(c, 0xf91e); break;
    case 0xeac9: jsr_abs_native(c, 0xf93d); break;
    case 0xeacc: jsr_abs_native(c, 0xf85e); break;
    case 0xeacf: return_short(c); break;
  }
  return true;
}

/* $05:F13B–$F164: ($E117,114+) — $F998; DP+$02=#$17; REP; LDA #$83EC/#$8464 →
 * JSR/JMP $EAA4; optional $F952 when $14C2 LSR C=0. */
static bool actor_hdl_f13b_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  /* F14A SEP runs after EAA4 already SEPped — treat as plain m8, not sep_entry. */
  const bool m16 = (pc == 0xf144 || pc == 0xf147 || pc == 0xf15f || pc == 0xf162);
  const bool rep_entry = (pc == 0xf142 || pc == 0xf15d);
  if(m16 && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xf13b: case 0xf13e: case 0xf140: case 0xf142: case 0xf144: case 0xf147:
    case 0xf14a: case 0xf14c: case 0xf14f: case 0xf150: case 0xf152: case 0xf155:
    case 0xf156: case 0xf159: case 0xf15b: case 0xf15d: case 0xf15f: case 0xf162:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf13b: jsr_abs_native(c, 0xf998); break;
    case 0xf13e: byte_constant(c, 0x17); break;
    case 0xf140: store_dp(c, 0x02, true); break;
    case 0xf142: sep_rep_native(c, 0x20, false); break;
    case 0xf144: lda16_constant(c, 0x83ec); break;
    case 0xf147: jsr_abs_native(c, 0xeaa4); break;
    case 0xf14a: sep_rep_native(c, 0x20, true); break;
    case 0xf14c: video_load(c, 0x14c2, false); break;
    case 0xf14f: lsr_a8(c); break;
    case 0xf150: branch_to(c, c->c, 0xf155); break;
    case 0xf152: jsr_abs_native(c, 0xf952); break;
    case 0xf155: return_short(c); break;
    case 0xf156: jsr_abs_native(c, 0xf998); break;
    case 0xf159: byte_constant(c, 0x17); break;
    case 0xf15b: store_dp(c, 0x02, true); break;
    case 0xf15d: sep_rep_native(c, 0x20, false); break;
    case 0xf15f: lda16_constant(c, 0x8464); break;
    case 0xf162: jmp_abs_native(c, 0xeaa4); break;
  }
  return true;
}

/* $05:F353–$F363: PHA; if DP+$B9==#$81 then PLA/CPY #$1400 else PLA/CPY #$1440; RTS.
 * Z from CPY selects slot. */
static bool actor_gate_f353_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xf353: case 0xf354: case 0xf356: case 0xf358: case 0xf35a: case 0xf35b:
    case 0xf35e: case 0xf35f: case 0xf360: case 0xf363:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf353: pha8(c); break;
    case 0xf354: load_dp8(c, 0xb9); break;
    case 0xf356: cmp_constant8(c, 0x81); break;
    case 0xf358: branch_to(c, !c->z, 0xf35f); break;
    case 0xf35a: pla8(c); break;
    case 0xf35b: cpy16_constant(c, 0x1400); break;
    case 0xf35e: return_short(c); break;
    case 0xf35f: pla8(c); break;
    case 0xf360: cpy16_constant(c, 0x1440); break;
    case 0xf363: return_short(c); break;
  }
  return true;
}

/* $05:F364–$F385: scale helper — [$12]×$1548 via $4202/$03; wrap $1548 vs [$12]+2;
 * LDY $4216 product; RTS. */
static bool actor_scale_f364_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xf364: case 0xf366: case 0xf369: case 0xf36c: case 0xf36f: case 0xf372:
    case 0xf375: case 0xf377: case 0xf378: case 0xf37a: case 0xf37d: case 0xf37f:
    case 0xf382: case 0xf385:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf364: lda_indirect_long8(c, 0x12); break;
    case 0xf366: store_register8(c, 0x4202); break;
    case 0xf369: video_load(c, 0x1548, false); break;
    case 0xf36c: store_register8(c, 0x4203); break;
    case 0xf36f: inc_abs8_native(c, 0x1548); break;
    case 0xf372: index_constant(c, true, 0x0002); break;
    case 0xf375: lda_indirect_long_y8(c, 0x12); break;
    case 0xf377: implied(c); c->c = true; break;
    case 0xf378: sbc8_constant(c, 0x01); break;
    case 0xf37a: cmp_abs8(c, 0x1548); break;
    case 0xf37d: branch_to(c, c->c, 0xf382); break;
    case 0xf37f: stz_abs8(c, 0x1548); break;
    case 0xf382: load_y_abs16(c, 0x4216); break;
    case 0xf385: return_short(c); break;
  }
  return true;
}

/* $05:F167–$F352: ($E117,20..) — $1540 nibble→DP+$10; $1546 phase #$80→$F29C /
 * #$82 DMA path via $F353/$F364/$F386/$8711; else $E089/$F91E queue; AND $B0;
 * $F29C tile expand + queue seed; RTS. */
static bool actor_hdl_f167_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = pc == 0xf197 || pc == 0xf19a || pc == 0xf19d || pc == 0xf1a0
                      || (pc == 0xf1a3 || pc == 0xf1a6 || pc == 0xf1d2 || pc == 0xf1d6)
                      || (pc == 0xf1d8 || pc == 0xf1db || pc == 0xf1dd || pc == 0xf1e0)
                      || (pc == 0xf1e2 || pc == 0xf1e4 || pc == 0xf1e5 || pc == 0xf1e8)
                      || (pc == 0xf1ea || pc == 0xf1ec || pc == 0xf1ed || pc == 0xf1ee)
                      || (pc == 0xf1ef || pc == 0xf1f1 || pc == 0xf1f4 || pc == 0xf1f7)
                      || (pc == 0xf1f8 || pc == 0xf1f9 || pc == 0xf1fb || pc == 0xf206)
                      || (pc == 0xf207 || pc == 0xf20a || pc == 0xf22e || pc == 0xf230)
                      || (pc == 0xf233 || pc == 0xf234 || pc == 0xf237 || pc == 0xf239)
                      || (pc == 0xf23c || pc == 0xf24d || pc == 0xf24f || pc == 0xf250)
                      || (pc == 0xf253 || pc == 0xf254 || pc == 0xf256 || pc == 0xf259)
                      || (pc == 0xf25b || pc == 0xf25c || pc == 0xf25f || pc == 0xf260)
                      || (pc == 0xf262 || pc == 0xf28f || pc == 0xf292 || pc == 0xf293)
                      || (pc == 0xf296 || pc == 0xf2a7 || pc == 0xf2ab || pc == 0xf2ad)
                      || (pc == 0xf2af || pc == 0xf2b2 || pc == 0xf2b5 || pc == 0xf2b6)
                      || (pc == 0xf2b8 || pc == 0xf2ba || pc == 0xf2bd || pc == 0xf2bf)
                      || (pc == 0xf2c3 || pc == 0xf2c4 || pc == 0xf2c5 || pc == 0xf2c6)
                      || (pc == 0xf2c7 || pc == 0xf2c8 || pc == 0xf2cb || pc == 0xf2cd)
                      || (pc == 0xf2cf || pc == 0xf2d0 || pc == 0xf2d3 || pc == 0xf32d)
                      || (pc == 0xf330 || pc == 0xf333 || pc == 0xf335 || pc == 0xf338)
                      || (pc == 0xf33b || pc == 0xf33c || pc == 0xf344 || pc == 0xf346)
                      || (pc == 0xf349 || pc == 0xf34a || pc == 0xf34d);
  const bool sep_entry = pc == 0xf1a9 || pc == 0xf1fc || pc == 0xf20b || pc == 0xf23f
                      || (pc == 0xf265 || pc == 0xf299 || pc == 0xf2d5 || pc == 0xf33d)
                      || (pc == 0xf350);
  const bool rep_entry = pc == 0xf195 || pc == 0xf1d0 || pc == 0xf204 || pc == 0xf22c
                      || (pc == 0xf24b || pc == 0xf28d || pc == 0xf2a5 || pc == 0xf32b)
                      || (pc == 0xf342);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xf167: case 0xf16a: case 0xf16c: case 0xf16d: case 0xf16f: case 0xf171:
    case 0xf174: case 0xf176: case 0xf178: case 0xf17b: case 0xf17d: case 0xf17f:
    case 0xf182: case 0xf184: case 0xf187: case 0xf189: case 0xf18b: case 0xf18d:
    case 0xf18f: case 0xf192: case 0xf195: case 0xf197: case 0xf19a: case 0xf19d:
    case 0xf1a0: case 0xf1a3: case 0xf1a6: case 0xf1a9: case 0xf1ab: case 0xf1ae:
    case 0xf1b1: case 0xf1b4: case 0xf1b7: case 0xf1ba: case 0xf1bc: case 0xf1be:
    case 0xf1c0: case 0xf1c3: case 0xf1c5: case 0xf1c7: case 0xf1ca: case 0xf1cc:
    case 0xf1ce: case 0xf1d0: case 0xf1d2: case 0xf1d6: case 0xf1d8: case 0xf1db:
    case 0xf1dd: case 0xf1e0: case 0xf1e2: case 0xf1e4: case 0xf1e5: case 0xf1e8:
    case 0xf1ea: case 0xf1ec: case 0xf1ed: case 0xf1ee: case 0xf1ef: case 0xf1f1:
    case 0xf1f4: case 0xf1f7: case 0xf1f8: case 0xf1f9: case 0xf1fb: case 0xf1fc:
    case 0xf1fe: case 0xf201: case 0xf202: case 0xf204: case 0xf206: case 0xf207:
    case 0xf20a: case 0xf20b: case 0xf20d: case 0xf20f: case 0xf212: case 0xf215:
    case 0xf217: case 0xf219: case 0xf21b: case 0xf21d: case 0xf221: case 0xf223:
    case 0xf224: case 0xf227: case 0xf228: case 0xf229: case 0xf22a: case 0xf22c:
    case 0xf22e: case 0xf230: case 0xf233: case 0xf234: case 0xf237: case 0xf239:
    case 0xf23c: case 0xf23f: case 0xf241: case 0xf244: case 0xf246: case 0xf249:
    case 0xf24b: case 0xf24d: case 0xf24f: case 0xf250: case 0xf253: case 0xf254:
    case 0xf256: case 0xf259: case 0xf25b: case 0xf25c: case 0xf25f: case 0xf260:
    case 0xf262: case 0xf265: case 0xf267: case 0xf269: case 0xf26c: case 0xf26f:
    case 0xf271: case 0xf274: case 0xf277: case 0xf27a: case 0xf27c: case 0xf27e:
    case 0xf280: case 0xf283: case 0xf286: case 0xf288: case 0xf28a: case 0xf28d:
    case 0xf28f: case 0xf292: case 0xf293: case 0xf296: case 0xf299: case 0xf29b:
    case 0xf29c: case 0xf29f: case 0xf2a1: case 0xf2a3: case 0xf2a5: case 0xf2a7:
    case 0xf2ab: case 0xf2ad: case 0xf2af: case 0xf2b2: case 0xf2b5: case 0xf2b6:
    case 0xf2b8: case 0xf2ba: case 0xf2bd: case 0xf2bf: case 0xf2c3: case 0xf2c4:
    case 0xf2c5: case 0xf2c6: case 0xf2c7: case 0xf2c8: case 0xf2cb: case 0xf2cd:
    case 0xf2cf: case 0xf2d0: case 0xf2d3: case 0xf2d5: case 0xf2d7: case 0xf2da:
    case 0xf2dc: case 0xf2df: case 0xf2e2: case 0xf2e4: case 0xf2e7: case 0xf2ea:
    case 0xf2ed: case 0xf2f0: case 0xf2f3: case 0xf2f6: case 0xf2f9: case 0xf2fc:
    case 0xf2fe: case 0xf301: case 0xf303: case 0xf306: case 0xf309: case 0xf30c:
    case 0xf30f: case 0xf312: case 0xf315: case 0xf317: case 0xf319: case 0xf31b:
    case 0xf31d: case 0xf320: case 0xf323: case 0xf326: case 0xf328: case 0xf32b:
    case 0xf32d: case 0xf330: case 0xf333: case 0xf335: case 0xf338: case 0xf33b:
    case 0xf33c: case 0xf33d: case 0xf33f: case 0xf342: case 0xf344: case 0xf346:
    case 0xf349: case 0xf34a: case 0xf34d: case 0xf350: case 0xf352:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xf167: video_load(c, 0x1540, false); break;
    case 0xf16a: and_constant8(c, 0x0f); break;
    case 0xf16c: asl_a8(c); break;
    case 0xf16d: store_dp(c, 0x10, true); break;
    case 0xf16f: stz_dp8(c, 0x11); break;
    case 0xf171: video_load(c, 0x1546, false); break;
    case 0xf174: cmp_constant8(c, 0x80); break;
    case 0xf176: branch_to(c, !c->z, 0xf17b); break;
    case 0xf178: jmp_abs_native(c, 0xf29c); break;
    case 0xf17b: cmp_constant8(c, 0x82); break;
    case 0xf17d: branch_to(c, c->z, 0xf1b1); break;
    case 0xf17f: video_load(c, 0x17dc, false); break;
    case 0xf182: branch_to(c, !c->z, 0xf1ae); break;
    case 0xf184: video_load(c, 0x1540, false); break;
    case 0xf187: cmp_constant8(c, 0x1a); break;
    case 0xf189: branch_to(c, c->z, 0xf18f); break;
    case 0xf18b: cmp_constant8(c, 0x1b); break;
    case 0xf18d: branch_to(c, !c->z, 0xf1ab); break;
    case 0xf18f: jsr_abs_native(c, 0xe089); break;
    case 0xf192: jsr_abs_native(c, 0xf91e); break;
    case 0xf195: sep_rep_native(c, 0x20, false); break;
    case 0xf197: lda16_constant(c, 0x3400); break;
    case 0xf19a: store_a_abs16(c, 0x0801); break;
    case 0xf19d: lda16_constant(c, 0x0200); break;
    case 0xf1a0: store_a_abs16(c, 0x0806); break;
    case 0xf1a3: lda16_constant(c, 0x0800); break;
    case 0xf1a6: store_a_abs16(c, 0x0803); break;
    case 0xf1a9: sep_rep_native(c, 0x20, true); break;
    case 0xf1ab: inc_abs8_native(c, 0x1546); break;
    case 0xf1ae: jmp_abs_native(c, 0xf27a); break;
    case 0xf1b1: jsr_abs_native(c, 0xb6b2); break;
    case 0xf1b4: video_load(c, 0x17dc, false); break;
    case 0xf1b7: jsr_abs_native(c, 0xf353); break;
    case 0xf1ba: branch_to(c, c->z, 0xf1c3); break;
    case 0xf1bc: cmp_constant8(c, 0x00); break;
    case 0xf1be: branch_to(c, !c->z, 0xf1ca); break;
    case 0xf1c0: jmp_abs_native(c, 0xf27a); break;
    case 0xf1c3: cmp_constant8(c, 0xff); break;
    case 0xf1c5: branch_to(c, !c->z, 0xf1ca); break;
    case 0xf1c7: jmp_abs_native(c, 0xf27a); break;
    case 0xf1ca: ldx_dp16(c, 0x10); break;
    case 0xf1cc: byte_constant(c, 0x17); break;
    case 0xf1ce: store_dp(c, 0x14, true); break;
    case 0xf1d0: sep_rep_native(c, 0x20, false); break;
    case 0xf1d2: lda_long_x16(c, 0x178018); break;
    case 0xf1d6: store_dp(c, 0x12, false); break;
    case 0xf1d8: cpx16_constant(c, 0x000e); break;
    case 0xf1db: branch_to(c, !c->z, 0xf1ea); break;
    case 0xf1dd: cpy16_constant(c, 0x1440); break;
    case 0xf1e0: branch_to(c, c->z, 0xf1ea); break;
    case 0xf1e2: load_dp16_to_a(c, 0x12); break;
    case 0xf1e4: implied(c); c->c = false; break;
    case 0xf1e5: adc16_constant(c, 0x000c); break;
    case 0xf1e8: store_dp(c, 0x12, false); break;
    case 0xf1ea: lda_indirect_long16(c, 0x12); break;
    case 0xf1ec: asl_a16(c); break;
    case 0xf1ed: asl_a16(c); break;
    case 0xf1ee: asl_a16(c); break;
    case 0xf1ef: store_dp(c, 0x15, false); break;
    case 0xf1f1: lda16_constant(c, 0xcac0); break;
    case 0xf1f4: jsr_abs_native(c, 0xf386); break;
    case 0xf1f7: lsr_a16(c); break;
    case 0xf1f8: implied(c); c->c = true; break;
    case 0xf1f9: sbc_dp16(c, 0x15); break;
    case 0xf1fb: tay16(c); break;
    case 0xf1fc: sep_rep_native(c, 0x20, true); break;
    case 0xf1fe: video_load(c, 0x17dc, false); break;
    case 0xf201: lsr_a8(c); break;
    case 0xf202: branch_to(c, !c->c, 0xf20d); break;
    case 0xf204: sep_rep_native(c, 0x20, false); break;
    case 0xf206: tya16(c); break;
    case 0xf207: and_constant16(c, 0xfbff); break;
    case 0xf20a: tay16(c); break;
    case 0xf20b: sep_rep_native(c, 0x20, true); break;
    case 0xf20d: sty_dp16(c, 0x00); break;
    case 0xf20f: jsr_abs_native(c, 0xb6b2); break;
    case 0xf212: jsr_abs_native(c, 0xf353); break;
    case 0xf215: branch_to(c, c->z, 0xf21d); break;
    case 0xf217: load_dp8(c, 0x01); break;
    case 0xf219: eor_constant8(c, 0x04); break;
    case 0xf21b: store_dp(c, 0x01, true); break;
    case 0xf21d: jsl_to(c, 0x8711); break;
    case 0xf221: byte_constant(c, 0x00); break;
    case 0xf223: xba_native(c); break;
    case 0xf224: video_load(c, 0x17db, false); break;
    case 0xf227: lsr_a8(c); break;
    case 0xf228: lsr_a8(c); break;
    case 0xf229: lsr_a8(c); break;
    case 0xf22a: and_constant8(c, 0xfe); break;
    case 0xf22c: sep_rep_native(c, 0x20, false); break;
    case 0xf22e: ora_dp16(c, 0x00); break;
    case 0xf230: store_a_abs_x16(c, 0x0801); break;
    case 0xf233: inc_a16(c); break;
    case 0xf234: store_a_abs_x16(c, 0x0809); break;
    case 0xf237: lda_indirect_long16(c, 0x12); break;
    case 0xf239: store_a_abs_x16(c, 0x0806); break;
    case 0xf23c: store_a_abs_x16(c, 0x080e); break;
    case 0xf23f: sep_rep_native(c, 0x20, true); break;
    case 0xf241: jsr_abs_native(c, 0xf364); break;
    case 0xf244: sty_dp16(c, 0x18); break;
    case 0xf246: jsr_abs_native(c, 0xf364); break;
    case 0xf249: sty_dp16(c, 0x1a); break;
    case 0xf24b: sep_rep_native(c, 0x20, false); break;
    case 0xf24d: load_dp16_to_a(c, 0x12); break;
    case 0xf24f: implied(c); c->c = false; break;
    case 0xf250: adc16_constant(c, 0x0004); break;
    case 0xf253: implied(c); c->c = false; break;
    case 0xf254: adc_dp16(c, 0x18); break;
    case 0xf256: store_a_abs_x16(c, 0x0803); break;
    case 0xf259: load_dp16_to_a(c, 0x12); break;
    case 0xf25b: implied(c); c->c = false; break;
    case 0xf25c: adc16_constant(c, 0x0004); break;
    case 0xf25f: implied(c); c->c = false; break;
    case 0xf260: adc_dp16(c, 0x1a); break;
    case 0xf262: store_a_abs_x16(c, 0x080b); break;
    case 0xf265: sep_rep_native(c, 0x20, true); break;
    case 0xf267: byte_constant(c, 0x17); break;
    case 0xf269: store_a_abs_x8(c, 0x0805); break;
    case 0xf26c: store_a_abs_x8(c, 0x080d); break;
    case 0xf26f: byte_constant(c, 0x81); break;
    case 0xf271: store_a_abs_x8(c, 0x0800); break;
    case 0xf274: store_a_abs_x8(c, 0x0808); break;
    case 0xf277: stz_abs_x8_only(c, 0x0810); break;
    case 0xf27a: load_dp8(c, 0xb0); break;
    case 0xf27c: and_constant8(c, 0xfd); break;
    case 0xf27e: store_dp(c, 0xb0, true); break;
    case 0xf280: jsr_abs_native(c, 0xb6b2); break;
    case 0xf283: load_a_abs_y8(c, 0x0021); break;
    case 0xf286: cmp_constant8(c, 0x02); break;
    case 0xf288: branch_to(c, !c->z, 0xf29b); break;
    case 0xf28a: jsr_abs_native(c, 0xf548); break;
    case 0xf28d: sep_rep_native(c, 0x20, false); break;
    case 0xf28f: lda16_constant(c, 0x0080); break;
    case 0xf292: implied(c); c->c = true; break;
    case 0xf293: sbc_abs_y16(c, 0x0024); break;
    case 0xf296: store_a_abs16(c, 0x17db); break;
    case 0xf299: sep_rep_native(c, 0x20, true); break;
    case 0xf29b: return_short(c); break;
    case 0xf29c: jsr_abs_native(c, 0xf998); break;
    case 0xf29f: ldx_dp16(c, 0x10); break;
    case 0xf2a1: byte_constant(c, 0x17); break;
    case 0xf2a3: store_dp(c, 0x14, true); break;
    case 0xf2a5: sep_rep_native(c, 0x20, false); break;
    case 0xf2a7: lda_long_x16(c, 0x178000); break;
    case 0xf2ab: store_dp(c, 0x12, false); break;
    case 0xf2ad: lda_indirect_long16(c, 0x12); break;
    case 0xf2af: jsr_abs_native(c, 0xf386); break;
    case 0xf2b2: index_constant(c, true, 0x0002); break;
    case 0xf2b5: tax16(c); break;
    case 0xf2b6: stx_dp16(c, 0x10); break;
    case 0xf2b8: lda_indirect_long_y16(c, 0x12); break;
    case 0xf2ba: cmp_constant16(c, 0xffff); break;
    case 0xf2bd: branch_to(c, c->z, 0xf2d5); break;
    case 0xf2bf: store_a_long_x16(c, 0x7f0000); break;
    case 0xf2c3: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xf2c4: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xf2c5: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xf2c6: implied(c); c->x++; zn(c, c->x, false); break;
    case 0xf2c7: txa16(c); break;
    case 0xf2c8: and_constant16(c, 0x003f); break;
    case 0xf2cb: branch_to(c, !c->z, 0xf2b8); break;
    case 0xf2cd: load_dp16_to_a(c, 0x10); break;
    case 0xf2cf: implied(c); c->c = false; break;
    case 0xf2d0: adc16_constant(c, 0x0040); break;
    case 0xf2d3: branch_to(c, true, 0xf2b5); break;
    case 0xf2d5: sep_rep_native(c, 0x20, true); break;
    case 0xf2d7: jsr_abs_native(c, 0xb6b2); break;
    case 0xf2da: byte_constant(c, 0x00); break;
    case 0xf2dc: store_a_abs_y8(c, 0x0021); break;
    case 0xf2df: jsr_abs_native(c, 0xf353); break;
    case 0xf2e2: branch_to(c, c->z, 0xf2e7); break;
    case 0xf2e4: jsr_abs_native(c, 0xf952); break;
    case 0xf2e7: jsr_abs_native(c, 0xf91e); break;
    case 0xf2ea: index_constant(c, true, 0x6500); break;
    case 0xf2ed: store_y_abs16(c, 0x0809); break;
    case 0xf2f0: index_constant(c, true, 0x0800); break;
    case 0xf2f3: store_y_abs16(c, 0x080b); break;
    case 0xf2f6: index_constant(c, true, 0x0280); break;
    case 0xf2f9: store_y_abs16(c, 0x080e); break;
    case 0xf2fc: byte_constant(c, 0x7f); break;
    case 0xf2fe: store_register8(c, 0x080d); break;
    case 0xf301: byte_constant(c, 0x80); break;
    case 0xf303: store_register8(c, 0x0808); break;
    case 0xf306: stz_abs8(c, 0x0810); break;
    case 0xf309: index_constant(c, false, 0x0100); break;
    case 0xf30c: stx_abs16(c, 0x17db); break;
    case 0xf30f: index_constant(c, false, 0xfff8); break;
    case 0xf312: stx_abs16(c, 0x17dd); break;
    case 0xf315: load_dp8(c, 0xb0); break;
    case 0xf317: and_constant8(c, 0xfd); break;
    case 0xf319: store_dp(c, 0xb0, true); break;
    case 0xf31b: byte_constant(c, 0x61); break;
    case 0xf31d: store_register8(c, 0x2108); break;
    case 0xf320: inc_abs8_native(c, 0x1546); break;
    case 0xf323: jsr_abs_native(c, 0xb6b2); break;
    case 0xf326: byte_constant(c, 0x02); break;
    case 0xf328: store_a_abs_y8(c, 0x0021); break;
    case 0xf32b: sep_rep_native(c, 0x20, false); break;
    case 0xf32d: lda16_constant(c, 0x0100); break;
    case 0xf330: store_a_abs_y16(c, 0x0026); break;
    case 0xf333: lda_indirect_long16(c, 0x12); break;
    case 0xf335: jsr_abs_native(c, 0xf386); break;
    case 0xf338: and_constant16(c, 0x003f); break;
    case 0xf33b: asl_a16(c); break;
    case 0xf33c: asl_a16(c); break;
    case 0xf33d: sep_rep_native(c, 0x20, true); break;
    case 0xf33f: jsr_abs_native(c, 0xf353); break;
    case 0xf342: sep_rep_native(c, 0x20, false); break;
    case 0xf344: branch_to(c, c->z, 0xf34d); break;
    case 0xf346: eor_constant16(c, 0xffff); break;
    case 0xf349: implied(c); c->c = false; break;
    case 0xf34a: adc16_constant(c, 0x0101); break;
    case 0xf34d: store_a_abs_y16(c, 0x0024); break;
    case 0xf350: sep_rep_native(c, 0x20, true); break;
    case 0xf352: return_short(c); break;
  }
  return true;
}


/* ==== Batch: CAFA D274 / D306 / D330 ==== */

static void cmp_dp16(Cpu *c, uint8_t offset) {
  uint16_t address = dp_address(c, offset);
  uint16_t value = read16(c, address);
  c->c = c->a >= value; zn(c, (uint16_t)(c->a - value), false);
}

/* $05:D274–$D305: ($CAFA,8E) face-slot swap — copy $0020 via $B6C0/$B6B2, ORA #$80,
 * JSR $C07B, snapshot $15F1..$15F9, swap $1400↔$1440 (X=#$000D..0), reseed $1401/#$31,
 * $141B/#$04, flag bits, STY #$000A→$1403, clear $0C37/$0C36/$01EE; LDA #0 / JMP $DD51. */
static bool actor_op_d274_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xd274: case 0xd277: case 0xd27a: case 0xd27b: case 0xd27e: case 0xd27f:
    case 0xd282: case 0xd285: case 0xd287: case 0xd28a: case 0xd28d: case 0xd28f:
    case 0xd292: case 0xd295: case 0xd298: case 0xd29b: case 0xd29e: case 0xd2a1:
    case 0xd2a4: case 0xd2a7: case 0xd2aa: case 0xd2ad: case 0xd2b0: case 0xd2b3:
    case 0xd2b6: case 0xd2b9: case 0xd2bc: case 0xd2bd: case 0xd2c0: case 0xd2c3:
    case 0xd2c4: case 0xd2c7: case 0xd2c8: case 0xd2ca: case 0xd2cd: case 0xd2d0:
    case 0xd2d2: case 0xd2d5: case 0xd2d7: case 0xd2da: case 0xd2dd: case 0xd2df:
    case 0xd2e2: case 0xd2e5: case 0xd2e7: case 0xd2ea: case 0xd2ed: case 0xd2ef:
    case 0xd2f2: case 0xd2f5: case 0xd2f8: case 0xd2fb: case 0xd2fe: case 0xd301:
    case 0xd303:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xd274: jsr_abs_native(c, 0xb6c0); break;
    case 0xd277: load_a_abs_y8(c, 0x0020); break;
    case 0xd27a: pha8(c); break;
    case 0xd27b: jsr_abs_native(c, 0xb6b2); break;
    case 0xd27e: pla8(c); break;
    case 0xd27f: store_a_abs_y8(c, 0x0020); break;
    case 0xd282: load_a_abs_y8(c, 0x0021); break;
    case 0xd285: ora_constant8(c, 0x80); break;
    case 0xd287: store_a_abs_y8(c, 0x0021); break;
    case 0xd28a: jsr_abs_native(c, 0xc07b); break;
    case 0xd28d: byte_constant(c, 0x02); break;
    case 0xd28f: store_register8(c, 0x15f1); break;
    case 0xd292: ldx_abs16(c, 0x14c3); break;
    case 0xd295: stx_abs16(c, 0x15f2); break;
    case 0xd298: ldx_abs16(c, 0x14c5); break;
    case 0xd29b: stx_abs16(c, 0x15f4); break;
    case 0xd29e: video_load(c, 0x1401, false); break;
    case 0xd2a1: store_register8(c, 0x15f6); break;
    case 0xd2a4: video_load(c, 0x1441, false); break;
    case 0xd2a7: store_register8(c, 0x15f7); break;
    case 0xd2aa: video_load(c, 0x141b, false); break;
    case 0xd2ad: store_register8(c, 0x15f8); break;
    case 0xd2b0: video_load(c, 0x145b, false); break;
    case 0xd2b3: store_register8(c, 0x15f9); break;
    case 0xd2b6: index_constant(c, false, 0x000d); break;
    case 0xd2b9: load_a_abs_x8(c, 0x1400); break;
    case 0xd2bc: pha8(c); break;
    case 0xd2bd: load_a_abs_x8(c, 0x1440); break;
    case 0xd2c0: store_a_abs_x8(c, 0x1400); break;
    case 0xd2c3: pla8(c); break;
    case 0xd2c4: store_a_abs_x8(c, 0x1440); break;
    case 0xd2c7: implied(c); c->x--; zn(c, c->x, false); break;
    case 0xd2c8: branch_to(c, !c->n, 0xd2b9); break;
    case 0xd2ca: video_load(c, 0x141b, false); break;
    case 0xd2cd: store_register8(c, 0x145b); break;
    case 0xd2d0: byte_constant(c, 0x31); break;
    case 0xd2d2: store_register8(c, 0x1401); break;
    case 0xd2d5: byte_constant(c, 0x04); break;
    case 0xd2d7: store_register8(c, 0x141b); break;
    case 0xd2da: video_load(c, 0x1400, false); break;
    case 0xd2dd: ora_constant8(c, 0x80); break;
    case 0xd2df: store_register8(c, 0x1400); break;
    case 0xd2e2: video_load(c, 0x1440, false); break;
    case 0xd2e5: and_constant8(c, 0x7f); break;
    case 0xd2e7: store_register8(c, 0x1440); break;
    case 0xd2ea: video_load(c, 0x1418, false); break;
    case 0xd2ed: ora_constant8(c, 0x70); break;
    case 0xd2ef: store_register8(c, 0x1418); break;
    case 0xd2f2: index_constant(c, true, 0x000a); break;
    case 0xd2f5: store_y_abs16(c, 0x1403); break;
    case 0xd2f8: stz_abs8(c, 0x0c37); break;
    case 0xd2fb: stz_abs8(c, 0x0c36); break;
    case 0xd2fe: stz_abs8(c, 0x01ee); break;
    case 0xd301: byte_constant(c, 0x00); break;
    case 0xd303: jmp_abs_native(c, 0xdd51); break;
  }
  return true;
}

/* $05:D306–$D32F: ($CAFA,7C) DP+$05=#$02 / JSR $DBF3; tier DP+$04 from $01EF
 * thresholds #$04/#$0A/#$10; STA $1540; JSR $F85E/$E073; JMP $DC48. */
static bool actor_op_d306_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xd306: case 0xd308: case 0xd30a: case 0xd30d: case 0xd310: case 0xd312:
    case 0xd314: case 0xd316: case 0xd318: case 0xd31a: case 0xd31c: case 0xd31e:
    case 0xd320: case 0xd322: case 0xd324: case 0xd327: case 0xd32a: case 0xd32d:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xd306: byte_constant(c, 0x02); break;
    case 0xd308: store_dp(c, 0x05, true); break;
    case 0xd30a: jsr_abs_native(c, 0xdbf3); break;
    case 0xd30d: video_load(c, 0x01ef, false); break;
    case 0xd310: cmp_constant8(c, 0x04); break;
    case 0xd312: branch_to(c, !c->c, 0xd322); break;
    case 0xd314: inc_dp8(c, 0x04); break;
    case 0xd316: cmp_constant8(c, 0x0a); break;
    case 0xd318: branch_to(c, !c->c, 0xd322); break;
    case 0xd31a: inc_dp8(c, 0x04); break;
    case 0xd31c: cmp_constant8(c, 0x10); break;
    case 0xd31e: branch_to(c, !c->c, 0xd322); break;
    case 0xd320: inc_dp8(c, 0x04); break;
    case 0xd322: load_dp8(c, 0x04); break;
    case 0xd324: store_register8(c, 0x1540); break;
    case 0xd327: jsr_abs_native(c, 0xf85e); break;
    case 0xd32a: jsr_abs_native(c, 0xe073); break;
    case 0xd32d: jmp_abs_native(c, 0xdc48); break;
  }
  return true;
}

/* $05:D330–$D3D5: ($CAFA,7A)/(,8C@$D3C1) — backup type at $003F,Y; scan $0B00.. for
 * bit6 actors (skip types #$61/#$54); pick max $0B03 → copy type/coords/scale into Y
 * slot; ORA #$80 / $C07B; $B11D → JSL $1D8583/$859B; LDA #0 / JMP $DD51. */
static bool actor_op_d330_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 =
      (pc == 0xd340 || pc == 0xd343 || pc == 0xd346 || pc == 0xd348 || pc == 0xd34b ||
       pc == 0xd34e || pc == 0xd351 || pc == 0xd353 || pc == 0xd356 || pc == 0xd358 ||
       pc == 0xd35b || pc == 0xd35d || pc == 0xd35f || pc == 0xd361 || pc == 0xd363 ||
       pc == 0xd364 || pc == 0xd365 || pc == 0xd368 || pc == 0xd369 || pc == 0xd36c ||
       pc == 0xd37d || pc == 0xd380 || pc == 0xd383 || pc == 0xd386 || pc == 0xd389 ||
       pc == 0xd38c || pc == 0xd38e || pc == 0xd3a5 || pc == 0xd3a7 || pc == 0xd3a8 ||
       pc == 0xd3aa);
  const bool sep_entry = (pc == 0xd36e || pc == 0xd390 || pc == 0xd3ad);
  const bool rep_entry = (pc == 0xd33e || pc == 0xd37b || pc == 0xd3a3);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xd330: case 0xd333: case 0xd336: case 0xd339: case 0xd33c: case 0xd33e:
    case 0xd340: case 0xd343: case 0xd346: case 0xd348: case 0xd34b: case 0xd34e:
    case 0xd351: case 0xd353: case 0xd356: case 0xd358: case 0xd35b: case 0xd35d:
    case 0xd35f: case 0xd361: case 0xd363: case 0xd364: case 0xd365: case 0xd368:
    case 0xd369: case 0xd36c: case 0xd36e: case 0xd370: case 0xd372: case 0xd375:
    case 0xd378: case 0xd37b: case 0xd37d: case 0xd380: case 0xd383: case 0xd386:
    case 0xd389: case 0xd38c: case 0xd38e: case 0xd390: case 0xd392: case 0xd395:
    case 0xd397: case 0xd398: case 0xd39a: case 0xd39c: case 0xd39d: case 0xd39f:
    case 0xd3a1: case 0xd3a3: case 0xd3a5: case 0xd3a7: case 0xd3a8: case 0xd3aa:
    case 0xd3ad: case 0xd3af: case 0xd3b1: case 0xd3b3: case 0xd3b6: case 0xd3b9:
    case 0xd3bb: case 0xd3be: case 0xd3c1: case 0xd3c4: case 0xd3c5: case 0xd3c7:
    case 0xd3cb: case 0xd3cd: case 0xd3d1: case 0xd3d3:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xd330: jsr_abs_native(c, 0xb6b2); break;
    case 0xd333: load_a_abs_y8(c, 0x0001); break;
    case 0xd336: store_a_abs_y8(c, 0x003f); break;
    case 0xd339: index_constant(c, false, 0x0000); break;
    case 0xd33c: stx_dp16(c, 0x00); break;
    case 0xd33e: sep_rep_native(c, 0x20, false); break;
    case 0xd340: video_load(c, 0x0b00, true); break;
    case 0xd343: and_constant16(c, 0x0040); break;
    case 0xd346: branch_to(c, c->z, 0xd363); break;
    case 0xd348: video_load(c, 0x0b01, true); break;
    case 0xd34b: and_constant16(c, 0x00ff); break;
    case 0xd34e: cmp_constant16(c, 0x0061); break;
    case 0xd351: branch_to(c, c->z, 0xd363); break;
    case 0xd353: cmp_constant16(c, 0x0054); break;
    case 0xd356: branch_to(c, c->z, 0xd363); break;
    case 0xd358: video_load(c, 0x0b03, true); break;
    case 0xd35b: cmp_dp16(c, 0x00); break;
    case 0xd35d: branch_to(c, !c->c, 0xd363); break;
    case 0xd35f: store_dp(c, 0x00, false); break;
    case 0xd361: stx_dp16(c, 0x02); break;
    case 0xd363: txa16(c); break;
    case 0xd364: implied(c); c->c = false; break;
    case 0xd365: adc16_constant(c, 0x0020); break;
    case 0xd368: tax16(c); break;
    case 0xd369: cpx16_constant(c, 0x0120); break;
    case 0xd36c: branch_to(c, !c->c, 0xd340); break;
    case 0xd36e: sep_rep_native(c, 0x20, true); break;
    case 0xd370: ldx_dp16(c, 0x02); break;
    case 0xd372: load_a_abs_x8(c, 0x0b01); break;
    case 0xd375: store_a_abs_y8(c, 0x0001); break;
    case 0xd378: store_a_abs_y8(c, 0x0020); break;
    case 0xd37b: sep_rep_native(c, 0x20, false); break;
    case 0xd37d: video_load(c, 0x0b03, true); break;
    case 0xd380: store_a_abs_y16(c, 0x0003); break;
    case 0xd383: video_load(c, 0x0b0a, true); break;
    case 0xd386: store_a_abs_y16(c, 0x000a); break;
    case 0xd389: video_load(c, 0x0b07, true); break;
    case 0xd38c: store_dp(c, 0x10, false); break;
    case 0xd38e: store_dp(c, 0x14, false); break;
    case 0xd390: sep_rep_native(c, 0x20, true); break;
    case 0xd392: load_a_abs_x8(c, 0x0b09); break;
    case 0xd395: store_dp(c, 0x16, true); break;
    case 0xd397: lsr_a8(c); break;
    case 0xd398: ror_dp8(c, 0x11); break;
    case 0xd39a: ror_dp8(c, 0x10); break;
    case 0xd39c: lsr_a8(c); break;
    case 0xd39d: ror_dp8(c, 0x11); break;
    case 0xd39f: ror_dp8(c, 0x10); break;
    case 0xd3a1: store_dp(c, 0x12, true); break;
    case 0xd3a3: sep_rep_native(c, 0x20, false); break;
    case 0xd3a5: load_dp16_to_a(c, 0x14); break;
    case 0xd3a7: implied(c); c->c = true; break;
    case 0xd3a8: sbc_dp16(c, 0x10); break;
    case 0xd3aa: store_a_abs_y16(c, 0x0007); break;
    case 0xd3ad: sep_rep_native(c, 0x20, true); break;
    case 0xd3af: load_dp8(c, 0x16); break;
    case 0xd3b1: sbc_dp8(c, 0x12); break;
    case 0xd3b3: store_a_abs_y8(c, 0x0009); break;
    case 0xd3b6: load_a_abs_y8(c, 0x0021); break;
    case 0xd3b9: ora_constant8(c, 0x80); break;
    case 0xd3bb: store_a_abs_y8(c, 0x0021); break;
    case 0xd3be: jsr_abs_native(c, 0xc07b); break;
    case 0xd3c1: jsr_abs_native(c, 0xb11d); break;
    case 0xd3c4: lsr_a8(c); break;
    case 0xd3c5: branch_to(c, !c->c, 0xd3cd); break;
    case 0xd3c7: jsl_to(c, 0x8583); break;
    case 0xd3cb: branch_to(c, true, 0xd3d1); break;
    case 0xd3cd: jsl_to(c, 0x859b); break;
    case 0xd3d1: byte_constant(c, 0x00); break;
    case 0xd3d3: jmp_abs_native(c, 0xdd51); break;
  }
  return true;
}
/* ==== Batch: CAFA DB2B–DD50 + helpers DDD1/E018/B594 + D3D6–D484 ==== */

static void adc_abs_y16(Cpu *c, uint16_t base) {
  (void)fetch(c); (void)fetch(c); idle(c);
  uint32_t low = (((uint32_t)c->db << 16) + base + c->y) & 0xffffffu;
  uint16_t value = c->read(c->mem, low); sample_interrupt(c);
  value |= (uint16_t)c->read(c->mem, (low + 1) & 0xffffffu) << 8;
  uint16_t old = c->a;
  unsigned sum = (unsigned)old + value + (c->c ? 1u : 0u);
  uint16_t outv = (uint16_t)sum;
  c->v = ((~(old ^ value) & (old ^ outv)) & 0x8000u) != 0;
  c->c = sum > 0xffffu;
  c->a = outv; zn(c, outv, false);
}


/* $05:DB2B–$DC3A: ($CAFA,42..) $DB2B/$DB65/$DB6F/$DB8E/$DB99/$DBB0/$DBC6 +
 * helpers $DBF3/$DBFA/$DC1E — scroll seed, face remap, tier helpers. */
static bool actor_op_db2b_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xdb34 || pc == 0xdb37 || pc == 0xdb38 || pc == 0xdb3b || pc == 0xdb3e || pc == 0xdb41 || pc == 0xdb44 || pc == 0xdb46 || pc == 0xdb49 || pc == 0xdb4a || pc == 0xdb4d || pc == 0xdb4f || pc == 0xdb52 || pc == 0xdb53 || pc == 0xdb56 || pc == 0xdbd6 || pc == 0xdbd9 || pc == 0xdbda || pc == 0xdbdc || pc == 0xdbdf || pc == 0xdbe2 || pc == 0xdbe5 || pc == 0xdbe7 || pc == 0xdbe9);
  if(m16 && c->mf) return false;
  const bool sep_entry = (pc == 0xdb59 || pc == 0xdbec);
  if(sep_entry && c->mf) return false;
  const bool rep_entry = (pc == 0xdb32 || pc == 0xdbd4);
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xdb2b: case 0xdb2d: case 0xdb30: case 0xdb32: case 0xdb34: case 0xdb37:
    case 0xdb38: case 0xdb3b: case 0xdb3e: case 0xdb41: case 0xdb44: case 0xdb46:
    case 0xdb49: case 0xdb4a: case 0xdb4d: case 0xdb4f: case 0xdb52: case 0xdb53:
    case 0xdb56: case 0xdb59: case 0xdb5b: case 0xdb5d: case 0xdb60: case 0xdb62:
    case 0xdb65: case 0xdb67: case 0xdb6a: case 0xdb6c: case 0xdb6f: case 0xdb71:
    case 0xdb74: case 0xdb77: case 0xdb7a: case 0xdb7b: case 0xdb7c: case 0xdb7e:
    case 0xdb7f: case 0xdb81: case 0xdb84: case 0xdb86: case 0xdb89: case 0xdb8c:
    case 0xdb8e: case 0xdb91: case 0xdb93: case 0xdb94: case 0xdb96: case 0xdb99:
    case 0xdb9c: case 0xdb9f: case 0xdba2: case 0xdba5: case 0xdba8: case 0xdbab:
    case 0xdbad: case 0xdbb0: case 0xdbb3: case 0xdbb6: case 0xdbb9: case 0xdbbb:
    case 0xdbbe: case 0xdbc1: case 0xdbc3: case 0xdbc6: case 0xdbc8: case 0xdbc9:
    case 0xdbcc: case 0xdbcf: case 0xdbd0: case 0xdbd2: case 0xdbd4: case 0xdbd6:
    case 0xdbd9: case 0xdbda: case 0xdbdc: case 0xdbdf: case 0xdbe2: case 0xdbe5:
    case 0xdbe7: case 0xdbe9: case 0xdbec: case 0xdbee: case 0xdbf0: case 0xdbf3:
    case 0xdbf5: case 0xdbf7: case 0xdbfa: case 0xdbfc: case 0xdbfe: case 0xdbff:
    case 0xdc02: case 0xdc04: case 0xdc06: case 0xdc08: case 0xdc0b: case 0xdc0d:
    case 0xdc0f: case 0xdc12: case 0xdc15: case 0xdc17: case 0xdc1a: case 0xdc1d:
    case 0xdc1e: case 0xdc20: case 0xdc22: case 0xdc25: case 0xdc27: case 0xdc29:
    case 0xdc2b: case 0xdc2d: case 0xdc2f: case 0xdc32: case 0xdc34: case 0xdc36:
    case 0xdc38: case 0xdc3a:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xdb2b: byte_constant(c, 0x82); break;
    case 0xdb2d: store_register8(c, 0x1552); break;
    case 0xdb30: ldy_dp16(c, 0x20); break;
    case 0xdb32: sep_rep_native(c, 0x20, false); break;
    case 0xdb34: load_a_abs_y16(c, 0x0026); break;
    case 0xdb37: implied(c); c->c = true; break;
    case 0xdb38: sbc16_constant(c, 0x0020); break;
    case 0xdb3b: video_store(c, 0x1556, false, c->a, false); break;
    case 0xdb3e: load_a_abs_y16(c, 0x0032); break;
    case 0xdb41: and_constant16(c, 0x0040); break;
    case 0xdb44: branch_to(c, !c->z, 0xdb4f); break;
    case 0xdb46: load_a_abs_y16(c, 0x0024); break;
    case 0xdb49: implied(c); c->c = false; break;
    case 0xdb4a: adc16_constant(c, 0x0010); break;
    case 0xdb4d: branch_to(c, true, 0xdb56); break;
    case 0xdb4f: load_a_abs_y16(c, 0x0024); break;
    case 0xdb52: implied(c); c->c = true; break;
    case 0xdb53: sbc16_constant(c, 0x0010); break;
    case 0xdb56: video_store(c, 0x1554, false, c->a, false); break;
    case 0xdb59: sep_rep_native(c, 0x20, true); break;
    case 0xdb5b: byte_constant(c, 0x10); break;
    case 0xdb5d: store_register8(c, 0x1553); break;
    case 0xdb60: byte_constant(c, 0x00); break;
    case 0xdb62: jmp_abs_native(c, 0xca8d); break;
    case 0xdb65: load_dp8(c, 0x04); break;
    case 0xdb67: jsr_abs_native(c, 0xdf60); break;
    case 0xdb6a: byte_constant(c, 0x01); break;
    case 0xdb6c: jmp_abs_native(c, 0xca8d); break;
    case 0xdb6f: byte_constant(c, 0x80); break;
    case 0xdb71: store_register8(c, 0x1546); break;
    case 0xdb74: stz_abs8(c, 0x1547); break;
    case 0xdb77: video_load(c, 0x01e7, false); break;
    case 0xdb7a: lsr_a8(c); break;
    case 0xdb7b: lsr_a8(c); break;
    case 0xdb7c: and_constant8(c, 0x01); break;
    case 0xdb7e: implied(c); c->c = false; break;
    case 0xdb7f: adc8_constant(c, 0x1f); break;
    case 0xdb81: jsr_abs_native(c, 0x9621); break;
    case 0xdb84: byte_constant(c, 0x00); break;
    case 0xdb86: jmp_abs_native(c, 0xca8d); break;
    case 0xdb89: video_load(c, 0x1551, false); break;
    case 0xdb8c: branch_to(c, true, 0xdb91); break;
    case 0xdb8e: video_load(c, 0x1546, false); break;
    case 0xdb91: branch_to(c, !c->n, 0xdb94); break;
    case 0xdb93: return_short(c); break;
    case 0xdb94: byte_constant(c, 0x00); break;
    case 0xdb96: jmp_abs_native(c, 0xca8d); break;
    case 0xdb99: jsr_abs_native(c, 0xdbf3); break;
    case 0xdb9c: jsr_abs_native(c, 0xddf4); break;
    case 0xdb9f: jsr_abs_native(c, 0xdc1e); break;
    case 0xdba2: store_register8(c, 0x1540); break;
    case 0xdba5: jsr_abs_native(c, 0xf85e); break;
    case 0xdba8: jsr_abs_native(c, 0xe073); break;
    case 0xdbab: byte_constant(c, 0x02); break;
    case 0xdbad: jmp_abs_native(c, 0xca8d); break;
    case 0xdbb0: jsr_abs_native(c, 0xdbf3); break;
    case 0xdbb3: jsr_abs_native(c, 0xddf4); break;
    case 0xdbb6: jsr_abs_native(c, 0xdc1e); break;
    case 0xdbb9: load_dp8(c, 0x04); break;
    case 0xdbbb: jsr_abs_native(c, 0xe018); break;
    case 0xdbbe: stz_abs8(c, 0x14fe); break;
    case 0xdbc1: byte_constant(c, 0x02); break;
    case 0xdbc3: jmp_abs_native(c, 0xdd51); break;
    case 0xdbc6: load_dp8(c, 0x04); break;
    case 0xdbc8: pha8(c); break;
    case 0xdbc9: video_load(c, 0x1540, false); break;
    case 0xdbcc: jsr_abs_native(c, 0xe01e); break;
    case 0xdbcf: pla8(c); break;
    case 0xdbd0: store_dp(c, 0x01, true); break;
    case 0xdbd2: stz_dp8(c, 0x00); break;
    case 0xdbd4: sep_rep_native(c, 0x20, false); break;
    case 0xdbd6: video_load(c, 0x0801, false); break;
    case 0xdbd9: implied(c); c->c = false; break;
    case 0xdbda: adc_dp16(c, 0x00); break;
    case 0xdbdc: video_store(c, 0x0801, false, c->a, false); break;
    case 0xdbdf: lda16_constant(c, 0x0800); break;
    case 0xdbe2: video_store(c, 0x0806, false, c->a, false); break;
    case 0xdbe5: asl_dp16(c, 0x00); break;
    case 0xdbe7: load_dp16_to_a(c, 0x00); break;
    case 0xdbe9: video_store(c, 0x0803, false, c->a, false); break;
    case 0xdbec: sep_rep_native(c, 0x20, true); break;
    case 0xdbee: byte_constant(c, 0x01); break;
    case 0xdbf0: jmp_abs_native(c, 0xdd51); break;
    case 0xdbf3: load_dp8(c, 0x05); break;
    case 0xdbf5: and_constant8(c, 0x7f); break;
    case 0xdbf7: store_register8(c, 0x1541); break;
    case 0xdbfa: bit_imm8(c, 0x10); break;
    case 0xdbfc: branch_to(c, c->z, 0xdc1d); break;
    case 0xdbfe: lsr_a8(c); break;
    case 0xdbff: video_load(c, 0x14c2, false); break;
    case 0xdc02: branch_to(c, !c->c, 0xdc06); break;
    case 0xdc04: eor_constant8(c, 0x01); break;
    case 0xdc06: and_constant8(c, 0x01); break;
    case 0xdc08: index_constant(c, false, 0x0000); break;
    case 0xdc0b: bit_dp8(c, 0x05); break;
    case 0xdc0d: branch_to(c, !c->n, 0xdc12); break;
    case 0xdc0f: index_constant(c, false, 0x0008); break;
    case 0xdc12: store_a_abs_x8(c, 0x155e); break;
    case 0xdc15: byte_constant(c, 0x00); break;
    case 0xdc17: store_a_abs_x8(c, 0x1559); break;
    case 0xdc1a: store_a_abs_x8(c, 0x155f); break;
    case 0xdc1d: return_short(c); break;
    case 0xdc1e: load_dp8(c, 0x04); break;
    case 0xdc20: branch_to(c, !c->z, 0xdc2b); break;
    case 0xdc22: video_load(c, 0x01e7, false); break;
    case 0xdc25: branch_to(c, c->n, 0xdc3a); break;
    case 0xdc27: inc_dp8(c, 0x04); break;
    case 0xdc29: branch_to(c, true, 0xdc3a); break;
    case 0xdc2b: cmp_constant8(c, 0x02); break;
    case 0xdc2d: branch_to(c, !c->z, 0xdc3a); break;
    case 0xdc2f: video_load(c, 0x01e7, false); break;
    case 0xdc32: bit_imm8(c, 0x04); break;
    case 0xdc34: branch_to(c, !c->z, 0xdc3a); break;
    case 0xdc36: inc_dp8(c, 0x04); break;
    case 0xdc38: branch_to(c, true, 0xdc3a); break;
    case 0xdc3a: return_short(c); break;
  }
  return true;
}

/* $05:DC3B–$DD50: ($CAFA,50..) $DC3B/$DC48/$DC7C/$DC93/$DC9C/$DCB2/$DCC2/$DCDE/$DD29
 * + helper $DD02 — unblocks $D306→$DC48; mosaic/APU/face leaves. */
static bool actor_op_dc3b_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0xdc3b: case 0xdc3d: case 0xdc40: case 0xdc43: case 0xdc45: case 0xdc48:
    case 0xdc4a: case 0xdc4c: case 0xdc4e: case 0xdc50: case 0xdc53: case 0xdc55:
    case 0xdc57: case 0xdc5a: case 0xdc5c: case 0xdc5f: case 0xdc62: case 0xdc64:
    case 0xdc66: case 0xdc68: case 0xdc6a: case 0xdc6d: case 0xdc6f: case 0xdc71:
    case 0xdc74: case 0xdc77: case 0xdc79: case 0xdc7c: case 0xdc7e: case 0xdc80:
    case 0xdc82: case 0xdc85: case 0xdc88: case 0xdc8b: case 0xdc8e: case 0xdc90:
    case 0xdc93: case 0xdc97: case 0xdc99: case 0xdc9c: case 0xdc9f: case 0xdca1:
    case 0xdca3: case 0xdca5: case 0xdca7: case 0xdca9: case 0xdcab: case 0xdcad:
    case 0xdcaf: case 0xdcb2: case 0xdcb4: case 0xdcb5: case 0xdcb7: case 0xdcb8:
    case 0xdcba: case 0xdcbd: case 0xdcbf: case 0xdcc2: case 0xdcc4: case 0xdcc6:
    case 0xdcc8: case 0xdccb: case 0xdccd: case 0xdcd0: case 0xdcd3: case 0xdcd6:
    case 0xdcd9: case 0xdcdb: case 0xdcde: case 0xdce0: case 0xdce2: case 0xdce4:
    case 0xdce6: case 0xdce9: case 0xdcec: case 0xdcee: case 0xdcf0: case 0xdcf2:
    case 0xdcf5: case 0xdcf7: case 0xdcfa: case 0xdcfd: case 0xdcff: case 0xdd02:
    case 0xdd04: case 0xdd06: case 0xdd08: case 0xdd0a: case 0xdd0c: case 0xdd0e:
    case 0xdd10: case 0xdd12: case 0xdd14: case 0xdd16: case 0xdd18: case 0xdd1a:
    case 0xdd1c: case 0xdd1e: case 0xdd20: case 0xdd22: case 0xdd24: case 0xdd26:
    case 0xdd28: case 0xdd29: case 0xdd2b: case 0xdd2d: case 0xdd30: case 0xdd32:
    case 0xdd34: case 0xdd36: case 0xdd38: case 0xdd3a: case 0xdd3c: case 0xdd3e:
    case 0xdd40: case 0xdd43: case 0xdd45: case 0xdd47: case 0xdd48: case 0xdd4a:
    case 0xdd4c: case 0xdd4e:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xdc3b: byte_constant(c, 0x80); break;
    case 0xdc3d: store_register8(c, 0x1546); break;
    case 0xdc40: stz_abs8(c, 0x1548); break;
    case 0xdc43: byte_constant(c, 0x00); break;
    case 0xdc45: jmp_abs_native(c, 0xdd51); break;
    case 0xdc48: load_dp8(c, 0x04); break;
    case 0xdc4a: cmp_constant8(c, 0x96); break;
    case 0xdc4c: branch_to(c, !c->z, 0xdc5c); break;
    case 0xdc4e: byte_constant(c, 0x80); break;
    case 0xdc50: store_register8(c, 0x1551); break;
    case 0xdc53: byte_constant(c, 0x91); break;
    case 0xdc55: store_dp(c, 0x05, true); break;
    case 0xdc57: jsr_abs_native(c, 0xdbfa); break;
    case 0xdc5a: branch_to(c, true, 0xdc77); break;
    case 0xdc5c: jsr_abs_native(c, 0xddf4); break;
    case 0xdc5f: video_load(c, 0x1540, false); break;
    case 0xdc62: cmp_dp8(c, 0x04); break;
    case 0xdc64: branch_to(c, !c->z, 0xdc77); break;
    case 0xdc66: cmp_constant8(c, 0x05); break;
    case 0xdc68: branch_to(c, !c->z, 0xdc6f); break;
    case 0xdc6a: jsr_abs_native(c, 0xddd1); break;
    case 0xdc6d: branch_to(c, !c->c, 0xdc77); break;
    case 0xdc6f: byte_constant(c, 0x80); break;
    case 0xdc71: store_register8(c, 0x1546); break;
    case 0xdc74: stz_abs8(c, 0x1548); break;
    case 0xdc77: byte_constant(c, 0x01); break;
    case 0xdc79: jmp_abs_native(c, 0xdd51); break;
    case 0xdc7c: load_dp8(c, 0xb0); break;
    case 0xdc7e: and_constant8(c, 0xfd); break;
    case 0xdc80: store_dp(c, 0xb0, true); break;
    case 0xdc82: stz_abs8(c, 0x1546); break;
    case 0xdc85: stz_abs8(c, 0x1558); break;
    case 0xdc88: stz_abs8(c, 0x1560); break;
    case 0xdc8b: stz_abs8(c, 0x1568); break;
    case 0xdc8e: byte_constant(c, 0x00); break;
    case 0xdc90: jmp_abs_native(c, 0xca8d); break;
    case 0xdc93: jsl_to(c, 0x89df); break;
    case 0xdc97: byte_constant(c, 0x00); break;
    case 0xdc99: jmp_abs_native(c, 0xca8d); break;
    case 0xdc9c: video_load(c, 0x1540, false); break;
    case 0xdc9f: cmp_constant8(c, 0x20); break;
    case 0xdca1: branch_to(c, !c->z, 0xdca9); break;
    case 0xdca3: load_dp8(c, 0x04); break;
    case 0xdca5: and_constant8(c, 0xfd); break;
    case 0xdca7: store_dp(c, 0x04, true); break;
    case 0xdca9: load_dp8(c, 0x04); break;
    case 0xdcab: store_dp(c, 0xb0, true); break;
    case 0xdcad: byte_constant(c, 0x01); break;
    case 0xdcaf: jmp_abs_native(c, 0xca8d); break;
    case 0xdcb2: byte_constant(c, 0x00); break;
    case 0xdcb4: xba_native(c); break;
    case 0xdcb5: load_dp8(c, 0x04); break;
    case 0xdcb7: tax8(c); break;
    case 0xdcb8: load_dp8(c, 0x05); break;
    case 0xdcba: store_a_abs_x8(c, 0x2106); break;
    case 0xdcbd: byte_constant(c, 0x02); break;
    case 0xdcbf: jmp_abs_native(c, 0xca8d); break;
    case 0xdcc2: byte_constant(c, 0x10); break;
    case 0xdcc4: store_dp(c, 0xb0, true); break;
    case 0xdcc6: load_dp8(c, 0x04); break;
    case 0xdcc8: store_register8(c, 0x153d); break;
    case 0xdccb: branch_to(c, c->z, 0xdcd9); break;
    case 0xdccd: jsr_abs_native(c, 0xc9fa); break;
    case 0xdcd0: index_constant(c, true, 0xffff); break;
    case 0xdcd3: store_y_abs16(c, 0x0200); break;
    case 0xdcd6: inc_abs8_native(c, 0x0715); break;
    case 0xdcd9: byte_constant(c, 0x01); break;
    case 0xdcdb: jmp_abs_native(c, 0xca8d); break;
    case 0xdcde: load_dp8(c, 0x04); break;
    case 0xdce0: branch_to(c, !c->n, 0xdcf2); break;
    case 0xdce2: cmp_constant8(c, 0xff); break;
    case 0xdce4: branch_to(c, !c->z, 0xdcfa); break;
    case 0xdce6: video_load(c, 0x15da, false); break;
    case 0xdce9: jsr_abs_native(c, 0xdd02); break;
    case 0xdcec: branch_to(c, !c->z, 0xdcfd); break;
    case 0xdcee: ora_constant8(c, 0x80); break;
    case 0xdcf0: branch_to(c, true, 0xdcfa); break;
    case 0xdcf2: jsr_abs_native(c, 0xdd02); break;
    case 0xdcf5: branch_to(c, !c->z, 0xdcfa); break;
    case 0xdcf7: store_register8(c, 0x15da); break;
    case 0xdcfa: jsr_abs_native(c, 0x9621); break;
    case 0xdcfd: byte_constant(c, 0x01); break;
    case 0xdcff: jmp_abs_native(c, 0xca8d); break;
    case 0xdd02: cmp_constant8(c, 0x03); break;
    case 0xdd04: branch_to(c, c->z, 0xdd28); break;
    case 0xdd06: cmp_constant8(c, 0x07); break;
    case 0xdd08: branch_to(c, c->z, 0xdd28); break;
    case 0xdd0a: cmp_constant8(c, 0x0b); break;
    case 0xdd0c: branch_to(c, c->z, 0xdd28); break;
    case 0xdd0e: cmp_constant8(c, 0x0e); break;
    case 0xdd10: branch_to(c, c->z, 0xdd28); break;
    case 0xdd12: cmp_constant8(c, 0x10); break;
    case 0xdd14: branch_to(c, c->z, 0xdd28); break;
    case 0xdd16: cmp_constant8(c, 0x12); break;
    case 0xdd18: branch_to(c, c->z, 0xdd28); break;
    case 0xdd1a: cmp_constant8(c, 0x1b); break;
    case 0xdd1c: branch_to(c, c->z, 0xdd28); break;
    case 0xdd1e: cmp_constant8(c, 0x2d); break;
    case 0xdd20: branch_to(c, c->z, 0xdd28); break;
    case 0xdd22: cmp_constant8(c, 0x2e); break;
    case 0xdd24: branch_to(c, c->z, 0xdd28); break;
    case 0xdd26: cmp_constant8(c, 0x31); break;
    case 0xdd28: return_short(c); break;
    case 0xdd29: load_dp8(c, 0x04); break;
    case 0xdd2b: branch_to(c, !c->z, 0xdd34); break;
    case 0xdd2d: video_load(c, 0x14c2, false); break;
    case 0xdd30: and_constant8(c, 0x01); break;
    case 0xdd32: branch_to(c, true, 0xdd4a); break;
    case 0xdd34: cmp_constant8(c, 0x80); break;
    case 0xdd36: branch_to(c, c->z, 0xdd3c); break;
    case 0xdd38: cmp_constant8(c, 0x81); break;
    case 0xdd3a: branch_to(c, !c->z, 0xdd4a); break;
    case 0xdd3c: and_constant8(c, 0x01); break;
    case 0xdd3e: store_dp(c, 0xb6, true); break;
    case 0xdd40: video_load(c, 0x14c2, false); break;
    case 0xdd43: and_constant8(c, 0x01); break;
    case 0xdd45: eor_dp8(c, 0xb6); break;
    case 0xdd47: implied(c); c->c = false; break;
    case 0xdd48: adc8_constant(c, 0x03); break;
    case 0xdd4a: store_dp(c, 0xb6, true); break;
    case 0xdd4c: byte_constant(c, 0x01); break;
    case 0xdd4e: jmp_abs_native(c, 0xca8d); break;
  }
  return true;
}

/* $05:DDD1–$DE25: helpers $DDD1 (pair SBC $2710) + $DDF4 remap (through early RTS);
 * deep $DE26+/$DEA1 now native (separate step). */
static bool actor_helper_ddd1_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xddd3 || pc == 0xddd6 || pc == 0xddd7 || pc == 0xdde5 || pc == 0xdde8 || pc == 0xdde9);
  if(m16 && c->mf) return false;
  const bool sep_entry = (pc == 0xddda || pc == 0xddec);
  if(sep_entry && c->mf) return false;
  const bool rep_entry = (pc == 0xddd1 || pc == 0xdde3);
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xddd1: case 0xddd3: case 0xddd6: case 0xddd7: case 0xddda: case 0xdddc:
    case 0xdddf: case 0xdde1: case 0xdde3: case 0xdde5: case 0xdde8: case 0xdde9:
    case 0xddec: case 0xddee: case 0xddf1: case 0xddf3: case 0xddf4: case 0xddf6:
    case 0xddf8: case 0xddfa: case 0xddfc: case 0xddfd: case 0xddff: case 0xde01:
    case 0xde02: case 0xde04: case 0xde06: case 0xde08: case 0xde09: case 0xde0a:
    case 0xde0c: case 0xde0d: case 0xde0e: case 0xde11: case 0xde13: case 0xde15:
    case 0xde17: case 0xde19: case 0xde1b: case 0xde1e: case 0xde20: case 0xde22:
    case 0xde24: case 0xde25:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xddd1: sep_rep_native(c, 0x20, false); break;
    case 0xddd3: video_load(c, 0x1407, false); break;
    case 0xddd6: implied(c); c->c = true; break;
    case 0xddd7: sbc16_constant(c, 0x2710); break;
    case 0xddda: sep_rep_native(c, 0x20, true); break;
    case 0xdddc: video_load(c, 0x1409, false); break;
    case 0xdddf: sbc8_constant(c, 0x00); break;
    case 0xdde1: branch_to(c, !c->c, 0xddf3); break;
    case 0xdde3: sep_rep_native(c, 0x20, false); break;
    case 0xdde5: video_load(c, 0x1447, false); break;
    case 0xdde8: implied(c); c->c = true; break;
    case 0xdde9: sbc16_constant(c, 0x2710); break;
    case 0xddec: sep_rep_native(c, 0x20, true); break;
    case 0xddee: video_load(c, 0x1449, false); break;
    case 0xddf1: sbc8_constant(c, 0x00); break;
    case 0xddf3: return_short(c); break;
    case 0xddf4: load_dp8(c, 0x04); break;
    case 0xddf6: cmp_constant8(c, 0xf0); break;
    case 0xddf8: branch_to(c, !c->c, 0xde02); break;
    case 0xddfa: and_constant8(c, 0x0f); break;
    case 0xddfc: implied(c); c->c = false; break;
    case 0xddfd: adc8_constant(c, 0x07); break;
    case 0xddff: store_dp(c, 0x04, true); break;
    case 0xde01: return_short(c); break;
    case 0xde02: cmp_constant8(c, 0xe0); break;
    case 0xde04: branch_to(c, !c->c, 0xde11); break;
    case 0xde06: and_constant8(c, 0x0f); break;
    case 0xde08: asl_a8(c); break;
    case 0xde09: xba_native(c); break;
    case 0xde0a: byte_constant(c, 0x00); break;
    case 0xde0c: xba_native(c); break;
    case 0xde0d: tax8(c); break;
    case 0xde0e: jmp_abs_native(c, 0xde26); break;
    case 0xde11: cmp_constant8(c, 0x91); break;
    case 0xde13: branch_to(c, c->z, 0xde19); break;
    case 0xde15: cmp_constant8(c, 0x92); break;
    case 0xde17: branch_to(c, !c->z, 0xde25); break;
    case 0xde19: ldy_dp16(c, 0x20); break;
    case 0xde1b: load_a_abs_y8(c, 0x0001); break;
    case 0xde1e: cmp_constant8(c, 0x10); break;
    case 0xde20: branch_to(c, !c->z, 0xde24); break;
    case 0xde22: inc_dp8(c, 0x04); break;
    case 0xde24: return_short(c); break;
    case 0xde25: return_short(c); break;
  }
  return true;
}

/* $05:E018–$E088: $E018/$E01E queue seed via $E089/$B6B2; mid-entry $E073 stores
 * $1540 into $1502,X from $1541 — callee of $D306/$DB99. */
static bool actor_helper_e018_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xe036 || pc == 0xe038 || pc == 0xe03a || pc == 0xe03d || pc == 0xe040 || pc == 0xe042 || pc == 0xe044 || pc == 0xe047 || pc == 0xe04a || pc == 0xe04c || pc == 0xe04f || pc == 0xe052 || pc == 0xe054 || pc == 0xe057 || pc == 0xe059 || pc == 0xe05c || pc == 0xe05f || pc == 0xe062);
  if(m16 && c->mf) return false;
  const bool sep_entry = (pc == 0xe065);
  if(sep_entry && c->mf) return false;
  const bool rep_entry = (pc == 0xe034 || pc == 0xe038 || pc == 0xe042);
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xe018: case 0xe01b: case 0xe01e: case 0xe021: case 0xe024: case 0xe026:
    case 0xe028: case 0xe02a: case 0xe02c: case 0xe02d: case 0xe02e: case 0xe02f:
    case 0xe030: case 0xe031: case 0xe032: case 0xe034: case 0xe036: case 0xe038:
    case 0xe03a: case 0xe03d: case 0xe040: case 0xe042: case 0xe044: case 0xe047:
    case 0xe04a: case 0xe04c: case 0xe04f: case 0xe052: case 0xe054: case 0xe057:
    case 0xe059: case 0xe05c: case 0xe05f: case 0xe062: case 0xe065: case 0xe067:
    case 0xe069: case 0xe06c: case 0xe06d: case 0xe070: case 0xe073: case 0xe075:
    case 0xe076: case 0xe079: case 0xe07c: case 0xe07d: case 0xe07f: case 0xe081:
    case 0xe082: case 0xe085: case 0xe088:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe018: store_register8(c, 0x1540); break;
    case 0xe01b: jsr_abs_native(c, 0xe089); break;
    case 0xe01e: jsr_abs_native(c, 0xb6b2); break;
    case 0xe021: video_load(c, 0x1541, false); break;
    case 0xe024: cmp_constant8(c, 0x10); break;
    case 0xe026: branch_to(c, c->z, 0xe038); break;
    case 0xe028: cmp_constant8(c, 0x11); break;
    case 0xe02a: branch_to(c, c->z, 0xe042); break;
    case 0xe02c: inc_a8(c); break;
    case 0xe02d: asl_a8(c); break;
    case 0xe02e: asl_a8(c); break;
    case 0xe02f: asl_a8(c); break;
    case 0xe030: asl_a8(c); break;
    case 0xe031: xba_native(c); break;
    case 0xe032: byte_constant(c, 0x00); break;
    case 0xe034: sep_rep_native(c, 0x20, false); break;
    case 0xe036: branch_to(c, true, 0xe04f); break;
    case 0xe038: sep_rep_native(c, 0x20, false); break;
    case 0xe03a: lda16_constant(c, 0x0000); break;
    case 0xe03d: cpy16_constant(c, 0x1400); break;
    case 0xe040: branch_to(c, true, 0xe04a); break;
    case 0xe042: sep_rep_native(c, 0x20, false); break;
    case 0xe044: lda16_constant(c, 0x0000); break;
    case 0xe047: cpy16_constant(c, 0x1440); break;
    case 0xe04a: branch_to(c, c->z, 0xe04f); break;
    case 0xe04c: lda16_constant(c, 0x1000); break;
    case 0xe04f: video_store(c, 0x0801, false, c->a, false); break;
    case 0xe052: load_dp16_to_a(c, 0x89); break;
    case 0xe054: cmp_constant16(c, 0x0800); break;
    case 0xe057: branch_to(c, !c->c, 0xe05c); break;
    case 0xe059: lda16_constant(c, 0x0800); break;
    case 0xe05c: video_store(c, 0x0806, false, c->a, false); break;
    case 0xe05f: lda16_constant(c, 0x0000); break;
    case 0xe062: video_store(c, 0x0803, false, c->a, false); break;
    case 0xe065: sep_rep_native(c, 0x20, true); break;
    case 0xe067: byte_constant(c, 0x7f); break;
    case 0xe069: store_register8(c, 0x0805); break;
    case 0xe06c: inc_a8(c); break;
    case 0xe06d: store_register8(c, 0x0800); break;
    case 0xe070: stz_abs8(c, 0x0808); break;
    case 0xe073: byte_constant(c, 0x00); break;
    case 0xe075: xba_native(c); break;
    case 0xe076: index_constant(c, false, 0x0000); break;
    case 0xe079: video_load(c, 0x1541, false); break;
    case 0xe07c: dec_a8(c); break;
    case 0xe07d: cmp_constant8(c, 0x04); break;
    case 0xe07f: branch_to(c, c->c, 0xe082); break;
    case 0xe081: tax8(c); break;
    case 0xe082: video_load(c, 0x1540, false); break;
    case 0xe085: store_a_abs_x8(c, 0x1502); break;
    case 0xe088: return_short(c); break;
  }
  return true;
}

/* $05:B594–$B5C3: type remap $B594/$B5AA (PHX) — 09↔10 / 0F↔11 / 42↔4D; TXA/PLX/RTS. */
static bool type_remap_b594_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0xb594: case 0xb595: case 0xb598: case 0xb59a: case 0xb59c: case 0xb59f:
    case 0xb5a1: case 0xb5a3: case 0xb5a6: case 0xb5a8: case 0xb5aa: case 0xb5ab:
    case 0xb5ae: case 0xb5b0: case 0xb5b2: case 0xb5b5: case 0xb5b7: case 0xb5b9:
    case 0xb5bc: case 0xb5be: case 0xb5c0: case 0xb5c1: case 0xb5c2: case 0xb5c3:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xb594: phx16(c); break;
    case 0xb595: index_constant(c, false, 0x0010); break;
    case 0xb598: cmp_constant8(c, 0x09); break;
    case 0xb59a: branch_to(c, c->z, 0xb5c1); break;
    case 0xb59c: index_constant(c, false, 0x0011); break;
    case 0xb59f: cmp_constant8(c, 0x0f); break;
    case 0xb5a1: branch_to(c, c->z, 0xb5c1); break;
    case 0xb5a3: index_constant(c, false, 0x004d); break;
    case 0xb5a6: cmp_constant8(c, 0x42); break;
    case 0xb5a8: branch_to(c, true, 0xb5be); break;
    case 0xb5aa: phx16(c); break;
    case 0xb5ab: index_constant(c, false, 0x0009); break;
    case 0xb5ae: cmp_constant8(c, 0x10); break;
    case 0xb5b0: branch_to(c, c->z, 0xb5c1); break;
    case 0xb5b2: index_constant(c, false, 0x000f); break;
    case 0xb5b5: cmp_constant8(c, 0x11); break;
    case 0xb5b7: branch_to(c, c->z, 0xb5c1); break;
    case 0xb5b9: index_constant(c, false, 0x0042); break;
    case 0xb5bc: cmp_constant8(c, 0x4d); break;
    case 0xb5be: branch_to(c, c->z, 0xb5c1); break;
    case 0xb5c0: tax8(c); break;
    case 0xb5c1: implied(c); set_a8(c, (uint8_t)c->x); break;
    case 0xb5c2: plx16(c); break;
    case 0xb5c3: return_short(c); break;
  }
  return true;
}

/* $05:D3D6–$D484: ($CAFA,9C/@$D408)/(,98@$D44E)/(,74@$D45F) + $D3D6/$D3E9 siblings —
 * face bit1 set/clear via $B594/$B5AA; JSL $1D8583/$859B; score adjust → $CA8D. */
static bool actor_op_d3d6_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  /* $D45F is both ($CAFA,74) m8 entry and m16 fallthrough from $D44E — accept either. */
  const bool either = (pc == 0xd45f);
  const bool m16 = (pc == 0xd45b || pc == 0xd45d || pc == 0xd461 || pc == 0xd462 || pc == 0xd463 || pc == 0xd464 || pc == 0xd466 || pc == 0xd468 || pc == 0xd46a || pc == 0xd46b || pc == 0xd46d || pc == 0xd475 || pc == 0xd477 || pc == 0xd478 || pc == 0xd47b);
  if(!either && m16 && c->mf) return false;
  const bool sep_entry = (pc == 0xd46f || pc == 0xd47e);
  if(sep_entry && c->mf) return false;
  const bool rep_entry = (pc == 0xd459 || pc == 0xd473);
  if(rep_entry && !c->mf) return false;
  if(!either && !m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xd3d6: case 0xd3d9: case 0xd3dc: case 0xd3de: case 0xd3e1: case 0xd3e4:
    case 0xd3e7: case 0xd3e9: case 0xd3ec: case 0xd3ef: case 0xd3f1: case 0xd3f4:
    case 0xd3f7: case 0xd3fa: case 0xd3fd: case 0xd3ff: case 0xd401: case 0xd404:
    case 0xd406: case 0xd408: case 0xd40a: case 0xd40c: case 0xd40f: case 0xd410:
    case 0xd412: case 0xd414: case 0xd416: case 0xd419: case 0xd41d: case 0xd41e:
    case 0xd421: case 0xd423: case 0xd426: case 0xd427: case 0xd429: case 0xd42b:
    case 0xd42d: case 0xd430: case 0xd434: case 0xd435: case 0xd438: case 0xd43a:
    case 0xd43d: case 0xd440: case 0xd442: case 0xd444: case 0xd447: case 0xd449:
    case 0xd44b: case 0xd44e: case 0xd450: case 0xd452: case 0xd454: case 0xd456:
    case 0xd459: case 0xd45b: case 0xd45d: case 0xd45f: case 0xd461: case 0xd462:
    case 0xd463: case 0xd464: case 0xd466: case 0xd468: case 0xd46a: case 0xd46b:
    case 0xd46d: case 0xd46f: case 0xd471: case 0xd473: case 0xd475: case 0xd477:
    case 0xd478: case 0xd47b: case 0xd47e: case 0xd480: case 0xd482:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xd3d6: jsr_abs_native(c, 0xb6b2); break;
    case 0xd3d9: load_a_abs_y8(c, 0x0000); break;
    case 0xd3dc: ora_constant8(c, 0x02); break;
    case 0xd3de: store_a_abs_y8(c, 0x0000); break;
    case 0xd3e1: load_a_abs_y8(c, 0x0020); break;
    case 0xd3e4: jsr_abs_native(c, 0xb594); break;
    case 0xd3e7: branch_to(c, true, 0xd3fa); break;
    case 0xd3e9: jsr_abs_native(c, 0xb6c0); break;
    case 0xd3ec: load_a_abs_y8(c, 0x0000); break;
    case 0xd3ef: and_constant8(c, 0xfd); break;
    case 0xd3f1: store_a_abs_y8(c, 0x0000); break;
    case 0xd3f4: load_a_abs_y8(c, 0x0020); break;
    case 0xd3f7: jsr_abs_native(c, 0xb5aa); break;
    case 0xd3fa: store_a_abs_y8(c, 0x0020); break;
    case 0xd3fd: store_dp(c, 0x05, true); break;
    case 0xd3ff: stz_dp8(c, 0x04); break;
    case 0xd401: cpy16_constant(c, 0x1400); break;
    case 0xd404: branch_to(c, c->z, 0xd408); break;
    case 0xd406: inc_dp8(c, 0x04); break;
    case 0xd408: load_dp8(c, 0x04); break;
    case 0xd40a: branch_to(c, !c->z, 0xd423); break;
    case 0xd40c: video_load(c, 0x1420, false); break;
    case 0xd40f: pha8(c); break;
    case 0xd410: load_dp8(c, 0x05); break;
    case 0xd412: cmp_constant8(c, 0xfe); break;
    case 0xd414: branch_to(c, c->z, 0xd419); break;
    case 0xd416: store_register8(c, 0x1420); break;
    case 0xd419: jsl_to(c, 0x8583); break;
    case 0xd41d: pla8(c); break;
    case 0xd41e: store_register8(c, 0x1420); break;
    case 0xd421: branch_to(c, true, 0xd438); break;
    case 0xd423: video_load(c, 0x1460, false); break;
    case 0xd426: pha8(c); break;
    case 0xd427: load_dp8(c, 0x05); break;
    case 0xd429: cmp_constant8(c, 0xfe); break;
    case 0xd42b: branch_to(c, c->z, 0xd430); break;
    case 0xd42d: store_register8(c, 0x1460); break;
    case 0xd430: jsl_to(c, 0x859b); break;
    case 0xd434: pla8(c); break;
    case 0xd435: store_register8(c, 0x1460); break;
    case 0xd438: byte_constant(c, 0x02); break;
    case 0xd43a: jmp_abs_native(c, 0xdd51); break;
    case 0xd43d: video_load(c, 0x14c7, false); break;
    case 0xd440: and_constant8(c, 0x10); break;
    case 0xd442: branch_to(c, true, 0xd447); break;
    case 0xd444: video_load(c, 0x14d2, false); break;
    case 0xd447: branch_to(c, !c->z, 0xd45f); break;
    case 0xd449: byte_constant(c, 0x02); break;
    case 0xd44b: jmp_abs_native(c, 0xdd51); break;
    case 0xd44e: load_dp8(c, 0xb9); break;
    case 0xd450: cmp_dp8(c, 0x04); break;
    case 0xd452: branch_to(c, c->z, 0xd459); break;
    case 0xd454: byte_constant(c, 0x03); break;
    case 0xd456: jmp_abs_native(c, 0xca8d); break;
    case 0xd459: sep_rep_native(c, 0x20, false); break;
    case 0xd45b: load_dp16_to_a(c, 0x05); break;
    case 0xd45d: store_dp(c, 0x04, false); break;
    case 0xd45f: sep_rep_native(c, 0x20, false); break;
    case 0xd461: implied(c); c->y--; zn(c, c->y, false); break;
    case 0xd462: tya16(c); break;
    case 0xd463: implied(c); c->c = false; break;
    case 0xd464: adc_dp16(c, 0x00); break;
    case 0xd466: store_dp(c, 0x00, false); break;
    case 0xd468: load_dp16_to_a(c, 0x04); break;
    case 0xd46a: implied(c); c->c = true; break;
    case 0xd46b: sbc_dp16(c, 0x00); break;
    case 0xd46d: store_dp(c, 0x04, false); break;
    case 0xd46f: sep_rep_native(c, 0x20, true); break;
    case 0xd471: ldy_dp16(c, 0x20); break;
    case 0xd473: sep_rep_native(c, 0x20, false); break;
    case 0xd475: load_dp16_to_a(c, 0x04); break;
    case 0xd477: implied(c); c->c = false; break;
    case 0xd478: adc_abs_y16(c, 0x0035); break;
    case 0xd47b: store_a_abs_y16(c, 0x0035); break;
    case 0xd47e: sep_rep_native(c, 0x20, true); break;
    case 0xd480: byte_constant(c, 0xff); break;
    case 0xd482: jmp_abs_native(c, 0xca8d); break;
  }
  return true;
}

/* TOTAL: 434 sites, 971 bytes */

/* ==== Batch: DDF4 deep DE26+DEA1, E117 EA8C/EED4/EF26–EFFF/ECE4, 1D8583 ==== */

/* $05:DE26–$DE62 + $DEA1–$DEB1: deep $DDF4 path — LDA $05DE63,X pair-table,
 * $B6B2+$B572 match loop, $DEA1 remap (BMI→#$20 else $05DEB2,X); optional $DBF3. */
static bool actor_helper_de26_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xde28 || pc == 0xde2c);
  const bool sep_entry = (pc == 0xde2e);
  const bool rep_entry = (pc == 0xde26);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xde26: case 0xde28: case 0xde2c: case 0xde2e: case 0xde30: case 0xde32:
    case 0xde34: case 0xde37: case 0xde3a: case 0xde3d: case 0xde3f: case 0xde42:
    case 0xde44: case 0xde45: case 0xde47: case 0xde49: case 0xde4b: case 0xde4d:
    case 0xde4e: case 0xde50: case 0xde52: case 0xde55: case 0xde57: case 0xde59:
    case 0xde5b: case 0xde5d: case 0xde5f: case 0xde62:
    case 0xdea1: case 0xdea3: case 0xdea5: case 0xdea6: case 0xdea8: case 0xdea9:
    case 0xdeaa: case 0xdeae: case 0xdeaf: case 0xdeb1:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xde26: sep_rep_native(c, 0x20, false); break;
    case 0xde28: lda_long_x16(c, 0x05de63); break;
    case 0xde2c: store_dp(c, 0x00, false); break;
    case 0xde2e: sep_rep_native(c, 0x20, true); break;
    case 0xde30: byte_constant(c, 0x05); break;
    case 0xde32: store_dp(c, 0x02, true); break;
    case 0xde34: jsr_abs_native(c, 0xb6b2); break;
    case 0xde37: load_a_abs_y8(c, 0x0020); break;
    case 0xde3a: jsr_abs_native(c, 0xb572); break;
    case 0xde3d: store_dp(c, 0x03, true); break;
    case 0xde3f: index_constant(c, true, 0x0000); break;
    case 0xde42: case 0xde50: lda_indirect_long_y8(c, 0x00); break;
    case 0xde44: case 0xde4d: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xde45: cmp_constant8(c, 0xff); break;
    case 0xde47: branch_to(c, c->z, 0xde50); break;
    case 0xde49: cmp_dp8(c, 0x03); break;
    case 0xde4b: branch_to(c, c->z, 0xde50); break;
    case 0xde4e: branch_to(c, true, 0xde42); break;
    case 0xde52: jsr_abs_native(c, 0xdea1); break;
    case 0xde55: store_dp(c, 0x04, true); break;
    case 0xde57: cmp_constant8(c, 0x20); break;
    case 0xde59: branch_to(c, !c->z, 0xde62); break;
    case 0xde5b: byte_constant(c, 0x10); break;
    case 0xde5d: store_dp(c, 0x05, true); break;
    case 0xde5f: jsr_abs_native(c, 0xdbf3); break;
    case 0xde62: case 0xdeae: case 0xdeb1: return_short(c); break;
    case 0xdea1: branch_to(c, c->n, 0xdeaf); break;
    case 0xdea3: and_constant8(c, 0x1f); break;
    case 0xdea5: case 0xdea8: xba_native(c); break;
    case 0xdea6: byte_constant(c, 0x00); break;
    case 0xdea9: tax16_from_a8(c); break;
    case 0xdeaa: load_a_long_x8(c, 0x05deb2); break;
    case 0xdeaf: byte_constant(c, 0x20); break;
  }
  return true;
}

/* $05:EED4–$EF1F: ($E117,44) — INC $1546; ≥#$BC clear/$RTS; else $1558 phase,
 * $155A/$155C from $05EF20,X by DP+$B7 class; RTS. */
static bool actor_hdl_eed4_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xeed4: case 0xeed7: case 0xeeda: case 0xeedc: case 0xeede: case 0xeee1:
    case 0xeee4: case 0xeee5: case 0xeee6: case 0xeee7: case 0xeee9: case 0xeeeb:
    case 0xeeee: case 0xeef1: case 0xeef4: case 0xeef7: case 0xeef9: case 0xeefb:
    case 0xeefd: case 0xef00: case 0xef02: case 0xef04: case 0xef07: case 0xef0b:
    case 0xef0e: case 0xef12: case 0xef15: case 0xef18: case 0xef1a: case 0xef1c:
    case 0xef1f:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xeed4: inc_abs8_native(c, 0x1546); break;
    case 0xeed7: video_load(c, 0x1546, false); break;
    case 0xeeda: cmp_constant8(c, 0xbc); break;
    case 0xeedc: branch_to(c, !c->c, 0xeee5); break;
    case 0xeede: stz_abs8(c, 0x1546); break;
    case 0xeee1: stz_abs8(c, 0x1558); break;
    case 0xeee4: case 0xef1f: return_short(c); break;
    case 0xeee5: case 0xeee6: lsr_a8(c); break;
    case 0xeee7: and_constant8(c, 0x01); break;
    case 0xeee9: ora_constant8(c, 0x80); break;
    case 0xeeeb: case 0xef1c: store_register8(c, 0x1558); break;
    case 0xeeee: stz_abs8(c, 0x155b); break;
    case 0xeef1: stz_abs8(c, 0x155d); break;
    case 0xeef4: index_constant(c, false, 0x0000); break;
    case 0xeef7: load_dp8(c, 0xb7); break;
    case 0xeef9: cmp_constant8(c, 0x38); break;
    case 0xeefb: branch_to(c, c->z, 0xef07); break;
    case 0xeefd: index_constant(c, false, 0x0002); break;
    case 0xef00: cmp_constant8(c, 0x3d); break;
    case 0xef02: branch_to(c, c->z, 0xef07); break;
    case 0xef04: index_constant(c, false, 0x0004); break;
    case 0xef07: load_a_long_x8(c, 0x05ef20); break;
    case 0xef0b: store_register8(c, 0x155a); break;
    case 0xef0e: load_a_long_x8(c, 0x05ef21); break;
    case 0xef12: store_register8(c, 0x155c); break;
    case 0xef15: cpx16_constant(c, 0x0002); break;
    case 0xef18: branch_to(c, !c->z, 0xef1f); break;
    case 0xef1a: byte_constant(c, 0x80); break;
  }
  return true;
}

/* $05:EF26–$EFFF: ($E117,42/46/4A) EF26 scroll nudge + EF5A/EFCC phase bodies;
 * shared $EFB0/$EFC0/$EFC6 tails. */
static bool actor_hdl_ef26_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xef32 || pc == 0xef34 || pc == 0xef37 || pc == 0xef3a || pc == 0xef3c
                      || pc == 0xef3f || pc == 0xef42 || pc == 0xef45);
  const bool sep_entry = (pc == 0xef48);
  const bool rep_entry = (pc == 0xef30);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xef26: case 0xef29: case 0xef2b: case 0xef2d: case 0xef2f: case 0xef30:
    case 0xef32: case 0xef34: case 0xef37: case 0xef3a: case 0xef3c: case 0xef3f:
    case 0xef42: case 0xef45: case 0xef48: case 0xef4a: case 0xef4d: case 0xef50:
    case 0xef52: case 0xef54: case 0xef56: case 0xef59:
    case 0xef5a: case 0xef5d: case 0xef5f: case 0xef61: case 0xef64: case 0xef67:
    case 0xef6a: case 0xef6d: case 0xef70: case 0xef73: case 0xef75: case 0xef77:
    case 0xef79: case 0xef7c: case 0xef7e: case 0xef80: case 0xef82: case 0xef84:
    case 0xef86: case 0xef88: case 0xef8a: case 0xef8c: case 0xef8e: case 0xef91:
    case 0xef93: case 0xef95: case 0xef97: case 0xef99: case 0xef9c: case 0xef9f:
    case 0xefa1: case 0xefa3: case 0xefa5: case 0xefa8: case 0xefab: case 0xefae:
    case 0xefb0: case 0xefb3: case 0xefb5: case 0xefb8: case 0xefba: case 0xefbc:
    case 0xefbf: case 0xefc0: case 0xefc1: case 0xefc3: case 0xefc6: case 0xefc8:
    case 0xefcb:
    case 0xefcc: case 0xefcf: case 0xefd1: case 0xefd3: case 0xefd5: case 0xefd8:
    case 0xefdb: case 0xefde: case 0xefe1: case 0xefe4: case 0xefe7: case 0xefe9:
    case 0xefec: case 0xefee: case 0xeff1: case 0xeff3: case 0xeff5: case 0xeff8:
    case 0xeffa: case 0xeffc: case 0xefff:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xef26: case 0xef5a: case 0xefcc: case 0xeff5: video_load(c, 0x1546, false); break;
    case 0xef29: case 0xef5d: case 0xefcf: cmp_constant8(c, 0x80); break;
    case 0xef2b: branch_to(c, c->z, 0xef61); break;
    case 0xef2d: load_dp8(c, 0xb9); break;
    case 0xef2f: lsr_a8(c); break;
    case 0xef30: sep_rep_native(c, 0x20, false); break;
    case 0xef32: branch_to(c, c->c, 0xef3c); break;
    case 0xef34: case 0xef37: dec_abs16(c, 0x155c); break;
    case 0xef3a: branch_to(c, true, 0xef42); break;
    case 0xef3c: case 0xef3f: inc_abs16(c, 0x155c); break;
    case 0xef42: case 0xef45: dec_abs16(c, 0x155a); break;
    case 0xef48: sep_rep_native(c, 0x20, true); break;
    case 0xef4a: jsr_abs_native(c, 0xefb0); break;
    case 0xef4d: case 0xefb0: case 0xefe9: video_load(c, 0x1559, false); break;
    case 0xef50: cmp_constant8(c, 0x08); break;
    case 0xef52: branch_to(c, !c->z, 0xef59); break;
    case 0xef54: byte_constant(c, 0x02); break;
    case 0xef56: store_register8(c, 0x1559); break;
    case 0xef59: case 0xefbf: case 0xefcb: case 0xefff: return_short(c); break;
    case 0xef5f: branch_to(c, !c->z, 0xefb0); break;
    case 0xef61: store_register8(c, 0x1558); break;
    case 0xef64: index_constant(c, true, 0x0054); break;
    case 0xef67: store_y_abs16(c, 0x155a); break;
    case 0xef6a: jsr_abs_native(c, 0xb6b2); break;
    case 0xef6d: index_constant(c, false, 0x0080); break;
    case 0xef70: case 0xef8e: load_a_abs_y8(c, 0x0034); break;
    case 0xef73: cmp_constant8(c, 0x03); break;
    case 0xef75: branch_to(c, c->z, 0xefa8); break;
    case 0xef77: byte_constant(c, 0x58); break;
    case 0xef79: store_register8(c, 0x155a); break;
    case 0xef7c: load_dp8(c, 0xb7); break;
    case 0xef7e: cmp_constant8(c, 0x50); break;
    case 0xef80: branch_to(c, c->z, 0xefa8); break;
    case 0xef82: cmp_constant8(c, 0x31); break;
    case 0xef84: branch_to(c, c->z, 0xefa8); break;
    case 0xef86: cmp_constant8(c, 0x27); break;
    case 0xef88: branch_to(c, c->z, 0xefa8); break;
    case 0xef8a: cmp_constant8(c, 0x29); break;
    case 0xef8c: branch_to(c, !c->z, 0xef99); break;
    case 0xef91: and_constant8(c, 0xfe); break;
    case 0xef93: cmp_constant8(c, 0x18); break;
    case 0xef95: branch_to(c, c->z, 0xefa5); break;
    case 0xef97: branch_to(c, !c->z, 0xefa8); break;
    case 0xef99: index_constant(c, false, 0x0090); break;
    case 0xef9c: load_a_abs_y8(c, 0x0021); break;
    case 0xef9f: and_constant8(c, 0x7f); break;
    case 0xefa1: cmp_constant8(c, 0x19); break;
    case 0xefa3: branch_to(c, c->z, 0xefa8); break;
    case 0xefa5: index_constant(c, false, 0x0070); break;
    case 0xefa8: stx_abs16(c, 0x155c); break;
    case 0xefab: jsr_abs_native(c, 0xf000); break;
    case 0xefae: branch_to(c, true, 0xefc6); break;
    case 0xefb3: branch_to(c, !c->z, 0xefcb); break;
    case 0xefb5: case 0xefee: video_load(c, 0x1558, false); break;
    case 0xefb8: cmp_constant8(c, 0x06); break;
    case 0xefba: branch_to(c, !c->c, 0xefc0); break;
    case 0xefbc: case 0xeffc: stz_abs8(c, 0x1546); break;
    case 0xefc0: inc_a8(c); break;
    case 0xefc1: ora_constant8(c, 0x80); break;
    case 0xefc3: store_register8(c, 0x1558); break;
    case 0xefc6: byte_constant(c, 0x08); break;
    case 0xefc8: store_register8(c, 0x1559); break;
    case 0xefd1: branch_to(c, !c->z, 0xefe9); break;
    case 0xefd3: byte_constant(c, 0x87); break;
    case 0xefd5: store_register8(c, 0x1558); break;
    case 0xefd8: index_constant(c, true, 0x0048); break;
    case 0xefdb: store_y_abs16(c, 0x155a); break;
    case 0xefde: index_constant(c, true, 0x0078); break;
    case 0xefe1: store_y_abs16(c, 0x155c); break;
    case 0xefe4: inc_abs8_native(c, 0x1546); break;
    case 0xefe7: branch_to(c, true, 0xefc6); break;
    case 0xefec: branch_to(c, !c->z, 0xefff); break;
    case 0xeff1: cmp_constant8(c, 0x09); break;
    case 0xeff3: branch_to(c, !c->c, 0xefc0); break;
    case 0xeff8: cmp_constant8(c, 0x83); break;
    case 0xeffa: branch_to(c, !c->c, 0xefd3); break;
  }
  return true;
}

/* $05:ECE4–$ED45: ($E117,4C) — #$80 seed $1558/#$0A/$155A/#$40/$155C/#$C0;
 * else m16 drift $155A+=2/$155C-=3 then $1558 from $155A; optional clear. */
static bool actor_hdl_ece4_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xed0c || pc == 0xed0f || pc == 0xed10 || pc == 0xed13
                      || pc == 0xed16 || pc == 0xed19 || pc == 0xed1a || pc == 0xed1d);
  const bool sep_entry = (pc == 0xed20);
  const bool rep_entry = (pc == 0xed0a);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xece4: case 0xece7: case 0xece9: case 0xeceb: case 0xeced: case 0xecf0:
    case 0xecf2: case 0xecf5: case 0xecf8: case 0xecfb: case 0xecfe: case 0xed01:
    case 0xed04: case 0xed05: case 0xed08: case 0xed0a: case 0xed0c: case 0xed0f:
    case 0xed10: case 0xed13: case 0xed16: case 0xed19: case 0xed1a: case 0xed1d:
    case 0xed20: case 0xed22: case 0xed25: case 0xed26: case 0xed28: case 0xed29:
    case 0xed2a: case 0xed2b: case 0xed2c: case 0xed2d: case 0xed2f: case 0xed31:
    case 0xed33: case 0xed35: case 0xed37: case 0xed3a: case 0xed3d: case 0xed3f:
    case 0xed42: case 0xed45:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xece4: video_load(c, 0x1546, false); break;
    case 0xece7: cmp_constant8(c, 0x80); break;
    case 0xece9: branch_to(c, !c->z, 0xed05); break;
    case 0xeceb: byte_constant(c, 0x87); break;
    case 0xeced: store_register8(c, 0x1558); break;
    case 0xecf0: byte_constant(c, 0x0a); break;
    case 0xecf2: store_register8(c, 0x1559); break;
    case 0xecf5: index_constant(c, true, 0x0040); break;
    case 0xecf8: store_y_abs16(c, 0x155a); break;
    case 0xecfb: index_constant(c, true, 0x00c0); break;
    case 0xecfe: store_y_abs16(c, 0x155c); break;
    case 0xed01: inc_abs8_native(c, 0x1546); break;
    case 0xed04: case 0xed45: return_short(c); break;
    case 0xed05: video_load(c, 0x1559, false); break;
    case 0xed08: branch_to(c, !c->z, 0xed45); break;
    case 0xed0a: sep_rep_native(c, 0x20, false); break;
    case 0xed0c: video_load(c, 0x155a, false); break;
    case 0xed0f: implied(c); c->c = false; break;
    case 0xed10: adc16_constant(c, 0x0002); break;
    case 0xed13: store_a_abs16(c, 0x155a); break;
    case 0xed16: video_load(c, 0x155c, false); break;
    case 0xed19: implied(c); c->c = true; break;
    case 0xed1a: sbc16_constant(c, 0x0003); break;
    case 0xed1d: store_a_abs16(c, 0x155c); break;
    case 0xed20: sep_rep_native(c, 0x20, true); break;
    case 0xed22: video_load(c, 0x155a, false); break;
    case 0xed25: implied(c); c->c = true; break;
    case 0xed26: sbc8_constant(c, 0x40); break;
    case 0xed28: case 0xed29: case 0xed2a: case 0xed2b: lsr_a8(c); break;
    case 0xed2c: implied(c); c->c = false; break;
    case 0xed2d: adc8_constant(c, 0x08); break;
    case 0xed2f: cmp_constant8(c, 0x0e); break;
    case 0xed31: branch_to(c, !c->c, 0xed35); break;
    case 0xed33: byte_constant(c, 0x0e); break;
    case 0xed35: ora_constant8(c, 0x80); break;
    case 0xed37: store_register8(c, 0x1558); break;
    case 0xed3a: video_load(c, 0x155d, false); break;
    case 0xed3d: branch_to(c, c->z, 0xed45); break;
    case 0xed3f: stz_abs8(c, 0x1558); break;
    case 0xed42: stz_abs8(c, 0x1546); break;
  }
  return true;
}

/* $1D:8583–$85DE: face→$0D0x queue seed — $8583/$859B entries share $85C1 remap
 * (#$FF→#$81 / #$FE→#$83 / else &#$7F) then JML $01C987. */
static bool face_queue_1d8583_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x1d || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x8583: case 0x8586: case 0x8589: case 0x858b: case 0x858e: case 0x8591:
    case 0x8593: case 0x8596: case 0x8599: case 0x859b: case 0x859e: case 0x85a1:
    case 0x85a3: case 0x85a6: case 0x85a9: case 0x85ab: case 0x85ae: case 0x85b1:
    case 0x85b3: case 0x85b6: case 0x85b8: case 0x85ba: case 0x85bc: case 0x85be:
    case 0x85c1: case 0x85c3: case 0x85c5: case 0x85c7: case 0x85c9: case 0x85cb:
    case 0x85cd: case 0x85cf: case 0x85d1: case 0x85d3: case 0x85d6: case 0x85d8:
    case 0x85db:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0x8583: index_constant(c, false, 0x2c00); break;
    case 0x8586: case 0x859e: stx_abs16(c, 0x0d7a); break;
    case 0x8589: byte_constant(c, 0xa0); break;
    case 0x858b: case 0x85a3: store_register8(c, 0x0d75); break;
    case 0x858e: index_constant(c, true, 0x0000); break;
    case 0x8591: byte_constant(c, 0x20); break;
    case 0x8593: case 0x85ab: store_a_abs_y8(c, 0x0d03); break;
    case 0x8596: video_load(c, 0x1420, false); break;
    case 0x8599: branch_to(c, true, 0x85c1); break;
    case 0x859b: index_constant(c, false, 0x2c80); break;
    case 0x85a1: byte_constant(c, 0xa1); break;
    case 0x85a6: index_constant(c, true, 0x0004); break;
    case 0x85a9: byte_constant(c, 0xe0); break;
    case 0x85ae: video_load(c, 0x01d6, false); break;
    case 0x85b1: branch_to(c, !c->n, 0x85be); break;
    case 0x85b3: video_load(c, 0x01e7, false); break;
    case 0x85b6: and_constant8(c, 0x02); break;
    case 0x85b8: branch_to(c, !c->z, 0x85be); break;
    case 0x85ba: byte_constant(c, 0x96); break;
    case 0x85bc: branch_to(c, true, 0x85d3); break;
    case 0x85be: video_load(c, 0x1460, false); break;
    case 0x85c1: cmp_constant8(c, 0xff); break;
    case 0x85c3: branch_to(c, !c->z, 0x85c9); break;
    case 0x85c5: byte_constant(c, 0x81); break;
    case 0x85c7: branch_to(c, true, 0x85d3); break;
    case 0x85c9: cmp_constant8(c, 0xfe); break;
    case 0x85cb: branch_to(c, !c->z, 0x85d1); break;
    case 0x85cd: byte_constant(c, 0x83); break;
    case 0x85cf: branch_to(c, true, 0x85d3); break;
    case 0x85d1: and_constant8(c, 0x7f); break;
    case 0x85d3: store_a_abs_y8(c, 0x0d01); break;
    case 0x85d6: byte_constant(c, 0xc0); break;
    case 0x85d8: store_a_abs_y8(c, 0x0d02); break;
    case 0x85db: jml_to(c, 0xc987, 0x01); break;
  }
  return true;
}

/* TOTAL net-new: 254 sites, 586 bytes (DE26+DEA1 38/78, EA8C prologue +12/24,
 * EED4 31/76, EF26–EFFF 92/218, ECE4 44/98, 1D8583 37/92) */
/* ==== Batch: E117 EB36 cluster + EA52/EA6F/EAD0/EAEB/EB08 + ED4A/EDB4/EE01/E921 ==== */

/* $05:EA52–$EA8B: ($E117) stream prologues — $F998; $1540-#$83/#$51 → ptr banks
 * $10/$1E; LDA long,X then JMP $EAA4 (shared expand). Dead SEP after JMP kept. */
static bool stream_prolog_ea52_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xea66 || pc == 0xea6a || pc == 0xea83
                      || pc == 0xea87);
  /* EA6D/EA8A are dead SEPs after JMP $EAA4 — treat as plain m8, not sep_entry. */
  const bool sep_entry = false;
  const bool rep_entry = (pc == 0xea64 || pc == 0xea81);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xea52: case 0xea55: case 0xea57: case 0xea58: case 0xea5b: case 0xea5c:
    case 0xea5e: case 0xea5f: case 0xea60: case 0xea62: case 0xea64: case 0xea66:
    case 0xea6a: case 0xea6d: case 0xea6f: case 0xea72: case 0xea74: case 0xea75:
    case 0xea78: case 0xea79: case 0xea7b: case 0xea7c: case 0xea7d: case 0xea7f:
    case 0xea81: case 0xea83: case 0xea87: case 0xea8a:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xea52: jsr_abs_native(c, 0xf998); break;
    case 0xea55: byte_constant(c, 0x00); break;
    case 0xea57: xba_native(c); break;
    case 0xea58: video_load(c, 0x1540, false); break;
    case 0xea5b: implied(c); c->c = true; break;
    case 0xea5c: sbc8_constant(c, 0x83); break;
    case 0xea5e: asl_a8(c); break;
    case 0xea5f: tax16_from_a8(c); break;
    case 0xea60: byte_constant(c, 0x10); break;
    case 0xea62: store_dp(c, 0x02, true); break;
    case 0xea64: sep_rep_native(c, 0x20, false); break;
    case 0xea66: lda_long_x16(c, 0x1093ed); break;
    case 0xea6a: jmp_abs_native(c, 0xeaa4); break;
    case 0xea6d: sep_rep_native(c, 0x20, true); break;
    case 0xea6f: jsr_abs_native(c, 0xf998); break;
    case 0xea72: byte_constant(c, 0x00); break;
    case 0xea74: xba_native(c); break;
    case 0xea75: video_load(c, 0x1540, false); break;
    case 0xea78: implied(c); c->c = true; break;
    case 0xea79: sbc8_constant(c, 0x51); break;
    case 0xea7b: asl_a8(c); break;
    case 0xea7c: tax16_from_a8(c); break;
    case 0xea7d: byte_constant(c, 0x1e); break;
    case 0xea7f: store_dp(c, 0x02, true); break;
    case 0xea81: sep_rep_native(c, 0x20, false); break;
    case 0xea83: lda_long_x16(c, 0x1ec0c6); break;
    case 0xea87: jmp_abs_native(c, 0xeaa4); break;
    case 0xea8a: sep_rep_native(c, 0x20, true); break;
  }
  return true;
}

/* $05:EAD0–$EB30: ($E117) queue-stream siblings — $EAD0 seeds DP+$83/$85 then BRA $EB21;
 * $EAEB phase/toggle + JSR $EB08; $EB08 $F998-less expand via $96ED/$F91E/$F93D/$F85E. */
static bool queue_stream_ead0_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xeae1 || pc == 0xeae5 || pc == 0xeb19
                      || pc == 0xeb1d);
  const bool sep_entry = (pc == 0xeae7 || pc == 0xeb1f);
  const bool rep_entry = (pc == 0xeadf || pc == 0xeb17);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xead0: case 0xead2: case 0xead3: case 0xead6: case 0xead7: case 0xead9:
    case 0xeada: case 0xeadb: case 0xeadd: case 0xeadf: case 0xeae1: case 0xeae5:
    case 0xeae7: case 0xeae9: case 0xeaeb: case 0xeaee: case 0xeaef: case 0xeaf1:
    case 0xeaf3: case 0xeaf6: case 0xeaf8: case 0xeafb: case 0xeafe: case 0xeaff:
    case 0xeb00: case 0xeb02: case 0xeb04: case 0xeb07: case 0xeb08: case 0xeb0a:
    case 0xeb0b: case 0xeb0e: case 0xeb0f: case 0xeb11: case 0xeb12: case 0xeb13:
    case 0xeb15: case 0xeb17: case 0xeb19: case 0xeb1d: case 0xeb1f: case 0xeb21:
    case 0xeb24: case 0xeb27: case 0xeb2a: case 0xeb2d: case 0xeb30:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xead0: byte_constant(c, 0x00); break;
    case 0xead2: xba_native(c); break;
    case 0xead3: video_load(c, 0x1540, false); break;
    case 0xead6: implied(c); c->c = true; break;
    case 0xead7: sbc8_constant(c, 0x83); break;
    case 0xead9: asl_a8(c); break;
    case 0xeada: tax16_from_a8(c); break;
    case 0xeadb: byte_constant(c, 0x10); break;
    case 0xeadd: store_dp(c, 0x85, true); break;
    case 0xeadf: sep_rep_native(c, 0x20, false); break;
    case 0xeae1: lda_long_x16(c, 0x1093ed); break;
    case 0xeae5: store_dp(c, 0x83, false); break;
    case 0xeae7: sep_rep_native(c, 0x20, true); break;
    case 0xeae9: branch_to(c, true, 0xeb21); break;
    case 0xeaeb: video_load(c, 0x1546, false); break;
    case 0xeaee: pha8(c); break;
    case 0xeaef: and_constant8(c, 0x07); break;
    case 0xeaf1: branch_to(c, !c->z, 0xeafe); break;
    case 0xeaf3: video_load(c, 0x1540, false); break;
    case 0xeaf6: eor_constant8(c, 0x03); break;
    case 0xeaf8: store_register8(c, 0x1540); break;
    case 0xeafb: jsr_abs_native(c, 0xeb08); break;
    case 0xeafe: pla8(c); break;
    case 0xeaff: inc_a8(c); break;
    case 0xeb00: branch_to(c, !c->z, 0xeb04); break;
    case 0xeb02: byte_constant(c, 0x80); break;
    case 0xeb04: store_register8(c, 0x1546); break;
    case 0xeb07: return_short(c); break;
    case 0xeb08: byte_constant(c, 0x00); break;
    case 0xeb0a: xba_native(c); break;
    case 0xeb0b: video_load(c, 0x1540, false); break;
    case 0xeb0e: implied(c); c->c = true; break;
    case 0xeb0f: sbc8_constant(c, 0x30); break;
    case 0xeb11: asl_a8(c); break;
    case 0xeb12: tax16_from_a8(c); break;
    case 0xeb13: byte_constant(c, 0x17); break;
    case 0xeb15: store_dp(c, 0x85, true); break;
    case 0xeb17: sep_rep_native(c, 0x20, false); break;
    case 0xeb19: lda_long_x16(c, 0x17969c); break;
    case 0xeb1d: store_dp(c, 0x83, false); break;
    case 0xeb1f: sep_rep_native(c, 0x20, true); break;
    case 0xeb21: index_constant(c, true, 0x0000); break;
    case 0xeb24: jsr_abs_native(c, 0x96ed); break;
    case 0xeb27: jsr_abs_native(c, 0xf91e); break;
    case 0xeb2a: jsr_abs_native(c, 0xf93d); break;
    case 0xeb2d: jsr_abs_native(c, 0xf85e); break;
    case 0xeb30: return_short(c); break;
  }
  return true;
}

/* $05:EB31–$ECD3: ($E117,4E/E6/F8) large phase body — $EB31/$EB36 seed DP+$10 + scroll/
 * $1547..$154F; continue path oscillates $155A/$155C; $EC61 table-drift via $05ECD4. */
static bool actor_hdl_eb36_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xeb89 || pc == 0xeb8b || pc == 0xeb8e
                      || pc == 0xeb8f || pc == 0xeb91 || pc == 0xeb94
                      || pc == 0xeb97 || pc == 0xeba4 || pc == 0xeba7
                      || pc == 0xeba9 || pc == 0xebab || pc == 0xebae
                      || pc == 0xebaf || pc == 0xebb1 || pc == 0xebb4
                      || pc == 0xebb7 || pc == 0xebc4 || pc == 0xebc7
                      || pc == 0xebc9 || pc == 0xebcc || pc == 0xebcd
                      || pc == 0xebd0 || pc == 0xebd3 || pc == 0xebd6
                      || pc == 0xebd7 || pc == 0xebda || pc == 0xebdd
                      || pc == 0xebdf || pc == 0xebe2 || pc == 0xebe4
                      || pc == 0xebe7 || pc == 0xebe8 || pc == 0xebeb
                      || pc == 0xebee || pc == 0xec09 || pc == 0xec0b
                      || pc == 0xec0e || pc == 0xec0f || pc == 0xec11
                      || pc == 0xec14 || pc == 0xec17 || pc == 0xec19
                      || pc == 0xec1b || pc == 0xec1e || pc == 0xec1f
                      || pc == 0xec21 || pc == 0xec24 || pc == 0xec27
                      || pc == 0xec29 || pc == 0xec2c || pc == 0xec2d
                      || pc == 0xec30 || pc == 0xec33 || pc == 0xec35
                      || pc == 0xec38 || pc == 0xec3a || pc == 0xec3d
                      || pc == 0xec3e || pc == 0xec41 || pc == 0xec88
                      || pc == 0xec8b || pc == 0xec8c || pc == 0xec90
                      || pc == 0xec93 || pc == 0xec96 || pc == 0xec97
                      || pc == 0xec9b);
  const bool sep_entry = (pc == 0xeb99 || pc == 0xebb9 || pc == 0xebf1 || pc == 0xec44 || pc == 0xec9e);
  const bool rep_entry = (pc == 0xeb87 || pc == 0xeba2 || pc == 0xebc2 || pc == 0xec07 || pc == 0xec86);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xeb31: case 0xeb34: case 0xeb36: case 0xeb39: case 0xeb3b: case 0xeb3e:
    case 0xeb40: case 0xeb42: case 0xeb44: case 0xeb46: case 0xeb49: case 0xeb4c:
    case 0xeb4f: case 0xeb52: case 0xeb55: case 0xeb57: case 0xeb5a: case 0xeb5d:
    case 0xeb60: case 0xeb63: case 0xeb66: case 0xeb68: case 0xeb6b: case 0xeb6e:
    case 0xeb71: case 0xeb74: case 0xeb77: case 0xeb7a: case 0xeb7b: case 0xeb7e:
    case 0xeb80: case 0xeb82: case 0xeb85: case 0xeb87: case 0xeb89: case 0xeb8b:
    case 0xeb8e: case 0xeb8f: case 0xeb91: case 0xeb94: case 0xeb97: case 0xeb99:
    case 0xeb9b: case 0xeb9c: case 0xeb9e: case 0xeba1: case 0xeba2: case 0xeba4:
    case 0xeba7: case 0xeba9: case 0xebab: case 0xebae: case 0xebaf: case 0xebb1:
    case 0xebb4: case 0xebb7: case 0xebb9: case 0xebbb: case 0xebbc: case 0xebbe:
    case 0xebc1: case 0xebc2: case 0xebc4: case 0xebc7: case 0xebc9: case 0xebcc:
    case 0xebcd: case 0xebd0: case 0xebd3: case 0xebd6: case 0xebd7: case 0xebda:
    case 0xebdd: case 0xebdf: case 0xebe2: case 0xebe4: case 0xebe7: case 0xebe8:
    case 0xebeb: case 0xebee: case 0xebf1: case 0xebf3: case 0xebf5: case 0xebf8:
    case 0xebfa: case 0xebfd: case 0xec00: case 0xec02: case 0xec05: case 0xec07:
    case 0xec09: case 0xec0b: case 0xec0e: case 0xec0f: case 0xec11: case 0xec14:
    case 0xec17: case 0xec19: case 0xec1b: case 0xec1e: case 0xec1f: case 0xec21:
    case 0xec24: case 0xec27: case 0xec29: case 0xec2c: case 0xec2d: case 0xec30:
    case 0xec33: case 0xec35: case 0xec38: case 0xec3a: case 0xec3d: case 0xec3e:
    case 0xec41: case 0xec44: case 0xec46: case 0xec49: case 0xec4c: case 0xec4e:
    case 0xec4f: case 0xec50: case 0xec51: case 0xec52: case 0xec53: case 0xec55:
    case 0xec57: case 0xec59: case 0xec5b: case 0xec5d: case 0xec60: case 0xec61:
    case 0xec64: case 0xec66: case 0xec68: case 0xec6a: case 0xec6d: case 0xec70:
    case 0xec73: case 0xec76: case 0xec79: case 0xec7c: case 0xec7d: case 0xec7f:
    case 0xec80: case 0xec83: case 0xec84: case 0xec85: case 0xec86: case 0xec88:
    case 0xec8b: case 0xec8c: case 0xec90: case 0xec93: case 0xec96: case 0xec97:
    case 0xec9b: case 0xec9e: case 0xeca0: case 0xeca3: case 0xeca4: case 0xeca6:
    case 0xeca7: case 0xeca8: case 0xeca9: case 0xecaa: case 0xecab: case 0xecad:
    case 0xecaf: case 0xecb1: case 0xecb3: case 0xecb5: case 0xecb8: case 0xecbb:
    case 0xecbd: case 0xecc0: case 0xecc3: case 0xecc5: case 0xecc7: case 0xecc9:
    case 0xeccc: case 0xeccd: case 0xecd0: case 0xecd3:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xeb31: index_constant(c, true, 0x0600); break;
    case 0xeb34: branch_to(c, true, 0xeb39); break;
    case 0xeb36: index_constant(c, true, 0x0e09); break;
    case 0xeb39: sty_dp16(c, 0x10); break;
    case 0xeb3b: video_load(c, 0x1546, false); break;
    case 0xeb3e: cmp_constant8(c, 0x80); break;
    case 0xeb40: branch_to(c, !c->z, 0xeb7b); break;
    case 0xeb42: load_dp8(c, 0x10); break;
    case 0xeb44: ora_constant8(c, 0x80); break;
    case 0xeb46: store_register8(c, 0x1558); break;
    case 0xeb49: index_constant(c, true, 0x0070); break;
    case 0xeb4c: store_y_abs16(c, 0x155a); break;
    case 0xeb4f: index_constant(c, true, 0x0080); break;
    case 0xeb52: store_y_abs16(c, 0x155c); break;
    case 0xeb55: byte_constant(c, 0x01); break;
    case 0xeb57: store_register8(c, 0x1547); break;
    case 0xeb5a: index_constant(c, true, 0x0300); break;
    case 0xeb5d: store_y_abs16(c, 0x1548); break;
    case 0xeb60: index_constant(c, true, 0x0080); break;
    case 0xeb63: store_y_abs16(c, 0x154a); break;
    case 0xeb66: byte_constant(c, 0x04); break;
    case 0xeb68: store_register8(c, 0x154c); break;
    case 0xeb6b: index_constant(c, true, 0x0000); break;
    case 0xeb6e: store_y_abs16(c, 0x154d); break;
    case 0xeb71: index_constant(c, true, 0x0080); break;
    case 0xeb74: store_y_abs16(c, 0x154f); break;
    case 0xeb77: inc_abs8_native(c, 0x1546); break;
    case 0xeb7a: return_short(c); break;
    case 0xeb7b: video_load(c, 0x1549, false); break;
    case 0xeb7e: store_dp(c, 0x01, true); break;
    case 0xeb80: stz_dp8(c, 0x02); break;
    case 0xeb82: video_load(c, 0x1547, false); break;
    case 0xeb85: cmp_constant8(c, 0x01); break;
    case 0xeb87: sep_rep_native(c, 0x20, false); break;
    case 0xeb89: branch_to(c, !c->z, 0xebab); break;
    case 0xeb8b: video_load(c, 0x155c, false); break;
    case 0xeb8e: implied(c); c->c = true; break;
    case 0xeb8f: sbc_dp16(c, 0x01); break;
    case 0xeb91: store_a_abs16(c, 0x155c); break;
    case 0xeb94: cmp_constant16(c, 0x0080); break;
    case 0xeb97: branch_to(c, c->c, 0xeba4); break;
    case 0xeb99: sep_rep_native(c, 0x20, true); break;
    case 0xeb9b: pha8(c); break;
    case 0xeb9c: byte_constant(c, 0x04); break;
    case 0xeb9e: store_register8(c, 0x154c); break;
    case 0xeba1: pla8(c); break;
    case 0xeba2: sep_rep_native(c, 0x20, false); break;
    case 0xeba4: cmp_constant16(c, 0x0070); break;
    case 0xeba7: branch_to(c, !c->c, 0xebc9); break;
    case 0xeba9: branch_to(c, c->c, 0xebe4); break;
    case 0xebab: video_load(c, 0x155c, false); break;
    case 0xebae: implied(c); c->c = false; break;
    case 0xebaf: adc_dp16(c, 0x01); break;
    case 0xebb1: store_a_abs16(c, 0x155c); break;
    case 0xebb4: cmp_constant16(c, 0x0080); break;
    case 0xebb7: branch_to(c, !c->c, 0xebc4); break;
    case 0xebb9: sep_rep_native(c, 0x20, true); break;
    case 0xebbb: pha8(c); break;
    case 0xebbc: byte_constant(c, 0x08); break;
    case 0xebbe: store_register8(c, 0x154c); break;
    case 0xebc1: pla8(c); break;
    case 0xebc2: sep_rep_native(c, 0x20, false); break;
    case 0xebc4: cmp_constant16(c, 0x0090); break;
    case 0xebc7: branch_to(c, !c->c, 0xebe4); break;
    case 0xebc9: video_load(c, 0x154a, false); break;
    case 0xebcc: implied(c); c->c = false; break;
    case 0xebcd: adc16_constant(c, 0x0006); break;
    case 0xebd0: store_a_abs16(c, 0x154a); break;
    case 0xebd3: video_load(c, 0x1548, false); break;
    case 0xebd6: implied(c); c->c = true; break;
    case 0xebd7: sbc_abs16(c, 0x154a); break;
    case 0xebda: store_a_abs16(c, 0x1548); break;
    case 0xebdd: branch_to(c, c->c, 0xebee); break;
    case 0xebdf: stz_abs16(c, 0x1548); break;
    case 0xebe2: branch_to(c, true, 0xebee); break;
    case 0xebe4: video_load(c, 0x1548, false); break;
    case 0xebe7: implied(c); c->c = false; break;
    case 0xebe8: adc_abs16(c, 0x154a); break;
    case 0xebeb: store_a_abs16(c, 0x1548); break;
    case 0xebee: video_load(c, 0x1548, false); break;
    case 0xebf1: sep_rep_native(c, 0x20, true); break;
    case 0xebf3: branch_to(c, !c->z, 0xebfd); break;
    case 0xebf5: video_load(c, 0x1547, false); break;
    case 0xebf8: eor_constant8(c, 0x03); break;
    case 0xebfa: store_register8(c, 0x1547); break;
    case 0xebfd: video_load(c, 0x154e, false); break;
    case 0xec00: store_dp(c, 0x01, true); break;
    case 0xec02: video_load(c, 0x154c, false); break;
    case 0xec05: cmp_constant8(c, 0x04); break;
    case 0xec07: sep_rep_native(c, 0x20, false); break;
    case 0xec09: branch_to(c, !c->z, 0xec1b); break;
    case 0xec0b: video_load(c, 0x155a, false); break;
    case 0xec0e: implied(c); c->c = true; break;
    case 0xec0f: sbc_dp16(c, 0x01); break;
    case 0xec11: store_a_abs16(c, 0x155a); break;
    case 0xec14: cmp_constant16(c, 0x0050); break;
    case 0xec17: branch_to(c, !c->c, 0xec29); break;
    case 0xec19: branch_to(c, c->c, 0xec3a); break;
    case 0xec1b: video_load(c, 0x155a, false); break;
    case 0xec1e: implied(c); c->c = false; break;
    case 0xec1f: adc_dp16(c, 0x01); break;
    case 0xec21: store_a_abs16(c, 0x155a); break;
    case 0xec24: cmp_constant16(c, 0x0060); break;
    case 0xec27: branch_to(c, !c->c, 0xec3a); break;
    case 0xec29: video_load(c, 0x154d, false); break;
    case 0xec2c: implied(c); c->c = true; break;
    case 0xec2d: sbc_abs16(c, 0x154f); break;
    case 0xec30: store_a_abs16(c, 0x154d); break;
    case 0xec33: branch_to(c, c->c, 0xec44); break;
    case 0xec35: stz_abs16(c, 0x154d); break;
    case 0xec38: branch_to(c, true, 0xec44); break;
    case 0xec3a: video_load(c, 0x154d, false); break;
    case 0xec3d: implied(c); c->c = false; break;
    case 0xec3e: adc_abs16(c, 0x154f); break;
    case 0xec41: store_a_abs16(c, 0x154d); break;
    case 0xec44: sep_rep_native(c, 0x20, true); break;
    case 0xec46: inc_abs8_native(c, 0x1546); break;
    case 0xec49: video_load(c, 0x1546, false); break;
    case 0xec4c: and_constant8(c, 0x7f); break;
    case 0xec4e: lsr_a8(c); break;
    case 0xec4f: lsr_a8(c); break;
    case 0xec50: lsr_a8(c); break;
    case 0xec51: lsr_a8(c); break;
    case 0xec52: implied(c); c->c = false; break;
    case 0xec53: adc8_dp(c, 0x10); break;
    case 0xec55: cmp_dp8(c, 0x11); break;
    case 0xec57: branch_to(c, !c->c, 0xec5b); break;
    case 0xec59: load_dp8(c, 0x11); break;
    case 0xec5b: ora_constant8(c, 0x80); break;
    case 0xec5d: store_register8(c, 0x1558); break;
    case 0xec60: return_short(c); break;
    case 0xec61: video_load(c, 0x1546, false); break;
    case 0xec64: cmp_constant8(c, 0x80); break;
    case 0xec66: branch_to(c, !c->z, 0xec7d); break;
    case 0xec68: byte_constant(c, 0x87); break;
    case 0xec6a: store_register8(c, 0x1558); break;
    case 0xec6d: index_constant(c, true, 0x0040); break;
    case 0xec70: store_y_abs16(c, 0x155a); break;
    case 0xec73: index_constant(c, true, 0x00c0); break;
    case 0xec76: store_y_abs16(c, 0x155c); break;
    case 0xec79: inc_abs8_native(c, 0x1546); break;
    case 0xec7c: return_short(c); break;
    case 0xec7d: byte_constant(c, 0x00); break;
    case 0xec7f: xba_native(c); break;
    case 0xec80: video_load(c, 0x1548, false); break;
    case 0xec83: asl_a8(c); break;
    case 0xec84: asl_a8(c); break;
    case 0xec85: tax16_from_a8(c); break;
    case 0xec86: sep_rep_native(c, 0x20, false); break;
    case 0xec88: video_load(c, 0x155a, false); break;
    case 0xec8b: implied(c); c->c = false; break;
    case 0xec8c: adc_long_x16(c, 0x05ecd4); break;
    case 0xec90: store_a_abs16(c, 0x155a); break;
    case 0xec93: video_load(c, 0x155c, false); break;
    case 0xec96: implied(c); c->c = true; break;
    case 0xec97: sbc_long_x16(c, 0x05ecd6); break;
    case 0xec9b: store_a_abs16(c, 0x155c); break;
    case 0xec9e: sep_rep_native(c, 0x20, true); break;
    case 0xeca0: video_load(c, 0x155a, false); break;
    case 0xeca3: implied(c); c->c = true; break;
    case 0xeca4: sbc8_constant(c, 0x40); break;
    case 0xeca6: lsr_a8(c); break;
    case 0xeca7: lsr_a8(c); break;
    case 0xeca8: lsr_a8(c); break;
    case 0xeca9: lsr_a8(c); break;
    case 0xecaa: implied(c); c->c = false; break;
    case 0xecab: adc8_constant(c, 0x08); break;
    case 0xecad: cmp_constant8(c, 0x0e); break;
    case 0xecaf: branch_to(c, !c->c, 0xecb3); break;
    case 0xecb1: byte_constant(c, 0x0e); break;
    case 0xecb3: ora_constant8(c, 0x80); break;
    case 0xecb5: store_register8(c, 0x1558); break;
    case 0xecb8: video_load(c, 0x155d, false); break;
    case 0xecbb: branch_to(c, c->z, 0xecd3); break;
    case 0xecbd: inc_abs8_native(c, 0x1548); break;
    case 0xecc0: video_load(c, 0x1548, false); break;
    case 0xecc3: cmp_constant8(c, 0x04); break;
    case 0xecc5: branch_to(c, c->c, 0xeccd); break;
    case 0xecc7: byte_constant(c, 0x80); break;
    case 0xecc9: store_register8(c, 0x1546); break;
    case 0xeccc: return_short(c); break;
    case 0xeccd: stz_abs8(c, 0x1558); break;
    case 0xecd0: stz_abs8(c, 0x1546); break;
    case 0xecd3: return_short(c); break;
  }
  return true;
}

/* $05:ED46–$ED87: ($E117,D0/4C-adj) #$83/#$80 → DP+$00; INC $1546; ÷3 via $4204..06;
 * remainder+$00 → $1558; seed $155A/#$68 $155C/#$A0. */
static bool actor_hdl_ed4a_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0xed46: case 0xed48: case 0xed4a: case 0xed4c: case 0xed4e: case 0xed51:
    case 0xed54: case 0xed56: case 0xed58: case 0xed5b: case 0xed5e: case 0xed5f:
    case 0xed62: case 0xed65: case 0xed67: case 0xed6a: case 0xed6b: case 0xed6c:
    case 0xed6d: case 0xed6e: case 0xed6f: case 0xed70: case 0xed71: case 0xed72:
    case 0xed75: case 0xed76: case 0xed78: case 0xed7b: case 0xed7e: case 0xed81:
    case 0xed84: case 0xed87:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xed46: byte_constant(c, 0x83); break;
    case 0xed48: branch_to(c, true, 0xed4c); break;
    case 0xed4a: byte_constant(c, 0x80); break;
    case 0xed4c: store_dp(c, 0x00, true); break;
    case 0xed4e: inc_abs8_native(c, 0x1546); break;
    case 0xed51: video_load(c, 0x1546, false); break;
    case 0xed54: cmp_constant8(c, 0xbc); break;
    case 0xed56: branch_to(c, !c->c, 0xed5f); break;
    case 0xed58: stz_abs8(c, 0x1546); break;
    case 0xed5b: stz_abs8(c, 0x1558); break;
    case 0xed5e: return_short(c); break;
    case 0xed5f: store_register8(c, 0x4204); break;
    case 0xed62: stz_abs8(c, 0x4205); break;
    case 0xed65: byte_constant(c, 0x03); break;
    case 0xed67: store_register8(c, 0x4206); break;
    case 0xed6a: nop_native(c); break;
    case 0xed6b: nop_native(c); break;
    case 0xed6c: nop_native(c); break;
    case 0xed6d: nop_native(c); break;
    case 0xed6e: nop_native(c); break;
    case 0xed6f: nop_native(c); break;
    case 0xed70: nop_native(c); break;
    case 0xed71: nop_native(c); break;
    case 0xed72: video_load(c, 0x4216, false); break;
    case 0xed75: implied(c); c->c = false; break;
    case 0xed76: adc8_dp(c, 0x00); break;
    case 0xed78: store_register8(c, 0x1558); break;
    case 0xed7b: index_constant(c, true, 0x0068); break;
    case 0xed7e: store_y_abs16(c, 0x155a); break;
    case 0xed81: index_constant(c, true, 0x00a0); break;
    case 0xed84: store_y_abs16(c, 0x155c); break;
    case 0xed87: return_short(c); break;
  }
  return true;
}

/* $05:EDB4–$EDFA: ($E117,5C) INC $1546; ≥#$BC clear; else phase $1558; $B6B2+$0034,Y
 * class → $05EDFB,X pair into $155A/$155C. */
static bool actor_hdl_edb4_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0xedb4: case 0xedb7: case 0xedba: case 0xedbc: case 0xedbe: case 0xedc1:
    case 0xedc4: case 0xedc5: case 0xedc6: case 0xedc7: case 0xedc9: case 0xedca:
    case 0xedcc: case 0xedcf: case 0xedd2: case 0xedd5: case 0xedd8: case 0xeddb:
    case 0xedde: case 0xede0: case 0xede2: case 0xede5: case 0xede7: case 0xede9:
    case 0xedec: case 0xedf0: case 0xedf3: case 0xedf7: case 0xedfa:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xedb4: inc_abs8_native(c, 0x1546); break;
    case 0xedb7: video_load(c, 0x1546, false); break;
    case 0xedba: cmp_constant8(c, 0xbc); break;
    case 0xedbc: branch_to(c, !c->c, 0xedc5); break;
    case 0xedbe: stz_abs8(c, 0x1546); break;
    case 0xedc1: stz_abs8(c, 0x1558); break;
    case 0xedc4: return_short(c); break;
    case 0xedc5: lsr_a8(c); break;
    case 0xedc6: lsr_a8(c); break;
    case 0xedc7: and_constant8(c, 0x01); break;
    case 0xedc9: implied(c); c->c = false; break;
    case 0xedca: adc8_constant(c, 0x81); break;
    case 0xedcc: store_register8(c, 0x1558); break;
    case 0xedcf: stz_abs8(c, 0x155b); break;
    case 0xedd2: stz_abs8(c, 0x155d); break;
    case 0xedd5: jsr_abs_native(c, 0xb6b2); break;
    case 0xedd8: index_constant(c, false, 0x0002); break;
    case 0xeddb: load_a_abs_y8(c, 0x0034); break;
    case 0xedde: cmp_constant8(c, 0x05); break;
    case 0xede0: branch_to(c, c->z, 0xedec); break;
    case 0xede2: index_constant(c, false, 0x0004); break;
    case 0xede5: cmp_constant8(c, 0x07); break;
    case 0xede7: branch_to(c, c->z, 0xedec); break;
    case 0xede9: index_constant(c, false, 0x0000); break;
    case 0xedec: load_a_long_x8(c, 0x05edfb); break;
    case 0xedf0: store_register8(c, 0x155a); break;
    case 0xedf3: load_a_long_x8(c, 0x05edfc); break;
    case 0xedf7: store_register8(c, 0x155c); break;
    case 0xedfa: return_short(c); break;
  }
  return true;
}

/* $05:EE01–$EE55: ($E117,5A) INC $1546; ≥#$BC clear; else #$82-phase; DP+$B7/$B6B2 gates
 * → $05EE56,X pair into $155A/$155C. */
static bool actor_hdl_ee01_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  if(!c->mf) return false;
  switch(pc) {
    case 0xee01: case 0xee04: case 0xee07: case 0xee09: case 0xee0b: case 0xee0e:
    case 0xee11: case 0xee12: case 0xee13: case 0xee14: case 0xee16: case 0xee18:
    case 0xee1b: case 0xee1e: case 0xee21: case 0xee23: case 0xee25: case 0xee27:
    case 0xee29: case 0xee2b: case 0xee2e: case 0xee31: case 0xee34: case 0xee36:
    case 0xee38: case 0xee3b: case 0xee3d: case 0xee40: case 0xee42: case 0xee44:
    case 0xee47: case 0xee4b: case 0xee4e: case 0xee52: case 0xee55:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xee01: inc_abs8_native(c, 0x1546); break;
    case 0xee04: video_load(c, 0x1546, false); break;
    case 0xee07: cmp_constant8(c, 0xbc); break;
    case 0xee09: branch_to(c, !c->c, 0xee12); break;
    case 0xee0b: stz_abs8(c, 0x1546); break;
    case 0xee0e: stz_abs8(c, 0x1558); break;
    case 0xee11: return_short(c); break;
    case 0xee12: lsr_a8(c); break;
    case 0xee13: lsr_a8(c); break;
    case 0xee14: and_constant8(c, 0x01); break;
    case 0xee16: ora_constant8(c, 0x82); break;
    case 0xee18: store_register8(c, 0x1558); break;
    case 0xee1b: stz_abs8(c, 0x155b); break;
    case 0xee1e: stz_abs8(c, 0x155d); break;
    case 0xee21: load_dp8(c, 0xb7); break;
    case 0xee23: cmp_constant8(c, 0x60); break;
    case 0xee25: branch_to(c, c->c, 0xee44); break;
    case 0xee27: cmp_constant8(c, 0x29); break;
    case 0xee29: branch_to(c, !c->z, 0xee3d); break;
    case 0xee2b: jsr_abs_native(c, 0xb6b2); break;
    case 0xee2e: load_a_abs_y8(c, 0x0034); break;
    case 0xee31: index_constant(c, false, 0x0000); break;
    case 0xee34: cmp_constant8(c, 0x0e); break;
    case 0xee36: branch_to(c, !c->z, 0xee47); break;
    case 0xee38: index_constant(c, false, 0x0002); break;
    case 0xee3b: branch_to(c, true, 0xee47); break;
    case 0xee3d: index_constant(c, false, 0x0004); break;
    case 0xee40: cmp_constant8(c, 0x30); break;
    case 0xee42: branch_to(c, c->z, 0xee47); break;
    case 0xee44: index_constant(c, false, 0x0006); break;
    case 0xee47: load_a_long_x8(c, 0x05ee56); break;
    case 0xee4b: store_register8(c, 0x155a); break;
    case 0xee4e: load_a_long_x8(c, 0x05ee57); break;
    case 0xee52: store_register8(c, 0x155c); break;
    case 0xee55: return_short(c); break;
  }
  return true;
}

/* $05:E921–$E97A: ($E117,D8) #$80 seed dual scroll banks $1560.. / $1558..; else m16
 * $1564-=4 $1562+=2 until ≥#$E0 then clear. */
static bool actor_hdl_e921_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xe959 || pc == 0xe95c || pc == 0xe95d
                      || pc == 0xe960 || pc == 0xe963 || pc == 0xe966
                      || pc == 0xe967 || pc == 0xe96a || pc == 0xe96d);
  const bool sep_entry = (pc == 0xe970);
  const bool rep_entry = (pc == 0xe957);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xe921: case 0xe924: case 0xe926: case 0xe928: case 0xe92a: case 0xe92d:
    case 0xe930: case 0xe933: case 0xe936: case 0xe939: case 0xe93b: case 0xe93e:
    case 0xe941: case 0xe944: case 0xe947: case 0xe94a: case 0xe94d: case 0xe950:
    case 0xe953: case 0xe956: case 0xe957: case 0xe959: case 0xe95c: case 0xe95d:
    case 0xe960: case 0xe963: case 0xe966: case 0xe967: case 0xe96a: case 0xe96d:
    case 0xe970: case 0xe972: case 0xe974: case 0xe977: case 0xe97a:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe921: video_load(c, 0x1546, false); break;
    case 0xe924: cmp_constant8(c, 0x80); break;
    case 0xe926: branch_to(c, !c->z, 0xe957); break;
    case 0xe928: byte_constant(c, 0x90); break;
    case 0xe92a: store_register8(c, 0x1560); break;
    case 0xe92d: index_constant(c, true, 0x0048); break;
    case 0xe930: store_y_abs16(c, 0x1562); break;
    case 0xe933: index_constant(c, true, 0x00a8); break;
    case 0xe936: store_y_abs16(c, 0x1564); break;
    case 0xe939: byte_constant(c, 0xc0); break;
    case 0xe93b: store_register8(c, 0x1566); break;
    case 0xe93e: index_constant(c, true, 0x1091); break;
    case 0xe941: store_y_abs16(c, 0x1558); break;
    case 0xe944: index_constant(c, true, 0x0040); break;
    case 0xe947: store_y_abs16(c, 0x155a); break;
    case 0xe94a: index_constant(c, true, 0x00a0); break;
    case 0xe94d: store_y_abs16(c, 0x155c); break;
    case 0xe950: stz_abs8(c, 0x155e); break;
    case 0xe953: inc_abs8_native(c, 0x1546); break;
    case 0xe956: return_short(c); break;
    case 0xe957: sep_rep_native(c, 0x20, false); break;
    case 0xe959: video_load(c, 0x1564, false); break;
    case 0xe95c: implied(c); c->c = true; break;
    case 0xe95d: sbc16_constant(c, 0x0004); break;
    case 0xe960: store_a_abs16(c, 0x1564); break;
    case 0xe963: video_load(c, 0x1562, false); break;
    case 0xe966: implied(c); c->c = false; break;
    case 0xe967: adc16_constant(c, 0x0002); break;
    case 0xe96a: store_a_abs16(c, 0x1562); break;
    case 0xe96d: cmp_constant16(c, 0x00e0); break;
    case 0xe970: sep_rep_native(c, 0x20, true); break;
    case 0xe972: branch_to(c, !c->c, 0xe97a); break;
    case 0xe974: stz_abs8(c, 0x1546); break;
    case 0xe977: stz_abs8(c, 0x1560); break;
    case 0xe97a: return_short(c); break;
  }
  return true;
}

/* prior batch retained above */

/* ==== Batch: E117 EE5E/EE99 shared drift + E97B/E9A2/E9D4–EA51 + ED88 + E8DC/E87F/E845 ==== */

/* $05:EE5E–$EED3: ($E117,11A/118) EE5E/EE99 seed paths share $EEBD m16 $155A-=2 drift. */
static bool actor_hdl_ee5e_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xeebf || pc == 0xeec2 || pc == 0xeec3 || pc == 0xeec6);
  const bool sep_entry = (pc == 0xeec9);
  const bool rep_entry = (pc == 0xeebd);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xee5e: case 0xee61: case 0xee63: case 0xee65: case 0xee67: case 0xee6a:
    case 0xee6d: case 0xee70: case 0xee73: case 0xee76: case 0xee79: case 0xee7a:
    case 0xee7b: case 0xee7c: case 0xee7e: case 0xee80: case 0xee83: case 0xee85:
    case 0xee87: case 0xee89: case 0xee8b: case 0xee8e: case 0xee91: case 0xee93:
    case 0xee95: case 0xee98: case 0xee99: case 0xee9c: case 0xee9e: case 0xeea0:
    case 0xeea2: case 0xeea5: case 0xeea8: case 0xeeab: case 0xeeae: case 0xeeb1:
    case 0xeeb4: case 0xeeb6: case 0xeeb9: case 0xeebc: case 0xeebd: case 0xeebf:
    case 0xeec2: case 0xeec3: case 0xeec6: case 0xeec9: case 0xeecb: case 0xeecd:
    case 0xeed0: case 0xeed3:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xee5e: case 0xee99: video_load(c, 0x1546, false); break;
    case 0xee61: case 0xee9c: cmp_constant8(c, 0x80); break;
    case 0xee63: branch_to(c, !c->z, 0xee7a); break;
    case 0xee65: case 0xeea0: byte_constant(c, 0x82); break;
    case 0xee67: case 0xee80: case 0xee8b: case 0xeea2: store_register8(c, 0x1558); break;
    case 0xee6a: index_constant(c, true, 0x0060); break;
    case 0xee6d: case 0xeea8: store_y_abs16(c, 0x155a); break;
    case 0xee70: index_constant(c, true, 0x0080); break;
    case 0xee73: case 0xeeae: store_y_abs16(c, 0x155c); break;
    case 0xee76: case 0xee95: case 0xeeb9: inc_abs8_native(c, 0x1546); break;
    case 0xee79: case 0xee98: case 0xeebc: case 0xeed3: return_short(c); break;
    case 0xee7a: case 0xee7b: lsr_a8(c); break;
    case 0xee7c: and_constant8(c, 0x01); break;
    case 0xee7e: ora_constant8(c, 0x82); break;
    case 0xee83: load_dp8(c, 0xb7); break;
    case 0xee85: cmp_constant8(c, 0x5a); break;
    case 0xee87: branch_to(c, !c->z, 0xee8e); break;
    case 0xee89: byte_constant(c, 0x82); break;
    case 0xee8e: video_load(c, 0x1546, false); break;
    case 0xee91: cmp_constant8(c, 0x9e); break;
    case 0xee93: branch_to(c, c->c, 0xeebd); break;
    case 0xee9e: branch_to(c, !c->z, 0xeebd); break;
    case 0xeea5: index_constant(c, true, 0x00d0); break;
    case 0xeeab: index_constant(c, true, 0x0070); break;
    case 0xeeb1: video_load(c, 0x155e, false); break;
    case 0xeeb4: ora_constant8(c, 0x20); break;
    case 0xeeb6: store_register8(c, 0x155e); break;
    case 0xeebd: sep_rep_native(c, 0x20, false); break;
    case 0xeebf: video_load(c, 0x155a, false); break;
    case 0xeec2: implied(c); c->c = true; break;
    case 0xeec3: sbc16_constant(c, 0x0002); break;
    case 0xeec6: store_a_abs16(c, 0x155a); break;
    case 0xeec9: sep_rep_native(c, 0x20, true); break;
    case 0xeecb: branch_to(c, !c->n, 0xeed3); break;
    case 0xeecd: stz_abs8(c, 0x1546); break;
    case 0xeed0: stz_abs8(c, 0x1558); break;
  }
  return true;
}

/* $05:E97B–$E9A1: ($E117,D4) INC $1546; ≥#$BC clear $1560; else ORA #$84 phase + Y seeds. */
static bool actor_hdl_e97b_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xe97b: case 0xe97e: case 0xe981: case 0xe983: case 0xe985: case 0xe988:
    case 0xe98b: case 0xe98c: case 0xe98d: case 0xe98e: case 0xe990: case 0xe992:
    case 0xe995: case 0xe998: case 0xe99b: case 0xe99e: case 0xe9a1:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe97b: inc_abs8_native(c, 0x1546); break;
    case 0xe97e: video_load(c, 0x1546, false); break;
    case 0xe981: cmp_constant8(c, 0xbc); break;
    case 0xe983: branch_to(c, !c->c, 0xe98c); break;
    case 0xe985: stz_abs8(c, 0x1546); break;
    case 0xe988: stz_abs8(c, 0x1560); break;
    case 0xe98b: case 0xe9a1: return_short(c); break;
    case 0xe98c: case 0xe98d: lsr_a8(c); break;
    case 0xe98e: and_constant8(c, 0x01); break;
    case 0xe990: ora_constant8(c, 0x84); break;
    case 0xe992: store_register8(c, 0x1560); break;
    case 0xe995: index_constant(c, true, 0x0040); break;
    case 0xe998: store_y_abs16(c, 0x1562); break;
    case 0xe99b: index_constant(c, true, 0x00b8); break;
    case 0xe99e: store_y_abs16(c, 0x1564); break;
  }
  return true;
}

/* $05:E9A2–$E9D3: ($E117,D2) bit1 of $1546 picks $155C; ORA $155E; #$92→$1558; DP+$51 gate. */
static bool actor_hdl_e9a2_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xe9a2: case 0xe9a5: case 0xe9a6: case 0xe9a7: case 0xe9aa: case 0xe9ac:
    case 0xe9af: case 0xe9b2: case 0xe9b5: case 0xe9b8: case 0xe9bb: case 0xe9bd:
    case 0xe9c0: case 0xe9c2: case 0xe9c5: case 0xe9c7: case 0xe9c9: case 0xe9cb:
    case 0xe9ce: case 0xe9d0: case 0xe9d3:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe9a2: video_load(c, 0x1546, false); break;
    case 0xe9a5: case 0xe9a6: lsr_a8(c); break;
    case 0xe9a7: index_constant(c, true, 0x0070); break;
    case 0xe9aa: branch_to(c, !c->c, 0xe9af); break;
    case 0xe9ac: index_constant(c, true, 0x0090); break;
    case 0xe9af: store_y_abs16(c, 0x155c); break;
    case 0xe9b2: index_constant(c, true, 0x0050); break;
    case 0xe9b5: store_y_abs16(c, 0x155a); break;
    case 0xe9b8: video_load(c, 0x155e, false); break;
    case 0xe9bb: ora_constant8(c, 0x20); break;
    case 0xe9bd: store_register8(c, 0x155e); break;
    case 0xe9c0: byte_constant(c, 0x92); break;
    case 0xe9c2: store_register8(c, 0x1558); break;
    case 0xe9c5: load_dp8(c, 0x51); break;
    case 0xe9c7: and_constant8(c, 0x03); break;
    case 0xe9c9: branch_to(c, c->z, 0xe9d3); break;
    case 0xe9cb: inc_abs8_native(c, 0x1546); break;
    case 0xe9ce: branch_to(c, !c->z, 0xe9d3); break;
    case 0xe9d0: stz_abs8(c, 0x1558); break;
    case 0xe9d3: return_short(c); break;
  }
  return true;
}

/* $05:E9D4–$EA51: ($E117,CC/CA) E9D4 early seed / phase-adjust BRA into EA0E's $EA2D
 * shared m16 $155C-=3 $155A-=2 drift (Y threshold from entry). */
static bool actor_hdl_e9d4_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xea31 || pc == 0xea34 || pc == 0xea35 || pc == 0xea38
                      || pc == 0xea3b || pc == 0xea3e || pc == 0xea3f || pc == 0xea42
                      || pc == 0xea45);
  const bool sep_entry = (pc == 0xea47);
  const bool rep_entry = (pc == 0xea2f);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xe9d4: case 0xe9d7: case 0xe9d9: case 0xe9db: case 0xe9dd: case 0xe9e0:
    case 0xe9e3: case 0xe9e6: case 0xe9e9: case 0xe9ec: case 0xe9ef: case 0xe9f0:
    case 0xe9f3: case 0xe9f5: case 0xe9f7: case 0xe9fa: case 0xe9fb: case 0xe9fc:
    case 0xe9fd: case 0xe9fe: case 0xe9ff: case 0xea01: case 0xea03: case 0xea04:
    case 0xea06: case 0xea09: case 0xea0c: case 0xea0e: case 0xea11: case 0xea14:
    case 0xea16: case 0xea18: case 0xea1a: case 0xea1d: case 0xea20: case 0xea23:
    case 0xea26: case 0xea29: case 0xea2c: case 0xea2d: case 0xea2f: case 0xea31:
    case 0xea34: case 0xea35: case 0xea38: case 0xea3b: case 0xea3e: case 0xea3f:
    case 0xea42: case 0xea45: case 0xea47: case 0xea49: case 0xea4b: case 0xea4e:
    case 0xea51:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe9d4: case 0xea11: video_load(c, 0x1546, false); break;
    case 0xe9d7: cmp_constant8(c, 0x8a); break;
    case 0xe9d9: branch_to(c, c->c, 0xe9f0); break;
    case 0xe9db: byte_constant(c, 0x93); break;
    case 0xe9dd: case 0xea1a: case 0xea06: store_register8(c, 0x1558); break;
    case 0xe9e0: index_constant(c, true, 0x00a0); break;
    case 0xe9e3: case 0xea20: store_y_abs16(c, 0x155a); break;
    case 0xe9e6: index_constant(c, true, 0x00d0); break;
    case 0xe9e9: case 0xea26: store_y_abs16(c, 0x155c); break;
    case 0xe9ec: case 0xea29: inc_abs8_native(c, 0x1546); break;
    case 0xe9ef: case 0xea2c: case 0xea51: return_short(c); break;
    case 0xe9f0: video_load(c, 0x1558, false); break;
    case 0xe9f3: cmp_constant8(c, 0x97); break;
    case 0xe9f5: branch_to(c, c->z, 0xea09); break;
    case 0xe9f7: video_load(c, 0x155c, false); break;
    case 0xe9fa: case 0xe9fb: case 0xe9fc: case 0xe9fd: case 0xe9fe: lsr_a8(c); break;
    case 0xe9ff: store_dp(c, 0x00, true); break;
    case 0xea01: byte_constant(c, 0x99); break;
    case 0xea03: implied(c); c->c = true; break;
    case 0xea04: sbc_dp8(c, 0x00); break;
    case 0xea09: index_constant(c, true, 0x0040); break;
    case 0xea0c: branch_to(c, true, 0xea2d); break;
    case 0xea0e: index_constant(c, true, 0xfff0); break;
    case 0xea14: cmp_constant8(c, 0x80); break;
    case 0xea16: branch_to(c, !c->z, 0xea2d); break;
    case 0xea18: byte_constant(c, 0x8f); break;
    case 0xea1d: index_constant(c, true, 0x0068); break;
    case 0xea23: index_constant(c, true, 0x00a0); break;
    case 0xea2d: sty_dp16(c, 0x00); break;
    case 0xea2f: sep_rep_native(c, 0x20, false); break;
    case 0xea31: video_load(c, 0x155c, false); break;
    case 0xea34: implied(c); c->c = true; break;
    case 0xea35: sbc16_constant(c, 0x0003); break;
    case 0xea38: store_a_abs16(c, 0x155c); break;
    case 0xea3b: video_load(c, 0x155a, false); break;
    case 0xea3e: implied(c); c->c = true; break;
    case 0xea3f: sbc16_constant(c, 0x0002); break;
    case 0xea42: store_a_abs16(c, 0x155a); break;
    case 0xea45: cmp_dp16(c, 0x00); break;
    case 0xea47: sep_rep_native(c, 0x20, true); break;
    case 0xea49: branch_to(c, !c->z, 0xea51); break;
    case 0xea4b: stz_abs8(c, 0x1546); break;
    case 0xea4e: stz_abs8(c, 0x1558); break;
  }
  return true;
}

/* $05:ED88–$EDB3: ($E117,136) sibling of $EDB4 — #$81-phase into $1560 bank + fixed coords. */
static bool actor_hdl_ed88_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xed88: case 0xed8b: case 0xed8e: case 0xed90: case 0xed92: case 0xed95:
    case 0xed98: case 0xed99: case 0xed9a: case 0xed9b: case 0xed9d: case 0xed9e:
    case 0xeda0: case 0xeda3: case 0xeda6: case 0xeda9: case 0xedab: case 0xedae:
    case 0xedb0: case 0xedb3:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xed88: inc_abs8_native(c, 0x1546); break;
    case 0xed8b: video_load(c, 0x1546, false); break;
    case 0xed8e: cmp_constant8(c, 0xbc); break;
    case 0xed90: branch_to(c, !c->c, 0xed99); break;
    case 0xed92: stz_abs8(c, 0x1546); break;
    case 0xed95: stz_abs8(c, 0x1560); break;
    case 0xed98: case 0xedb3: return_short(c); break;
    case 0xed99: case 0xed9a: lsr_a8(c); break;
    case 0xed9b: and_constant8(c, 0x01); break;
    case 0xed9d: implied(c); c->c = false; break;
    case 0xed9e: adc8_constant(c, 0x81); break;
    case 0xeda0: store_register8(c, 0x1560); break;
    case 0xeda3: stz_abs8(c, 0x1563); break;
    case 0xeda6: stz_abs8(c, 0x1565); break;
    case 0xeda9: byte_constant(c, 0x60); break;
    case 0xedab: store_register8(c, 0x1562); break;
    case 0xedae: byte_constant(c, 0x70); break;
    case 0xedb0: store_register8(c, 0x1564); break;
  }
  return true;
}

/* $05:E8DC–$E920: ($E117,DA) #$80 seed + ORA $155E #$40; else m16 $155C-=4 $155A-=2. */
static bool actor_hdl_e8dc_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xe902 || pc == 0xe905 || pc == 0xe906 || pc == 0xe909
                      || pc == 0xe90c || pc == 0xe90f || pc == 0xe910 || pc == 0xe913);
  const bool sep_entry = (pc == 0xe916);
  const bool rep_entry = (pc == 0xe900);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xe8dc: case 0xe8df: case 0xe8e1: case 0xe8e3: case 0xe8e5: case 0xe8e8:
    case 0xe8eb: case 0xe8ee: case 0xe8f1: case 0xe8f4: case 0xe8f7: case 0xe8f9:
    case 0xe8fc: case 0xe8ff: case 0xe900: case 0xe902: case 0xe905: case 0xe906:
    case 0xe909: case 0xe90c: case 0xe90f: case 0xe910: case 0xe913: case 0xe916:
    case 0xe918: case 0xe91a: case 0xe91d: case 0xe920:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe8dc: video_load(c, 0x1546, false); break;
    case 0xe8df: cmp_constant8(c, 0x80); break;
    case 0xe8e1: branch_to(c, !c->z, 0xe900); break;
    case 0xe8e3: byte_constant(c, 0x90); break;
    case 0xe8e5: store_register8(c, 0x1558); break;
    case 0xe8e8: index_constant(c, true, 0x0050); break;
    case 0xe8eb: store_y_abs16(c, 0x155a); break;
    case 0xe8ee: index_constant(c, true, 0x0090); break;
    case 0xe8f1: store_y_abs16(c, 0x155c); break;
    case 0xe8f4: video_load(c, 0x155e, false); break;
    case 0xe8f7: ora_constant8(c, 0x40); break;
    case 0xe8f9: store_register8(c, 0x155e); break;
    case 0xe8fc: inc_abs8_native(c, 0x1546); break;
    case 0xe8ff: case 0xe920: return_short(c); break;
    case 0xe900: sep_rep_native(c, 0x20, false); break;
    case 0xe902: video_load(c, 0x155c, false); break;
    case 0xe905: implied(c); c->c = true; break;
    case 0xe906: sbc16_constant(c, 0x0004); break;
    case 0xe909: store_a_abs16(c, 0x155c); break;
    case 0xe90c: video_load(c, 0x155a, false); break;
    case 0xe90f: implied(c); c->c = true; break;
    case 0xe910: sbc16_constant(c, 0x0002); break;
    case 0xe913: store_a_abs16(c, 0x155a); break;
    case 0xe916: sep_rep_native(c, 0x20, true); break;
    case 0xe918: branch_to(c, c->c, 0xe920); break;
    case 0xe91a: stz_abs8(c, 0x1546); break;
    case 0xe91d: stz_abs8(c, 0x1558); break;
  }
  return true;
}

/* $05:E87F–$E8DB: ($E117,DE) early seed to #$8A; else #$93 gate + m16 $155C-=3 $155A+=2. */
static bool actor_hdl_e87f_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xe8ba || pc == 0xe8bd || pc == 0xe8be || pc == 0xe8c1
                      || pc == 0xe8c4 || pc == 0xe8c7 || pc == 0xe8c8 || pc == 0xe8cb
                      || pc == 0xe8ce);
  const bool sep_entry = (pc == 0xe8d1);
  const bool rep_entry = (pc == 0xe8b8);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xe87f: case 0xe882: case 0xe884: case 0xe886: case 0xe888: case 0xe88b:
    case 0xe88e: case 0xe891: case 0xe894: case 0xe897: case 0xe89a: case 0xe89c:
    case 0xe89f: case 0xe8a2: case 0xe8a3: case 0xe8a6: case 0xe8a8: case 0xe8aa:
    case 0xe8ad: case 0xe8ae: case 0xe8af: case 0xe8b0: case 0xe8b1: case 0xe8b2:
    case 0xe8b3: case 0xe8b5: case 0xe8b8: case 0xe8ba: case 0xe8bd: case 0xe8be:
    case 0xe8c1: case 0xe8c4: case 0xe8c7: case 0xe8c8: case 0xe8cb: case 0xe8ce:
    case 0xe8d1: case 0xe8d3: case 0xe8d5: case 0xe8d8: case 0xe8db:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe87f: video_load(c, 0x1546, false); break;
    case 0xe882: cmp_constant8(c, 0x8a); break;
    case 0xe884: branch_to(c, c->c, 0xe8a3); break;
    case 0xe886: byte_constant(c, 0x97); break;
    case 0xe888: case 0xe8b5: store_register8(c, 0x1558); break;
    case 0xe88b: index_constant(c, true, 0x0040); break;
    case 0xe88e: store_y_abs16(c, 0x155a); break;
    case 0xe891: index_constant(c, true, 0x00c0); break;
    case 0xe894: store_y_abs16(c, 0x155c); break;
    case 0xe897: video_load(c, 0x155e, false); break;
    case 0xe89a: ora_constant8(c, 0x80); break;
    case 0xe89c: store_register8(c, 0x155e); break;
    case 0xe89f: inc_abs8_native(c, 0x1546); break;
    case 0xe8a2: case 0xe8db: return_short(c); break;
    case 0xe8a3: video_load(c, 0x1558, false); break;
    case 0xe8a6: cmp_constant8(c, 0x93); break;
    case 0xe8a8: branch_to(c, c->z, 0xe8b8); break;
    case 0xe8aa: video_load(c, 0x155c, false); break;
    case 0xe8ad: case 0xe8ae: case 0xe8af: case 0xe8b0: case 0xe8b1: lsr_a8(c); break;
    case 0xe8b2: implied(c); c->c = false; break;
    case 0xe8b3: adc8_constant(c, 0x91); break;
    case 0xe8b8: sep_rep_native(c, 0x20, false); break;
    case 0xe8ba: video_load(c, 0x155c, false); break;
    case 0xe8bd: implied(c); c->c = true; break;
    case 0xe8be: sbc16_constant(c, 0x0003); break;
    case 0xe8c1: store_a_abs16(c, 0x155c); break;
    case 0xe8c4: video_load(c, 0x155a, false); break;
    case 0xe8c7: implied(c); c->c = false; break;
    case 0xe8c8: adc16_constant(c, 0x0002); break;
    case 0xe8cb: store_a_abs16(c, 0x155a); break;
    case 0xe8ce: cmp_constant16(c, 0x00f0); break;
    case 0xe8d1: sep_rep_native(c, 0x20, true); break;
    case 0xe8d3: branch_to(c, !c->c, 0xe8db); break;
    case 0xe8d5: stz_abs8(c, 0x1546); break;
    case 0xe8d8: stz_abs8(c, 0x1558); break;
  }
  return true;
}

/* $05:E845–$E87E: ($E117,E2) INC phase; ≥#$BC clear; else DP+$00/#$80+# (≥#$9E) → $1558. */
static bool actor_hdl_e845_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xe845: case 0xe847: case 0xe849: case 0xe84c: case 0xe84f: case 0xe851:
    case 0xe853: case 0xe856: case 0xe859: case 0xe85a: case 0xe85c: case 0xe85e:
    case 0xe860: case 0xe861: case 0xe862: case 0xe864: case 0xe865: case 0xe867:
    case 0xe86a: case 0xe86d: case 0xe870: case 0xe873: case 0xe876: case 0xe879:
    case 0xe87b: case 0xe87e:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe845: byte_constant(c, 0x80); break;
    case 0xe847: store_dp(c, 0x00, true); break;
    case 0xe849: inc_abs8_native(c, 0x1546); break;
    case 0xe84c: video_load(c, 0x1546, false); break;
    case 0xe84f: cmp_constant8(c, 0xbc); break;
    case 0xe851: branch_to(c, !c->c, 0xe85a); break;
    case 0xe853: stz_abs8(c, 0x1546); break;
    case 0xe856: stz_abs8(c, 0x1558); break;
    case 0xe859: case 0xe87e: return_short(c); break;
    case 0xe85a: cmp_constant8(c, 0x9e); break;
    case 0xe85c: branch_to(c, !c->c, 0xe860); break;
    case 0xe85e: inc_dp8(c, 0x00); break;
    case 0xe860: case 0xe861: lsr_a8(c); break;
    case 0xe862: and_constant8(c, 0x01); break;
    case 0xe864: implied(c); c->c = false; break;
    case 0xe865: adc8_dp(c, 0x00); break;
    case 0xe867: store_register8(c, 0x1558); break;
    case 0xe86a: index_constant(c, true, 0x004c); break;
    case 0xe86d: store_y_abs16(c, 0x155a); break;
    case 0xe870: index_constant(c, true, 0x0070); break;
    case 0xe873: store_y_abs16(c, 0x155c); break;
    case 0xe876: video_load(c, 0x155e, false); break;
    case 0xe879: ora_constant8(c, 0x20); break;
    case 0xe87b: store_register8(c, 0x155e); break;
  }
  return true;
}

/* net-new: 258 sites, 597 bytes (EE5E–EED3 50/118, E97B 17/39, E9A2 21/50,
 * E9D4–EA51 55/126, ED88 20/44, E8DC 28/69, E87F 41/93, E845 26/58) */

/* ==== Batch: E117 E7FF ÷ + E6xx/E5xx leaves (E6D6/E6B1/E61B/E5AA) ==== */

/* $05:E7FF–$E844: ($E117,E4) — m16 (DP+$51>>2)÷3 via $4204/06; rem+class → $1558;
 * $B6B2 slot; $0001,Y==#$21 ? #$44 : #$60 → $155A; #$70/$155C; ORA $155E #$20. */
static bool actor_hdl_e7ff_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xe801 || pc == 0xe803 || pc == 0xe804 || pc == 0xe805);
  const bool sep_entry = (pc == 0xe808);
  const bool rep_entry = (pc == 0xe7ff);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xe7ff: case 0xe801: case 0xe803: case 0xe804: case 0xe805: case 0xe808:
    case 0xe80a: case 0xe80c: case 0xe80f: case 0xe811: case 0xe812: case 0xe814:
    case 0xe816: case 0xe818: case 0xe81a: case 0xe81c: case 0xe81d: case 0xe820:
    case 0xe823: case 0xe826: case 0xe829: case 0xe82c: case 0xe82e: case 0xe830:
    case 0xe833: case 0xe836: case 0xe839: case 0xe83c: case 0xe83f: case 0xe841:
    case 0xe844:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe7ff: sep_rep_native(c, 0x20, false); break;
    case 0xe801: load_dp16_to_a(c, 0x51); break;
    case 0xe803: case 0xe804: lsr_a16(c); break;
    case 0xe805: store_a_abs16(c, 0x4204); break;
    case 0xe808: sep_rep_native(c, 0x20, true); break;
    case 0xe80a: byte_constant(c, 0x03); break;
    case 0xe80c: store_register8(c, 0x4206); break;
    case 0xe80f: load_dp8(c, 0xb7); break;
    case 0xe811: implied(c); c->c = true; break;
    case 0xe812: sbc8_constant(c, 0x36); break;
    case 0xe814: cmp_constant8(c, 0x02); break;
    case 0xe816: branch_to(c, !c->c, 0xe81a); break;
    case 0xe818: byte_constant(c, 0x01); break;
    case 0xe81a: ora_constant8(c, 0x80); break;
    case 0xe81c: implied(c); c->c = false; break;
    case 0xe81d: adc_abs8(c, 0x4216); break;
    case 0xe820: store_register8(c, 0x1558); break;
    case 0xe823: jsr_abs_native(c, 0xb6b2); break;
    case 0xe826: index_constant(c, false, 0x0044); break;
    case 0xe829: load_a_abs_y8(c, 0x0001); break;
    case 0xe82c: cmp_constant8(c, 0x21); break;
    case 0xe82e: branch_to(c, c->z, 0xe833); break;
    case 0xe830: index_constant(c, false, 0x0060); break;
    case 0xe833: stx_abs16(c, 0x155a); break;
    case 0xe836: index_constant(c, true, 0x0070); break;
    case 0xe839: store_y_abs16(c, 0x155c); break;
    case 0xe83c: video_load(c, 0x155e, false); break;
    case 0xe83f: ora_constant8(c, 0x20); break;
    case 0xe841: store_register8(c, 0x155e); break;
    case 0xe844: return_short(c); break;
  }
  return true;
}

/* $05:E6B1–$E6D5: ($E117,EE) INC $1546; ≥#$BC clear $1560; else (#>>2)&1+#$81 → $1560
 * + #$60/#$60 → $1562/$1564. */
static bool actor_hdl_e6b1_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xe6b1: case 0xe6b4: case 0xe6b7: case 0xe6b9: case 0xe6bb: case 0xe6be:
    case 0xe6c1: case 0xe6c2: case 0xe6c3: case 0xe6c4: case 0xe6c6: case 0xe6c7:
    case 0xe6c9: case 0xe6cc: case 0xe6cf: case 0xe6d2: case 0xe6d5:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe6b1: inc_abs8_native(c, 0x1546); break;
    case 0xe6b4: video_load(c, 0x1546, false); break;
    case 0xe6b7: cmp_constant8(c, 0xbc); break;
    case 0xe6b9: branch_to(c, !c->c, 0xe6c2); break;
    case 0xe6bb: stz_abs8(c, 0x1546); break;
    case 0xe6be: stz_abs8(c, 0x1560); break;
    case 0xe6c1: case 0xe6d5: return_short(c); break;
    case 0xe6c2: case 0xe6c3: lsr_a8(c); break;
    case 0xe6c4: and_constant8(c, 0x01); break;
    case 0xe6c6: implied(c); c->c = false; break;
    case 0xe6c7: adc8_constant(c, 0x81); break;
    case 0xe6c9: store_register8(c, 0x1560); break;
    case 0xe6cc: index_constant(c, true, 0x0060); break;
    case 0xe6cf: store_y_abs16(c, 0x1562); break;
    case 0xe6d2: store_y_abs16(c, 0x1564); break;
  }
  return true;
}

/* $05:E61B–$E6B0: ($E117,F4/F2/F0) E61B dual-scroll nudge; E655/E659 seed via $E68A;
 * shared DEC $155A×3/$156A×2 + INC/clear tail. */
static bool actor_hdl_e61b_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xe61b: case 0xe61e: case 0xe620: case 0xe622: case 0xe624: case 0xe627:
    case 0xe62a: case 0xe62b: case 0xe62d: case 0xe630: case 0xe631: case 0xe633:
    case 0xe636: case 0xe639: case 0xe63a: case 0xe63c: case 0xe63f: case 0xe641:
    case 0xe644: case 0xe645: case 0xe647: case 0xe64a: case 0xe64d: case 0xe64e:
    case 0xe650: case 0xe653: case 0xe655: case 0xe657: case 0xe659: case 0xe65b:
    case 0xe65d: case 0xe660: case 0xe662: case 0xe664: case 0xe666: case 0xe669:
    case 0xe66c: case 0xe66f: case 0xe672: case 0xe675: case 0xe678: case 0xe67b:
    case 0xe67d: case 0xe680: case 0xe683: case 0xe686: case 0xe689: case 0xe68a:
    case 0xe68d: case 0xe690: case 0xe693: case 0xe696: case 0xe698: case 0xe69b:
    case 0xe69e: case 0xe6a1: case 0xe6a4: case 0xe6a7: case 0xe6aa: case 0xe6ad:
    case 0xe6b0:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe61b: video_load(c, 0x1546, false); break;
    case 0xe61e: cmp_constant8(c, 0x80); break;
    case 0xe620: branch_to(c, !c->z, 0xe627); break;
    case 0xe622: byte_constant(c, 0xa5); break;
    case 0xe624: case 0xe666: jsr_abs_native(c, 0xe68a); break;
    case 0xe627: video_load(c, 0x14c2, false); break;
    case 0xe62a: lsr_a8(c); break;
    case 0xe62b: branch_to(c, !c->c, 0xe641); break;
    case 0xe62d: video_load(c, 0x155c, false); break;
    case 0xe630: implied(c); c->c = true; break;
    case 0xe631: sbc8_constant(c, 0x06); break;
    case 0xe633: store_register8(c, 0x155c); break;
    case 0xe636: video_load(c, 0x156c, false); break;
    case 0xe639: implied(c); c->c = true; break;
    case 0xe63a: sbc8_constant(c, 0x04); break;
    case 0xe63c: store_register8(c, 0x156c); break;
    case 0xe63f: branch_to(c, true, 0xe653); break;
    case 0xe641: video_load(c, 0x155c, false); break;
    case 0xe644: implied(c); c->c = false; break;
    case 0xe645: adc8_constant(c, 0x06); break;
    case 0xe647: store_register8(c, 0x155c); break;
    case 0xe64a: video_load(c, 0x156c, false); break;
    case 0xe64d: implied(c); c->c = false; break;
    case 0xe64e: adc8_constant(c, 0x04); break;
    case 0xe650: store_register8(c, 0x156c); break;
    case 0xe653: branch_to(c, true, 0xe678); break;
    case 0xe655: byte_constant(c, 0xa5); break;
    case 0xe657: branch_to(c, true, 0xe65b); break;
    case 0xe659: byte_constant(c, 0xa4); break;
    case 0xe65b: store_dp(c, 0x00, true); break;
    case 0xe65d: video_load(c, 0x1546, false); break;
    case 0xe660: cmp_constant8(c, 0x80); break;
    case 0xe662: branch_to(c, !c->z, 0xe669); break;
    case 0xe664: load_dp8(c, 0x00); break;
    case 0xe669: case 0xe66c: case 0xe66f: dec_abs8_native(c, 0x155a); break;
    case 0xe672: case 0xe675: dec_abs8_native(c, 0x156a); break;
    case 0xe678: inc_abs8_native(c, 0x1546); break;
    case 0xe67b: branch_to(c, !c->z, 0xe689); break;
    case 0xe67d: stz_abs8(c, 0x1558); break;
    case 0xe680: stz_abs8(c, 0x1568); break;
    case 0xe683: stz_abs8(c, 0x155f); break;
    case 0xe686: stz_abs8(c, 0x156f); break;
    case 0xe689: case 0xe6b0: return_short(c); break;
    case 0xe68a: store_register8(c, 0x1558); break;
    case 0xe68d: store_register8(c, 0x1568); break;
    case 0xe690: stz_abs8(c, 0x155a); break;
    case 0xe693: stz_abs8(c, 0x155c); break;
    case 0xe696: byte_constant(c, 0x80); break;
    case 0xe698: store_register8(c, 0x156a); break;
    case 0xe69b: store_register8(c, 0x156c); break;
    case 0xe69e: store_register8(c, 0x155f); break;
    case 0xe6a1: store_register8(c, 0x156f); break;
    case 0xe6a4: video_load(c, 0x155e, false); break;
    case 0xe6a7: store_register8(c, 0x156e); break;
    case 0xe6aa: video_load(c, 0x1559, false); break;
    case 0xe6ad: store_register8(c, 0x1569); break;
  }
  return true;
}

/* $05:E5AA–$E61A: ($E117,FA) #$80 seed; else $B6B2±3 drift on $155A/$155C; phase
 * clamp to #$8E via (A&7F)>>3+9. */
static bool actor_hdl_e5aa_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xe5e4 || pc == 0xe5e5 || pc == 0xe5e6 || pc == 0xe5e9
                      || pc == 0xe5ec || pc == 0xe5ed || pc == 0xe5ee || pc == 0xe5f1);
  const bool sep_entry = (pc == 0xe5f4);
  const bool rep_entry = (pc == 0xe5e2);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xe5aa: case 0xe5ad: case 0xe5af: case 0xe5b1: case 0xe5b3: case 0xe5b6:
    case 0xe5b9: case 0xe5bc: case 0xe5bf: case 0xe5c2: case 0xe5c5: case 0xe5c6:
    case 0xe5c9: case 0xe5cc: case 0xe5cf: case 0xe5d0: case 0xe5d2: case 0xe5d5:
    case 0xe5d8: case 0xe5db: case 0xe5dd: case 0xe5df: case 0xe5e2: case 0xe5e4:
    case 0xe5e5: case 0xe5e6: case 0xe5e9: case 0xe5ec: case 0xe5ed: case 0xe5ee:
    case 0xe5f1: case 0xe5f4: case 0xe5f6: case 0xe5f9: case 0xe5fc: case 0xe5fe:
    case 0xe600: case 0xe603: case 0xe606: case 0xe607: case 0xe609: case 0xe60a:
    case 0xe60b: case 0xe60c: case 0xe60d: case 0xe60f: case 0xe611: case 0xe613:
    case 0xe615: case 0xe617: case 0xe61a:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe5aa: video_load(c, 0x1546, false); break;
    case 0xe5ad: cmp_constant8(c, 0x80); break;
    case 0xe5af: branch_to(c, !c->z, 0xe5c6); break;
    case 0xe5b1: byte_constant(c, 0x89); break;
    case 0xe5b3: store_register8(c, 0x1558); break;
    case 0xe5b6: index_constant(c, true, 0x0060); break;
    case 0xe5b9: store_y_abs16(c, 0x155a); break;
    case 0xe5bc: index_constant(c, true, 0x0080); break;
    case 0xe5bf: store_y_abs16(c, 0x155c); break;
    case 0xe5c2: inc_abs8_native(c, 0x1546); break;
    case 0xe5c5: case 0xe606: case 0xe61a: return_short(c); break;
    case 0xe5c6: jsr_abs_native(c, 0xb6b2); break;
    case 0xe5c9: index_constant(c, false, 0xfffd); break;
    case 0xe5cc: load_a_abs_y8(c, 0x0021); break;
    case 0xe5cf: lsr_a8(c); break;
    case 0xe5d0: branch_to(c, !c->c, 0xe5d5); break;
    case 0xe5d2: index_constant(c, false, 0x0003); break;
    case 0xe5d5: index_constant(c, true, 0xfffd); break;
    case 0xe5d8: video_load(c, 0x1546, false); break;
    case 0xe5db: cmp_constant8(c, 0x8c); break;
    case 0xe5dd: branch_to(c, c->c, 0xe5e2); break;
    case 0xe5df: index_constant(c, true, 0x0003); break;
    case 0xe5e2: sep_rep_native(c, 0x20, false); break;
    case 0xe5e4: tya16(c); break;
    case 0xe5e5: implied(c); c->c = false; break;
    case 0xe5e6: adc_abs16(c, 0x155a); break;
    case 0xe5e9: store_a_abs16(c, 0x155a); break;
    case 0xe5ec: txa16(c); break;
    case 0xe5ed: implied(c); c->c = false; break;
    case 0xe5ee: adc_abs16(c, 0x155c); break;
    case 0xe5f1: store_a_abs16(c, 0x155c); break;
    case 0xe5f4: sep_rep_native(c, 0x20, true); break;
    case 0xe5f6: inc_abs8_native(c, 0x1546); break;
    case 0xe5f9: video_load(c, 0x1546, false); break;
    case 0xe5fc: cmp_constant8(c, 0xc0); break;
    case 0xe5fe: branch_to(c, !c->c, 0xe607); break;
    case 0xe600: stz_abs8(c, 0x1558); break;
    case 0xe603: stz_abs8(c, 0x1546); break;
    case 0xe607: and_constant8(c, 0x7f); break;
    case 0xe609: case 0xe60a: case 0xe60b: lsr_a8(c); break;
    case 0xe60c: implied(c); c->c = false; break;
    case 0xe60d: adc8_constant(c, 0x09); break;
    case 0xe60f: cmp_constant8(c, 0x0e); break;
    case 0xe611: branch_to(c, !c->c, 0xe615); break;
    case 0xe613: byte_constant(c, 0x0e); break;
    case 0xe615: ora_constant8(c, 0x80); break;
    case 0xe617: store_register8(c, 0x1558); break;
  }
  return true;
}

/* $05:E6D6–$E7FE: ($E117,EC/116) phase #$80 stream expand via $F998/$F91E;
 * #$81 queue seed via $F685; else ±3 $17DD scroll until bound. */
static bool actor_hdl_e6d6_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xe70c || pc == 0xe70f || pc == 0xe710 || pc == 0xe713 || pc == 0xe716 || pc == 0xe723
                      || pc == 0xe726 || pc == 0xe727 || pc == 0xe72a || pc == 0xe740 || pc == 0xe743 || pc == 0xe747
                      || pc == 0xe749 || pc == 0xe74d || pc == 0xe74f || pc == 0xe752 || pc == 0xe753 || pc == 0xe755
                      || pc == 0xe759 || pc == 0xe75a || pc == 0xe75b || pc == 0xe75d || pc == 0xe761 || pc == 0xe762
                      || pc == 0xe763 || pc == 0xe764 || pc == 0xe765 || pc == 0xe768 || pc == 0xe769 || pc == 0xe76c
                      || pc == 0xe76e || pc == 0xe771 || pc == 0xe773 || pc == 0xe777 || pc == 0xe778 || pc == 0xe779
                      || pc == 0xe77b || pc == 0xe77f || pc == 0xe780 || pc == 0xe781 || pc == 0xe783 || pc == 0xe787
                      || pc == 0xe788 || pc == 0xe789 || pc == 0xe78b || pc == 0xe78f || pc == 0xe790 || pc == 0xe791
                      || pc == 0xe794 || pc == 0xe795 || pc == 0xe796 || pc == 0xe797 || pc == 0xe79a || pc == 0xe7b3
                      || pc == 0xe7b6 || pc == 0xe7b8 || pc == 0xe7bc || pc == 0xe7bd || pc == 0xe7bf || pc == 0xe7c2
                      || pc == 0xe7c6 || pc == 0xe7c7 || pc == 0xe7ca || pc == 0xe7cc || pc == 0xe7cd || pc == 0xe7ce
                      || pc == 0xe7cf || pc == 0xe7d0 || pc == 0xe7d2 || pc == 0xe7d4 || pc == 0xe7d5 || pc == 0xe7d6
                      || pc == 0xe7d9 || pc == 0xe7da || pc == 0xe7dd || pc == 0xe7df || pc == 0xe7e2 || pc == 0xe7e5
                      || pc == 0xe7e9 || pc == 0xe7ea || pc == 0xe7eb || pc == 0xe7ee);
  const bool sep_entry = (pc == 0xe719 || pc == 0xe72d || pc == 0xe79c || pc == 0xe7f0);
  const bool rep_entry = (pc == 0xe70a || pc == 0xe721 || pc == 0xe73e || pc == 0xe7b1);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xe6d6: case 0xe6d9: case 0xe6db: case 0xe6dd: case 0xe6df: case 0xe6e1:
    case 0xe6e4: case 0xe6e6: case 0xe6e9: case 0xe6ec: case 0xe6ef: case 0xe6f1:
    case 0xe6f3: case 0xe6f6: case 0xe6f9: case 0xe6fc: case 0xe6ff: case 0xe702:
    case 0xe703: case 0xe706: case 0xe708: case 0xe70a: case 0xe70c: case 0xe70f:
    case 0xe710: case 0xe713: case 0xe716: case 0xe719: case 0xe71b: case 0xe71d:
    case 0xe720: case 0xe721: case 0xe723: case 0xe726: case 0xe727: case 0xe72a:
    case 0xe72d: case 0xe72f: case 0xe731: case 0xe734: case 0xe735: case 0xe738:
    case 0xe73a: case 0xe73c: case 0xe73e: case 0xe740: case 0xe743: case 0xe747:
    case 0xe749: case 0xe74d: case 0xe74f: case 0xe752: case 0xe753: case 0xe755:
    case 0xe759: case 0xe75a: case 0xe75b: case 0xe75d: case 0xe761: case 0xe762:
    case 0xe763: case 0xe764: case 0xe765: case 0xe768: case 0xe769: case 0xe76c:
    case 0xe76e: case 0xe771: case 0xe773: case 0xe777: case 0xe778: case 0xe779:
    case 0xe77b: case 0xe77f: case 0xe780: case 0xe781: case 0xe783: case 0xe787:
    case 0xe788: case 0xe789: case 0xe78b: case 0xe78f: case 0xe790: case 0xe791:
    case 0xe794: case 0xe795: case 0xe796: case 0xe797: case 0xe79a: case 0xe79c:
    case 0xe79e: case 0xe7a1: case 0xe7a3: case 0xe7a5: case 0xe7a7: case 0xe7a9:
    case 0xe7ac: case 0xe7ae: case 0xe7b1: case 0xe7b3: case 0xe7b6: case 0xe7b8:
    case 0xe7bc: case 0xe7bd: case 0xe7bf: case 0xe7c2: case 0xe7c6: case 0xe7c7:
    case 0xe7ca: case 0xe7cc: case 0xe7cd: case 0xe7ce: case 0xe7cf: case 0xe7d0:
    case 0xe7d2: case 0xe7d4: case 0xe7d5: case 0xe7d6: case 0xe7d9: case 0xe7da:
    case 0xe7dd: case 0xe7df: case 0xe7e2: case 0xe7e5: case 0xe7e9: case 0xe7ea:
    case 0xe7eb: case 0xe7ee: case 0xe7f0: case 0xe7f2: case 0xe7f5: case 0xe7f8:
    case 0xe7fb: case 0xe7fe:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe6d6: video_load(c, 0x1546, false); break;
    case 0xe6d9: cmp_constant8(c, 0x80); break;
    case 0xe6db: branch_to(c, c->z, 0xe735); break;
    case 0xe6dd: cmp_constant8(c, 0x81); break;
    case 0xe6df: branch_to(c, !c->z, 0xe703); break;
    case 0xe6e1: jsr_abs_native(c, 0xf685); break;
    case 0xe6e4: byte_constant(c, 0x62); break;
    case 0xe6e6: store_register8(c, 0x2108); break;
    case 0xe6e9: index_constant(c, true, 0x0100); break;
    case 0xe6ec: video_load(c, 0x1540, false); break;
    case 0xe6ef: cmp_constant8(c, 0x76); break;
    case 0xe6f1: branch_to(c, c->z, 0xe6f6); break;
    case 0xe6f3: index_constant(c, true, 0x0140); break;
    case 0xe6f6: store_y_abs16(c, 0x17dd); break;
    case 0xe6f9: index_constant(c, true, 0x0000); break;
    case 0xe6fc: store_y_abs16(c, 0x17db); break;
    case 0xe6ff: case 0xe7fb: inc_abs8_native(c, 0x1546); break;
    case 0xe702: case 0xe720: case 0xe734: case 0xe7fe: return_short(c); break;
    case 0xe703: video_load(c, 0x1540, false); break;
    case 0xe706: cmp_constant8(c, 0x76); break;
    case 0xe708: branch_to(c, c->z, 0xe721); break;
    case 0xe70a: case 0xe721: case 0xe73e: case 0xe7b1: sep_rep_native(c, 0x20, false); break;
    case 0xe70c: case 0xe723: video_load(c, 0x17dd, false); break;
    case 0xe70f: implied(c); c->c = false; break;
    case 0xe710: adc16_constant(c, 0x0003); break;
    case 0xe713: case 0xe72a: store_a_abs16(c, 0x17dd); break;
    case 0xe716: cmp_constant16(c, 0x0240); break;
    case 0xe719: case 0xe72d: case 0xe79c: case 0xe7f0: sep_rep_native(c, 0x20, true); break;
    case 0xe71b: branch_to(c, !c->c, 0xe734); break;
    case 0xe71d: case 0xe731: stz_abs8(c, 0x1546); break;
    case 0xe726: implied(c); c->c = true; break;
    case 0xe727: sbc16_constant(c, 0x0003); break;
    case 0xe72f: branch_to(c, c->c, 0xe734); break;
    case 0xe735: jsr_abs_native(c, 0xf998); break;
    case 0xe738: byte_constant(c, 0x17); break;
    case 0xe73a: store_dp(c, 0x02, true); break;
    case 0xe73c: store_dp(c, 0x06, true); break;
    case 0xe740: index_constant(c, false, 0x0010); break;
    case 0xe743: lda_long_x16(c, 0x178000); break;
    case 0xe747: store_dp(c, 0x04, false); break;
    case 0xe749: lda_long_x16(c, 0x178018); break;
    case 0xe74d: store_dp(c, 0x00, false); break;
    case 0xe74f: index_constant(c, false, 0x0000); break;
    case 0xe752: implied(c); c->y = c->x; zn(c, c->y, false); break; /* TXY */
    case 0xe753: case 0xe75b: lda_indirect_long_y16(c, 0x00); break;
    case 0xe755: store_a_long_x16(c, 0x7f001e); break;
    case 0xe759: case 0xe75a: case 0xe777: case 0xe778: case 0xe77f: case 0xe780:
    case 0xe787: case 0xe788: case 0xe795: case 0xe796:
      implied(c); c->y++; zn(c, c->y, false); break;
    case 0xe75d: store_a_long_x16(c, 0x7f0020); break;
    case 0xe761: case 0xe762: implied(c); c->y--; zn(c, c->y, false); break;
    case 0xe763: case 0xe78f: txa16(c); break;
    case 0xe7d4: tya16(c); break;
    case 0xe764: case 0xe790: implied(c); c->c = false; break;
    case 0xe765: case 0xe791: adc16_constant(c, 0x0040); break;
    case 0xe768: case 0xe794: tax16(c); break;
    case 0xe769: cpx16_constant(c, 0x05c0); break;
    case 0xe76c: branch_to(c, !c->c, 0xe753); break;
    case 0xe76e: index_constant(c, true, 0x0000); break;
    case 0xe771: case 0xe779: case 0xe781: case 0xe789: lda_indirect_long_y16(c, 0x04); break;
    case 0xe773: store_a_long_x16(c, 0x7f001c); break;
    case 0xe77b: store_a_long_x16(c, 0x7f001e); break;
    case 0xe783: store_a_long_x16(c, 0x7f0020); break;
    case 0xe78b: store_a_long_x16(c, 0x7f0022); break;
    case 0xe797: cpy16_constant(c, 0x0028); break;
    case 0xe79a: branch_to(c, !c->c, 0xe771); break;
    case 0xe79e: video_load(c, 0x1540, false); break;
    case 0xe7a1: cmp_constant8(c, 0x76); break;
    case 0xe7a3: branch_to(c, c->z, 0xe7f2); break;
    case 0xe7a5: byte_constant(c, 0x7f); break;
    case 0xe7a7: store_dp(c, 0x02, true); break;
    case 0xe7a9: index_constant(c, false, 0x0000); break;
    case 0xe7ac: stx_dp16(c, 0x00); break;
    case 0xe7ae: index_constant(c, true, 0x07c0); break;
    case 0xe7b3: lda16_constant(c, 0x001f); break;
    case 0xe7b6: store_dp(c, 0x10, false); break;
    case 0xe7b8: lda_long_x16(c, 0x7f0000); break;
    case 0xe7bc: pha16(c); break;
    case 0xe7bd: lda_indirect_long_y16(c, 0x00); break;
    case 0xe7bf: eor_constant16(c, 0x8000); break;
    case 0xe7c2: store_a_long_x16(c, 0x7f0000); break;
    case 0xe7c6: pla16(c); break;
    case 0xe7c7: eor_constant16(c, 0x8000); break;
    case 0xe7ca: sta_indirect_long_y16(c, 0x00); break;
    case 0xe7cc: case 0xe7cd: case 0xe7e9: case 0xe7ea:
      implied(c); c->x++; zn(c, c->x, false); break;
    case 0xe7ce: case 0xe7cf: implied(c); c->y++; zn(c, c->y, false); break;
    case 0xe7d0: dec_dp16(c, 0x10); break;
    case 0xe7d2: branch_to(c, !c->n, 0xe7b8); break;
    case 0xe7d5: implied(c); c->c = true; break;
    case 0xe7d6: sbc16_constant(c, 0x0080); break;
    case 0xe7d9: tay16(c); break;
    case 0xe7da: cpx16_constant(c, 0x0280); break;
    case 0xe7dd: branch_to(c, !c->c, 0xe7b3); break;
    case 0xe7df: index_constant(c, false, 0x07c0); break;
    case 0xe7e2: lda16_constant(c, 0x0000); break;
    case 0xe7e5: store_a_long_x16(c, 0x7f0000); break;
    case 0xe7eb: cpx16_constant(c, 0x0800); break;
    case 0xe7ee: branch_to(c, !c->c, 0xe7e5); break;
    case 0xe7f2: jsr_abs_native(c, 0xf91e); break;
    case 0xe7f5: index_constant(c, true, 0x0800); break;
    case 0xe7f8: store_y_abs16(c, 0x0806); break;
  }
  return true;
}

/* net-new: 324 sites, 736 bytes (E7FF 31/70, E6D6 134/297, E6B1 17/37,
 * E61B+E68A 61/150, E5AA 51/113, E565 30/69) */

/* $05:E565–$E5A9: ($E117,FA-1) INC $1546; #$80 seed #$85/#$60/#$80; <$A0 toggle
 * $155B/$155F bit2; ≥#$C0 clear; else (A&4)>>1|#$60 → $155A. */
static bool actor_hdl_e565_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xe565: case 0xe568: case 0xe56b: case 0xe56d: case 0xe56f: case 0xe571:
    case 0xe574: case 0xe577: case 0xe57a: case 0xe57d: case 0xe580: case 0xe581:
    case 0xe583: case 0xe585: case 0xe587: case 0xe58a: case 0xe58c: case 0xe58f:
    case 0xe590: case 0xe592: case 0xe594: case 0xe597: case 0xe59a: case 0xe59d:
    case 0xe59e: case 0xe5a0: case 0xe5a1: case 0xe5a3: case 0xe5a6: case 0xe5a9:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe565: video_load(c, 0x1546, false); break;
    case 0xe568: inc_abs8_native(c, 0x1546); break;
    case 0xe56b: cmp_constant8(c, 0x80); break;
    case 0xe56d: branch_to(c, !c->z, 0xe581); break;
    case 0xe56f: byte_constant(c, 0x85); break;
    case 0xe571: store_register8(c, 0x1558); break;
    case 0xe574: index_constant(c, true, 0x0060); break;
    case 0xe577: store_y_abs16(c, 0x155a); break;
    case 0xe57a: index_constant(c, true, 0x0080); break;
    case 0xe57d: store_y_abs16(c, 0x155c); break;
    case 0xe580: case 0xe58f: case 0xe59d: case 0xe5a9: return_short(c); break;
    case 0xe581: cmp_constant8(c, 0xa0); break;
    case 0xe583: branch_to(c, c->c, 0xe590); break;
    case 0xe585: and_constant8(c, 0x04); break;
    case 0xe587: store_register8(c, 0x155b); break;
    case 0xe58a: eor_constant8(c, 0x04); break;
    case 0xe58c: store_register8(c, 0x155f); break;
    case 0xe590: cmp_constant8(c, 0xc0); break;
    case 0xe592: branch_to(c, !c->c, 0xe59e); break;
    case 0xe594: stz_abs8(c, 0x1546); break;
    case 0xe597: stz_abs8(c, 0x1558); break;
    case 0xe59a: stz_abs8(c, 0x155f); break;
    case 0xe59e: and_constant8(c, 0x04); break;
    case 0xe5a0: lsr_a8(c); break;
    case 0xe5a1: ora_constant8(c, 0x60); break;
    case 0xe5a3: store_register8(c, 0x155a); break;
    case 0xe5a6: stz_abs8(c, 0x155b); break;
  }
  return true;
}

/* ==== Batch: E117 $E2xx cluster + $E342/$E347 + $E446 (236 sites) ==== */

/* $05:E24F–$E25F: ($E117,122..) $E24F STZ $1546; $E253 copy $1541→$1511/$1521
 * then join $E25C STZ $1546; RTS. */
static bool actor_hdl_e24f_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xe24f: case 0xe252: case 0xe253: case 0xe256: case 0xe259: case 0xe25c:
    case 0xe25f:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe24f: case 0xe25c: stz_abs8(c, 0x1546); break;
    case 0xe252: case 0xe25f: return_short(c); break;
    case 0xe253: video_load(c, 0x1541, false); break;
    case 0xe256: store_register8(c, 0x1511); break;
    case 0xe259: store_register8(c, 0x1521); break;
  }
  return true;
}

/* $05:E260–$E2BB: ($E117,12A/132) #$80 seed via dual $1D8807 + coords; else
 * $1559 gate / $1558 wrap ADC #$81; double-INC $1546 (clear on wrap). */
static bool actor_hdl_e260_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xe260: case 0xe263: case 0xe265: case 0xe267: case 0xe26a: case 0xe26d:
    case 0xe270: case 0xe273: case 0xe276: case 0xe279: case 0xe27d: case 0xe280:
    case 0xe284: case 0xe287: case 0xe28a: case 0xe28d: case 0xe290: case 0xe292:
    case 0xe293: case 0xe295: case 0xe298: case 0xe29a: case 0xe29d: case 0xe2a0:
    case 0xe2a3: case 0xe2a5: case 0xe2a8: case 0xe2ab: case 0xe2ae: case 0xe2af:
    case 0xe2b2: case 0xe2b4: case 0xe2b7: case 0xe2b9: case 0xe2bb:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe260: video_load(c, 0x1546, false); break;
    case 0xe263: cmp_constant8(c, 0x80); break;
    case 0xe265: branch_to(c, !c->z, 0xe2af); break;
    case 0xe267: video_load(c, 0x155e, false); break;
    case 0xe26a: store_register8(c, 0x1566); break;
    case 0xe26d: store_register8(c, 0x156e); break;
    case 0xe270: stz_abs8(c, 0x1561); break;
    case 0xe273: stz_abs8(c, 0x1569); break;
    case 0xe276: index_constant(c, false, 0x0008); break;
    case 0xe279: case 0xe280: jsl_to(c, 0x8807); break;
    case 0xe27d: index_constant(c, false, 0x0010); break;
    case 0xe284: index_constant(c, true, 0x0060); break;
    case 0xe287: store_y_abs16(c, 0x155a); break;
    case 0xe28a: index_constant(c, true, 0x0080); break;
    case 0xe28d: store_y_abs16(c, 0x155c); break;
    case 0xe290: byte_constant(c, 0x1c); break;
    case 0xe292: implied(c); c->c = false; break;
    case 0xe293: adc8_constant(c, 0x81); break;
    case 0xe295: store_register8(c, 0x1558); break;
    case 0xe298: byte_constant(c, 0x08); break;
    case 0xe29a: store_register8(c, 0x1559); break;
    case 0xe29d: case 0xe2a0: inc_abs8_native(c, 0x1546); break;
    case 0xe2a3: branch_to(c, !c->z, 0xe2ae); break;
    case 0xe2a5: stz_abs8(c, 0x1558); break;
    case 0xe2a8: stz_abs8(c, 0x1560); break;
    case 0xe2ab: stz_abs8(c, 0x1568); break;
    case 0xe2ae: return_short(c); break;
    case 0xe2af: video_load(c, 0x1559, false); break;
    case 0xe2b2: branch_to(c, !c->z, 0xe29d); break;
    case 0xe2b4: video_load(c, 0x1558, false); break;
    case 0xe2b7: cmp_constant8(c, 0x1f); break;
    case 0xe2b9: branch_to(c, c->c, 0xe290); break;
    case 0xe2bb: branch_to(c, !c->c, 0xe292); break;
  }
  return true;
}

/* $05:E2BD–$E2F3: ($E117,120) INC phase; ≥#$BC clear; else #$91/$1558 + pair
 * from $05E2F4[X] via DP+$B7 gate (==#$28 → X=0 else 2). Data at $E2F4–$E2F7. */
static bool actor_hdl_e2bd_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xe2bd: case 0xe2c0: case 0xe2c3: case 0xe2c5: case 0xe2c7: case 0xe2ca:
    case 0xe2cd: case 0xe2ce: case 0xe2d0: case 0xe2d3: case 0xe2d6: case 0xe2d9:
    case 0xe2dc: case 0xe2de: case 0xe2e0: case 0xe2e2: case 0xe2e5: case 0xe2e9:
    case 0xe2ec: case 0xe2f0: case 0xe2f3:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe2bd: inc_abs8_native(c, 0x1546); break;
    case 0xe2c0: video_load(c, 0x1546, false); break;
    case 0xe2c3: cmp_constant8(c, 0xbc); break;
    case 0xe2c5: branch_to(c, !c->c, 0xe2ce); break;
    case 0xe2c7: stz_abs8(c, 0x1546); break;
    case 0xe2ca: stz_abs8(c, 0x1558); break;
    case 0xe2cd: case 0xe2f3: return_short(c); break;
    case 0xe2ce: byte_constant(c, 0x91); break;
    case 0xe2d0: store_register8(c, 0x1558); break;
    case 0xe2d3: stz_abs8(c, 0x155b); break;
    case 0xe2d6: stz_abs8(c, 0x155d); break;
    case 0xe2d9: index_constant(c, false, 0x0000); break;
    case 0xe2dc: load_dp8(c, 0xb7); break;
    case 0xe2de: cmp_constant8(c, 0x28); break;
    case 0xe2e0: branch_to(c, c->z, 0xe2e5); break;
    case 0xe2e2: index_constant(c, false, 0x0002); break;
    case 0xe2e5: load_a_long_x8(c, 0x05e2f4); break;
    case 0xe2e9: store_register8(c, 0x155a); break;
    case 0xe2ec: load_a_long_x8(c, 0x05e2f5); break;
    case 0xe2f0: store_register8(c, 0x155c); break;
  }
  return true;
}

/* $05:E2F8–$E31E: ($E117,11E) INC phase; ≥#$BC clear $1560; else
 * (A>>2)&1|#$82 → $1560 + Y #$0068/#$0090 → $1562/$1564. */
static bool actor_hdl_e2f8_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xe2f8: case 0xe2fb: case 0xe2fe: case 0xe300: case 0xe302: case 0xe305:
    case 0xe308: case 0xe309: case 0xe30a: case 0xe30b: case 0xe30d: case 0xe30f:
    case 0xe312: case 0xe315: case 0xe318: case 0xe31b: case 0xe31e:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe2f8: inc_abs8_native(c, 0x1546); break;
    case 0xe2fb: video_load(c, 0x1546, false); break;
    case 0xe2fe: cmp_constant8(c, 0xbc); break;
    case 0xe300: branch_to(c, !c->c, 0xe309); break;
    case 0xe302: stz_abs8(c, 0x1546); break;
    case 0xe305: stz_abs8(c, 0x1560); break;
    case 0xe308: case 0xe31e: return_short(c); break;
    case 0xe309: case 0xe30a: lsr_a8(c); break;
    case 0xe30b: and_constant8(c, 0x01); break;
    case 0xe30d: ora_constant8(c, 0x82); break;
    case 0xe30f: store_register8(c, 0x1560); break;
    case 0xe312: index_constant(c, true, 0x0068); break;
    case 0xe315: store_y_abs16(c, 0x1562); break;
    case 0xe318: index_constant(c, true, 0x0090); break;
    case 0xe31b: store_y_abs16(c, 0x1564); break;
  }
  return true;
}

/* $05:E31F–$E341: ($E117,11C) INC phase; ≥#$BC clear; else #$A7/$1560 +
 * Y #$0024/#$0088 → $1562/$1564. */
static bool actor_hdl_e31f_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xe31f: case 0xe322: case 0xe325: case 0xe327: case 0xe329: case 0xe32c:
    case 0xe32f: case 0xe330: case 0xe332: case 0xe335: case 0xe338: case 0xe33b:
    case 0xe33e: case 0xe341:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe31f: inc_abs8_native(c, 0x1546); break;
    case 0xe322: video_load(c, 0x1546, false); break;
    case 0xe325: cmp_constant8(c, 0xbc); break;
    case 0xe327: branch_to(c, !c->c, 0xe330); break;
    case 0xe329: stz_abs8(c, 0x1546); break;
    case 0xe32c: stz_abs8(c, 0x1560); break;
    case 0xe32f: case 0xe341: return_short(c); break;
    case 0xe330: byte_constant(c, 0xa7); break;
    case 0xe332: store_register8(c, 0x1560); break;
    case 0xe335: index_constant(c, true, 0x0024); break;
    case 0xe338: store_y_abs16(c, 0x1562); break;
    case 0xe33b: index_constant(c, true, 0x0088); break;
    case 0xe33e: store_y_abs16(c, 0x1564); break;
  }
  return true;
}

/* $05:E342–$E38D: ($E117,102/104) Y=#$8004/#$8406 → DP+$00; #$80 seed via
 * DP+$01/$F000; else $1561 gate / compare DP+$00 / rewrite $1560|#$80. */
static bool actor_hdl_e342_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0xe342: case 0xe345: case 0xe347: case 0xe34a: case 0xe34c: case 0xe34f:
    case 0xe351: case 0xe353: case 0xe355: case 0xe358: case 0xe35b: case 0xe35e:
    case 0xe361: case 0xe364: case 0xe367: case 0xe369: case 0xe36c: case 0xe36e:
    case 0xe371: case 0xe373: case 0xe375: case 0xe377: case 0xe379: case 0xe37b:
    case 0xe37e: case 0xe381: case 0xe382: case 0xe383: case 0xe385: case 0xe388:
    case 0xe38a: case 0xe38d:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe342: index_constant(c, true, 0x8004); break;
    case 0xe345: branch_to(c, true, 0xe34a); break;
    case 0xe347: index_constant(c, true, 0x8406); break;
    case 0xe34a: sty_dp16(c, 0x00); break;
    case 0xe34c: video_load(c, 0x1546, false); break;
    case 0xe34f: cmp_constant8(c, 0x80); break;
    case 0xe351: branch_to(c, !c->z, 0xe369); break;
    case 0xe353: load_dp8(c, 0x01); break;
    case 0xe355: store_register8(c, 0x1560); break;
    case 0xe358: index_constant(c, true, 0x0058); break;
    case 0xe35b: store_y_abs16(c, 0x1562); break;
    case 0xe35e: index_constant(c, true, 0x0080); break;
    case 0xe361: store_y_abs16(c, 0x1564); break;
    case 0xe364: jsr_abs_native(c, 0xf000); break;
    case 0xe367: branch_to(c, true, 0xe388); break;
    case 0xe369: video_load(c, 0x1561, false); break;
    case 0xe36c: branch_to(c, !c->z, 0xe38d); break;
    case 0xe36e: video_load(c, 0x1560, false); break;
    case 0xe371: cmp_dp8(c, 0x00); break;
    case 0xe373: branch_to(c, !c->c, 0xe382); break;
    case 0xe375: cmp_constant8(c, 0x04); break;
    case 0xe377: branch_to(c, !c->z, 0xe37e); break;
    case 0xe379: byte_constant(c, 0x84); break;
    case 0xe37b: store_register8(c, 0x1560); break;
    case 0xe37e: stz_abs8(c, 0x1546); break;
    case 0xe381: case 0xe38d: return_short(c); break;
    case 0xe382: inc_a8(c); break;
    case 0xe383: ora_constant8(c, 0x80); break;
    case 0xe385: store_register8(c, 0x1560); break;
    case 0xe388: byte_constant(c, 0x08); break;
    case 0xe38a: store_register8(c, 0x1561); break;
  }
  return true;
}

/* $05:E446–$E564: ($E117,FC) multi-phase scroll seed — #$80 dual-bank seed;
 * #$81 m16 $155C+=4/$1564-=4; #$82 wait $1569; #$83 m16 pinch + bump;
 * else m16 expand then clear ≥#$06. */
static bool actor_hdl_e446_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7fu) != 0x05 || c->e || c->xf || c->d) return false;
  const bool m16 = (pc == 0xe486 || pc == 0xe489 || pc == 0xe48a || pc == 0xe48d
                      || pc == 0xe490 || pc == 0xe493 || pc == 0xe494 || pc == 0xe497
                      || pc == 0xe4e4 || pc == 0xe4e7 || pc == 0xe4ea || pc == 0xe4ed
                      || pc == 0xe4f0 || pc == 0xe4f3 || pc == 0xe4f6 || pc == 0xe4f9
                      || pc == 0xe532 || pc == 0xe535 || pc == 0xe538 || pc == 0xe53b
                      || pc == 0xe53e || pc == 0xe541 || pc == 0xe544 || pc == 0xe547);
  const bool sep_entry = (pc == 0xe49a || pc == 0xe4fc || pc == 0xe54a);
  const bool rep_entry = (pc == 0xe484 || pc == 0xe4e2 || pc == 0xe530);
  if(m16 && c->mf) return false;
  if(sep_entry && c->mf) return false;
  if(rep_entry && !c->mf) return false;
  if(!m16 && !sep_entry && !rep_entry && !c->mf) return false;
  switch(pc) {
    case 0xe446: case 0xe449: case 0xe44b: case 0xe44d: case 0xe44f: case 0xe452:
    case 0xe455: case 0xe458: case 0xe45b: case 0xe45e: case 0xe461: case 0xe464:
    case 0xe467: case 0xe46a: case 0xe46d: case 0xe470: case 0xe473: case 0xe476:
    case 0xe479: case 0xe47c: case 0xe47f: case 0xe480: case 0xe482: case 0xe484:
    case 0xe486: case 0xe489: case 0xe48a: case 0xe48d: case 0xe490: case 0xe493:
    case 0xe494: case 0xe497: case 0xe49a: case 0xe49c: case 0xe49f: case 0xe4a1:
    case 0xe4a3: case 0xe4a6: case 0xe4a9: case 0xe4ac: case 0xe4af: case 0xe4b2:
    case 0xe4b5: case 0xe4b8: case 0xe4bb: case 0xe4be: case 0xe4bf: case 0xe4c1:
    case 0xe4c3: case 0xe4c6: case 0xe4c8: case 0xe4cb: case 0xe4ce: case 0xe4d1:
    case 0xe4d4: case 0xe4d7: case 0xe4da: case 0xe4dd: case 0xe4de: case 0xe4e0:
    case 0xe4e2: case 0xe4e4: case 0xe4e7: case 0xe4ea: case 0xe4ed: case 0xe4f0:
    case 0xe4f3: case 0xe4f6: case 0xe4f9: case 0xe4fc: case 0xe4fe: case 0xe501:
    case 0xe503: case 0xe506: case 0xe508: case 0xe50a: case 0xe50b: case 0xe50d:
    case 0xe510: case 0xe513: case 0xe515: case 0xe518: case 0xe51b: case 0xe51c:
    case 0xe51f: case 0xe522: case 0xe525: case 0xe528: case 0xe52b: case 0xe52e:
    case 0xe530: case 0xe532: case 0xe535: case 0xe538: case 0xe53b: case 0xe53e:
    case 0xe541: case 0xe544: case 0xe547: case 0xe54a: case 0xe54c: case 0xe54f:
    case 0xe551: case 0xe554: case 0xe556: case 0xe558: case 0xe55b: case 0xe55e:
    case 0xe561: case 0xe564:
      break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xe446: video_load(c, 0x1546, false); break;
    case 0xe449: cmp_constant8(c, 0x80); break;
    case 0xe44b: branch_to(c, !c->z, 0xe480); break;
    case 0xe44d: byte_constant(c, 0x83); break;
    case 0xe44f: store_register8(c, 0x1558); break;
    case 0xe452: store_register8(c, 0x1560); break;
    case 0xe455: index_constant(c, true, 0x0060); break;
    case 0xe458: store_y_abs16(c, 0x155a); break;
    case 0xe45b: store_y_abs16(c, 0x1562); break;
    case 0xe45e: index_constant(c, true, 0xffd0); break;
    case 0xe461: store_y_abs16(c, 0x155c); break;
    case 0xe464: index_constant(c, true, 0x0130); break;
    case 0xe467: store_y_abs16(c, 0x1564); break;
    case 0xe46a: video_load(c, 0x1559, false); break;
    case 0xe46d: store_register8(c, 0x1561); break;
    case 0xe470: store_register8(c, 0x1569); break;
    case 0xe473: load_y_abs16(c, 0x155e); break;
    case 0xe476: store_y_abs16(c, 0x1566); break;
    case 0xe479: store_y_abs16(c, 0x156e); break;
    case 0xe47c: case 0xe4bb: case 0xe4da: inc_abs8_native(c, 0x1546); break;
    case 0xe47f: case 0xe4be: case 0xe4dd: case 0xe51b: case 0xe564: return_short(c); break;
    case 0xe480: cmp_constant8(c, 0x81); break;
    case 0xe482: branch_to(c, !c->z, 0xe4bf); break;
    case 0xe484: case 0xe4e2: case 0xe530: sep_rep_native(c, 0x20, false); break;
    case 0xe486: video_load(c, 0x155c, false); break;
    case 0xe489: implied(c); c->c = false; break;
    case 0xe48a: adc16_constant(c, 0x0004); break;
    case 0xe48d: store_a_abs16(c, 0x155c); break;
    case 0xe490: video_load(c, 0x1564, false); break;
    case 0xe493: implied(c); c->c = true; break;
    case 0xe494: sbc16_constant(c, 0x0004); break;
    case 0xe497: store_a_abs16(c, 0x1564); break;
    case 0xe49a: case 0xe4fc: case 0xe54a: sep_rep_native(c, 0x20, true); break;
    case 0xe49c: video_load(c, 0x155c, false); break;
    case 0xe49f: cmp_constant8(c, 0x80); break;
    case 0xe4a1: branch_to(c, !c->z, 0xe4be); break;
    case 0xe4a3: stz_abs8(c, 0x1558); break;
    case 0xe4a6: stz_abs8(c, 0x1560); break;
    case 0xe4a9: index_constant(c, true, 0x1084); break;
    case 0xe4ac: store_y_abs16(c, 0x1568); break;
    case 0xe4af: index_constant(c, true, 0x0060); break;
    case 0xe4b2: store_y_abs16(c, 0x156a); break;
    case 0xe4b5: index_constant(c, true, 0x0080); break;
    case 0xe4b8: store_y_abs16(c, 0x156c); break;
    case 0xe4bf: cmp_constant8(c, 0x82); break;
    case 0xe4c1: branch_to(c, !c->z, 0xe4de); break;
    case 0xe4c3: video_load(c, 0x1569, false); break;
    case 0xe4c6: branch_to(c, !c->z, 0xe4be); break;
    case 0xe4c8: index_constant(c, true, 0x0084); break;
    case 0xe4cb: store_y_abs16(c, 0x1568); break;
    case 0xe4ce: index_constant(c, true, 0x0880); break;
    case 0xe4d1: store_y_abs16(c, 0x1558); break;
    case 0xe4d4: index_constant(c, true, 0x0880); break;
    case 0xe4d7: store_y_abs16(c, 0x1560); break;
    case 0xe4de: cmp_constant8(c, 0x83); break;
    case 0xe4e0: branch_to(c, !c->z, 0xe530); break;
    case 0xe4e4: case 0xe4e7: dec_abs16(c, 0x155c); break;
    case 0xe4ea: case 0xe4ed: inc_abs16(c, 0x155a); break;
    case 0xe4f0: case 0xe4f3: inc_abs16(c, 0x1564); break;
    case 0xe4f6: case 0xe4f9: dec_abs16(c, 0x1562); break;
    case 0xe4fe: video_load(c, 0x1559, false); break;
    case 0xe501: branch_to(c, !c->z, 0xe4be); break;
    case 0xe503: video_load(c, 0x1558, false); break;
    case 0xe506: cmp_constant8(c, 0x06); break;
    case 0xe508: branch_to(c, c->c, 0xe51c); break;
    case 0xe50a: inc_a8(c); break;
    case 0xe50b: ora_constant8(c, 0x80); break;
    case 0xe50d: store_register8(c, 0x1558); break;
    case 0xe510: store_register8(c, 0x1560); break;
    case 0xe513: byte_constant(c, 0x05); break;
    case 0xe515: store_register8(c, 0x1559); break;
    case 0xe518: store_register8(c, 0x1561); break;
    case 0xe51c: index_constant(c, true, 0x0060); break;
    case 0xe51f: store_y_abs16(c, 0x155a); break;
    case 0xe522: store_y_abs16(c, 0x1562); break;
    case 0xe525: index_constant(c, true, 0x0080); break;
    case 0xe528: store_y_abs16(c, 0x155c); break;
    case 0xe52b: store_y_abs16(c, 0x1564); break;
    case 0xe52e: branch_to(c, true, 0xe4ce); break;
    case 0xe532: case 0xe535: inc_abs16(c, 0x155c); break;
    case 0xe538: case 0xe53b: inc_abs16(c, 0x155a); break;
    case 0xe53e: case 0xe541: dec_abs16(c, 0x1564); break;
    case 0xe544: case 0xe547: dec_abs16(c, 0x1562); break;
    case 0xe54c: video_load(c, 0x1559, false); break;
    case 0xe54f: branch_to(c, !c->z, 0xe564); break;
    case 0xe551: video_load(c, 0x1558, false); break;
    case 0xe554: cmp_constant8(c, 0x06); break;
    case 0xe556: branch_to(c, !c->c, 0xe50a); break;
    case 0xe558: stz_abs8(c, 0x1546); break;
    case 0xe55b: stz_abs8(c, 0x1558); break;
    case 0xe55e: stz_abs8(c, 0x1560); break;
    case 0xe561: stz_abs8(c, 0x1568); break;
  }
  return true;
}

/* $01:C91F-C959: resolve the actor's graphics pointer and prepare the
 * $00:85F5 upload call. C943 is the shared short-call pointer selector. */
static bool actor_graphics_c91f_step(Cpu *c, uint16_t pc) {
  if((c->k & 0x7f) != 1 || c->e || c->xf) return false;
  bool wide = pc==0xc924 || pc==0xc926 || pc==0xc928 ||
    pc==0xc949 || pc==0xc94c || pc==0xc94e || pc==0xc94f ||
    pc==0xc950 || pc==0xc951 || pc==0xc952;
  if(c->mf == wide) return false;
  switch(pc) {
    case 0xc91f: case 0xc922: case 0xc924: case 0xc926: case 0xc928:
    case 0xc92a: case 0xc92d: case 0xc92e: case 0xc930: case 0xc933:
    case 0xc935: case 0xc938: case 0xc93a: case 0xc93c: case 0xc93e:
    case 0xc942: case 0xc943: case 0xc945: case 0xc947: case 0xc949:
    case 0xc94c: case 0xc94e: case 0xc94f: case 0xc950: case 0xc951:
    case 0xc952: case 0xc954: case 0xc957: case 0xc958: case 0xc959: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
    case 0xc91f: jsr_abs_native(c,0xc943); break;
    case 0xc922: case 0xc947: set_m(c,false); break;
    case 0xc924: lda_indirect_long_y16(c,0); break;
    case 0xc926: case 0xc94c: store_dp(c,0,false); break;
    case 0xc928: case 0xc952: set_m(c,true); break;
    case 0xc92a: load_a_abs_x8(c,0x0d02); break;
    case 0xc92d: dec_a8(c); break;
    case 0xc92e: store_dp(c,0x0a,true); break;
    case 0xc930: load_a_abs_x8(c,0x0d03); break;
    case 0xc933: store_dp(c,0x0c,true); break;
    case 0xc935: load_a_abs_x8(c,0x0d00); break;
    case 0xc938: and_constant8(c,1); break;
    case 0xc93a: store_dp(c,0x10,true); break;
    case 0xc93c: stz_dp8(c,0x12); break;
    case 0xc93e: jsl_to(c,0x85f5); break;
    case 0xc942: return_long(c); break;
    case 0xc943: byte_constant(c,2); break;
    case 0xc945: store_dp(c,2,true); break;
    case 0xc949: lda16_constant(c,0xa264); break;
    case 0xc94e: txa16(c); break;
    case 0xc94f: case 0xc950: lsr_a16(c); break;
    case 0xc951: case 0xc958: tay16(c); break;
    case 0xc954: load_a_abs_y8(c,0x0d40); break;
    case 0xc957: asl_a8(c); break;
    case 0xc959: return_short(c); break;
  }
  return true;
}

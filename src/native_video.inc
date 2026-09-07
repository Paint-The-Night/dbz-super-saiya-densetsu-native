/* Included after native.c's bus helpers. Fixed-address reconstruction of the
 * frame upload routine, not an instruction decoder. Original region 8324-83CB.
 * OAM shadow: 0400..061F. Palette shadow: 0200..03FF, dirty flag 0715.
 * VRAM queue: 8-byte entries at 0800; bit 7 of byte 0 means occupied.
 * Entry layout: VMAIN, VMADD word, source word, source bank, length word.
 */
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

/* $008DFE: scene setup — load $01A0 into DP+$03, chain palette/attr helpers,
 * store $20 to $0700, then JSL $06F14D. */
static bool scene_setup_8dfe_step(Cpu *c, uint16_t pc) {
  if(c->e || c->xf || c->d || !c->mf) return false;
  switch(pc) {
    case 0x8dfe: case 0x8e01: case 0x8e03: case 0x8e07: case 0x8e0a: case 0x8e0c:
    case 0x8e10: case 0x8e14: case 0x8e18: case 0x8e1c: case 0x8e1e: case 0x8e21:
    case 0x8e25: break;
    default: return false;
  }
  (void)fetch(c);
  switch(pc) {
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

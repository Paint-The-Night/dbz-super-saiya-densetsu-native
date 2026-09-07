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

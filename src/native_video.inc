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
  bool enable = pc >= 0x8460;
  if((enable && pc > 0x8470) || (!enable && pc > 0x828e)) return false;
  static const uint16_t off_enable[] = {0x8460,0x8463,0x8465,0x8468,0x846b,0x846d,0x8470};
  static const uint16_t off_disable[] = {0x8282,0x8284,0x8285,0x8287,0x8289,0x828b,0x828e};
  const uint16_t *list = enable ? off_enable : off_disable;
  unsigned count = 7;
  bool valid = false; for(unsigned i=0;i<count;i++) valid |= pc == list[i];
  if(!valid) return false;
  (void)fetch(c);
  switch(pc) {
    case 0x8282: byte_constant(c, 0); break;
    case 0x8460: video_load(c, 0x0716, false); break;
    case 0x8284: xba_native(c); break;
    case 0x8285: load_dp8(c, 0x28); break;
    case 0x8287: and_constant8(c, 0x0e); break;
    case 0x8289: store_dp(c, 0x28, true); break;
    case 0x828b: store_register8(c, 0x4200); break;
    case 0x8463: and_constant8(c, 0x1f); break;
    case 0x8465: store_register8(c, 0x0719); break;
    case 0x8468: video_load(c, 0x0718, false); break;
    case 0x846b: store_dp(c, 0x29, true); break;
    case 0x846d: store_register8(c, 0x2100); break;
    case 0x828e: case 0x8470: return_long(c); break;
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

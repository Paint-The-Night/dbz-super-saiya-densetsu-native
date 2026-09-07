/* Execute actual ROM instructions with the independent interpreter, then
 * compare native replacements: all registers/flags, ordered bus transactions,
 * and final seed. Exhaustively covers both RNG streams' 16-bit seed space. */
#include "native.h"
#include "rom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uint32_t address; uint8_t value,kind; } Event;
typedef struct {
  uint8_t ram[65536]; const uint8_t *rom;
  Event events[128]; unsigned count, busy_reads;
} Bus;
static void require(bool value,const char *why) { if(!value) { fprintf(stderr,"FAIL: %s\n",why);exit(1); } }
static void record(Bus *b,uint32_t a,uint8_t v,uint8_t kind) {
  require(b->count<128,"bus log overflow");
  Event *e=&b->events[b->count++];e->address=a;e->value=v;e->kind=kind;
}
static uint8_t read_bus(void *mem,uint32_t address) {
  Bus *b=mem;uint8_t v;
  if((address & 0xffffu)>=0x8000 && !(address>>16 & 0x7fu)) v=b->rom[address&0x7fff];
  else v=b->ram[address&65535];
  if((address&65535)==0x4212 && b->busy_reads) { v|=1; b->busy_reads--; }
  record(b,address,v,'r');return v;
}
static void write_bus(void *mem,uint32_t address,uint8_t value) {
  Bus *b=mem;b->ram[address&65535]=value;record(b,address,value,'w');
}
static void idle_bus(void *mem,bool waiting) { record(mem,0,waiting,'i'); }
static Cpu make_cpu(Bus *bus,uint16_t pc,unsigned variant) {
  Cpu c={0};c.mem=bus;c.read=read_bus;c.write=write_bus;c.idle=idle_bus;
  c.pc=pc;c.sp=0x1ff;c.a=0xa53c;c.x=0x12;c.y=0x34;c.mf=true;
  c.xf=variant&1;c.c=variant&2;c.n=variant&4;c.v=variant&8;c.z=variant&16;c.i=true;
  return c;
}
static void compare(Cpu *a,Cpu *b,Bus *ba,Bus *bb) {
  // Compare architectural fields, not compiler-dependent structure padding.
#define SAME(field) require(a->field==b->field,"CPU field differs: " #field)
  SAME(a);SAME(x);SAME(y);SAME(sp);SAME(pc);SAME(dp);SAME(k);SAME(db);
  SAME(c);SAME(z);SAME(v);SAME(n);SAME(i);SAME(d);SAME(xf);SAME(mf);SAME(e);
  SAME(waiting);SAME(stopped);SAME(irqWanted);SAME(nmiWanted);SAME(intWanted);SAME(resetWanted);
#undef SAME
  require(ba->count==bb->count,"bus cycle counts differ");
  for(unsigned i=0;i<ba->count;i++)
    require(ba->events[i].address==bb->events[i].address && ba->events[i].value==bb->events[i].value && ba->events[i].kind==bb->events[i].kind,"bus read/write/idle sequence differs");
}
static void word(Bus *b,unsigned addr,unsigned value) {
  b->ram[addr]=(uint8_t)value;b->ram[addr+1]=(uint8_t)(value>>8);
}
static unsigned getword(Bus *b,unsigned addr) {
  return b->ram[addr]|((unsigned)b->ram[addr+1]<<8);
}
int main(int argc,char **argv) {
  require(argc==2,"ROM argument required");
  char digest[65];dbz_sha256((const uint8_t*)"abc",3,digest);
  require(!strcmp(digest,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),"SHA-256 abc vector");
  dbz_sha256((const uint8_t*)"",0,digest);
  require(!strcmp(digest,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"),"SHA-256 empty vector");
  size_t size;uint8_t *rom=dbz_read_rom(argv[1],&size);require(rom!=NULL,"pinned ROM identity");
  Bus *a=calloc(1,sizeof(Bus)),*b=calloc(1,sizeof(Bus));require(a&&b,"allocation");a->rom=b->rom=rom;
  DbzExecution *stats=calloc(1,sizeof(DbzExecution));require(stats!=NULL,"stats allocation");
  for(unsigned stream=0;stream<2;stream++) for(unsigned seed=0;seed<65536;seed++) {
    unsigned addr=stream?0x31:0x2f;
    a->ram[addr]=b->ram[addr]=(uint8_t)seed;a->ram[addr+1]=b->ram[addr+1]=(uint8_t)(seed>>8);
    a->ram[0x200]=b->ram[0x200]=0xff;a->ram[0x201]=b->ram[0x201]=0x8f;a->ram[0x202]=b->ram[0x202]=0;
    a->count=b->count=0;
    Cpu ca=make_cpu(a,stream?0x82b5:0x82a3,seed),cb=make_cpu(b,stream?0x82b5:0x82a3,seed);
    for(unsigned step=0;step<11;step++) {
      require(dbz_native_step(&ca,stats),"expected native RNG instruction");
      lakesnes_cpu_runOpcode(&cb);compare(&ca,&cb,a,b);
    }
    unsigned expected=(seed*5+17)&65535;
    require(((unsigned)a->ram[addr]|((unsigned)a->ram[addr+1]<<8))==expected,"recovered RNG formula");
    require(ca.pc==0x9000 && ca.sp==0x202,"RNG return frame");
  }
  // Exhaustive cached brightness/control byte, both display operations, and
  // aligned/unaligned direct pages exercise the extra bus idle cycle.
  for(unsigned blank=0;blank<2;blank++) for(unsigned dp=0;dp<2;dp++) for(unsigned value=0;value<256;value++) {
    a->count=b->count=0;
    Cpu ca=make_cpu(a,blank?0x8299:0x828f,value),cb=make_cpu(b,blank?0x8299:0x828f,value);
    ca.dp=cb.dp=dp?0x101:0;unsigned addr=ca.dp+0x29;
    a->ram[addr]=b->ram[addr]=(uint8_t)value;
    for(unsigned step=0;step<5;step++) {
      require(dbz_native_step(&ca,stats),"expected display instruction");lakesnes_cpu_runOpcode(&cb);compare(&ca,&cb,a,b);
    }
  }
  for(unsigned variant=0;variant<32;variant++) {
    a->count=b->count=0;Cpu ca=make_cpu(a,0x8000,variant),cb=make_cpu(b,0x8000,variant);
    ca.e=cb.e=true;ca.xf=cb.xf=true;
    for(unsigned step=0;step<9;step++) {
      require(dbz_native_step(&ca,stats),"expected reset-prefix instruction");lakesnes_cpu_runOpcode(&cb);compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x800e && !ca.e && ca.i && !ca.d,"reset prefix exit");
  }
  // Every held mask, with zero/same/complement/mixed previous state. Two ports
  // use different masks; variants cover unaligned DP and the busy-wait branch.
  for(unsigned held=0;held<65536;held++) for(unsigned variant=0;variant<4;variant++) {
    unsigned prior=variant==0?0:variant==1?held:variant==2?(held^65535):((held*257+12345)&65535);
    unsigned held2=held^0xa55a,prior2=prior^0x5aa5;
    Cpu ca=make_cpu(a,0x82d4,variant*2),cb=make_cpu(b,0x82d4,variant*2);
    ca.dp=cb.dp=variant&1?0x101:0;
    for(unsigned side=0;side<2;side++) {
      Bus *bus=side?b:a;bus->count=0;bus->busy_reads=variant==3;
      bus->ram[0x4212]=0;word(bus,0x4218,held);word(bus,0x421a,held2);
      word(bus,ca.dp+0x4b,prior);word(bus,ca.dp+0x4d,prior2);word(bus,0x200,0x8fff);
    }
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<30) {
      require(dbz_native_step(&ca,stats),"expected controller instruction");
      lakesnes_cpu_runOpcode(&cb);compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x201,"controller return");
    require(getword(a,ca.dp+0x43)==held && getword(a,ca.dp+0x45)==held2,"held buttons");
    require(getword(a,ca.dp+0x47)==(held&~prior) && getword(a,ca.dp+0x49)==(held2&~prior2),"new buttons");
    require(getword(a,ca.dp+0x4b)==held && getword(a,ca.dp+0x4d)==held2,"previous buttons");
  }
  for(unsigned value=0;value<256;value++) for(unsigned variant=0;variant<2;variant++) {
    Cpu ca=make_cpu(a,0x82fb,variant),cb=make_cpu(b,0x82fb,variant);
    ca.dp=cb.dp=variant?0x101:0;a->count=b->count=0;
    word(a,0x200,0x8fff);word(b,0x200,0x8fff);
    for(unsigned i=0;i<8;i++) a->ram[ca.dp+0x33+i]=b->ram[ca.dp+0x33+i]=(uint8_t)(value+i*37);
    for(unsigned step=0;step<17;step++) {
      require(dbz_native_step(&ca,stats),"expected scroll instruction");
      lakesnes_cpu_runOpcode(&cb);compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x201,"scroll return");
  }
  // Video uploads: queue lengths 0..7, both HDMA branches, clean/dirty palette,
  // patterned entry payloads, both ROM mirrors, and unaligned direct pages.
  for(unsigned variant=0;variant<1024;variant++) {
    memset(a->ram,0,sizeof(a->ram));memset(b->ram,0,sizeof(b->ram));
    unsigned entries=variant&7;
    Cpu ca=make_cpu(a,0x8324,variant&30),cb=make_cpu(b,0x8324,variant&30);
    ca.dp=cb.dp=variant&8?0x101:0;ca.k=cb.k=variant&16?0x80:0;
    ca.db=cb.db=variant&32?0x80:0;
    for(unsigned side=0;side<2;side++) {
      Bus *bus=side?b:a;
      word(bus,0x200,0x8fff);bus->ram[0x202]=0;
      bus->ram[ca.dp+0xba]=variant&64?0x80:0;
      bus->ram[ca.dp+0x62]=(uint8_t)variant;
      word(bus,ca.dp+0xa8,variant*37);
      bus->ram[0x715]=variant&128?(uint8_t)((variant&127)+1):0;
      for(unsigned entry=0;entry<entries;entry++) {
        for(unsigned byte=0;byte<8;byte++) bus->ram[0x800+entry*8+byte]=(uint8_t)(variant+entry*37+byte*71);
        bus->ram[0x800+entry*8]|=0x80;
      }
    }
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<1000) {
      a->count=b->count=0;
      require(dbz_native_step(&ca,stats),"expected frame-upload instruction");
      lakesnes_cpu_runOpcode(&cb);compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"upload return");
    require(!memcmp(a->ram,b->ram,sizeof(a->ram)),"upload memory differs");
    require(a->ram[0x715]==0 && getword(a,ca.dp+0xa8)==0,"upload flags cleared");
    for(unsigned entry=0;entry<entries;entry++) require(a->ram[0x800+entry*8]==0,"queue entry consumed");
    require(a->ram[0x420c]==(uint8_t)variant,"HDMA mask restored");
  }
  for(unsigned variant=0;variant<=512;variant++) for(unsigned palette=0;palette<2;palette++) {
    memset(a->ram,0x5a,sizeof(a->ram));memset(b->ram,0x5a,sizeof(b->ram));
    Cpu ca=make_cpu(a,palette?0x86b6:0x86a0,variant&30),cb=make_cpu(b,palette?0x86b6:0x86a0,variant&30);
    ca.dp=cb.dp=variant&1?0x101:0;ca.sp=cb.sp=0x1df;
    ca.k=cb.k=variant&2?0x80:0;ca.db=cb.db=variant&4?0x80:0;
    word(a,0x1e0,0x8fff);word(b,0x1e0,0x8fff);a->ram[0x1e2]=b->ram[0x1e2]=0;
    word(a,ca.dp+0x55,variant);word(b,ca.dp+0x55,variant);
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<2000) {
      a->count=b->count=0;
      require(dbz_native_step(&ca,stats),"expected sprite/palette instruction");
      lakesnes_cpu_runOpcode(&cb);compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x1e2,"sprite/palette return");
    require(!memcmp(a->ram,b->ram,sizeof(a->ram)),"sprite/palette memory differs");
    if(palette) for(unsigned i=0x200;i<0x400;i++) require(a->ram[i]==0,"palette shadow cleared");
    else if(!(variant&3)) for(unsigned i=variant;i<512;i+=4) require(a->ram[0x401+i]==0xf0,"unused sprite hidden");
  }
  for(unsigned routine=0;routine<2;routine++) for(unsigned value=0;value<256;value++) {
    unsigned start=routine?0x8460:0x8282;
    memset(a->ram,0,sizeof(a->ram));memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,start,value),cb=make_cpu(b,start,value);
    ca.db=cb.db=0;word(a,0x200,0x8fff);word(b,0x200,0x8fff);
    a->ram[0x28]=b->ram[0x28]=(uint8_t)(value^0xa5);
    a->ram[0x716]=b->ram[0x716]=(uint8_t)(value^0x5a);
    a->ram[0x718]=b->ram[0x718]=(uint8_t)(value^0x3c);
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<10) {
      a->count=b->count=0;require(dbz_native_step(&ca,stats),"expected display-control instruction");
      lakesnes_cpu_runOpcode(&cb);compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"display-control return");
  }
  for(unsigned value=0;value<256;value++) for(unsigned mode=0;mode<2;mode++) {
    memset(a->ram,0,sizeof(a->ram));memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x83e4,value),cb=make_cpu(b,0x83e4,value);
    ca.db=cb.db=0;word(a,0x200,0x8fff);word(b,0x200,0x8fff);
    a->ram[0x716]=b->ram[0x716]=(uint8_t)(mode?0x80|value:value);
    a->ram[0x718]=b->ram[0x718]=(uint8_t)(value^0x4d);
    a->ram[0x29]=b->ram[0x29]=(uint8_t)(value^0xa7);
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<20) {
      a->count=b->count=0;require(dbz_native_step(&ca,stats),"expected transition-init instruction");
      lakesnes_cpu_runOpcode(&cb);compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"transition-init return");
  }
  for(unsigned value=0;value<256;value++) for(unsigned flags=0;flags<2;flags++) {
    memset(a->ram,0,sizeof(a->ram));memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8433,value),cb=make_cpu(b,0x8433,value);
    ca.db=cb.db=0;word(a,0x200,0x8fff);word(b,0x200,0x8fff);
    a->ram[0x719]=b->ram[0x719]=(uint8_t)value;
    a->ram[0x716]=b->ram[0x716]=(uint8_t)(flags?0x80|value:value);
    a->ram[0x717]=b->ram[0x717]=(uint8_t)(flags?0x80:value);
    a->ram[0x727]=b->ram[0x727]=(uint8_t)(value^0x36);
    a->ram[0x724]=b->ram[0x724]=(uint8_t)(value^0x81);
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<30) {
      a->count=b->count=0;require(dbz_native_step(&ca,stats),"expected transition-finish instruction");
      lakesnes_cpu_runOpcode(&cb);compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"transition-finish return");
  }
  for(unsigned state=0;state<4;state++) for(unsigned value=0;value<8;value++) {
    memset(a->ram,0,sizeof(a->ram));memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x83cc,value),cb=make_cpu(b,0x83cc,value);
    ca.db=cb.db=0;word(a,0x200,0x8fff);word(b,0x200,0x8fff);
    a->ram[0x716]=b->ram[0x716]=(uint8_t)(state?0x01:0);
    a->ram[0x71a]=b->ram[0x71a]=(uint8_t)state;
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<40) {
      a->count=b->count=0;require(dbz_native_step(&ca,stats),"expected transition-dispatch instruction");
      lakesnes_cpu_runOpcode(&cb);compare(&ca,&cb,a,b);
      if(ca.pc==0x8402 || ca.pc==0x8417) break;
    }
    require(ca.pc==0x9000 || ca.pc==0x8402 || ca.pc==0x8417,"transition-dispatch target");
  }
  for(unsigned value=0;value<256;value++) for(unsigned flags=0;flags<2;flags++) {
    memset(a->ram,0,sizeof(a->ram));memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8471,value),cb=make_cpu(b,0x8471,value);
    ca.db=cb.db=0;word(a,0x200,0x8fff);word(b,0x200,0x8fff);
    a->ram[0x716]=b->ram[0x716]=(uint8_t)(flags?0x20:0);
    a->ram[0x718]=b->ram[0x718]=(uint8_t)value;
    a->ram[0xd8c]=b->ram[0xd8c]=(uint8_t)(flags?0x80|value:value);
    a->ram[0x24]=b->ram[0x24]=(uint8_t)(value^0x31);
    a->ram[0x27]=b->ram[0x27]=(uint8_t)(value^0x62);
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<30) {
      a->count=b->count=0;require(dbz_native_step(&ca,stats),"expected mosaic instruction");
      lakesnes_cpu_runOpcode(&cb);compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"mosaic return");
  }
  Cpu guard=make_cpu(a,0x82a3,0);guard.d=true;
  a->count=0;require(!dbz_native_step(&guard,stats)&&a->count==0,"decimal mode falls back without effects");
  guard.d=false;guard.intWanted=true;
  require(!dbz_native_step(&guard,stats)&&a->count==0,"pending interrupt falls back without effects");
  guard=make_cpu(a,0x82d4,0);guard.xf=true;
  require(!dbz_native_step(&guard,stats)&&a->count==0,"controller index width guard");
  guard.xf=false;guard.e=true;
  require(!dbz_native_step(&guard,stats)&&a->count==0,"controller emulation guard");
  guard=make_cpu(a,0x82fb,0);guard.mf=false;
  require(!dbz_native_step(&guard,stats)&&a->count==0,"scroll accumulator width guard");
  printf("PASS: 131072 RNG, 1024 display, 32 reset, 262144 controller, 512 scroll, 1024 upload, 513 sprite, 513 palette, 512 control, 512 init, 512 finish, 32 dispatch, 512 mosaic cases; CPU state and bus sequences match.\n");
  free(stats);free(a);free(b);free(rom);return 0;
}

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
  Event events[256]; unsigned count, busy_reads;
} Bus;
static void require(bool value,const char *why) { if(!value) { fprintf(stderr,"FAIL: %s\n",why);exit(1); } }
static void record(Bus *b,uint32_t a,uint8_t v,uint8_t kind) {
  require(b->count<256,"bus log overflow");
  Event *e=&b->events[b->count++];e->address=a;e->value=v;e->kind=kind;
}
static uint8_t read_bus(void *mem,uint32_t address) {
  Bus *b=mem;uint8_t v;
  uint8_t bank=(uint8_t)(address>>16 & 0x7fu);
  /* Bank 0: real ROM. Banks 2/4/6: real ROM for $02D812 / $0485B6 / $06EF11+F14D.
   * Other banks keep the RAM overlay so fixtures can plant stubs/tables. */
  if((address & 0xffffu)>=0x8000 && (bank == 0 || bank == 2 || bank == 4 || bank == 6)) {
    size_t off = (size_t)bank * 0x8000u + (address & 0x7fffu);
    v = b->rom[off];
  } else v=b->ram[address&65535];
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
  for(unsigned routine=0;routine<3;routine++) for(unsigned value=0;value<256;value++) {
    unsigned start=routine==0?0x8282:routine==1?0x8275:0x8460;
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
  for(unsigned start_case=0;start_case<2;start_case++) for(unsigned value=0;value<256;value++) {
    memset(a->ram,0,sizeof(a->ram));memset(b->ram,0,sizeof(b->ram));
    unsigned start=start_case?0x8417:0x8402;
    Cpu ca=make_cpu(a,start,value),cb=make_cpu(b,start,value);ca.db=cb.db=0;
    word(a,0x200,0x8fff);word(b,0x200,0x8fff);
    a->ram[0x719]=b->ram[0x719]=(uint8_t)value;
    a->ram[0x718]=b->ram[0x718]=(uint8_t)(value^0x42);
    a->ram[0x716]=b->ram[0x716]=(uint8_t)(value^0x15);
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<100) {
      a->count=b->count=0;require(dbz_native_step(&ca,stats),"expected transition-timing instruction");
      lakesnes_cpu_runOpcode(&cb);compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"transition-timing return");
  }
  for(unsigned value=0;value<256;value++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x89ac,value), cb=make_cpu(b,0x89ac,value); ca.db=cb.db=0; ca.xf=cb.xf=false; ca.y=cb.y=(uint16_t)(0x1200|value);
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0; while(ca.pc!=0x9000 && steps++<10) { a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 89ac instruction"); lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b); }
    require(ca.pc==0x9000 && ca.sp==0x202,"89ac return");
  }
  for(unsigned start_i=0;start_i<2;start_i++) for(unsigned value=0;value<256;value++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    unsigned start=start_i?0x8afc:0x8af1; Cpu ca=make_cpu(a,start,value), cb=make_cpu(b,start,value); ca.db=cb.db=0; ca.xf=cb.xf=false; ca.x=cb.x=(uint16_t)(0x4000|value); ca.y=cb.y=(uint16_t)(0x2000|value);
    word(a,0x200,0x8fff); word(b,0x200,0x8fff); unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<12) { a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected coordinate helper instruction"); lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b); }
    require(ca.pc==0x9000 && ca.sp==0x202,"coordinate helper return");
  }
  for(unsigned start_i=0;start_i<2;start_i++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram)); unsigned start=start_i?0x8b0e:0x8b07;
    Cpu ca=make_cpu(a,start,0), cb=make_cpu(b,start,0); ca.db=cb.db=0; ca.xf=cb.xf=false; ca.x=cb.x=0xfff0; ca.y=cb.y=0xfff0; word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0; while(ca.pc!=0x9000 && steps++<10) { a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected increment helper instruction"); lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b); }
    require(ca.pc==0x9000 && ca.sp==0x202,"increment helper return");
  }
  for(unsigned occupied=0;occupied<8;occupied++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8711,occupied), cb=make_cpu(b,0x8711,occupied); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    for(unsigned i=0;i<occupied;i++) a->ram[0x800+i*8]=b->ram[0x800+i*8]=0x80;
    a->ram[0x800+occupied*8]=b->ram[0x800+occupied*8]=0x01;
    unsigned steps=0; while(ca.pc!=0x9000 && steps++<200) { a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected queue-find instruction"); lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b); }
    require(ca.pc==0x9000 && ca.x==(occupied*8) && ca.sp==0x202,"queue-find return");
  }
  for(unsigned base=0;base<4;base++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8724,base), cb=make_cpu(b,0x8724,base); ca.db=cb.db=0; ca.xf=cb.xf=false; ca.x=cb.x=(uint16_t)(base*8);
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x800+base*8+8]=b->ram[0x800+base*8+8]=0xff;
    unsigned steps=0; while(ca.pc!=0x9000 && steps++<20) { a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected queue-mark instruction"); lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b); }
    require(ca.pc==0x9000 && ca.x==(base*8) && a->ram[0x800+base*8+8]==0 && ca.sp==0x202,"queue-mark return");
  }
  for(unsigned value=0;value<256;value++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x86fc,value), cb=make_cpu(b,0x86fc,value); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0]=b->ram[0]=(uint8_t)value; a->ram[1]=b->ram[1]=(uint8_t)(value^0x55); a->ram[2]=b->ram[2]=(uint8_t)(value^0xaa);
    a->ram[0x2134]=b->ram[0x2134]=(uint8_t)(value^1); a->ram[0x2135]=b->ram[0x2135]=(uint8_t)(value^2); a->ram[0x2136]=b->ram[0x2136]=(uint8_t)(value^3);
    unsigned steps=0; while(ca.pc!=0x9000 && steps++<20) { a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected multiply instruction"); lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b); }
    require(ca.pc==0x9000 && ca.sp==0x202,"multiply return");
  }
  for(unsigned value=0;value<64;value++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8887,value), cb=make_cpu(b,0x8887,value); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff); word(a,0x02,(value*3+7)&0xffff); word(b,0x02,(value*3+7)&0xffff);
    unsigned steps=0; while(ca.pc!=0x9000 && steps++<40) { a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected dma-setup instruction"); lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b); }
    require(ca.pc==0x9000 && ca.sp==0x202,"dma-setup return");
  }
  for(unsigned idx=0;idx<32;idx++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x88ea,idx), cb=make_cpu(b,0x88ea,idx); ca.db=cb.db=0; ca.xf=cb.xf=false; ca.a=cb.a=0x1234;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x03]=b->ram[0x03]=(uint8_t)idx;
    a->ram[0x06]=b->ram[0x06]=0x40; a->ram[0x07]=b->ram[0x07]=0x01; a->ram[0x08]=b->ram[0x08]=0x00;
    for(unsigned i=0;i<96;i++) a->ram[0x140+i]=b->ram[0x140+i]=(uint8_t)(0x80+i);
    unsigned steps=0; while(ca.pc!=0x9000 && steps++<40) { a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected pointer-resolve instruction"); lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b); }
    require(ca.pc==0x9000 && a->ram[0x715]==1 && ca.sp==0x202,"pointer-resolve return");
  }
  for(unsigned words=1;words<=4;words++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x88c5,words), cb=make_cpu(b,0x88c5,words); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    ca.x=cb.x=(uint16_t)(0x10 + (words-1)*2); ca.y=cb.y=(uint16_t)((words-1)*2);
    a->ram[0]=b->ram[0]=0x80; a->ram[1]=b->ram[1]=0x03; a->ram[2]=b->ram[2]=0x00;
    for(unsigned i=0;i<16;i++) a->ram[0x380+i]=b->ram[0x380+i]=(uint8_t)(0x10+i);
    unsigned steps=0; while(ca.pc!=0x9000 && steps++<80) { a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected palette-copy instruction"); lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b); }
    require(ca.pc==0x9000 && ca.sp==0x202,"palette-copy return");
  }
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8b15,0), cb=make_cpu(b,0x8b15,0); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    for(unsigned i=0;i<0x40;i++) a->ram[0xd00+i]=b->ram[0xd00+i]=0xaa;
    unsigned steps=0; while(ca.pc!=0x9000 && steps++<200) { a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected clear-0d00 instruction"); lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b); }
    require(ca.pc==0x9000 && ca.sp==0x202,"clear-0d00 return");
    for(unsigned i=0;i<0x40;i+=4) require(a->ram[0xd00+i]==0,"0d00 cleared stride");
  }
  for(unsigned yv=0;yv<16;yv++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8ad6,yv), cb=make_cpu(b,0x8ad6,yv); ca.db=cb.db=0; ca.xf=cb.xf=false; ca.y=cb.y=(uint16_t)yv; ca.a=cb.a=0xabcd;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0b1c+yv]=b->ram[0x0b1c+yv]=(uint8_t)(yv%11);
    unsigned steps=0; while(ca.pc!=0x9000 && steps++<20) { a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected table-lookup instruction"); lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b); }
    require(ca.pc==0x9000 && ca.sp==0x202,"table-lookup return");
  }

  for(unsigned idx=0;idx<16;idx++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x88d8,idx), cb=make_cpu(b,0x88d8,idx); ca.db=cb.db=0; ca.xf=cb.xf=false; ca.a=cb.a=0x55aa;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x03]=b->ram[0x03]=(uint8_t)idx;
    /* $06F945 table entries each point at $000400+idx (bank 0) */
    for(unsigned i=0;i<32;i++) {
      a->ram[0xf945+i*3]=b->ram[0xf945+i*3]=(uint8_t)(0x00+i);
      a->ram[0xf945+i*3+1]=b->ram[0xf945+i*3+1]=0x04;
      a->ram[0xf945+i*3+2]=b->ram[0xf945+i*3+2]=0x00;
    }
    for(unsigned i=0;i<0x40;i++) a->ram[0x400+i]=b->ram[0x400+i]=(uint8_t)(0x80+i);
    unsigned steps=0; while(ca.pc!=0x9000 && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 88d8 instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && a->ram[0x715]==1 && ca.sp==0x202,"88d8 return");
  }
  for(unsigned value=0;value<32;value++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8908,value), cb=make_cpu(b,0x8908,value); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    for(unsigned i=0;i<0x40;i++) a->ram[0xd200+i]=b->ram[0xd200+i]=(uint8_t)(0x10+i+value);
    unsigned steps=0; while(ca.pc!=0x9000 && steps++<200) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8908 instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && a->ram[0x715]==1 && ca.sp==0x202,"8908 return");
    for(unsigned i=0;i<0x40;i++) require(a->ram[0x2c0+i]==(uint8_t)(0x10+i+value),"8908 palette bytes");
  }
  for(unsigned flags=0;flags<4;flags++) for(unsigned value=0;value<32;value++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x86c8,value), cb=make_cpu(b,0x86c8,value); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1a1]=b->ram[0x1a1]=(uint8_t)(flags&1?0x80|value:value);
    a->ram[0x1a0]=b->ram[0x1a0]=(uint8_t)(value*3);
    a->ram[0x1bd]=b->ram[0x1bd]=(uint8_t)(value*5);
    for(unsigned i=0;i<0x80;i++) a->ram[0x8584+i]=b->ram[0x8584+i]=(uint8_t)(0xa0+i);
    unsigned steps=0; while(ca.pc!=0x9000 && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 86c8 instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"86c8 return");
  }
  for(unsigned rem=0;rem<3;rem++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8732,rem), cb=make_cpu(b,0x8732,rem); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0]=b->ram[0]=0x80; a->ram[1]=b->ram[1]=0x03; a->ram[2]=b->ram[2]=0x00;
    a->ram[4]=b->ram[4]=(uint8_t)(rem*2); a->ram[5]=b->ram[5]=0;
    a->ram[6]=b->ram[6]=0x10; a->ram[7]=b->ram[7]=0x20;
    for(unsigned i=0;i<16;i++) a->ram[0x380+i]=b->ram[0x380+i]=(uint8_t)(0x50+i);
    unsigned steps=0; while(ca.pc!=0x9000 && steps++<200) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8732 instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"8732 remainder return");
  }
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8746,0), cb=make_cpu(b,0x8746,0); ca.db=cb.db=0; ca.xf=cb.xf=false; ca.y=cb.y=0xfffe;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0]=b->ram[0]=0x80; a->ram[1]=b->ram[1]=0x03; a->ram[2]=b->ram[2]=0x00;
    a->ram[4]=b->ram[4]=0; a->ram[5]=b->ram[5]=1;
    for(unsigned i=0;i<8;i++) a->ram[0x380+i]=b->ram[0x380+i]=(uint8_t)(0x70+i);
    unsigned steps=0; while(ca.pc!=0x9000 && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8732 page-wrap instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202 && a->ram[5]==0,"8732 page-wrap return");
  }
  for(unsigned pair=0;pair<8;pair++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8921,pair), cb=make_cpu(b,0x8921,pair); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1d2]=b->ram[0x1d2]=(uint8_t)pair;
    a->ram[0x1d3]=b->ram[0x1d3]=(uint8_t)(pair+1);
    for(unsigned i=0;i<32;i++) {
      a->ram[0xf945+i*3]=b->ram[0xf945+i*3]=0x00;
      a->ram[0xf945+i*3+1]=b->ram[0xf945+i*3+1]=0x04;
      a->ram[0xf945+i*3+2]=b->ram[0xf945+i*3+2]=0x00;
    }
    for(unsigned i=0;i<0x200;i++) a->ram[0x400+i]=b->ram[0x400+i]=(uint8_t)(0x11+(i&0x3f));
    unsigned steps=0; while(ca.pc!=0x9000 && steps++<4000) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8921 instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"8921 return");
  }
  for(unsigned idx=0;idx<8;idx++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x88ae,idx), cb=make_cpu(b,0x88ae,idx); ca.db=cb.db=0; ca.xf=cb.xf=false; ca.a=cb.a=0x4321;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x03]=b->ram[0x03]=(uint8_t)idx;
    for(unsigned i=0;i<32;i++) {
      a->ram[0xf876+i*3]=b->ram[0xf876+i*3]=0x00;
      a->ram[0xf876+i*3+1]=b->ram[0xf876+i*3+1]=0x05;
      a->ram[0xf876+i*3+2]=b->ram[0xf876+i*3+2]=0x00;
    }
    for(unsigned i=0;i<0x100;i++) a->ram[0x500+i]=b->ram[0x500+i]=(uint8_t)(0x30+i);
    unsigned steps=0; while(ca.pc!=0x9000 && steps++<2000) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 88ae instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"88ae return");
  }

  /* $008B28: HDMA-mask clear + $0800 wipe + dispatch JMP */
  for(unsigned mask=0;mask<8;mask++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8b28,mask), cb=make_cpu(b,0x8b28,mask); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x62]=b->ram[0x62]=(uint8_t)(0xff - mask);
    a->ram[0x24]=b->ram[0x24]=(uint8_t)(mask & 3);
    a->ram[0x85b6]=b->ram[0x85b6]=0x6b; /* stub JSL $0485B6 */
    a->ram[0xfb8d]=b->ram[0xfb8d]=0x6b; /* stub JSL $03FB8D */
    for(unsigned i=0;i<0x80;i++) a->ram[0x800+i]=b->ram[0x800+i]=0x5a;
    unsigned steps=0;
    while(ca.pc>=0x8b28 && ca.pc<=0x8b53 && steps++<2000) {
      a->count=b->count=0;
      if(!dbz_native_step(&ca,stats)) lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(steps<2000 && !(ca.pc>=0x8b28 && ca.pc<=0x8b53),"8b28 reached jump target");
    for(unsigned i=0;i<0x80;i++) require(a->ram[0x800+i]==0,"8b28 queue wiped");
  }
  /* $008996: fill $7E4800 with $FFFF */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8996,0), cb=make_cpu(b,0x8996,0); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    for(unsigned i=0;i<0x400;i++) a->ram[0x4800+i]=b->ram[0x4800+i]=0x11;
    a->ram[0xd7d]=b->ram[0xd7d]=0xaa; a->ram[0xd7e]=b->ram[0xd7e]=0xbb;
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<3000) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8996 instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"8996 return");
    for(unsigned i=0;i<0x400;i++) require(a->ram[0x4800+i]==0xff,"8996 fill");
    require(a->ram[0xd7d]==0 && a->ram[0xd7e]==0,"8996 cleared 0d7d");
  }
  /* $009B36: queue producer from [DP+$04] */
  for(unsigned len=1;len<=4;len++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x9b36,len), cb=make_cpu(b,0x9b36,len); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[4]=b->ram[4]=0x40; a->ram[5]=b->ram[5]=0x03; a->ram[6]=b->ram[6]=0x00; a->ram[8]=b->ram[8]=0;
    a->ram[0x340]=(uint8_t)len; b->ram[0x340]=(uint8_t)len;
    a->ram[0x341]=b->ram[0x341]=0x12; a->ram[0x342]=b->ram[0x342]=0x34;
    for(unsigned i=0;i<len;i++) a->ram[0x343+i]=b->ram[0x343+i]=(uint8_t)(0x90+i);
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<400) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 9b36 instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"9b36 return");
    require(a->ram[0x800]==0x80 && a->ram[0x806]==(uint8_t)len,"9b36 queue header");
  }
  /* $009B79: compact DP queue producer */
  for(unsigned sz=1;sz<=4;sz++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x9b79,sz), cb=make_cpu(b,0x9b79,sz); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x00,0x1234); word(b,0x00,0x1234);
    a->ram[2]=b->ram[2]=(uint8_t)(sz | (sz==3?0x80:0));
    word(a,0x713,0x0010); word(b,0x713,0x0010);
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<200) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 9b79 instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"9b79 return");
    require((a->ram[0x800]&0x80)!=0,"9b79 occupied");
  }
  /* $008954: palette index load via $06F945 / $8AF1 */
  for(unsigned idx=0;idx<8;idx++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8954,idx), cb=make_cpu(b,0x8954,idx); ca.db=cb.db=0; ca.xf=cb.xf=false;
    ca.a=cb.a=(uint16_t)(0x5500|idx); ca.y=cb.y=0x0010;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    for(unsigned i=0;i<16;i++) {
      a->ram[0xf945+i*3]=b->ram[0xf945+i*3]=0x00;
      a->ram[0xf945+i*3+1]=b->ram[0xf945+i*3+1]=0x04;
      a->ram[0xf945+i*3+2]=b->ram[0xf945+i*3+2]=0x00;
    }
    for(unsigned i=0;i<0x40;i++) a->ram[0x400+i]=b->ram[0x400+i]=(uint8_t)(0x20+i);
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<500) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8954 instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && a->ram[0x715]==1 && ca.sp==0x202,"8954 return");
  }
  /* $008E26: scene pointer build */
  for(unsigned av=0;av<8;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8e26,av), cb=make_cpu(b,0x8e26,av); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1a0]=b->ram[0x1a0]=(uint8_t)(av<4?av:0x30+av);
    for(unsigned i=0;i<0x40;i++) a->ram[0xe41b+i]=b->ram[0xe41b+i]=(uint8_t)(0x70+i);
    /* $06F2DE + (A<<1): place pointer records in ram mirror of bank 0 at F2DE */
    for(unsigned i=0;i<0x80;i++) {
      a->ram[0xf2de + i]=b->ram[0xf2de + i]=0;
    }
    /* For each doubled index, store little-endian ptr to $000500 and nested at $000510 */
    for(unsigned i=0;i<0x40;i++) {
      unsigned off=0xf2de + i * 2;
      a->ram[off]=b->ram[off]=0x00; a->ram[off+1]=b->ram[off+1]=0x05;
    }
    a->ram[0x500]=b->ram[0x500]=0x10; a->ram[0x501]=b->ram[0x501]=0x05; /* nested ptr $0510 */
    a->ram[0x510]=b->ram[0x510]=0xab; a->ram[0x511]=b->ram[0x511]=0xcd;
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<200) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8e26 instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"8e26 return");
  }
  /* $008D2B: multiply/table helper */
  for(unsigned av=0;av<8;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8d2b,av), cb=make_cpu(b,0x8d2b,av); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1bd]=b->ram[0x1bd]=(uint8_t)(av*3+1);
    word(a,0x4216,(av*5)&0xffff); word(b,0x4216,(av*5)&0xffff);
    for(unsigned i=0;i<0x100;i++) {
      a->ram[0xe629+i]=b->ram[0xe629+i]=(uint8_t)(i*3);
      a->ram[0xe3fb+i]=b->ram[0xe3fb+i]=(uint8_t)(0x40+i);
    }
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8d2b instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"8d2b return");
  }
  /* $0485B6: 8-slot allocator */
  for(unsigned av=0;av<8;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x85b6,av), cb=make_cpu(b,0x85b6,av);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.a=cb.a=(uint8_t)(av==0?0x80:(0x10+av));
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    for(unsigned i=0;i<8;i++) a->ram[0x1309+i]=b->ram[0x1309+i]=(uint8_t)(av>i?0x11+i:0);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==4) && steps++<80) {
      /* RTL returns to bank on stack; make_cpu pushed 00:8FFF so k becomes 0 */
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 85b6 instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
      if(ca.pc==0x9000) break;
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"85b6 return");
  }
  /* $00C559 empty stream + $00C68C one tile + $0090E6/$00946F/$0090C1 chain pieces */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xc559,av), cb=make_cpu(b,0xc559,av); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x83,0x8000); word(b,0x83,0x8000); a->ram[0x85]=b->ram[0x85]=0x08;
    a->ram[0x8000]=b->ram[0x8000]=0; a->ram[0x8001]=b->ram[0x8001]=0; /* length 0 */
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<200) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected c559 instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"c559 empty return");
  }
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xc68c,av), cb=make_cpu(b,0xc68c,av); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x86,0x9000); word(b,0x86,0x9000);
    word(a,0x89,0x0010); word(b,0x89,0x0010);
    a->ram[0x88]=b->ram[0x88]=0x7e;
    for(unsigned i=0;i<16;i++) a->ram[0x9000+i]=b->ram[0x9000+i]=(uint8_t)(0xa0+i);
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<400) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected c68c instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"c68c return");
  }
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x90e6,av), cb=make_cpu(b,0x90e6,av); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff); word(a,0x89,0x0010); word(b,0x89,0x0010);
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 90e6 instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"90e6 return");
  }
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x946f,av), cb=make_cpu(b,0x946f,av); ca.db=cb.db=0; ca.xf=cb.xf=false;
    ca.sp=cb.sp=0x1bf; word(a,0x1c0,0x8fff); word(b,0x1c0,0x8fff); a->ram[0x1c2]=b->ram[0x1c2]=0;
    for(unsigned i=0;i<0x80;i++) a->ram[0xdd44+i]=b->ram[0xdd44+i]=(uint8_t)(0x20+i);
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<2000) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 946f instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x1c2,"946f return");
  }
  /* $008DFE full E2E through native $90C1 callees and stubbed $06F14D */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8dfe,av), cb=make_cpu(b,0x8dfe,av); ca.db=cb.db=0; ca.xf=cb.xf=false;
    ca.sp=cb.sp=0x1bf; word(a,0x1c0,0x8fff); word(b,0x1c0,0x8fff); a->ram[0x1c2]=b->ram[0x1c2]=0;
    a->ram[0x1a0]=b->ram[0x1a0]=(uint8_t)av;
    a->ram[0x1d2]=b->ram[0x1d2]=(uint8_t)av;
    a->ram[0x1d3]=b->ram[0x1d3]=(uint8_t)(av+1);
    a->ram[0x1a1]=b->ram[0x1a1]=0; a->ram[0x1bd]=b->ram[0x1bd]=(uint8_t)(av*3);
    for(unsigned i=0;i<32;i++) {
      a->ram[0xf876+i*3]=b->ram[0xf876+i*3]=0x00;
      a->ram[0xf876+i*3+1]=b->ram[0xf876+i*3+1]=0x05;
      a->ram[0xf876+i*3+2]=b->ram[0xf876+i*3+2]=0x00;
      a->ram[0xf945+i*3]=b->ram[0xf945+i*3]=0x00;
      a->ram[0xf945+i*3+1]=b->ram[0xf945+i*3+1]=0x04;
      a->ram[0xf945+i*3+2]=b->ram[0xf945+i*3+2]=0x00;
    }
    for(unsigned i=0;i<0x200;i++) {
      a->ram[0x400+i]=b->ram[0x400+i]=(uint8_t)(0x11+(i&0x3f));
      a->ram[0x500+i]=b->ram[0x500+i]=(uint8_t)(0x30+(i&0x3f));
    }
    for(unsigned i=0;i<0x40;i++) a->ram[0xd200+i]=b->ram[0xd200+i]=(uint8_t)(0x10+i);
    for(unsigned i=0;i<0x80;i++) a->ram[0x8584+i]=b->ram[0x8584+i]=(uint8_t)(0xa0+i);
    for(unsigned i=0;i<0x80;i++) a->ram[0xdd44+i]=b->ram[0xdd44+i]=(uint8_t)(0x20+i);
    a->ram[0x8000]=b->ram[0x8000]=0; a->ram[0x8001]=b->ram[0x8001]=0; /* empty decompress */
    unsigned steps=0;
    while(!(ca.pc==0xf14d && ca.k==6) && steps++<20000) {
      a->count=b->count=0;
      if(!dbz_native_step(&ca,stats)) lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xf14d && ca.k==6 && a->ram[0x700]==0x20,"8dfe reached native F14D");
  }
  /* $008E18..8E21: attr helper + $0700=$20 until JSL $06F14D */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8e18,av), cb=make_cpu(b,0x8e18,av); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1a1]=b->ram[0x1a1]=0; a->ram[0x1a0]=b->ram[0x1a0]=(uint8_t)av;
    a->ram[0x1bd]=b->ram[0x1bd]=(uint8_t)(av*3);
    for(unsigned i=0;i<0x80;i++) a->ram[0x8584+i]=b->ram[0x8584+i]=(uint8_t)(0xa0+i);
    unsigned steps=0;
    while(!(ca.pc==0xf14d && ca.k==6) && steps++<80) {
      a->count=b->count=0;
      if(!dbz_native_step(&ca,stats)) lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xf14d && ca.k==6 && a->ram[0x700]==0x20,"8e18 reached F14D");
  }
  /* $02D812: early RTL when A not in {$33,$35,$37}; copy path when A=$35 */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xd812,av), cb=make_cpu(b,0xd812,av);
    ca.k=cb.k=2; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.a=cb.a=0x10; /* miss */
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected d812 miss");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
      if(ca.pc==0x9000) break;
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"d812 early rtl");
  }
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xd812,av), cb=make_cpu(b,0xd812,av);
    ca.k=cb.k=2; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.a=cb.a=0x35;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x1a2,0x1111); word(b,0x1a2,0x1111); word(a,0x1a4,0x2222); word(b,0x1a4,0x2222);
    word(a,0x33,0x3333); word(b,0x33,0x3333); word(a,0x35,0x4444); word(b,0x35,0x4444);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected d812 copy");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
      if(ca.pc==0x9000) break;
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"d812 copy rtl");
    require(getword(a,0x1c0)==0x1111 && getword(a,0x1c4)==0x1111,"d812 01a2 copies");
    require(getword(a,0x1c2)==0x2222 && getword(a,0x1c6)==0x2222,"d812 01a4 copies");
    require(getword(a,0x1c8)==0x3333 && getword(a,0x1cc)==0x3333,"d812 dp33 copies");
    require(getword(a,0x1ca)==0x4444 && getword(a,0x1ce)==0x4444,"d812 dp35 copies");
  }
  /* $009BBF: tiled VRAM enqueue from [DP+$00]; 1x1 tile */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x9bbf,av), cb=make_cpu(b,0x9bbf,av); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    /* long pointer DP+$00 -> $000400 bank0 RAM */
    a->ram[0]=b->ram[0]=0x00; a->ram[1]=b->ram[1]=0x04; a->ram[2]=b->ram[2]=0x00;
    word(a,0x03,0x2000); word(b,0x03,0x2000);
    a->ram[0x400]=b->ram[0x400]=0; /* width count */
    a->ram[0x401]=b->ram[0x401]=0; /* row count */
    a->ram[0x402]=b->ram[0x402]=0xab; a->ram[0x403]=b->ram[0x403]=0xcd;
    word(a,0x711,0x0000); word(b,0x711,0x0000);
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<400) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 9bbf instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"9bbf return");
    require(a->ram[0x800]==0x80 && a->ram[0x806]==0x02,"9bbf queue length");
    require(a->ram[0xa00]==0xab && a->ram[0xa01]==0xcd,"9bbf payload words");
    require(getword(a,0x711)==0x0002,"9bbf advanced 0711");
  }
  /* $008E76: through native 8D2B+$02D812 until unrecovered $8B7A */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8e76,av), cb=make_cpu(b,0x8e76,av); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x1a2,0x1234); word(b,0x1a2,0x1234); word(a,0x1a4,0x5678); word(b,0x1a4,0x5678);
    word(a,0x33,0x0101); word(b,0x33,0x0101); word(a,0x35,0x0202); word(b,0x35,0x0202);
    a->ram[0x1bd]=b->ram[0x1bd]=(uint8_t)(av+1);
    word(a,0x4216,(av+1)*5); word(b,0x4216,(av+1)*5);
    for(unsigned i=0;i<0x100;i++) {
      a->ram[0xe629+i]=b->ram[0xe629+i]=(uint8_t)i;
      a->ram[0xe3fb+i]=b->ram[0xe3fb+i]=(uint8_t)(0x10+i);
    }
    unsigned steps=0;
    while(!(ca.pc==0x8b7a && ca.k==0) && steps++<200) {
      a->count=b->count=0;
      if(!dbz_native_step(&ca,stats)) lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x8b7a && ca.k==0 && a->ram[0x1a1]==0x80,"8e76 reached unrecovered 8B7A");
    require(getword(a,0x1de)==0x1234 && getword(a,0x1e0)==0x5678,"8e76 scroll snapshot");
  }
  /* $06F14D: clamp preamble until first JSL $06EF11 */
  for(unsigned av=0;av<8;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xf14d,av), cb=make_cpu(b,0xf14d,av);
    ca.k=cb.k=6; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x700]=b->ram[0x700]=0x20;
    a->ram[0x702]=b->ram[0x702]=(uint8_t)(2 + (av & 3));
    a->ram[0x703]=b->ram[0x703]=(uint8_t)(2 + ((av >> 2) & 3));
    word(a,0x1a2,0x0090 + av * 3); word(b,0x1a2,0x0090 + av * 3);
    word(a,0x1a4,0x0088 + av * 5); word(b,0x1a4,0x0088 + av * 5);
    a->ram[0x1a1]=b->ram[0x1a1]=0x00;
    unsigned steps=0;
    while(!(ca.pc==0xf1ca && ca.k==6) && steps++<200) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected f14d instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xf1ca && ca.k==6,"f14d reached EF11 call");
  }
  /* $06F14D underflow clamp path: positions below origin -> zero scrolls */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xf14d,av), cb=make_cpu(b,0xf14d,av);
    ca.k=cb.k=6; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x702]=b->ram[0x702]=0x10; a->ram[0x703]=b->ram[0x703]=0x10;
    word(a,0x1a2,0x0010); word(b,0x1a2,0x0010);
    word(a,0x1a4,0x0010); word(b,0x1a4,0x0010);
    unsigned steps=0;
    while(!(ca.pc==0xf1ca && ca.k==6) && steps++<200) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected f14d underflow");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(a->ram[0x33]==0 && a->ram[0x34]==0 && a->ram[0x35]==0 && a->ram[0x36]==0,
            "f14d underflow cleared scrolls");
  }
  /* $06F089 early-exit: negative/out-of-range coords clear DP+$04/$05 via $F082 */
  for(unsigned path=0;path<4;path++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xf089,path), cb=make_cpu(b,0xf089,path);
    ca.k=cb.k=6; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x702]=b->ram[0x702]=0x10; a->ram[0x703]=b->ram[0x703]=0x10;
    if(path==0) { a->ram[1]=b->ram[1]=0x80; } /* BMI $01 */
    else if(path==1) { a->ram[1]=b->ram[1]=0x10; } /* CMP $0703 BEQ */
    else if(path==2) { a->ram[1]=b->ram[1]=0x05; a->ram[3]=b->ram[3]=0x80; }
    else { a->ram[1]=b->ram[1]=0x05; a->ram[3]=b->ram[3]=0x10; }
    a->ram[4]=b->ram[4]=0xaa; a->ram[5]=b->ram[5]=0xbb;
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected f089 early-exit");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"f089 early-exit return");
    require(a->ram[4]==0 && a->ram[5]==0,"f089 early-exit cleared DP+04/05");
  }
  /* $06F089 success path: planted multiply product + map bytes */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xf089,av), cb=make_cpu(b,0xf089,av);
    ca.k=cb.k=6; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.y=cb.y=0x1234;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0]=b->ram[0]=(uint8_t)(0x10+av);
    a->ram[1]=b->ram[1]=0x02;
    a->ram[2]=b->ram[2]=(uint8_t)(0x20+av);
    a->ram[3]=b->ram[3]=0x03;
    a->ram[0x702]=b->ram[0x702]=0x20; a->ram[0x703]=b->ram[0x703]=0x20;
    word(a,0x704,0x0400); word(b,0x704,0x0400);
    a->ram[0x706]=b->ram[0x706]=0x00; /* bank 0 long pointer */
    word(a,0x707,0x0500); word(b,0x707,0x0500);
    a->ram[0x709]=b->ram[0x709]=0x00;
    word(a,0x4216,0x0011*(av+1)); word(b,0x4216,0x0011*(av+1));
    /* tile index byte at [$04] after reloc = base $0400 + product + row */
    for(unsigned i=0;i<0x200;i++) a->ram[0x400+i]=b->ram[0x400+i]=(uint8_t)(i&0x3f);
    for(unsigned i=0;i<0x200;i++) a->ram[0x500+i]=b->ram[0x500+i]=(uint8_t)(0x80+i);
    /* $06F1EE table comes from real bank-6 ROM via read_bus */
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<300) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected f089 success");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"f089 success return");
    require(ca.y==0x1234,"f089 restored Y");
  }
  /* $06EF11: build queue entry until first JSL $06F089 */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xef11,av), cb=make_cpu(b,0xef11,av);
    ca.k=cb.k=6; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x700]=b->ram[0x700]=(av&1)?0x10:0x00;
    a->ram[0]=b->ram[0]=0x40; a->ram[1]=b->ram[1]=0x01;
    a->ram[2]=b->ram[2]=0x80; a->ram[3]=b->ram[3]=0x02;
    unsigned steps=0;
    while(!(ca.pc==0xf089 && ca.k==6) && steps++<80) {
      a->count=b->count=0;
      if(!dbz_native_step(&ca,stats)) lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xf089 && ca.k==6,"ef11 reached F089");
    require(a->ram[0x800]==0x81,"ef11 VMAIN/occupied");
    require(a->ram[0x803]==0x80 && a->ram[0x804]==0x08,"ef11 source 7E:0880");
    require(a->ram[0x806]==0x40 && a->ram[0x807]==0x00,"ef11 length $40");
  }
  /* $06EF11 full: F089 early-exits each iteration (coords out of range) */
  for(unsigned av=0;av<2;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xef11,av), cb=make_cpu(b,0xef11,av);
    ca.k=cb.k=6; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x700]=b->ram[0x700]=0x00;
    a->ram[0]=b->ram[0]=0x20; a->ram[1]=b->ram[1]=0x80; /* negative row -> F089 early */
    a->ram[2]=b->ram[2]=0x00; a->ram[3]=b->ram[3]=0x00;
    a->ram[0x702]=b->ram[0x702]=0x10; a->ram[0x703]=b->ram[0x703]=0x10;
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<5000) {
      a->count=b->count=0;
      if(!dbz_native_step(&ca,stats)) lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"ef11 full return");
    require(a->ram[0x800]==0x81,"ef11 kept queue head");
    /* 32 early-exit words of zero written to $0880 */
    require(a->ram[0x880]==0 && a->ram[0x881]==0 && a->ram[0x8be]==0 && a->ram[0x8bf]==0,
            "ef11 wrote zero tile words");
  }

  /* $0089B8: accumulate through stubs — plant RTL at unrecovered callees via
   * executing until first JSL then verifying native body with interpreted 8A28/8A69
   * early path: provide actor tables so 8A28 can complete. */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x89b8,av), cb=make_cpu(b,0x89b8,av); ca.db=cb.db=0; ca.xf=cb.xf=false;
    ca.x=cb.x=0; ca.y=cb.y=0;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    /* long pointer DP+$00 -> $000600 */
    a->ram[0]=b->ram[0]=0x00; a->ram[1]=b->ram[1]=0x06; a->ram[2]=b->ram[2]=0x00;
    for(unsigned i=0;i<0x20;i++) a->ram[0x600+i]=b->ram[0x600+i]=(uint8_t)(0x10+i);
    for(unsigned i=0;i<0x20;i++) a->ram[0xb00+i]=b->ram[0xb00+i]=(uint8_t)(i);
    /* $0B1C actor type / fields used by 8A28 */
    for(unsigned i=0;i<0x40;i++) a->ram[0xb1c+i]=b->ram[0xb1c+i]=0;
    a->ram[0xb1c]=b->ram[0xb1c]=0; /* type */
    word(a,0x4216,0x0000); word(b,0x4216,0x0000);
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<2000) {
      a->count=b->count=0;
      if(!dbz_native_step(&ca,stats)) lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"89b8 return");
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
  printf("PASS: native equivalence suites including EF11/F089/F14D/89B8/D812/9BBF/8D2B/85B6/C559/C68C/90E6/946F and 8DFE-to-F14D; CPU state and bus sequences match.\n");
  free(stats);free(a);free(b);free(rom);return 0;
}

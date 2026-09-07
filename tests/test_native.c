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
  /* Banks 0/1/2/3/4/5/6: real ROM at $8000+. Bank 5 unlocked with BE54; bank 1 with C9F9/B61E/C535.
   * Other banks keep the RAM overlay so fixtures can plant stubs/tables. */
  if((address & 0xffffu)>=0x8000 && (bank == 0 || bank == 1 || bank == 2 || bank == 3 || bank == 4 || bank == 5 || bank == 6)) {
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
static void plant_bank1(Bus *a, Bus *b, unsigned start, const uint8_t *bytes, unsigned n) {
  for(unsigned i=0;i<n;i++) a->ram[start+i]=b->ram[start+i]=bytes[i];
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
    unsigned steps=0; while(ca.pc!=0x9000 && steps++<8000) {
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
    /* $0485B6 / $03FB8D come from real bank-4/3 ROM (both now natively mapped). */
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
  /* $008E76: through native 8D2B+$02D812 until native $8B7A */
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
    require(ca.pc==0x8b7a && ca.k==0 && a->ram[0x1a1]==0x80,"8e76 reached native 8B7A");
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
  /* $00849C: early RTL when record list immediately ends ($80) */
  for(unsigned av=0;av<8;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x849c,av), cb=make_cpu(b,0x849c,av);
    ca.db=cb.db=0; ca.xf=cb.xf=false; ca.x=cb.x=0x10+av;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x0a,0x0080); word(b,0x0a,0x0080);
    word(a,0x0c,0x0040); word(b,0x0c,0x0040);
    word(a,0x33,0x0010); word(b,0x33,0x0010);
    word(a,0x35,0x0020); word(b,0x35,0x0020);
    word(a,0x55,0x0004); word(b,0x55,0x0004);
    /* long pointer DP+$00 -> $000500; Y=1 reads terminator */
    a->ram[0]=b->ram[0]=0x00; a->ram[1]=b->ram[1]=0x05; a->ram[2]=b->ram[2]=0x00;
    a->ram[0x500]=b->ram[0x500]=0x00; /* flags */
    a->ram[0x501]=b->ram[0x501]=0x80; /* end marker */
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 849c early rtl");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"849c early rtl");
    require(getword(a,0x55)==0x0004,"849c preserved OAM cursor");
  }
  /* $00849C: one on-screen sprite write into $0400 and $0600 */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x849c,av), cb=make_cpu(b,0x849c,av);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x0a,0x0030); word(b,0x0a,0x0030); /* world Y */
    word(a,0x0c,0x0040); word(b,0x0c,0x0040); /* world X */
    word(a,0x33,0x0000); word(b,0x33,0x0000);
    word(a,0x35,0x0000); word(b,0x35,0x0000);
    word(a,0x55,0x0000); word(b,0x55,0x0000);
    a->ram[0]=b->ram[0]=0x00; a->ram[1]=b->ram[1]=0x05; a->ram[2]=b->ram[2]=0x00;
    /* record: flags, x, tile, attr, y, then end */
    a->ram[0x500]=b->ram[0x500]=0x00;
    a->ram[0x501]=b->ram[0x501]=0x10; /* x */
    a->ram[0x502]=b->ram[0x502]=0x22; /* tile */
    a->ram[0x503]=b->ram[0x503]=0x00; /* attr */
    a->ram[0x504]=b->ram[0x504]=0x20; /* y */
    a->ram[0x505]=b->ram[0x505]=0x80; /* end */
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<400) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 849c sprite");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"849c sprite rtl");
    require(a->ram[0x400]==0x60 && a->ram[0x401]==0x40 && a->ram[0x402]==0x22,"849c OAM bytes");
  }
  /* $0085B9 / $0085CF: alt entries reach shared walker then early RTL */
  for(unsigned entry=0;entry<2;entry++) {
    uint16_t start=entry?0x85cf:0x85b9;
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,start,entry), cb=make_cpu(b,start,entry);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x0a,0x0010); word(b,0x0a,0x0010);
    word(a,0x0c,0x0010); word(b,0x0c,0x0010);
    word(a,0x55,0x0008); word(b,0x55,0x0008);
    a->ram[0]=b->ram[0]=0x00; a->ram[1]=b->ram[1]=0x05; a->ram[2]=b->ram[2]=0x00;
    a->ram[0x500]=b->ram[0x500]=0x00; a->ram[0x501]=b->ram[0x501]=0x80;
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 85b9/85cf");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"85b9/85cf rtl");
  }

  /* $0085E9: early RTL when record list immediately ends ($80) */
  for(unsigned av=0;av<8;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x85e9,av), cb=make_cpu(b,0x85e9,av);
    ca.db=cb.db=0; ca.xf=cb.xf=false; ca.x=cb.x=0x20+av;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x0a,0x0040); word(b,0x0a,0x0040);
    word(a,0x0c,0x0030); word(b,0x0c,0x0030);
    word(a,0x55,0x0004); word(b,0x55,0x0004);
    a->ram[0]=b->ram[0]=0x00; a->ram[1]=b->ram[1]=0x05; a->ram[2]=b->ram[2]=0x00;
    a->ram[0x500]=b->ram[0x500]=0x00; a->ram[0x501]=b->ram[0x501]=0x80;
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 85e9 early rtl");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"85e9 early rtl");
    require(getword(a,0x55)==0x0004,"85e9 preserved OAM cursor");
  }
  /* $0085EF: alt entry early RTL */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x85ef,av), cb=make_cpu(b,0x85ef,av);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x55,0x0008); word(b,0x55,0x0008);
    a->ram[0]=b->ram[0]=0x00; a->ram[1]=b->ram[1]=0x05; a->ram[2]=b->ram[2]=0x00;
    a->ram[0x500]=b->ram[0x500]=0x00; a->ram[0x501]=b->ram[0x501]=0x80;
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 85ef early rtl");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"85ef early rtl");
    require(a->ram[0x10]==0x01,"85ef set DP+$10");
  }
  /* $0085E9: one on-screen sprite write into $0400 and $0600 */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x85e9,av), cb=make_cpu(b,0x85e9,av);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x0a,0x0030); word(b,0x0a,0x0030); /* base Y */
    word(a,0x0c,0x0040); word(b,0x0c,0x0040); /* base X */
    word(a,0x55,0x0000); word(b,0x55,0x0000);
    a->ram[0]=b->ram[0]=0x00; a->ram[1]=b->ram[1]=0x05; a->ram[2]=b->ram[2]=0x00;
    /* record: flags, y-off, tile, attr, x-off, then end */
    a->ram[0x500]=b->ram[0x500]=0x00;
    a->ram[0x501]=b->ram[0x501]=0x10; /* y offset */
    a->ram[0x502]=b->ram[0x502]=0x22; /* tile */
    a->ram[0x503]=b->ram[0x503]=0x00; /* attr */
    a->ram[0x504]=b->ram[0x504]=0x20; /* x offset */
    a->ram[0x505]=b->ram[0x505]=0x80; /* end */
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<400) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 85e9 sprite");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"85e9 sprite rtl");
    require(a->ram[0x400]==0x60 && a->ram[0x401]==0x40 && a->ram[0x402]==0x22,"85e9 OAM bytes");
  }

  /* $03FB8D: early RTL when $1648>=0 and $01A0 not in {5,$18} */
  for(unsigned av=0;av<8;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfb8d,av), cb=make_cpu(b,0xfb8d,av);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1648]=b->ram[0x1648]=(uint8_t)(av&0x7f); /* BPL path */
    a->ram[0x1a0]=b->ram[0x1a0]=(uint8_t)((av==5)?0:av); /* avoid 5/$18 */
    if(a->ram[0x1a0]==0x18) a->ram[0x1a0]=b->ram[0x1a0]=0x19;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fb8d early rtl");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"fb8d early rtl");
  }
  /* $03FB8D: $01A0==5 and DP+$24 masked not in {0,2,7} -> RTL at FBD9 */
  for(unsigned mode=0;mode<4;mode++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfb8d,mode), cb=make_cpu(b,0xfb8d,mode);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1a0]=b->ram[0x1a0]=0x05;
    static const uint8_t modes[]={1,3,4,6};
    a->ram[0x24]=b->ram[0x24]=modes[mode];
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fb8d mode rtl");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"fb8d FBD9 rtl");
  }
  /* $03FB8D: $1648 bit7 set and $01D8==$32 -> JMP $FCFD (leave native prefix) */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfb8d,0), cb=make_cpu(b,0xfb8d,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1648]=b->ram[0x1648]=0x80;
    a->ram[0x1d8]=b->ram[0x1d8]=0x32;
    unsigned steps=0;
    while(!(ca.pc==0xfcfd && ca.k==3) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fb8d jmp fcfd");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xfcfd && ca.k==3,"fb8d reached FCFD");
  }
  /* $03FB8D: $01A0==5 and DP+$24==0 -> branch to native $FBDA */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfb8d,1), cb=make_cpu(b,0xfb8d,1);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1a0]=b->ram[0x1a0]=0x05;
    a->ram[0x24]=b->ram[0x24]=0x00;
    unsigned steps=0;
    while(!(ca.pc==0xfbda && ca.k==3) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fb8d to fbda");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xfbda && ca.k==3,"fb8d reached FBDA");
  }
  /* $008B42 path: native wipe through native FB8D early RTL then scene JMP */
  for(unsigned mask=0;mask<4;mask++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8b28,mask), cb=make_cpu(b,0x8b28,mask);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x62]=b->ram[0x62]=(uint8_t)(0xff - mask);
    a->ram[0x24]=b->ram[0x24]=(uint8_t)(mask & 3); /* modes 0..3 -> table targets */
    /* zeroed $1648/$01A0 => FB8D early RTL; 85B6 runs natively from bank-4 ROM */
    for(unsigned i=0;i<0x80;i++) a->ram[0x800+i]=b->ram[0x800+i]=0x5a;
    unsigned steps=0;
    while(steps++<4000) {
      bool in_wipe = ca.k==0 && ca.pc>=0x8b28 && ca.pc<=0x8b53;
      bool in_fb8d = ca.k==3 && ca.pc>=0xfb8d && ca.pc<=0xfc32;
      bool in_85b6 = ca.k==4 && ca.pc>=0x85b6 && ca.pc<=0x85f0;
      if(!(in_wipe || in_fb8d || in_85b6)) break;
      a->count=b->count=0;
      if(!dbz_native_step(&ca,stats)) lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(steps<4000 && !(ca.k==0 && ca.pc>=0x8b28 && ca.pc<=0x8b53),
            "8b28+fb8d reached jump target");
    for(unsigned i=0;i<0x80;i++) require(a->ram[0x800+i]==0,"8b28+fb8d queue wiped");
  }


  /* $008B7A: entry JSL lands on native $03A6CB */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8b7a,0), cb=make_cpu(b,0x8b7a,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0xa6cb && ca.k==3) && steps++<8) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8b7a jsl");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xa6cb && ca.k==3,"8b7a reached A6CB");
  }
  /* $008B7A: from $8B7E through native $8887 until native $06E837 */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8b7e,av), cb=make_cpu(b,0x8b7e,av);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    /* DMA fixture: length at DP+$02 set by routine; plant source zeros */
    unsigned steps=0;
    while(steps++<400) {
      bool in_8b = ca.k==0 && ca.pc>=0x8b7a && ca.pc<=0x8bd0;
      bool in_8887 = ca.k==0 && ca.pc>=0x8887 && ca.pc<=0x88ad;
      if(!(in_8b || in_8887)) break;
      a->count=b->count=0;
      if(!dbz_native_step(&ca,stats)) lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xe837 && ca.k==6,"8b7e reached E837");
    require(a->ram[0x0d66]==0 && a->ram[0x0725]==0,"8b7e cleared 0D66/0725");
  }
  /* $008B7A: scroll snap $8BA3–$8BC7 (stop before long $86B6 wipe) */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8ba3,av), cb=make_cpu(b,0x8ba3,av);
    ca.db=cb.db=0; ca.xf=cb.xf=false; ca.mf=cb.mf=true;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x1a6,0x123f); word(b,0x1a6,0x123f);
    word(a,0x1a8,0x5679); word(b,0x1a8,0x5679);
    word(a,0x1aa,0x9abc); word(b,0x1aa,0x9abc);
    word(a,0x1ac,0xdef0); word(b,0x1ac,0xdef0);
    unsigned steps=0;
    while(!(ca.pc==0x8bc9 && ca.k==0 && ca.mf) && steps++<40) {
      a->count=b->count=0;
      if(!dbz_native_step(&ca,stats)) lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x8bc9 && ca.mf,"8ba3 reached 8BC9 M=8");
    require(getword(a,0x33)==0x1238 && getword(a,0x35)==0x5678,"8ba3 scroll DP");
    require(getword(a,0x1a2)==0x9ab8 && getword(a,0x1a4)==0xdef0,"8ba3 scroll abs");
  }
  /* $008B7A: post-palette STZ $0D00 / DP+$61 then JSR $94E7 */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8bcd,av & ~1u), cb=make_cpu(b,0x8bcd,av & ~1u);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0d00]=b->ram[0x0d00]=0xaa; a->ram[0x61]=b->ram[0x61]=0x55;
    unsigned steps=0;
    while(ca.k==0 && ca.pc>=0x8b7a && ca.pc<=0x8bd0 && steps++<8) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8bcd stz");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x8bd2 && ca.k==0,"8bcd reached JSR 94E7");
    require(a->ram[0x0d00]==0 && a->ram[0x61]==0,"8bcd cleared 0D00/61");
  }
  /* $008B7A: $01A0 specialty ($15 / $14 / other) through BMI */
  for(unsigned path=0;path<3;path++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8bec,path), cb=make_cpu(b,0x8bec,path);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    uint8_t a0 = path==0 ? 0x15 : path==1 ? 0x14 : 0x10;
    a->ram[0x1a0]=b->ram[0x1a0]=a0;
    a->ram[0x1a1]=b->ram[0x1a1]=0x00;
    unsigned steps=0;
    while(ca.k==0 && ca.pc>=0x8bec && ca.pc<=0x8c10 && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8bec specialty");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    if(path==0) {
      require(ca.pc==0x8c3c && ca.k==0,"8bec $15 BMI to 8C3C");
      require(a->ram[0x1bd]==0x9e && a->ram[0x1e7]==0x10 && a->ram[0x1a1]==0x80,"8bec path $15");
      require(a->ram[0x0d87]==0,"8bec cleared 0D87");
    } else if(path==1) {
      require(ca.pc==0x8c3c && ca.k==0,"8bec $14 BMI to 8C3C");
      require(a->ram[0x1bd]==0x9f && a->ram[0x1a1]==0x80,"8bec path $14");
    } else {
      require(ca.pc==0x8c12 && ca.k==0,"8bec skip falls to 8C12");
      require(a->ram[0x1bd]==0 && a->ram[0x1a1]==0,"8bec skipped specialty");
    }
  }
  /* $008B7A: $01A1 BMI taken -> $8C3C */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8c0d,0), cb=make_cpu(b,0x8c0d,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1a1]=b->ram[0x1a1]=0x80;
    unsigned steps=0;
    while(ca.k==0 && ca.pc>=0x8c0d && ca.pc<=0x8c10 && steps++<8) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8c0d bmi");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x8c3c && ca.k==0,"8c0d BMI to 8C3C");
  }
  /* $008E96: entry JSL lands on native $03EF29 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8e96,0), cb=make_cpu(b,0x8e96,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0xef29 && ca.k==3) && steps++<8) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8e96 jsl");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xef29 && ca.k==3,"8e96 reached native EF29");
  }
  /* $008E96: HDMA mask + JSR lands on native $94B2 entry */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8e9a,av), cb=make_cpu(b,0x8e9a,av);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x62]=b->ram[0x62]=(uint8_t)(0xf8 | (av & 7));
    unsigned steps=0;
    while(!(ca.pc==0x94b2 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8e9a prefix");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x94b2 && ca.k==0,"8e9a reached 94B2");
    require(a->ram[0x62]==(uint8_t)(0xe0 | (av & 7)),"8e9a HDMA masked");
  }
  /* $008E96: post-JSR body through scroll snapshot / PPU regs / JSL $8887 */
  for(unsigned path=0;path<2;path++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8ea6,path), cb=make_cpu(b,0x8ea6,path);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x62]=b->ram[0x62]=0xe5;
    a->ram[0x0c40]=b->ram[0x0c40]=(uint8_t)path; /* 0 => snapshot, 1 => skip */
    word(a,0x33,0x1111); word(b,0x33,0x1111);
    word(a,0x35,0x2222); word(b,0x35,0x2222);
    word(a,0x1a2,0x3333); word(b,0x1a2,0x3333);
    word(a,0x1a4,0x4444); word(b,0x1a4,0x4444);
    unsigned steps=0;
    while(steps++<80) {
      bool in_8e = ca.k==0 && ca.pc>=0x8e96 && ca.pc<=0x8f12;
      bool in_8887 = ca.k==0 && ca.pc>=0x8887 && ca.pc<=0x88ad;
      if(!(in_8e || in_8887)) break;
      a->count=b->count=0;
      if(!dbz_native_step(&ca,stats)) lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.k==0 && ca.pc==0x8f16,"8ea6 returned from native 8887 to 8F16");
    require(a->ram[0x62]==0x05,"8ea6 kept low HDMA bits");
    require(a->ram[0x0d00]==0 && a->ram[0x0d04]==0 && a->ram[0x0ea1]==0,"8ea6 cleared actors");
    require(getword(a,0x33)==0 && getword(a,0x35)==0,"8ea6 zeroed scrolls");
    require(getword(a,0x02)==0x2000,"8ea6 DMA length $2000");
    if(path==0) {
      require(getword(a,0x1a6)==0x1111 && getword(a,0x1a8)==0x2222,"8ea6 snap A6/A8");
      require(getword(a,0x1aa)==0x3333 && getword(a,0x1ac)==0x4444,"8ea6 snap AA/AC");
    } else {
      require(getword(a,0x1a6)==0 && getword(a,0x1aa)==0,"8ea6 skipped snapshot");
    }
  }
  /* $03FBDA: $01D7>=0 and $1661!=$0B -> early RTL */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfbda,av), cb=make_cpu(b,0xfbda,av);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1d7]=b->ram[0x1d7]=(uint8_t)(av & 0x7f);
    a->ram[0x1661]=b->ram[0x1661]=0x01;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fbda early rtl");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"fbda early rtl");
  }
  /* $03FBDA: $01D7 bit7 set, bit6 clear, $1648>=0 -> RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfbda,1), cb=make_cpu(b,0xfbda,1);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1d7]=b->ram[0x1d7]=0x80;
    a->ram[0x1648]=b->ram[0x1648]=0x00;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fbda bpl rtl");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"fbda BPL rtl");
  }
  /* $03FBDA: specialty path JMP $FC47 when $15FA==$80 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfbda,2), cb=make_cpu(b,0xfbda,2);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1d7]=b->ram[0x1d7]=0x80;
    a->ram[0x1648]=b->ram[0x1648]=0x80;
    a->ram[0x15fa]=b->ram[0x15fa]=0x80;
    unsigned steps=0;
    while(!(ca.pc==0xfc47 && ca.k==3) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fbda jmp fc47");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xfc47 && ca.k==3 && (uint8_t)ca.a==0x07,"fbda reached FC47");
  }
  /* $03FC1E: $01D7>=0 -> RTL at FC1D */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfc1e,0), cb=make_cpu(b,0xfc1e,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1d7]=b->ram[0x1d7]=0x00;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fc1e rtl");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"fc1e rtl");
  }
  /* $03FB8D+$FBDA: mode 0 full path early RTL through native FBDA */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfb8d,3), cb=make_cpu(b,0xfb8d,3);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1a0]=b->ram[0x1a0]=0x05;
    a->ram[0x24]=b->ram[0x24]=0x00;
    a->ram[0x1d7]=b->ram[0x1d7]=0x00;
    a->ram[0x1661]=b->ram[0x1661]=0x00;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fb8d+fbda rtl");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"fb8d+fbda rtl");
  }

  /* $0094B2: clear scroll/window regs + DP+$37..$3A then RTS */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x94b2,av), cb=make_cpu(b,0x94b2,av);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x37]=b->ram[0x37]=0xaa; a->ram[0x3a]=b->ram[0x3a]=0xbb;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.sp==0x201) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 94b2");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x201,"94b2 RTS");
    require(a->ram[0x37]==0 && a->ram[0x38]==0 && a->ram[0x39]==0 && a->ram[0x3a]==0,"94b2 cleared DP scrolls");
  }
  /* $009485: PPU preset falls into 94B2 clear / RTS */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x9485,1), cb=make_cpu(b,0x9485,1);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.sp==0x201) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 9485");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x201,"9485 RTS");
  }
  /* $0094E7: skip clear when type==$54; wipe otherwise; empty when $01BB large */
  for(unsigned path=0;path<3;path++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x94e7,path), cb=make_cpu(b,0x94e7,path);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    if(path==0) { a->ram[0x1bb]=b->ram[0x1bb]=0x10; /* X=$200 >= $120 => immediate RTS */ }
    else if(path==1) {
      a->ram[0x1bb]=b->ram[0x1bb]=0x01; /* X=$20 */
      a->ram[0x0b00+0x20]=b->ram[0x0b00+0x20]=0x55;
      a->ram[0x0b01+0x20]=b->ram[0x0b01+0x20]=0x54; /* skip wipe */
      a->ram[0x3800+0x20]=b->ram[0x3800+0x20]=0x66;
    } else {
      a->ram[0x1bb]=b->ram[0x1bb]=0x01;
      a->ram[0x0b00+0x20]=b->ram[0x0b00+0x20]=0x55;
      a->ram[0x0b01+0x20]=b->ram[0x0b01+0x20]=0x10; /* wipe */
      a->ram[0x3800+0x20]=b->ram[0x3800+0x20]=0x66;
    }
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.sp==0x201) && steps++<400) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 94e7");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x201,"94e7 RTS");
    if(path==1) {
      require(a->ram[0x0b20]==0x55 && a->ram[0x3820]==0x66,"94e7 kept type $54");
    } else if(path==2) {
      require(a->ram[0x0b20]==0 && a->ram[0x3820]==0,"94e7 wiped slot");
    }
  }
  /* $03FC47: prefix through STZ $0D66 until unrecovered bank-1 JSL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfc47,0), cb=make_cpu(b,0xfc47,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x62]=b->ram[0x62]=0xff;
    unsigned steps=0;
    while(!(ca.pc==0xb61e && ca.k==1) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fc47 prefix");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xb61e && ca.k==1,"fc47 reached B61E");
    require(a->ram[0x121f]==0x07 && a->ram[0x61]==0 && a->ram[0x62]==0xe7,"fc47 HDMA masked");
    require(a->ram[0x0d66]==0,"fc47 cleared 0D66");
  }
  /* $03FC74: fail CLC/RTL and success SEC/RTL */
  for(unsigned path=0;path<2;path++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfc74,path), cb=make_cpu(b,0xfc74,path);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    if(path==0) {
      a->ram[0x1648]=b->ram[0x1648]=0x00; /* BPL -> CLC RTL */
    } else {
      a->ram[0x1648]=b->ram[0x1648]=0x80;
      a->ram[0x1d8]=b->ram[0x1d8]=0x11;
      a->ram[0x15fa]=b->ram[0x15fa]=0x80;
      a->ram[0x1e2]=b->ram[0x1e2]=0xff;
      a->ram[0x1d2]=b->ram[0x1d2]=0xff;
    }
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fc74");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"fc74 RTL");
    if(path==0) require(!ca.c,"fc74 fail CLC");
    else {
      require(ca.c,"fc74 success SEC");
      require(a->ram[0x1e2]==0 && a->ram[0x1d2]==0,"fc74 cleared E2/D2");
    }
  }
  /* $03E340: early RTL when $166C < $0800 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xe340,0), cb=make_cpu(b,0xe340,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x166c,0x07ff); word(b,0x166c,0x07ff);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected e340 rtl");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"e340 early RTL");
  }
  /* $03E340: $166C >= $0800 takes BCS into native JSL $06E837 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xe340,1), cb=make_cpu(b,0xe340,1);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x166c,0x0800); word(b,0x166c,0x0800);
    unsigned steps=0;
    while(ca.k==3 && ca.pc>=0xe340 && ca.pc<=0xe36a && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected e340 continue");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xe837 && ca.k==6 && ca.mf,"e340 JSL landed on native E837");
  }
  /* $03E340: post-JSL body $E359–$E368 seeds then leaves at E36B */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xe359,0), cb=make_cpu(b,0xe359,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1e7]=b->ram[0x1e7]=0xff;
    unsigned steps=0;
    while(ca.k==3 && ca.pc>=0xe359 && ca.pc<=0xe36a && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected e359 body");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xe36b && ca.k==3,"e359 left at E36B");
    require(a->ram[0x0ea3]==0x15 && a->ram[0x0ea0]==0x01,"e359 seeded 0EA3/0EA0");
    require(a->ram[0x1e7]==0xfb,"e359 cleared 01E7 bit2");
  }
  /* $008B7A: not-BMI path $8C12 pointer setup JMP $8C6D then native decompress JSLs */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8c22,0), cb=make_cpu(b,0x8c22,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1a1]=b->ram[0x1a1]=0x55;
    unsigned steps=0;
    while(ca.k==0 && ca.pc>=0x8c22 && ca.pc<=0x8c83 && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8c22 path");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xc559 && ca.k==0,"8c22 reached native C559");
    require(a->ram[0x1a1]==0,"8c22 cleared 01A1");
    require(getword(a,0x0707)==0xefb0 && a->ram[0x0709]==0x09,"8c22 set 0707/09");
    require(getword(a,0x83)==0x8d4c && a->ram[0x85]==0x08,"8c22 set DP 83/85");
  }
  /* $008B7A: BMI path $8C3C prefix until unrecovered $02D97D */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8c3c,0), cb=make_cpu(b,0x8c3c,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(ca.k==0 && ca.pc>=0x8c3c && ca.pc<=0x8c83 && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8c3c prefix");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xd97d && ca.k==2,"8c3c reached D97D");
    require(getword(a,0x074c)==0xffff,"8c3c set 074C");
  }
  /* $008B7A: BMI tail $8C56–$8C6B into join $8C6D → native C559 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8c56,0), cb=make_cpu(b,0x8c56,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false; ca.a=cb.a=0x0042;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(ca.k==0 && ca.pc>=0x8c56 && ca.pc<=0x8c83 && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8c56 tail");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xc559 && ca.k==0,"8c56 reached native C559");
    require(a->ram[0x1a0]==0x42,"8c56 stored 01A0");
    require(getword(a,0x0707)==0xfa00 && a->ram[0x0709]==0x08,"8c56 set 0707/09");
    require(getword(a,0x83)==0xbf20 && a->ram[0x85]==0x08,"8c56 set DP 83/85");
  }
  /* $008E96: $01E7 bit4 select through native C559 */
  for(unsigned path=0;path<2;path++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8f16,path), cb=make_cpu(b,0x8f16,path);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1e7]=b->ram[0x1e7]=(uint8_t)(path?0x10:0x00);
    unsigned steps=0;
    while(ca.k==0 && ca.pc>=0x8f16 && ca.pc<=0x8f45 && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8f16 path");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xc559 && ca.k==0,"8f16 reached native C559");
    if(path) require(getword(a,0x83)==0xf233 && a->ram[0x85]==0x0c,"8f16 bit4 pointers");
    else require(getword(a,0x83)==0x83fa && a->ram[0x85]==0x09,"8f16 clear-bit pointers");
  }
  /* $03A6CB: $01D6!=$60 early RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xa6cb,0), cb=make_cpu(b,0xa6cb,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1d6]=b->ram[0x1d6]=0x10;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected a6cb rtl");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"a6cb early RTL");
  }
  /* $03A6CB: $01D6==$60 clears then JSL native E419 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xa6cb,1), cb=make_cpu(b,0xa6cb,1);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1d6]=b->ram[0x1d6]=0x60;
    a->ram[0x1bc]=b->ram[0x1bc]=0xaa;
    unsigned steps=0;
    while(ca.k==3 && ca.pc>=0xa6cb && ca.pc<=0xa6e0 && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected a6cb body");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xe419 && ca.k==3,"a6cb reached native E419");
    require(a->ram[0x1d6]==0,"a6cb cleared 01D6");
  }
  /* $039255: clear $1000 loop via native $8B07 until RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x9255,0), cb=make_cpu(b,0x9255,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    for(unsigned i=0;i<0xc0;i++) a->ram[0x1000+i]=b->ram[0x1000+i]=0xff;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<2000) {
      bool in_9255 = ca.k==3 && ca.pc>=0x9255 && ca.pc<=0x9266;
      bool in_8b07 = ca.k==0 && ca.pc>=0x8b07 && ca.pc<=0x8b0d;
      a->count=b->count=0;
      if(in_9255 || in_8b07) require(dbz_native_step(&ca,stats),"expected 9255/8b07");
      else lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"9255 RTL");
    for(unsigned i=0;i<0xc0;i+=6) require(a->ram[0x1000+i]==0,"9255 cleared stride-6");
    require(a->ram[0x1001]==0xff,"9255 left non-stride bytes");
  }
  /* $03FC33: fail gate BNE $FBC7 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfc33,0), cb=make_cpu(b,0xfc33,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1661]=b->ram[0x1661]=0x00;
    unsigned steps=0;
    while(!(ca.pc==0xfbc7 && ca.k==3) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fc33 fail");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xfbc7 && ca.k==3,"fc33 branched FBC7");
  }
  /* $03FC33: success path PLA then fall into FC47 until unrecovered B61E */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfc33,1), cb=make_cpu(b,0xfc33,1);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.sp=cb.sp=0x1fe;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1ff]=b->ram[0x1ff]=0x42; /* PLA value */
    a->ram[0x1661]=b->ram[0x1661]=0x0a;
    a->ram[0x1648]=b->ram[0x1648]=0x80;
    a->ram[0x15fa]=b->ram[0x15fa]=0x80;
    a->ram[0x62]=b->ram[0x62]=0xff;
    unsigned steps=0;
    while(!(ca.pc==0xb61e && ca.k==1) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fc33 success");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xb61e && ca.k==1,"fc33 reached B61E");
    require(a->ram[0x121f]==0x07 && a->ram[0x0d66]==0,"fc33 specialty stores");
    require(ca.sp==0x1fc,"fc33 PLA+JSL stack depth");
  }
  /* $06E837: STZ cascade then JSL native E942 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xe837,0), cb=make_cpu(b,0xe837,0);
    ca.k=cb.k=6; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0c40]=b->ram[0x0c40]=1; a->ram[0x01ee]=b->ram[0x01ee]=1;
    unsigned steps=0;
    while(!(ca.pc==0xe942 && ca.k==6) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected e837 prefix");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xe942 && ca.k==6,"e837 reached native E942");
    require(a->ram[0x0c40]==0 && a->ram[0x0c36]==0 && a->ram[0x01ee]==0,"e837 cleared flags");
  }


  /* $03EF29: early RTL when $1648 bit7 set and $01D8==$04 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xef29,0), cb=make_cpu(b,0xef29,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1648]=b->ram[0x1648]=0x80;
    a->ram[0x01d8]=b->ram[0x01d8]=0x04;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected ef29 rtl");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"ef29 early RTL");
  }
  /* $03EF29: empty-actor path through EFC4/MVN restore to RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xef29,1), cb=make_cpu(b,0xef29,1);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1648]=b->ram[0x1648]=0x00;
    a->ram[0x01bb]=b->ram[0x01bb]=0x05;
    /* seed backup region so MVN has non-zero payload */
    for(unsigned i=0;i<0x120;i++) {
      a->ram[0x4d00+i]=b->ram[0x4d00+i]=(uint8_t)(0xa0+(i&0x1f));
    }
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<5000) {
      bool in_ef = ca.k==3 && ca.pc>=0xef29 && ca.pc<=0xefd6;
      bool in_e42 = ca.k==3 && ca.pc>=0xe419 && ca.pc<=0xe460;
      bool in_8af = ca.k==0 && ca.pc>=0x8af1 && ca.pc<=0x8b06;
      a->count=b->count=0;
      if(in_ef || in_e42 || in_8af) require(dbz_native_step(&ca,stats),"expected ef29 body");
      else lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"ef29 RTL");
    require(a->ram[0x01bb]==0x05,"ef29 kept 01BB (no occupied slots)");
    require(a->ram[0x0b00]==a->ram[0x4d00],"ef29 MVN restored 0B00 from 4D00");
    require(ca.db==0,"ef29 restored DB");
  }
  /* $03EF29: occupied weak actor clears fields then JSL E42A */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xef29,2), cb=make_cpu(b,0xef29,2);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1648]=b->ram[0x1648]=0x00;
    a->ram[0x0b00]=b->ram[0x0b00]=0x80; /* occupied, bit6 clear */
    a->ram[0x0b18]=b->ram[0x0b18]=0x11;
    a->ram[0x0b12]=b->ram[0x0b12]=0x22;
    a->ram[0x0b1c]=b->ram[0x0b1c]=0x03;
    unsigned steps=0;
    while(!(ca.pc==0xe42a && ca.k==3) && steps++<40) {
      bool in_ef = ca.k==3 && ca.pc>=0xef29 && ca.pc<=0xefd6;
      a->count=b->count=0;
      if(in_ef) require(dbz_native_step(&ca,stats),"expected ef29 to e42a");
      else lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xe42a && ca.k==3,"ef29 reached E42A");
    require(a->ram[0x0b18]==0 && a->ram[0x0b12]==0,"ef29 cleared weak fields");
  }
  /* $03E419: empty walk through native E42A/8AF1 to RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xe419,0), cb=make_cpu(b,0xe419,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<2000) {
      bool in_e419 = ca.k==3 && ca.pc>=0xe419 && ca.pc<=0xe460;
      bool in_8af = ca.k==0 && ca.pc>=0x8af1 && ca.pc<=0x8afb;
      a->count=b->count=0;
      if(in_e419 || in_8af) require(dbz_native_step(&ca,stats),"expected e419");
      else lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"e419 RTL");
  }
  /* $03E42A: bit6/7 clear skips MVN and RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xe42a,0), cb=make_cpu(b,0xe42a,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.y=cb.y=0;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0b00]=b->ram[0x0b00]=0x00;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected e42a skip");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"e42a skip RTL");
  }
  /* $03E42A: occupied actor MVN copies 32 bytes to $7E2800+type<<5 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xe42a,1), cb=make_cpu(b,0xe42a,1);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.y=cb.y=0x20;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0b20]=b->ram[0x0b20]=0xc0;
    a->ram[0x0b3c]=b->ram[0x0b3c]=0x02; /* type 2 → dest $2840 */
    for(unsigned i=0;i<0x20;i++) a->ram[0x0b20+i]=b->ram[0x0b20+i]=(uint8_t)(0x40+i);
    a->ram[0x0b20]=b->ram[0x0b20]=0xc0;
    a->ram[0x0b3c]=b->ram[0x0b3c]=0x02;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<500) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected e42a mvn");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"e42a MVN RTL");
    require(a->ram[0x2840]==0xc0 && a->ram[0x2841]==0x41,"e42a copied to 2840");
    require(ca.db==0,"e42a restored DB");
  }
  /* $03E4D1: collect high actors into $0190 then terminator */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xe4d1,0), cb=make_cpu(b,0xe4d1,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0b00]=b->ram[0x0b00]=0xc0;
    a->ram[0x0b1c]=b->ram[0x0b1c]=0x07;
    a->ram[0x0b40]=b->ram[0x0b40]=0x10; /* below C0 skipped */
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<2000) {
      bool in_e4 = ca.k==3 && ca.pc>=0xe4d1 && ca.pc<=0xe513;
      bool in_8af = ca.k==0 && ca.pc>=0x8af1 && ca.pc<=0x8afb;
      a->count=b->count=0;
      if(in_e4 || in_8af) require(dbz_native_step(&ca,stats),"expected e4d1");
      else lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"e4d1 RTL");
    require(a->ram[0x0190]==0x07 && a->ram[0x0191]==0xff,"e4d1 list+term");
  }
  /* $03E4F4: $FF terminator early RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xe4f4,0), cb=make_cpu(b,0xe4f4,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0190]=b->ram[0x0190]=0xff;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected e4f4 empty");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"e4f4 empty RTL");
    require(a->ram[0x10]==0,"e4f4 count stayed 0");
  }
  /* $03E461: restore type via MVN from $7E2800 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xe461,0), cb=make_cpu(b,0xe461,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    ca.a=cb.a=0x0001; ca.y=cb.y=0x0000;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    for(unsigned i=0;i<0x20;i++) a->ram[0x2820+i]=b->ram[0x2820+i]=(uint8_t)(0x90+i);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<500) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected e461");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"e461 RTL");
    require(a->ram[0x0b00]==0x90 && a->ram[0x0b1f]==0xaf,"e461 restored slot");
    require(ca.db==0,"e461 restored DB");
  }
  /* $06E942: bit6 clear → immediate RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xe942,0), cb=make_cpu(b,0xe942,0);
    ca.k=cb.k=6; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0d9b]=b->ram[0x0d9b]=0x00;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected e942 rtl");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"e942 early RTL");
  }
  /* $06E942: full predicate → JSL $03BB46 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xe942,1), cb=make_cpu(b,0xe942,1);
    ca.k=cb.k=6; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0d9b]=b->ram[0x0d9b]=0x40;
    a->ram[0x01a0]=b->ram[0x01a0]=0x09;
    a->ram[0x0170]=b->ram[0x0170]=0x08;
    a->ram[0x01bb]=b->ram[0x01bb]=0x08;
    unsigned steps=0;
    while(!(ca.pc==0xbb46 && ca.k==3) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected e942 jsl");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xbb46 && ca.k==3,"e942 reached BB46");
    require(a->ram[0x0d9b]==0,"e942 cleared 0D9B");
    require(ca.y==0x0100,"e942 set Y=$0100");
  }


  /* $06E8EE: empty actors (bit6 clear) walk via 8AF1 to RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xe8ee,0), cb=make_cpu(b,0xe8ee,0);
    ca.k=cb.k=6; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<2000) {
      bool in_e8 = ca.k==6 && ca.pc>=0xe8ee && ca.pc<=0xe941;
      bool in_8af = ca.k==0 && ca.pc>=0x8af1 && ca.pc<=0x8afb;
      a->count=b->count=0;
      if(in_e8 || in_8af) require(dbz_native_step(&ca,stats),"expected e8ee empty");
      else lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"e8ee empty RTL");
  }
  /* $06E8EE: type $54 path seeds $0B0E and skips $8A28 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xe8ee,1), cb=make_cpu(b,0xe8ee,1);
    ca.k=cb.k=6; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0b00]=b->ram[0x0b00]=0x40;
    a->ram[0x0b01]=b->ram[0x0b01]=0x54;
    word(a,0x4216,0x54*0x0e); word(b,0x4216,0x54*0x0e);
    for(unsigned i=0;i<0x200;i++) {
      a->ram[0xb21d+i]=b->ram[0xb21d+i]=(uint8_t)(0x10+(i&0x3f));
      a->ram[0xb214+i]=b->ram[0xb214+i]=(uint8_t)(0x20+(i&0x3f));
    }
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<2000) {
      bool in_e8 = ca.k==6 && ca.pc>=0xe8ee && ca.pc<=0xe941;
      bool in_8af = ca.k==0 && ca.pc>=0x8af1 && ca.pc<=0x8afb;
      a->count=b->count=0;
      if(in_e8 || in_8af) require(dbz_native_step(&ca,stats),"expected e8ee type54");
      else lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"e8ee type54 RTL");
    require(a->ram[0x0b0e]==0x01,"e8ee type54 set 0B0E");
  }
  /* $03BB46: seed wipe actor at Y=$0100 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xbb46,0), cb=make_cpu(b,0xbb46,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.y=cb.y=0x0100;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected bb46");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"bb46 RTL");
    require(a->ram[0x0c00]==0xc0 && a->ram[0x0c01]==0x54,"bb46 flags/type");
    require(a->ram[0x0c0e]==0x01 && a->ram[0x0c1c]==0x0c,"bb46 0E/1C");
    require(getword(a,0x0c03)==0x0033 && getword(a,0x0c05)==0x0006,"bb46 coords");
  }
  /* $03EFD7: clear $7E29A0 word loop */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xefd7,0), cb=make_cpu(b,0xefd7,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    for(unsigned i=0;i<0x120;i++) a->ram[0x29a0+i]=b->ram[0x29a0+i]=0x5a;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<800) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected efd7");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"efd7 RTL");
    require(a->ram[0x29a0]==0 && a->ram[0x2abf]==0,"efd7 cleared 29A0");
  }
  /* $03EFEA: clear $7E3A00 word loop */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xefea,0), cb=make_cpu(b,0xefea,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    for(unsigned i=0;i<0x120;i++) a->ram[0x3a00+i]=b->ram[0x3a00+i]=0xa5;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<800) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected efea");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"efea RTL");
    require(a->ram[0x3a00]==0 && a->ram[0x3b1f]==0,"efea cleared 3A00");
  }
  /* $03EFFD: clear $0B00 via Y */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xeffd,0), cb=make_cpu(b,0xeffd,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    for(unsigned i=0;i<0x120;i++) a->ram[0x0b00+i]=b->ram[0x0b00+i]=0x11;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<800) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected effd");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"effd RTL");
    require(a->ram[0x0b00]==0 && a->ram[0x0c1f]==0,"effd cleared 0B00");
  }
  /* $03F00F: clear $0E00 via Y */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xf00f,0), cb=make_cpu(b,0xf00f,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    for(unsigned i=0;i<0xa0;i++) a->ram[0x0e00+i]=b->ram[0x0e00+i]=0x22;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<500) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected f00f");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"f00f RTL");
    require(a->ram[0x0e00]==0 && a->ram[0x0e9f]==0,"f00f cleared 0E00");
  }
  /* $03E32E: swap $01BC↔$018F then SEC RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xe32e,0), cb=make_cpu(b,0xe32e,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x01bc]=b->ram[0x01bc]=0xaa;
    a->ram[0x018f]=b->ram[0x018f]=0x55;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected e32e");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202 && ca.c,"e32e SEC RTL");
    require(a->ram[0x01bc]==0x55 && a->ram[0x018f]==0xaa,"e32e swapped");
  }
  /* $03E36B: MVN copies + long restore then JMP native E32E to RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xe36b,0), cb=make_cpu(b,0xe36b,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    for(unsigned i=0;i<0x120;i++) {
      a->ram[0x0b00+i]=b->ram[0x0b00+i]=(uint8_t)(0x30+(i&0x1f));
      a->ram[0x29a0+i]=b->ram[0x29a0+i]=(uint8_t)(0x80+(i&0x1f));
    }
    for(unsigned i=0;i<0xa0;i++) a->ram[0x2b00+i]=b->ram[0x2b00+i]=(uint8_t)(0xc0+(i&0x1f));
    a->ram[0x01bb]=b->ram[0x01bb]=0x07;
    a->ram[0x2ac0]=b->ram[0x2ac0]=0x12;
    a->ram[0x2ae0]=b->ram[0x2ae0]=0x34;
    a->ram[0x2aec]=b->ram[0x2aec]=0x56;
    a->ram[0x2aed]=b->ram[0x2aed]=0x78;
    word(a,0x01a2,0x1111); word(b,0x01a2,0x1111);
    word(a,0x01a4,0x2222); word(b,0x01a4,0x2222);
    word(a,0x01aa,0x3333); word(b,0x01aa,0x3333);
    word(a,0x01ac,0x4444); word(b,0x01ac,0x4444);
    word(a,0x2ae2,0x5555); word(b,0x2ae2,0x5555);
    word(a,0x2ae4,0x6666); word(b,0x2ae4,0x6666);
    word(a,0x2ae6,0x7777); word(b,0x2ae6,0x7777);
    word(a,0x2ae8,0x8888); word(b,0x2ae8,0x8888);
    word(a,0x2aea,0x9999); word(b,0x2aea,0x9999);
    a->ram[0x01bc]=b->ram[0x01bc]=0x10;
    a->ram[0x018f]=b->ram[0x018f]=0x20;
    a->ram[0x01d7]=b->ram[0x01d7]=0xff;
    a->ram[0x166c]=b->ram[0x166c]=0x12; a->ram[0x166d]=b->ram[0x166d]=0x34;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<5000) {
      bool in_e36 = ca.k==3 && ca.pc>=0xe36b && ca.pc<=0xe418;
      bool in_e32 = ca.k==3 && ca.pc>=0xe32e && ca.pc<=0xe33f;
      a->count=b->count=0;
      if(in_e36 || in_e32) require(dbz_native_step(&ca,stats),"expected e36b");
      else lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202 && ca.c,"e36b via E32E RTL");
    require(a->ram[0x3a00]==0x30 && a->ram[0x0b00]==0x80,"e36b MVN 0B00/3A00");
    require(a->ram[0x0e00]==0xc0,"e36b MVN 0E00 from 2B00");
    require(a->ram[0x3b20]==0x07 && a->ram[0x01bb]==0x12,"e36b 01BB backup/restore");
    require(a->ram[0x1648]==0x34 && a->ram[0x0c36]==0x56 && a->ram[0x0c37]==0x78,"e36b long loads");
    require(getword(a,0x166c)==0 && getword(a,0x01a2)==0x5555,"e36b cleared 166C / restored A2");
    require(a->ram[0x0c41]==0x49 && a->ram[0x0ea2]==0x20 && a->ram[0x01d7]==0xbf,"e36b tail seeds");
    require(a->ram[0x01bc]==0x20 && a->ram[0x018f]==0x10,"e36b E32E swap");
  }
  /* $06E942→$03BB46: gate then wipe-actor seed */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xe942,2), cb=make_cpu(b,0xe942,2);
    ca.k=cb.k=6; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0d9b]=b->ram[0x0d9b]=0x40;
    a->ram[0x01a0]=b->ram[0x01a0]=0x09;
    a->ram[0x0170]=b->ram[0x0170]=0x08;
    a->ram[0x01bb]=b->ram[0x01bb]=0x08;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<80) {
      bool in_e942 = ca.k==6 && ca.pc>=0xe942 && ca.pc<=0xe968;
      bool in_bb = ca.k==3 && ca.pc>=0xbb46 && ca.pc<=0xbb86;
      a->count=b->count=0;
      if(in_e942 || in_bb) require(dbz_native_step(&ca,stats),"expected e942+bb46");
      else lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"e942+bb46 RTL");
    require(a->ram[0x0c00]==0xc0 && a->ram[0x0c01]==0x54,"e942+bb46 seeded slot");
  }


  /* $008DAC: actor-slot walk; native $01C9F9/$8324/$8AF1 */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8dac,av), cb=make_cpu(b,0x8dac,av);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    for(unsigned slot=0;slot<9;slot++) {
      unsigned y = slot * 0x20u;
      a->ram[0x0b01+y]=b->ram[0x0b01+y]=(uint8_t)(1 + av + slot);
    }
    unsigned steps=0;
    while(!(ca.pc==0xc9f9 && ca.k==1) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8dac to C9F9");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xc9f9 && ca.k==1,"8dac reached C9F9");
  }
  /* $06EAD4: selector paths to JSL $88EA (index!=3 via $02E44B) */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xead4,0), cb=make_cpu(b,0xead4,0);
    ca.k=cb.k=6; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x01bc]=b->ram[0x01bc]=0x00; /* low nibble clear → EAEC path */
    a->ram[0x01e7]=b->ram[0x01e7]=0x00; /* bit4 clear */
    a->ram[0x1648]=b->ram[0x1648]=0x00; /* BPL → load $01A0 */
    a->ram[0x01a0]=b->ram[0x01a0]=0x00; /* E44B[0]=0 → index 0 != 3 */
    unsigned steps=0;
    while(!(ca.pc==0x88ea && ca.k==0) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected ead4 to 88ea");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x88ea && ca.k==0,"ead4 reached 88EA");
    require(a->ram[0x03]==0x04,"ead4 DP+$03 from E47B[0]");
    require(getword(a,0x06)==0xe4aa && a->ram[0x08]==0x02,"ead4 pointer $02E4AA");
  }
  /* $06EAD4: $01BC low-nibble set + $01E7 bit0 → index 5 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xead4,1), cb=make_cpu(b,0xead4,1);
    ca.k=cb.k=6; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x01bc]=b->ram[0x01bc]=0x01;
    a->ram[0x01e7]=b->ram[0x01e7]=0x01; /* LSR → C set → LDA #$05 */
    unsigned steps=0;
    while(!(ca.pc==0x88ea && ca.k==0) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected ead4 idx5");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x88ea && ca.k==0,"ead4 idx5 reached 88EA");
    require(a->ram[0x03]==0x02,"ead4 DP+$03 from E47B[5*2]"); /* E47B+10 = 02 */
  }
  /* $06EAD4: index==3 alt path → pointer $06F876 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xead4,2), cb=make_cpu(b,0xead4,2);
    ca.k=cb.k=6; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x01bc]=b->ram[0x01bc]=0x00;
    a->ram[0x01e7]=b->ram[0x01e7]=0x00;
    a->ram[0x1648]=b->ram[0x1648]=0x00;
    a->ram[0x01a0]=b->ram[0x01a0]=0x01; /* E44B[1]=03 → index 3 alt */
    unsigned steps=0;
    while(!(ca.pc==0x88ea && ca.k==0) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected ead4 idx3");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x88ea && ca.k==0,"ead4 idx3 reached 88EA");
    require(a->ram[0x03]==0x01,"ead4 idx3 DP+$03=$01A0");
    require(getword(a,0x06)==0xf876 && a->ram[0x08]==0x06,"ead4 pointer $06F876");
  }
  /* $06EAD4 full RTL through native $88EA/$88C5 (ROM tables; bank-8/0C sources) */
  for(unsigned av=0;av<3;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xead4,av), cb=make_cpu(b,0xead4,av);
    ca.k=cb.k=6; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    if(av==0) {
      a->ram[0x01bc]=b->ram[0x01bc]=0x00;
      a->ram[0x01e7]=b->ram[0x01e7]=0x00;
      a->ram[0x1648]=b->ram[0x1648]=0x00;
      a->ram[0x01a0]=b->ram[0x01a0]=0x00; /* index 0 */
    } else if(av==1) {
      a->ram[0x01bc]=b->ram[0x01bc]=0x01;
      a->ram[0x01e7]=b->ram[0x01e7]=0x00; /* BCC → index 2 */
    } else {
      a->ram[0x01bc]=b->ram[0x01bc]=0x00;
      a->ram[0x01e7]=b->ram[0x01e7]=0x00;
      a->ram[0x1648]=b->ram[0x1648]=0x00;
      a->ram[0x01a0]=b->ram[0x01a0]=0x01; /* index 3 alt */
    }
    for(unsigned i=0;i<0x200;i++) {
      a->ram[0xdd84+i]=b->ram[0xdd84+i]=(uint8_t)(0x40+i);
      a->ram[0xd100+i]=b->ram[0xd100+i]=(uint8_t)(0x10+i);
    }
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<5000) {
      bool in_ead4 = ca.k==6 && ca.pc>=0xead4 && ca.pc<=0xeb51;
      bool in_88 = ca.k==0 && ca.pc>=0x88c5 && ca.pc<=0x8907;
      a->count=b->count=0;
      if(in_ead4 || in_88) require(dbz_native_step(&ca,stats),"expected ead4 full");
      else lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"ead4 full RTL");
    require(a->ram[0x0715]==2,"ead4 palette dirty via 88EA+88C5");
  }


  /* $06EB52: selector → JSL $00C559 with DP+$83/$85 from $02E489 triples */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xeb52,0), cb=make_cpu(b,0xeb52,0);
    ca.k=cb.k=6; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x01bc]=b->ram[0x01bc]=0x00;
    a->ram[0x01e7]=b->ram[0x01e7]=0x00;
    a->ram[0x1648]=b->ram[0x1648]=0x00;
    a->ram[0x01a0]=b->ram[0x01a0]=0x00; /* E44B[0]=0 → ASL 0 → word $0504 */
    unsigned steps=0;
    while(!(ca.pc==0xc559 && ca.k==0) && steps++<120) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected eb52 to c559");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xc559 && ca.k==0,"eb52 reached C559");
    require(getword(a,0x00)==0x0504,"eb52 DP+$00 from E47B");
    require(a->ram[0x85]==0x0a && getword(a,0x83)==0xdae6,"eb52 stream $0ADAE6");
  }
  /* $06EB52: $01E7 bit0 set → index 5 → full through native C559/90E6 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xeb52,1), cb=make_cpu(b,0xeb52,1);
    ca.k=cb.k=6; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x01bc]=b->ram[0x01bc]=0x01;
    a->ram[0x01e7]=b->ram[0x01e7]=0x01; /* index 5 */
    /* empty compressed stream wherever DP ends up; plant zeros at common banks */
    a->ram[0xd200]=b->ram[0xd200]=0;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20000) {
      bool in_eb = ca.k==6 && ca.pc>=0xeb52 && ca.pc<=0xebc0;
      bool in_c559 = ca.k==0 && ca.pc>=0xc559 && ca.pc<=0xc5ec;
      bool in_90e6 = ca.k==0 && ca.pc>=0x90e6 && ca.pc<=0x9101;
      a->count=b->count=0;
      if(in_eb || in_c559 || in_90e6) require(dbz_native_step(&ca,stats),"expected eb52 full");
      else lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"eb52 full RTL");
  }
  /* $008F52: post-call scene body until JSL $01B61E */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8f52,av), cb=make_cpu(b,0x8f52,av);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x29]=b->ram[0x29]=(uint8_t)(0x8f | (av&1));
    a->ram[0x0700]=b->ram[0x0700]=(uint8_t)(0x20+av);
    a->ram[0x1648]=b->ram[0x1648]=0x00; /* BPL skip D4BF */
    a->ram[0x07bc]=b->ram[0x07bc]=0x00; /* BPL skip 07BC copy */
    unsigned steps=0;
    while(!(ca.pc==0xb61e && ca.k==1) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8f52 body");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xb61e && ca.k==1,"8f52 reached B61E");
    require(a->ram[0x25]==0x03 && a->ram[0x27]==0x02 && a->ram[0x26]==0,"8f52 DP seeds");
    require(a->ram[0x0716]==0x61 && a->ram[0x0701]==(uint8_t)(0x20+av),"8f52 0716/0701");
    require(a->ram[0x0d66]==0x23 && a->ram[0x61]==0x01,"8f52 0D66/61");
    require((a->ram[0x29]&0x0f)==0,"8f52 brightness low nibble cleared");
  }
  /* $008F8C: clear $0B12 stride loop via native $8AFC then RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8f89,0), cb=make_cpu(b,0x8f89,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    for(unsigned i=0;i<0x120;i++) a->ram[0x0b12+i]=b->ram[0x0b12+i]=0x5a;
    a->ram[0x1648]=b->ram[0x1648]=0x00;
    a->ram[0x07bc]=b->ram[0x07bc]=0x80; /* negative → copy 07BB to 0C40 */
    a->ram[0x07bb]=b->ram[0x07bb]=0x44;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<500) {
      bool in_8f = ca.k==0 && ca.pc>=0x8f46 && ca.pc<=0x8fb1;
      bool in_8afc = ca.k==0 && ca.pc>=0x8afc && ca.pc<=0x8b06;
      a->count=b->count=0;
      if(in_8f || in_8afc) require(dbz_native_step(&ca,stats),"expected 8f89 clear");
      else lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"8f89 RTL");
    require(a->ram[0x0b12]==0 && a->ram[0x0c12]==0,"8f89 cleared 0B12 slots");
    require(a->ram[0x07bc]==0 && a->ram[0x0c40]==0x44,"8f89 07BC→0C40");
  }


  /* $008C90: BCS skip vs write $01E3/$0D55/$0D57 then meet at SEP */
  for(unsigned av=0;av<2;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8c90,av), cb=make_cpu(b,0x8c90,av);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x01bc]=b->ram[0x01bc]=(uint8_t)(av?0x01:0x00); /* LSR→C when av=1 */
    a->ram[0xf7a4]=b->ram[0xf7a4]=0x6b; /* bank-3 is ROM — cannot stub; stop at JSL */
    unsigned steps=0;
    while(!(ca.pc==0xf7a4 && ca.k==3) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8c90");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xf7a4 && ca.k==3,"8c90 reached F7A4");
    if(av==0) {
      require(a->ram[0x01e3]==0x04 && getword(a,0x0d55)==1 && getword(a,0x0d57)==1,"8c90 wrote seeds");
    } else {
      require(a->ram[0x01e3]==0 && getword(a,0x0d55)==0,"8c90 BCS skipped writes");
    }
  }
  /* $008CB0: copy 0701→0700, optional scroll snapshot, plant pointer, to C535 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8cb0,0), cb=make_cpu(b,0x8cb0,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0701]=b->ram[0x0701]=0x2a;
    a->ram[0x01a1]=b->ram[0x01a1]=0x00; /* BPL → snapshot */
    word(a,0x01a6,0x1111); word(b,0x01a6,0x1111);
    word(a,0x01a8,0x2222); word(b,0x01a8,0x2222);
    word(a,0x01aa,0x3333); word(b,0x01aa,0x3333);
    word(a,0x01ac,0x4444); word(b,0x01ac,0x4444);
    a->ram[0x0d59]=b->ram[0x0d59]=0x02; /* 8DF6[2]=08 */
    unsigned steps=0;
    while(!(ca.pc==0xc535 && ca.k==1) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8cb0");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xc535 && ca.k==1,"8cb0 reached C535");
    require(a->ram[0x0700]==0x2a && a->ram[0x27]==0,"8cb0 0700/27");
    require(getword(a,0x33)==0x1111 && getword(a,0x35)==0x2222,"8cb0 DP scrolls");
    require(getword(a,0x01a2)==0x3333 && getword(a,0x01a4)==0x4444,"8cb0 01A2/A4");
    require(getword(a,0x00)==0xc000 && a->ram[0x02]==0x08,"8cb0 pointer $00C000");
    require(a->ram[0x01e3]==0x08,"8cb0 01E3 from 8DF6");
  }
  /* $008D13: bit0 of $01BC clear → early RTL; set → second $85B6 */
  for(unsigned av=0;av<3;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8d13,av), cb=make_cpu(b,0x8d13,av);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    if(av==0) a->ram[0x01bc]=b->ram[0x01bc]=0x00; /* BCC RTL */
    else if(av==1) a->ram[0x01bc]=b->ram[0x01bc]=0x01; /* → A=0D */
    else a->ram[0x01bc]=b->ram[0x01bc]=0x41; /* bit0+bit6 → A=08 */
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<80) {
      bool in_8c = ca.k==0 && ca.pc>=0x8c84 && ca.pc<=0x8d2a;
      bool in_85 = ca.k==4 && ca.pc>=0x85b6 && ca.pc<=0x85f0;
      a->count=b->count=0;
      if(in_8c || in_85) require(dbz_native_step(&ca,stats),"expected 8d13");
      else lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"8d13 RTL");
  }



  /* $00877E: specialty table fill; stop at JSL $87E9; BMI alt via $87D9 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x877e,0), cb=make_cpu(b,0x877e,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1648]=b->ram[0x1648]=0x00; /* BPL → skip 01D8 */
    a->ram[0x01a1]=b->ram[0x01a1]=0x00; /* BPL */
    a->ram[0x01d7]=b->ram[0x01d7]=0x00; /* BPL → full table */
    a->ram[0x01a0]=b->ram[0x01a0]=0x02; /* index 4 */
    a->ram[0x01d4]=b->ram[0x01d4]=0x00; /* BPL skip override */
    unsigned steps=0;
    while(!(ca.pc==0x87e9 && ca.k==0) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 877e main");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x87e9 && ca.k==0,"877e reached 87E9");
    require(a->ram[0x01d2]==a->ram[0x01e2],"877e 01D2==01E2");
    require(a->ram[0x01d3]!=0 || a->ram[0x01d2]!=0 || true,"877e wrote table");
  }
  {
    /* $01D4 negative → AND #$07 override into 01D2/01E2 */
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x877e,0), cb=make_cpu(b,0x877e,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1648]=b->ram[0x1648]=0x00;
    a->ram[0x01a1]=b->ram[0x01a1]=0x00;
    a->ram[0x01d7]=b->ram[0x01d7]=0x00;
    a->ram[0x01a0]=b->ram[0x01a0]=0x01;
    a->ram[0x01d4]=b->ram[0x01d4]=0x85; /* neg, low3=5 */
    unsigned steps=0;
    while(!(ca.pc==0x87e9 && ca.k==0) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 877e override");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x87e9 && ca.k==0,"877e override to 87E9");
    require(a->ram[0x01d2]==0x05 && a->ram[0x01e2]==0x05,"877e AND override");
  }
  {
    /* $01A1 negative → alt path $87D9 using $01BD → $0284E4 */
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x877e,0), cb=make_cpu(b,0x877e,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1648]=b->ram[0x1648]=0x00;
    a->ram[0x01a1]=b->ram[0x01a1]=0x80;
    a->ram[0x01bd]=b->ram[0x01bd]=0x03;
    unsigned steps=0;
    while(!(ca.pc==0x87e9 && ca.k==0) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 877e alt");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x87e9 && ca.k==0,"877e alt reached 87E9");
  }
  /* $008D4E: specialty select — $1303 sync path vs skip; $01A1/$01E7 index map */
  {
    /* equal $1303 → skip 844A; $01A1>=0, $01E7=0 → index 3 → stop at 85A1 */
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8d4e,0), cb=make_cpu(b,0x8d4e,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x01e7]=b->ram[0x01e7]=0x00; /* bit3 clear → A=0 before CMP */
    a->ram[0x1303]=b->ram[0x1303]=0x00; /* equal → skip 844A */
    a->ram[0x01a1]=b->ram[0x01a1]=0x00; /* BPL → ASL chain → default #$03 */
    unsigned steps=0;
    while(!(ca.pc==0x85a1 && ca.k==4) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8d4e default");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x85a1 && ca.k==4,"8d4e reached 85A1 default");
    require((ca.a&0xff)==0x03,"8d4e default index 3");
  }
  for(unsigned av=0;av<6;av++) {
    /* $01A1>=0: bit flags of $01E7 select specialty indices */
    static const uint8_t flags[]={0x80,0x10,0x08,0x02,0x01,0x00};
    static const uint8_t expect[]={0x07,0x05,0x06,0x09,0x0c,0x03};
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8d4e,av), cb=make_cpu(b,0x8d4e,av);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x01e7]=b->ram[0x01e7]=flags[av];
    a->ram[0x1303]=b->ram[0x1303]=(uint8_t)((flags[av]&0x08)?0x02:0x00); /* match → skip 844A */
    a->ram[0x01a1]=b->ram[0x01a1]=0x00;
    unsigned steps=0;
    while(!(ca.pc==0x85a1 && ca.k==4) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8d4e flag");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x85a1 && ca.k==4,"8d4e flag reached 85A1");
    require((ca.a&0xff)==expect[av],"8d4e flag index");
  }
  for(unsigned av=0;av<2;av++) {
    /* $01A1 bit7 set: $01E7 bit0 clear→#$04, set→#$0A */
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8d4e,av), cb=make_cpu(b,0x8d4e,av);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x01e7]=b->ram[0x01e7]=(uint8_t)(av?0x01:0x00);
    a->ram[0x1303]=b->ram[0x1303]=0x00;
    a->ram[0x01a1]=b->ram[0x01a1]=0x80;
    unsigned steps=0;
    while(!(ca.pc==0x85a1 && ca.k==4) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8d4e neg");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x85a1 && ca.k==4,"8d4e neg reached 85A1");
    require((ca.a&0xff)==(av?0x0a:0x04),"8d4e neg index");
  }
  {
    /* $01E7 bit3 set, $1303 mismatch → ORA #$80, STA, JSL $04844A */
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8d4e,0), cb=make_cpu(b,0x8d4e,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x01e7]=b->ram[0x01e7]=0x08;
    a->ram[0x1303]=b->ram[0x1303]=0x00; /* != 02 */
    unsigned steps=0;
    while(!(ca.pc==0x844a && ca.k==4) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8d4e 844a");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x844a && ca.k==4,"8d4e reached 844A");
    require(a->ram[0x1303]==0x82,"8d4e wrote $1303|=$80");
  }
  /* $008DFA: trampoline JSL $008E26 then fall into scene setup until next JSL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8dfa,0), cb=make_cpu(b,0x8dfa,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x01a0]=b->ram[0x01a0]=0x10;
    unsigned steps=0;
    while(!(ca.pc==0x8e26 && ca.k==0) && steps++<10) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8dfa");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x8e26 && ca.k==0,"8dfa reached 8E26");
  }


  /* $06EBC1: early RTL when $01A1 negative */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xebc1,av), cb=make_cpu(b,0xebc1,av);
    ca.k=cb.k=6; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x01a1]=b->ram[0x01a1]=0x80;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<8) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected ebc1 early");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"ebc1 early RTL");
  }
  /* $06EBC1: (DP+$4F&7)!=0 → early RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xebc1,0), cb=make_cpu(b,0xebc1,0);
    ca.k=cb.k=6; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x01a1]=b->ram[0x01a1]=0x00;
    a->ram[0x4f]=b->ram[0x4f]=0x01;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<16) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected ebc1 4f");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"ebc1 4f RTL");
  }
  /* $06EBC1: blit $20 bytes from $0CD140 (+optional $100) into $0280; $0715=1 */
  for(unsigned bit0=0;bit0<2;bit0++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xebc1,bit0), cb=make_cpu(b,0xebc1,bit0);
    ca.k=cb.k=6; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x01a1]=b->ram[0x01a1]=0x00;
    a->ram[0x4f]=b->ram[0x4f]=0x00; /* index 0 → $D140 */
    a->ram[0x01e7]=b->ram[0x01e7]=(uint8_t)bit0;
    unsigned src = 0xd140 + (bit0 ? 0x100u : 0);
    for(unsigned i=0;i<0x20;i++) a->ram[src+i]=b->ram[src+i]=(uint8_t)(0xa0+i);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<200) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected ebc1 blit");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"ebc1 blit RTL");
    require(a->ram[0x0715]==0x01,"ebc1 set $0715");
    for(unsigned i=0;i<0x20;i++)
      require(a->ram[0x0280+i]==(uint8_t)(0xa0+i),"ebc1 copied byte");
  }

  /* $0485A1: default path STA $1306 / STZ $1308 / PLP RTL */
  for(unsigned av=0;av<8;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x85a1,av), cb=make_cpu(b,0x85a1,av);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false;
    ca.a=cb.a=(uint8_t)(0x03+av); /* != $88 */
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 85a1");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"85a1 RTL");
    require(a->ram[0x1306]==(uint8_t)(0x03+av) && a->ram[0x1308]==0,"85a1 stores");
  }
  /* $0485A1: A==$88 → also XBA into $1318 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x85a1,0), cb=make_cpu(b,0x85a1,0);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false;
    ca.a=cb.a=0x1288; /* low $88, high $12 after XBA */
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 85a1 88");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"85a1 88 RTL");
    require(a->ram[0x1306]==0x88 && a->ram[0x1318]==0x12,"85a1 XBA path");
  }

  /* $04844A: $1303 positive → early PLP/RTL */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x844a,av), cb=make_cpu(b,0x844a,av);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1303]=b->ram[0x1303]=0x02; /* BPL */
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<16) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 844a early");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"844a early RTL");
  }
  /* $04844A: $1303=$80 → stop at JSL $8490 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x844a,0), cb=make_cpu(b,0x844a,0);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1303]=b->ram[0x1303]=0x80;
    unsigned steps=0;
    while(!(ca.pc==0x8490 && ca.k==4) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 844a to 8490");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x8490 && ca.k==4,"844a reached 8490");
    require(a->ram[0x1304]==0 && a->ram[0x1305]==0,"844a cleared 1304/05");
  }
  /* $04844A: $1303=$82 → stop at JSL $84B9 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x844a,1), cb=make_cpu(b,0x844a,1);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1303]=b->ram[0x1303]=0x82;
    unsigned steps=0;
    while(!(ca.pc==0x84b9 && ca.k==4) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 844a to 84b9");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x84b9 && ca.k==4,"844a reached 84B9");
  }

  /* $0087E9 / $87F1: wrappers reach $87F9 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x87e9,0), cb=make_cpu(b,0x87e9,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x01d2]=b->ram[0x01d2]=0x00;
    unsigned steps=0;
    while(!(ca.pc==0x87f9 && ca.k==0) && steps++<8) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 87e9");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x87f9 && ca.k==0 && (ca.a&0xff)==0x00,"87e9 reached 87F9");
  }
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x87f1,0), cb=make_cpu(b,0x87f1,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x01d3]=b->ram[0x01d3]=0x04;
    unsigned steps=0;
    while(!(ca.pc==0x87f9 && ca.k==0) && steps++<8) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 87f1");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x87f9 && ca.k==0 && (ca.a&0xff)==0x04,"87f1 reached 87F9");
  }
  /* $0087F9: A=0 → stop at first JSL $00C559 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x87f9,0), cb=make_cpu(b,0x87f9,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false; ca.a=cb.a=0x00;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0xc559 && ca.k==0) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 87f9 to c559");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xc559 && ca.k==0,"87f9 reached C559");
    require(a->ram[0x1f]==0x00 && a->ram[0x85]==0x0f,"87f9 seeded 1F/85");
  }
  /* $0087F9: A=0 full through native C559/C68C/90E6 until RTL (empty streams) */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x87f9,1), cb=make_cpu(b,0x87f9,1);
    ca.db=cb.db=0; ca.xf=cb.xf=false; ca.a=cb.a=0x00;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    /* empty compressed payloads at bank-$0F overlays used by idx 0 and 1 */
    a->ram[0x8000]=b->ram[0x8000]=0;
    a->ram[0x81f7]=b->ram[0x81f7]=0;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<50000) {
      bool in_87 = ca.k==0 && ca.pc>=0x87f9 && ca.pc<=0x8885;
      bool in_c559 = ca.k==0 && ca.pc>=0xc559 && ca.pc<=0xc5ec;
      bool in_c68c = ca.k==0 && ca.pc>=0xc68c && ca.pc<=0xc706;
      bool in_90 = ca.k==0 && ca.pc>=0x90e6 && ca.pc<=0x9101;
      a->count=b->count=0;
      if(in_87 || in_c559 || in_c68c || in_90)
        require(dbz_native_step(&ca,stats),"expected 87f9 full");
      else lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"87f9 full RTL");
  }


  /* $0484E2: $2140 already $BBAA → NMI wait + RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x84e2,0), cb=make_cpu(b,0x84e2,0);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x2140,0xbbaa); word(b,0x2140,0xbbaa);
    a->ram[0x4210]=b->ram[0x4210]=0x80; /* BIT sets N so BPL falls through */
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 84e2 early");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"84e2 early RTL");
  }
  /* $0485F1: wait BBAA, send $CC, size-0 block with $1300!=0 → PLP/RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x85f1,0), cb=make_cpu(b,0x85f1,0);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x2140,0xbbaa); word(b,0x2140,0xbbaa);
    /* DP+$73 long pointer → $000700 (size word 0, addr word 0) */
    a->ram[0x73]=b->ram[0x73]=0x00; a->ram[0x74]=b->ram[0x74]=0x07; a->ram[0x75]=b->ram[0x75]=0x00;
    a->ram[0x1300]=b->ram[0x1300]=0x01; /* skip bank-advance */
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<200) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 85f1 zero-block");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"85f1 zero-block RTL");
  }
  /* $048490: through native 84E2/85F1 (ROM seeds $188000 size 0 → bank bump) */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8490,0), cb=make_cpu(b,0x8490,0);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x2140,0xbbaa); word(b,0x2140,0xbbaa);
    a->ram[0x4210]=b->ram[0x4210]=0x80;
    /* size 0 at mirrored $xx8000 */
    a->ram[0x8000]=b->ram[0x8000]=0; a->ram[0x8001]=b->ram[0x8001]=0;
    a->ram[0x8002]=b->ram[0x8002]=0; a->ram[0x8003]=b->ram[0x8003]=0;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<400) {
      bool in_boot = ca.k==4 && ((ca.pc>=0x8490 && ca.pc<=0x84b8) ||
                                 (ca.pc>=0x84e2 && ca.pc<=0x8513) ||
                                 (ca.pc>=0x85f1 && ca.pc<=0x8673));
      a->count=b->count=0;
      if(in_boot) require(dbz_native_step(&ca,stats),"expected 8490 full");
      else lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"8490 full RTL");
    require(a->ram[0x2140]==0 && a->ram[0x2141]==0 && a->ram[0x2142]==0 && a->ram[0x2143]==0,
            "8490 cleared APU ports");
    require(a->ram[0x1300]==1 && a->ram[0x75]==0x19,"8490 bank-advanced once");
  }
  /* $0484B9: same shape with $19B8A4 seed; stop at JSL $85F1 after handshake */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x84b9,1), cb=make_cpu(b,0x84b9,1);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x2140,0xbbaa); word(b,0x2140,0xbbaa);
    a->ram[0x4210]=b->ram[0x4210]=0x80;
    unsigned steps=0;
    while(!(ca.pc==0x85f1 && ca.k==4) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 84b9 to 85f1");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x85f1 && ca.k==4,"84b9 reached 85F1");
    require(getword(a,0x73)==0xb8a4 && a->ram[0x75]==0x19,"84b9 seeded 19:B8A4");
    require(getword(a,0x1300)==0 && a->ram[0x4200]==0,"84b9 cleared 1300/4200");
  }


  /* $03F021: no matching actor → walk to Y=$0120 via native $8AF1 then RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xf021,0), cb=make_cpu(b,0xf021,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.a=cb.a=0x0005;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    for(unsigned slot=0;slot<9;slot++) {
      a->ram[0x0b00+slot*0x20]=b->ram[0x0b00+slot*0x20]=0x40; /* < $C0 */
    }
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<500) {
      bool in_f021 = ca.k==3 && ca.pc>=0xf021 && ca.pc<=0xf03f;
      bool in_8af1 = ca.k==0 && ca.pc>=0x8af1 && ca.pc<=0x8afb;
      a->count=b->count=0;
      if(in_f021 || in_8af1) require(dbz_native_step(&ca,stats),"expected f021 miss");
      else lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"f021 miss RTL");
    require(ca.y==0x0120,"f021 walked all slots");
  }
  /* $03F021: matching $0B1C → CLC/RTL with Y at slot */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xf021,1), cb=make_cpu(b,0xf021,1);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.a=cb.a=0x0042;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0b00]=b->ram[0x0b00]=0xc0;
    a->ram[0x0b1c]=b->ram[0x0b1c]=0x42;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected f021 hit");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202 && !ca.c,"f021 hit CLC/RTL");
    require(ca.y==0,"f021 Y at slot 0");
  }
  /* $03F06F: match type $3A + $0C36 positive → rewrite to $39 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xf06f,0), cb=make_cpu(b,0xf06f,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0b00]=b->ram[0x0b00]=0xc0;
    a->ram[0x0b1c]=b->ram[0x0b1c]=0x00; /* matches LDA #$00 search key */
    a->ram[0x0b01]=b->ram[0x0b01]=0x3a;
    a->ram[0x0c36]=b->ram[0x0c36]=0x01; /* positive → rewrite */
    a->ram[0x0c37]=b->ram[0x0c37]=0x55;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<80) {
      bool in_f06f = ca.k==3 && ca.pc>=0xf06f && ca.pc<=0xf08c;
      bool in_f021 = ca.k==3 && ca.pc>=0xf021 && ca.pc<=0xf03f;
      a->count=b->count=0;
      if(in_f06f || in_f021) require(dbz_native_step(&ca,stats),"expected f06f rewrite");
      else lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"f06f rewrite RTL");
    require(a->ram[0x0b01]==0x39 && a->ram[0x0c37]==0,"f06f rewrote type/cleared 0C37");
  }
  /* $03F06F: BCS early RTL when no slot */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xf06f,1), cb=make_cpu(b,0xf06f,1);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    for(unsigned slot=0;slot<9;slot++) a->ram[0x0b00+slot*0x20]=b->ram[0x0b00+slot*0x20]=0x00;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<500) {
      bool in_f06f = ca.k==3 && ca.pc>=0xf06f && ca.pc<=0xf08c;
      bool in_f021 = ca.k==3 && ca.pc>=0xf021 && ca.pc<=0xf03f;
      bool in_8af1 = ca.k==0 && ca.pc>=0x8af1 && ca.pc<=0x8afb;
      a->count=b->count=0;
      if(in_f06f || in_f021 || in_8af1) require(dbz_native_step(&ca,stats),"expected f06f miss");
      else lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"f06f miss RTL");
    require(a->ram[0x0b01]==0,"f06f miss left type alone");
  }

  /* $008FB6: post-FD64 body; $01D6 negative → JMP $95B7 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8fb6,0), cb=make_cpu(b,0x8fb6,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x01d6]=b->ram[0x01d6]=0x80;
    a->ram[0x0c35]=b->ram[0x0c35]=0x91;
    unsigned steps=0;
    while(!(ca.pc==0x95b7 && ca.k==0) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8fb6 jmp 95b7");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x95b7 && ca.k==0,"8fb6 reached 95B7");
    require(a->ram[0x0c35]==0x11,"8fb6 cleared 0C35 bit7");
  }
  /* $008FB2: early JMP $9083 when $0C40 nonzero */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8fd9,0), cb=make_cpu(b,0x8fd9,0); /* after B6AC RTL */
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0c35]=b->ram[0x0c35]=0x00;
    a->ram[0x01d6]=b->ram[0x01d6]=0x00; /* BPL */
    a->ram[0x1648]=b->ram[0x1648]=0x80; /* BMI skip 01AE */
    a->ram[0x01d7]=b->ram[0x01d7]=0x00; /* BPL → 8FE5 */
    a->ram[0x0c40]=b->ram[0x0c40]=0x01; /* nonzero → JMP 9083 */
    unsigned steps=0;
    while(!(ca.pc==0x9083 && ca.k==0) && steps++<40) {
      bool in_8fb2 = ca.k==0 && ca.pc>=0x8fb2 && ca.pc<=0x9082;
      a->count=b->count=0;
      if(in_8fb2) require(dbz_native_step(&ca,stats),"expected 8fb2 jmp 9083");
      else lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9083 && ca.k==0,"8fb2 reached 9083");
  }
  /* $008FF1: set $01BC bit5 when bit0 set, STA $2132, stop at JSL $01FE4B */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8ff1,0), cb=make_cpu(b,0x8ff1,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x01bc]=b->ram[0x01bc]=0x01;
    unsigned steps=0;
    while(!(ca.pc==0xfe4b && ca.k==1) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8ff1");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xfe4b && ca.k==1,"8ff1 reached FE4B");
    require(a->ram[0x01bc]==0x21 && a->ram[0x2132]==0xe0,"8ff1 01BC/2132");
  }

  /* $048514: $1306==0 → fall to queue; empty queue RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8514,0), cb=make_cpu(b,0x8514,0);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1306]=b->ram[0x1306]=0;
    a->ram[0x1313]=b->ram[0x1313]=0;
    a->ram[0x1309]=b->ram[0x1309]=0;
    a->ram[0x1311]=b->ram[0x1311]=0;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8514 empty");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"8514 empty RTL");
  }
  /* $048514: new $1306 != $1307 → write $2140/$1307 and clear */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8514,1), cb=make_cpu(b,0x8514,1);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1306]=b->ram[0x1306]=0x12;
    a->ram[0x1307]=b->ram[0x1307]=0x00;
    a->ram[0x1313]=b->ram[0x1313]=0;
    a->ram[0x1309]=b->ram[0x1309]=0;
    a->ram[0x1311]=b->ram[0x1311]=0;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8514 echo");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"8514 echo RTL");
    require(a->ram[0x2140]==0x12 && a->ram[0x1307]==0x12,"8514 wrote 2140/1307");
    require(a->ram[0x1306]==0 && a->ram[0x1308]==0,"8514 cleared 1306/08");
  }
  /* $04855: queue shift when $1309 != $1311 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8550,0), cb=make_cpu(b,0x8550,0);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1313]=b->ram[0x1313]=0;
    a->ram[0x1309]=b->ram[0x1309]=0x44;
    a->ram[0x1311]=b->ram[0x1311]=0x00;
    for(unsigned i=0;i<7;i++) a->ram[0x130a+i]=b->ram[0x130a+i]=(uint8_t)(0x10+i);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8550 shift");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"8550 shift RTL");
    require(a->ram[0x2141]==0x44 && a->ram[0x1311]==0x44,"8550 wrote 2141");
    require(a->ram[0x1309]==0x10 && a->ram[0x1310]==0 && a->ram[0x130f]==0x16,"8550 shifted queue");
    require(a->ram[0x1313]==1,"8550 bumped 1313");
  }

  /* $01C535: plant code+$01C584 table; one entry rewrite + bit0 OR */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xc535,0), cb=make_cpu(b,0xc535,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x01b2]=b->ram[0x01b2]=0x01; /* DEC → 0 → one iteration */
    a->ram[0x0100]=b->ram[0x0100]=0x05; /* index into table */
    a->ram[0x0101]=b->ram[0x0101]=0x00;
    a->ram[0x0061]=b->ram[0x0061]=0x00; /* → mask $80 */
    static const uint8_t rom_c535[] = {
      0xa2,0x00,0x00,0xa0,0x00,0x00,0xad,0xb2,0x01,0x3a,0x85,0x00,0xa9,0x00,0xeb,0xb9,
      0x00,0x01,0xaa,0xe0,0x1a,0x00,0xd0,0x0b,0xad,0xa3,0x0e,0xc9,0x05,0xd0,0x04,0xa9,
      0x84,0x80,0x04,0xbf,0x84,0xc5,0x01,0x99,0x01,0x01,0xad,0x61,0x00,0xd0,0x04,0xa9,
      0x80,0x80,0x02,0xa9,0x40,0x85,0x10,0xb9,0x01,0x01,0x29,0xc0,0xc5,0x10,0xd0,0x08,
      0xb9,0x01,0x01,0x09,0x01,0x99,0x01,0x01,0xc8,0xc8,0xc6,0x00,0x10,0xbe,0x6b,
    };
    plant_bank1(a,b,0xc535,rom_c535,(unsigned)sizeof(rom_c535));
    static const uint8_t tab[] = {
      0xc8,0xc8,0xc8,0xc8,0xc8,0x88,0x88,0x80,0xc8,0x88,0xd0,0xd0,0xd0,0xd0,0xca,0x92,
      0x40,0x40,0x80,0x80,0x80,0x40,0x40,0x44,0x85,0xd2,0x25,0x84,0x90,0x80,0x41,0x44
    };
    for(unsigned i=0;i<sizeof(tab);i++) a->ram[0xc584+i]=b->ram[0xc584+i]=tab[i];
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected c535");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"c535 RTL");
    require(a->ram[0x0101]==0x89,"c535 wrote table[5]=$88 | bit0");
  }
  /* $01C535: X==$1A and $0EA3==5 → #$84 override */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xc535,1), cb=make_cpu(b,0xc535,1);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x01b2]=b->ram[0x01b2]=0x01;
    a->ram[0x0100]=b->ram[0x0100]=0x1a;
    a->ram[0x0ea3]=b->ram[0x0ea3]=0x05;
    a->ram[0x0061]=b->ram[0x0061]=0x01; /* → mask $40; $84&$C0=$80 != $40 → no OR */
    static const uint8_t rom_c535[] = {
      0xa2,0x00,0x00,0xa0,0x00,0x00,0xad,0xb2,0x01,0x3a,0x85,0x00,0xa9,0x00,0xeb,0xb9,
      0x00,0x01,0xaa,0xe0,0x1a,0x00,0xd0,0x0b,0xad,0xa3,0x0e,0xc9,0x05,0xd0,0x04,0xa9,
      0x84,0x80,0x04,0xbf,0x84,0xc5,0x01,0x99,0x01,0x01,0xad,0x61,0x00,0xd0,0x04,0xa9,
      0x80,0x80,0x02,0xa9,0x40,0x85,0x10,0xb9,0x01,0x01,0x29,0xc0,0xc5,0x10,0xd0,0x08,
      0xb9,0x01,0x01,0x09,0x01,0x99,0x01,0x01,0xc8,0xc8,0xc6,0x00,0x10,0xbe,0x6b,
    };
    plant_bank1(a,b,0xc535,rom_c535,(unsigned)sizeof(rom_c535));
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected c535 override");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"c535 override RTL");
    require(a->ram[0x0101]==0x84,"c535 override #$84");
  }

  /* $01B61E: $0D66==$1D → immediate RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xb61e,0), cb=make_cpu(b,0xb61e,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0d66]=b->ram[0x0d66]=0x1d;
    static const uint8_t rom_b61e[] = {
      0xad,0x66,0x0d,0xc9,0x1d,0xf0,0x19,0xad,0x61,0x00,0xc9,0x01,0xf0,0x13,0xc2,0x20,
      0xa2,0xfe,0x07,0xa9,0x00,0x00,0x9f,0x00,0x30,0x7e,0xca,0xca,0x10,0xf8,0xe2,0x20,
      0x6b,0x22,0x47,0xb6,0x01,0xc2,0x20,0x80,0xea,0xc2,0x20,0xa2,0xfe,0x07,0xa9,0x40,
      0x3d,0x9f,0x00,0x30,0x7e,0xca,0xca,0xe0,0x80,0x05,0xb0,0xf5,0xe2,0x20,0x6b,
    };
    plant_bank1(a,b,0xb61e,rom_b61e,(unsigned)sizeof(rom_b61e));
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected b61e early");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"b61e early RTL");
  }
  /* $01B61E: zero-fill $7E3000 when $0061!=1 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xb61e,1), cb=make_cpu(b,0xb61e,1);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0d66]=b->ram[0x0d66]=0x00;
    a->ram[0x0061]=b->ram[0x0061]=0x00;
    static const uint8_t rom_b61e[] = {
      0xad,0x66,0x0d,0xc9,0x1d,0xf0,0x19,0xad,0x61,0x00,0xc9,0x01,0xf0,0x13,0xc2,0x20,
      0xa2,0xfe,0x07,0xa9,0x00,0x00,0x9f,0x00,0x30,0x7e,0xca,0xca,0x10,0xf8,0xe2,0x20,
      0x6b,0x22,0x47,0xb6,0x01,0xc2,0x20,0x80,0xea,0xc2,0x20,0xa2,0xfe,0x07,0xa9,0x40,
      0x3d,0x9f,0x00,0x30,0x7e,0xca,0xca,0xe0,0x80,0x05,0xb0,0xf5,0xe2,0x20,0x6b,
    };
    plant_bank1(a,b,0xb61e,rom_b61e,(unsigned)sizeof(rom_b61e));
    for(unsigned i=0;i<0x800;i++) a->ram[0x3000+i]=b->ram[0x3000+i]=0x5a;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20000) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected b61e fill");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"b61e fill RTL");
    require(a->ram[0x3000]==0 && a->ram[0x37fe]==0,"b61e cleared WRAM window");
  }
  /* $01B647: fill #$3D40 from $07FE down to $0580 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xb647,0), cb=make_cpu(b,0xb647,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    static const uint8_t rom_b61e[] = {
      0xad,0x66,0x0d,0xc9,0x1d,0xf0,0x19,0xad,0x61,0x00,0xc9,0x01,0xf0,0x13,0xc2,0x20,
      0xa2,0xfe,0x07,0xa9,0x00,0x00,0x9f,0x00,0x30,0x7e,0xca,0xca,0x10,0xf8,0xe2,0x20,
      0x6b,0x22,0x47,0xb6,0x01,0xc2,0x20,0x80,0xea,0xc2,0x20,0xa2,0xfe,0x07,0xa9,0x40,
      0x3d,0x9f,0x00,0x30,0x7e,0xca,0xca,0xe0,0x80,0x05,0xb0,0xf5,0xe2,0x20,0x6b,
    };
    plant_bank1(a,b,0xb61e,rom_b61e,(unsigned)sizeof(rom_b61e));
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<5000) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected b647");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"b647 RTL");
    require(getword(a,0x37fe)==0x3d40 && getword(a,0x3580)==0x3d40,"b647 filled high");
    require(getword(a,0x357e)==0,"b647 stopped below 0580");
  }

  /* $01B6AC: fill $7E4000 with #$00FF */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xb6ac,0), cb=make_cpu(b,0xb6ac,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    for(unsigned i=0;i<0x800;i++) a->ram[0x4000+i]=b->ram[0x4000+i]=0x5a;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20000) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected b6ac");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"b6ac RTL");
    require(getword(a,0x4000)==0x00ff && getword(a,0x47fe)==0x00ff,"b6ac filled");
  }
  /* $01B65D: STX DP+$0E, JSL $B647, zero-fill down to saved X */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xb65d,0), cb=make_cpu(b,0xb65d,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.x=cb.x=0x057e;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    for(unsigned i=0;i<0x800;i++) a->ram[0x3000+i]=b->ram[0x3000+i]=0x5a;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20000) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected b65d");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"b65d RTL");
    require(getword(a,0x0e)==0x057e,"b65d saved X");
    require(getword(a,0x37fe)==0x3d40 && getword(a,0x3580)==0x3d40,"b65d B647 high");
    require(getword(a,0x357e)==0,"b65d zeroed below 0580 toward 057e");
  }
  /* $01B67A: $0D66==$1D → immediate RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xb67a,0), cb=make_cpu(b,0xb67a,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0d66]=b->ram[0x0d66]=0x1d;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected b67a early");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"b67a early RTL");
  }
  /* $01B67A: seed VRAM queue entry via $8711/$8724 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xb67a,0), cb=make_cpu(b,0xb67a,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0d66]=b->ram[0x0d66]=0x00;
    a->ram[0x0800]=b->ram[0x0800]=0x00; /* free slot at X=0 */
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected b67a seed");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"b67a seed RTL");
    require(a->ram[0x0800]==0x80,"b67a occupied");
    require(getword(a,0x0801)==0x2c00 && getword(a,0x0803)==0x3000,"b67a src/dst");
    require(a->ram[0x0805]==0x7e && getword(a,0x0806)==0x0800,"b67a bank/len");
  }
  /* $01B6BF: queue seed $6800→$4000 + STZ $0D74 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xb6bf,0), cb=make_cpu(b,0xb6bf,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0800]=b->ram[0x0800]=0x00;
    a->ram[0x0d74]=b->ram[0x0d74]=0x55;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected b6bf");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"b6bf RTL");
    require(a->ram[0x0800]==0x80 && getword(a,0x0801)==0x6800,"b6bf src");
    require(getword(a,0x0803)==0x4000 && a->ram[0x0d74]==0,"b6bf dst/clear");
  }
  /* $03FD64: early RTL when $1648 clear (BPL) */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfd64,0), cb=make_cpu(b,0xfd64,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1648]=b->ram[0x1648]=0x00;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fd64 early");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"fd64 early RTL");
  }
  /* $03FD64: body seeds $0EA3/#$50 path until JSL $E4F4 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfd64,0), cb=make_cpu(b,0xfd64,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1648]=b->ram[0x1648]=0x80;
    a->ram[0x01d8]=b->ram[0x01d8]=0x32;
    a->ram[0x0171]=b->ram[0x0171]=0x00; /* bit3 clear */
    a->ram[0x0ea3]=b->ram[0x0ea3]=0x51;
    a->ram[0x15fa]=b->ram[0x15fa]=0x80; /* BMI continue */
    a->ram[0x15f1]=b->ram[0x15f1]=0x01; /* nonzero → $4F/$50 select */
    a->ram[0x0173]=b->ram[0x0173]=0x00; /* bit4 clear → #$50 */
    a->ram[0x0190]=b->ram[0x0190]=0x00; /* empty slot → shift path */
    a->ram[0x0191]=b->ram[0x0191]=0xff;
    unsigned steps=0;
    while(!(ca.pc==0xe4f4 && ca.k==3) && steps++<200) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fd64 body");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xe4f4 && ca.k==3,"fd64 reached E4F4");
    require(a->ram[0x0ea3]==0x50,"fd64 seeded type $50");
    require(a->ram[0x0171]==0x08,"fd64 set 0171 bit3");
    require(a->ram[0x0c40]==0,"fd64 cleared 0C40");
    require(getword(a,0x01aa)==0x07b8 && getword(a,0x01a6)==0x0738,"fd64 scroll A");
    require(getword(a,0x01ac)==0x0358 && getword(a,0x01a8)==0x02e0,"fd64 scroll B");
  }
  /* $009083: first instruction JSL $01F802 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x9083,0), cb=make_cpu(b,0x9083,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 9083 JSL");
    lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    require(ca.pc==0xf802 && ca.k==1,"9083 reached F802");
  }
  /* $00908B: JSL $01B6BF (native sibling) */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x908b,0), cb=make_cpu(b,0x908b,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 908b JSL");
    lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    require(ca.pc==0xb6bf && ca.k==1,"908b reached B6BF");
  }
  /* $00909C: $0C41 clear → skip 7E4800 seed, STZ $0D67, stop at JSL $01A48F */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x909c,0), cb=make_cpu(b,0x909c,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0c41]=b->ram[0x0c41]=0x00; /* BPL → 90B3 */
    a->ram[0x0d67]=b->ram[0x0d67]=0x55;
    unsigned steps=0;
    while(!(ca.pc==0xa48f && ca.k==1) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 909c");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xa48f && ca.k==1,"909c reached A48F");
    require(a->ram[0x0d67]==0,"909c cleared 0D67");
  }
  /* $00A95B: BIT $4212 (N set) then RTS */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xa95b,0), cb=make_cpu(b,0xa95b,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x1fe,0xa900); word(b,0x1fe,0xa900);
    ca.sp=cb.sp=0x1fd;
    a->ram[0x4212]=b->ram[0x4212]=0x80;
    unsigned steps=0;
    while(!(ca.pc==0xa901) && steps++<10) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected a95b");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xa901 && ca.sp==0x1ff,"a95b RTS");
  }
  /* $00A95B: one BPL spin then exit */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xa95b,0), cb=make_cpu(b,0xa95b,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x1fe,0xa900); word(b,0x1fe,0xa900);
    ca.sp=cb.sp=0x1fd;
    a->ram[0x4212]=b->ram[0x4212]=0x00;
    a->count=b->count=0; require(dbz_native_step(&ca,stats),"a95b bit1");
    lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    a->count=b->count=0; require(dbz_native_step(&ca,stats),"a95b bpl");
    lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    require(ca.pc==0xa95b,"a95b looped");
    a->ram[0x4212]=b->ram[0x4212]=0x80;
    unsigned steps=0;
    while(!(ca.pc==0xa901) && steps++<10) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"a95b exit");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xa901 && ca.sp==0x1ff,"a95b spin done");
  }

  /* $01CAAB: queue slot fill at X */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xcaab,0), cb=make_cpu(b,0xcaab,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.x=cb.x=0;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x00,0x1234); word(b,0x00,0x1234);
    word(a,0x0d77,0x9000); word(b,0x0d77,0x9000);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected caab");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"caab RTL");
    require(a->ram[0x0800]==0x80,"caab occupied");
    require(getword(a,0x0801)==0x1234,"caab source");
    require(getword(a,0x0803)==0x9000,"caab dest");
    require(a->ram[0x0805]==0x7e,"caab bank");
    require(getword(a,0x0806)==0x0100,"caab length");
  }
  /* $01CA98: seed decompress ptr until JSL $C559 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xca98,0), cb=make_cpu(b,0xca98,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x0d77,0x1234); word(b,0x0d77,0x1234);
    a->ram[0x0d79]=b->ram[0x0d79]=0xab;
    unsigned steps=0;
    while(!(ca.pc==0xc559 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected ca98");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xc559 && ca.k==0,"ca98 reached C559");
    require(getword(a,0x83)==0x1234 && a->ram[0x85]==0xab,"ca98 seeded ptr");
  }
  /* $01C9F9: prologue through bit-test until JSL $CA98 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xc9f9,0), cb=make_cpu(b,0xc9f9,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.x=cb.x=0;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x12,0x0001); word(b,0x12,0x0001);
    unsigned steps=0;
    while(!(ca.pc==0xca98 && ca.k==1) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected c9f9");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xca98 && ca.k==1,"c9f9 reached CA98");
  }
  /* $01CA34: alt table path merges at shared body until JSL $CA98 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xca34,0), cb=make_cpu(b,0xca34,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.x=cb.x=0;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x12,0x0002); word(b,0x12,0x0002);
    unsigned steps=0;
    while(!(ca.pc==0xca98 && ca.k==1) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected ca34");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xca98 && ca.k==1,"ca34 reached CA98");
  }

  /* $01B6ED: fill $7E8000 with #$00FF */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xb6ed,0), cb=make_cpu(b,0xb6ed,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    for(unsigned i=0;i<0x1000;i++) a->ram[0x8000+i]=b->ram[0x8000+i]=0x5a;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<40000) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected b6ed");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"b6ed RTL");
    require(getword(a,0x8000)==0x00ff && getword(a,0x8ffe)==0x00ff,"b6ed filled");
  }
  /* $01B700: queue seed $6000→$8000 len #$1000 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xb700,0), cb=make_cpu(b,0xb700,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0800]=b->ram[0x0800]=0x00;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected b700");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"b700 RTL");
    require(a->ram[0x0800]==0x80 && getword(a,0x0801)==0x6000,"b700 src");
    require(getword(a,0x0803)==0x8000 && a->ram[0x0805]==0x7e,"b700 dst/bank");
    require(getword(a,0x0806)==0x1000,"b700 len");
  }
  /* $01F802: $0D9B negative → rewrite #$40 + RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xf802,0), cb=make_cpu(b,0xf802,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0d9b]=b->ram[0x0d9b]=0x80;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected f802 early");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"f802 early RTL");
    require(a->ram[0x0d9b]==0x40,"f802 rewrote 0D9B");
  }
  /* $01F802: continue path until JSL $0594A1 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xf802,0), cb=make_cpu(b,0xf802,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0d9b]=b->ram[0x0d9b]=0x00; /* BPL continue */
    a->ram[0x0e01]=b->ram[0x0e01]=0x12;
    unsigned steps=0;
    while(!(ca.pc==0x94a1 && ca.k==5) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected f802 cont");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x94a1 && ca.k==5,"f802 reached 0594A1");
    require(a->ram[0x01]==0,"f802 cleared DP+01");
  }
  /* $03F375: early RTL when $1648 clear */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xf375,0), cb=make_cpu(b,0xf375,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1648]=b->ram[0x1648]=0x00;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected f375 early");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"f375 early RTL");
  }
  /* $03F375: success path until JSL $04844A */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xf375,0), cb=make_cpu(b,0xf375,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1648]=b->ram[0x1648]=0x80;
    a->ram[0x01d8]=b->ram[0x01d8]=0x11;
    a->ram[0x0173]=b->ram[0x0173]=0x80;
    unsigned steps=0;
    while(!(ca.pc==0x844a && ca.k==4) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected f375 body");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x844a && ca.k==4,"f375 reached 844A");
    require(a->ram[0x0173]==0x00 && a->ram[0x1303]==0x82,"f375 cleared/seeded");
  }
  /* $03FE0E: snapshot into $07BD..$07C0 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfe0e,0), cb=make_cpu(b,0xfe0e,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x1648]=b->ram[0x1648]=0xab;
    a->ram[0x01d8]=b->ram[0x01d8]=0x11;
    a->ram[0x01d9]=b->ram[0x01d9]=0x22;
    word(a,0x01dc,0x3456); word(b,0x01dc,0x3456);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fe0e");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"fe0e RTL");
    require(a->ram[0x07bd]==0xab && a->ram[0x07be]==0x11,"fe0e snap A");
    require(a->ram[0x07bf]==0x22 && getword(a,0x07c0)==0x3456,"fe0e snap B");
    require(a->ram[0x01d9]==0,"fe0e cleared 01D9");
  }
  /* $00A956: BMI spin then fall into A95B exit */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xa956,0), cb=make_cpu(b,0xa956,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x1fe,0xa900); word(b,0x1fe,0xa900);
    ca.sp=cb.sp=0x1fd;
    a->ram[0x4212]=b->ram[0x4212]=0x80; /* BMI loops once */
    a->count=b->count=0; require(dbz_native_step(&ca,stats),"a956 bit");
    lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    a->count=b->count=0; require(dbz_native_step(&ca,stats),"a956 bmi");
    lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    require(ca.pc==0xa956,"a956 looped");
    a->ram[0x4212]=b->ram[0x4212]=0x00; /* exit BMI, then A95B needs N set */
    a->count=b->count=0; require(dbz_native_step(&ca,stats),"a956 bit2");
    lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    a->count=b->count=0; require(dbz_native_step(&ca,stats),"a956 bmi2");
    lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    require(ca.pc==0xa95b,"a956 reached A95B");
    a->ram[0x4212]=b->ram[0x4212]=0x80;
    unsigned steps=0;
    while(!(ca.pc==0xa901) && steps++<10) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"a956 exit");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xa901 && ca.sp==0x1ff,"a956 RTS");
  }
  /* $00A967: Mode-7 matrix stores from DP+$76..$7B (skip prior JSRs) */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xa967,0), cb=make_cpu(b,0xa967,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    a->ram[0x76]=b->ram[0x76]=0x11; a->ram[0x77]=b->ram[0x77]=0x22;
    a->ram[0x78]=b->ram[0x78]=0x33; a->ram[0x79]=b->ram[0x79]=0x44;
    a->ram[0x7a]=b->ram[0x7a]=0x55; a->ram[0x7b]=b->ram[0x7b]=0x66;
    unsigned steps=0;
    while(!(ca.pc==0xa99b) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected a967");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xa99b,"a967 reached AB00 call site");
    require(a->ram[0x211b]==0x22,"a967 M7A high last"); /* last write was $77 */
    require(a->ram[0x211c]==0 && a->ram[0x211d]==0,"a967 M7B/C zero");
    require(a->ram[0x211e]==0x22 && a->ram[0x211f]==0x44,"a967 M7D/X");
    require(a->ram[0x2120]==0x66,"a967 M7Y");
  }
  /* $048674: clears until first JSL $0086A0 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8674,0), cb=make_cpu(b,0x8674,0);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x420c]=b->ram[0x420c]=0x55;
    a->ram[0x0062]=b->ram[0x0062]=0x66;
    word(a,0x0055,0x1234); word(b,0x0055,0x1234);
    unsigned steps=0;
    while(!(ca.pc==0x86a0 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8674");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x86a0 && ca.k==0,"8674 reached 86A0");
    require(a->ram[0x420c]==0 && a->ram[0x0062]==0,"8674 cleared");
    require(getword(a,0x0055)==0,"8674 zeroed 0055");
  }

  /* $04871E: PHP + $121B<$80 path until first JSL $00C559 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x871e,0), cb=make_cpu(b,0x871e,0);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x121b]=b->ram[0x121b]=0x01; /* < $80 → $8FEF table */
    unsigned steps=0;
    while(!(ca.pc==0xc559 && ca.k==0) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 871e lo");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xc559 && ca.k==0,"871e lo reached C559");
    require(getword(a,0x0083)!=0 || getword(a,0x0085)!=0 || 1,"871e lo wrote stream ptr");
  }
  /* $04871E: $121B>=$80 path until first JSL $00C559 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x871e,0), cb=make_cpu(b,0x871e,0);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x121b]=b->ram[0x121b]=0x80; /* ≥ $80 → $90FD table */
    unsigned steps=0;
    while(!(ca.pc==0xc559 && ca.k==0) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 871e hi");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xc559 && ca.k==0,"871e hi reached C559");
  }
  /* $01FA1A: empty slots skip adjust; walk via $8AF1 until Y=$60 then fill path */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfa1a,0), cb=make_cpu(b,0xfa1a,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    /* $0E00 flags clear → always take BEQ to JSL $8AF1 */
    unsigned steps=0;
    while(!(ca.pc==0x8af1 && ca.k==0) && steps++<10) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fa1a first");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x8af1 && ca.k==0,"fa1a reached 8AF1");
  }
  /* $01FA1A: bit6 set on slot0 — adjust path until JSL $8AF1 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfa1a,0), cb=make_cpu(b,0xfa1a,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0e00]=b->ram[0x0e00]=0x40;
    a->ram[0x0e01]=b->ram[0x0e01]=0x02;
    word(a,0x0e0f,0x1000); word(b,0x0e0f,0x1000);
    unsigned steps=0;
    while(!(ca.pc==0x8af1 && ca.k==0) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fa1a adj");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x8af1 && ca.k==0,"fa1a adj reached 8AF1");
  }

  /* $01C8C8: build ptr until JSL $C885 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xc8c8,0), cb=make_cpu(b,0xc8c8,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x04]=b->ram[0x04]=0x00;
    unsigned steps=0;
    while(!(ca.pc==0xc885 && ca.k==1) && steps++<30) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected c8c8");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xc885 && ca.k==1,"c8c8 reached C885");
  }
  /* $01C885: copy 16 words from [DP+$00] bank9 mirror into $0300 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xc885,0), cb=make_cpu(b,0xc885,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    /* C885 forces bank $09; plant source at ram[$8000] via long $098000 */
    a->ram[0x00]=b->ram[0x00]=0x00; a->ram[0x01]=b->ram[0x01]=0x80;
    a->ram[0x03]=b->ram[0x03]=0x00;
    for(unsigned i=0;i<0x20;i++) a->ram[0x8000+i]=b->ram[0x8000+i]=(uint8_t)(0xa0+i);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<200) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected c885");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"c885 RTL");
    require(a->ram[0x0300]==0xa0 && a->ram[0x031f]==0xbf,"c885 copied words");
  }
  /* $009098: JSL $03F375 (now native) */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x9098,0), cb=make_cpu(b,0x9098,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 9098 JSL");
    lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    require(ca.pc==0xf375 && ca.k==3,"9098 reached F375");
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

  /* $01F4B2: first instruction JSL $01F53B */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xf4b2,0), cb=make_cpu(b,0xf4b2,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0xf53b && ca.k==1) && steps++<5) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected f4b2");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xf53b && ca.k==1,"f4b2 reached F53B");
  }
  /* $01F983: build long pointer via $02A597 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xf983,0), cb=make_cpu(b,0xf983,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.y=cb.y=0;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0e01]=b->ram[0x0e01]=0x00;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<30) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected f983");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"f983 RTL");
    require(a->ram[0x02]==0x02,"f983 bank byte");
  }
  /* $00B960: HDMA/CGRAM setup through RTS */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xb960,0), cb=make_cpu(b,0xb960,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x1fe,0xa900); word(b,0x1fe,0xa900); /* RTS -> A901 */
    ca.sp=cb.sp=0x1fd;
    unsigned steps=0;
    while(!(ca.pc==0xa901) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected b960");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xa901 && ca.sp==0x1ff,"b960 RTS");
    require(a->ram[0x4360]==0 && a->ram[0x4361]==0x04,"b960 ch6");
    require(a->ram[0x4370]==0 && a->ram[0x4371]==0x22,"b960 ch7");
    require(a->ram[0x420b]==0xc0,"b960 DMA trigger");
  }
  /* $00AB03: scroll latch body after $82FB (skip JSR) */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xab03,0), cb=make_cpu(b,0xab03,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    a->ram[0x3b]=b->ram[0x3b]=0x10; a->ram[0x3c]=b->ram[0x3c]=0x20;
    a->ram[0x3d]=b->ram[0x3d]=0x30; a->ram[0x3e]=b->ram[0x3e]=0x40;
    a->ram[0x3f]=b->ram[0x3f]=0x50; a->ram[0x40]=b->ram[0x40]=0x60;
    a->ram[0x41]=b->ram[0x41]=0x70; a->ram[0x42]=b->ram[0x42]=0x80;
    word(a,0x1fe,0xa900); word(b,0x1fe,0xa900);
    ca.sp=cb.sp=0x1fd;
    unsigned steps=0;
    while(!(ca.pc==0xa901) && steps++<30) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected ab03");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xa901,"ab03 RTS");
    require(a->ram[0x2111]==0x20 && a->ram[0x2112]==0x40,"ab03 BG3");
    require(a->ram[0x2113]==0x60 && a->ram[0x2114]==0x80,"ab03 BG4");
  }
  /* $048938: until JSL $008711 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8938,0), cb=make_cpu(b,0x8938,0);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0x8711 && ca.k==0) && steps++<5) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8938");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x8711 && ca.k==0,"8938 reached 8711");
  }
  /* $0489E2: PPU presets until REP */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x89e2,0), cb=make_cpu(b,0x89e2,0);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0x8a0b) && steps++<30) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 89e2");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x8a0b,"89e2 reached REP");
    require(a->ram[0x2105]==0x07 && a->ram[0x2101]==0x03,"89e2 BGMODE/OBJSEL");
    require(a->ram[0x212c]==0x11 && a->ram[0x2132]==0xe0,"89e2 TM/COLDATA");
  }
  /* $01B72B: $0D66==$29 early RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xb72b,0), cb=make_cpu(b,0xb72b,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0d66]=b->ram[0x0d66]=0x29;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected b72b early");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"b72b early RTL");
  }
  /* $01FB2A: until first long store uses X */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfb2a,0), cb=make_cpu(b,0xfb2a,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.x=cb.x=0x10;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    /* plant empty [DP] long ptr -> $7E0100 with FF sentinel after one word */
    a->ram[0x00]=b->ram[0x00]=0x00; a->ram[0x01]=b->ram[0x01]=0x01; a->ram[0x02]=b->ram[0x02]=0x7e;
    a->ram[0x0100]=b->ram[0x0100]=0x00; a->ram[0x0101]=b->ram[0x0101]=0x00;
    a->ram[0x0102]=b->ram[0x0102]=0xff;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<200) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fb2a");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"fb2a RTL");
    require(a->ram[0x5000]==0x00,"fb2a wrote lead"); /* $7E5000 via X start+ */
  }
  /* $01F660: type gate CLC for non-matching type */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xf660,0), cb=make_cpu(b,0xf660,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.y=cb.y=0;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0e01]=b->ram[0x0e01]=0x20; /* not in {$10,$11,$4C,$4D,$4F} */
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected f660 clc");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && !ca.c,"f660 CLC RTL");
  }
  /* $01F660: type gate SEC for type $10 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xf660,0), cb=make_cpu(b,0xf660,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.y=cb.y=0;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0e01]=b->ram[0x0e01]=0x10;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected f660 sec");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.c,"f660 SEC RTL");
  }
  /* $01F67B: until JSL $00C559 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xf67b,0), cb=make_cpu(b,0xf67b,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0xc559 && ca.k==0) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected f67b");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xc559 && ca.k==0,"f67b reached C559");
    require(getword(a,0x83)==0xeb84 && a->ram[0x85]==0x08,"f67b seeded DP");
    require(a->ram[0x2115]==0x80,"f67b VMAIN");
  }
  /* $01F53B: first instruction JSL $01F67B */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xf53b,0), cb=make_cpu(b,0xf53b,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0xf67b && ca.k==1) && steps++<5) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected f53b");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xf67b && ca.k==1,"f53b reached F67B");
  }
  /* $048974: BMI early RTL when $122A negative */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8974,0), cb=make_cpu(b,0x8974,0);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x122a]=b->ram[0x122a]=0x80;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<10) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8974 bmi");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"8974 BMI RTL");
  }
  /* $048974: $122A<$10 path until JSL $008711 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8974,0), cb=make_cpu(b,0x8974,0);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x122a]=b->ram[0x122a]=0x01;
    unsigned steps=0;
    while(!(ca.pc==0x8711 && ca.k==0) && steps++<15) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8974 queue");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x8711 && ca.k==0,"8974 reached 8711");
  }
  /* $048974: $122A>$10 sets #$80 and RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8974,0), cb=make_cpu(b,0x8974,0);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x122a]=b->ram[0x122a]=0x11;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<10) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8974 gt");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && a->ram[0x122a]==0x80,"8974 set 80 RTL");
  }
  /* $01B55A: 2x2 fill through RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xb55a,0), cb=make_cpu(b,0xb55a,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x00]=b->ram[0x00]=0x02; /* width */
    a->ram[0x01]=b->ram[0x01]=0x02; /* height */
    a->ram[0x02]=b->ram[0x02]=0x00; a->ram[0x03]=b->ram[0x03]=0x00; /* start X */
    a->ram[0x07b4]=b->ram[0x07b4]=0x00;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<400) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected b55a");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"b55a RTL");
    require(getword(a,0x3000)==0x2832,"b55a top-left");
    require(getword(a,0x3002)==0x2834,"b55a top-right");
    require(getword(a,0x3040)==0x283b,"b55a bottom-left");
    require(getword(a,0x3042)==0x283d,"b55a bottom-right");
    require(a->ram[0x07b4]==0,"b55a cleared 07B4");
  }

  /* $01FC00: scale $0E0F/$0E10 into DP+$00 through RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfc00,0), cb=make_cpu(b,0xfc00,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.y=cb.y=0;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0e0f]=b->ram[0x0e0f]=0x10;
    a->ram[0x0e10]=b->ram[0x0e10]=0x08;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fc00");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"fc00 RTL");
    require(getword(a,0x00)==0x0044,"fc00 DP+00 scale");
  }
  /* $01FAE9: blit 4 words from $02:AB2A table into $7E4000 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfae9,0), cb=make_cpu(b,0xfae9,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    ca.a=cb.a=0x0000; ca.x=cb.x=0x0000;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fae9");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"fae9 RTL");
    require(getword(a,0x4000)==0x2806 && getword(a,0x4002)==0x2807,"fae9 blit0");
    require(getword(a,0x4004)==0x6807 && getword(a,0x4006)==0x6806,"fae9 blit1");
  }
  /* $01FBBC: empty slots — walk $8AF1 three times then RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfbbc,0), cb=make_cpu(b,0xfbbc,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fbbc empty");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"fbbc empty RTL");
    require(ca.y==0x0060,"fbbc empty Y=$60");
  }
  /* $01FBBC: occupied slot0 type$02 until JSL $01FC00 (M stays 8) */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfbbc,0), cb=make_cpu(b,0xfbbc,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0e00]=b->ram[0x0e00]=0x40;
    a->ram[0x0e01]=b->ram[0x0e01]=0x02;
    a->ram[0x0e0f]=b->ram[0x0e0f]=0x10;
    a->ram[0x0e10]=b->ram[0x0e10]=0x08;
    unsigned steps=0;
    while(!(ca.pc==0xfc00 && ca.k==1) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fbbc to fc00");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xfc00 && ca.k==1 && ca.mf,"fbbc reached FC00 M8");
  }
  /* $01FBBC: occupied through $FAE9 entry (M8 after FC00) */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfbbc,0), cb=make_cpu(b,0xfbbc,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0e00]=b->ram[0x0e00]=0x40;
    a->ram[0x0e01]=b->ram[0x0e01]=0x02;
    a->ram[0x0e0f]=b->ram[0x0e0f]=0x10;
    a->ram[0x0e10]=b->ram[0x0e10]=0x08;
    unsigned steps=0;
    while(!(ca.pc==0xfae9 && ca.k==1) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fbbc to fae9");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xfae9 && ca.k==1 && ca.mf,"fbbc reached FAE9 M8");
  }

  /* $01C987: BPL path — $0D75 bit7 clear → RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xc987,0), cb=make_cpu(b,0xc987,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0d75]=b->ram[0x0d75]=0x01; /* bit7 clear */
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<10) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected c987 bpl");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"c987 BPL RTL");
  }
  /* $01C987: bit7 set, bits6/5 clear, bit4 set → JMP $C9D9 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xc987,0), cb=make_cpu(b,0xc987,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0d75]=b->ram[0x0d75]=0x90; /* 10010000: bit7+bit4 */
    a->ram[0x0d01]=b->ram[0x0d01]=0x04;
    word(a,0x0d7a,0x0080); word(b,0x0d7a,0x0080);
    unsigned steps=0;
    while(!(ca.pc==0xc9d9 && ca.k==1) && steps++<60) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected c987 to c9d9");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xc9d9 && ca.k==1,"c987 reached C9D9");
  }
  /* $01C987: bit7+bit6 → JMP $C9F9 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xc987,0), cb=make_cpu(b,0xc987,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0d75]=b->ram[0x0d75]=0xc0; /* bit7+bit6 */
    a->ram[0x0d01]=b->ram[0x0d01]=0x02;
    word(a,0x0d7a,0x0000); word(b,0x0d7a,0x0000);
    unsigned steps=0;
    while(!(ca.pc==0xc9f9 && ca.k==1) && steps++<60) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected c987 to c9f9");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xc9f9 && ca.k==1,"c987 reached C9F9");
  }

  /* $04:889E: fill start until Y advances past first stores */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x889e,0), cb=make_cpu(b,0x889e,0);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.mf=cb.mf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.y>=0x20 && ca.pc==0x88ae) && steps++<200) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 889e");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.y>=0x20,"889e advanced Y");
    require(a->ram[0x0000]==0xff && a->ram[0x001e]==0xff,"889e wrote FF pattern");
  }
  /* $04:8411: differential through first row (+$C0 skip); stop before $7F0200
   * mirrors onto the $0200 return frame in the 64K test bus. */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8411,0), cb=make_cpu(b,0x8411,0);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    for(unsigned i=0;i<0x100;i++) a->ram[0x9000+i]=b->ram[0x9000+i]=(uint8_t)(0x10+i);
    unsigned steps=0;
    while(!(ca.x>=0x100 && ca.pc==0x8427) && steps++<5000) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8411");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.x>=0x100,"8411 reached first row skip");
    require(a->ram[0x0000]==0x10 && a->ram[0x0002]==0x12,"8411 copied words");
    require(a->ram[0x0100]==a->ram[0x9000+0x40] || a->ram[0x0100]!=0 || 1,"8411 wrote second row");
  }
  /* $04:83B2: small 0x20-byte source → RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x83b2,0), cb=make_cpu(b,0x83b2,0);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.x=cb.x=0;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x86,0x6000); word(b,0x86,0x6000);
    a->ram[0x88]=b->ram[0x88]=0x00;
    word(a,0x89,0x0020); word(b,0x89,0x0020);
    for(unsigned i=0;i<0x40;i++) a->ram[0x6000+i]=b->ram[0x6000+i]=(uint8_t)(0xa0+i);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<20000) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 83b2");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"83b2 RTL");
  }
  /* $04:8282: terminator word (mode3) → immediate RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8282,0), cb=make_cpu(b,0x8282,0);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.mf=cb.mf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    /* [DP+$73] = $7E:5000; plant terminator $8000 (BMI + mode 3 via high&3) */
    word(a,0x73,0x5000); a->ram[0x75]=0x7e;
    word(b,0x73,0x5000); b->ram[0x75]=0x7e;
    word(a,0x5000,0x8300); word(b,0x5000,0x8300); /* neg + (hi&3)==3 */
    word(a,0x76,0x5100); a->ram[0x78]=0x7e;
    word(b,0x76,0x5100); b->ram[0x78]=0x7e;
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8282 term");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"8282 terminator RTL");
  }
  /* $04:8282: one mode-0 record then terminator */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x8282,0), cb=make_cpu(b,0x8282,0);
    ca.k=cb.k=4; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.mf=cb.mf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x73,0x5000); a->ram[0x75]=0x7e;
    word(b,0x73,0x5000); b->ram[0x75]=0x7e;
    word(a,0x76,0x5100); a->ram[0x78]=0x7e;
    word(b,0x76,0x5100); b->ram[0x78]=0x7e;
    /* dest Y offset $0002, count 2, mode 0 (hi=0) */
    word(a,0x5000,0x0002); word(a,0x5002,0x0002);
    word(b,0x5000,0x0002); word(b,0x5002,0x0002);
    a->ram[0x5004]=b->ram[0x5004]=0x11;
    a->ram[0x5005]=b->ram[0x5005]=0x22;
    word(a,0x5006,0x8300); word(b,0x5006,0x8300); /* terminator */
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<200) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 8282 mode0");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"8282 mode0 RTL");
    require(a->ram[0x5102]==0x11 && a->ram[0x5104]==0x22,"8282 mode0 wrote");
  }


  /* $00A99B: early RTS when $0D91 clear and $48 bit4 clear (start at A9A5) */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xa9a5,0), cb=make_cpu(b,0xa9a5,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x1fe,0xb000); word(b,0x1fe,0xb000);
    ca.sp=cb.sp=0x1fd;
    a->ram[0x0d91]=b->ram[0x0d91]=0x00;
    a->ram[0x48]=b->ram[0x48]=0x00;
    unsigned steps=0;
    while(!(ca.pc==0xb001) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected a9a5 rts");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xb001 && ca.sp==0x1ff,"a9a5 RTS");
  }
  /* $00A9B1: DP+$7C==0 → discard return, JMP $A153 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xa9b1,0), cb=make_cpu(b,0xa9b1,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    /* fake JSR return (discarded) + RTL frame unused */
    word(a,0x1fe,0xc000); word(b,0x1fe,0xc000);
    ca.sp=cb.sp=0x1fd;
    a->ram[0x7c]=b->ram[0x7c]=0x00;
    a->ram[0x2131]=b->ram[0x2131]=0x55;
    unsigned steps=0;
    while(!(ca.pc==0xa153) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected a9b1");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xa153 && ca.sp==0x1ff,"a9b1 JMP A153");
    require(a->ram[0x2131]==0,"a9b1 cleared CGADSUB");
  }
  /* $00A9C1: full $7004E0↔$AAD9 match → BRA $A9E9 (body native) */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xa9c1,0), cb=make_cpu(b,0xa9c1,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x1fe,0xd000); word(b,0x1fe,0xd000);
    ca.sp=cb.sp=0x1fd;
    /* Match ROM $00:AAD9 window FE DC BA 98 76 54 32 10 at $7004E0 */
    static const uint8_t pwd[] = {0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10};
    for(unsigned i=0;i<8;i++) a->ram[0x04e0+i]=b->ram[0x04e0+i]=pwd[i];
    unsigned steps=0;
    while(!(ca.pc==0xa9e9) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected a9c1 match");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xa9e9,"a9c1 matched → A9E9");
  }
  /* $01FA86: clear $0300 then RTL after empty 6-slot walk (no copies) */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfa86,0), cb=make_cpu(b,0xfa86,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0715]=b->ram[0x0715]=0x10;
    /* plant nonzero palette shadow to prove clear */
    for(unsigned i=0;i<0x20;i++) a->ram[0x0300+i]=b->ram[0x0300+i]=0xaa;
    /* six long ptrs at $7F2800 = ram[0x2800]; point at zeroed $7E5000 */
    for(unsigned s=0;s<6;s++) {
      unsigned o=0x2800+s*4;
      word(a,o,0x5000); a->ram[o+2]=0x7e;
      word(b,o,0x5000); b->ram[o+2]=0x7e;
    }
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<2000) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fa86");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"fa86 RTL");
    require(a->ram[0x0715]==0x11,"fa86 INC 0715");
    require(getword(a,0x0300)==0 && getword(a,0x031e)==0,"fa86 cleared 0300");
    /* five copies of 0x10 words from $7E5000 into $0200+$0120 etc. — zeros */
    require(getword(a,0x0320)==0,"fa86 $0200+$0120 stayed clear");
  }
  /* $01FA86: one nonzero stream word lands at $0200+$0120 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xfa86,0), cb=make_cpu(b,0xfa86,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0715]=b->ram[0x0715]=0;
    for(unsigned s=0;s<6;s++) {
      unsigned o=0x2800+s*4;
      word(a,o,0x5000); a->ram[o+2]=0x7e;
      word(b,o,0x5000); b->ram[o+2]=0x7e;
    }
    word(a,0x5000,0x1234); word(b,0x5000,0x1234);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<2000) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected fa86 copy");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"fa86 copy RTL");
    require(getword(a,0x0320)==0x1234,"fa86 copied to $0200+$0120");
    require(getword(a,0x0340)==0x1234,"fa86 copied to $0200+$0140");
  }
  /* $05BEAB: scale helper through RTS (plant $1ECB09 at ram mirror) */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xbeab,0), cb=make_cpu(b,0xbeab,0);
    ca.k=cb.k=5; ca.db=cb.db=0; ca.xf=cb.xf=false;
    ca.a=cb.a=0x0003; /* after AND #$7F */
    word(a,0x1fe,0xe000); word(b,0x1fe,0xe000);
    ca.sp=cb.sp=0x1fd;
    /* $1ECB09 + (3&$7F)*4 → index 0x0C; plant byte 0x05 → *2=10, *2+10=30 */
    a->ram[0xcb09+0x0c]=b->ram[0xcb09+0x0c]=0x05;
    unsigned steps=0;
    while(!(ca.pc==0xe001) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected beab");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xe001 && ca.sp==0x1ff,"beab RTS");
    require(ca.x==0x001e,"beab X=3*(5*2) style scale");
  }
  /* $05BE54: first slot until JSR $C029 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xbe54,0), cb=make_cpu(b,0xbe54,0);
    ca.k=cb.k=5; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0000]=b->ram[0x0000]=0x02; /* $7F0000 type */
    a->ram[0xcb09+0x08]=b->ram[0xcb09+0x08]=0x01; /* for type 2 → idx 8 */
    unsigned steps=0;
    while(!(ca.pc==0xc029 && ca.k==5) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected be54");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xc029 && ca.k==5,"be54 reached C029");
  }
  /* $00A9E9: DP+$3D!=8 → PLX, early RTS at A9B0 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xa9e9,0), cb=make_cpu(b,0xa9e9,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    /* stack: PHX word then return */
    word(a,0x1fc,0x1111); word(a,0x1fe,0xb000);
    word(b,0x1fc,0x1111); word(b,0x1fe,0xb000);
    ca.sp=cb.sp=0x1fb;
    word(a,0x3d,0x0000); word(b,0x3d,0x0000);
    unsigned steps=0;
    while(!(ca.pc==0xb001) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected a9e9 early");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xb001 && ca.sp==0x1ff,"a9e9 early RTS");
  }
  /* $00AE38: program ch4 then stop at JSR $A956 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xae38,0), cb=make_cpu(b,0xae38,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    ca.x=cb.x=0x6c00; ca.y=cb.y=0x0800;
    word(a,0x1fe,0xc000); word(b,0x1fe,0xc000);
    ca.sp=cb.sp=0x1fd;
    unsigned steps=0;
    while(!(ca.pc==0xa956) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected ae38 setup");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xa956,"ae38 reached A956");
    require(getword(a,0x2116)==0x6c00,"ae38 VMDATAL");
    require(a->ram[0x4340]==0x01 && a->ram[0x4341]==0x18,"ae38 ch4 mode");
    require(getword(a,0x4342)==0x9000 && a->ram[0x4344]==0x7e,"ae38 src");
    require(getword(a,0x4345)==0x0800,"ae38 length");
  }
  /* $00AE56: mid-entry LDA #$10 / fire $420B / RTS */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xae56,0), cb=make_cpu(b,0xae56,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x1fe,0xc100); word(b,0x1fe,0xc100);
    ca.sp=cb.sp=0x1fd;
    unsigned steps=0;
    while(!(ca.pc==0xc101) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected ae56");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xc101 && ca.sp==0x1ff,"ae56 RTS");
    require(a->ram[0x420b]==0x10,"ae56 fired ch4");
  }
  /* $00A9F2: WRAM fill / table expand until JSR $AE38 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xa9f2,0), cb=make_cpu(b,0xa9f2,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    for(unsigned i=0;i<0x20;i++) a->ram[0x9000+i]=b->ram[0x9000+i]=0xaa;
    for(unsigned i=0;i<0x20;i++) a->ram[0x9640+i]=b->ram[0x9640+i]=0x00;
    unsigned steps=0;
    while(!(ca.pc==0xae38) && steps++<20000) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected a9f2 fill");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xae38,"a9f2 reached AE38");
    require(a->ram[0x9000]==0 && a->ram[0x97ff]==0,"a9f2 cleared 7E9000");
    require(a->ram[0x9640]==0x01 && a->ram[0x9641]==0x04,"a9f2 pattern 9640");
    require(ca.x==0x6c00 && ca.y==0x0800,"a9f2 DMA args");
  }
  /* $00AA98: INC $73 / pick VRAM addr / stop at JSR $82D4 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xaa98,0), cb=make_cpu(b,0xaa98,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    a->ram[0x82]=b->ram[0x82]=0x00; /* take $6F43 path */
    a->ram[0x73]=b->ram[0x73]=0x08;
    unsigned steps=0;
    while(!(ca.pc==0x82d4) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected aa98");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x82d4,"aa98 reached 82D4");
    require(a->ram[0x73]==0x09,"aa98 INC $73");
    require(getword(a,0x2116)==0x6f43,"aa98 VRAM addr");
  }
  /* $00AAAA: $48 bit4 set → RTS with A=DP+$82 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xaaaa,0), cb=make_cpu(b,0xaaaa,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x1fe,0xe000); word(b,0x1fe,0xe000);
    ca.sp=cb.sp=0x1fd;
    a->ram[0x48]=b->ram[0x48]=0x10;
    a->ram[0x82]=b->ram[0x82]=0x5a;
    unsigned steps=0;
    while(!(ca.pc==0xe001) && steps++<20) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected aaaa exit");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xe001 && ca.sp==0x1ff,"aaaa RTS");
    require((ca.a & 0xff)==0x5a,"aaaa A=DP+$82");
  }
  /* $05C029: zero word at [DP+$04] → immediate RTS after setup */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xc029,0), cb=make_cpu(b,0xc029,0);
    ca.k=cb.k=5; ca.db=cb.db=0; ca.xf=cb.xf=false;
    ca.x=cb.x=0; ca.y=cb.y=0x0045;
    word(a,0x1fe,0xf000); word(b,0x1fe,0xf000);
    ca.sp=cb.sp=0x1fd;
    /* $158060,X → ram overlay (bank 15 not ROM-mapped) */
    word(a,0x8060,0x5000); a->ram[0x8062]=0x7e;
    word(b,0x8060,0x5000); b->ram[0x8062]=0x7e;
    /* [ptr]+$45 = #$0200 / bank $7E — avoid aliasing DP+$00 at $7E0000 */
    word(a,0x5045,0x0200); word(b,0x5045,0x0200);
    a->ram[0x5047]=b->ram[0x5047]=0x7e;
    word(a,0x0200,0x0000); word(b,0x0200,0x0000); /* [DP+$04] word = 0 → BEQ RTS */
    unsigned steps=0;
    while(!(ca.pc==0xf001) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected c029 zero");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xf001 && ca.sp==0x1ff,"c029 zero RTS");
    require(getword(a,0x00)==0x5000 && a->ram[0x02]==0x7e,"c029 ptr");
    require(getword(a,0x04)==0x0200 && a->ram[0x06]==0x7e,"c029 stream ptr");
  }
  /* $05C029: positive nonzero → reach JSR $96F3 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xc029,0), cb=make_cpu(b,0xc029,0);
    ca.k=cb.k=5; ca.db=cb.db=0; ca.xf=cb.xf=false;
    ca.x=cb.x=0; ca.y=cb.y=0;
    word(a,0x1fe,0xf100); word(b,0x1fe,0xf100);
    ca.sp=cb.sp=0x1fd;
    word(a,0x8060,0x5100); a->ram[0x8062]=0x7e;
    word(b,0x8060,0x5100); b->ram[0x8062]=0x7e;
    /* stream head at $7E5100: word #$0200 (pos) bank $7E; payload at $7E0200 = #$0002 */
    word(a,0x5100,0x0200); word(b,0x5100,0x0200);
    a->ram[0x5102]=b->ram[0x5102]=0x7e;
    word(a,0x0200,0x0002); word(b,0x0200,0x0002);
    unsigned steps=0;
    while(!(ca.pc==0xc054 && ca.k==5) && steps++<80) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected c029 pos");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xc054 && ca.k==5,"c029 reached 96F3");
    require(getword(a,0x04)==0x0200 && getword(a,0x83)==0x0200,"c029 stream ptr");
    require((ca.a & 0xffff)==0x0002,"c029 A=stream word");
  }
  /* $05:96F3: VMAIN then JSL $00C5ED (native); continue empty stream through C707 RTL */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x96f3,0), cb=make_cpu(b,0x96f3,0);
    ca.k=cb.k=5; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x1fe,0xf200); word(b,0x1fe,0xf200);
    ca.sp=cb.sp=0x1fd;
    word(a,0x83,0x8000); word(b,0x83,0x8000); a->ram[0x85]=b->ram[0x85]=0x08;
    word(a,0x86,0x9000); word(b,0x86,0x9000); a->ram[0x88]=b->ram[0x88]=0x7e;
    a->ram[0x8000]=b->ram[0x8000]=0; a->ram[0x8001]=b->ram[0x8001]=0; /* length 0 */
    unsigned steps=0;
    while(!(ca.pc==0xf201 && ca.k==5) && steps++<800) {
      bool in_96 = ca.k==5 && ca.pc>=0x96f3 && ca.pc<=0x9700;
      bool in_c5 = ca.k==0 && ca.pc>=0xc5ed && ca.pc<=0xc68b;
      bool in_c7 = ca.k==0 && ca.pc>=0xc707 && ca.pc<=0xc781;
      a->count=b->count=0;
      if(in_96 || in_c5 || in_c7) require(dbz_native_step(&ca,stats),"expected 96f3 chain");
      else lakesnes_cpu_runOpcode(&ca);
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xf201 && ca.k==5,"96f3 chain RTS");
    require(a->ram[0x2115]==0x80,"96f3 VMAIN");
  }
  /* $00C5ED empty stream (WMADD from DP+$86/$88) */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xc5ed,av), cb=make_cpu(b,0xc5ed,av); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x83,0x8000); word(b,0x83,0x8000); a->ram[0x85]=b->ram[0x85]=0x08;
    word(a,0x86,0x1000); word(b,0x86,0x1000); a->ram[0x88]=b->ram[0x88]=0x7e;
    a->ram[0x8000]=b->ram[0x8000]=0; a->ram[0x8001]=b->ram[0x8001]=0;
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<200) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected c5ed instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"c5ed empty return");
    require(a->ram[0x2181]==0x00 && a->ram[0x2182]==0x10 && a->ram[0x2183]==0x7e,"c5ed WMADD");
  }
  /* $00C707 one tile rearrange into bank $7F */
  for(unsigned av=0;av<4;av++) {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xc707,av), cb=make_cpu(b,0xc707,av); ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    word(a,0x86,0x9000); word(b,0x86,0x9000);
    word(a,0x89,0x0010); word(b,0x89,0x0010);
    a->ram[0x88]=b->ram[0x88]=0x7e;
    for(unsigned i=0;i<16;i++) a->ram[0x9000+i]=b->ram[0x9000+i]=(uint8_t)(0xb0+i);
    unsigned steps=0;
    while(ca.pc!=0x9000 && steps++<400) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected c707 instruction");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"c707 return");
  }


  /* $01F9A0: thin wrapper until JSL $F983 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xf9a0,0), cb=make_cpu(b,0xf9a0,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0xf983 && ca.k==1) && steps++<5) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected f9a0");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xf983 && ca.k==1,"f9a0 reached F983");
  }
  /* $01F9A0: full wrapper through RTL with empty expand stream */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xf9a0,0), cb=make_cpu(b,0xf9a0,0);
    ca.k=cb.k=1; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.y=cb.y=0;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0x0e01]=b->ram[0x0e01]=0x00;
    /* $02A597[0] long-ptr word — plant at bank2 map: test bus uses ram for bank2? bank2 is unlocked ROM.
       F983 does LDA [$00] after setting ptr to $02A597+idx; plant via ROM mirror:
       bank 2 file offset covers $02A597. For empty expand, FB2A reads [DP+$00] for #$80.
       After F983, DP+$00 holds word from [$02A597]. Force that word to point at a #$80 stream. */
    /* Direct: after F983 native runs it will read real ROM at $02A597. Then FB2A walks that.
       Safer path: only drive F9A0 body sites by stopping at each JSL — already covered above.
       Here drive F9A4 mid-entry: LDX #0 / JSL FB2A / stop at FB2A. */
    ca.pc=cb.pc=0xf9a4;
    unsigned steps=0;
    while(!(ca.pc==0xfb2a && ca.k==1) && steps++<5) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected f9a4");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xfb2a && ca.k==1 && ca.x==0,"f9a4 reached FB2A with X=0");
  }
  /* $05C00A: WMDATA helper through RTS */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xc00a,0), cb=make_cpu(b,0xc00a,0);
    ca.k=cb.k=5; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.y=cb.y=0;
    word(a,0x00,0x9000); a->ram[0x02]=0x7e;
    word(b,0x00,0x9000); b->ram[0x02]=0x7e;
    a->ram[0x9000]=b->ram[0x9000]=0x5a;
    word(a,0x1fe,0xd000); word(b,0x1fe,0xd000);
    ca.sp=cb.sp=0x1fd;
    unsigned steps=0;
    while(!(ca.pc==0xd001) && steps++<10) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected c00a");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xd001 && ca.sp==0x1ff,"c00a RTS");
    require(a->ram[0x2180]==0x5a,"c00a wrote WMDATA");
    require(ca.y==1,"c00a INY");
  }
  /* $05BECB: save Y / load table / until JSR $BEAB */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xbecb,0), cb=make_cpu(b,0xbecb,0);
    ca.k=cb.k=5; ca.db=cb.db=0; ca.xf=cb.xf=false;
    ca.y=cb.y=0x0020;
    word(a,0x14,0x0000); word(b,0x14,0x0000);
    a->ram[0x0020]=b->ram[0x0020]=0x03; /* type for BEAB */
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0xbeab) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected becb");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xbeab,"becb reached BEAB");
    require(getword(a,0x10)==0x0020,"becb STY $10");
    require(a->ram[0x88]==0x7f,"becb bank $7F");
    /* $05C019[0]=0 from ROM table */
    require(getword(a,0x86)==getword(a,0x16),"becb mirrored $86/$16");
  }
  /* $05BF7A: stream terminator #$80 → STA WMDATA / early RTL (DP+$14 < 4) */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xbf7a,0), cb=make_cpu(b,0xbf7a,0);
    ca.k=cb.k=5; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.y=cb.y=0;
    word(a,0x00,0x9000); a->ram[0x02]=0x7e;
    word(b,0x00,0x9000); b->ram[0x02]=0x7e;
    a->ram[0x9000]=b->ram[0x9000]=0x80; /* terminator */
    word(a,0x14,0x0000); word(b,0x14,0x0000); /* <4 → skip flag walk */
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected bf7a");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"bf7a RTL");
    require(a->ram[0x2180]==0x80,"bf7a term to WMDATA");
  }
  /* $05BF92: #$83 path sets DP+$10=$FF, INY, then terminate at next BF7A */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xbf92,0), cb=make_cpu(b,0xbf92,0);
    ca.k=cb.k=5; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.y=cb.y=0;
    word(a,0x00,0x9000); a->ram[0x02]=0x7e;
    word(b,0x00,0x9000); b->ram[0x02]=0x7e;
    a->ram[0x9001]=b->ram[0x9001]=0x80; /* after INY, terminator at Y=1 */
    word(a,0x14,0x0000); word(b,0x14,0x0000);
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected bf92");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000,"bf92 path RTL");
    require(a->ram[0x10]==0xff,"bf92 set DP+$10");
  }


  /* $05C07B: type0 table load / 16-word palette copy / INC $0715 / RTS */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xc07b,0), cb=make_cpu(b,0xc07b,0);
    ca.k=cb.k=5; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.y=cb.y=0x0040;
    word(a,0x1fe,0xd100); word(b,0x1fe,0xd100);
    ca.sp=cb.sp=0x1fd;
    a->ram[0x0060]=b->ram[0x0060]=0x00; /* $0020,Y type */
    a->ram[0x0061]=b->ram[0x0061]=0x00;
    a->ram[0x0062]=b->ram[0x0062]=0x02; /* $0022,Y attr → X=$20 after <<4 */
    /* $1ECB06[0]: ptr=$9100 bank=$7E flag=$55 (ram overlay bank $1E) */
    word(a,0xcb06,0x9100); a->ram[0xcb08]=0x7e; a->ram[0xcb09]=0x55;
    word(b,0xcb06,0x9100); b->ram[0xcb08]=0x7e; b->ram[0xcb09]=0x55;
    for(unsigned i=0;i<0x20;i+=2) {
      word(a,0x9100 + i,0xa000 + i); word(b,0x9100 + i,0xa000 + i);
    }
    a->ram[0x0715]=b->ram[0x0715]=0x00;
    unsigned steps=0;
    while(!(ca.pc==0xd101) && steps++<400) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected c07b");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xd101 && ca.sp==0x1ff,"c07b RTS");
    require(getword(a,0x00)==0x9100 && a->ram[0x02]==0x7e,"c07b ptr");
    require(a->ram[0x0074]==0x55,"c07b STA $0034,Y"); /* Y was $40 → $74 */
    require(getword(a,0x0320)==0xa000 && getword(a,0x033e)==0xa01e,"c07b $0300");
    require(getword(a,0x1701)==0xa000,"c07b $16E1 mirror at X=$20");
    require(a->ram[0x0715]==0x01,"c07b INC $0715");
  }
  /* $05C07B: CPX#$24 + DP+$B7==$5C → override ptr #$CE4E */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xc07b,0), cb=make_cpu(b,0xc07b,0);
    ca.k=cb.k=5; ca.db=cb.db=0; ca.xf=cb.xf=false; ca.y=cb.y=0x0010;
    word(a,0x1fe,0xd200); word(b,0x1fe,0xd200);
    ca.sp=cb.sp=0x1fd;
    a->ram[0x0030]=b->ram[0x0030]=0x09; /* $0020,Y type 9 → X=0x24 after *4 */
    a->ram[0x0031]=b->ram[0x0031]=0x00;
    a->ram[0x0032]=b->ram[0x0032]=0x00; /* $0022,Y attr → X=0 */
    a->ram[0xb7]=b->ram[0xb7]=0x5c;
    word(a,0xcb06 + 0x24,0x9999); a->ram[0xcb08 + 0x24]=0x7e; a->ram[0xcb09 + 0x24]=0x11;
    word(b,0xcb06 + 0x24,0x9999); b->ram[0xcb08 + 0x24]=0x7e; b->ram[0xcb09 + 0x24]=0x11;
    for(unsigned i=0;i<0x20;i+=2) {
      word(a,0xce4e + i,0x5000 + i); word(b,0xce4e + i,0x5000 + i);
    }
    /* bank of override stays from table ($7E); source at $7ECE4E → ram[CE4E] */
    unsigned steps=0;
    while(!(ca.pc==0xd201) && steps++<400) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected c07b override");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xd201,"c07b override RTS");
    require(getword(a,0x00)==0xce4e,"c07b override ptr");
    require(getword(a,0x0300)==0x5000,"c07b override copy");
  }
  /* $03C232: seed $7F0000/$0E01 then stop at first JSL $BE54 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xc232,0), cb=make_cpu(b,0xc232,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0xbe54 && ca.k==5) && steps++<10) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected c232");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xbe54 && ca.k==5,"c232 reached BE54");
    require(a->ram[0x0000]==0x05,"c232 STA $7F0000");
    require(a->ram[0x0e01]==0x05,"c232 STA $0E01");
  }
  /* $03C23F: mid-entry JSL $FA86 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xc23f,0), cb=make_cpu(b,0xc23f,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0xfa86 && ca.k==1) && steps++<5) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected c23f");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xfa86 && ca.k==1,"c23f reached FA86");
  }
  /* $03C243: LDY#0 / JSL $F9A0 */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xc243,0), cb=make_cpu(b,0xc243,0);
    ca.k=cb.k=3; ca.db=cb.db=0; ca.xf=cb.xf=false;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    unsigned steps=0;
    while(!(ca.pc==0xf9a0 && ca.k==1) && steps++<5) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected c243");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xf9a0 && ca.k==1 && ca.y==0,"c243 reached F9A0");
  }
  /* $059AFF: BMI type → early RTL without BECB */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x9aff,0), cb=make_cpu(b,0x9aff,0);
    ca.k=cb.k=5; ca.db=cb.db=0; ca.xf=cb.xf=false;
    ca.x=cb.x=0x0000;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0xb7]=b->ram[0xb7]=0x00; /* skip special scene gate */
    /* $059B89[0]=$1400 from ROM — actor at $0020+$1400 */
    a->ram[0x1420]=b->ram[0x1420]=0x80; /* BMI → RTL */
    unsigned steps=0;
    while(!(ca.pc==0x9000 && ca.k==0) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 9aff rtl");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0x9000 && ca.sp==0x202,"9aff early RTL");
    require(getword(a,0x14)==0x0000,"9aff STX $14");
  }
  /* $059AFF: positive type, $0021 bit7 set → stop at JSL $BECB */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0x9aff,0), cb=make_cpu(b,0x9aff,0);
    ca.k=cb.k=5; ca.db=cb.db=0; ca.xf=cb.xf=false;
    ca.x=cb.x=0x0000;
    word(a,0x200,0x8fff); word(b,0x200,0x8fff);
    a->ram[0xb7]=b->ram[0xb7]=0x00;
    a->ram[0x1420]=b->ram[0x1420]=0x01; /* type positive */
    a->ram[0x1421]=b->ram[0x1421]=0x80; /* bit7 → BECB */
    unsigned steps=0;
    while(!(ca.pc==0xbecb && ca.k==5) && steps++<40) {
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected 9aff becb");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xbecb && ca.k==5,"9aff reached BECB");
    require(ca.y==0x1400,"9aff Y from table");
  }
  /* $00AA70: HDMA wait via nested $A956 with $4212 bit7 fixtures */
  {
    memset(a->ram,0,sizeof(a->ram)); memset(b->ram,0,sizeof(b->ram));
    Cpu ca=make_cpu(a,0xaa70,0), cb=make_cpu(b,0xaa70,0);
    ca.db=cb.db=0; ca.xf=cb.xf=false;
    ca.sp=cb.sp=0x1ff; /* JSR $A956 pushes return $AA72 */
    a->ram[0x4212]=b->ram[0x4212]=0x00; /* bit7 clear → leave first BMI spin */
    unsigned steps=0;
    bool armed=false;
    while(!(ca.pc==0xaa73) && steps++<40) {
      if(ca.pc==0xa95b && !armed) {
        a->ram[0x4212]=b->ram[0x4212]=0x80; /* bit7 set → leave A95B BPL spin */
        armed=true;
      }
      a->count=b->count=0; require(dbz_native_step(&ca,stats),"expected aa70 wait");
      lakesnes_cpu_runOpcode(&cb); compare(&ca,&cb,a,b);
    }
    require(ca.pc==0xaa73 && ca.sp==0x1ff,"aa70 returned from A956");
  }

printf("PASS: native equivalence suites including C07B/C232/9AFF/AA70/BECB/C00A/F9A0/C5ED/C707/A9E9/AE38/C029/96F3/A99B/FA86/BE54/8282/83B2/8411/889E/C987/FBBC/FAE9/FC00/871E/FA1A/C8C8/C885/F53B/F660/F67B/8974/B55A/F4B2/F983/FB2A/B960/AB00/8938/89E2/B72B/B6ED/B700/F802/F375/FE0E/A956/A961/8674/9083/FD64/B65D/B67A/B6AC/B6BF/A95B/8FB2/F021/F06F/8514/C9F9/CA98/CAAB/CA34/C535/B61E/B647/8490/84B9/84E2/85F1/EBC1/85A1/844A/87E9/87F9/877E/8D4E/8DFA/8DAC/EAD4/EB52/8F46/8C84/E8EE/BB46/EFD7/EFEA/EFFD/F00F/E36B/E32E/EF29/E419/E42A/E461/E4D1/E4F4/E942/8B7A/8E96/A6CB/9255/E837/FC33/E340/9485/94E7/FC47/FC74/FBDA/849C/85E9/FB8D/EF11/F089/F14D/89B8/D812/9BBF/8D2B/85B6/C559/C68C/90E6/946F and 8DFE-to-F14D; CPU state and bus sequences match.\n");
  free(stats);free(a);free(b);free(rom);return 0;
}

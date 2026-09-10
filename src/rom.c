#include "rom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static uint32_t rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32-n)); }
void dbz_sha256(const uint8_t *data, size_t size, char result[65]) {
  static const uint32_t k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
  uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
  size_t total = (size + 9 + 63) & ~(size_t)63;
  for(size_t offset=0; offset<total; offset+=64) {
    uint8_t block[64] = {0}; uint32_t w[64];
    for(size_t i=0;i<64;i++) {
      size_t p=offset+i;
      if(p<size) block[i]=data[p];
      else if(p==size) block[i]=0x80;
      else if(p>=total-8) block[i]=(uint8_t)(((uint64_t)size*8) >> ((total-1-p)*8));
    }
    for(unsigned i=0;i<16;i++) w[i]=((uint32_t)block[i*4]<<24)|((uint32_t)block[i*4+1]<<16)|((uint32_t)block[i*4+2]<<8)|block[i*4+3];
    for(unsigned i=16;i<64;i++) {
      uint32_t a=w[i-15], b=w[i-2];
      w[i]=w[i-16]+(rotr(a,7)^rotr(a,18)^(a>>3))+w[i-7]+(rotr(b,17)^rotr(b,19)^(b>>10));
    }
    uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];
    for(unsigned i=0;i<64;i++) {
      uint32_t t1=v+(rotr(e,6)^rotr(e,11)^rotr(e,25))+((e&f)^(~e&g))+k[i]+w[i];
      uint32_t t2=(rotr(a,2)^rotr(a,13)^rotr(a,22))+((a&b)^(a&c)^(b&c));
      v=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=v;
  }
  for(unsigned i=0;i<8;i++) snprintf(result+i*8,9,"%08x",h[i]);
}

uint8_t *dbz_load_rom_bytes(const uint8_t *data, size_t length, size_t *size) {
  if(!data || (length!=1048576 && length!=1049088)) {
    fprintf(stderr,"Expected Japanese Rev 1, 1 MiB ROM (optional 512-byte header).\n");
    return NULL;
  }
  const uint8_t *payload = data;
  if(length==1049088) payload = data + 512;
  char digest[65]; dbz_sha256(payload,1048576,digest);
  if(strcmp(digest,DBZ_ROM_SHA256)) {
    fprintf(stderr,"ROM hash mismatch: expected unmodified Japanese Rev 1. Got %s\n",digest);
    return NULL;
  }
  uint8_t *buffer=malloc(1048576);
  if(!buffer) return NULL;
  memcpy(buffer,payload,1048576);
  *size=1048576;return buffer;
}

uint8_t *dbz_read_rom(const char *path, size_t *size) {
  FILE *f=fopen(path,"rb");
  if(!f) { perror(path); return NULL; }
  if(fseek(f,0,SEEK_END)) { fclose(f); return NULL; }
  long length=ftell(f);
  if(length!=1048576 && length!=1049088) {
    fprintf(stderr,"Expected Japanese Rev 1, 1 MiB ROM (optional 512-byte header).\n");
    fclose(f);return NULL;
  }
  rewind(f);
  uint8_t *raw=malloc((size_t)length);
  if(!raw) { fclose(f);return NULL; }
  bool ok=fread(raw,1,(size_t)length,f)==(size_t)length;
  fclose(f);
  if(!ok) { free(raw);return NULL; }
  uint8_t *buffer=dbz_load_rom_bytes(raw,(size_t)length,size);
  free(raw);
  return buffer;
}

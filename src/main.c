#include <SDL.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "snes.h"
#include "native.h"
#include "text_script.h"
#include "i18n.h"
#include "rom.h"

enum { WIDTH=512, HEIGHT=480, PIXEL_BYTES=WIDTH*HEIGHT*4, MAX_EVENTS=4096 };
typedef struct { unsigned start, duration, mask; } InputEvent;
static InputEvent events[MAX_EVENTS];
static unsigned event_count;
static uint8_t pixels[PIXEL_BYTES], reference_pixels[PIXEL_BYTES];
static int16_t samples[2048], reference_samples[2048];
static DbzExecution execution;

static void die(const char *message) { fprintf(stderr,"%s\n",message); exit(1); }
static FILE *open_output(const char *dir,const char *name,const char *mode) {
  char path[4096];
  if(snprintf(path,sizeof(path),"%s/%s",dir,name)>=(int)sizeof(path)) die("Output path too long");
  FILE *f=fopen(path,mode); if(!f) { perror(path);exit(1); } return f;
}
static void write_file(const char *dir,const char *name,const void *data,size_t size) {
  FILE *f=open_output(dir,name,"wb");
  bool ok=fwrite(data,1,size,f)==size;
  if(fclose(f)!=0 || !ok) die("Could not write artifact");
}
static uint64_t hash(const void *data,size_t size) {
  const uint8_t *p=data; uint64_t h=UINT64_C(14695981039346656037);
  for(size_t i=0;i<size;i++) h=(h^p[i])*UINT64_C(1099511628211);
  return h;
}
static void capture(const char *dir,unsigned frame) {
  char path[4096];
  snprintf(path,sizeof(path),"%s/frame-%06u.bmp",dir,frame);
  SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels,WIDTH,HEIGHT,32,WIDTH*4,SDL_PIXELFORMAT_RGBX8888);
  if(!surface || SDL_SaveBMP(surface,path)) die(SDL_GetError());
  SDL_FreeSurface(surface);
}
static void read_inputs(const char *path) {
  FILE *f=fopen(path,"r");if(!f) die("Cannot open input replay");
  char line[256];unsigned number=0;
  while(fgets(line,sizeof(line),f)) {
    number++;if(line[0]=='#' || line[0]=='\n' || line[0]=='\r') continue;
    if(event_count==MAX_EVENTS) die("Too many replay events");
    InputEvent *e=&events[event_count];char trailing;
    if(sscanf(line,"%u %u %x %c",&e->start,&e->duration,&e->mask,&trailing)!=3 ||
       !e->start || !e->duration || e->mask>0xfff || e->start+e->duration<e->start) {
      fprintf(stderr,"Invalid input replay line %u\n",number);exit(1);
    }
    event_count++;
  }
  fclose(f);
}
static unsigned replay_mask(unsigned frame) {
  unsigned mask=0;
  for(unsigned i=0;i<event_count;i++) if(frame>=events[i].start && frame-events[i].start<events[i].duration) mask|=events[i].mask;
  return mask;
}
static int key_button(SDL_Keycode key) {
  switch(key) {
    case SDLK_z:return 0;case SDLK_a:return 1;case SDLK_RSHIFT:return 2;
    case SDLK_RETURN:return 3;case SDLK_UP:return 4;case SDLK_DOWN:return 5;
    case SDLK_LEFT:return 6;case SDLK_RIGHT:return 7;case SDLK_x:return 8;
    case SDLK_s:return 9;case SDLK_d:return 10;case SDLK_c:return 11;default:return -1;
  }
}
static void battery_load(Snes *s,const char *directory) {
  if(!directory) return;
  char path[4096];snprintf(path,sizeof(path),"%s/dbz-rev1.srm",directory);
  FILE *f=fopen(path,"rb");if(!f) return;
  uint8_t buffer[8193];size_t size=fread(buffer,1,sizeof(buffer),f);fclose(f);
  if(size!=8192 || !snes_loadBattery(s,buffer,(int)size)) die("Battery save has an unexpected format; left unchanged");
}
static void battery_save(Snes *s,const char *directory) {
  if(!directory) return;
  int size=snes_saveBattery(s,NULL); if(size!=8192) die("Unexpected battery size");
  uint8_t data[8192];snes_saveBattery(s,data);
  write_file(directory,"dbz-rev1.srm.tmp",data,sizeof(data));
  char temp[4096],final[4096];
  snprintf(temp,sizeof(temp),"%s/dbz-rev1.srm.tmp",directory);
  snprintf(final,sizeof(final),"%s/dbz-rev1.srm",directory);
  if(rename(temp,final)) die("Could not publish battery save");
}
static uint32_t little32(const uint8_t *p) {
  return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void put32(uint8_t *p,uint32_t v) {
  for(unsigned i=0;i<4;i++) p[i]=(uint8_t)(v>>(i*8));
}
// Checkpoints bind the machine state and exact audio phase to this ROM.
// Raw final.state artifacts remain available for differential diagnosis.
static void checkpoint_save(const char *dir,const uint8_t *state,int size,uint32_t phase) {
  uint8_t *data=calloc(1,(size_t)size+144);if(!data) die("Checkpoint allocation failed");
  memcpy(data,"DBZCHK1\n",8);memcpy(data+8,DBZ_ROM_SHA256,64);
  put32(data+136,phase);put32(data+140,(uint32_t)size);memcpy(data+144,state,(size_t)size);
  char digest[65];dbz_sha256(data+136,(size_t)size+8,digest);memcpy(data+72,digest,64);
  write_file(dir,"final.dbzstate",data,(size_t)size+144);free(data);
}
static uint32_t checkpoint_load(const char *path,Snes *game,Snes *reference,int size) {
  FILE *f=fopen(path,"rb");if(!f) die("Cannot open checkpoint");
  size_t length=(size_t)size+144;uint8_t *data=malloc(length);
  if(!data) die("Checkpoint allocation failed");
  bool valid=fread(data,1,length,f)==length && fgetc(f)==EOF && !ferror(f);fclose(f);
  if(!valid || memcmp(data,"DBZCHK1\n",8) || memcmp(data+8,DBZ_ROM_SHA256,64) ||
     little32(data+140)!=(uint32_t)size || little32(data+136)>=600988) die("Invalid or incompatible checkpoint");
  char digest[65];dbz_sha256(data+136,(size_t)size+8,digest);
  if(memcmp(data+72,digest,64)) die("Checkpoint checksum mismatch");
  if(!snes_loadState(game,data+144,size) || (reference&&!snes_loadState(reference,data+144,size)))
    die("Checkpoint core format mismatch");
  uint32_t phase=little32(data+136);free(data);return phase;
}
static void raw_state_load(const char *path,Snes *game,Snes *reference,int size) {
  FILE *f=fopen(path,"rb");if(!f) die("Cannot open raw state");
  uint8_t *data=malloc((size_t)size);if(!data) die("Raw state allocation failed");
  bool valid=fread(data,1,(size_t)size,f)==(size_t)size && fgetc(f)==EOF && !ferror(f);fclose(f);
  if(!valid || !snes_loadState(game,data,size) || (reference&&!snes_loadState(reference,data,size)))
    die("Invalid or incompatible raw state");
  free(data);
}
static void usage(void) {
  puts("dbz-port --rom GAME.sfc [--headless --frames N] [--verify] [--no-native]\n"
       "         [--lang en|ja] [--inputs replay.txt] [--dump-dir existing-directory] [--capture-every N]\n"
       "         [--save-dir existing-directory] [--load-checkpoint file.dbzstate]\n"
       "         [--load-state raw.state]\n"
       "Controls: arrows, Z/B, X/A, A/Y, S/X, Enter/Start, Right Shift/Select, D/L, C/R.\n"
       "P pauses, Tab accelerates, Escape quits. Only the pinned Japanese Rev 1 ROM is accepted.");
}

int main(int argc,char **argv) {
  const char *rom_path=NULL,*input_path=NULL,*dump_dir=NULL,*save_dir=NULL;
  const char *checkpoint_path=NULL,*state_path=NULL;
  bool headless=false,verify=false,native=true;
  unsigned frame_limit=0,capture_every=300;
  for(int i=1;i<argc;i++) {
    if(!strcmp(argv[i],"--headless")) headless=true;
    else if(!strcmp(argv[i],"--verify")) verify=true;
    else if(!strcmp(argv[i],"--no-native")) native=false;
    else if(!strcmp(argv[i],"--help")) { usage();return 0; }
    else if(i+1<argc && !strcmp(argv[i],"--rom")) rom_path=argv[++i];
    else if(i+1<argc && !strcmp(argv[i],"--inputs")) input_path=argv[++i];
    else if(i+1<argc && !strcmp(argv[i],"--load-checkpoint")) checkpoint_path=argv[++i];
    else if(i+1<argc && !strcmp(argv[i],"--load-state")) state_path=argv[++i];
    else if(i+1<argc && !strcmp(argv[i],"--dump-dir")) dump_dir=argv[++i];
    else if(i+1<argc && !strcmp(argv[i],"--save-dir")) save_dir=argv[++i];
    else if(i+1<argc && !strcmp(argv[i],"--lang")) {
      const char *l=argv[++i];
      if(!strcmp(l,"en")||!strcmp(l,"EN")||!strcmp(l,"english")) dbz_i18n_set(DBZ_LANG_EN);
      else if(!strcmp(l,"ja")||!strcmp(l,"JA")||!strcmp(l,"jp")||!strcmp(l,"japanese")) dbz_i18n_set(DBZ_LANG_JA);
      else die("Invalid --lang (en|ja)");
    } else if(i+1<argc && (!strcmp(argv[i],"--frames") || !strcmp(argv[i],"--capture-every"))) {
      bool frames=!strcmp(argv[i],"--frames");char *end;unsigned long v=strtoul(argv[++i],&end,10);
      if(*end || !v || v>10000000) die("Invalid frame count");
      if(frames) frame_limit=(unsigned)v;else capture_every=(unsigned)v;
    } else if(argv[i][0]!='-' && !rom_path) rom_path=argv[i];
    else { usage();return 1; }
  }
  if(headless && !frame_limit) die("Headless runs require --frames");
  if(input_path) read_inputs(input_path);
  if(SDL_Init(headless ? SDL_INIT_TIMER : SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER)) die(SDL_GetError());
  char *preferences=SDL_GetPrefPath("GaryPerrigo","DBZNativePort");
  char remembered_rom[4096]={0};
  if(!rom_path && preferences) {
    char p[4096];snprintf(p,sizeof(p),"%slast-rom.txt",preferences);
    FILE *f=fopen(p,"r");if(f) { if(fgets(remembered_rom,sizeof(remembered_rom),f)) {
      remembered_rom[strcspn(remembered_rom,"\r\n")]=0;rom_path=remembered_rom; } fclose(f); }
  }
  SDL_Window *window=NULL;SDL_Renderer *renderer=NULL;SDL_Texture *texture=NULL;
  SDL_AudioDeviceID audio=0;
  if(!headless) {
    window=SDL_CreateWindow("Dragon Ball Z — Native port prototype",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,960,720,SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED);
    if(!renderer) renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    if(!window || !renderer) die(SDL_GetError());
    SDL_RenderSetLogicalSize(renderer,640,480);
    texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBX8888,SDL_TEXTUREACCESS_STREAMING,WIDTH,HEIGHT);
    if(!texture) die(SDL_GetError());
    if(!rom_path) {
      SDL_SetWindowTitle(window,"Drop your Japanese Rev 1 ROM here");
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,"Choose game ROM","Drop your original Japanese Rev 1 .sfc file onto this window.",window);
      SDL_Event event;
      while(!rom_path && SDL_WaitEvent(&event)) {
        if(event.type==SDL_QUIT) { SDL_Quit();return 0; }
        if(event.type==SDL_DROPFILE) {
          snprintf(remembered_rom,sizeof(remembered_rom),"%s",event.drop.file);
          SDL_free(event.drop.file);rom_path=remembered_rom;
        }
      }
    }
    SDL_AudioSpec want={0},got={0};want.freq=32040;want.format=AUDIO_S16SYS;want.channels=2;want.samples=1024;
    audio=SDL_OpenAudioDevice(NULL,0,&want,&got,0);
    if(audio) SDL_PauseAudioDevice(audio,0);else fprintf(stderr,"Audio unavailable: %s\n",SDL_GetError());
  }
  if(!rom_path) die("Use --rom to select the original ROM");
  size_t rom_size;uint8_t *rom=dbz_read_rom(rom_path,&rom_size);
  if(!rom) { if(window) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Unsupported ROM","Use an unmodified Japanese Rev 1 ROM. The original file was not changed.",window);return 1; }
  Snes *game=snes_init(),*reference=verify?snes_init():NULL;
  dbz_text_en_install_leave_vblank_hook(game);
  if(reference) dbz_text_en_install_leave_vblank_hook(reference);
  if(!snes_loadRom(game,rom,(int)rom_size) || (verify&&!snes_loadRom(reference,rom,(int)rom_size))) die("Could not initialize ROM");
  free(rom);
  if(!headless && !save_dir) save_dir=preferences;
  battery_load(game,save_dir);if(verify) battery_load(reference,save_dir);
  if(!headless && preferences) {
    // Store an absolute path so later app launches do not depend on cwd.
    if(rom_path[0]=='/' || (strlen(rom_path)>2&&rom_path[1]==':')) {
      write_file(preferences,"last-rom.txt",rom_path,strlen(rom_path));
    }
    SDL_SetWindowTitle(window,"Dragon Ball Z — Native port prototype");
  }
  execution.cpu=game->cpu;execution.enabled=native;dbz_set_execution(&execution);
  int state_size=snes_saveState(game,NULL);
  uint32_t sample_phase=0;
  if(checkpoint_path && state_path) die("Choose only one state loader");
  if(checkpoint_path) sample_phase=checkpoint_load(checkpoint_path,game,reference,state_size);
  if(state_path) raw_state_load(state_path,game,reference,state_size);
  uint8_t *state=malloc((size_t)state_size),*reference_state=verify?malloc((size_t)state_size):NULL;
  if(!state || (verify&&!reference_state)) die("State allocation failed");
  snes_saveState(game,state);
  FILE *trace=dump_dir?open_output(dump_dir,"frames.jsonl","w"):NULL;
  const bool trace_reference = verify && getenv("DBZ_TRACE_REFERENCE") != NULL;
  unsigned frame=0,keys=0;bool running=true,paused=false,turbo=false,matched=true;
  uint64_t started=SDL_GetPerformanceCounter(),deadline=started,frequency=SDL_GetPerformanceFrequency();
  uint64_t active_audio=0;
  while(running && (!frame_limit || frame<frame_limit)) {
    SDL_Event e;
    while(SDL_PollEvent(&e)) {
      if(e.type==SDL_QUIT) running=false;
      if(e.type==SDL_KEYDOWN || e.type==SDL_KEYUP) {
        bool down=e.type==SDL_KEYDOWN;int button=key_button(e.key.keysym.sym);
        if(button>=0) { if(down) keys|=1u<<button;else keys&=~(1u<<button); }
        if(e.key.keysym.sym==SDLK_ESCAPE && down) running=false;
        if(e.key.keysym.sym==SDLK_p && down&&!e.key.repeat) paused=!paused;
        if(e.key.keysym.sym==SDLK_TAB) turbo=down;
      }
      if(e.type==SDL_WINDOWEVENT && e.window.event==SDL_WINDOWEVENT_FOCUS_LOST) { keys=0;turbo=false; }
    }
    if(!running) break;
    if(paused) { SDL_Delay(16);deadline=SDL_GetPerformanceCounter();continue; }
    frame++;
    // Recorded game input must be reproducible even while the window is focused.
    unsigned mask=input_path?replay_mask(frame):keys;
    for(int b=0;b<12;b++) { snes_setButtonState(game,1,b,(mask>>b)&1u);if(verify) snes_setButtonState(reference,1,b,(mask>>b)&1u); }
    snes_runFrame(game);
    if(verify) {
      if(trace_reference) { execution.cpu=reference->cpu; execution.enabled=false; }
      snes_runFrame(reference);
      if(trace_reference) { execution.cpu=game->cpu; execution.enabled=native; }
    }
    snes_setPixels(game,pixels);if(verify) snes_setPixels(reference,reference_pixels);
    sample_phase+=320400000;int count=(int)(sample_phase/600988);sample_phase%=600988;
    snes_setSamples(game,samples,count);if(verify) snes_setSamples(reference,reference_samples,count);
    for(int i=0;i<count*2;i++) if(samples[i]) { active_audio++;break; }
    if(snes_saveState(game,state)!=state_size) die("State size changed unexpectedly");
    if(verify) {
      bool state_match=snes_saveState(reference,reference_state)==state_size && !memcmp(state,reference_state,(size_t)state_size);
      bool video_match=!memcmp(pixels,reference_pixels,sizeof(pixels));
      bool audio_match=!memcmp(samples,reference_samples,(size_t)count*4);
      if(!state_match || !video_match || !audio_match) {
        size_t state_diff=(size_t)-1;
        if(!state_match) for(size_t i=0;i<(size_t)state_size;i++) if(state[i]!=reference_state[i]) { state_diff=i; break; }
        fprintf(stderr,"Divergence frame %u: state=%d video=%d audio=%d PC=%02x:%04x state_diff=%lld\n",frame,state_match,video_match,audio_match,game->cpu->k,game->cpu->pc,state_diff==(size_t)-1?-1LL:(long long)state_diff);
        if(dump_dir) { write_file(dump_dir,"divergence-native.state",state,(size_t)state_size);write_file(dump_dir,"divergence-reference.state",reference_state,(size_t)state_size); }
        matched=false;running=false;
      }
    }
    if(trace) fprintf(trace,"{\"frame\":%u,\"input\":%u,\"state_hash\":\"%016" PRIx64 "\",\"video_hash\":\"%016" PRIx64 "\",\"audio_hash\":\"%016" PRIx64 "\",\"pc\":\"%02x:%04x\",\"native_steps\":%" PRIu64 "}\n",frame,mask,hash(state,(size_t)state_size),hash(pixels,sizeof(pixels)),hash(samples,(size_t)count*4),game->cpu->k,game->cpu->pc,execution.native_steps);
    if(dump_dir && (!(frame%capture_every) || (frame_limit&&frame==frame_limit) || !matched)) capture(dump_dir,frame);
    if(!headless) {
      if(audio&&!turbo&&SDL_GetQueuedAudioSize(audio)<(unsigned)count*4*5) SDL_QueueAudio(audio,samples,(unsigned)count*4);
      SDL_UpdateTexture(texture,NULL,pixels,WIDTH*4);SDL_RenderClear(renderer);SDL_RenderCopy(renderer,texture,NULL,NULL);SDL_RenderPresent(renderer);
      deadline+=(uint64_t)((double)frequency/60.0988);
      if(!turbo) { uint64_t now=SDL_GetPerformanceCounter();if(deadline>now) SDL_Delay((uint32_t)((deadline-now)*1000/frequency));else if(now-deadline>frequency/4) deadline=now; }
      else deadline=SDL_GetPerformanceCounter();
    }
  }
  if(trace) fclose(trace);
  double elapsed=(double)(SDL_GetPerformanceCounter()-started)/(double)frequency;
  FILE *report=dump_dir?open_output(dump_dir,"report.json","w"):stdout;
  fprintf(report,"{\"rom_sha256\":\"%s\",\"frames\":%u,\"verification_enabled\":%s,\"equivalence_passed\":%s,\"elapsed_seconds\":%.3f,\"frames_with_audio\":%" PRIu64 ",",DBZ_ROM_SHA256,frame,verify?"true":"false",verify?(matched?"true":"false"):"null",elapsed,active_audio);
  dbz_write_execution(report,&execution);fprintf(report,"}\n");
  if(dump_dir) {
    fclose(report);
    FILE *coverage=open_output(dump_dir,"executed-addresses.csv","w");
    fputs("rom_offset,cpu_address,hits\n",coverage);
    for(unsigned i=0;i<0x100000;i++) if(execution.visited[i]) fprintf(coverage,"%06x,%02x:%04x,%u\n",i,i/32768,0x8000+(i%32768),execution.visited[i]);
    fclose(coverage);
    write_file(dump_dir,"final.state",state,(size_t)state_size);
    if(matched) checkpoint_save(dump_dir,state,state_size,sample_phase);
  }
  fprintf(stderr,"Completed %u frames in %.2fs; native steps=%" PRIu64 ", baseline steps=%" PRIu64 "; verification=%s\n",frame,elapsed,execution.native_steps,execution.interpreted_steps,verify?(matched?"PASS":"FAIL"):"off");
  if(matched) battery_save(game,save_dir);
  free(state);free(reference_state);dbz_set_execution(NULL);snes_free(game);if(reference)snes_free(reference);
  if(audio) SDL_CloseAudioDevice(audio);
  SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);
  SDL_free(preferences);SDL_Quit();return matched?0:2;
}

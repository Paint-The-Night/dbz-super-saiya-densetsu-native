/* Browser host for the DBZ native port. ROM is never embedded: the player
 * supplies Japanese Rev 1 via the File API; validation uses dbz_load_rom_bytes. */
#include <SDL.h>
#include <emscripten.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native.h"
#include "rom.h"
#include "snes.h"

enum { WIDTH = 512, HEIGHT = 480, PIXEL_BYTES = WIDTH * HEIGHT * 4 };

static uint8_t pixels[PIXEL_BYTES];
static int16_t samples[2048];
static DbzExecution execution;

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_AudioDeviceID audio;
static Snes *game;
/* Dual masks: keyboard and touch OR'd each frame so releasing one device
 * does not clear buttons still held on the other. */
static unsigned keys_kb, keys_touch;
static bool paused, turbo, loop_running;
static uint32_t sample_phase;
static uint64_t deadline, frequency;
static const char *preferences = "/dbz-saves";
static char status_message[256];

static int key_button(SDL_Keycode key) {
  switch(key) {
    case SDLK_z: return 0;
    case SDLK_a: return 1;
    case SDLK_RSHIFT: return 2;
    case SDLK_RETURN: return 3;
    case SDLK_UP: return 4;
    case SDLK_DOWN: return 5;
    case SDLK_LEFT: return 6;
    case SDLK_RIGHT: return 7;
    case SDLK_x: return 8;
    case SDLK_s: return 9;
    case SDLK_d: return 10;
    case SDLK_c: return 11;
    default: return -1;
  }
}

static void set_status(const char *msg) {
  snprintf(status_message, sizeof(status_message), "%s", msg ? msg : "");
  EM_ASM({
    const el = document.getElementById('status');
    if(el) el.textContent = UTF8ToString($0);
  }, status_message);
}

static void battery_load(Snes *s, const char *directory) {
  if(!directory) return;
  char path[4096];
  snprintf(path, sizeof(path), "%s/dbz-rev1.srm", directory);
  FILE *f = fopen(path, "rb");
  if(!f) return;
  uint8_t buffer[8193];
  size_t size = fread(buffer, 1, sizeof(buffer), f);
  fclose(f);
  if(size == 8192) snes_loadBattery(s, buffer, (int)size);
}

static void battery_save(Snes *s, const char *directory) {
  if(!directory || !s) return;
  int size = snes_saveBattery(s, NULL);
  if(size != 8192) return;
  uint8_t data[8192];
  snes_saveBattery(s, data);
  char path[4096];
  snprintf(path, sizeof(path), "%s/dbz-rev1.srm", directory);
  FILE *f = fopen(path, "wb");
  if(!f) return;
  fwrite(data, 1, sizeof(data), f);
  fclose(f);
  EM_ASM({
    if(typeof FS !== 'undefined' && FS.syncfs) {
      FS.syncfs(false, function(err) {
        if(err) console.warn('IDBFS sync failed', err);
      });
    }
  });
}

static void frame_tick(void) {
  if(!game || !loop_running) return;

  SDL_Event e;
  while(SDL_PollEvent(&e)) {
    if(e.type == SDL_QUIT) {
      loop_running = false;
      battery_save(game, preferences);
      emscripten_cancel_main_loop();
      return;
    }
    if(e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
      bool down = e.type == SDL_KEYDOWN;
      int button = key_button(e.key.keysym.sym);
      if(button >= 0) {
        if(down) keys_kb |= 1u << button;
        else keys_kb &= ~(1u << button);
      }
      if(e.key.keysym.sym == SDLK_p && down && !e.key.repeat) paused = !paused;
      if(e.key.keysym.sym == SDLK_TAB) turbo = down;
    }
    /* Focus-lost: clear keyboard (+ turbo) only. Touch buttons stay held so a
     * soft-keyboard or browser chrome steal does not drop on-screen pad state. */
    if(e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
      keys_kb = 0;
      turbo = false;
    }
  }

  if(paused) {
    deadline = SDL_GetPerformanceCounter();
    return;
  }

  unsigned keys = keys_kb | keys_touch;
  for(int b = 0; b < 12; b++) snes_setButtonState(game, 1, b, (keys >> b) & 1u);
  snes_runFrame(game);
  snes_setPixels(game, pixels);
  sample_phase += 320400000;
  int count = (int)(sample_phase / 600988);
  sample_phase %= 600988;
  snes_setSamples(game, samples, count);

  if(audio && !turbo && SDL_GetQueuedAudioSize(audio) < (unsigned)count * 4 * 5)
    SDL_QueueAudio(audio, samples, (unsigned)count * 4);

  SDL_UpdateTexture(texture, NULL, pixels, WIDTH * 4);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);

  deadline += (uint64_t)((double)frequency / 60.0988);
  if(!turbo) {
    uint64_t now = SDL_GetPerformanceCounter();
    if(now - deadline > frequency / 4) deadline = now;
  } else {
    deadline = SDL_GetPerformanceCounter();
  }
}

/* Touch / overlay: button 0..11 same mapping as key_button (B,Y,Select,Start,Up,Down,Left,Right,A,X,L,R). */
EMSCRIPTEN_KEEPALIVE
void dbz_web_set_button(int button, int down) {
  if(button < 0 || button > 11) return;
  if(down) keys_touch |= 1u << button;
  else keys_touch &= ~(1u << button);
}

EMSCRIPTEN_KEEPALIVE
void dbz_web_set_turbo(int down) {
  turbo = down != 0;
}

EMSCRIPTEN_KEEPALIVE
void dbz_web_toggle_pause(void) {
  paused = !paused;
}

/* Returns 0 on success, 1 bad size/header, 2 hash mismatch, 3 init failure. */
EMSCRIPTEN_KEEPALIVE
int dbz_web_load_rom(const uint8_t *data, int length) {
  if(game) {
    set_status("ROM already loaded. Reload the page to choose another.");
    return 3;
  }
  if(length != 1048576 && length != 1049088) {
    set_status("Wrong file size. Need unmodified Japanese Rev 1 (1 MiB, optional 512-byte header).");
    return 1;
  }

  size_t rom_size = 0;
  uint8_t *rom = dbz_load_rom_bytes(data, (size_t)length, &rom_size);
  if(!rom) {
    set_status("ROM hash mismatch. Only the unmodified Japanese Rev 1 is accepted.");
    return 2;
  }

  game = snes_init();
  if(!game || !snes_loadRom(game, rom, (int)rom_size)) {
    free(rom);
    if(game) { snes_free(game); game = NULL; }
    set_status("Could not initialize emulator with this ROM.");
    return 3;
  }
  free(rom);

  battery_load(game, preferences);
  execution.cpu = game->cpu;
  execution.enabled = true;
  dbz_set_execution(&execution);
  sample_phase = 0;
  keys_kb = 0;
  keys_touch = 0;
  paused = false;
  turbo = false;
  loop_running = true;
  frequency = SDL_GetPerformanceFrequency();
  deadline = SDL_GetPerformanceCounter();

  EM_ASM({
    const panel = document.getElementById('loader');
    if(panel) panel.classList.add('hidden');
    const canvas = document.getElementById('canvas');
    if(canvas) { canvas.focus(); canvas.tabIndex = 0; }
    const pad = document.getElementById('touch-pad');
    if(pad) pad.classList.remove('hidden');
  });
  set_status("Playing — keyboard or on-screen pad. P pause, Tab turbo.");
  emscripten_set_main_loop(frame_tick, 0, 0);
  return 0;
}

EMSCRIPTEN_KEEPALIVE
const char *dbz_web_expected_sha256(void) {
  return DBZ_ROM_SHA256;
}

int main(void) {
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
    set_status(SDL_GetError());
    return 1;
  }

  /* Persist battery saves under IDBFS (local browser only; never uploaded). */
  EM_ASM({
    try { FS.mkdir('/dbz-saves'); } catch (e) {}
    try {
      FS.mount(IDBFS, {}, '/dbz-saves');
      FS.syncfs(true, function(err) {
        if(err) console.warn('IDBFS restore failed', err);
      });
    } catch (e) {
      console.warn('IDBFS unavailable', e);
    }
  });

  window = SDL_CreateWindow("Dragon Ball Z — Super Saiya Densetsu",
                            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            960, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if(!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if(!window || !renderer) {
    set_status(SDL_GetError());
    return 1;
  }
  SDL_RenderSetLogicalSize(renderer, 640, 480);
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBX8888,
                              SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
  if(!texture) {
    set_status(SDL_GetError());
    return 1;
  }

  SDL_AudioSpec want = {0}, got = {0};
  want.freq = 32040;
  want.format = AUDIO_S16SYS;
  want.channels = 2;
  want.samples = 1024;
  audio = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
  if(audio) SDL_PauseAudioDevice(audio, 0);
  else fprintf(stderr, "Audio unavailable: %s\n", SDL_GetError());

  set_status("Load your legally obtained Japanese Rev 1 .sfc to start.");
  EM_ASM({
    if(typeof Module !== 'undefined' && Module.onRuntimeReady)
      Module.onRuntimeReady();
    else if(typeof window !== 'undefined')
      window.dispatchEvent(new Event('dbz-ready'));
  });
  return 0;
}

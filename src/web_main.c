/* Browser host for the DBZ native port. ROM is never embedded: the player
 * supplies Japanese Rev 1 via the File API; validation uses dbz_load_rom_bytes.
 * After a validated ROM is offered, an in-game LANGUAGE boot menu (SDL) is the
 * first screen in the game viewport — not a website HTML panel.
 * Language selects native i18n state only; the same clean JP ROM always boots
 * (no IPS / ROM patching). */
#include <SDL.h>
#include <emscripten.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "i18n.h"
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
/* Triple masks OR'd each frame: keyboard, optional classic pad, canvas gestures.
 * Pulses (tap/long-press/flick) are queued separately so they last N frames. */
static unsigned keys_kb, keys_touch, keys_gesture;
enum { PULSE_SLOTS = 16 };
static struct { unsigned mask; int remaining; } pulses[PULSE_SLOTS];
static bool paused, turbo, loop_running;
static uint32_t sample_phase;
static uint64_t deadline, frequency;
static const char *preferences = "/dbz-saves";
static char status_message[256];

/* ---- In-game language boot menu (before emulator title) ---- */
enum { BOOT_NONE = 0, BOOT_LANG = 1, BOOT_PLAYING = 2 };
static int boot_phase = BOOT_NONE;
static int lang_sel;          /* 0 = ENGLISH, 1 = 日本語 */
static unsigned lang_prev_keys;
static int lang_confirm_lock; /* debounce frames after enter */
/* Raster strips from JS (BGRX): title subtitle + JP option label */
enum { LABEL_W = 200, LABEL_H = 28, LABEL_BYTES = LABEL_W * LABEL_H * 4 };
static uint8_t label_gengo[LABEL_BYTES];
static uint8_t label_nihongo[LABEL_BYTES];
static uint8_t labels_ready;

/* Tiny 8×8 glyphs for A–Z, space, and a few symbols (bit0 = leftmost). */
static const uint8_t FONT8[96][8] = {
  /* 0x20 space */ {0,0,0,0,0,0,0,0},
  /* ! */ {0x10,0x10,0x10,0x10,0x10,0x00,0x10,0},
  /* " */ {0x28,0x28,0,0,0,0,0,0},
  /* # */ {0x28,0x7C,0x28,0x28,0x7C,0x28,0,0},
  /* $ */ {0x10,0x3C,0x50,0x38,0x14,0x78,0x10,0},
  /* % */ {0x60,0x64,0x08,0x10,0x20,0x4C,0x0C,0},
  /* & */ {0x20,0x50,0x50,0x20,0x54,0x48,0x34,0},
  /* ' */ {0x10,0x10,0,0,0,0,0,0},
  /* ( */ {0x08,0x10,0x20,0x20,0x20,0x10,0x08,0},
  /* ) */ {0x20,0x10,0x08,0x08,0x08,0x10,0x20,0},
  /* * */ {0x00,0x28,0x10,0x7C,0x10,0x28,0,0},
  /* + */ {0x00,0x10,0x10,0x7C,0x10,0x10,0,0},
  /* , */ {0,0,0,0,0,0x10,0x10,0x20},
  /* - */ {0,0,0,0x7C,0,0,0,0},
  /* . */ {0,0,0,0,0,0,0x10,0},
  /* / */ {0x04,0x08,0x10,0x20,0x40,0,0,0},
  /* 0 */ {0x38,0x44,0x4C,0x54,0x64,0x44,0x38,0},
  /* 1 */ {0x10,0x30,0x10,0x10,0x10,0x10,0x38,0},
  /* 2 */ {0x38,0x44,0x04,0x18,0x20,0x40,0x7C,0},
  /* 3 */ {0x38,0x44,0x04,0x18,0x04,0x44,0x38,0},
  /* 4 */ {0x08,0x18,0x28,0x48,0x7C,0x08,0x08,0},
  /* 5 */ {0x7C,0x40,0x78,0x04,0x04,0x44,0x38,0},
  /* 6 */ {0x18,0x20,0x40,0x78,0x44,0x44,0x38,0},
  /* 7 */ {0x7C,0x04,0x08,0x10,0x20,0x20,0x20,0},
  /* 8 */ {0x38,0x44,0x44,0x38,0x44,0x44,0x38,0},
  /* 9 */ {0x38,0x44,0x44,0x3C,0x04,0x08,0x30,0},
  /* : */ {0,0x10,0,0,0,0x10,0,0},
  /* ; */ {0,0x10,0,0,0,0x10,0x10,0x20},
  /* < */ {0x08,0x10,0x20,0x40,0x20,0x10,0x08,0},
  /* = */ {0,0,0x7C,0,0x7C,0,0,0},
  /* > */ {0x40,0x20,0x10,0x08,0x10,0x20,0x40,0},
  /* ? */ {0x38,0x44,0x04,0x08,0x10,0x00,0x10,0},
  /* @ */ {0x38,0x44,0x5C,0x54,0x5C,0x40,0x38,0},
  /* A */ {0x38,0x44,0x44,0x7C,0x44,0x44,0x44,0},
  /* B */ {0x78,0x44,0x44,0x78,0x44,0x44,0x78,0},
  /* C */ {0x38,0x44,0x40,0x40,0x40,0x44,0x38,0},
  /* D */ {0x78,0x44,0x44,0x44,0x44,0x44,0x78,0},
  /* E */ {0x7C,0x40,0x40,0x78,0x40,0x40,0x7C,0},
  /* F */ {0x7C,0x40,0x40,0x78,0x40,0x40,0x40,0},
  /* G */ {0x38,0x44,0x40,0x5C,0x44,0x44,0x38,0},
  /* H */ {0x44,0x44,0x44,0x7C,0x44,0x44,0x44,0},
  /* I */ {0x38,0x10,0x10,0x10,0x10,0x10,0x38,0},
  /* J */ {0x1C,0x08,0x08,0x08,0x08,0x48,0x30,0},
  /* K */ {0x44,0x48,0x50,0x60,0x50,0x48,0x44,0},
  /* L */ {0x40,0x40,0x40,0x40,0x40,0x40,0x7C,0},
  /* M */ {0x44,0x6C,0x54,0x54,0x44,0x44,0x44,0},
  /* N */ {0x44,0x64,0x54,0x4C,0x44,0x44,0x44,0},
  /* O */ {0x38,0x44,0x44,0x44,0x44,0x44,0x38,0},
  /* P */ {0x78,0x44,0x44,0x78,0x40,0x40,0x40,0},
  /* Q */ {0x38,0x44,0x44,0x44,0x54,0x48,0x34,0},
  /* R */ {0x78,0x44,0x44,0x78,0x50,0x48,0x44,0},
  /* S */ {0x38,0x44,0x40,0x38,0x04,0x44,0x38,0},
  /* T */ {0x7C,0x10,0x10,0x10,0x10,0x10,0x10,0},
  /* U */ {0x44,0x44,0x44,0x44,0x44,0x44,0x38,0},
  /* V */ {0x44,0x44,0x44,0x44,0x44,0x28,0x10,0},
  /* W */ {0x44,0x44,0x44,0x54,0x54,0x6C,0x44,0},
  /* X */ {0x44,0x44,0x28,0x10,0x28,0x44,0x44,0},
  /* Y */ {0x44,0x44,0x28,0x10,0x10,0x10,0x10,0},
  /* Z */ {0x7C,0x04,0x08,0x10,0x20,0x40,0x7C,0},
};

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

static void put_px(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  if((unsigned)x >= (unsigned)WIDTH || (unsigned)y >= (unsigned)HEIGHT) return;
  /* Match LakeSnes default BGRX packing used with SDL_PIXELFORMAT_RGBX8888. */
  uint8_t *p = pixels + (y * WIDTH + x) * 4;
  p[0] = b;
  p[1] = g;
  p[2] = r;
  p[3] = 0;
}

static void fill_rect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
  for(int j = 0; j < h; j++)
    for(int i = 0; i < w; i++)
      put_px(x + i, y + j, r, g, b);
}

static void draw_frame(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, int thick) {
  for(int t = 0; t < thick; t++) {
    fill_rect(x + t, y + t, w - 2 * t, 1, r, g, b);
    fill_rect(x + t, y + h - 1 - t, w - 2 * t, 1, r, g, b);
    fill_rect(x + t, y + t, 1, h - 2 * t, r, g, b);
    fill_rect(x + w - 1 - t, y + t, 1, h - 2 * t, r, g, b);
  }
}

static void draw_char(int x, int y, char c, uint8_t r, uint8_t g, uint8_t b, int scale) {
  unsigned idx = (unsigned char)c;
  if(idx < 32 || idx > 90) {
    if(idx >= 'a' && idx <= 'z') idx = (unsigned)(idx - 'a' + 'A');
    else idx = (unsigned)'?';
  }
  idx -= 32;
  if(idx >= 96) idx = (unsigned)'?' - 32;
  const uint8_t *glyph = FONT8[idx];
  for(int row = 0; row < 8; row++) {
    uint8_t bits = glyph[row];
    for(int col = 0; col < 8; col++) {
      if(bits & (0x80u >> col)) {
        fill_rect(x + col * scale, y + row * scale, scale, scale, r, g, b);
      }
    }
  }
}

static void draw_text(int x, int y, const char *s, uint8_t r, uint8_t g, uint8_t b, int scale) {
  int cx = x;
  for(; *s; s++) {
    if(*s == ' ') { cx += 8 * scale; continue; }
    draw_char(cx, y, *s, r, g, b, scale);
    cx += 8 * scale;
  }
}

static void blit_label(const uint8_t *src, int dx, int dy, int w, int h) {
  for(int j = 0; j < h; j++) {
    for(int i = 0; i < w; i++) {
      const uint8_t *s = src + (j * w + i) * 4;
      /* Skip near-black / transparent */
      if(s[0] < 8 && s[1] < 8 && s[2] < 8) continue;
      put_px(dx + i, dy + j, s[2], s[1], s[0]); /* src is BGRX from JS */
    }
  }
}

static unsigned collect_keys(void) {
  unsigned pulse = 0;
  for(int i = 0; i < PULSE_SLOTS; i++) {
    if(pulses[i].remaining > 0) {
      pulse |= pulses[i].mask;
      pulses[i].remaining--;
    }
  }
  return keys_kb | keys_touch | keys_gesture | pulse;
}

static void present_pixels(void) {
  SDL_UpdateTexture(texture, NULL, pixels, WIDTH * 4);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
}

static void draw_lang_menu(void) {
  /* Full-bleed dark playfield */
  fill_rect(0, 0, WIDTH, HEIGHT, 8, 12, 28);

  const int win_w = 360;
  const int win_h = 220;
  const int win_x = (WIDTH - win_w) / 2;
  const int win_y = (HEIGHT - win_h) / 2 - 10;

  fill_rect(win_x, win_y, win_w, win_h, 14, 20, 40);
  draw_frame(win_x, win_y, win_w, win_h, 245, 166, 35, 3);      /* gold/orange */
  draw_frame(win_x + 4, win_y + 4, win_w - 8, win_h - 8, 90, 72, 16, 1);
  draw_frame(win_x + 6, win_y + 6, win_w - 12, win_h - 12, 58, 72, 110, 1);

  /* Title */
  const char *title = dbz_i18n_str(DBZ_STR_LANG_TITLE);
  int tw = (int)strlen(title) * 8 * 2;
  draw_text(win_x + (win_w - tw) / 2, win_y + 22, title, 255, 213, 106, 2);

  if(labels_ready)
    blit_label(label_gengo, win_x + (win_w - 120) / 2, win_y + 48, 120, 22);
  else
    draw_text(win_x + (win_w - 9 * 8) / 2, win_y + 52, "GENGO", 232, 196, 106, 1);

  /* Rows */
  const int row0_y = win_y + 92;
  const int row1_y = win_y + 132;
  const int row_x = win_x + 48;
  const int row_w = win_w - 96;

  for(int i = 0; i < 2; i++) {
    int ry = i == 0 ? row0_y : row1_y;
    bool on = lang_sel == i;
    fill_rect(row_x, ry, row_w, 32, on ? 36 : 26, on ? 48 : 34, on ? 72 : 56);
    draw_frame(row_x, ry, row_w, 32, on ? 245 : 106, on ? 166 : 90, on ? 35 : 40, 2);
    if(on)
      draw_text(row_x + 10, ry + 8, ">", 245, 166, 35, 2);
    if(i == 0) {
      draw_text(row_x + 40, ry + 8, "ENGLISH", on ? 255 : 240, on ? 233 : 230, on ? 168 : 200, 2);
    } else if(labels_ready) {
      blit_label(label_nihongo, row_x + 40, ry + 4, 140, 24);
    } else {
      draw_text(row_x + 40, ry + 8, "JAPANESE", on ? 255 : 240, on ? 233 : 230, on ? 168 : 200, 2);
    }
  }

  draw_text(win_x + 28, win_y + win_h - 28, dbz_i18n_str(DBZ_STR_LANG_HINT), 154, 163, 189, 1);
}

static void lang_confirm(void);
static void menu_tick(void);
static void frame_tick(void);

static void enter_playing_chrome(void) {
  EM_ASM({
    document.body.classList.add('playing');
    const panel = document.getElementById('loader');
    if(panel) panel.classList.add('hidden');
    const header = document.querySelector('header');
    if(header) header.classList.add('hidden');
    const controls = document.querySelector('.controls');
    if(controls) controls.classList.add('hidden');
    const footer = document.querySelector('footer');
    if(footer) footer.classList.add('hidden');
    const canvas = document.getElementById('canvas');
    if(canvas) { canvas.focus(); canvas.tabIndex = 0; }
    const chrome = document.getElementById('touch-chrome');
    if(chrome) chrome.classList.remove('hidden');
    const hint = document.getElementById('gesture-hint');
    if(hint && !localStorage.getItem('dbz-gesture-hint-dismissed'))
      hint.classList.remove('hidden');
    if(typeof window.__dbzOnRomLoaded === 'function') window.__dbzOnRomLoaded();
  });
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

static void poll_input_events(void) {
  SDL_Event e;
  while(SDL_PollEvent(&e)) {
    if(e.type == SDL_QUIT) {
      loop_running = false;
      if(game) battery_save(game, preferences);
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
      if(boot_phase == BOOT_PLAYING) {
        if(e.key.keysym.sym == SDLK_p && down && !e.key.repeat) paused = !paused;
        if(e.key.keysym.sym == SDLK_TAB) turbo = down;
      }
    }
    if(e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
      keys_kb = 0;
      turbo = false;
    }
  }
}

static void lang_confirm(void) {
  /* Prefer setting lang from C; web may also call dbz_i18n_set as a backup. */
  DbzLang chosen = lang_sel == 1 ? DBZ_LANG_JA : DBZ_LANG_EN;
  dbz_i18n_set(chosen);
  const char *lang = chosen == DBZ_LANG_JA ? "ja" : "en";
  boot_phase = BOOT_NONE;
  lang_confirm_lock = 8;
  keys_kb = 0;
  keys_touch = 0;
  keys_gesture = 0;
  for(int i = 0; i < PULSE_SLOTS; i++) { pulses[i].mask = 0; pulses[i].remaining = 0; }
  emscripten_cancel_main_loop();
  EM_ASM({
    const lang = UTF8ToString($0);
    try { localStorage.setItem('dbz-lang', lang); } catch (e) {}
    if(typeof window.__dbzOnLangPicked === 'function')
      window.__dbzOnLangPicked(lang);
  }, lang);
}

static void menu_tick(void) {
  if(boot_phase != BOOT_LANG) return;
  poll_input_events();
  if(!loop_running) return;

  unsigned keys = collect_keys();
  unsigned pressed = keys & ~lang_prev_keys;
  lang_prev_keys = keys;

  if(lang_confirm_lock > 0) {
    lang_confirm_lock--;
  } else {
    if(pressed & (1u << 4)) lang_sel = (lang_sel + 1) % 2; /* Up */
    if(pressed & (1u << 5)) lang_sel = (lang_sel + 1) % 2; /* Down */
    /* A / B / Start / X confirm */
    if(pressed & ((1u << 8) | (1u << 0) | (1u << 3) | (1u << 9))) {
      lang_confirm();
      return;
    }
  }

  draw_lang_menu();
  present_pixels();
}

static void frame_tick(void) {
  if(!game || !loop_running || boot_phase != BOOT_PLAYING) return;

  poll_input_events();
  if(!loop_running) return;

  if(paused) {
    deadline = SDL_GetPerformanceCounter();
    return;
  }

  unsigned keys = collect_keys();
  for(int b = 0; b < 12; b++) snes_setButtonState(game, 1, b, (keys >> b) & 1u);
  snes_runFrame(game);
  snes_setPixels(game, pixels);
  sample_phase += 320400000;
  int count = (int)(sample_phase / 600988);
  sample_phase %= 600988;
  snes_setSamples(game, samples, count);

  if(audio && !turbo && SDL_GetQueuedAudioSize(audio) < (unsigned)count * 4 * 5)
    SDL_QueueAudio(audio, samples, (unsigned)count * 4);

  present_pixels();

  deadline += (uint64_t)((double)frequency / 60.0988);
  if(!turbo) {
    uint64_t now = SDL_GetPerformanceCounter();
    if(now - deadline > frequency / 4) deadline = now;
  } else {
    deadline = SDL_GetPerformanceCounter();
  }
}

/* Classic pad / chrome: button 0..11 = B,Y,Select,Start,Up,Down,Left,Right,A,X,L,R. */
EMSCRIPTEN_KEEPALIVE
void dbz_web_set_button(int button, int down) {
  if(button < 0 || button > 11) return;
  if(down) keys_touch |= 1u << button;
  else keys_touch &= ~(1u << button);
}

/* Held canvas virtual-stick mask (typically D-pad bits 4–7). */
EMSCRIPTEN_KEEPALIVE
void dbz_web_set_gesture_mask(unsigned mask) {
  keys_gesture = mask & 0xFFFu;
}

/* Pulse a button for N frames (tap/flick/chrome). Survives across frame_tick. */
EMSCRIPTEN_KEEPALIVE
void dbz_web_pulse_button(int button, int frames) {
  if(button < 0 || button > 11) return;
  if(frames < 1) frames = 1;
  if(frames > 30) frames = 30;
  unsigned bit = 1u << button;
  for(int i = 0; i < PULSE_SLOTS; i++) {
    if(pulses[i].remaining <= 0) {
      pulses[i].mask = bit;
      pulses[i].remaining = frames;
      return;
    }
  }
  pulses[0].mask = bit;
  pulses[0].remaining = frames;
}

EMSCRIPTEN_KEEPALIVE
void dbz_web_set_turbo(int down) {
  turbo = down != 0;
}

EMSCRIPTEN_KEEPALIVE
void dbz_web_toggle_pause(void) {
  if(boot_phase == BOOT_PLAYING) paused = !paused;
}

/* Shared post-validation boot: takes owned 1 MiB buffer (freed here). */
static int boot_prepared_rom(uint8_t *rom, size_t rom_size) {
  if(game) {
    free(rom);
    set_status("ROM already loaded. Reload the page to choose another.");
    return 3;
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
  keys_gesture = 0;
  for(int i = 0; i < PULSE_SLOTS; i++) { pulses[i].mask = 0; pulses[i].remaining = 0; }
  paused = false;
  turbo = false;
  loop_running = true;
  boot_phase = BOOT_PLAYING;
  frequency = SDL_GetPerformanceFrequency();
  deadline = SDL_GetPerformanceCounter();

  enter_playing_chrome();
  if(dbz_i18n_get() == DBZ_LANG_EN)
    set_status(dbz_i18n_str(DBZ_STR_EN_WIP_STATUS));
  else
    set_status(dbz_i18n_str(DBZ_STR_PLAYING_STATUS));
  emscripten_set_main_loop(frame_tick, 0, 0);
  return 0;
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
  return boot_prepared_rom(rom, rom_size);
}

/* Load a clean 1 MiB Japanese Rev 1 buffer (same bytes for EN and JP).
 * Re-checks SHA-256 so patched images cannot boot. Prefer dbz_web_load_rom. */
EMSCRIPTEN_KEEPALIVE
int dbz_web_load_prepared_rom(const uint8_t *data, int length) {
  if(game) {
    set_status("ROM already loaded. Reload the page to choose another.");
    return 3;
  }
  if(!data || length != 1048576) {
    set_status("Prepared ROM must be exactly 1,048,576 bytes.");
    return 1;
  }
  char digest[65];
  dbz_sha256(data, 1048576, digest);
  if(strcmp(digest, DBZ_ROM_SHA256) != 0) {
    set_status("ROM hash mismatch. Only the unmodified Japanese Rev 1 is accepted.");
    return 2;
  }
  uint8_t *rom = malloc(1048576);
  if(!rom) {
    set_status("Out of memory allocating ROM buffer.");
    return 3;
  }
  memcpy(rom, data, 1048576);
  return boot_prepared_rom(rom, 1048576);
}

EMSCRIPTEN_KEEPALIVE
const char *dbz_web_expected_sha256(void) {
  return DBZ_ROM_SHA256;
}

/* Show in-game LANGUAGE / 言語 boot menu as the first canvas screen.
 * initial_sel: 0=ENGLISH, 1=日本語 (from localStorage preference).
 * skip_if_start: if non-zero and Start is held, auto-confirm initial_sel. */
EMSCRIPTEN_KEEPALIVE
void dbz_web_enter_lang_menu(int initial_sel, int skip_if_start) {
  if(game || boot_phase == BOOT_PLAYING) return;
  if(boot_phase == BOOT_LANG) return;

  lang_sel = initial_sel ? 1 : 0;
  lang_prev_keys = 0xFFFFFFFFu; /* ignore currently-held keys for one edge */
  lang_confirm_lock = 10;
  boot_phase = BOOT_LANG;
  loop_running = true;
  labels_ready = false;

  /* Ask JS to rasterize JP labels into our BGRX buffers. */
  EM_ASM({
    function raster(text, ptr, w, h) {
      var c = document.createElement('canvas');
      c.width = w;
      c.height = h;
      var ctx = c.getContext('2d');
      ctx.imageSmoothingEnabled = false;
      ctx.clearRect(0, 0, w, h);
      ctx.fillStyle = '#ffe9a8';
      ctx.font = 'bold 18px "Segoe UI", "Yu Gothic", "Hiragino Sans", sans-serif';
      ctx.textBaseline = 'middle';
      ctx.fillText(text, 2, h / 2);
      var img = ctx.getImageData(0, 0, w, h).data;
      var heap = Module.HEAPU8;
      for(var i = 0; i < w * h; i++) {
        var s = i * 4;
        var d = ptr + s;
        /* Store BGRX to match put_px / LakeSnes */
        heap[d + 0] = img[s + 2];
        heap[d + 1] = img[s + 1];
        heap[d + 2] = img[s + 0];
        heap[d + 3] = img[s + 3];
      }
    }
    try {
      raster('言語', $0, 120, 22);
      raster('日本語', $1, 140, 24);
      Module.HEAPU8[$2] = 1;
    } catch (e) {
      console.warn('boot label raster failed', e);
      Module.HEAPU8[$2] = 0;
    }
    document.body.classList.add('playing');
    const panel = document.getElementById('loader');
    if(panel) panel.classList.add('hidden');
    const header = document.querySelector('header');
    if(header) header.classList.add('hidden');
    const controls = document.querySelector('.controls');
    if(controls) controls.classList.add('hidden');
    const footer = document.querySelector('footer');
    if(footer) footer.classList.add('hidden');
    const canvas = document.getElementById('canvas');
    if(canvas) { canvas.focus(); canvas.tabIndex = 0; }
    const chrome = document.getElementById('touch-chrome');
    if(chrome) chrome.classList.remove('hidden');
  }, (uintptr_t)label_gengo, (uintptr_t)label_nihongo, (uintptr_t)&labels_ready);

  /* Quiet override: held Start skips menu and uses remembered cursor. */
  if(skip_if_start) {
    unsigned held = keys_kb | keys_touch | keys_gesture;
    if(held & (1u << 3)) {
      lang_confirm();
      return;
    }
  }

  set_status("Language — ↑↓ / flick · A / tap to confirm");
  draw_lang_menu();
  present_pixels();
  emscripten_set_main_loop(menu_tick, 0, 0);
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

#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "board.h"
#include "config.h"

// Grid geometry derived from DOT_SPACING. 466px / 10px -> 47x47 cells.
#define GRID_N   (LCD_W / DOT_SPACING + 1)
#define GRID_ORG ((LCD_W - (GRID_N - 1) * DOT_SPACING) / 2)
#define NCELLS   (GRID_N * GRID_N)

struct RGB { uint8_t r, g, b; };

// Palette slots a cell can reference. 0 is always the ambient background.
enum { SLOT_AMBIENT = 0, SLOT_ACCENT, SLOT_MONEY, SLOT_WARN, SLOT_DIM, NSLOTS };

// ---------------------------------------------------------------------------
// CellBuf -- a 47x47 drawing surface in grid space. Cards draw into one of
// these; the DotGrid turns it into lit dots. Everything is cell coordinates,
// never pixels.
// ---------------------------------------------------------------------------
struct CellBuf {
  uint8_t v[NCELLS];   // target intensity 0..255 (0 = unlit/background)
  uint8_t c[NCELLS];   // palette slot

  void clear();
  void plot(int x, int y, uint8_t val, uint8_t slot = SLOT_ACCENT);
  void hline(int x, int y, int w, uint8_t val, uint8_t slot = SLOT_ACCENT);
  void vline(int x, int y, int h, uint8_t val, uint8_t slot = SLOT_ACCENT);
  void fillRect(int x, int y, int w, int h, uint8_t val, uint8_t slot = SLOT_ACCENT);
  void ring(float cx, float cy, float r, float thickness, uint8_t val, uint8_t slot = SLOT_ACCENT);
  void arc(float cx, float cy, float r, float a0, float a1, float thickness,
           uint8_t val, uint8_t slot = SLOT_ACCENT);

  // 3x5 micro text -- the label tier (folded to uppercase)
  void micro(int x, int y, const char *s, uint8_t val, uint8_t slot = SLOT_DIM);
  void microCenter(int y, const char *s, uint8_t val, uint8_t slot = SLOT_DIM);
  static int microW(const char *s);

  // 5x7 small text
  void text(int x, int y, const char *s, uint8_t val, uint8_t slot = SLOT_ACCENT);
  void textCenter(int y, const char *s, uint8_t val, uint8_t slot = SLOT_ACCENT);
  static int textW(const char *s);

  // 7x11 display text (digits, $ . , : k M - space %)
  void big(int x, int y, const char *s, uint8_t val, uint8_t slot = SLOT_ACCENT);
  void bigCenter(int y, const char *s, uint8_t val, uint8_t slot = SLOT_ACCENT);
  static int bigW(const char *s);

  // Column chart that fades toward its base, for the sparkline card.
  void bars(int x, int y, int h, const int32_t *vals, int n, int barw = 2,
            uint8_t slot = SLOT_ACCENT);
};

// ---------------------------------------------------------------------------
// DotGrid -- owns the dots, the shimmer, the staggered reveal and the blitter.
// ---------------------------------------------------------------------------
class DotGrid {
 public:
  bool begin(Arduino_Canvas *canvas);

  void setPalette(int slot, RGB c) { pal_[slot] = c; }

  // Cross-fade to a new frame with the noise-staggered reveal.
  void present(const CellBuf &next);
  // Replace the frame immediately -- for per-frame animation (bursts, charts).
  void presentInstant(const CellBuf &next);

  void update(uint32_t nowMs);   // advance brightness + colour
  void render();                 // blit dots into the canvas framebuffer

  bool revealing() const { return reveal_ < 1.0f; }

 private:
  struct Dot {
    int16_t  px, py;      // pixel centre
    uint16_t idx;         // index into CellBuf
    float    bright;
    float    phase;       // shimmer phase
    float    noise;       // 0..1 reveal order
    float    r, g, b;     // current colour, smoothed
  };

  Arduino_Canvas *cv_   = nullptr;
  uint16_t       *fb_   = nullptr;
  Dot            *dots_ = nullptr;
  uint16_t        ndots_ = 0;

  uint8_t curV_[NCELLS], curC_[NCELLS];
  uint8_t prvV_[NCELLS], prvC_[NCELLS];
  float   reveal_ = 1.0f;
  uint32_t revealStart_ = 0;

  RGB pal_[NSLOTS];
};

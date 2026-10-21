#pragma once
#include <Arduino.h>
#include "board.h"
#include "config.h"
#include "textlayer.h"

// Receives one finished band of pixels, already in the panel's byte order.
typedef void (*BandSink)(int y0, int h, uint16_t *pixels);

// Grid geometry derived from DOT_SPACING. The panel is a rectangle, so width
// and height are independent; 368x448 at 7px -> 52 x 64 cells.
#define GRID_W   (LCD_W / DOT_SPACING)
#define GRID_H   (LCD_H / DOT_SPACING)
#define GRID_OX  ((LCD_W - (GRID_W - 1) * DOT_SPACING) / 2)
#define GRID_OY  ((LCD_H - (GRID_H - 1) * DOT_SPACING) / 2)
#define NCELLS   (GRID_W * GRID_H)

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
  // Renders in horizontal bands held in internal SRAM and hands each one to
  // `sink`. Pixels are written big-endian so the sink can DMA them straight
  // out: Arduino_GFX's own writePixels() byte-swaps per pixel, which costs
  // ~29ms/frame at this resolution -- more than the SPI transfer itself.
  bool begin(BandSink sink);

  void setPalette(int slot, RGB c) { pal_[slot] = c; }

  // Cross-fade to a new frame with the noise-staggered reveal.
  void present(const CellBuf &next);
  // Replace the frame immediately -- for per-frame animation (bursts, charts).
  void presentInstant(const CellBuf &next);

  void update(uint32_t nowMs);   // advance brightness + colour
  void render(const TextLayer *text = nullptr);   // dots + crisp text -> panel

  bool revealing() const { return reveal_ < 1.0f; }

#if PROFILE_FRAME
  uint32_t tDraw = 0, tPush = 0;   // us accumulated by render(), for profiling
#endif

 private:
  struct Dot {
    int16_t  px, py;      // pixel centre
    uint16_t idx;         // index into CellBuf
    float    bright;
    float    noise;       // 0..1 reveal order
    float    r, g, b;     // current colour, smoothed
    // Shimmer is sin(t*speed + k) per dot. Angle addition turns that into
    // sin(t*speed)*cos(k) + cos(t*speed)*sin(k), and cos(k)/sin(k) are fixed
    // per dot -- so they're baked here and the per-frame loop does no trig.
    float    cp2, sp2;    // k = phase*2   (slow pulse)
    float    cp6, sp6;    // k = phase*6   (twinkle)
    float    cw,  sw;     // k = x*.012 + y*.010 (spatial wave)
  };

  static const int BAND_H = 16;          // rows per band; 466*16*2 = 14.9KB

  BandSink     sink_  = nullptr;
  uint16_t    *band_  = nullptr;         // internal DMA-capable SRAM
  Dot         *dots_  = nullptr;
  uint16_t     ndots_ = 0;
  uint16_t     rowStart_[GRID_H + 1];    // dots_ index where each grid row begins

  uint8_t curV_[NCELLS], curC_[NCELLS];
  uint8_t prvV_[NCELLS], prvC_[NCELLS];
  float   reveal_ = 1.0f;
  uint32_t revealStart_ = 0;

  RGB pal_[NSLOTS];

  // Precomputed anti-aliased dot coverage, one 7x7 patch per brightness step.
  // Replaces a per-pixel sqrtf in the hot loop.
  static const int SPR_N = 32, SPR_R = 3, SPR_D = 2 * SPR_R + 1;
  uint8_t spr_[SPR_N][SPR_D * SPR_D];
  void buildSprites();
};

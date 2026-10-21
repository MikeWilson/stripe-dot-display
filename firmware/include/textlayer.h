#pragma once
#include <Arduino.h>
#include "board.h"

// ---------------------------------------------------------------------------
// Crisp text drawn at pixel resolution, composited over the dot grid.
//
// The dot lattice is the point of the headline numerals, but quantising a
// 14px label to a 7px lattice throws away most of its strokes and buys
// nothing -- small type just gets harder to read. So labels and captions are
// rasterised as solid glyphs instead, and only the big numbers stay dotty.
//
// Positions are in PIXELS, not cells. Cards convert with rowY() so the two
// layers still line up on the same grid.
// ---------------------------------------------------------------------------

struct TextItem {
  int16_t  x, y;          // top-left, pixels
  uint8_t  scale;         // 5x7 glyph scaled by this (2 -> 10x14 px)
  uint8_t  r, g, b;
  char     s[40];
};

struct TextLayer {
  static const int MAX = 10;
  TextItem item[MAX];
  int      n = 0;

  void clear() { n = 0; }

  // Width in pixels a string will occupy at `scale`.
  static int width(const char *s, uint8_t scale) {
    int len = strlen(s);
    return len ? (len * 6 - 1) * scale : 0;
  }

  void add(int x, int y, uint8_t scale, uint8_t r, uint8_t g, uint8_t b, const char *s) {
    if (n >= MAX) return;
    TextItem &t = item[n++];
    t.x = x; t.y = y; t.scale = scale;
    t.r = r; t.g = g; t.b = b;
    strlcpy(t.s, s, sizeof(t.s));
  }

  void centre(int y, uint8_t scale, uint8_t r, uint8_t g, uint8_t b, const char *s) {
    add((LCD_W - width(s, scale)) / 2, y, scale, r, g, b, s);
  }
};

// Rasterise every item that touches rows [y0, y0+h) of `band`.
void drawTextBand(uint16_t *band, int y0, int h, const TextLayer &tl);

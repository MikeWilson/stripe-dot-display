#include "textlayer.h"
#include "fonts.h"
#include "dotgrid.h"

// Solid blocky glyphs, big-endian to match the band buffer the dot renderer
// fills (see DotGrid::render -- the panel is fed raw, so nothing byte-swaps
// on the way out).
// Which grid cell sits under a pixel.
static inline int cellAt(int px, int py) {
  int gx = (px - GRID_OX + DOT_SPACING / 2) / DOT_SPACING;
  int gy = (py - GRID_OY + DOT_SPACING / 2) / DOT_SPACING;
  if (gx < 0) gx = 0; else if (gx >= GRID_W) gx = GRID_W - 1;
  if (gy < 0) gy = 0; else if (gy >= GRID_H) gy = GRID_H - 1;
  return gy * GRID_W + gx;
}

static void drawLayer(uint16_t *band, int y0, int h, const TextLayer &tl,
                      const uint8_t *onMask, uint8_t want) {
  for (int i = 0; i < tl.n; i++) {
    const TextItem &t = tl.item[i];
    const int gh = 7 * t.scale;
    if (t.y + gh <= y0 || t.y >= y0 + h) continue;      // not in this band

    const uint16_t col =
        (uint16_t)(((t.r & 0xF8) << 8) | ((t.g & 0xFC) << 3) | (t.b >> 3));
    const uint16_t be = __builtin_bswap16(col);

    int x = t.x;
    for (const char *p = t.s; *p; p++) {
      char ch = *p;
      if (ch < 32 || ch > 126) ch = '?';
      const uint8_t *glyph = FONT5X7[ch - 32];

      for (int cx = 0; cx < 5; cx++) {
        const uint8_t bits = glyph[cx];
        if (!bits) continue;
        for (int cy = 0; cy < 7; cy++) {
          if (!(bits & (1 << cy))) continue;
          // one glyph pixel -> a scale x scale block
          for (int sy = 0; sy < t.scale; sy++) {
            const int py = t.y + cy * t.scale + sy;
            if (py < y0 || py >= y0 + h || py < t.clipTop || py >= t.clipBot) continue;
            uint16_t *row = band + (size_t)(py - y0) * LCD_W;
            for (int sx = 0; sx < t.scale; sx++) {
              const int px = x + cx * t.scale + sx;
              if (px < 0 || px >= LCD_W) continue;
              if (onMask && onMask[cellAt(px, py)] != want) continue;
              row[px] = be;
            }
          }
        }
      }
      x += 6 * t.scale;
    }
  }
}

void drawTextBand(uint16_t *band, int y0, int h,
                  const TextLayer &prev, const TextLayer &cur,
                  const uint8_t *onMask) {
  if (!onMask) { drawLayer(band, y0, h, cur, nullptr, 1); return; }
  drawLayer(band, y0, h, prev, onMask, 0);   // cells still on the old frame
  drawLayer(band, y0, h, cur,  onMask, 1);   // cells that have flipped
}

#include "textlayer.h"
#include "fonts.h"

// Solid blocky glyphs, big-endian to match the band buffer the dot renderer
// fills (see DotGrid::render -- the panel is fed raw, so nothing byte-swaps
// on the way out).
void drawTextBand(uint16_t *band, int y0, int h, const TextLayer &tl) {
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
            if (py < y0 || py >= y0 + h) continue;
            uint16_t *row = band + (size_t)(py - y0) * LCD_W;
            for (int sx = 0; sx < t.scale; sx++) {
              const int px = x + cx * t.scale + sx;
              if (px < 0 || px >= LCD_W) continue;
              row[px] = be;
            }
          }
        }
      }
      x += 6 * t.scale;
    }
  }
}

#include "dotgrid.h"
#include "fonts.h"
#include <math.h>

static inline int   clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float frand() { return (float)esp_random() / 4294967295.0f; }

#if DOTGRID_CHECK_CLIP
// Host-build only: counts glyph cells placed where the round panel has no dot.
// Rings and bursts are *supposed* to fly off the edge, so only text is checked.
int g_textClip = 0;
static inline void noteGlyphCell(int x, int y) {
  const float cx = LCD_W / 2.0f, cy = LCD_H / 2.0f, rMax = LCD_W / 2.0f - 2.0f;
  float px = GRID_ORG + x * DOT_SPACING, py = GRID_ORG + y * DOT_SPACING;
  if (x < 0 || y < 0 || x >= GRID_N || y >= GRID_N ||
      sqrtf((px - cx) * (px - cx) + (py - cy) * (py - cy)) > rMax) g_textClip++;
}
#else
#define noteGlyphCell(x, y) ((void)0)
#endif

// ===========================================================================
// CellBuf
// ===========================================================================
void CellBuf::clear() {
  memset(v, 0, sizeof(v));
  memset(c, SLOT_AMBIENT, sizeof(c));
}

void CellBuf::plot(int x, int y, uint8_t val, uint8_t slot) {
  if (x < 0 || y < 0 || x >= GRID_N || y >= GRID_N) return;
  int i = y * GRID_N + x;
  if (val > v[i]) { v[i] = val; c[i] = slot; }   // brightest write wins
}

void CellBuf::hline(int x, int y, int w, uint8_t val, uint8_t slot) {
  for (int i = 0; i < w; i++) plot(x + i, y, val, slot);
}
void CellBuf::vline(int x, int y, int h, uint8_t val, uint8_t slot) {
  for (int i = 0; i < h; i++) plot(x, y + i, val, slot);
}
void CellBuf::fillRect(int x, int y, int w, int h, uint8_t val, uint8_t slot) {
  for (int j = 0; j < h; j++) hline(x, y + j, w, val, slot);
}

void CellBuf::ring(float cx, float cy, float r, float thickness, uint8_t val, uint8_t slot) {
  int lo = clampi((int)floorf(cy - r - thickness), 0, GRID_N - 1);
  int hi = clampi((int)ceilf (cy + r + thickness), 0, GRID_N - 1);
  for (int y = lo; y <= hi; y++) {
    for (int x = 0; x < GRID_N; x++) {
      float d = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy));
      float e = fabsf(d - r) / thickness;
      if (e > 1.0f) continue;
      plot(x, y, (uint8_t)(val * (1.0f - e * e)), slot);
    }
  }
}

void CellBuf::arc(float cx, float cy, float r, float a0, float a1, float thickness,
                  uint8_t val, uint8_t slot) {
  for (int y = 0; y < GRID_N; y++) {
    for (int x = 0; x < GRID_N; x++) {
      float dx = x - cx, dy = y - cy;
      float d = sqrtf(dx * dx + dy * dy);
      float e = fabsf(d - r) / thickness;
      if (e > 1.0f) continue;
      float a = atan2f(dy, dx);
      if (a < 0) a += 2 * PI;
      if (a < a0 || a > a1) continue;
      plot(x, y, (uint8_t)(val * (1.0f - e * e)), slot);
    }
  }
}

int CellBuf::microW(const char *s) {
  int n = strlen(s);
  return n ? n * 5 - 2 : 0;   // 3 inked columns + 2 of gap
}

void CellBuf::micro(int x, int y, const char *s, uint8_t val, uint8_t slot) {
  for (; *s; s++) {
    char ch = *s;
    if (ch >= 'a' && ch <= 'z') ch -= 32;
    if (ch < 0x20 || ch > 0x5A) ch = ' ';
    const uint8_t *g = FONT3X5[ch - 0x20];
    for (int col = 0; col < 3; col++)
      for (int row = 0; row < 5; row++)
        if (g[col] & (1 << row)) { noteGlyphCell(x + col, y + row); plot(x + col, y + row, val, slot); }
    x += 5;
  }
}

void CellBuf::microCenter(int y, const char *s, uint8_t val, uint8_t slot) {
  micro((GRID_N - microW(s)) / 2, y, s, val, slot);
}

int CellBuf::textW(const char *s) {
  int n = strlen(s);
  return n ? n * 6 - 1 : 0;
}

void CellBuf::text(int x, int y, const char *s, uint8_t val, uint8_t slot) {
  for (; *s; s++) {
    char ch = *s;
    if (ch < 32 || ch > 126) ch = '?';
    const uint8_t *g = FONT5X7[ch - 32];
    for (int col = 0; col < 5; col++)
      for (int row = 0; row < 7; row++)
        if (g[col] & (1 << row)) { noteGlyphCell(x + col, y + row); plot(x + col, y + row, val, slot); }
    x += 6;
  }
}

void CellBuf::textCenter(int y, const char *s, uint8_t val, uint8_t slot) {
  text((GRID_N - textW(s)) / 2, y, s, val, slot);
}

int CellBuf::bigW(const char *s) {
  int w = 0;
  for (; *s; s++) {
    int i = bigIndex(*s);
    w += (i < 0 ? 4 : BIGW[i]) + 1;
  }
  return w ? w - 1 : 0;
}

void CellBuf::big(int x, int y, const char *s, uint8_t val, uint8_t slot) {
  for (; *s; s++) {
    int i = bigIndex(*s);
    if (i < 0) { x += 5; continue; }
    int w = BIGW[i];
    for (int row = 0; row < BIGH; row++) {
      uint8_t bits = FONTBIG[i][row];
      for (int col = 0; col < w; col++)
        if (bits & (1 << (6 - col))) { noteGlyphCell(x + col, y + row); plot(x + col, y + row, val, slot); }
    }
    x += w + 1;
  }
}

void CellBuf::bigCenter(int y, const char *s, uint8_t val, uint8_t slot) {
  big((GRID_N - bigW(s)) / 2, y, s, val, slot);
}

void CellBuf::bars(int x, int y, int h, const int32_t *vals, int n, int barw, uint8_t slot) {
  int32_t peak = 1;
  for (int i = 0; i < n; i++) if (vals[i] > peak) peak = vals[i];
  const int pitch = barw + 1;
  for (int i = 0; i < n; i++) {
    int bx = x + i * pitch;
    int bh = (int)roundf((float)vals[i] / peak * h);
    if (vals[i] > 0 && bh < 1) bh = 1;
    for (int j = 0; j < bh; j++) {
      // brightest at the top of each bar, fading toward the baseline
      float t = (float)(j + 1) / bh;
      uint8_t val = (uint8_t)(85 + 170 * t);
      for (int k = 0; k < barw; k++) plot(bx + k, y + h - 1 - j, val, slot);
    }
    for (int k = 0; k < barw; k++) plot(bx + k, y + h, 65, SLOT_DIM);  // baseline
  }
}

// ===========================================================================
// DotGrid
// ===========================================================================
bool DotGrid::begin(Arduino_Canvas *canvas) {
  cv_ = canvas;
  fb_ = canvas->getFramebuffer();
  if (!fb_) return false;

  pal_[SLOT_AMBIENT] = RGB COL_AMBIENT;
  pal_[SLOT_ACCENT]  = RGB COL_ACCENT;
  pal_[SLOT_MONEY]   = RGB COL_MONEY;
  pal_[SLOT_WARN]    = RGB COL_WARN;
  pal_[SLOT_DIM]     = RGB COL_DIM;

  dots_ = (Dot *)heap_caps_malloc(sizeof(Dot) * NCELLS, MALLOC_CAP_SPIRAM);
  if (!dots_) dots_ = (Dot *)malloc(sizeof(Dot) * NCELLS);
  if (!dots_) return false;

  const float cx = LCD_W / 2.0f, cy = LCD_H / 2.0f;
  const float rMax = LCD_W / 2.0f - 2.0f;

  ndots_ = 0;
  for (int gy = 0; gy < GRID_N; gy++) {
    for (int gx = 0; gx < GRID_N; gx++) {
      float px = GRID_ORG + gx * DOT_SPACING;
      float py = GRID_ORG + gy * DOT_SPACING;
      float d  = sqrtf((px - cx) * (px - cx) + (py - cy) * (py - cy));
      if (d > rMax) continue;                       // round panel: clip corners

      Dot &dt = dots_[ndots_++];
      dt.px = (int16_t)px;
      dt.py = (int16_t)py;
      dt.idx = gy * GRID_N + gx;
      dt.bright = AMBIENT_LEVEL;
      dt.phase  = frand() * 2 * PI;
      // Reveal order: a radial sweep outward with organic grain on top, so
      // transitions read as a wipe rather than a uniform fade.
      dt.noise  = clampf(0.62f * (d / rMax) + 0.38f * frand(), 0.0f, 1.0f);
      RGB a = pal_[SLOT_AMBIENT];
      dt.r = a.r; dt.g = a.g; dt.b = a.b;
    }
  }

  memset(curV_, 0, sizeof(curV_)); memset(curC_, 0, sizeof(curC_));
  memset(prvV_, 0, sizeof(prvV_)); memset(prvC_, 0, sizeof(prvC_));
  return true;
}

void DotGrid::present(const CellBuf &next) {
  memcpy(prvV_, curV_, sizeof(curV_));
  memcpy(prvC_, curC_, sizeof(curC_));
  memcpy(curV_, next.v, sizeof(curV_));
  memcpy(curC_, next.c, sizeof(curC_));
  reveal_ = 0.0f;
  revealStart_ = millis();
}

void DotGrid::presentInstant(const CellBuf &next) {
  memcpy(curV_, next.v, sizeof(curV_));
  memcpy(curC_, next.c, sizeof(curC_));
  memcpy(prvV_, next.v, sizeof(prvV_));
  memcpy(prvC_, next.c, sizeof(prvC_));
  reveal_ = 1.0f;
}

void DotGrid::update(uint32_t now) {
  if (reveal_ < 1.0f)
    reveal_ = clampf((float)(now - revealStart_) / (float)REVEAL_MS, 0.0f, 1.0f);

  const float t = (float)now;

  for (uint16_t i = 0; i < ndots_; i++) {
    Dot &d = dots_[i];
    bool shown = reveal_ >= d.noise;
    uint8_t tv = shown ? curV_[d.idx] : prvV_[d.idx];
    uint8_t ts = shown ? curC_[d.idx] : prvC_[d.idx];

    float target;
    if (tv == 0) {
      target = AMBIENT_LEVEL;
    } else {
      // Shimmer, ported from HeroGrid: slow pulse + spatial wave + twinkle +
      // rare sparkle. Scaled by the cell's own intensity so cards can render
      // soft edges and half-lit chrome.
      float pulse   = sinf(t * SH_PULSE_SPEED + d.phase * 2.0f) * SH_PULSE_AMP;
      float wave    = sinf(t * SH_WAVE_SPEED + d.px * 0.012f + d.py * 0.010f) * SH_WAVE_AMP;
      float twinkle = sinf(t * SH_TWINKLE_SPEED + d.phase * 6.0f) > 0.8f ? SH_TWINKLE_AMP : 0.0f;
      float sparkle = frand() > SH_SPARKLE_ODDS ? frand() * 0.30f : 0.0f;
      float lit     = LIT_LEVEL + pulse + wave + twinkle + sparkle;
      target = AMBIENT_LEVEL + (lit - AMBIENT_LEVEL) * (tv / 255.0f);
    }

    d.bright += (target - d.bright) * (target > d.bright ? DOT_ATTACK : DOT_DECAY);

    RGB want = pal_[tv == 0 ? SLOT_AMBIENT : ts];
    d.r += (want.r - d.r) * 0.14f;
    d.g += (want.g - d.g) * 0.14f;
    d.b += (want.b - d.b) * 0.14f;
  }
}

// The canvas framebuffer lives in PSRAM, where a read-modify-write costs far
// more than a store. We can skip the read entirely: the grid pitch exceeds the
// widest dot, so two dots never touch the same pixel and there is nothing to
// blend against on a freshly cleared (true black) frame.
static_assert(DOT_SPACING >= 2 * (DOT_BASE_RADIUS + DOT_RADIUS_GAIN),
              "DOT_SPACING must exceed the widest dot or dots will overlap; "
              "restore the additive blend in putPx() if you want them to.");

static inline void putPx(uint16_t *p, int r, int g, int b) {
  *p = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void DotGrid::render() {
  memset(fb_, 0, (size_t)LCD_W * LCD_H * sizeof(uint16_t));

  for (uint16_t i = 0; i < ndots_; i++) {
    const Dot &d = dots_[i];
    float a = clampf(d.bright, 0.0f, 1.0f);
    if (a < 0.02f) continue;

    // Radius tracks brightness -- this is what gives the grid its texture.
    float rad = DOT_BASE_RADIUS + a * DOT_RADIUS_GAIN;

    int x0 = clampi((int)floorf(d.px - rad - 0.5f), 0, LCD_W - 1);
    int x1 = clampi((int)ceilf (d.px + rad + 0.5f), 0, LCD_W - 1);
    int y0 = clampi((int)floorf(d.py - rad - 0.5f), 0, LCD_H - 1);
    int y1 = clampi((int)ceilf (d.py + rad + 0.5f), 0, LCD_H - 1);

    for (int y = y0; y <= y1; y++) {
      float dy = (y + 0.5f) - d.py;
      uint16_t *row = fb_ + y * LCD_W;
      for (int x = x0; x <= x1; x++) {
        float dx = (x + 0.5f) - d.px;
        float dist = sqrtf(dx * dx + dy * dy);
        float cov  = rad + 0.5f - dist;        // 1px anti-aliased falloff
        if (cov <= 0.0f) continue;
        if (cov > 1.0f) cov = 1.0f;
        float f = cov * a;
        putPx(row + x, (int)(d.r * f), (int)(d.g * f), (int)(d.b * f));
      }
    }
  }
}

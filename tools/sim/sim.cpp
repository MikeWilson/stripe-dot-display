// Host-side renderer for the dot grid. Compiles the *same* dotgrid.cpp,
// cards.cpp and fonts.cpp the firmware runs, so layout and typography can be
// checked without flashing the board.
//
//   ./build.sh && ./sim <outdir>
//
// Writes one raw RGB file per screen; png.py turns them into PNGs.
#include "dotgrid.h"
#include "board.h"
#include "cards.h"
#include "state.h"
#include <string>
#include <vector>

static unsigned long g_ms = 0;
unsigned long millis() { return g_ms; }
unsigned long micros() { return g_ms * 1000; }
uint32_t esp_random() { return (uint32_t)rand() << 16 ^ (uint32_t)rand(); }
void *heap_caps_malloc(size_t n, int) { return malloc(n); }
void *heap_caps_aligned_alloc(size_t a, size_t n, int) {
  return aligned_alloc(a, (n + a - 1) / a * a);
}

// Stand-in for the panel: reassemble the bands into a full frame we can dump.
static uint16_t g_fb[LCD_W * LCD_H];
static void simSink(int y0, int h, uint16_t *px) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < LCD_W; i++)
      g_fb[(size_t)(y0 + j) * LCD_W + i] = __builtin_bswap16(px[(size_t)j * LCD_W + i]);
}
static DotGrid   grid;
static TextLayer txt, prevTxt, shownTxt;

// Settle the brightness/colour lerps so we capture the steady state rather
// than a mid-fade frame.
static void settle(int frames) {
  for (int i = 0; i < frames; i++) { g_ms += 1000 / TARGET_FPS; grid.update(g_ms); }
}

extern int g_textClip;
static int g_clipped = 0, g_mark = 0;

static void markText()  { g_mark = g_textClip; }
static void checkClipping(const CellBuf &, const std::string &name) {
  int bad = g_textClip - g_mark;
  if (bad) {
    printf("  !! %-14s %d glyph cells fall off the round panel\n", name.c_str(), bad);
    g_clipped += bad;
  }
}

static void dump(const std::string &dir, const std::string &name) {
  grid.render(&prevTxt, &shownTxt);
  const uint16_t *fb = g_fb;
  std::vector<uint8_t> rgb((size_t)LCD_W * LCD_H * 3);
  for (int i = 0; i < LCD_W * LCD_H; i++) {
    uint16_t p = fb[i];
    rgb[i * 3 + 0] = (uint8_t)(((p >> 11) & 0x1F) << 3);
    rgb[i * 3 + 1] = (uint8_t)(((p >> 5) & 0x3F) << 2);
    rgb[i * 3 + 2] = (uint8_t)((p & 0x1F) << 3);
  }
  std::string path = dir + "/" + name + ".rgb";
  FILE *f = fopen(path.c_str(), "wb");
  fwrite(rgb.data(), 1, rgb.size(), f);
  fclose(f);
  printf("  %-16s %s\n", name.c_str(), path.c_str());
}

int main(int argc, char **argv) {
  std::string dir = argc > 1 ? argv[1] : ".";
  srand(7);
  { // png.py needs the panel size; it is no longer a fixed square.
    FILE *m = fopen((dir + "/size.txt").c_str(), "w");
    fprintf(m, "%d %d\n", LCD_W, LCD_H);
    fclose(m);
  }
  if (!grid.begin(simSink)) { fprintf(stderr, "grid alloc failed\n"); return 1; }
  printf("panel %dx%d, grid %dx%d (%d cells), %dpx pitch\n",
         LCD_W, LCD_H, GRID_W, GRID_H, NCELLS, DOT_SPACING);

  Stats s;
  s.valid          = true;
  s.todayCents     = 128450;
  s.todayCount     = 7;
  s.availableCents = 4820133;
  s.pendingCents   = 219900;
  s.payoutCents    = 312845;
  snprintf(s.payoutWhen, sizeof(s.payoutWhen), "OCT 24");
  int32_t d[14] = {42000, 118000, 96500, 150000, 61000, 0, 33000,
                   187000, 92000, 143500, 76000, 205000, 88000, 128450};
  memcpy(s.days, d, sizeof(d));
  s.nDays      = 14;
  s.mrrCents   = 1849900;
  s.activeSubs = 143;
  s.epoch      = 1761062520;   // 2025-10-21 ~14:42 UTC
  s.epochAtMs  = 0;

  static CellBuf buf;
  const char *names[] = {"today", "sparkline", "balance", "payout", "clock", "mrr"};

  for (int i = 0; i < CARD__N; i++) {
    markText();
    drawCard(buf, txt, (CardId)i, s, i, CARD__N);
    checkClipping(buf, names[i]);
    grid.presentInstant(buf); prevTxt = txt; shownTxt = txt;
    settle(60);
    dump(dir, names[i]);
  }

  PaymentEvent e;
  e.cents = 4999;
  snprintf(e.who,  sizeof(e.who),  "mike@example.com");
  snprintf(e.what, sizeof(e.what), "VISA 4242");
  markText();
  drawPayment(buf, txt, e, s);
  checkClipping(buf, "payment");
  grid.presentInstant(buf); prevTxt = txt; shownTxt = txt; settle(60); dump(dir, "payment");

  PaymentEvent big;
  big.cents = 1284950;
  snprintf(big.who,  sizeof(big.who),  "acme-corp@example.com");
  snprintf(big.what, sizeof(big.what), "ANNUAL PLAN");
  markText();
  drawPayment(buf, txt, big, s);
  checkClipping(buf, "payment-large");
  grid.presentInstant(buf); prevTxt = txt; shownTxt = txt; settle(60); dump(dir, "payment-large");

  // Replay the burst at the real frame rate so the phosphor trail is honest.
  {
    const int frames = ALERT_BURST_MS * TARGET_FPS / 1000;
    for (int f = 0; f <= frames; f++) {
      float t = (float)f / frames;
      txt.clear();
      drawBurst(buf, t);
      grid.presentInstant(buf); prevTxt = txt; shownTxt = txt;
      g_ms += 1000 / TARGET_FPS;
      grid.update(g_ms);
      if (f == frames * 15 / 100 || f == frames * 35 / 100 || f == frames * 60 / 100 || f == frames * 85 / 100) {
        char n[32]; snprintf(n, sizeof(n), "burst-%02d", (int)(t * 100));
        dump(dir, n);
      }
    }
  }

  markText();
  drawBoot(buf, txt, "SYNCING", 1.4f);
  checkClipping(buf, "boot");
  grid.presentInstant(buf); prevTxt = txt; shownTxt = txt; settle(60); dump(dir, "boot");

  markText();
  drawOffline(buf, txt, 1.0f);
  checkClipping(buf, "offline");
  grid.presentInstant(buf); prevTxt = txt; shownTxt = txt; settle(60); dump(dir, "offline");

  // The wipe, frame by frame: content should be erased by a dark sweep and
  // repainted behind it, with the text going with the dots rather than popping.
  drawCard(buf, txt, CARD_TODAY, s, 0, CARD__N);
  grid.presentInstant(buf); prevTxt = txt; shownTxt = txt; settle(60);
  drawCard(buf, txt, CARD_BALANCE, s, 2, CARD__N);
  prevTxt = shownTxt; shownTxt = txt;
  grid.present(buf);
  {
    const int frames = REVEAL_MS * TARGET_FPS / 1000;
    for (int f = 1; f <= frames; f++) {
      settle(1);
      if (f == frames / 5 || f == frames * 2 / 5 ||
          f == frames * 3 / 5 || f == frames * 4 / 5) {
        char n[32]; snprintf(n, sizeof(n), "wipe-%d", f * 100 / frames);
        dump(dir, n);
      }
    }
  }

  printf(g_clipped ? "\nFAIL: %d clipped cells\n" : "\nno clipping\n", g_clipped);
  return g_clipped ? 1 : 0;
}

// Host-side renderer for the dot grid. Compiles the *same* dotgrid.cpp,
// cards.cpp and fonts.cpp the firmware runs, so layout and typography can be
// checked without flashing the board.
//
//   ./build.sh && ./sim <outdir>
//
// Writes one raw RGB file per screen; png.py turns them into PNGs.
#include "dotgrid.h"
#include "cards.h"
#include "state.h"
#include <string>
#include <vector>

static unsigned long g_ms = 0;
unsigned long millis() { return g_ms; }
uint32_t esp_random() { return (uint32_t)rand() << 16 ^ (uint32_t)rand(); }
void *heap_caps_malloc(size_t n, int) { return malloc(n); }

static Arduino_Canvas canvas(LCD_W, LCD_H);
static DotGrid grid;

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
  grid.render();
  const uint16_t *fb = canvas.getFramebuffer();
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
  if (!grid.begin(&canvas)) { fprintf(stderr, "grid alloc failed\n"); return 1; }
  printf("grid %dx%d (%d cells), %dpx pitch\n", GRID_N, GRID_N, NCELLS, DOT_SPACING);

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
    drawCard(buf, (CardId)i, s, i, CARD__N);
    checkClipping(buf, names[i]);
    grid.presentInstant(buf);
    settle(60);
    dump(dir, names[i]);
  }

  PaymentEvent e;
  e.cents = 4999;
  snprintf(e.who,  sizeof(e.who),  "mike@example.com");
  snprintf(e.what, sizeof(e.what), "VISA 4242");
  markText();
  drawPayment(buf, e, s);
  checkClipping(buf, "payment");
  grid.presentInstant(buf); settle(60); dump(dir, "payment");

  PaymentEvent big;
  big.cents = 1284950;
  snprintf(big.who,  sizeof(big.who),  "acme-corp@example.com");
  snprintf(big.what, sizeof(big.what), "ANNUAL PLAN");
  markText();
  drawPayment(buf, big, s);
  checkClipping(buf, "payment-large");
  grid.presentInstant(buf); settle(60); dump(dir, "payment-large");

  // Replay the burst at the real frame rate so the phosphor trail is honest.
  {
    const int frames = ALERT_BURST_MS * TARGET_FPS / 1000;
    for (int f = 0; f <= frames; f++) {
      float t = (float)f / frames;
      drawBurst(buf, t);
      grid.presentInstant(buf);
      g_ms += 1000 / TARGET_FPS;
      grid.update(g_ms);
      if (f == frames * 15 / 100 || f == frames * 35 / 100 || f == frames * 60 / 100 || f == frames * 85 / 100) {
        char n[32]; snprintf(n, sizeof(n), "burst-%02d", (int)(t * 100));
        dump(dir, n);
      }
    }
  }

  markText();
  drawBoot(buf, "SYNCING", 1.4f);
  checkClipping(buf, "boot");
  grid.presentInstant(buf); settle(60); dump(dir, "boot");

  markText();
  drawOffline(buf, 1.0f);
  checkClipping(buf, "offline");
  grid.presentInstant(buf); settle(60); dump(dir, "offline");

  // Mid-wipe: the staggered radial reveal between two cards.
  drawCard(buf, CARD_TODAY, s, 0, CARD__N);
  grid.presentInstant(buf); settle(60);
  drawCard(buf, CARD_BALANCE, s, 2, CARD__N);
  grid.present(buf);
  settle(10);
  dump(dir, "wipe");

  printf(g_clipped ? "\nFAIL: %d clipped cells\n" : "\nno clipping\n", g_clipped);
  return g_clipped ? 1 : 0;
}

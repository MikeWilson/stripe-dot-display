// ---------------------------------------------------------------------------
// Pin finder v4.  Build with:  pio run -e pinfinder -t upload
//
// v3 established that the panel is alive: all configs put *something* on the
// glass, so LCD data 4-7 and CS 12 are right. What is left is resolution,
// column offset and clock pin -- get the width wrong and every row lands at
// the wrong byte offset, which is exactly the "glitchy, broken up" look.
//
// So stop using colour as the signal. Each config now draws its own number in
// huge type plus a one-pixel border hugging the panel edge. The correct config
// is the one where the border is a clean rectangle touching all four sides
// with no tearing, shear or wrap.
//
// Clock dropped to 20MHz so signal integrity cannot be confused with a
// geometry error.
// ---------------------------------------------------------------------------
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>

#define HOLD_MS 9000

struct Cfg {
  const char *label;
  bool    co5300;
  int8_t  cs, sck, d0, d1, d2, d3, rst;
  int16_t w, h;
  uint8_t off;
};

static const Cfg CFGS[] = {
  {"368x448 sck11 off16", true,  12, 11, 4,5,6,7, GFX_NOT_DEFINED, 368, 448, 16},
  {"368x448 sck11 off0",  true,  12, 11, 4,5,6,7, GFX_NOT_DEFINED, 368, 448, 0},
  {"466x466 sck38 off6",  true,  12, 38, 4,5,6,7, 39,              466, 466, 6},
  {"466x466 sck11 off6",  true,  12, 11, 4,5,6,7, GFX_NOT_DEFINED, 466, 466, 6},
  {"410x502 sck11 off0",  true,  12, 11, 4,5,6,7, 8,               410, 502, 0},
  {"240x296 sck11 off0",  true,  12, 11, 4,5,6,7, GFX_NOT_DEFINED, 240, 296, 0},
};
static const int NCFG = sizeof(CFGS) / sizeof(CFGS[0]);

#define MAGIC 0xB0A5C0E4u
RTC_NOINIT_ATTR static uint32_t g_magic, g_idx;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Wire.begin(15, 14);

  if (g_magic != MAGIC) { g_magic = MAGIC; g_idx = 0; }
  if (g_idx >= (uint32_t)NCFG) g_idx = 0;
  const Cfg &c = CFGS[g_idx];
  const int n = (int)g_idx + 1;

  Serial.printf("\n[%d/%d] %s\n", n, NCFG, c.label);

  auto *bus = new Arduino_ESP32QSPI(c.cs, c.sck, c.d0, c.d1, c.d2, c.d3);
  auto *g = new Arduino_CO5300(bus, c.rst, 0, c.w, c.h, c.off, 0, 0, 0);

  if (g->begin(20000000)) {
    g->setBrightness(255);
    g->fillScreen(RGB565_BLACK);

    // Border hugging the true panel edge: shear or wrap shows up instantly.
    g->drawRect(0, 0, c.w, c.h, RGB565_WHITE);
    g->drawRect(2, 2, c.w - 4, c.h - 4, RGB565_RED);

    // Solid corner blocks -- all four present and square means geometry is right.
    const int m = 24;
    g->fillRect(0, 0, m, m, RGB565_GREEN);
    g->fillRect(c.w - m, 0, m, m, RGB565_GREEN);
    g->fillRect(0, c.h - m, m, m, RGB565_GREEN);
    g->fillRect(c.w - m, c.h - m, m, m, RGB565_GREEN);

    // The config number, big enough to read across a desk.
    g->setTextColor(RGB565_WHITE);
    g->setTextSize(9);
    g->setCursor(c.w / 2 - 28, c.h / 2 - 60);
    g->print(n);

    g->setTextSize(2);
    g->setCursor(10, c.h / 2 + 40);
    g->print(c.label);
  }

  g_idx++;
  delay(HOLD_MS);
  esp_restart();
}
void loop() {}

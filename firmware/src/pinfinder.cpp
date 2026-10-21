// ---------------------------------------------------------------------------
// Pin finder.  Build with:  pio run -e pinfinder -t upload
//
// Waveshare shipped the 1.43" AMOLED with more than one panel controller and
// more than one pin map, and the published tables disagree with each other.
// QSPI is write-only, so a wrong guess still "initialises" fine and simply
// leaves the screen black -- you cannot tell from software which one is right.
//
// So: try one candidate per boot, paint the whole screen a distinct colour,
// hold it for six seconds, then reboot into the next. Watch the screen, note
// which colour appears, and put that combination into board.h.
//
// Uses the library's own fillScreen() rather than the dot renderer, so this
// also isolates the panel question from anything in dotgrid.cpp.
// ---------------------------------------------------------------------------
#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#define HOLD_MS 6000

struct Combo {
  const char *name;
  const char *colour;
  uint16_t    rgb;
  bool        co5300;        // false = SH8601
  int8_t      cs, clk, d0, d1, d2, d3, rst, en;
};

static const Combo COMBOS[] = {
  {"A/CO5300 rst21 en42", "RED",     RGB565_RED,     true,  9, 10, 11, 12, 13, 14, 21, 42},
  {"A/SH8601 rst21 en42", "GREEN",   RGB565_GREEN,   false, 9, 10, 11, 12, 13, 14, 21, 42},
  {"B/CO5300 rst38 en43", "BLUE",    RGB565_BLUE,    true, 10, 11, 12, 13, 14, 15, 38, 43},
  {"B/SH8601 rst38 en43", "YELLOW",  RGB565_YELLOW,  false,10, 11, 12, 13, 14, 15, 38, 43},
  {"A/CO5300 rst21 no-en","MAGENTA", RGB565_MAGENTA, true,  9, 10, 11, 12, 13, 14, 21, -1},
  {"A/CO5300 no-rst en42","CYAN",    RGB565_CYAN,    true,  9, 10, 11, 12, 13, 14, -1, 42},
};
static const int NCOMBO = sizeof(COMBOS) / sizeof(COMBOS[0]);

// Survives a warm reboot, so each boot picks up where the last left off.
#define FINDER_MAGIC 0xB0A5C0DEu
RTC_NOINIT_ATTR static uint32_t g_magic;
RTC_NOINIT_ATTR static uint32_t g_idx;

void setup() {
  Serial.begin(115200);
  delay(600);                       // let USB CDC come back after the reboot

  if (g_magic != FINDER_MAGIC) { g_magic = FINDER_MAGIC; g_idx = 0; }
  if (g_idx >= (uint32_t)NCOMBO) g_idx = 0;
  const Combo &c = COMBOS[g_idx];

  Serial.printf("\n[%lu/%d] %s  -> expect a solid %s screen\n",
                (unsigned long)g_idx + 1, NCOMBO, c.name, c.colour);

  // Some revisions gate the panel rail behind an enable pin. Drive both
  // candidates high regardless -- neither is used for anything else here.
  for (int p : {42, 43}) { pinMode(p, OUTPUT); digitalWrite(p, HIGH); }
  delay(50);
  if (c.en >= 0) { pinMode(c.en, OUTPUT); digitalWrite(c.en, HIGH); delay(50); }

  auto *bus = new Arduino_ESP32QSPI(c.cs, c.clk, c.d0, c.d1, c.d2, c.d3);

  // setBrightness() is on the concrete drivers, not the Arduino_GFX base,
  // so keep a typed pointer alongside the generic one.
  Arduino_CO5300 *co = nullptr;
  Arduino_SH8601 *sh = nullptr;
  Arduino_GFX    *panel;
  if (c.co5300) {
    co = new Arduino_CO5300(bus, c.rst, 0, 466, 466, 6, 0, 6, 0);
    panel = co;
  } else {
    sh = new Arduino_SH8601(bus, c.rst, 0, 466, 466, 6, 0, 6, 0);
    panel = sh;
  }

  bool ok = panel->begin(40000000);     // conservative clock while probing
  Serial.printf("      begin() = %s (write-only bus: true proves nothing)\n",
                ok ? "true" : "false");

  if (ok) {
    if (co) co->setBrightness(255);
    if (sh) sh->setBrightness(255);
    panel->fillScreen(c.rgb);
    // A white bar across the middle, so a wrong byte order still shows *something*
    panel->fillRect(0, 210, 466, 46, RGB565_WHITE);
  }

  g_idx++;
  delay(HOLD_MS);
  Serial.println("      next...");
  esp_restart();
}

void loop() {}

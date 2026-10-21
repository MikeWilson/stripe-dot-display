#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "board.h"
#include "config.h"
#include "dotgrid.h"
#include "cards.h"
#include "relay.h"
#include "state.h"
#include "demo.h"

#if PANEL_VARIANT == PANEL_CO5300
  #define PanelDriver Arduino_CO5300
#else
  #define PanelDriver Arduino_SH8601
#endif

static Arduino_DataBus *bus   = nullptr;
static PanelDriver     *panel = nullptr;

// Push a band with one DMA transfer. Going through draw16bitRGBBitmap()
// instead would re-walk every pixel to byte-swap it -- the band is already
// in the panel's byte order, so there is nothing to do but send it.
static void pushBand(int y0, int h, uint16_t *px) {
  panel->startWrite();
  panel->writeAddrWindow(0, y0, LCD_W, h);
  bus->writeBytes((uint8_t *)px, (uint32_t)LCD_W * h * sizeof(uint16_t));
  panel->endWrite();
}

static DotGrid grid;
static Stats   gStats;
#if !DEMO_MODE
static Relay   relay;
#endif

// Two frames: what we just composed, and what the grid is currently showing.
static CellBuf work;
static CellBuf shown;

static const CardId kIdle[] = { IDLE_CARDS };
static const int    kNIdle  = sizeof(kIdle) / sizeof(kIdle[0]);

enum Mode { M_BOOT, M_IDLE, M_BURST, M_ALERT, M_OFFLINE };
static Mode     mode       = M_BOOT;
static int      card       = 0;
static uint32_t modeStart  = 0;
static uint32_t cardStart  = 0;
static bool     forceWipe  = false;      // next present() uses the reveal
static PaymentEvent current;

static inline bool sameAsShown() {
  return memcmp(work.v, shown.v, sizeof(work.v)) == 0 &&
         memcmp(work.c, shown.c, sizeof(work.c)) == 0;
}

static void commit() {
  if (forceWipe) {
    grid.present(work);
    forceWipe = false;
  } else if (!sameAsShown()) {
    grid.presentInstant(work);
  } else {
    return;
  }
  memcpy(shown.v, work.v, sizeof(work.v));
  memcpy(shown.c, work.c, sizeof(work.c));
}

static void enter(Mode m) { mode = m; modeStart = millis(); forceWipe = true; }

void setup() {
  Serial.begin(115200);

#if PIN_LCD_EN >= 0
  pinMode(PIN_LCD_EN, OUTPUT);
  digitalWrite(PIN_LCD_EN, HIGH);
  delay(20);
#endif

  bus = new Arduino_ESP32QSPI(PIN_LCD_CS, PIN_LCD_CLK,
                              PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2, PIN_LCD_D3);
  panel = new PanelDriver(bus, PIN_LCD_RST, 0, LCD_W, LCD_H,
                          LCD_COL_OFFSET, 0, LCD_COL_OFFSET, 0);
  if (!panel->begin(LCD_QSPI_HZ)) {
    Serial.println("display init failed -- check PANEL_VARIANT / PINMAP_VARIANT in board.h");
    while (true) delay(1000);
  }
  panel->setBrightness(SCREEN_BRIGHTNESS);
  panel->fillScreen(RGB565_BLACK);

  if (!grid.begin(pushBand)) {
    Serial.println("dot grid alloc failed");
    while (true) delay(1000);
  }
  Serial.printf("grid %dx%d, %d cells\n", GRID_N, GRID_N, NCELLS);
  Serial.printf("cpu %lu MHz, flash %lu MHz, psram %s\n",
                (unsigned long)getCpuFrequencyMhz(),
                (unsigned long)(ESP.getFlashChipSpeed() / 1000000),
                psramFound() ? "yes" : "no");

#if DEMO_MODE
  demoBegin(gStats);
  Serial.println("DEMO_MODE: no WiFi, synthetic stats, fake sales");
#else
  relay.begin();
#endif
  enter(M_BOOT);
}

void loop() {
  const uint32_t now = millis();

#if DEMO_MODE
  demoTick(gStats);
  const bool wifiUp = true;
  const bool gotPayment = demoPayment(current);
#else
  relay.loop();
  gStats = relay.stats();
  const bool wifiUp = relay.wifiUp();
  const bool gotPayment = relay.takePayment(current);
#endif

  const Stats &st = gStats;
  const float  t  = now / 1000.0f;

  // --- mode transitions ----------------------------------------------------
  if (mode != M_BOOT && gotPayment) enter(M_BURST);

  switch (mode) {
    case M_BOOT:
      if (st.valid) { card = 0; cardStart = now; enter(M_IDLE); }
      else if (!wifiUp && now - modeStart > 20000) enter(M_OFFLINE);
      break;

    case M_OFFLINE:
      if (wifiUp) enter(M_BOOT);
      break;

    case M_BURST:
      if (now - modeStart >= ALERT_BURST_MS) enter(M_ALERT);
      break;

    case M_ALERT:
      if (now - modeStart >= ALERT_HOLD_MS) { cardStart = now; enter(M_IDLE); }
      break;

    case M_IDLE:
      if (!wifiUp && !st.valid) enter(M_OFFLINE);
      else if (now - cardStart >= CARD_DWELL_MS) {
        card = (card + 1) % kNIdle;
        cardStart = now;
        forceWipe = true;
      }
      break;
  }

  // --- compose -------------------------------------------------------------
  switch (mode) {
    case M_BOOT:
      drawBoot(work, wifiUp ? "SYNCING" : "WIFI", t);
      break;
    case M_OFFLINE:
      drawOffline(work, t);
      break;
    case M_BURST:
      drawBurst(work, (float)(now - modeStart) / (float)ALERT_BURST_MS);
      break;
    case M_ALERT:
      drawPayment(work, current, st);
      break;
    case M_IDLE:
      drawCard(work, kIdle[card], st, card, kNIdle);
      break;
  }

#if PROFILE_FRAME
  uint32_t t0 = micros();
  commit();
  uint32_t t1 = micros();
  grid.update(now);
  uint32_t t2 = micros();
  grid.render();            // renders and pushes bands straight to the panel
  uint32_t t3 = micros();
  static uint32_t acc[3] = {0}, accN = 0;
  acc[0] += t1 - t0; acc[1] += t2 - t1; acc[2] += t3 - t2;
  if (++accN == 30) {
    Serial.printf("compose %lu  update %lu  band-draw %lu  panel-push %lu  (us/frame)\n",
                  acc[0] / 30, acc[1] / 30, (unsigned long)grid.tDraw,
                  (unsigned long)grid.tPush);
    acc[0] = acc[1] = acc[2] = accN = 0;
  }
#else
  commit();
  grid.update(now);
  grid.render();
#endif

  // Health heartbeat: the only way to see framerate and memory headroom once
  // the board is off the bench.
  static uint32_t fpsAt = 0;
  static uint16_t frames = 0;
  frames++;
  if (now - fpsAt >= 5000) {
    Serial.printf("fps %.1f  mode %d  heap %u  psram %u\n",
                  frames * 1000.0f / (now - fpsAt), (int)mode,
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
    fpsAt = now;
    frames = 0;
  }

  // Pace to TARGET_FPS without ever sleeping when we're already behind.
  static uint32_t nextFrame = 0;
  const uint32_t period = 1000 / TARGET_FPS;
  uint32_t end = millis();
  if ((int32_t)(nextFrame - end) > 0) delay(nextFrame - end);
  nextFrame = (end > nextFrame + period) ? end + period : nextFrame + period;
}

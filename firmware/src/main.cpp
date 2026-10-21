#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "board.h"
#include "config.h"
#include "dotgrid.h"
#include "cards.h"
#include "relay.h"
#include "state.h"

#if PANEL_VARIANT == PANEL_CO5300
  #define PanelDriver Arduino_CO5300
#else
  #define PanelDriver Arduino_SH8601
#endif

static Arduino_DataBus *bus    = nullptr;
static PanelDriver     *panel  = nullptr;
static Arduino_Canvas  *canvas = nullptr;

static DotGrid grid;
static Relay   relay;

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
  canvas = new Arduino_Canvas(LCD_W, LCD_H, panel);

  if (!canvas->begin(LCD_QSPI_HZ)) {
    Serial.println("display init failed -- check PANEL_VARIANT / PINMAP_VARIANT in board.h");
    while (true) delay(1000);
  }
  panel->setBrightness(SCREEN_BRIGHTNESS);
  canvas->fillScreen(BLACK);
  canvas->flush();

  if (!grid.begin(canvas)) {
    Serial.println("dot grid alloc failed");
    while (true) delay(1000);
  }
  Serial.printf("grid %dx%d, %d cells\n", GRID_N, GRID_N, NCELLS);

  relay.begin();
  enter(M_BOOT);
}

void loop() {
  const uint32_t now = millis();
  relay.loop();

  const Stats &st = relay.stats();
  const float  t  = now / 1000.0f;

  // --- mode transitions ----------------------------------------------------
  PaymentEvent pending;
  bool hasPayment = relay.takePayment(pending);

  if (hasPayment && mode != M_BOOT) {
    current = pending;
    enter(M_BURST);
  }

  switch (mode) {
    case M_BOOT:
      if (st.valid) { card = 0; cardStart = now; enter(M_IDLE); }
      else if (!relay.wifiUp() && now - modeStart > 20000) enter(M_OFFLINE);
      break;

    case M_OFFLINE:
      if (relay.wifiUp()) enter(M_BOOT);
      break;

    case M_BURST:
      if (now - modeStart >= ALERT_BURST_MS) enter(M_ALERT);
      break;

    case M_ALERT:
      if (now - modeStart >= ALERT_HOLD_MS) { cardStart = now; enter(M_IDLE); }
      break;

    case M_IDLE:
      if (!relay.wifiUp() && !st.valid) enter(M_OFFLINE);
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
      drawBoot(work, relay.wifiUp() ? "SYNCING" : "WIFI", t);
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

  commit();
  grid.update(now);
  grid.render();
  canvas->flush();

  // Pace to TARGET_FPS without ever sleeping when we're already behind.
  static uint32_t nextFrame = 0;
  const uint32_t period = 1000 / TARGET_FPS;
  uint32_t end = millis();
  if ((int32_t)(nextFrame - end) > 0) delay(nextFrame - end);
  nextFrame = (end > nextFrame + period) ? end + period : nextFrame + period;
}

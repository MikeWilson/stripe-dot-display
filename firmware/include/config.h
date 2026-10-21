#pragma once
// ===========================================================================
//  EDIT THIS FILE. Everything you'd want to tweak lives here.
// ===========================================================================

// --- Demo mode --------------------------------------------------------------
// 1 = no WiFi, no relay, no Stripe: synthetic stats and a fake sale every few
// seconds. Flash this first on a new board -- it checks the panel, the pin map
// and the framerate without needing any of the cloud setup.
#define DEMO_MODE              1
#define DEMO_PAYMENT_EVERY_MS  14000

// --- Credentials ------------------------------------------------------------
// Real values belong in secrets.h, which is gitignored. Copy
// secrets.example.h to secrets.h and fill it in. Until you do, these
// placeholders keep the repo building, and DEMO_MODE never reads them.
#if defined(__has_include)
  #if __has_include("secrets.h")
    #include "secrets.h"
  #endif
#endif

#ifndef WIFI_SSID
  #define WIFI_SSID      "your-ssid"
#endif
#ifndef WIFI_PASS
  #define WIFI_PASS      "your-password"
#endif
#ifndef RELAY_HOST
  #define RELAY_HOST     "stripe-dots.YOURNAME.workers.dev"
#endif
#ifndef RELAY_PORT
  #define RELAY_PORT     443
#endif
#ifndef DEVICE_TOKEN
  #define DEVICE_TOKEN   "paste-the-DEVICE_TOKEN-secret-here"
#endif

// --- Look -------------------------------------------------------------------
#define DOT_SPACING      8      // px between dots. 6 = fine, 8 = default, 12 = chunky
#define DOT_BASE_RADIUS  1.25f  // px radius of a fully-dim dot
#define DOT_RADIUS_GAIN  1.55f  // extra px radius at full brightness
#define AMBIENT_LEVEL    0.085f // brightness of unlit background dots
#define LIT_LEVEL        0.78f  // brightness of lit dots before shimmer
#define SCREEN_BRIGHTNESS 180   // AMOLED panel brightness, 0-255

// Dots light fast and fade slow, like phosphor. A symmetric lerp makes fast
// animation (the payment burst) wash out, because a travelling ring only
// touches each dot for a frame or two.
#define DOT_ATTACK       0.60f
#define DOT_DECAY        0.22f

// Shimmer: ported from the docs-graph HeroGrid dark-mode constants.
#define SH_PULSE_AMP     0.15f
#define SH_WAVE_AMP      0.12f
#define SH_TWINKLE_AMP   0.12f
#define SH_SPARKLE_ODDS  0.985f
#define SH_PULSE_SPEED   0.005f
#define SH_WAVE_SPEED    0.004f
#define SH_TWINKLE_SPEED 0.020f

// Palette (r,g,b)
#define COL_AMBIENT      {120, 126, 148}   // cool grey background dots
#define COL_ACCENT       {99,  91,  255}   // Stripe purple #635BFF
#define COL_MONEY        {52,  211, 153}   // emerald, used for incoming payments
#define COL_WARN         {245, 158, 11}    // amber
#define COL_DIM          {150, 158, 190}   // labels / secondary text

// --- Idle rotation ----------------------------------------------------------
// Delete lines, reorder them, or duplicate them. Order here is the order shown.
#define IDLE_CARDS       CARD_TODAY,      \
                         CARD_SPARKLINE,  \
                         CARD_BALANCE,    \
                         CARD_PAYOUT,     \
                         CARD_CLOCK

#define CARD_DWELL_MS      8000   // how long each idle card holds
#define REVEAL_MS          650    // staggered dot wipe between cards

// --- Payment alert ----------------------------------------------------------
#define ALERT_HOLD_MS      11000  // how long a payment stays on screen
#define ALERT_BURST_MS     650    // the expanding ring before the amount lands
#define ALERT_MIN_CENTS    0      // ignore payments smaller than this
#define SHOW_CUSTOMER      1      // show payer email/name under the amount

// --- Housekeeping -----------------------------------------------------------
#define SNAPSHOT_EVERY_MS  60000  // refresh idle stats from the relay
#define TARGET_FPS         30
#define PROFILE_FRAME      0      // 1 = log per-stage frame timing over serial
#define CURRENCY_SYMBOL    "$"
#define TZ_OFFSET_SECONDS  (-8 * 3600)   // for the clock card + "today"

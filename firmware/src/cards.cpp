#include "cards.h"
#include <math.h>
#include <time.h>

// --- Layout -----------------------------------------------------------------
// Portrait rectangle (52 x 64 cells at the default pitch). Rows scale with
// GRID_H so changing DOT_SPACING doesn't break the composition.
#define ROW_LABEL  (GRID_H * 14 / 64)
#define ROW_BIG    (GRID_H * 23 / 64)
#define ROW_SUB    (GRID_H * 38 / 64)
#define ROW_SUB2   (GRID_H * 45 / 64)
#define ROW_PAGER  (GRID_H * 53 / 64)
#define CENX       (GRID_W / 2)
#define CENY       (GRID_H / 2)

// Palette as plain values, for the text layer (the dot layer uses slots).
static const RGB C_DIM = COL_DIM, C_MONEY = COL_MONEY, C_WARN = COL_WARN;
static inline RGB shade(RGB c, float k) {
  return RGB{(uint8_t)(c.r * k), (uint8_t)(c.g * k), (uint8_t)(c.b * k)};
}
// Cell row -> pixel row, so both layers sit on the same grid.
static inline int rowY(int row) { return GRID_OY + row * DOT_SPACING; }

#define TXT(tl, row, scale, col, str) \
  (tl).centre(rowY(row), (scale), (col).r, (col).g, (col).b, (str))

static const char *MON[12] = {"JAN","FEB","MAR","APR","MAY","JUN",
                              "JUL","AUG","SEP","OCT","NOV","DEC"};

// Solid text is far more compact than dot-quantised text, so captions that
// used to need truncating to ~10 characters now fit whole.
static int textFits(uint8_t scale) { return (LCD_W - 16) / (6 * scale); }

// Copies at most `maxc` chars, and if the source is an email keeps the part
// before the @ -- "mike" beats "mike@exampl".
static void fitText(char *dst, size_t n, const char *src, int maxc) {
  char tmp[64];
  strlcpy(tmp, src, sizeof(tmp));
  if ((int)strlen(tmp) > maxc) {
    char *at = strchr(tmp, '@');
    if (at) *at = '\0';
  }
  if (maxc < 1) maxc = 1;
  if ((int)strlen(tmp) > maxc) tmp[maxc] = '\0';
  strlcpy(dst, tmp, n);
}

// A dim rectangle of dots just inside the edge, so the composition has a
// frame to sit in. (On the old round panel this was a ring.)
static void bezel(CellBuf &c, uint8_t val = 55) {
  c.hline(0, 0,          GRID_W, val, SLOT_DIM);
  c.hline(0, GRID_H - 1, GRID_W, val, SLOT_DIM);
  c.vline(0, 0,          GRID_H, val, SLOT_DIM);
  c.vline(GRID_W - 1, 0, GRID_H, val, SLOT_DIM);
}

static void pager(CellBuf &c, int page, int pages) {
  if (pages < 2) return;
  int w = pages * 3 - 1;
  int x = (GRID_W - w) / 2;
  for (int i = 0; i < pages; i++)
    c.plot(x + i * 3, ROW_PAGER, i == page ? 255 : 80,
           i == page ? SLOT_ACCENT : SLOT_DIM);
}

// Type hierarchy, coarse to fine:
//   ROW_LABEL  3x5 micro, all caps  -- what this card is
//   ROW_BIG    7x11 display         -- the number
//   ROW_SUB    5x7 or 3x5           -- secondary value (5x7 for money: the
//                                      3x5 digits are too ambiguous)
//   ROW_SUB2   3x5 micro            -- caption
static void head(CellBuf &c, TextLayer &t, const char *label,
                 const char *big, uint8_t slot) {
  TXT(t, ROW_LABEL, 2, shade(C_DIM, 0.80f), label);   // crisp
  c.bigCenter(ROW_BIG, big, 255, slot);               // dots
}

// ---------------------------------------------------------------------------
void drawCard(CellBuf &c, TextLayer &t, CardId id, const Stats &s, int page, int pages) {
  c.clear();
  t.clear();
  bezel(c);
  char big[32], sub[40];
  sub[0] = '\0';

  switch (id) {
    case CARD_TODAY:
      fmtBigMoney(s.todayCents, big, sizeof(big));
      snprintf(sub, sizeof(sub), "%ld PAYMENT%s", (long)s.todayCount,
               s.todayCount == 1 ? "" : "S");
      head(c, t, "TODAY", big, SLOT_ACCENT);
      TXT(t, ROW_SUB, 3, C_DIM, sub);
      break;

    case CARD_SPARKLINE: {
      TXT(t, ROW_LABEL, 2, shade(C_DIM, 0.80f), "14 DAYS");
      int n = s.nDays > 0 ? s.nDays : 14;
      if (n > 14) n = 14;
      const int barw = 2, pitch = barw + 1;
      int w = n * pitch - 1;
      int h = GRID_H * 14 / 64;
      c.bars((GRID_W - w) / 2, ROW_BIG, h, s.days, n, barw, SLOT_ACCENT);
      int64_t tot = 0;
      for (int i = 0; i < n; i++) tot += s.days[i];
      fmtBigMoney(tot, sub, sizeof(sub));
      TXT(t, ROW_SUB + 1, 3, C_DIM, sub);
      break;
    }

    case CARD_BALANCE:
      fmtBigMoney(s.availableCents, big, sizeof(big));
      head(c, t, "AVAILABLE", big, SLOT_ACCENT);
      if (s.pendingCents > 0) {
        fmtBigMoney(s.pendingCents, sub, sizeof(sub));
        TXT(t, ROW_SUB,      3, C_DIM,                sub);
        TXT(t, ROW_SUB2 + 1, 2, shade(C_DIM, 0.60f), "PENDING");
      }
      break;

    case CARD_PAYOUT:
      fmtBigMoney(s.payoutCents, big, sizeof(big));
      snprintf(sub, sizeof(sub), "%s", s.payoutWhen[0] ? s.payoutWhen : "NO PAYOUT");
      head(c, t, "PAYOUT", big, SLOT_WARN);
      TXT(t, ROW_SUB, 3, C_DIM, sub);
      break;

    case CARD_MRR:
      fmtBigMoney(s.mrrCents, big, sizeof(big));
      snprintf(sub, sizeof(sub), "%ld ACTIVE", (long)s.activeSubs);
      head(c, t, "MRR", big, SLOT_ACCENT);
      TXT(t, ROW_SUB, 3, C_DIM, sub);
      break;

    case CARD_CLOCK: {
      uint32_t e = nowEpoch(s);
      if (!e) { head(c, t, "CLOCK", "--:--", SLOT_DIM); break; }
      time_t tsec = (time_t)(e + TZ_OFFSET_SECONDS);   // don't shadow TextLayer &t
      struct tm tm; gmtime_r(&tsec, &tm);
      int h12 = tm.tm_hour % 12; if (h12 == 0) h12 = 12;
      snprintf(big,  sizeof(big),  "%d:%02d", h12, tm.tm_min);
      snprintf(sub,  sizeof(sub),  "%s %d", MON[tm.tm_mon], tm.tm_mday);
      head(c, t, tm.tm_hour < 12 ? "AM" : "PM", big, SLOT_ACCENT);
      TXT(t, ROW_SUB, 3, C_DIM, sub);
      break;
    }

    default: break;
  }

  if (!s.valid) TXT(t, ROW_SUB2 + 2, 2, shade(C_WARN, 0.75f), "STALE");
  pager(c, page, pages);
}

// ---------------------------------------------------------------------------
// The "ka-ching". Three beats inside ALERT_BURST_MS:
//   1. the whole face lifts for ~100ms, brightest at the centre
//   2. a full-brightness rim races outward, a dim echo trailing it
//   3. both leave the panel and the amount wipes in behind them
// The rim holds 255 the whole way -- fading it as it travels is what made an
// earlier version read as a vague cloud instead of a shockwave.
void drawBurst(CellBuf &c, float t) {
  c.clear();
  bezel(c, 45);

  const float ease = powf(t, 0.70f);
  // Reach past the corner so the ring fully leaves a rectangular panel.
  const float rMax = sqrtf((float)(CENX * CENX + CENY * CENY)) + 3.0f;
  const float r0   = ease * rMax;

  // A tight, bright core for the first third. An earlier version lifted the
  // whole face instead, which flattened the contrast the rim needs to read.
  if (t < 0.35f) {
    float k = 1.0f - t / 0.35f;
    c.ring(CENX, CENY, 0.0f, 2.0f + 6.0f * k, 255, SLOT_MONEY);  // filled blob
  }

  const float out = t > 0.75f ? (1.0f - (t - 0.75f) / 0.25f) : 1.0f;
  if (r0 > 0.5f)        c.ring(CENX, CENY, r0,        4.0f, (uint8_t)(255 * out), SLOT_MONEY);
  if (r0 - 8.0f > 0.5f) c.ring(CENX, CENY, r0 - 8.0f, 2.0f, (uint8_t)(120 * out), SLOT_MONEY);
}

// ---------------------------------------------------------------------------
void drawPayment(CellBuf &c, TextLayer &t, const PaymentEvent &e, const Stats &s) {
  (void)s;
  c.clear();
  t.clear();
  bezel(c, 70);

  char big[32];
  fmtBigMoney(e.cents, big, sizeof(big));
  const uint8_t slot = e.refund ? SLOT_WARN : SLOT_MONEY;

  TXT(t, ROW_LABEL, 2, e.refund ? C_WARN : C_MONEY, e.refund ? "REFUND" : "PAYMENT");
  c.bigCenter(ROW_BIG, big, 255, slot);

#if SHOW_CUSTOMER
  char line[48];
  if (e.who[0]) {
    // Solid text fits the whole address now; no need to drop the domain.
    fitText(line, sizeof(line), e.who, textFits(2));
    TXT(t, ROW_SUB, 2, C_DIM, line);
  }
  if (e.what[0]) {
    fitText(line, sizeof(line), e.what, textFits(2));
    TXT(t, ROW_SUB2, 2, shade(C_DIM, 0.60f), line);
  }
#endif
}

// ---------------------------------------------------------------------------
void drawBoot(CellBuf &c, TextLayer &tl, const char *line, float t) {
  c.clear();
  tl.clear();
  bezel(c, 45);
  float a0 = fmodf(t * 2.2f, 2 * PI);                // sweep while connecting
  c.arc(CENX, CENY, CENX - 4.0f, a0, a0 + 1.1f, 1.4f, 230, SLOT_ACCENT);
  TXT(tl, CENY - 2, 3, C_DIM, line);
}

void drawOffline(CellBuf &c, TextLayer &tl, float t) {
  c.clear();
  tl.clear();
  bezel(c, 55);
  float pulse = 0.45f + 0.30f * sinf(t * 2.0f);
  TXT(tl, CENY - 7, 3, C_WARN, "OFFLINE");
  TXT(tl, CENY + 2, 2, shade(C_DIM, pulse), "RETRYING");
}

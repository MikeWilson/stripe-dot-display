#include "cards.h"
#include <math.h>
#include <time.h>

// --- Layout -----------------------------------------------------------------
// Rows scale with GRID_N so changing DOT_SPACING doesn't break the layout.
#define ROW_LABEL  (GRID_N * 14 / 59)
#define ROW_BIG    (GRID_N * 22 / 59)
#define ROW_SUB    (GRID_N * 38 / 59)
#define ROW_SUB2   (GRID_N * 44 / 59)
#define ROW_PAGER  (GRID_N * 50 / 59)
#define CEN        (GRID_N / 2)

static const char *MON[12] = {"JAN","FEB","MAR","APR","MAY","JUN",
                              "JUL","AUG","SEP","OCT","NOV","DEC"};

// Usable width, in cells, of a band `h` rows tall starting at `row`. The panel
// is round, so a line near the top or bottom has far less room than the middle.
static int chordAt(int row, int h) {
  const float r = CEN - 1.0f;
  float worst = fmaxf(fabsf((float)(row - CEN)), fabsf((float)(row + h - 1 - CEN)));
  float v = r * r - worst * worst;
  return v <= 0 ? 0 : (int)(2.0f * sqrtf(v));
}
static int microFits(int row) { return chordAt(row, 5) / 5; }   // chars

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

// A dim ring just inside the bezel, so the round panel has an edge to hold.
static void bezel(CellBuf &c, uint8_t val = 55) {
  c.ring(CEN, CEN, CEN - 0.6f, 0.9f, val, SLOT_DIM);
}

static void pager(CellBuf &c, int page, int pages) {
  if (pages < 2) return;
  int w = pages * 3 - 1;
  int x = (GRID_N - w) / 2;
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
static void head(CellBuf &c, const char *label, const char *big, uint8_t slot) {
  c.microCenter(ROW_LABEL, label, 205, SLOT_DIM);
  c.bigCenter  (ROW_BIG,   big,   255, slot);
}

// ---------------------------------------------------------------------------
void drawCard(CellBuf &c, CardId id, const Stats &s, int page, int pages) {
  c.clear();
  bezel(c);
  char big[32], sub[40];
  sub[0] = '\0';

  switch (id) {
    case CARD_TODAY:
      fmtBigMoney(s.todayCents, big, sizeof(big));
      snprintf(sub, sizeof(sub), "%ld PAYMENT%s", (long)s.todayCount,
               s.todayCount == 1 ? "" : "S");
      head(c, "TODAY", big, SLOT_ACCENT);
      c.microCenter(ROW_SUB, sub, 190, SLOT_DIM);
      break;

    case CARD_SPARKLINE: {
      c.microCenter(ROW_LABEL, "14 DAYS", 205, SLOT_DIM);
      int n = s.nDays > 0 ? s.nDays : 14;
      if (n > 14) n = 14;
      const int barw = 2, pitch = barw + 1;
      int w = n * pitch - 1;
      int h = GRID_N * 14 / 59;
      c.bars((GRID_N - w) / 2, ROW_BIG, h, s.days, n, barw, SLOT_ACCENT);
      int64_t tot = 0;
      for (int i = 0; i < n; i++) tot += s.days[i];
      fmtBigMoney(tot, sub, sizeof(sub));
      c.textCenter(ROW_SUB + 1, sub, 185, SLOT_DIM);
      break;
    }

    case CARD_BALANCE:
      fmtBigMoney(s.availableCents, big, sizeof(big));
      head(c, "AVAILABLE", big, SLOT_ACCENT);
      if (s.pendingCents > 0) {
        fmtBigMoney(s.pendingCents, sub, sizeof(sub));
        c.textCenter (ROW_SUB,      sub,       185, SLOT_DIM);
        c.microCenter(ROW_SUB2 + 1, "PENDING", 125, SLOT_DIM);
      }
      break;

    case CARD_PAYOUT:
      fmtBigMoney(s.payoutCents, big, sizeof(big));
      snprintf(sub, sizeof(sub), "%s", s.payoutWhen[0] ? s.payoutWhen : "NO PAYOUT");
      head(c, "PAYOUT", big, SLOT_WARN);
      c.microCenter(ROW_SUB, sub, 190, SLOT_DIM);
      break;

    case CARD_MRR:
      fmtBigMoney(s.mrrCents, big, sizeof(big));
      snprintf(sub, sizeof(sub), "%ld ACTIVE", (long)s.activeSubs);
      head(c, "MRR", big, SLOT_ACCENT);
      c.microCenter(ROW_SUB, sub, 190, SLOT_DIM);
      break;

    case CARD_CLOCK: {
      uint32_t e = nowEpoch(s);
      if (!e) { head(c, "CLOCK", "--:--", SLOT_DIM); break; }
      time_t t = (time_t)(e + TZ_OFFSET_SECONDS);
      struct tm tm; gmtime_r(&t, &tm);
      int h12 = tm.tm_hour % 12; if (h12 == 0) h12 = 12;
      snprintf(big,  sizeof(big),  "%d:%02d", h12, tm.tm_min);
      snprintf(sub,  sizeof(sub),  "%s %d", MON[tm.tm_mon], tm.tm_mday);
      head(c, tm.tm_hour < 12 ? "AM" : "PM", big, SLOT_ACCENT);
      c.microCenter(ROW_SUB, sub, 190, SLOT_DIM);
      break;
    }

    default: break;
  }

  if (!s.valid) c.microCenter(ROW_SUB2 + 2, "STALE", 130, SLOT_WARN);
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
  const float r0   = ease * (CEN + 4);

  // A tight, bright core for the first third. An earlier version lifted the
  // whole face instead, which flattened the contrast the rim needs to read.
  if (t < 0.35f) {
    float k = 1.0f - t / 0.35f;
    c.ring(CEN, CEN, 0.0f, 2.0f + 6.0f * k, 255, SLOT_MONEY);   // filled blob
  }

  const float out = t > 0.75f ? (1.0f - (t - 0.75f) / 0.25f) : 1.0f;
  if (r0 > 0.5f)        c.ring(CEN, CEN, r0,        4.0f, (uint8_t)(255 * out), SLOT_MONEY);
  if (r0 - 8.0f > 0.5f) c.ring(CEN, CEN, r0 - 8.0f, 2.0f, (uint8_t)(120 * out), SLOT_MONEY);
}

// ---------------------------------------------------------------------------
void drawPayment(CellBuf &c, const PaymentEvent &e, const Stats &s) {
  (void)s;
  c.clear();
  bezel(c, 70);

  char big[32];
  fmtBigMoney(e.cents, big, sizeof(big));
  const uint8_t slot = e.refund ? SLOT_WARN : SLOT_MONEY;

  c.microCenter(ROW_LABEL, e.refund ? "REFUND" : "PAYMENT", 230, slot);
  c.bigCenter  (ROW_BIG, big, 255, slot);

#if SHOW_CUSTOMER
  char line[48];
  if (e.who[0]) {
    fitText(line, sizeof(line), e.who, microFits(ROW_SUB));
    c.microCenter(ROW_SUB, line, 195, SLOT_DIM);
  }
  if (e.what[0]) {
    fitText(line, sizeof(line), e.what, microFits(ROW_SUB2));
    c.microCenter(ROW_SUB2, line, 130, SLOT_DIM);
  }
#endif
}

// ---------------------------------------------------------------------------
void drawBoot(CellBuf &c, const char *line, float t) {
  c.clear();
  bezel(c, 45);
  float a0 = fmodf(t * 2.2f, 2 * PI);                // sweep while connecting
  c.arc(CEN, CEN, CEN - 4.0f, a0, a0 + 1.1f, 1.4f, 230, SLOT_ACCENT);
  c.microCenter(CEN - 2, line, 205, SLOT_DIM);
}

void drawOffline(CellBuf &c, float t) {
  c.clear();
  bezel(c, 55);
  uint8_t pulse = (uint8_t)(110 + 70 * sinf(t * 2.0f));
  c.microCenter(CEN - 7, "OFFLINE", 220, SLOT_WARN);
  c.microCenter(CEN + 2, "RETRYING", pulse, SLOT_DIM);
}

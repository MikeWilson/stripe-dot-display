#include "demo.h"
#include "config.h"
#include <Arduino.h>

// --- compile-time clock -----------------------------------------------------
// Seeds the clock card from __DATE__/__TIME__ so a freshly flashed demo shows
// roughly the right time instead of an obviously stale one.
static int monthFromName(const char *m) {
  static const char *N = "JanFebMarAprMayJunJulAugSepOctNovDec";
  for (int i = 0; i < 12; i++)
    if (!strncmp(m, N + i * 3, 3)) return i;
  return 0;
}

// days since 1970-01-01 (Howard Hinnant's days_from_civil)
static long daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (long)era * 146097 + (long)doe - 719468;
}

static uint32_t buildEpoch() {
  char mon[4] = {__DATE__[0], __DATE__[1], __DATE__[2], 0};
  int day  = atoi(__DATE__ + 4);
  int year = atoi(__DATE__ + 7);
  int hh   = atoi(__TIME__ + 0);
  int mm   = atoi(__TIME__ + 3);
  int ss   = atoi(__TIME__ + 6);
  long days = daysFromCivil(year, monthFromName(mon) + 1, day);
  // __TIME__ is local, so undo the display offset to get UTC.
  return (uint32_t)(days * 86400L + hh * 3600 + mm * 60 + ss) - TZ_OFFSET_SECONDS;
}

// --- fake sales -------------------------------------------------------------
struct Fake { int32_t cents; const char *who; const char *what; bool refund; };

static const Fake FAKES[] = {
  {  4999, "mike@example.com",      "VISA 4242",   false},
  {  1200, "dana@acme.io",          "PRO MONTHLY", false},
  {128495, "ops@northwind.co",      "ANNUAL PLAN", false},
  {   900, "sam@example.org",       "MASTERCARD",  false},
  { 24900, "finance@globex.com",    "TEAM SEAT",   false},
  {  3500, "refund@example.com",    "",            true },
  {    99, "tiny@example.com",      "AMEX 1005",   false},
  {750000, "enterprise@initech.com","ENTERPRISE",  false},
};
static const int NFAKES = sizeof(FAKES) / sizeof(FAKES[0]);

static int      idx_  = 0;
static uint32_t next_ = 0;

void demoBegin(Stats &s) {
  s = Stats();
  s.valid          = true;
  s.todayCents     = 128450;
  s.todayCount     = 7;
  s.availableCents = 4820133;
  s.pendingCents   = 219900;
  s.payoutCents    = 312845;
  strlcpy(s.payoutWhen, "OCT 24", sizeof(s.payoutWhen));
  static const int32_t d[14] = {42000, 118000, 96500, 150000, 61000, 0, 33000,
                                187000, 92000, 143500, 76000, 205000, 88000, 128450};
  memcpy(s.days, d, sizeof(d));
  s.nDays      = 14;
  s.mrrCents   = 1849900;
  s.activeSubs = 143;
  s.epoch      = buildEpoch();
  s.epochAtMs  = millis();

  next_ = millis() + DEMO_PAYMENT_EVERY_MS;
}

void demoTick(Stats &) {}

bool demoPayment(PaymentEvent &out) {
  if ((int32_t)(millis() - next_) < 0) return false;
  next_ = millis() + DEMO_PAYMENT_EVERY_MS;

  const Fake &f = FAKES[idx_];
  idx_ = (idx_ + 1) % NFAKES;

  out = PaymentEvent();
  out.cents  = f.cents;
  out.refund = f.refund;
  strlcpy(out.who,  f.who,  sizeof(out.who));
  strlcpy(out.what, f.what, sizeof(out.what));
  return true;
}

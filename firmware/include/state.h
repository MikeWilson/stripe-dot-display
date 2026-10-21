#pragma once
#include <Arduino.h>

// Everything the idle cards render from. Filled by net.cpp from /v1/snapshot.
struct Stats {
  bool     valid        = false;
  int64_t  todayCents   = 0;
  int32_t  todayCount   = 0;
  int64_t  availableCents = 0;
  int64_t  pendingCents   = 0;
  int64_t  payoutCents    = 0;
  char     payoutWhen[12] = "";     // "OCT 24"
  int32_t  days[14]       = {0};    // oldest -> newest, in cents
  int32_t  nDays          = 0;
  int64_t  mrrCents       = 0;
  int32_t  activeSubs     = 0;
  uint32_t epoch          = 0;      // unix seconds, captured at epochAtMs
  uint32_t epochAtMs      = 0;
};

struct PaymentEvent {
  int64_t cents = 0;
  char    who[48]  = "";    // payer email / name
  char    what[48] = "";    // description or "VISA 4242"
  bool    refund   = false;
};

uint32_t nowEpoch(const Stats &s);

// "$99.99" / "$12,840" / "$123.4k" / "$12.3M" -- sized to fit the big font.
void fmtBigMoney(int64_t cents, char *out, size_t n);
// "$12,840.50" in full, for micro-font sub-lines.
void fmtFullMoney(int64_t cents, char *out, size_t n);

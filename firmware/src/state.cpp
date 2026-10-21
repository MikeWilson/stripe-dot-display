#include "state.h"
#include "config.h"
#include <stdio.h>
#include <string.h>

uint32_t nowEpoch(const Stats &s) {
  if (!s.epoch) return 0;
  return s.epoch + (millis() - s.epochAtMs) / 1000;
}

// Write `v` with thousands separators into out, returns chars written.
static int groupDigits(int64_t v, char *out, size_t n) {
  char raw[24];
  int len = snprintf(raw, sizeof(raw), "%lld", (long long)v);
  int commas = (len - 1) / 3;
  int total = len + commas;
  if ((size_t)total + 1 > n) { snprintf(out, n, "%lld", (long long)v); return strlen(out); }
  out[total] = '\0';
  for (int i = len - 1, o = total - 1, c = 0; i >= 0; i--, o--) {
    out[o] = raw[i];
    if (++c % 3 == 0 && i > 0) out[--o] = ',';
  }
  return total;
}

void fmtBigMoney(int64_t cents, char *out, size_t n) {
  bool neg = cents < 0;
  if (neg) cents = -cents;
  char body[24];

  if (cents < 10000LL) {                       // under $100 -> keep the cents
    snprintf(body, sizeof(body), "%lld.%02lld",
             (long long)(cents / 100), (long long)(cents % 100));
  } else if (cents < 10000000LL) {             // under $100k -> whole dollars
    groupDigits(cents / 100, body, sizeof(body));
  } else if (cents < 1000000000LL) {           // under $10M -> 123.4k
    int64_t k = cents / 100;                   // dollars
    snprintf(body, sizeof(body), "%lld.%lldk",
             (long long)(k / 1000), (long long)((k % 1000) / 100));
  } else {                                     // 12.3M
    int64_t k = cents / 100;
    snprintf(body, sizeof(body), "%lld.%lldM",
             (long long)(k / 1000000), (long long)((k % 1000000) / 100000));
  }
  snprintf(out, n, "%s%s%s", neg ? "-" : "", CURRENCY_SYMBOL, body);
}

void fmtFullMoney(int64_t cents, char *out, size_t n) {
  bool neg = cents < 0;
  if (neg) cents = -cents;
  char g[24];
  groupDigits(cents / 100, g, sizeof(g));
  snprintf(out, n, "%s%s%s.%02lld", neg ? "-" : "", CURRENCY_SYMBOL, g,
           (long long)(cents % 100));
}

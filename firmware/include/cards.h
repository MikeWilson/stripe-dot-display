#pragma once
#include "dotgrid.h"
#include "state.h"

// The set of screens. Reference these from IDLE_CARDS in config.h.
enum CardId {
  CARD_TODAY,       // today's gross volume + payment count
  CARD_SPARKLINE,   // last 14 days as dot bars
  CARD_BALANCE,     // available balance + pending
  CARD_PAYOUT,      // next payout amount + date
  CARD_CLOCK,       // time + date
  CARD_MRR,         // MRR + active subscribers
  CARD__N
};

void drawCard   (CellBuf &c, CardId id, const Stats &s, int page, int pages);
void drawPayment(CellBuf &c, const PaymentEvent &e, const Stats &s);
void drawBurst  (CellBuf &c, float t);              // t: 0..1
void drawBoot   (CellBuf &c, const char *line, float t);
void drawOffline(CellBuf &c, float t);

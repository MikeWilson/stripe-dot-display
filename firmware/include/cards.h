#pragma once
#include "dotgrid.h"
#include "textlayer.h"
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

// Each card fills two layers: `c` is the dot lattice (headline numerals,
// bars, frame) and `t` is crisp pixel text (labels and captions, which the
// lattice would only make harder to read).
void drawCard   (CellBuf &c, TextLayer &t, CardId id, const Stats &s, int page, int pages);
void drawPayment(CellBuf &c, TextLayer &t, const PaymentEvent &e, const Stats &s);
// Payment announcement: a ragged flood of colour sweeps down over the card
// and drains back up, uncovering it. tt runs 0..1 across ALERT_BURST_MS.
void drawSweep  (CellBuf &c, TextLayer &t, const PaymentEvent &e,
                 const Stats &s, float tt);
int  sweepClipPx(float tt);
void drawBoot   (CellBuf &c, TextLayer &t, const char *line, float tt);
void drawOffline(CellBuf &c, TextLayer &t, float tt);

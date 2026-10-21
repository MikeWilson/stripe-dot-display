#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "state.h"

// Holds one long-lived TLS Server-Sent-Events connection to the Cloudflare
// Worker. The Worker pushes `stats` on connect + on a timer, and `payment` the
// instant Stripe's webhook lands. The device never sees a Stripe key.
class Relay {
 public:
  void begin();
  void loop();

  bool  online()   const { return streaming_; }
  bool  wifiUp()   const { return WiFi.status() == WL_CONNECTED; }
  const Stats &stats() const { return stats_; }

  // Pops the oldest queued payment, if any.
  bool takePayment(PaymentEvent &out);

 private:
  void connectWifi();
  bool openStream();
  void closeStream(const char *why);
  bool readBodyByte(uint8_t &b);
  void feedLine(const String &line);
  void dispatch();

  WiFiClientSecure cli_;
  Stats  stats_;

  bool     streaming_   = false;
  uint32_t lastByteMs_  = 0;
  uint32_t nextTryMs_   = 0;
  uint32_t backoffMs_   = 1000;

  // chunked-transfer state
  enum ChState { CH_SIZE, CH_DATA, CH_TRAIL };
  ChState  ch_      = CH_SIZE;
  uint32_t chLeft_  = 0;
  String   chSize_;
  bool     chunked_ = true;

  // SSE accumulation
  String line_, evName_, evData_;

  // single-slot payment queue (a burst of sales still shows each one)
  static const int QN = 6;
  PaymentEvent q_[QN];
  int qHead_ = 0, qTail_ = 0;
};

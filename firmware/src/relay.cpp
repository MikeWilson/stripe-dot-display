#include "relay.h"
#include "config.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include "esp_crt_bundle.h"

// Full Mozilla root bundle baked into the Arduino core -- real certificate
// validation, no pinning to go stale. Core 3.x wants an explicit length.
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t rootca_crt_bundle_end[]   asm("_binary_x509_crt_bundle_end");

#define STREAM_IDLE_TIMEOUT_MS 90000   // relay heartbeats every 20s

void Relay::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  connectWifi();
#if defined(ESP_ARDUINO_VERSION) && \
    ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  cli_.setCACertBundle(rootca_crt_bundle_start,
                       (size_t)(rootca_crt_bundle_end - rootca_crt_bundle_start));
#else
  cli_.setCACertBundle(rootca_crt_bundle_start);
#endif
  cli_.setTimeout(8);
}

void Relay::connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void Relay::loop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (streaming_) closeStream("wifi down");
    static uint32_t retry = 0;
    if (millis() - retry > 5000) { retry = millis(); connectWifi(); }
    return;
  }

  if (!streaming_) {
    if ((int32_t)(millis() - nextTryMs_) < 0) return;
    if (openStream()) {
      backoffMs_ = 1000;
    } else {
      backoffMs_ = backoffMs_ < 30000 ? backoffMs_ * 2 : 30000;
      nextTryMs_ = millis() + backoffMs_;
    }
    return;
  }

  // Drain whatever arrived this tick.
  uint8_t b;
  int guard = 4096;
  while (guard-- > 0 && readBodyByte(b)) {
    lastByteMs_ = millis();
    if (b == '\n') { feedLine(line_); line_ = ""; }
    else if (b != '\r') { if (line_.length() < 2048) line_ += (char)b; }
  }

  if (!cli_.connected())                              closeStream("peer closed");
  else if (millis() - lastByteMs_ > STREAM_IDLE_TIMEOUT_MS) closeStream("idle");
}

bool Relay::openStream() {
  cli_.stop();
  if (!cli_.connect(RELAY_HOST, RELAY_PORT)) return false;

  cli_.printf("GET /v1/stream HTTP/1.1\r\n"
              "Host: %s\r\n"
              "Authorization: Bearer %s\r\n"
              "Accept: text/event-stream\r\n"
              "Cache-Control: no-cache\r\n"
              "User-Agent: stripe-dot-display/1\r\n"
              "Connection: keep-alive\r\n\r\n",
              RELAY_HOST, DEVICE_TOKEN);

  // Status line + headers.
  uint32_t t0 = millis();
  bool ok = false, sawBlank = false;
  chunked_ = false;
  while (millis() - t0 < 8000 && !sawBlank) {
    if (!cli_.available()) { delay(5); continue; }
    String h = cli_.readStringUntil('\n');
    h.trim();
    if (h.length() == 0) { sawBlank = true; break; }
    if (h.startsWith("HTTP/")) ok = h.indexOf(" 200") > 0;
    String lower = h; lower.toLowerCase();
    if (lower.startsWith("transfer-encoding:") && lower.indexOf("chunked") > 0)
      chunked_ = true;
  }

  if (!ok || !sawBlank) { cli_.stop(); return false; }

  ch_ = CH_SIZE; chLeft_ = 0; chSize_ = "";
  line_ = ""; evName_ = ""; evData_ = "";
  streaming_ = true;
  lastByteMs_ = millis();
  return true;
}

void Relay::closeStream(const char *why) {
  log_w("stream closed: %s", why);
  cli_.stop();
  streaming_ = false;
  nextTryMs_ = millis() + backoffMs_;
}

// Transparently strips HTTP chunked framing. Returns false when nothing is
// available right now (not an error).
bool Relay::readBodyByte(uint8_t &out) {
  if (!chunked_) {
    if (!cli_.available()) return false;
    int v = cli_.read();
    if (v < 0) return false;
    out = (uint8_t)v;
    return true;
  }

  for (;;) {
    switch (ch_) {
      case CH_SIZE: {
        while (cli_.available()) {
          int v = cli_.read();
          if (v < 0) return false;
          if (v == '\n') {
            chLeft_ = strtoul(chSize_.c_str(), nullptr, 16);
            chSize_ = "";
            if (chLeft_ == 0) { closeStream("final chunk"); return false; }
            ch_ = CH_DATA;
            break;
          }
          if (v != '\r' && chSize_.length() < 16) chSize_ += (char)v;
        }
        if (ch_ != CH_DATA) return false;
        break;
      }
      case CH_DATA: {
        if (chLeft_ == 0) { ch_ = CH_TRAIL; break; }
        if (!cli_.available()) return false;
        int v = cli_.read();
        if (v < 0) return false;
        chLeft_--;
        out = (uint8_t)v;
        return true;
      }
      case CH_TRAIL: {
        // consume the \r\n that terminates the chunk
        while (cli_.available()) {
          int v = cli_.read();
          if (v == '\n') { ch_ = CH_SIZE; break; }
        }
        if (ch_ != CH_SIZE) return false;
        break;
      }
    }
  }
}

void Relay::feedLine(const String &l) {
  if (l.length() == 0) { dispatch(); return; }
  if (l.startsWith(":")) return;                       // heartbeat comment
  if (l.startsWith("event:")) { evName_ = l.substring(6); evName_.trim(); return; }
  if (l.startsWith("data:")) {
    String d = l.substring(5);
    if (d.startsWith(" ")) d.remove(0, 1);
    evData_ += d;
    return;
  }
}

void Relay::dispatch() {
  if (evData_.length() == 0) { evName_ = ""; return; }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, evData_);
  evData_ = "";
  String name = evName_;
  evName_ = "";
  if (err) { log_w("bad json: %s", err.c_str()); return; }

  if (name == "stats") {
    stats_.todayCents     = doc["today"]["cents"]  | 0LL;
    stats_.todayCount     = doc["today"]["count"]  | 0;
    stats_.availableCents = doc["available"]       | 0LL;
    stats_.pendingCents   = doc["pending"]         | 0LL;
    stats_.payoutCents    = doc["payout"]["cents"] | 0LL;
    strlcpy(stats_.payoutWhen, doc["payout"]["when"] | "", sizeof(stats_.payoutWhen));
    stats_.mrrCents       = doc["mrr"]             | 0LL;
    stats_.activeSubs     = doc["subs"]            | 0;

    JsonArray days = doc["days"].as<JsonArray>();
    stats_.nDays = 0;
    for (JsonVariant v : days) {
      if (stats_.nDays >= 14) break;
      stats_.days[stats_.nDays++] = v.as<int32_t>();
    }

    uint32_t e = doc["epoch"] | 0UL;
    if (e) { stats_.epoch = e; stats_.epochAtMs = millis(); }
    stats_.valid = true;

  } else if (name == "payment") {
    int64_t cents = doc["cents"] | 0LL;
    if (llabs(cents) < ALERT_MIN_CENTS) return;
    int nxt = (qTail_ + 1) % QN;
    if (nxt == qHead_) return;                         // queue full, drop
    PaymentEvent &e = q_[qTail_];
    e = PaymentEvent();
    e.cents  = cents;
    e.refund = doc["refund"] | false;
    strlcpy(e.who,  doc["who"]  | "", sizeof(e.who));
    strlcpy(e.what, doc["what"] | "", sizeof(e.what));
    qTail_ = nxt;
  }
}

bool Relay::takePayment(PaymentEvent &out) {
  if (qHead_ == qTail_) return false;
  out = q_[qHead_];
  qHead_ = (qHead_ + 1) % QN;
  return true;
}

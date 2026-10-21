// Focused I2C probe: two candidate buses, no filtering, plus a chip-ID read
// so we know the devices are real and not bus artefacts.
#include <Arduino.h>
#include <Wire.h>

static void probe(const char *label, int sda, int scl) {
  Serial.printf("\n--- %s: SDA=%d SCL=%d ---\n", label, sda, scl);
  Wire.end();
  if (!Wire.begin(sda, scl, 100000)) { Serial.println("  begin() failed"); return; }
  Wire.setTimeOut(50);
  int n = 0;
  for (uint8_t a = 0x08; a <= 0x77; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.printf("  0x%02X\n", a); n++; }
  }
  Serial.printf("  %d device(s)\n", n);
}

// Read one register from a device, so we can tell a real chip from a phantom ACK.
static int rd(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom((int)addr, 1) != 1) return -1;
  return Wire.read();
}

void setup() {
  Serial.begin(115200);
  delay(2500);
  Serial.println("\n=== focused I2C probe ===");

  probe("documented 1.43C bus", 47, 48);
  probe("bus found by sweep",   15, 14);

  // Re-select whichever bus answered and identify the chips properly.
  Wire.end(); Wire.begin(15, 14, 100000); Wire.setTimeOut(50);
  int axp = rd(0x34, 0x03);          // AXP2101 chip ID, expect 0x4A
  int imu = rd(0x6B, 0x00);          // QMI8658 WHO_AM_I, expect 0x05
  int tca = rd(0x20, 0x01);          // TCA9554 output port
  Serial.printf("\non 15/14: AXP2101 id=0x%02X (expect 4A)  QMI8658 who=0x%02X (expect 05)"
                "  TCA9554 out=0x%02X\n", axp, imu, tca);

  Wire.end(); Wire.begin(47, 48, 100000); Wire.setTimeOut(50);
  Serial.printf("on 47/48: AXP2101 id=0x%02X  QMI8658 who=0x%02X\n",
                rd(0x34, 0x03), rd(0x6B, 0x00));
  Serial.println("\nPROBE DONE");
}
void loop() { delay(1000); }

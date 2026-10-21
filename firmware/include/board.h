#pragma once
// ---------------------------------------------------------------------------
// Waveshare ESP32-S3-Touch-AMOLED-1.8   368 x 448, CO5300 over QSPI, N16R8
//
// Identified from the unit, not from a product page. An I2C sweep found the
// bus on SDA=15/SCL=14 carrying 0x15 touch, 0x18 ES8311, 0x20 TCA9554,
// 0x34 AXP2101 (chip id read back 0x4A) and 0x6B QMI8658 (WHO_AM_I 0x05).
// Values below are from waveshareteam/ESP32-S3-Touch-AMOLED-1.8,
// examples/arduino-v2/libraries/Mylibrary/pin_config.h and its GFX example.
//
// Two traps worth remembering before "correcting" anything here:
//   * QSPI is write-only. Wrong pins still let begin() return true and just
//     leave the screen black -- you cannot detect a bad guess in software.
//   * Wrong WIDTH is not a crop. Every row lands at the wrong byte offset and
//     the whole image shears. If the picture is torn, suspect LCD_W first.
// ---------------------------------------------------------------------------

// Display: CO5300, QSPI. No reset line is wired on this board.
#define PIN_LCD_CS     12
#define PIN_LCD_CLK    11
#define PIN_LCD_D0     4
#define PIN_LCD_D1     5
#define PIN_LCD_D2     6
#define PIN_LCD_D3     7
#define PIN_LCD_RST    GFX_NOT_DEFINED

#define LCD_W          368
#define LCD_H          448
#define LCD_COL_OFF1   16     // vendor example uses a 16px column offset
#define LCD_ROW_OFF1   0
#define LCD_COL_OFF2   0
#define LCD_ROW_OFF2   0
#define LCD_QSPI_HZ    80000000

// Shared I2C: touch + ES8311 codec + TCA9554 + AXP2101 + RTC + IMU.
// Panel power is on the AXP2101 rail, not a GPIO enable.
#define PIN_I2C_SDA    15
#define PIN_I2C_SCL    14
#define PIN_TOUCH_INT  21
#define ADDR_TOUCH     0x15
#define ADDR_TCA9554   0x20
#define ADDR_AXP2101   0x34
#define ADDR_RTC       0x51
#define ADDR_IMU       0x6B

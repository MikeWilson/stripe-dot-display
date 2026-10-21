#pragma once
// ---------------------------------------------------------------------------
// Waveshare ESP32-S3-Touch-AMOLED-1.43 board map.
//
// NOTE: Waveshare shipped this board with two different panel controllers and
// (depending on revision) two different QSPI pin maps. If the screen stays
// black, flip PANEL_VARIANT first, then PINMAP_VARIANT. Ground truth is the
// Waveshare demo zip for YOUR board revision -- the pin table is at the top of
// its display driver file.
// ---------------------------------------------------------------------------

#define PANEL_CO5300   1
#define PANEL_SH8601   2
#define PANEL_VARIANT  PANEL_CO5300   // <-- try the other one if init fails

#define PINMAP_A       1   // cs9  clk10 d11..14 rst21   (most common)
#define PINMAP_B       2   // cs10 clk11 d12..15 rst38 en43
#define PINMAP_VARIANT PINMAP_A

#if PINMAP_VARIANT == PINMAP_A
  #define PIN_LCD_CS   9
  #define PIN_LCD_CLK  10
  #define PIN_LCD_D0   11
  #define PIN_LCD_D1   12
  #define PIN_LCD_D2   13
  #define PIN_LCD_D3   14
  #define PIN_LCD_RST  21
  #define PIN_LCD_EN   -1
#else
  #define PIN_LCD_CS   10
  #define PIN_LCD_CLK  11
  #define PIN_LCD_D0   12
  #define PIN_LCD_D1   13
  #define PIN_LCD_D2   14
  #define PIN_LCD_D3   15
  #define PIN_LCD_RST  38
  #define PIN_LCD_EN   43
#endif

#define LCD_W          466
#define LCD_H          466
#define LCD_COL_OFFSET 6     // CO5300/SH8601 on this panel start at column 6
#define LCD_QSPI_HZ    80000000

// Shared I2C bus: FT3168 touch + QMI8658 IMU + PCF85063 RTC
#define PIN_I2C_SDA    47
#define PIN_I2C_SCL    48
#define PIN_TOUCH_INT  -1
#define ADDR_TOUCH     0x38

#define PIN_BAT_ADC    5

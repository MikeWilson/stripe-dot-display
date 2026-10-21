#pragma once
#include <Arduino.h>

// --- 5x7 column-major ASCII font (0x20..0x7E) -------------------------------
// Each glyph is 5 bytes; bit N of a byte is row N (top = bit 0).
extern const uint8_t FONT5X7[95][5];

// --- 7x11 row-major display font --------------------------------------------
// Big numerals for the headline amount. Each glyph is 11 rows; within a row
// bit 6 is the leftmost column. BIGW[] gives the inked width of each glyph so
// '.' and ',' don't reserve a full 7 cells.
#define BIGH 11
extern const uint8_t  FONTBIG[][BIGH];
extern const uint8_t  BIGW[];

// Index into FONTBIG for a character, or -1 if unsupported.
int bigIndex(char c);

// --- 3x5 micro font, ASCII 0x20..0x5A (space..Z) ----------------------------
// Label tier. Column-major, 3 bytes per glyph, bit N = row N (top = bit 0).
// Lowercase input is folded to uppercase by the renderer.
extern const uint8_t FONT3X5[59][3];

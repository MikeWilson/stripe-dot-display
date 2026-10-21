// Host shim: just enough Arduino for the rendering code to compile off-device.
#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

#define MALLOC_CAP_SPIRAM 0

unsigned long millis();
uint32_t      esp_random();
void         *heap_caps_malloc(size_t n, int caps);

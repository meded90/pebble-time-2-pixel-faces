#pragma once
#include <pebble.h>

// Starts once per application launch on Emery; Round 2 remains static.
void intro_start(BitmapLayer *layer, GBitmap **still, Layer *foreground);
void intro_stop(void);

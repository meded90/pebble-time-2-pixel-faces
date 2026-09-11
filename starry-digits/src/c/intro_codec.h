#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Caller owns one framebuffer and bounded scratch storage. Failure may partially
// update the framebuffer; the player must restore the static background.
bool intro_decode(const uint8_t *input, size_t size, uint8_t *scratch,
                  size_t capacity, uint8_t *pixels, uint16_t stride);

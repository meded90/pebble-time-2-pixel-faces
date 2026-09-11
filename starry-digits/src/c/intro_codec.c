#include "intro_codec.h"
#include <string.h>

bool intro_decode(const uint8_t *input, size_t size, uint8_t *scratch,
                  size_t capacity, uint8_t *pixels, uint16_t stride) {
  if (!input || !scratch || !pixels || stride < 200) return false;
  size_t in = 0, out = 0;
  while (in < size) {
    uint8_t flags = input[in++];
    for (unsigned bit = 0; bit < 8 && in < size; bit++) {
      if (!(flags & (1u << bit))) {
        if (out >= capacity) return false;
        scratch[out++] = input[in++];
        continue;
      }
      if (size - in < 2) return false;
      uint16_t token = input[in] | ((uint16_t)input[in + 1] << 8);
      in += 2;
      size_t distance = (token >> 4) + 1;
      size_t count = (token & 15) + 3;
      if ((token & 15) == 15) {
        if (in >= size) return false;
        count += input[in++];
      }
      if (distance > out || count > capacity - out) return false;
      while (count--) { scratch[out] = scratch[out - distance]; out++; }
    }
  }
  size_t cursor = 0, pixel = 0;
  while (cursor < out) {
    uint8_t packet = scratch[cursor++];
    size_t count = (packet & 127) + 1;
    if (count > 200 * 228 - pixel) return false;
    if (packet & 128) { pixel += count; continue; }
    if (count > out - cursor) return false;
    while (count) {
      size_t x = pixel % 200, length = count < 200 - x ? count : 200 - x;
      memcpy(pixels + (pixel / 200) * stride + x, scratch + cursor, length);
      pixel += length; cursor += length; count -= length;
    }
  }
  return pixel == 200 * 228;
}

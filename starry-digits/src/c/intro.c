#include "intro.h"
#if defined(PBL_PLATFORM_EMERY)
#include "intro_config.h"
#include <stdlib.h>
#include <string.h>
#include "intro_codec.h"

static AppTimer *s_timer;
static GBitmap *s_frame;
static GBitmap **s_still;
static BitmapLayer *s_layer;
static Layer *s_foreground;
static uint8_t *s_chunk, *s_delta;
static bool s_started;
static uint16_t s_rendered;
static uint32_t s_decode_total, s_decode_max;


void intro_stop(void) {
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
  if (s_frame) gbitmap_destroy(s_frame);
  free(s_chunk);
  free(s_delta);
  s_chunk = s_delta = NULL;
  s_frame = NULL;
  if (s_layer && s_still) {
    if (!*s_still) *s_still = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
    bitmap_layer_set_bitmap(s_layer, *s_still);
    if (s_foreground) layer_mark_dirty(s_foreground);
  }
  s_foreground = NULL;
  s_layer = NULL;
  s_still = NULL;
}

static uint32_t read_u32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool load_frame(uint16_t index) {
  ResHandle resource = resource_get_handle(RESOURCE_ID_INTRO_DATA);
  uint8_t offsets[8];
  if (resource_load_byte_range(resource, index * 4, offsets, 8) != 8) return false;
  uint32_t start = read_u32(offsets), end = read_u32(offsets + 4);
  if (start < (INTRO_FRAMES + 1) * 4 || end <= start ||
      end - start > INTRO_MAX_CHUNK || end > resource_size(resource)) return false;
  size_t size = end - start;
  if (resource_load_byte_range(resource, start, s_chunk, size) != size) return false;
  if (!intro_decode(s_chunk, size, s_delta, INTRO_MAX_DELTA,
                    gbitmap_get_data(s_frame), gbitmap_get_bytes_per_row(s_frame))) return false;
  bitmap_layer_set_bitmap(s_layer, s_frame);
  if (s_foreground) layer_mark_dirty(s_foreground);
  return true;
}

static void next_frame(void *context) {
  (void)context;
  s_timer = NULL;
  if (s_rendered >= INTRO_FRAMES) {
    intro_stop();
    APP_LOG(APP_LOG_LEVEL_INFO, "Intro completed %u/%u frames; heap %u; timers stopped",
            s_rendered, INTRO_FRAMES, (unsigned)heap_bytes_free());
    APP_LOG(APP_LOG_LEVEL_INFO, "Intro decode total=%lu max=%lu ms",
            (unsigned long)s_decode_total, (unsigned long)s_decode_max);
    return;
  }
  time_t before_s, after_s;
  uint16_t before_ms, after_ms;
  time_ms(&before_s, &before_ms);
  if (!load_frame(s_rendered)) {
    intro_stop();
    APP_LOG(APP_LOG_LEVEL_WARNING, "Intro frame failed; static background restored");
    return;
  }
  time_ms(&after_s, &after_ms);
  int32_t decode_ms = (after_s - before_s) * 1000 + after_ms - before_ms;
  // Wall time is diagnostic only: syncing the watch clock must never jump,
  // skip, shorten, or abort the animation.
  if (decode_ms >= 0 && decode_ms < 1000) {
    s_decode_total += decode_ms;
    if ((uint32_t)decode_ms > s_decode_max) s_decode_max = decode_ms;
  }
  s_rendered++;
  s_timer = app_timer_register(INTRO_FRAME_MS, next_frame, NULL);
  if (!s_timer) intro_stop();
}

void intro_start(BitmapLayer *layer, GBitmap **still, Layer *foreground) {
  if (s_started || !layer || !still || !*still) return;
  s_started = true;
  s_layer = layer;
  s_foreground = foreground;
  s_still = still;
  bitmap_layer_set_bitmap(layer, NULL);
  gbitmap_destroy(*still);
  *still = NULL;
  s_frame = gbitmap_create_blank(GSize(200, 228), GBitmapFormat8Bit);
  s_chunk = malloc(INTRO_MAX_CHUNK);
  s_delta = malloc(INTRO_MAX_DELTA);
  if (!s_frame || !s_chunk || !s_delta) { intro_stop(); return; }
  memset(gbitmap_get_data(s_frame), GColorBlack.argb,
         gbitmap_get_bytes_per_row(s_frame) * 228);
  s_rendered = 0;
  s_decode_total = s_decode_max = 0;
  APP_LOG(APP_LOG_LEVEL_INFO, "Intro started once; %u frames", INTRO_FRAMES);
  next_frame(NULL);
}
#else
void intro_start(BitmapLayer *layer, GBitmap **still, Layer *foreground) { (void)layer; (void)still; (void)foreground; }
void intro_stop(void) {}
#endif

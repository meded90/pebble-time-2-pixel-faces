#include <pebble.h>

static Window *s_window;
static BitmapLayer *s_background_layer;
static GBitmap *s_background_bitmap;
static Layer *s_time_layer;
static GBitmap *s_time_digit_bitmaps[4];
static struct tm s_now;

static const uint32_t DIGIT_RESOURCE_IDS[10] = {
  RESOURCE_ID_IMAGE_DIGIT_0,
  RESOURCE_ID_IMAGE_DIGIT_1,
  RESOURCE_ID_IMAGE_DIGIT_2,
  RESOURCE_ID_IMAGE_DIGIT_3,
  RESOURCE_ID_IMAGE_DIGIT_4,
  RESOURCE_ID_IMAGE_DIGIT_5,
  RESOURCE_ID_IMAGE_DIGIT_6,
  RESOURCE_ID_IMAGE_DIGIT_7,
  RESOURCE_ID_IMAGE_DIGIT_8,
  RESOURCE_ID_IMAGE_DIGIT_9,
};

enum {
  TIME_LEFT = 8,
  TIME_TOP = 16,
  DIGIT_WIDTH = 48,
  DIGIT_HEIGHT = 58,
  TIME_ROW_GAP = 0,
};

static void get_time_digits(int digits[4]) {
  int hour = s_now.tm_hour;
  if (!clock_is_24h_style()) {
    hour %= 12;
    if (hour == 0) {
      hour = 12;
    }
  }

  digits[0] = hour / 10;
  digits[1] = hour % 10;
  digits[2] = s_now.tm_min / 10;
  digits[3] = s_now.tm_min % 10;
}

static void reload_time_digit_bitmaps(void) {
  int digits[4];
  get_time_digits(digits);

  for (int index = 0; index < 4; index++) {
    if (s_time_digit_bitmaps[index]) {
      gbitmap_destroy(s_time_digit_bitmaps[index]);
    }
    s_time_digit_bitmaps[index] =
        gbitmap_create_with_resource(DIGIT_RESOURCE_IDS[digits[index]]);
  }
}

static void time_layer_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  // Round (gabbro) anchors top-left like the emery/PT2 layout, but far enough
  // in that the circle doesn't clip the grid's top-left corner (~40px inset).
  const int origin_left = PBL_IF_ROUND_ELSE(40, TIME_LEFT);
  const int origin_top = PBL_IF_ROUND_ELSE(40, TIME_TOP);
  for (int index = 0; index < 4; index++) {
    const int row = index / 2;
    const int column = index % 2;
    GBitmap *digit_bitmap = s_time_digit_bitmaps[index];
    if (!digit_bitmap) {
      continue;
    }

    const GRect destination = GRect(
      origin_left + column * DIGIT_WIDTH,
      origin_top + row * (DIGIT_HEIGHT + TIME_ROW_GAP),
      DIGIT_WIDTH,
      DIGIT_HEIGHT
    );
    graphics_draw_bitmap_in_rect(ctx, digit_bitmap, destination);
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_now = *tick_time;
  reload_time_digit_bitmaps();
  if (s_time_layer) {
    layer_mark_dirty(s_time_layer);
  }
}

static void window_load(Window *window) {
  Layer *root_layer = window_get_root_layer(window);
  window_set_background_color(window, GColorOxfordBlue);

  s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
  if (s_background_bitmap) {
    s_background_layer = bitmap_layer_create(layer_get_bounds(root_layer));
    if (s_background_layer) {
      bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
      bitmap_layer_set_compositing_mode(s_background_layer, GCompOpSet);
      layer_add_child(root_layer, bitmap_layer_get_layer(s_background_layer));
    }
  }

  reload_time_digit_bitmaps();

  s_time_layer = layer_create(layer_get_bounds(root_layer));
  if (s_time_layer) {
    layer_set_update_proc(s_time_layer, time_layer_update_proc);
    layer_add_child(root_layer, s_time_layer);
  }
}

static void window_unload(Window *window) {
  if (s_time_layer) {
    layer_destroy(s_time_layer);
  }
  for (int index = 0; index < 4; index++) {
    if (s_time_digit_bitmaps[index]) {
      gbitmap_destroy(s_time_digit_bitmaps[index]);
    }
  }
  if (s_background_layer) {
    bitmap_layer_destroy(s_background_layer);
  }
  if (s_background_bitmap) {
    gbitmap_destroy(s_background_bitmap);
  }
}

static void init(void) {
  time_t now = time(NULL);
  s_now = *localtime(&now);

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

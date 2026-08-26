#include <pebble.h>

static Window *s_window;
static BitmapLayer *s_background_layer;
static GBitmap *s_background_bitmap;
static BitmapLayer *s_digit_layers[4];
static GBitmap *s_digit_bitmaps[4];
static int8_t s_digit_values[4] = {-1, -1, -1, -1};

static const uint32_t s_digit_resource_ids[10] = {
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

static void set_digit(uint8_t position, uint8_t value) {
  if (position >= 4 || value > 9 || !s_digit_layers[position]
      || s_digit_values[position] == value) {
    return;
  }

  GBitmap *next_bitmap = gbitmap_create_with_resource(s_digit_resource_ids[value]);
  if (!next_bitmap) {
    return;
  }

  GBitmap *previous_bitmap = s_digit_bitmaps[position];
  s_digit_bitmaps[position] = next_bitmap;
  s_digit_values[position] = value;
  bitmap_layer_set_bitmap(s_digit_layers[position], next_bitmap);
  if (previous_bitmap) {
    gbitmap_destroy(previous_bitmap);
  }
}

static void update_time(struct tm *tick_time) {
  int hour = tick_time->tm_hour;
  if (!clock_is_24h_style()) {
    hour %= 12;
    if (hour == 0) {
      hour = 12;
    }
  }

  set_digit(0, hour / 10);
  set_digit(1, hour % 10);
  set_digit(2, tick_time->tm_min / 10);
  set_digit(3, tick_time->tm_min % 10);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time(tick_time);
}

static void window_load(Window *window) {
  Layer *root_layer = window_get_root_layer(window);
  window_set_background_color(window, GColorWhite);

  s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
  if (!s_background_bitmap) {
    return;
  }

  s_background_layer = bitmap_layer_create(layer_get_bounds(root_layer));
  if (!s_background_layer) {
    return;
  }

  bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
  bitmap_layer_set_compositing_mode(s_background_layer, GCompOpSet);
  layer_add_child(root_layer, bitmap_layer_get_layer(s_background_layer));

  const GRect digit_frames[4] = {
    GRect(1, 2, 50, 64),
    GRect(39, 2, 50, 64),
    GRect(1, 61, 50, 64),
    GRect(39, 61, 50, 64),
  };
  for (uint8_t index = 0; index < 4; index++) {
    s_digit_layers[index] = bitmap_layer_create(digit_frames[index]);
    if (!s_digit_layers[index]) {
      return;
    }
    bitmap_layer_set_compositing_mode(s_digit_layers[index], GCompOpSet);
    layer_add_child(root_layer, bitmap_layer_get_layer(s_digit_layers[index]));
  }

  time_t now = time(NULL);
  update_time(localtime(&now));
}

static void window_unload(Window *window) {
  for (uint8_t index = 0; index < 4; index++) {
    if (s_digit_layers[index]) {
      bitmap_layer_destroy(s_digit_layers[index]);
      s_digit_layers[index] = NULL;
    }
    if (s_digit_bitmaps[index]) {
      gbitmap_destroy(s_digit_bitmaps[index]);
      s_digit_bitmaps[index] = NULL;
    }
    s_digit_values[index] = -1;
  }
  if (s_background_layer) {
    bitmap_layer_destroy(s_background_layer);
  }
  if (s_background_bitmap) {
    gbitmap_destroy(s_background_bitmap);
  }
}

static void init(void) {
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

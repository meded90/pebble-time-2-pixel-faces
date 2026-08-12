#include <pebble.h>

static Window *s_window;
static BitmapLayer *s_background_layer;
static GBitmap *s_background_bitmap;
static TextLayer *s_hour_layer;
static TextLayer *s_minute_layer;
static char s_hour_text[3];
static char s_minute_text[3];

static void configure_time_layer(TextLayer *layer) {
  text_layer_set_background_color(layer, GColorClear);
  text_layer_set_text_color(layer, GColorBlack);
  text_layer_set_font(layer, fonts_get_system_font(FONT_KEY_LECO_38_BOLD_NUMBERS));
  text_layer_set_text_alignment(layer, GTextAlignmentCenter);
}

static void update_time(struct tm *tick_time) {
  int hour = tick_time->tm_hour;
  if (!clock_is_24h_style()) {
    hour %= 12;
    if (hour == 0) {
      hour = 12;
    }
  }

  snprintf(s_hour_text, sizeof(s_hour_text), "%02d", hour);
  snprintf(s_minute_text, sizeof(s_minute_text), "%02d", tick_time->tm_min);
  if (s_hour_layer && s_minute_layer) {
    text_layer_set_text(s_hour_layer, s_hour_text);
    text_layer_set_text(s_minute_layer, s_minute_text);
  }
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

  s_hour_layer = text_layer_create(GRect(5, 3, 64, 46));
  s_minute_layer = text_layer_create(GRect(5, 49, 64, 46));
  if (!s_hour_layer || !s_minute_layer) {
    return;
  }

  configure_time_layer(s_hour_layer);
  configure_time_layer(s_minute_layer);
  layer_add_child(root_layer, text_layer_get_layer(s_hour_layer));
  layer_add_child(root_layer, text_layer_get_layer(s_minute_layer));
}

static void window_unload(Window *window) {
  if (s_hour_layer) {
    text_layer_destroy(s_hour_layer);
  }
  if (s_minute_layer) {
    text_layer_destroy(s_minute_layer);
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

  time_t now = time(NULL);
  update_time(localtime(&now));
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

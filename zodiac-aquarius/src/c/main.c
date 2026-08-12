#include <pebble.h>

static Window *s_window;
static GBitmap *s_background_bitmap;
static Layer *s_canvas;
static struct tm s_now;

enum {
  TIME_LEFT = 10,
  TIME_TOP = 8,
  TIME_WIDTH = 58,
  TIME_ROW_HEIGHT = 34,
};

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  if (s_background_bitmap) {
    graphics_draw_bitmap_in_rect(ctx, s_background_bitmap,
                                 layer_get_bounds(layer));
  }

  int hour = s_now.tm_hour;
  if (!clock_is_24h_style()) {
    hour %= 12;
    if (hour == 0) {
      hour = 12;
    }
  }

  char hour_text[3];
  char minute_text[3];
  snprintf(hour_text, sizeof(hour_text), "%02d", hour);
  snprintf(minute_text, sizeof(minute_text), "%02d", s_now.tm_min);

  graphics_context_set_text_color(ctx, GColorBlue);
  GFont time_font = fonts_get_system_font(FONT_KEY_GOTHIC_28);
  graphics_draw_text(
    ctx,
    hour_text,
    time_font,
    GRect(TIME_LEFT, TIME_TOP, TIME_WIDTH, TIME_ROW_HEIGHT),
    GTextOverflowModeFill,
    GTextAlignmentLeft,
    NULL
  );
  graphics_draw_text(
    ctx,
    minute_text,
    time_font,
    GRect(TIME_LEFT, TIME_TOP + TIME_ROW_HEIGHT, TIME_WIDTH, TIME_ROW_HEIGHT),
    GTextOverflowModeFill,
    GTextAlignmentLeft,
    NULL
  );
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_now = *tick_time;
  layer_mark_dirty(s_canvas);
}

static void window_load(Window *window) {
  Layer *root_layer = window_get_root_layer(window);

  s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);

  s_canvas = layer_create(layer_get_bounds(root_layer));
  layer_set_update_proc(s_canvas, canvas_update_proc);
  layer_add_child(root_layer, s_canvas);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
  gbitmap_destroy(s_background_bitmap);
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

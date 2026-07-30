#include <pebble.h>

static Window *s_window;
static Layer *s_canvas;
static int s_battery_percent;
static struct tm s_now;

static const GColor COLOR_CREAM = GColorFromHEX(0xE8E0C8);
static const GColor COLOR_BLUE = GColorFromHEX(0x55779A);
static const GColor COLOR_RED = GColorFromHEX(0xC84F2B);
static const GColor COLOR_YELLOW = GColorFromHEX(0xD9A52A);

static void fill_rect(GContext *ctx, GRect rect, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_rect(ctx, rect, 0, GCornerNone);
}

static void stroke_rect(GContext *ctx, GRect rect) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_rect(ctx, rect);
}

static void draw_text(GContext *ctx, const char *text, GRect rect,
                      const char *font_key, GColor color,
                      GTextAlignment alignment) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, fonts_get_system_font(font_key), rect,
                     GTextOverflowModeTrailingEllipsis, alignment, NULL);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  const int gap = 4;
  const int left_w = 116;
  const int right_x = left_w + gap;
  const int right_w = bounds.size.w - right_x;
  const int top_h = 138;
  const int bottom_y = top_h + gap;
  const int bottom_h = bounds.size.h - bottom_y;

  fill_rect(ctx, bounds, GColorBlack);

  GRect time_rect = GRect(0, 0, left_w, top_h);
  GRect accent_rect = GRect(right_x, 0, right_w, 68);
  GRect date_rect = GRect(right_x, 72, right_w, 66);
  GRect battery_rect = GRect(0, bottom_y, left_w, bottom_h);
  GRect color_rect = GRect(right_x, bottom_y, right_w, bottom_h);

  fill_rect(ctx, time_rect, COLOR_CREAM);
  fill_rect(ctx, accent_rect, COLOR_RED);
  fill_rect(ctx, date_rect, COLOR_CREAM);
  fill_rect(ctx, battery_rect, COLOR_BLUE);
  fill_rect(ctx, color_rect, COLOR_YELLOW);

  stroke_rect(ctx, time_rect);
  stroke_rect(ctx, accent_rect);
  stroke_rect(ctx, date_rect);
  stroke_rect(ctx, battery_rect);
  stroke_rect(ctx, color_rect);

  char hour[3];
  char minute[3];
  strftime(hour, sizeof(hour), clock_is_24h_style() ? "%H" : "%I", &s_now);
  strftime(minute, sizeof(minute), "%M", &s_now);

  draw_text(ctx, hour, GRect(8, 9, left_w - 16, 62),
            FONT_KEY_LECO_42_NUMBERS, GColorBlack, GTextAlignmentCenter);
  draw_text(ctx, minute, GRect(8, 70, left_w - 16, 62),
            FONT_KEY_LECO_42_NUMBERS, GColorBlack, GTextAlignmentCenter);

  char weekday[8];
  char day[4];
  strftime(weekday, sizeof(weekday), "%a", &s_now);
  strftime(day, sizeof(day), "%d", &s_now);
  draw_text(ctx, weekday, GRect(right_x + 4, 80, right_w - 8, 28),
            FONT_KEY_GOTHIC_24_BOLD, GColorBlack, GTextAlignmentCenter);
  draw_text(ctx, day, GRect(right_x + 4, 106, right_w - 8, 28),
            FONT_KEY_GOTHIC_28_BOLD, GColorBlack, GTextAlignmentCenter);

  char battery[8];
  snprintf(battery, sizeof(battery), "%d%%", s_battery_percent);
  draw_text(ctx, battery, GRect(10, bottom_y + bottom_h - 38, left_w - 20, 30),
            FONT_KEY_GOTHIC_24_BOLD, COLOR_CREAM, GTextAlignmentLeft);
}

static void update_time(struct tm *tick_time) {
  s_now = *tick_time;
  layer_mark_dirty(s_canvas);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time(tick_time);
}

static void battery_handler(BatteryChargeState state) {
  s_battery_percent = state.charge_percent;
  layer_mark_dirty(s_canvas);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update_proc);
  layer_add_child(root, s_canvas);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
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
  battery_handler(battery_state_service_peek());

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}


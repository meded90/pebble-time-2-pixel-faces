#include <pebble.h>

static Window *s_window;
static Layer *s_canvas;
static int s_battery_percent;
static struct tm s_now;

static const GColor COLOR_PANEL = GColorFromHEX(0x555555);
static const GColor COLOR_PANEL_TOP = GColorFromHEX(0x444444);
static const GColor COLOR_INK = GColorFromHEX(0xE8E0C8);
static const GColor COLOR_MUTED = GColorFromHEX(0x8F8B7C);
static const GColor COLOR_GREEN = GColorFromHEX(0x70945E);

static void draw_text(GContext *ctx, const char *text, GRect rect,
                      const char *font_key, GColor color,
                      GTextAlignment alignment) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, fonts_get_system_font(font_key), rect,
                     GTextOverflowModeTrailingEllipsis, alignment, NULL);
}

static void draw_flip_digit(GContext *ctx, GRect rect, char digit) {
  graphics_context_set_fill_color(ctx, COLOR_PANEL);
  graphics_fill_rect(ctx, rect, 5, GCornersAll);

  graphics_context_set_fill_color(ctx, COLOR_PANEL_TOP);
  graphics_fill_rect(ctx, GRect(rect.origin.x + 2, rect.origin.y + 2,
                                rect.size.w - 4, rect.size.h / 2 - 2),
                     3, GCornersTop);

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_round_rect(ctx, rect, 5);

  const int hinge_y = rect.origin.y + rect.size.h / 2;
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_line(ctx, GPoint(rect.origin.x, hinge_y),
                     GPoint(rect.origin.x + rect.size.w - 1, hinge_y));

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(rect.origin.x - 1, hinge_y - 4, 5, 8),
                     1, GCornersAll);
  graphics_fill_rect(ctx, GRect(rect.origin.x + rect.size.w - 4,
                                hinge_y - 4, 5, 8),
                     1, GCornersAll);

  char value[2] = {digit, '\0'};
  draw_text(ctx, value, GRect(rect.origin.x, rect.origin.y + 17,
                              rect.size.w, 62),
            FONT_KEY_LECO_42_NUMBERS, COLOR_INK, GTextAlignmentCenter);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  char time_digits[5];
  strftime(time_digits, sizeof(time_digits),
           clock_is_24h_style() ? "%H%M" : "%I%M", &s_now);

  const int panel_w = 60;
  const int panel_h = 98;
  draw_flip_digit(ctx, GRect(6, 8, panel_w, panel_h), time_digits[0]);
  draw_flip_digit(ctx, GRect(69, 8, panel_w, panel_h), time_digits[1]);
  draw_flip_digit(ctx, GRect(6, 112, panel_w, panel_h), time_digits[2]);
  draw_flip_digit(ctx, GRect(69, 112, panel_w, panel_h), time_digits[3]);

  char weekday[8];
  char day[4];
  char month[8];
  strftime(weekday, sizeof(weekday), "%a", &s_now);
  strftime(day, sizeof(day), "%d", &s_now);
  strftime(month, sizeof(month), "%b", &s_now);

  draw_text(ctx, weekday, GRect(138, 36, 58, 30),
            FONT_KEY_GOTHIC_24_BOLD, COLOR_INK, GTextAlignmentCenter);
  draw_text(ctx, day, GRect(138, 70, 58, 34),
            FONT_KEY_GOTHIC_28_BOLD, COLOR_INK, GTextAlignmentCenter);
  draw_text(ctx, month, GRect(138, 107, 58, 28),
            FONT_KEY_GOTHIC_24_BOLD, COLOR_INK, GTextAlignmentCenter);

  graphics_context_set_stroke_color(ctx, COLOR_MUTED);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(145, 151), GPoint(190, 151));

  char battery[8];
  snprintf(battery, sizeof(battery), "%d%%", s_battery_percent);
  draw_text(ctx, battery, GRect(138, 174, 58, 30),
            FONT_KEY_GOTHIC_24_BOLD, COLOR_GREEN, GTextAlignmentCenter);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_now = *tick_time;
  layer_mark_dirty(s_canvas);
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
  s_now = *localtime(&now);
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

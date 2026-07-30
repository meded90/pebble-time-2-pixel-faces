#include <pebble.h>

static Window *s_window;
static Layer *s_canvas;
static int s_battery_percent;
static struct tm s_now;

// Sampled from Piet Mondrian's "Composition with Red, Blue and Yellow".
static const GColor COLOR_CREAM = GColorFromHEX(0xE7E7E8);
static const GColor COLOR_BLUE = GColorFromHEX(0x015D9E);
static const GColor COLOR_RED = GColorFromHEX(0xDD271F);
static const GColor COLOR_YELLOW = GColorFromHEX(0xEEDB6E);

#define PIXEL_FONT_VARIANT 2

static const uint8_t DIGITS_3X7[10][7] = {
  {0x7, 0x5, 0x5, 0x5, 0x5, 0x5, 0x7},
  {0x2, 0x6, 0x2, 0x2, 0x2, 0x2, 0x7},
  {0x7, 0x1, 0x1, 0x7, 0x4, 0x4, 0x7},
  {0x7, 0x1, 0x1, 0x7, 0x1, 0x1, 0x7},
  {0x5, 0x5, 0x5, 0x7, 0x1, 0x1, 0x1},
  {0x7, 0x4, 0x4, 0x7, 0x1, 0x1, 0x7},
  {0x7, 0x4, 0x4, 0x7, 0x5, 0x5, 0x7},
  {0x7, 0x1, 0x1, 0x2, 0x2, 0x2, 0x2},
  {0x7, 0x5, 0x5, 0x7, 0x5, 0x5, 0x7},
  {0x7, 0x5, 0x5, 0x7, 0x1, 0x1, 0x7},
};

static const uint8_t DIGITS_4X9[10][9] = {
  {0x6, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x6},
  {0x2, 0x6, 0xA, 0x2, 0x2, 0x2, 0x2, 0x2, 0x7},
  {0x6, 0x9, 0x1, 0x1, 0x2, 0x4, 0x8, 0x8, 0xF},
  {0xE, 0x1, 0x1, 0x2, 0x6, 0x1, 0x1, 0x1, 0xE},
  {0x1, 0x3, 0x5, 0x9, 0xF, 0x1, 0x1, 0x1, 0x1},
  {0xF, 0x8, 0x8, 0xE, 0x1, 0x1, 0x1, 0x9, 0x6},
  {0x6, 0x8, 0x8, 0xE, 0x9, 0x9, 0x9, 0x9, 0x6},
  {0xF, 0x1, 0x1, 0x2, 0x2, 0x4, 0x4, 0x8, 0x8},
  {0x6, 0x9, 0x9, 0x6, 0x9, 0x9, 0x9, 0x9, 0x6},
  {0x6, 0x9, 0x9, 0x9, 0x7, 0x1, 0x1, 0x1, 0x6},
};

static const uint8_t DIGITS_5X9[10][9] = {
  {0xE, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0xE},
  {0x4, 0xC, 0x14, 0x4, 0x4, 0x4, 0x4, 0x4, 0x1F},
  {0xE, 0x11, 0x1, 0x2, 0x4, 0x8, 0x10, 0x10, 0x1F},
  {0x1E, 0x1, 0x1, 0x2, 0xE, 0x1, 0x1, 0x1, 0x1E},
  {0x2, 0x6, 0xA, 0x12, 0x1F, 0x2, 0x2, 0x2, 0x2},
  {0x1F, 0x10, 0x10, 0x1E, 0x1, 0x1, 0x1, 0x11, 0xE},
  {0xE, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x11, 0xE},
  {0x1F, 0x1, 0x2, 0x2, 0x4, 0x4, 0x8, 0x8, 0x8},
  {0xE, 0x11, 0x11, 0xE, 0x11, 0x11, 0x11, 0x11, 0xE},
  {0xE, 0x11, 0x11, 0x11, 0xF, 0x1, 0x1, 0x1, 0xE},
};

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

static uint8_t get_digit_row(int digit, int row, int variant) {
  if (variant == 1) {
    return DIGITS_3X7[digit][row];
  }
  if (variant == 3) {
    return DIGITS_5X9[digit][row];
  }
  return DIGITS_4X9[digit][row];
}

static void draw_pixel_number(GContext *ctx, const char *text, GRect rect,
                              int variant) {
  int cols = 4;
  int rows = 9;
  int pixel_w = 7;
  int pixel_h = 9;
  int digit_gap = 9;

  if (variant == 1) {
    cols = 3;
    rows = 7;
    pixel_w = 6;
    pixel_h = 7;
    digit_gap = 8;
  } else if (variant == 3) {
    cols = 5;
    pixel_w = 4;
    digit_gap = 7;
  }

  const int glyph_w = cols * pixel_w;
  const int glyph_h = rows * pixel_h;
  const int total_w = glyph_w * 2 + digit_gap;
  const int origin_x = rect.origin.x + (rect.size.w - total_w) / 2;
  const int origin_y = rect.origin.y + (rect.size.h - glyph_h) / 2;

  graphics_context_set_fill_color(ctx, GColorBlack);
  for (int digit_index = 0; digit_index < 2; digit_index++) {
    const int digit = text[digit_index] - '0';
    if (digit < 0 || digit > 9) {
      continue;
    }

    const int digit_x = origin_x + digit_index * (glyph_w + digit_gap);
    for (int row = 0; row < rows; row++) {
      const uint8_t row_bits = get_digit_row(digit, row, variant);
      for (int col = 0; col < cols; col++) {
        if (row_bits & (1 << (cols - col - 1))) {
          graphics_fill_rect(
              ctx,
              GRect(digit_x + col * pixel_w, origin_y + row * pixel_h,
                    pixel_w, pixel_h),
              0, GCornerNone);
        }
      }
    }
  }
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  const int gap = 4;
  const int left_w = 120;
  const int right_x = left_w + gap;
  const int right_w = bounds.size.w - right_x;
  const int bottom_h = 43;
  const int bottom_y = bounds.size.h - bottom_h;
  const int top_h = bottom_y - gap;

  fill_rect(ctx, bounds, GColorBlack);

  GRect time_rect = GRect(0, 0, left_w, top_h);
  GRect accent_rect = GRect(right_x, 0, right_w, 68);
  GRect date_rect = GRect(right_x, 72, right_w, top_h - 72);
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

  draw_pixel_number(ctx, hour, GRect(8, 4, left_w - 16, 82),
                    PIXEL_FONT_VARIANT);
  draw_pixel_number(ctx, minute, GRect(8, 95, left_w - 16, 82),
                    PIXEL_FONT_VARIANT);

  char weekday[8];
  char day[4];
  strftime(weekday, sizeof(weekday), "%a", &s_now);
  strftime(day, sizeof(day), "%d", &s_now);
  draw_text(ctx, weekday, GRect(right_x + 4, 95, right_w - 8, 28),
            FONT_KEY_GOTHIC_24_BOLD, GColorBlack, GTextAlignmentCenter);
  draw_text(ctx, day, GRect(right_x + 4, 121, right_w - 8, 28),
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

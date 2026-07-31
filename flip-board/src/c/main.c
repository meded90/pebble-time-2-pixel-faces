#include <pebble.h>

static Window *s_window;
static Layer *s_canvas;
static AppTimer *s_flip_timer;
static int s_battery_percent;
static struct tm s_now;
static char s_visible_digits[5];
static char s_next_digits[5];
static bool s_flipping_digits[4];
static uint8_t s_flip_step;
static GColor s_background_color;
static GColor s_line_color;
static GColor s_digit_color;

static const GColor COLOR_HIGHLIGHT = GColorFromHEX(0xAAAAAA);
static const GColor COLOR_GREEN = GColorFromHEX(0x70D45E);

#define DEFAULT_BACKGROUND_COLOR_HEX 0x555555
#define DEFAULT_LINE_COLOR_HEX 0x000000
#define DEFAULT_DIGIT_COLOR_HEX 0xE8E0C8
#define FLIP_STEPS 8
#define FLIP_FRAME_MS 65
#define GLYPH_COLUMNS 5
#define GLYPH_ROWS 10
#define GLYPH_CELL_WIDTH 6
#define GLYPH_CELL_HEIGHT 7

enum {
  PERSIST_KEY_BACKGROUND_COLOR = 100,
  PERSIST_KEY_LINE_COLOR = 101,
  PERSIST_KEY_DIGIT_COLOR = 102,
};

static const uint8_t DIGIT_ROWS[10][GLYPH_ROWS] = {
  {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
  {0x04, 0x0C, 0x14, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F},
  {0x0E, 0x11, 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10, 0x1F},
  {0x1E, 0x01, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x01, 0x01, 0x1E},
  {0x02, 0x06, 0x0A, 0x12, 0x12, 0x1F, 0x02, 0x02, 0x02, 0x02},
  {0x1F, 0x10, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x01, 0x11, 0x0E},
  {0x06, 0x08, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x11, 0x0E},
  {0x1F, 0x01, 0x01, 0x02, 0x02, 0x04, 0x04, 0x08, 0x08, 0x08},
  {0x0E, 0x11, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x11, 0x11, 0x0E},
  {0x0E, 0x11, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x01, 0x02, 0x0C},
};

static void draw_text(GContext *ctx, const char *text, GRect rect,
                      const char *font_key, GColor color,
                      GTextAlignment alignment) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, fonts_get_system_font(font_key), rect,
                     GTextOverflowModeTrailingEllipsis, alignment, NULL);
}

static GRect panel_face_rect(GRect rect) {
  return GRect(rect.origin.x, rect.origin.y, rect.size.w - 4, rect.size.h - 5);
}

static void draw_digit_half(GContext *ctx, GRect face, char digit,
                            bool top_half, int scale_percent) {
  if (digit < '0' || digit > '9' || scale_percent <= 0) {
    return;
  }

  const int digit_index = digit - '0';
  const int glyph_width = GLYPH_COLUMNS * GLYPH_CELL_WIDTH;
  const int glyph_height = GLYPH_ROWS * GLYPH_CELL_HEIGHT;
  const int glyph_x = face.origin.x + (face.size.w - glyph_width) / 2;
  const int glyph_y = face.origin.y + (face.size.h - glyph_height) / 2;
  const int hinge_y = face.origin.y + face.size.h / 2;
  const int first_row = top_half ? 0 : GLYPH_ROWS / 2;
  const int last_row = top_half ? GLYPH_ROWS / 2 : GLYPH_ROWS;

  graphics_context_set_fill_color(ctx, s_digit_color);
  for (int row = first_row; row < last_row; ++row) {
    const int source_y = glyph_y + row * GLYPH_CELL_HEIGHT;
    const int half_offset = top_half
      ? source_y - glyph_y
      : source_y - hinge_y;
    const int scaled_start = half_offset * scale_percent / 100;
    const int scaled_end =
      (half_offset + GLYPH_CELL_HEIGHT) * scale_percent / 100;
    const int pixel_y = top_half
      ? hinge_y - (glyph_height / 2 * scale_percent / 100) + scaled_start
      : hinge_y + scaled_start;
    const int scaled_height = scaled_end - scaled_start;
    const int pixel_height = scaled_height > 0 ? scaled_height : 1;

    for (int column = 0; column < GLYPH_COLUMNS; ++column) {
      if (DIGIT_ROWS[digit_index][row] & (1 << (GLYPH_COLUMNS - 1 - column))) {
        graphics_fill_rect(
          ctx,
          GRect(glyph_x + column * GLYPH_CELL_WIDTH, pixel_y,
                GLYPH_CELL_WIDTH, pixel_height),
          0, GCornerNone);
      }
    }
  }
}

static void draw_panel_shell(GContext *ctx, GRect rect) {
  const GRect face = panel_face_rect(rect);
  const int hinge_y = face.origin.y + face.size.h / 2;

  graphics_context_set_fill_color(ctx, s_line_color);
  graphics_fill_rect(ctx, GRect(rect.origin.x + 4, rect.origin.y + 5,
                                rect.size.w - 4, rect.size.h - 5),
                     6, GCornersAll);

  graphics_context_set_fill_color(ctx, s_background_color);
  graphics_fill_rect(ctx, face, 5, GCornersAll);

  graphics_context_set_stroke_color(ctx, s_line_color);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_round_rect(ctx, face, 5);

  graphics_context_set_stroke_color(ctx, COLOR_HIGHLIGHT);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(face.origin.x + 6, face.origin.y + 2),
                     GPoint(face.origin.x + face.size.w - 7,
                            face.origin.y + 2));
  graphics_draw_line(ctx, GPoint(face.origin.x + 2, face.origin.y + 7),
                     GPoint(face.origin.x + 2, hinge_y - 7));
  graphics_draw_line(ctx, GPoint(face.origin.x + 2, hinge_y + 7),
                     GPoint(face.origin.x + 2,
                            face.origin.y + face.size.h - 8));
}

static void draw_panel_hinge(GContext *ctx, GRect face) {
  const int hinge_y = face.origin.y + face.size.h / 2;
  graphics_context_set_stroke_color(ctx, s_line_color);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_line(ctx, GPoint(face.origin.x, hinge_y),
                     GPoint(face.origin.x + face.size.w - 1, hinge_y));

  graphics_context_set_fill_color(ctx, s_line_color);
  graphics_fill_rect(ctx, GRect(face.origin.x - 3, hinge_y - 5, 6, 10),
                     1, GCornersAll);
  graphics_fill_rect(ctx, GRect(face.origin.x + face.size.w - 3,
                                hinge_y - 5, 6, 10),
                     1, GCornersAll);

  graphics_context_set_stroke_color(ctx, COLOR_HIGHLIGHT);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(face.origin.x - 2, hinge_y - 4),
                     GPoint(face.origin.x + 1, hinge_y - 4));
  graphics_draw_line(ctx,
                     GPoint(face.origin.x + face.size.w - 2, hinge_y - 4),
                     GPoint(face.origin.x + face.size.w + 1, hinge_y - 4));
  graphics_draw_line(ctx, GPoint(face.origin.x - 2, hinge_y - 3),
                     GPoint(face.origin.x - 2, hinge_y + 2));
  graphics_draw_line(ctx,
                     GPoint(face.origin.x + face.size.w - 2, hinge_y - 3),
                     GPoint(face.origin.x + face.size.w - 2, hinge_y + 2));
}

static void draw_flip_digit(GContext *ctx, GRect rect, int index) {
  const GRect face = panel_face_rect(rect);
  draw_panel_shell(ctx, rect);

  if (!s_flipping_digits[index]) {
    draw_digit_half(ctx, face, s_visible_digits[index], true, 100);
    draw_digit_half(ctx, face, s_visible_digits[index], false, 100);
  } else {
    const int progress = s_flip_step * 100 / FLIP_STEPS;
    if (progress < 50) {
      draw_digit_half(ctx, face, s_visible_digits[index], true,
                      100 - progress * 2);
      draw_digit_half(ctx, face, s_visible_digits[index], false, 100);
    } else {
      draw_digit_half(ctx, face, s_next_digits[index], true, 100);
      draw_digit_half(ctx, face, s_next_digits[index], false,
                      (progress - 50) * 2);
    }
  }

  draw_panel_hinge(ctx, face);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, s_background_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  const int panel_w = 56;
  const int panel_h = 93;
  draw_flip_digit(ctx, GRect(13, 13, panel_w, panel_h), 0);
  draw_flip_digit(ctx, GRect(70, 13, panel_w, panel_h), 1);
  draw_flip_digit(ctx, GRect(13, 122, panel_w, panel_h), 2);
  draw_flip_digit(ctx, GRect(70, 122, panel_w, panel_h), 3);

  char weekday[8];
  char day[4];
  char month[8];
  strftime(weekday, sizeof(weekday), "%a", &s_now);
  strftime(day, sizeof(day), "%d", &s_now);
  strftime(month, sizeof(month), "%b", &s_now);
  for (int i = 0; weekday[i]; ++i) {
    if (weekday[i] >= 'a' && weekday[i] <= 'z') {
      weekday[i] -= 'a' - 'A';
    }
  }
  for (int i = 0; month[i]; ++i) {
    if (month[i] >= 'a' && month[i] <= 'z') {
      month[i] -= 'a' - 'A';
    }
  }

  draw_text(ctx, weekday, GRect(132, 34, 64, 30),
            FONT_KEY_GOTHIC_24_BOLD, s_digit_color, GTextAlignmentCenter);
  draw_text(ctx, day, GRect(132, 69, 64, 34),
            FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM, s_digit_color,
            GTextAlignmentCenter);
  draw_text(ctx, month, GRect(132, 106, 64, 28),
            FONT_KEY_GOTHIC_24_BOLD, s_digit_color, GTextAlignmentCenter);

  graphics_context_set_stroke_color(ctx, s_line_color);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(141, 151), GPoint(190, 151));

  char battery[8];
  snprintf(battery, sizeof(battery), "%d%%", s_battery_percent);
  draw_text(ctx, battery, GRect(132, 173, 64, 30),
            FONT_KEY_GOTHIC_24_BOLD, COLOR_GREEN, GTextAlignmentCenter);
}

static void format_time_digits(char output[5], const struct tm *time_value) {
  strftime(output, 5, clock_is_24h_style() ? "%H%M" : "%I%M", time_value);
}

static void flip_timer_callback(void *context) {
  s_flip_timer = NULL;
  ++s_flip_step;

  if (s_flip_step >= FLIP_STEPS) {
    memcpy(s_visible_digits, s_next_digits, sizeof(s_visible_digits));
    memset(s_flipping_digits, 0, sizeof(s_flipping_digits));
  } else {
    s_flip_timer = app_timer_register(FLIP_FRAME_MS, flip_timer_callback, NULL);
  }

  layer_mark_dirty(s_canvas);
}

static void begin_flip_to(const char next_digits[5]) {
  bool has_change = false;
  for (int i = 0; i < 4; ++i) {
    s_flipping_digits[i] = s_visible_digits[i] != next_digits[i];
    has_change = has_change || s_flipping_digits[i];
  }

  memcpy(s_next_digits, next_digits, sizeof(s_next_digits));
  if (!has_change) {
    return;
  }

  if (s_flip_timer) {
    app_timer_cancel(s_flip_timer);
  }
  s_flip_step = 0;
  s_flip_timer = app_timer_register(FLIP_FRAME_MS, flip_timer_callback, NULL);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_now = *tick_time;
  char next_digits[5];
  format_time_digits(next_digits, &s_now);
  begin_flip_to(next_digits);
  layer_mark_dirty(s_canvas);
}

static void battery_handler(BatteryChargeState state) {
  s_battery_percent = state.charge_percent;
  layer_mark_dirty(s_canvas);
}

static GColor load_color(int persist_key, uint32_t default_hex) {
  const uint32_t hex = persist_exists(persist_key)
    ? (uint32_t)persist_read_int(persist_key)
    : default_hex;
  return GColorFromHEX(hex);
}

static bool update_color_from_message(DictionaryIterator *iterator,
                                      uint32_t message_key, int persist_key,
                                      GColor *color) {
  Tuple *tuple = dict_find(iterator, message_key);
  if (!tuple) {
    return false;
  }

  const uint32_t hex = tuple->value->uint32 & 0xFFFFFF;
  *color = GColorFromHEX(hex);
  persist_write_int(persist_key, (int32_t)hex);
  return true;
}

static void inbox_received_handler(DictionaryIterator *iterator,
                                   void *context) {
  bool changed = false;
  changed |= update_color_from_message(
    iterator, MESSAGE_KEY_BACKGROUND_COLOR, PERSIST_KEY_BACKGROUND_COLOR,
    &s_background_color);
  changed |= update_color_from_message(
    iterator, MESSAGE_KEY_LINE_COLOR, PERSIST_KEY_LINE_COLOR, &s_line_color);
  changed |= update_color_from_message(
    iterator, MESSAGE_KEY_DIGIT_COLOR, PERSIST_KEY_DIGIT_COLOR,
    &s_digit_color);

  if (changed) {
    layer_mark_dirty(s_canvas);
  }
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
  s_background_color =
    load_color(PERSIST_KEY_BACKGROUND_COLOR, DEFAULT_BACKGROUND_COLOR_HEX);
  s_line_color = load_color(PERSIST_KEY_LINE_COLOR, DEFAULT_LINE_COLOR_HEX);
  s_digit_color = load_color(PERSIST_KEY_DIGIT_COLOR, DEFAULT_DIGIT_COLOR_HEX);

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  time_t now = time(NULL);
  s_now = *localtime(&now);
  format_time_digits(s_visible_digits, &s_now);
  memcpy(s_next_digits, s_visible_digits, sizeof(s_next_digits));
  battery_handler(battery_state_service_peek());

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);

  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(128, 128);
}

static void deinit(void) {
  if (s_flip_timer) {
    app_timer_cancel(s_flip_timer);
  }
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  app_message_deregister_callbacks();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

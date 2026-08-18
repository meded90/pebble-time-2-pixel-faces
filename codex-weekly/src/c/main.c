#include <pebble.h>

#define ACTIVITY_COLUMNS 12
#define ACTIVITY_ROWS 7
#define ACTIVITY_COUNT (ACTIVITY_COLUMNS * ACTIVITY_ROWS)

enum {
  PERSIST_WEEK_LEFT = 1,
  PERSIST_RESET_TEXT = 2,
  PERSIST_ACTIVITY_MAP = 3,
};

static Window *s_window;
static Layer *s_canvas;
static AppTimer *s_initial_sync_timer;
static struct tm s_now;
static int s_week_left;
static int s_sync_state;
static bool s_has_weekly_data;
static bool s_has_activity_data;
static char s_reset_text[12] = "-";
static char s_activity_map[ACTIVITY_COUNT + 1];

static const GColor COLOR_WHITE = GColorFromHEX(0xFFFFFF);
static const GColor COLOR_GRAY = GColorFromHEX(0x555555);
static const GColor COLOR_LIGHT_GRAY = GColorFromHEX(0xAAAAAA);
static const GColor COLOR_CYAN = GColorFromHEX(0x00FFFF);
static const GColor COLOR_LIME = GColorFromHEX(0xAAFF00);
static const GColor COLOR_YELLOW = GColorFromHEX(0xFFFF00);
static const GColor COLOR_RED = GColorFromHEX(0xFF0000);
static const GColor ACTIVITY_COLORS[5] = {
  GColorFromHEX(0x000055),
  GColorFromHEX(0x0055AA),
  GColorFromHEX(0x0055FF),
  GColorFromHEX(0x00AAFF),
  GColorFromHEX(0x00FFFF),
};
static const char *MONTH_NAMES[12] = {
  "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
  "JUL", "AUG", "SEP", "OCT", "NOV", "DEC",
};

static const char FONT_3X5_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789%:-.?";
static const uint8_t FONT_3X5[][5] = {
  {2, 5, 7, 5, 5}, {6, 5, 6, 5, 6}, {3, 4, 4, 4, 3},
  {6, 5, 5, 5, 6}, {7, 4, 6, 4, 7}, {7, 4, 6, 4, 4},
  {3, 4, 5, 5, 3}, {5, 5, 7, 5, 5}, {7, 2, 2, 2, 7},
  {1, 1, 1, 5, 2}, {5, 5, 6, 5, 5}, {4, 4, 4, 4, 7},
  {5, 7, 7, 5, 5}, {5, 7, 7, 7, 5}, {2, 5, 5, 5, 2},
  {6, 5, 6, 4, 4}, {2, 5, 5, 3, 1}, {6, 5, 6, 5, 5},
  {3, 4, 2, 1, 6}, {7, 2, 2, 2, 2}, {5, 5, 5, 5, 7},
  {5, 5, 5, 5, 2}, {5, 5, 7, 7, 5}, {5, 5, 2, 5, 5},
  {5, 5, 2, 2, 2}, {7, 1, 2, 4, 7},
  {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {6, 1, 2, 4, 7},
  {6, 1, 2, 1, 6}, {5, 5, 7, 1, 1}, {7, 4, 6, 1, 6},
  {7, 4, 7, 5, 7}, {7, 1, 2, 2, 2}, {7, 5, 7, 5, 7},
  {7, 5, 7, 1, 7},
  {5, 1, 2, 4, 5}, {0, 2, 0, 2, 0}, {0, 0, 7, 0, 0},
  {0, 0, 0, 0, 2}, {7, 1, 2, 0, 2},
};

static const uint8_t DIGITS_5X7[10][7] = {
  {14, 17, 19, 21, 25, 17, 14},
  {4, 12, 4, 4, 4, 4, 14},
  {14, 17, 1, 2, 4, 8, 31},
  {30, 1, 1, 14, 1, 1, 30},
  {2, 6, 10, 18, 31, 2, 2},
  {31, 16, 16, 30, 1, 1, 30},
  {14, 16, 16, 30, 17, 17, 14},
  {31, 1, 2, 4, 8, 8, 8},
  {14, 17, 17, 14, 17, 17, 14},
  {14, 17, 17, 15, 1, 1, 14},
};

static void fill_rect(GContext *ctx, GRect rect, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_rect(ctx, rect, 0, GCornerNone);
}

static const uint8_t *small_glyph(char character) {
  const char *match = strchr(FONT_3X5_CHARS, character);
  if (!match) {
    return NULL;
  }
  return FONT_3X5[match - FONT_3X5_CHARS];
}

static void draw_small_text(GContext *ctx, const char *text, int x, int y,
                            int scale, GColor color) {
  graphics_context_set_fill_color(ctx, color);

  for (int character_index = 0; text[character_index]; ++character_index) {
    char character = text[character_index];
    if (character == ' ') {
      x += scale * 4;
      continue;
    }

    const uint8_t *glyph = small_glyph(character);
    if (!glyph) {
      x += scale * 4;
      continue;
    }

    for (int row = 0; row < 5; ++row) {
      for (int column = 0; column < 3; ++column) {
        if (glyph[row] & (1 << (2 - column))) {
          fill_rect(ctx,
                    GRect(x + column * scale, y + row * scale, scale, scale),
                    color);
        }
      }
    }
    x += scale * 4;
  }
}

static void draw_large_time(GContext *ctx) {
  char time_text[6];
  strftime(time_text, sizeof(time_text),
           clock_is_24h_style() ? "%H:%M" : "%I:%M", &s_now);

  const int scale = 8;
  int x = 8;
  const int y = 20;
  graphics_context_set_fill_color(ctx, COLOR_WHITE);

  for (int character_index = 0; time_text[character_index]; ++character_index) {
    const char character = time_text[character_index];
    if (character == ':') {
      fill_rect(ctx, GRect(x, y + 2 * scale, scale, scale), COLOR_WHITE);
      fill_rect(ctx, GRect(x, y + 4 * scale, scale, scale), COLOR_WHITE);
      x += scale + 4;
      continue;
    }

    const int digit = character - '0';
    if (digit < 0 || digit > 9) {
      continue;
    }

    for (int row = 0; row < 7; ++row) {
      for (int column = 0; column < 5; ++column) {
        if (DIGITS_5X7[digit][row] & (1 << (4 - column))) {
          fill_rect(ctx,
                    GRect(x + column * scale, y + row * scale, scale, scale),
                    COLOR_WHITE);
        }
      }
    }
    x += 5 * scale + 4;
  }
}

static void draw_weekly_limit(GContext *ctx) {
  draw_small_text(ctx, "WEEK LEFT", 4, 84, 2, COLOR_CYAN);

  if (s_has_weekly_data) {
    char percent_text[8];
    snprintf(percent_text, sizeof(percent_text), "%d%%", s_week_left);
    draw_small_text(ctx, percent_text, 152, 84, 4, COLOR_CYAN);
  } else {
    fill_rect(ctx, GRect(170, 92, 26, 4), COLOR_CYAN);
  }

  const int segment_count = 12;
  const int segment_width = 14;
  const int segment_gap = 2;
  const int filled_segments =
      (s_week_left * segment_count + 50) / 100;

  for (int segment = 0; segment < segment_count; ++segment) {
    GRect rect = GRect(
        5 + segment * (segment_width + segment_gap),
        108,
        segment_width,
        13
    );
    if (!s_has_weekly_data) {
      fill_rect(ctx, rect, COLOR_GRAY);
    } else if (segment < filled_segments) {
      fill_rect(ctx, rect, COLOR_LIME);
    } else {
      graphics_context_set_stroke_color(ctx, COLOR_GRAY);
      graphics_context_set_stroke_width(ctx, 1);
      graphics_draw_rect(ctx, rect);
    }
  }

  if (s_has_weekly_data) {
    char reset_line[24];
    snprintf(reset_line, sizeof(reset_line), "RESET %s", s_reset_text);
    draw_small_text(ctx, reset_line, 4, 126, 2, COLOR_LIGHT_GRAY);
  } else {
    draw_small_text(ctx, "RESET -", 4, 126, 2, COLOR_LIGHT_GRAY);
  }
}

static void draw_activity(GContext *ctx) {
  draw_small_text(ctx, "PERSONAL USAGE", 4, 142, 2, COLOR_CYAN);

  const int grid_x = 4;
  const int grid_y = 158;
  const int cell_width = 14;
  const int cell_height = 6;
  const int gap = 2;
  const int column_width = cell_width + gap;
  const int row_height = cell_height + gap;
  const int grid_height =
      ACTIVITY_ROWS * cell_height + (ACTIVITY_ROWS - 1) * gap;
  const int month_label_y = 216;

  time_t now = time(NULL);
  struct tm *utc_now = gmtime(&now);
  const int monday_offset = utc_now ? (utc_now->tm_wday + 6) % 7 : 0;
  const time_t activity_start =
      now - (monday_offset + (ACTIVITY_COLUMNS - 1) * 7) * 24 * 60 * 60;

  bool first_column_has_month_start = false;
  for (int row = 0; row < ACTIVITY_ROWS; ++row) {
    time_t cell_time = activity_start + row * 24 * 60 * 60;
    struct tm *cell_date = gmtime(&cell_time);
    if (cell_date && cell_date->tm_mday == 1) {
      first_column_has_month_start = true;
      break;
    }
  }

  if (!first_column_has_month_start) {
    struct tm *start_date = gmtime(&activity_start);
    if (start_date) {
      draw_small_text(ctx, MONTH_NAMES[start_date->tm_mon], grid_x,
                      month_label_y, 2, COLOR_LIGHT_GRAY);
    }
  }

  for (int column = 0; column < ACTIVITY_COLUMNS; ++column) {
    for (int row = 0; row < ACTIVITY_ROWS; ++row) {
      const int index = column * ACTIVITY_ROWS + row;
      const int cell_x = grid_x + column * column_width;
      const int cell_y = grid_y + row * row_height;
      int intensity = s_activity_map[index] - '0';
      if (intensity < 0 || intensity > 4) {
        intensity = 0;
      }

      fill_rect(ctx,
                GRect(cell_x, cell_y, cell_width, cell_height),
                s_has_activity_data ? ACTIVITY_COLORS[intensity] : COLOR_GRAY);

      time_t cell_time = activity_start + index * 24 * 60 * 60;
      struct tm *cell_date = gmtime(&cell_time);
      if (!cell_date || cell_date->tm_mday != 1) {
        continue;
      }

      draw_small_text(ctx, MONTH_NAMES[cell_date->tm_mon], cell_x,
                      month_label_y, 2, COLOR_LIGHT_GRAY);
      if (!s_has_activity_data) {
        continue;
      }
      if (row == 0 && column > 0) {
        fill_rect(ctx, GRect(cell_x - gap, grid_y, gap, grid_height),
                  COLOR_YELLOW);
      } else if (row > 0) {
        fill_rect(ctx, GRect(cell_x, cell_y - gap, cell_width, gap),
                  COLOR_YELLOW);
      }
    }
  }
}

static void draw_sync_indicator(GContext *ctx) {
  GColor color = COLOR_GRAY;
  if (s_sync_state == 1) {
    color = COLOR_CYAN;
  } else if (s_sync_state == 2) {
    color = COLOR_LIME;
  } else if (s_sync_state == 3) {
    color = COLOR_RED;
  }
  fill_rect(ctx, GRect(188, 3, 10, 10), color);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  fill_rect(ctx, layer_get_bounds(layer), GColorBlack);
  draw_small_text(ctx, "CODEX", 4, 4, 2, COLOR_WHITE);
  draw_sync_indicator(ctx);
  draw_large_time(ctx);
  draw_weekly_limit(ctx);
  draw_activity(ctx);
}

static void request_sync(void) {
  s_sync_state = 1;
  if (s_canvas) {
    layer_mark_dirty(s_canvas);
  }

  DictionaryIterator *iterator;
  AppMessageResult result = app_message_outbox_begin(&iterator);
  if (result == APP_MSG_OK && iterator) {
    dict_write_uint8(iterator, MESSAGE_KEY_REQUEST_SYNC, 1);
    result = app_message_outbox_send();
  }
  if (result != APP_MSG_OK) {
    s_sync_state = 3;
    if (s_canvas) {
      layer_mark_dirty(s_canvas);
    }
  }
}

static void initial_sync_callback(void *context) {
  s_initial_sync_timer = NULL;
  request_sync();
}

static void save_status(void) {
  if (s_has_weekly_data) {
    persist_write_int(PERSIST_WEEK_LEFT, s_week_left);
    persist_write_string(PERSIST_RESET_TEXT, s_reset_text);
  }
  if (s_has_activity_data) {
    persist_write_string(PERSIST_ACTIVITY_MAP, s_activity_map);
  }
}

static void load_status(void) {
  for (int index = 0; index < ACTIVITY_COUNT; ++index) {
    s_activity_map[index] = '0';
  }
  s_activity_map[ACTIVITY_COUNT] = '\0';

  if (persist_exists(PERSIST_WEEK_LEFT)) {
    s_has_weekly_data = true;
    s_week_left = persist_read_int(PERSIST_WEEK_LEFT);
  }
  if (persist_exists(PERSIST_RESET_TEXT)) {
    persist_read_string(PERSIST_RESET_TEXT, s_reset_text,
                        sizeof(s_reset_text));
  }
  if (persist_exists(PERSIST_ACTIVITY_MAP)) {
    s_has_activity_data = true;
    persist_read_string(PERSIST_ACTIVITY_MAP, s_activity_map,
                        sizeof(s_activity_map));
  }
}

static void inbox_received_handler(DictionaryIterator *iterator,
                                   void *context) {
  Tuple *week_left = dict_find(iterator, MESSAGE_KEY_WEEK_LEFT);
  Tuple *reset_text = dict_find(iterator, MESSAGE_KEY_RESET_TEXT);
  Tuple *activity_map = dict_find(iterator, MESSAGE_KEY_ACTIVITY_MAP);
  Tuple *sync_state = dict_find(iterator, MESSAGE_KEY_SYNC_STATE);

  if (week_left) {
    s_has_weekly_data = true;
    s_week_left = (int)week_left->value->int32;
    if (s_week_left < 0) {
      s_week_left = 0;
    } else if (s_week_left > 100) {
      s_week_left = 100;
    }
  }

  if (reset_text && reset_text->type == TUPLE_CSTRING) {
    snprintf(s_reset_text, sizeof(s_reset_text), "%s",
             reset_text->value->cstring);
  }

  if (activity_map && activity_map->type == TUPLE_CSTRING) {
    s_has_activity_data = true;
    const char *incoming = activity_map->value->cstring;
    const size_t length = strlen(incoming);
    for (int index = 0; index < ACTIVITY_COUNT; ++index) {
      s_activity_map[index] =
          index < (int)length && incoming[index] >= '0' &&
                  incoming[index] <= '4'
              ? incoming[index]
              : '0';
    }
    s_activity_map[ACTIVITY_COUNT] = '\0';
  }

  if (sync_state) {
    s_sync_state = (int)sync_state->value->int32;
  }

  if (week_left || reset_text || activity_map) {
    save_status();
  }
  layer_mark_dirty(s_canvas);
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
  s_sync_state = 3;
  layer_mark_dirty(s_canvas);
}

static void outbox_failed_handler(DictionaryIterator *iterator,
                                  AppMessageResult reason, void *context) {
  s_sync_state = 3;
  layer_mark_dirty(s_canvas);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_now = *tick_time;
  if (tick_time->tm_min % 30 == 0) {
    request_sync();
  }
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
  load_status();

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  time_t now = time(NULL);
  s_now = *localtime(&now);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  app_message_register_inbox_received(inbox_received_handler);
  app_message_register_inbox_dropped(inbox_dropped_handler);
  app_message_register_outbox_failed(outbox_failed_handler);
  app_message_open(256, 64);
  s_initial_sync_timer = app_timer_register(1500, initial_sync_callback, NULL);
}

static void deinit(void) {
  if (s_initial_sync_timer) {
    app_timer_cancel(s_initial_sync_timer);
  }
  tick_timer_service_unsubscribe();
  app_message_deregister_callbacks();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

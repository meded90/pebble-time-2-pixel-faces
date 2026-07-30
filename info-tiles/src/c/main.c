#include <pebble.h>

static Window *s_window;
static Layer *s_canvas;
static int s_battery_percent;
static int s_steps = -1;
static int s_heart_rate = -1;
static int s_temperature;
static int s_weather_code;
static bool s_has_weather;
static struct tm s_now;

static const GColor COLOR_BLUE = GColorFromHEX(0x55779A);
static const GColor COLOR_GREEN = GColorFromHEX(0x55AA55);
static const GColor COLOR_PANEL = GColorFromHEX(0x2B2B2B);
static const GColor COLOR_INK = GColorFromHEX(0xE8E0C8);
static const GColor COLOR_RED = GColorFromHEX(0xC84F2B);
static const GColor COLOR_SUN = GColorFromHEX(0xE0B238);

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

static void draw_battery_icon(GContext *ctx, GPoint origin, int percent) {
  graphics_context_set_stroke_color(ctx, COLOR_INK);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_rect(ctx, GRect(origin.x, origin.y, 25, 13));
  graphics_draw_line(ctx, GPoint(origin.x + 26, origin.y + 4),
                     GPoint(origin.x + 26, origin.y + 8));

  int fill_width = (21 * percent) / 100;
  graphics_context_set_fill_color(ctx, COLOR_GREEN);
  graphics_fill_rect(ctx, GRect(origin.x + 2, origin.y + 2, fill_width, 9),
                     0, GCornerNone);
}

static void draw_heart(GContext *ctx, GPoint center) {
  graphics_context_set_fill_color(ctx, COLOR_RED);
  graphics_fill_circle(ctx, GPoint(center.x - 4, center.y - 3), 5);
  graphics_fill_circle(ctx, GPoint(center.x + 4, center.y - 3), 5);
  GPathInfo path_info = {
    .num_points = 3,
    .points = (GPoint[]) {
      GPoint(center.x - 9, center.y - 1),
      GPoint(center.x + 9, center.y - 1),
      GPoint(center.x, center.y + 10),
    },
  };
  GPath *path = gpath_create(&path_info);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

static void draw_steps_icon(GContext *ctx, GPoint origin) {
  graphics_context_set_fill_color(ctx, COLOR_INK);
  graphics_fill_circle(ctx, GPoint(origin.x + 5, origin.y + 5), 4);
  graphics_fill_rect(ctx, GRect(origin.x + 1, origin.y + 10, 8, 12),
                     3, GCornersAll);
  graphics_fill_circle(ctx, GPoint(origin.x + 17, origin.y + 11), 4);
  graphics_fill_rect(ctx, GRect(origin.x + 13, origin.y + 16, 8, 12),
                     3, GCornersAll);
}

static void draw_weather_icon(GContext *ctx, GPoint origin, int code) {
  bool is_clear = code == 0;
  bool is_rain = code >= 51 && code <= 82;

  graphics_context_set_fill_color(ctx, COLOR_SUN);
  graphics_fill_circle(ctx, GPoint(origin.x + 23, origin.y + 11), 8);
  graphics_context_set_stroke_color(ctx, COLOR_SUN);
  graphics_context_set_stroke_width(ctx, 2);
  for (int i = 0; i < 8; ++i) {
    int32_t angle = TRIG_MAX_ANGLE * i / 8;
    GPoint inner = GPoint(origin.x + 23 + (cos_lookup(angle) * 11) / TRIG_MAX_RATIO,
                          origin.y + 11 + (sin_lookup(angle) * 11) / TRIG_MAX_RATIO);
    GPoint outer = GPoint(origin.x + 23 + (cos_lookup(angle) * 15) / TRIG_MAX_RATIO,
                          origin.y + 11 + (sin_lookup(angle) * 15) / TRIG_MAX_RATIO);
    graphics_draw_line(ctx, inner, outer);
  }

  if (!is_clear) {
    graphics_context_set_fill_color(ctx, COLOR_INK);
    graphics_fill_circle(ctx, GPoint(origin.x + 12, origin.y + 24), 8);
    graphics_fill_circle(ctx, GPoint(origin.x + 23, origin.y + 21), 11);
    graphics_fill_rect(ctx, GRect(origin.x + 6, origin.y + 23, 35, 10),
                       2, GCornersAll);
  }

  if (is_rain) {
    graphics_context_set_stroke_color(ctx, COLOR_BLUE);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx, GPoint(origin.x + 13, origin.y + 36),
                       GPoint(origin.x + 10, origin.y + 42));
    graphics_draw_line(ctx, GPoint(origin.x + 25, origin.y + 36),
                       GPoint(origin.x + 22, origin.y + 42));
    graphics_draw_line(ctx, GPoint(origin.x + 37, origin.y + 36),
                       GPoint(origin.x + 34, origin.y + 42));
  }
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);

  fill_rect(ctx, bounds, GColorBlack);

  GRect time_tile = GRect(0, 0, 124, 72);
  GRect battery_tile = GRect(128, 0, 72, 72);
  GRect weather_tile = GRect(0, 76, 124, 78);
  GRect heart_tile = GRect(128, 76, 72, 78);
  GRect steps_tile = GRect(0, 158, 200, 70);

  fill_rect(ctx, time_tile, COLOR_BLUE);
  fill_rect(ctx, battery_tile, COLOR_PANEL);
  fill_rect(ctx, weather_tile, COLOR_PANEL);
  fill_rect(ctx, heart_tile, COLOR_PANEL);
  fill_rect(ctx, steps_tile, COLOR_GREEN);

  stroke_rect(ctx, time_tile);
  stroke_rect(ctx, battery_tile);
  stroke_rect(ctx, weather_tile);
  stroke_rect(ctx, heart_tile);
  stroke_rect(ctx, steps_tile);

  char time_buffer[8];
  char date_buffer[16];
  strftime(time_buffer, sizeof(time_buffer),
           clock_is_24h_style() ? "%H:%M" : "%I:%M", &s_now);
  strftime(date_buffer, sizeof(date_buffer), "%a %d", &s_now);
  draw_text(ctx, time_buffer, GRect(7, 4, 110, 38),
            FONT_KEY_GOTHIC_28_BOLD, COLOR_INK, GTextAlignmentLeft);
  draw_text(ctx, date_buffer, GRect(8, 38, 108, 28),
            FONT_KEY_GOTHIC_24_BOLD, COLOR_INK, GTextAlignmentLeft);

  draw_battery_icon(ctx, GPoint(145, 11), s_battery_percent);
  char battery_buffer[8];
  snprintf(battery_buffer, sizeof(battery_buffer), "%d%%", s_battery_percent);
  draw_text(ctx, battery_buffer, GRect(132, 35, 64, 30),
            FONT_KEY_GOTHIC_24_BOLD, COLOR_INK, GTextAlignmentCenter);

  draw_weather_icon(ctx, GPoint(8, 91), s_has_weather ? s_weather_code : 3);
  char temperature_buffer[8];
  if (s_has_weather) {
    snprintf(temperature_buffer, sizeof(temperature_buffer), "%d°", s_temperature);
  } else {
    snprintf(temperature_buffer, sizeof(temperature_buffer), "--°");
  }
  draw_text(ctx, temperature_buffer, GRect(56, 94, 62, 38),
            FONT_KEY_GOTHIC_28_BOLD, COLOR_INK, GTextAlignmentCenter);

  draw_heart(ctx, GPoint(164, 98));
  char heart_buffer[16];
  if (s_heart_rate >= 0) {
    snprintf(heart_buffer, sizeof(heart_buffer), "%d", s_heart_rate);
  } else {
    snprintf(heart_buffer, sizeof(heart_buffer), "--");
  }
  draw_text(ctx, heart_buffer, GRect(132, 116, 64, 30),
            FONT_KEY_GOTHIC_24_BOLD, COLOR_INK, GTextAlignmentCenter);

  draw_steps_icon(ctx, GPoint(12, 178));
  char steps_buffer[16];
  if (s_steps >= 0) {
    snprintf(steps_buffer, sizeof(steps_buffer), "%d", s_steps);
  } else {
    snprintf(steps_buffer, sizeof(steps_buffer), "--");
  }
  draw_text(ctx, steps_buffer, GRect(54, 170, 136, 42),
            FONT_KEY_GOTHIC_28_BOLD, COLOR_INK, GTextAlignmentLeft);

}

static void update_health(void) {
#if defined(PBL_HEALTH)
  time_t now = time(NULL);
  HealthServiceAccessibilityMask steps_access =
      health_service_metric_accessible(HealthMetricStepCount,
                                       time_start_of_today(), now);
  if (steps_access & HealthServiceAccessibilityMaskAvailable) {
    s_steps = (int)health_service_sum_today(HealthMetricStepCount);
  } else {
    s_steps = -1;
  }

  HealthServiceAccessibilityMask hr_access =
      health_service_metric_accessible(HealthMetricHeartRateBPM, now, now);
  if (hr_access & HealthServiceAccessibilityMaskAvailable) {
    HealthValue value =
        health_service_peek_current_value(HealthMetricHeartRateBPM);
    s_heart_rate = value > 0 ? (int)value : -1;
  } else {
    s_heart_rate = -1;
  }
#endif
  layer_mark_dirty(s_canvas);
}

static void request_weather(void) {
  DictionaryIterator *iterator;
  AppMessageResult result = app_message_outbox_begin(&iterator);
  if (result == APP_MSG_OK && iterator) {
    dict_write_uint8(iterator, MESSAGE_KEY_REQUEST_WEATHER, 1);
    app_message_outbox_send();
  }
}

static void inbox_received_handler(DictionaryIterator *iterator, void *context) {
  Tuple *temperature = dict_find(iterator, MESSAGE_KEY_TEMPERATURE);
  Tuple *weather_code = dict_find(iterator, MESSAGE_KEY_WEATHER_CODE);
  if (temperature && weather_code) {
    s_temperature = (int)temperature->value->int32;
    s_weather_code = (int)weather_code->value->int32;
    s_has_weather = true;
    layer_mark_dirty(s_canvas);
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_now = *tick_time;
  update_health();
  if (tick_time->tm_min % 30 == 0) {
    request_weather();
  }
  layer_mark_dirty(s_canvas);
}

static void battery_handler(BatteryChargeState state) {
  s_battery_percent = state.charge_percent;
  layer_mark_dirty(s_canvas);
}

static void health_handler(HealthEventType event, void *context) {
  update_health();
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

#if defined(PBL_HEALTH)
  health_service_events_subscribe(health_handler, NULL);
#endif
  update_health();

  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(128, 64);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
#if defined(PBL_HEALTH)
  health_service_events_unsubscribe();
#endif
  app_message_deregister_callbacks();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

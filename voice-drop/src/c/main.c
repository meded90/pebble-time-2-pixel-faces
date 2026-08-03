#include <pebble.h>

static Window *s_window;
static Layer *s_canvas_layer;
static GFont s_time_font;
static GFont s_label_font;
static AppTimer *s_status_timer;
static VoiceDropStatus s_status;
static VoiceDropState s_previous_state = VoiceDropStateIdle;

static const GColor COLOR_BACKGROUND = { .argb = 0b11000000 };
static const GColor COLOR_FOREGROUND = { .argb = 0b11111111 };
static const GColor COLOR_MUTED = { .argb = 0b11101010 };
static const GColor COLOR_RECORD = { .argb = 0b11110000 };

static void draw_centered_text(GContext *ctx, const char *text, int16_t y,
                               int16_t height, GFont font, GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, font, GRect(12, y, PBL_DISPLAY_WIDTH - 24, height),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static const char *state_label(void) {
  switch (s_status.state) {
    case VoiceDropStateRecording:
      return "RECORDING";
    case VoiceDropStateStopping:
      return "SAVING";
    case VoiceDropStateSyncing:
      return "SYNCING";
    case VoiceDropStateError:
      return "ERROR";
    case VoiceDropStateIdle:
    default:
      return s_status.queued_recordings ? "IN QUEUE" : "READY";
  }
}

static void draw_play_icon(GContext *ctx, GPoint center) {
  GPathInfo path_info = {
    .num_points = 3,
    .points = (GPoint[]) {
      {center.x - 15, center.y - 24},
      {center.x - 15, center.y + 24},
      {center.x + 25, center.y},
    },
  };
  GPath *path = gpath_create(&path_info);
  if (!path) {
    return;
  }
  graphics_context_set_fill_color(ctx, COLOR_FOREGROUND);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

static void draw_record_icon(GContext *ctx, GPoint center) {
  graphics_context_set_fill_color(ctx, COLOR_RECORD);
  graphics_fill_circle(ctx, center, 24);
  graphics_context_set_stroke_color(ctx, COLOR_FOREGROUND);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, center, 30);
}

static void draw_progress_icon(GContext *ctx, GPoint center) {
  const int32_t angle = (time(NULL) % 8) * 45;
  graphics_context_set_stroke_color(ctx, COLOR_FOREGROUND);
  graphics_context_set_stroke_width(ctx, 5);
  graphics_draw_arc(ctx, GRect(center.x - 27, center.y - 27, 54, 54),
                    GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(angle),
                    DEG_TO_TRIGANGLE(angle + 275));
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, COLOR_BACKGROUND);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  time_t now = time(NULL);
  struct tm *time_now = localtime(&now);
  char time_buffer[8];
  strftime(time_buffer, sizeof(time_buffer),
           clock_is_24h_style() ? "%H:%M" : "%I:%M", time_now);
  graphics_context_set_text_color(ctx, COLOR_FOREGROUND);
  graphics_draw_text(ctx, time_buffer, s_time_font,
                     GRect(PBL_DISPLAY_WIDTH - 86, 8, 74, 32),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

  const GPoint center = GPoint(PBL_DISPLAY_WIDTH / 2, 111);
  if (s_status.state == VoiceDropStateRecording) {
    draw_record_icon(ctx, center);
  } else if (s_status.state == VoiceDropStateStopping ||
             s_status.state == VoiceDropStateSyncing) {
    draw_progress_icon(ctx, center);
  } else if (s_status.state == VoiceDropStateError) {
    graphics_context_set_stroke_color(ctx, COLOR_RECORD);
    graphics_context_set_stroke_width(ctx, 5);
    graphics_draw_line(ctx, GPoint(center.x - 21, center.y - 21),
                       GPoint(center.x + 21, center.y + 21));
    graphics_draw_line(ctx, GPoint(center.x + 21, center.y - 21),
                       GPoint(center.x - 21, center.y + 21));
  } else {
    draw_play_icon(ctx, center);
  }

  draw_centered_text(ctx, state_label(), 158, 28, s_label_font,
                     s_status.state == VoiceDropStateError ? COLOR_RECORD : COLOR_MUTED);

  char detail[32];
  if (s_status.state == VoiceDropStateRecording ||
      s_status.state == VoiceDropStateStopping) {
    snprintf(detail, sizeof(detail), "%02lu:%02lu",
             (unsigned long)(s_status.elapsed_seconds / 60),
             (unsigned long)(s_status.elapsed_seconds % 60));
  } else if (s_status.state == VoiceDropStateError) {
    snprintf(detail, sizeof(detail), "E%d", (int)s_status.last_error);
  } else if (s_status.queued_recordings) {
    snprintf(detail, sizeof(detail), "%u REC · %lu KB",
             (unsigned)s_status.queued_recordings,
             (unsigned long)(s_status.queued_bytes / 1024));
  } else {
    snprintf(detail, sizeof(detail), "SELECT");
  }
  draw_centered_text(ctx, detail, 187, 28, s_label_font, COLOR_FOREGROUND);
}

static void refresh_status(void *context) {
  s_status_timer = NULL;
  VoiceDropStatus next_status;
  if (voice_drop_get_status(&next_status)) {
    s_status = next_status;
    if (s_status.state != s_previous_state) {
      if (s_status.state == VoiceDropStateRecording) {
        vibes_short_pulse();
      } else if (s_previous_state == VoiceDropStateStopping &&
                 s_status.state == VoiceDropStateIdle) {
        vibes_double_pulse();
      } else if (s_status.state == VoiceDropStateError) {
        vibes_long_pulse();
      }
      s_previous_state = s_status.state;
    }
  } else {
    s_status.state = VoiceDropStateError;
  }
  layer_mark_dirty(s_canvas_layer);
  s_status_timer = app_timer_register(500, refresh_status, NULL);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  VoiceDropResult result = VoiceDropResultBusy;
  if (s_status.state == VoiceDropStateRecording) {
    result = voice_drop_stop();
  } else if (s_status.state == VoiceDropStateIdle ||
             s_status.state == VoiceDropStateError) {
    result = voice_drop_start();
  }
  if (result != VoiceDropResultSuccess) {
    vibes_long_pulse();
  }
  if (s_status_timer) {
    app_timer_cancel(s_status_timer);
    s_status_timer = NULL;
  }
  refresh_status(NULL);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(s_canvas_layer);
}

static void window_load(Window *window) {
  s_canvas_layer = layer_create(layer_get_bounds(window_get_root_layer(window)));
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_get_root_layer(window), s_canvas_layer);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
  s_canvas_layer = NULL;
}

static void init(void) {
  s_time_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  s_label_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  s_window = window_create();
  window_set_background_color(s_window, COLOR_BACKGROUND);
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  refresh_status(NULL);
}

static void deinit(void) {
  if (s_status_timer) {
    app_timer_cancel(s_status_timer);
  }
  VoiceDropStatus final_status;
  if (voice_drop_get_status(&final_status) &&
      (final_status.state == VoiceDropStateRecording ||
       final_status.state == VoiceDropStateError)) {
    // The firmware owns finalization after this call, so it can close the last
    // chunk and commit the recording even while the watchapp is exiting.
    voice_drop_stop();
  }
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

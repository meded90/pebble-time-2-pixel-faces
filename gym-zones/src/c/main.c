#include <pebble.h>

#include "fitness_math.h"
#include "workout_logic.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define APP_STATE_VERSION 1
#define APP_STATE_MAGIC 0x475A5631UL

#define HR_SAMPLE_PERIOD_SECONDS 5
#define HR_STALE_SECONDS 30
#define CONFIRM_TIMEOUT_SECONDS 10
#define HRV_SAMPLE_PERIOD_SECONDS 1
#define HRV_MAX_SAMPLES 240
#define CHART_MINUTES 60

#define WAKEUP_INVALID_ID ((WakeupId)-1)
#define WAKEUP_COOKIE_MAGIC 0x475A0000L
#define WAKEUP_COOKIE_MASK 0xFFFF0000L

enum {
  PERSIST_KEY_SETTINGS = 10,
  PERSIST_KEY_SESSION = 11,
  PERSIST_KEY_REST = 12,
  PERSIST_KEY_SUMMARY = 13,
  PERSIST_KEY_CHART = 14,
  PERSIST_KEY_HRV = 15,
  PERSIST_KEY_REST_GENERATION = 16,
};

typedef enum {
  SCREEN_MAIN = 0,
  SCREEN_HRV,
  SCREEN_HRV_MEASURING,
  SCREEN_CONFIRM,
} Screen;

typedef enum {
  ACCOUNT_NO_DATA = 0,
  ACCOUNT_BELOW_ZONE = 1,
  ACCOUNT_ZONE_1 = 2,
  ACCOUNT_ZONE_2 = 3,
  ACCOUNT_ZONE_3 = 4,
  ACCOUNT_ZONE_4 = 5,
  ACCOUNT_ZONE_5 = 6,
} AccountState;

typedef enum {
  HRV_VIEW_IDLE = 0,
  HRV_VIEW_LOW_SIGNAL,
  HRV_VIEW_NO_SIGNAL,
  HRV_VIEW_UNSUPPORTED,
  HRV_VIEW_HEALTH_OFF,
} HrvViewState;

typedef struct {
  uint32_t magic;
  uint8_t version;
  uint8_t configured;
  uint8_t zone_mode;
  uint8_t age_formula;
  uint16_t max_hr;
  uint8_t age;
  uint8_t target_zone;
  uint8_t zone_vibes;
  uint8_t vibrations_enabled;
  uint16_t rest_preset_seconds;
  uint16_t manual_lower[FITNESS_ZONE_COUNT];
} SettingsV1;

typedef struct {
  uint32_t magic;
  uint8_t version;
  uint8_t active;
  uint8_t accounting_state;
  uint8_t reserved;
  int32_t start_time;
  int32_t closed_at;
  int32_t auto_finish_deadline;
  int32_t accounting_since;
  int32_t last_hr_at;
  uint32_t zone_seconds[FITNESS_ZONE_COUNT];
  uint32_t below_zone_seconds;
  uint32_t no_data_seconds;
  uint16_t max_hr;
  uint16_t reserved_2;
} SessionStateV1;

typedef struct {
  uint32_t magic;
  uint8_t version;
  uint8_t active;
  uint8_t alerted;
  uint8_t signal_available;
  uint16_t generation;
  uint16_t reserved;
  int32_t started_at;
  int32_t deadline;
  int32_t wakeup_id;
} RestStateV1;

typedef struct {
  uint32_t magic;
  uint8_t version;
  uint8_t valid;
  uint8_t auto_finished;
  uint8_t reserved;
  int32_t start_time;
  int32_t end_time;
  uint32_t duration_seconds;
  uint32_t zone_seconds[FITNESS_ZONE_COUNT];
  uint32_t no_data_seconds;
  uint16_t max_hr;
  uint16_t reserved_2;
} WorkoutSummaryV1;

typedef struct {
  uint32_t date_key;
  int32_t measured_at;
  uint16_t rmssd_ms;
  uint16_t mean_ppi_ms;
  uint16_t duration_seconds;
  uint16_t valid_intervals;
  uint16_t rejected_intervals;
  uint16_t reserved;
} HrvResultV1;

typedef struct {
  uint32_t magic;
  uint8_t version;
  uint8_t baseline_count;
  uint8_t latest_valid;
  uint8_t reserved;
  HrvResultV1 baseline[FITNESS_HRV_BASELINE_MAX];
  HrvResultV1 latest;
} HrvStoreV1;

typedef struct {
  uint32_t magic;
  uint8_t version;
  uint8_t head;
  uint8_t reserved[2];
  int32_t last_minute;
  uint8_t bpm[CHART_MINUTES];
  uint8_t sample_count[CHART_MINUTES];
  uint8_t source[CHART_MINUTES];
} ChartStateV1;

typedef char PersistedSettingsMustFit[(sizeof(SettingsV1) <= PERSIST_DATA_MAX_LENGTH) ? 1 : -1];
typedef char PersistedSessionMustFit[(sizeof(SessionStateV1) <= PERSIST_DATA_MAX_LENGTH) ? 1 : -1];
typedef char PersistedRestMustFit[(sizeof(RestStateV1) <= PERSIST_DATA_MAX_LENGTH) ? 1 : -1];
typedef char PersistedSummaryMustFit[(sizeof(WorkoutSummaryV1) <= PERSIST_DATA_MAX_LENGTH) ? 1 : -1];
typedef char PersistedChartMustFit[(sizeof(ChartStateV1) <= PERSIST_DATA_MAX_LENGTH) ? 1 : -1];
typedef char PersistedHrvMustFit[(sizeof(HrvStoreV1) <= PERSIST_DATA_MAX_LENGTH) ? 1 : -1];
typedef char PersistedTotalMustFit[
    ((sizeof(SettingsV1) + sizeof(SessionStateV1) + sizeof(RestStateV1) +
      sizeof(WorkoutSummaryV1) + sizeof(ChartStateV1) + sizeof(HrvStoreV1)) < 4096)
        ? 1
        : -1];

static Window *s_window;
static Layer *s_canvas_layer;
static Layer *s_dots_layer;

static SettingsV1 s_settings;
static SessionStateV1 s_session;
static RestStateV1 s_rest;
static WorkoutSummaryV1 s_summary;
static HrvStoreV1 s_hrv_store;
static ChartStateV1 s_chart;

static FitnessZoneBounds s_zone_bounds;
static FitnessZoneStabilizer s_zone_stabilizer;
static FitnessBpmFilter s_bpm_filter;
static WorkoutHrvLifecycle s_hrv_lifecycle;

static Screen s_screen = SCREEN_MAIN;
static Screen s_confirm_return_screen = SCREEN_MAIN;
static HrvViewState s_hrv_view_state = HRV_VIEW_IDLE;
static int32_t s_confirm_deadline;
static int32_t s_zone_streak_started;
static uint8_t s_stable_zone;
static uint16_t s_current_hr;
static bool s_hr_fresh;
static bool s_zone_recovering = true;
static bool s_health_subscribed;
static bool s_health_no_permission;
static bool s_health_not_supported;
static bool s_stale_wakeup_launch;
static int32_t s_last_persisted_minute;
static int s_last_dot_bucket = -1;
static char s_toast[28];
static int32_t s_toast_until;

static uint16_t s_hrv_samples[HRV_MAX_SAMPLES];
static uint16_t s_hrv_sample_count;
static int32_t s_hrv_first_valid_at;
static int32_t s_hrv_last_event_at;
static bool s_hrv_request_active;
static uint16_t s_rest_generation_seed;
static bool s_canvas_coordinates;

static char s_glance_subtitle[24];

static const GColor COLOR_BLACK = GColorBlack;
static const GColor COLOR_WHITE = GColorWhite;
static const GColor COLOR_STEEL = GColorFromHEX(0x555555);
static const GColor COLOR_DARK_STEEL = GColorFromHEX(0x303030);
static const GColor COLOR_MUTED = GColorFromHEX(0xAAAAAA);
static const GColor COLOR_GOAL = GColorFromHEX(0xFFFF55);
static const GColor COLOR_ZONES[FITNESS_ZONE_COUNT] = {
    GColorFromHEX(0x55AAFF),
    GColorFromHEX(0x55FFAA),
    GColorFromHEX(0xFFFF55),
    GColorFromHEX(0xFFAA00),
    GColorFromHEX(0xFF0055),
};

static uint32_t seconds_between(int32_t end, int32_t start) {
  if (end <= start) {
    return 0;
  }
  return (uint32_t)end - (uint32_t)start;
}

static uint16_t clamp_u16(uint32_t value) {
  return value > UINT16_MAX ? UINT16_MAX : (uint16_t)value;
}

static int32_t now_seconds(void) {
  return (int32_t)time(NULL);
}

static void mark_canvas_dirty(void) {
  if (s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void mark_dots_dirty(void) {
  if (s_dots_layer) {
    layer_mark_dirty(s_dots_layer);
  }
}

static bool write_blob(uint32_t key, const void *data, size_t size) {
  const int result = persist_write_data(key, data, size);
  if (result != (int)size) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "persist %lu failed: %d", (unsigned long)key, result);
    return false;
  }
  return true;
}

static bool read_blob(uint32_t key, void *data, size_t size) {
  if (!persist_exists(key) || persist_get_size(key) != (int)size) {
    return false;
  }
  return persist_read_data(key, data, size) == (int)size;
}

static void settings_defaults(void) {
  memset(&s_settings, 0, sizeof(s_settings));
  s_settings.magic = APP_STATE_MAGIC;
  s_settings.version = APP_STATE_VERSION;
  s_settings.zone_mode = FITNESS_ZONE_MODE_MAX_HR;
  s_settings.age_formula = FITNESS_AGE_FORMULA_208;
  s_settings.max_hr = 190;
  s_settings.age = 30;
  s_settings.vibrations_enabled = true;
  s_settings.rest_preset_seconds = 120;
  const uint16_t defaults[FITNESS_ZONE_COUNT] = {95, 114, 133, 152, 171};
  memcpy(s_settings.manual_lower, defaults, sizeof(defaults));
}

static FitnessZoneConfig zone_config_from_settings(const SettingsV1 *settings) {
  FitnessZoneConfig config;
  memset(&config, 0, sizeof(config));
  config.mode = (FitnessZoneMode)settings->zone_mode;
  config.max_hr = settings->max_hr;
  config.age = settings->age;
  config.age_formula = (FitnessAgeFormula)settings->age_formula;
  memcpy(config.manual_lower, settings->manual_lower, sizeof(config.manual_lower));
  return config;
}

static bool settings_are_valid(const SettingsV1 *settings, bool require_zone_config) {
  if (!settings || settings->magic != APP_STATE_MAGIC ||
      settings->version != APP_STATE_VERSION || settings->configured > 1 ||
      settings->zone_mode > FITNESS_ZONE_MODE_MANUAL ||
      settings->age_formula > FITNESS_AGE_FORMULA_220 ||
      settings->target_zone > FITNESS_ZONE_COUNT ||
      settings->zone_vibes > 1 || settings->vibrations_enabled > 1) {
    return false;
  }
  if (settings->rest_preset_seconds != 90 && settings->rest_preset_seconds != 120 &&
      settings->rest_preset_seconds != 180 && settings->rest_preset_seconds != 300) {
    return false;
  }
  if (!require_zone_config && !settings->configured) {
    return true;
  }
  FitnessZoneBounds ignored;
  const FitnessZoneConfig config = zone_config_from_settings(settings);
  return fitness_zone_bounds_build(&config, &ignored);
}

static void refresh_zone_bounds(void) {
  memset(&s_zone_bounds, 0, sizeof(s_zone_bounds));
  if (!s_settings.configured) {
    return;
  }
  const FitnessZoneConfig config = zone_config_from_settings(&s_settings);
  if (!fitness_zone_bounds_build(&config, &s_zone_bounds)) {
    s_settings.configured = false;
  }
}

static void load_settings(void) {
  SettingsV1 loaded;
  if (read_blob(PERSIST_KEY_SETTINGS, &loaded, sizeof(loaded)) &&
      settings_are_valid(&loaded, false)) {
    s_settings = loaded;
  } else {
    settings_defaults();
  }
  refresh_zone_bounds();
}

static void save_settings(void) {
  write_blob(PERSIST_KEY_SETTINGS, &s_settings, sizeof(s_settings));
}

static void reset_rest_state(void) {
  memset(&s_rest, 0, sizeof(s_rest));
  s_rest.magic = APP_STATE_MAGIC;
  s_rest.version = APP_STATE_VERSION;
  s_rest.wakeup_id = WAKEUP_INVALID_ID;
  s_rest.signal_available = true;
}

static void reset_chart(void) {
  memset(&s_chart, 0, sizeof(s_chart));
  s_chart.magic = APP_STATE_MAGIC;
  s_chart.version = APP_STATE_VERSION;
}

static void reset_session(void) {
  memset(&s_session, 0, sizeof(s_session));
  s_session.magic = APP_STATE_MAGIC;
  s_session.version = APP_STATE_VERSION;
  s_session.accounting_state = ACCOUNT_NO_DATA;
}

static void reset_summary(void) {
  memset(&s_summary, 0, sizeof(s_summary));
  s_summary.magic = APP_STATE_MAGIC;
  s_summary.version = APP_STATE_VERSION;
}

static void reset_hrv_store(void) {
  memset(&s_hrv_store, 0, sizeof(s_hrv_store));
  s_hrv_store.magic = APP_STATE_MAGIC;
  s_hrv_store.version = APP_STATE_VERSION;
}

static void load_persisted_state(void) {
  SessionStateV1 session;
  if (read_blob(PERSIST_KEY_SESSION, &session, sizeof(session)) &&
      session.magic == APP_STATE_MAGIC && session.version == APP_STATE_VERSION &&
      session.active && session.accounting_state <= ACCOUNT_ZONE_5) {
    s_session = session;
  } else {
    reset_session();
  }

  RestStateV1 rest;
  if (read_blob(PERSIST_KEY_REST, &rest, sizeof(rest)) &&
      rest.magic == APP_STATE_MAGIC && rest.version == APP_STATE_VERSION &&
      rest.active) {
    s_rest = rest;
  } else {
    reset_rest_state();
  }

  WorkoutSummaryV1 summary;
  if (read_blob(PERSIST_KEY_SUMMARY, &summary, sizeof(summary)) &&
      summary.magic == APP_STATE_MAGIC && summary.version == APP_STATE_VERSION &&
      summary.valid) {
    s_summary = summary;
  } else {
    reset_summary();
  }

  ChartStateV1 chart;
  if (s_session.active && read_blob(PERSIST_KEY_CHART, &chart, sizeof(chart)) &&
      chart.magic == APP_STATE_MAGIC && chart.version == APP_STATE_VERSION &&
      chart.head < CHART_MINUTES) {
    s_chart = chart;
  } else {
    reset_chart();
  }

  HrvStoreV1 hrv;
  if (read_blob(PERSIST_KEY_HRV, &hrv, sizeof(hrv)) &&
      hrv.magic == APP_STATE_MAGIC && hrv.version == APP_STATE_VERSION &&
      hrv.baseline_count <= FITNESS_HRV_BASELINE_MAX && hrv.latest_valid <= 1) {
    s_hrv_store = hrv;
  } else {
    reset_hrv_store();
  }
}

static void save_session(void) {
  if (s_session.active) {
    write_blob(PERSIST_KEY_SESSION, &s_session, sizeof(s_session));
  } else if (persist_exists(PERSIST_KEY_SESSION)) {
    persist_delete(PERSIST_KEY_SESSION);
  }
}

static void save_rest(void) {
  if (s_rest.active) {
    write_blob(PERSIST_KEY_REST, &s_rest, sizeof(s_rest));
  } else if (persist_exists(PERSIST_KEY_REST)) {
    persist_delete(PERSIST_KEY_REST);
  }
}

static void save_chart(void) {
  if (s_session.active) {
    write_blob(PERSIST_KEY_CHART, &s_chart, sizeof(s_chart));
  } else if (persist_exists(PERSIST_KEY_CHART)) {
    persist_delete(PERSIST_KEY_CHART);
  }
}

static void save_summary(void) {
  if (s_summary.valid) {
    write_blob(PERSIST_KEY_SUMMARY, &s_summary, sizeof(s_summary));
  }
}

static void save_hrv_store(void) {
  write_blob(PERSIST_KEY_HRV, &s_hrv_store, sizeof(s_hrv_store));
}

static void account_add(uint8_t state, uint32_t seconds) {
  if (seconds == 0) {
    return;
  }
  if (state == ACCOUNT_NO_DATA) {
    s_session.no_data_seconds += seconds;
  } else if (state == ACCOUNT_BELOW_ZONE) {
    s_session.below_zone_seconds += seconds;
  } else if (state >= ACCOUNT_ZONE_1 && state <= ACCOUNT_ZONE_5) {
    s_session.zone_seconds[state - ACCOUNT_ZONE_1] += seconds;
  }
}

static void session_account_to(int32_t end_time) {
  if (!s_session.active || end_time <= s_session.accounting_since) {
    return;
  }

  const uint8_t state = s_session.accounting_state;
  if (state != ACCOUNT_NO_DATA && s_session.last_hr_at > 0) {
    const int32_t stale_at = s_session.last_hr_at + HR_STALE_SECONDS;
    if (s_session.accounting_since < stale_at && end_time > stale_at) {
      account_add(state, seconds_between(stale_at, s_session.accounting_since));
      account_add(ACCOUNT_NO_DATA, seconds_between(end_time, stale_at));
      s_session.accounting_state = ACCOUNT_NO_DATA;
      s_session.accounting_since = end_time;
      return;
    }
  }

  account_add(state, seconds_between(end_time, s_session.accounting_since));
  s_session.accounting_since = end_time;
}

static void transition_account_state(uint8_t state, int32_t at_time) {
  session_account_to(at_time);
  s_session.accounting_state = state;
  s_session.accounting_since = at_time;
}

static uint8_t account_state_for_zone(uint8_t zone) {
  return zone == 0 ? ACCOUNT_BELOW_ZONE : (uint8_t)(ACCOUNT_ZONE_1 + zone - 1);
}

static uint8_t chart_physical_index(uint8_t logical_index) {
  return (uint8_t)((s_chart.head + logical_index) % CHART_MINUTES);
}

static void chart_shift(uint32_t minutes) {
  if (minutes >= CHART_MINUTES) {
    memset(s_chart.bpm, 0, sizeof(s_chart.bpm));
    memset(s_chart.sample_count, 0, sizeof(s_chart.sample_count));
    memset(s_chart.source, 0, sizeof(s_chart.source));
    s_chart.head = 0;
    return;
  }
  for (uint32_t minute = 0; minute < minutes; ++minute) {
    s_chart.head = (uint8_t)((s_chart.head + 1U) % CHART_MINUTES);
    const uint8_t newest = chart_physical_index(CHART_MINUTES - 1);
    s_chart.bpm[newest] = 0;
    s_chart.sample_count[newest] = 0;
    s_chart.source[newest] = 0;
  }
}

static bool chart_advance(int32_t at_time) {
  const int32_t minute = at_time / 60;
  if (s_chart.last_minute == 0) {
    s_chart.last_minute = minute;
    return true;
  }
  if (minute <= s_chart.last_minute) {
    return false;
  }
  chart_shift((uint32_t)(minute - s_chart.last_minute));
  s_chart.last_minute = minute;
  return true;
}

static void chart_add_local(uint16_t bpm, int32_t at_time) {
  chart_advance(at_time);
  const uint8_t index = chart_physical_index(CHART_MINUTES - 1);
  const uint8_t value = bpm > UINT8_MAX ? UINT8_MAX : (uint8_t)bpm;
  if (s_chart.source[index] != 1 || s_chart.sample_count[index] == 0) {
    s_chart.bpm[index] = value;
    s_chart.sample_count[index] = 1;
    s_chart.source[index] = 1;
    return;
  }
  const uint8_t count = s_chart.sample_count[index];
  const uint16_t next = (uint16_t)(((uint32_t)s_chart.bpm[index] * count + value) /
                                   ((uint32_t)count + 1));
  s_chart.bpm[index] = (uint8_t)next;
  if (count < UINT8_MAX) {
    s_chart.sample_count[index] = (uint8_t)(count + 1);
  }
}

static void chart_backfill_health(int32_t at_time) {
#if defined(PBL_HEALTH)
  if (!s_session.active) {
    return;
  }
  chart_advance(at_time);
  HealthMinuteData history[CHART_MINUTES];
  const int32_t earliest_chart_minute = s_chart.last_minute - (CHART_MINUTES - 1);
  int32_t first_full_session_minute = (s_session.start_time + 59) / 60;
  if (first_full_session_minute < earliest_chart_minute) {
    first_full_session_minute = earliest_chart_minute;
  }
  time_t start = (time_t)first_full_session_minute * 60;
  time_t end = (time_t)(s_chart.last_minute + 1) * 60;
  if (start >= end) {
    return;
  }
  const uint32_t count = health_service_get_minute_history(
      history, CHART_MINUTES, &start, &end);
  if (count == 0) {
    return;
  }
  const int32_t returned_first_minute = (int32_t)(start / 60);
  for (uint32_t i = 0; i < count; ++i) {
    const int32_t minute = returned_first_minute + (int32_t)i;
    const int logical_index = minute - earliest_chart_minute;
    if (logical_index < 0 || logical_index >= CHART_MINUTES ||
        history[i].is_invalid || history[i].heart_rate_bpm == 0) {
      continue;
    }
    const uint8_t index = chart_physical_index((uint8_t)logical_index);
    if (s_chart.source[index] == 1) {
      continue;
    }
    s_chart.bpm[index] = history[i].heart_rate_bpm;
    s_chart.sample_count[index] = 1;
    s_chart.source[index] = 2;
  }
#else
  (void)at_time;
#endif
}

static void cancel_rest_wakeup(void) {
  if (s_rest.wakeup_id >= 0) {
    wakeup_cancel((WakeupId)s_rest.wakeup_id);
  }
  s_rest.wakeup_id = WAKEUP_INVALID_ID;
}

static int32_t rest_cookie(void) {
  return WAKEUP_COOKIE_MAGIC | (int32_t)s_rest.generation;
}

static WorkoutRestLifecycle rest_lifecycle_from_state(void) {
  WorkoutRestLifecycle lifecycle = {
      .active = s_rest.active,
      .alert_delivered = s_rest.alerted,
      .deadline = s_rest.deadline,
      .generation = s_rest.generation,
  };
  return lifecycle;
}

static void apply_rest_lifecycle(const WorkoutRestLifecycle *lifecycle) {
  s_rest.active = lifecycle->active;
  s_rest.alerted = lifecycle->alert_delivered;
  s_rest.deadline = (int32_t)lifecycle->deadline;
  s_rest.generation = lifecycle->generation;
}

static void record_rest_generation(uint16_t generation) {
  s_rest_generation_seed = generation;
  persist_write_int(PERSIST_KEY_REST_GENERATION, s_rest_generation_seed);
}

static bool rest_cookie_matches(int32_t cookie) {
  if ((cookie & WAKEUP_COOKIE_MASK) != WAKEUP_COOKIE_MAGIC) {
    return false;
  }
  const WorkoutRestLifecycle lifecycle = rest_lifecycle_from_state();
  return workout_rest_accepts_cookie(&lifecycle,
                                     (uint16_t)(cookie & 0xFFFF));
}

static void schedule_rest_wakeup(void) {
  cancel_rest_wakeup();
  if (!s_rest.active || s_rest.deadline <= now_seconds()) {
    return;
  }
  const WakeupId id = wakeup_schedule((time_t)s_rest.deadline, rest_cookie(), false);
  if (id < 0) {
    s_rest.signal_available = false;
    s_rest.wakeup_id = WAKEUP_INVALID_ID;
    APP_LOG(APP_LOG_LEVEL_WARNING, "rest wakeup failed: %ld", (long)id);
  } else {
    s_rest.signal_available = true;
    s_rest.wakeup_id = id;
  }
  save_rest();
}

static void cancel_rest(void) {
  WorkoutRestLifecycle lifecycle = rest_lifecycle_from_state();
  (void)workout_rest_cancel(&lifecycle);
  cancel_rest_wakeup();
  reset_rest_state();
  save_rest();
}

static void rest_alert_if_due(int32_t at_time) {
  if (!s_session.active) {
    return;
  }
  WorkoutRestLifecycle lifecycle = rest_lifecycle_from_state();
  if (!workout_rest_claim_alert(&lifecycle, at_time,
                                lifecycle.generation)) {
    return;
  }
  apply_rest_lifecycle(&lifecycle);
  cancel_rest_wakeup();
  if (s_settings.vibrations_enabled) {
    vibes_short_pulse();
  }
  save_rest();
  mark_canvas_dirty();
}

static void start_rest(void) {
  if (!s_session.active) {
    return;
  }
  const int32_t now = now_seconds();
  cancel_rest_wakeup();
  reset_rest_state();
  WorkoutRestLifecycle lifecycle;
  workout_rest_lifecycle_init(&lifecycle);
  lifecycle.generation = s_rest_generation_seed;
  const uint16_t generation = workout_rest_start(
      &lifecycle, now, s_settings.rest_preset_seconds);
  if (generation == 0) {
    return;
  }
  apply_rest_lifecycle(&lifecycle);
  record_rest_generation(generation);
  s_rest.started_at = now;
  schedule_rest_wakeup();
  mark_canvas_dirty();
}

static void extend_rest(void) {
  if (!s_rest.active) {
    start_rest();
    return;
  }
  const int32_t now = now_seconds();
  WorkoutRestLifecycle lifecycle = rest_lifecycle_from_state();
  const uint16_t generation = workout_rest_extend(
      &lifecycle, now, WORKOUT_REST_EXTENSION_SECONDS);
  if (generation == 0) {
    return;
  }
  apply_rest_lifecycle(&lifecycle);
  record_rest_generation(generation);
  schedule_rest_wakeup();
  rest_alert_if_due(now);
  mark_canvas_dirty();
}

static void sync_hrv_request_from_lifecycle(void) {
#if defined(PBL_HEALTH)
  if (s_hrv_request_active && !s_hrv_lifecycle.request_active) {
    health_service_set_hrv_sample_period(0);
  }
#endif
  s_hrv_request_active = s_hrv_lifecycle.request_active;
}

static void stop_hrv_request(void) {
#if defined(PBL_HEALTH)
  if (s_hrv_request_active || s_hrv_lifecycle.request_active) {
    health_service_set_hrv_sample_period(0);
  }
#endif
  s_hrv_lifecycle.request_active = false;
  s_hrv_request_active = false;
}

static void reset_runtime_hr(void) {
  s_current_hr = 0;
  s_hr_fresh = false;
  s_stable_zone = 0;
  s_zone_streak_started = 0;
  s_zone_recovering = true;
  fitness_bpm_filter_reset(&s_bpm_filter);
  fitness_zone_stabilizer_reset(&s_zone_stabilizer);
}

static void set_hr_sampling(bool active) {
#if defined(PBL_HEALTH)
  health_service_set_heart_rate_sample_period(active ? HR_SAMPLE_PERIOD_SECONDS : 0);
#else
  (void)active;
#endif
}

static void finish_session(int32_t end_time, bool auto_finished) {
  if (!s_session.active) {
    return;
  }
  if (end_time < s_session.start_time) {
    end_time = s_session.start_time;
  }
  session_account_to(end_time);

  reset_summary();
  s_summary.valid = true;
  s_summary.auto_finished = auto_finished;
  s_summary.start_time = s_session.start_time;
  s_summary.end_time = end_time;
  s_summary.duration_seconds = seconds_between(end_time, s_session.start_time);
  memcpy(s_summary.zone_seconds, s_session.zone_seconds,
         sizeof(s_summary.zone_seconds));
  s_summary.no_data_seconds = s_session.no_data_seconds;
  s_summary.max_hr = s_session.max_hr;
  save_summary();

  cancel_rest();
  reset_session();
  reset_chart();
  save_session();
  save_chart();
  set_hr_sampling(false);
  reset_runtime_hr();
  s_screen = SCREEN_MAIN;
  s_confirm_deadline = 0;
  mark_canvas_dirty();
  mark_dots_dirty();
}

static void start_session(void) {
  const int32_t now = now_seconds();
  cancel_rest();
  reset_session();
  reset_chart();
  s_session.active = true;
  s_session.start_time = now;
  s_session.accounting_since = now;
  s_session.accounting_state = ACCOUNT_NO_DATA;
  s_screen = SCREEN_MAIN;
  reset_runtime_hr();
  chart_advance(now);
  save_session();
  save_chart();
  set_hr_sampling(true);
  mark_canvas_dirty();
  mark_dots_dirty();
}

static void show_toast(const char *message) {
  snprintf(s_toast, sizeof(s_toast), "%s", message ? message : "");
  s_toast_until = now_seconds() + 3;
  mark_canvas_dirty();
}

static void maybe_vibrate_target_transition(uint8_t previous, uint8_t next,
                                            bool recovering) {
  if (!s_settings.vibrations_enabled || !s_settings.zone_vibes ||
      s_settings.target_zone == 0 || s_rest.active || recovering) {
    return;
  }
  if (previous != s_settings.target_zone && next == s_settings.target_zone) {
    vibes_short_pulse();
  } else if (previous == s_settings.target_zone && next != s_settings.target_zone) {
    vibes_double_pulse();
  }
}

static void process_heart_rate(uint16_t raw_bpm, int32_t at_time) {
  if (!s_session.active || raw_bpm == 0) {
    return;
  }

  session_account_to(at_time);
  const bool was_stale = !s_hr_fresh || s_session.last_hr_at == 0 ||
                         at_time - s_session.last_hr_at >= HR_STALE_SECONDS;
  if (was_stale) {
    fitness_bpm_filter_reset(&s_bpm_filter);
    fitness_zone_stabilizer_reset(&s_zone_stabilizer);
    s_stable_zone = 0;
    s_zone_streak_started = 0;
    s_zone_recovering = true;
    transition_account_state(ACCOUNT_NO_DATA, at_time);
  }

  s_session.last_hr_at = at_time;
  const uint16_t filtered_bpm =
      fitness_bpm_filter_update(&s_bpm_filter, raw_bpm);
  if (filtered_bpm == 0) {
    return;
  }
  s_current_hr = filtered_bpm;
  s_hr_fresh = true;
  if (raw_bpm > s_session.max_hr) {
    s_session.max_hr = raw_bpm;
  }
  chart_add_local(filtered_bpm, at_time);

  if (!s_settings.configured) {
    transition_account_state(ACCOUNT_BELOW_ZONE, at_time);
    mark_canvas_dirty();
    return;
  }

  const uint8_t previous = s_stable_zone;
  const uint8_t next = fitness_zone_stabilizer_update(
      &s_zone_stabilizer, &s_zone_bounds, filtered_bpm);
  const bool pending_initial_zone = next == 0 &&
                                    s_zone_stabilizer.candidate_readings > 0;
  if (pending_initial_zone) {
    mark_canvas_dirty();
    return;
  }

  if (next != previous || s_session.accounting_state == ACCOUNT_NO_DATA) {
    const bool recovering = s_zone_recovering;
    transition_account_state(account_state_for_zone(next), at_time);
    s_stable_zone = next;
    s_zone_streak_started = next > 0 ? at_time : 0;
    maybe_vibrate_target_transition(previous, next, recovering);
    s_zone_recovering = false;
  }
  mark_canvas_dirty();
}

static void check_hr_stale(int32_t at_time) {
  if (!s_session.active) {
    return;
  }
  session_account_to(at_time);
  if (!s_hr_fresh || s_session.last_hr_at == 0 ||
      at_time - s_session.last_hr_at < HR_STALE_SECONDS) {
    return;
  }
  reset_runtime_hr();
  transition_account_state(ACCOUNT_NO_DATA, at_time);
  mark_canvas_dirty();
}

static uint32_t local_date_key(int32_t timestamp) {
  time_t value = timestamp;
  struct tm *local = localtime(&value);
  if (!local) {
    return 0;
  }
  return (uint32_t)(local->tm_year + 1900) * 512U + (uint32_t)local->tm_yday;
}

static bool hrv_baseline(uint32_t *out_baseline) {
  if (!out_baseline || s_hrv_store.baseline_count < 3) {
    return false;
  }
  uint32_t values[FITNESS_HRV_BASELINE_MAX];
  for (uint8_t i = 0; i < s_hrv_store.baseline_count; ++i) {
    values[i] = s_hrv_store.baseline[i].rmssd_ms;
  }
  return fitness_median_u32(values, s_hrv_store.baseline_count, out_baseline);
}

static void store_hrv_result(const FitnessHrvResult *result, int32_t measured_at) {
  if (!result || !result->valid) {
    return;
  }
  HrvResultV1 entry;
  memset(&entry, 0, sizeof(entry));
  entry.date_key = local_date_key(measured_at);
  entry.measured_at = measured_at;
  entry.rmssd_ms = clamp_u16(result->rmssd_ms);
  entry.mean_ppi_ms = result->valid_intervals
                          ? clamp_u16(result->accepted_coverage_ms /
                                      result->valid_intervals)
                          : 0;
  entry.duration_seconds = WORKOUT_HRV_CAPTURE_SECONDS;
  entry.valid_intervals = clamp_u16(result->valid_intervals);
  entry.rejected_intervals = clamp_u16(result->rejected_intervals);

  bool have_today = false;
  for (uint8_t i = 0; i < s_hrv_store.baseline_count; ++i) {
    if (s_hrv_store.baseline[i].date_key == entry.date_key) {
      have_today = true;
      break;
    }
  }
  if (!have_today) {
    if (s_hrv_store.baseline_count < FITNESS_HRV_BASELINE_MAX) {
      s_hrv_store.baseline[s_hrv_store.baseline_count++] = entry;
    } else {
      memmove(&s_hrv_store.baseline[0], &s_hrv_store.baseline[1],
              sizeof(s_hrv_store.baseline[0]) * (FITNESS_HRV_BASELINE_MAX - 1));
      s_hrv_store.baseline[FITNESS_HRV_BASELINE_MAX - 1] = entry;
    }
  }
  s_hrv_store.latest = entry;
  s_hrv_store.latest_valid = true;
  save_hrv_store();
}

static void finish_hrv_measurement(void) {
  stop_hrv_request();
  const FitnessHrvResult result = fitness_hrv_calculate_rmssd(
      s_hrv_samples, s_hrv_sample_count);
  (void)workout_hrv_finish(&s_hrv_lifecycle, result.valid);
  if (result.valid) {
    store_hrv_result(&result, now_seconds());
    s_hrv_view_state = HRV_VIEW_IDLE;
  } else {
    s_hrv_view_state = HRV_VIEW_LOW_SIGNAL;
  }
  s_screen = SCREEN_HRV;
  mark_canvas_dirty();
}

static void cancel_hrv_measurement(void) {
  (void)workout_hrv_cancel(&s_hrv_lifecycle);
  sync_hrv_request_from_lifecycle();
  stop_hrv_request();
  s_hrv_sample_count = 0;
  s_hrv_first_valid_at = 0;
  s_hrv_last_event_at = 0;
  s_hrv_view_state = HRV_VIEW_IDLE;
  s_screen = SCREEN_HRV;
  mark_canvas_dirty();
}

static void update_health_accessibility(void) {
#if defined(PBL_HEALTH)
  const time_t now = time(NULL);
  const HealthServiceAccessibilityMask mask =
      health_service_metric_accessible(HealthMetricHeartRateBPM, now, now);
  s_health_no_permission =
      (mask & HealthServiceAccessibilityMaskNoPermission) != 0;
  s_health_not_supported =
      (mask & HealthServiceAccessibilityMaskNotSupported) != 0;
#else
  s_health_no_permission = false;
  s_health_not_supported = true;
#endif
}

static void start_hrv_measurement(void) {
  if (s_session.active || s_screen != SCREEN_HRV) {
    return;
  }
  update_health_accessibility();
  if (s_health_no_permission) {
    s_hrv_view_state = HRV_VIEW_HEALTH_OFF;
    mark_canvas_dirty();
    return;
  }
  if (s_health_not_supported || !s_health_subscribed) {
    s_hrv_view_state = HRV_VIEW_UNSUPPORTED;
    mark_canvas_dirty();
    return;
  }
#if defined(PBL_HEALTH)
  if (!health_service_set_hrv_sample_period(HRV_SAMPLE_PERIOD_SECONDS)) {
    s_hrv_view_state = HRV_VIEW_UNSUPPORTED;
    mark_canvas_dirty();
    return;
  }
#else
  s_hrv_view_state = HRV_VIEW_UNSUPPORTED;
  mark_canvas_dirty();
  return;
#endif
  memset(s_hrv_samples, 0, sizeof(s_hrv_samples));
  s_hrv_sample_count = 0;
  const int32_t started_at = now_seconds();
  workout_hrv_lifecycle_init(&s_hrv_lifecycle);
  if (!workout_hrv_start(&s_hrv_lifecycle, started_at)) {
#if defined(PBL_HEALTH)
    health_service_set_hrv_sample_period(0);
#endif
    s_hrv_view_state = HRV_VIEW_UNSUPPORTED;
    mark_canvas_dirty();
    return;
  }
  s_hrv_first_valid_at = 0;
  s_hrv_last_event_at = 0;
  sync_hrv_request_from_lifecycle();
  s_hrv_view_state = HRV_VIEW_IDLE;
  s_screen = SCREEN_HRV_MEASURING;
  mark_canvas_dirty();
}

static void process_hrv_ppi(uint16_t ppi, int32_t at_time) {
  if (!s_hrv_request_active || s_screen != SCREEN_HRV_MEASURING) {
    return;
  }

  const bool ppi_is_usable = ppi >= 300 && ppi <= 2000;
  WorkoutHrvTransition transition;
  if (s_hrv_lifecycle.phase == WORKOUT_HRV_WAITING_FOR_PPI &&
      ppi_is_usable) {
    transition = workout_hrv_on_usable_ppi(&s_hrv_lifecycle, at_time);
  } else {
    transition = workout_hrv_poll(&s_hrv_lifecycle, at_time);
  }
  sync_hrv_request_from_lifecycle();
  if (transition == WORKOUT_HRV_TRANSITION_WAIT_TIMEOUT) {
    s_hrv_view_state = HRV_VIEW_NO_SIGNAL;
    s_screen = SCREEN_HRV;
    mark_canvas_dirty();
    return;
  }
  if (transition == WORKOUT_HRV_TRANSITION_CAPTURE_READY) {
    finish_hrv_measurement();
    return;
  }
  if (transition == WORKOUT_HRV_TRANSITION_FIRST_PPI) {
    s_hrv_first_valid_at = (int32_t)s_hrv_lifecycle.first_ppi_at;
    s_hrv_sample_count = 0;
  }
  if (s_hrv_lifecycle.phase != WORKOUT_HRV_CAPTURING) {
    return;
  }
  if (s_hrv_last_event_at > 0 && at_time - s_hrv_last_event_at > 10 &&
      s_hrv_sample_count < HRV_MAX_SAMPLES) {
    s_hrv_samples[s_hrv_sample_count++] = 0;
  }
  if (s_hrv_sample_count < HRV_MAX_SAMPLES) {
    s_hrv_samples[s_hrv_sample_count++] = ppi;
  }
  s_hrv_last_event_at = at_time;
  mark_canvas_dirty();
}

static void health_handler(HealthEventType event, void *context) {
  (void)context;
#if defined(PBL_HEALTH)
  const int32_t now = now_seconds();
  if (event == HealthEventHeartRateUpdate && s_session.active) {
    HealthValue value =
        health_service_peek_current_value(HealthMetricHeartRateRawBPM);
    if (value <= 0 || value > UINT16_MAX) {
      value = health_service_peek_current_value(HealthMetricHeartRateBPM);
    }
    if (value > 0 && value <= UINT16_MAX) {
      process_heart_rate((uint16_t)value, now);
    }
  } else if (event == HealthEventHRVUpdate) {
    process_hrv_ppi(health_service_peek_hrv_ppi_ms(), now);
  } else if (event == HealthEventSignificantUpdate) {
    update_health_accessibility();
    if (s_session.active) {
      chart_backfill_health(now);
    }
    mark_canvas_dirty();
  }
#else
  (void)event;
#endif
}

static void fill_rect(GContext *ctx, GRect rect, GColor color) {
  if (s_canvas_coordinates) {
    rect.origin.y -= 28;
  }
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_rect(ctx, rect, 0, GCornerNone);
}

static void draw_text(GContext *ctx, const char *text, GRect rect,
                      const char *font_key, GColor color,
                      GTextAlignment alignment) {
  if (s_canvas_coordinates) {
    rect.origin.y -= 28;
  }
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, fonts_get_system_font(font_key), rect,
                     GTextOverflowModeTrailingEllipsis, alignment, NULL);
}

static void draw_line(GContext *ctx, GPoint from, GPoint to) {
  if (s_canvas_coordinates) {
    from.y -= 28;
    to.y -= 28;
  }
  graphics_draw_line(ctx, from, to);
}

static void format_duration(uint32_t seconds, char *buffer, size_t size) {
  if (!fitness_format_duration(seconds, buffer, size)) {
    snprintf(buffer, size, "--");
  }
}

static uint32_t active_elapsed(int32_t now) {
  return s_session.active ? seconds_between(now, s_session.start_time) : 0;
}

static void draw_time_band(GContext *ctx, const char *top, const char *bottom,
                           GColor bottom_color) {
  fill_rect(ctx, GRect(0, 28, 200, 37), COLOR_STEEL);
  const time_t now = time(NULL);
  struct tm *local = localtime(&now);
  char clock[8] = "--:--";
  if (local) {
    strftime(clock, sizeof(clock), clock_is_24h_style() ? "%H:%M" : "%I:%M",
             local);
  }
  draw_text(ctx, clock, GRect(8, 25, 126, 43),
            FONT_KEY_BITHAM_34_MEDIUM_NUMBERS, COLOR_WHITE, GTextAlignmentLeft);
  draw_text(ctx, top, GRect(132, 30, 59, 16), FONT_KEY_GOTHIC_14_BOLD,
            COLOR_WHITE, GTextAlignmentRight);
  draw_text(ctx, bottom, GRect(126, 43, 65, 22), FONT_KEY_GOTHIC_18_BOLD,
            bottom_color, GTextAlignmentRight);
}

static void draw_footer(GContext *ctx, const char *middle_top,
                        const char *middle_bottom, const char *right_top,
                        const char *right_bottom, int32_t now) {
  graphics_context_set_stroke_color(ctx, COLOR_STEEL);
  draw_line(ctx, GPoint(8, 199), GPoint(192, 199));
  draw_text(ctx, "TIME", GRect(8, 201, 55, 15), FONT_KEY_GOTHIC_14,
            COLOR_MUTED, GTextAlignmentLeft);
  char elapsed[16] = "--";
  if (s_session.active) {
    format_duration(active_elapsed(now), elapsed, sizeof(elapsed));
  } else if (s_summary.valid) {
    format_duration(s_summary.duration_seconds, elapsed, sizeof(elapsed));
  }
  draw_text(ctx, elapsed, GRect(8, 213, 70, 17), FONT_KEY_GOTHIC_14_BOLD,
            COLOR_WHITE, GTextAlignmentLeft);
  draw_text(ctx, middle_top, GRect(71, 201, 58, 15), FONT_KEY_GOTHIC_14,
            COLOR_MUTED, GTextAlignmentCenter);
  draw_text(ctx, middle_bottom, GRect(67, 213, 66, 17),
            FONT_KEY_GOTHIC_14_BOLD, COLOR_WHITE, GTextAlignmentCenter);
  draw_text(ctx, right_top, GRect(132, 201, 60, 15), FONT_KEY_GOTHIC_14,
            COLOR_MUTED, GTextAlignmentRight);
  draw_text(ctx, right_bottom, GRect(128, 213, 64, 17),
            FONT_KEY_GOTHIC_14_BOLD, COLOR_WHITE, GTextAlignmentRight);
}

static int16_t chart_y(uint16_t bpm) {
  int value = bpm;
  if (value < 60) {
    value = 60;
  } else if (value > 200) {
    value = 200;
  }
  return (int16_t)(187 - ((value - 60) * 69) / 140);
}

static void draw_zone_column(GContext *ctx, uint8_t active_zone) {
  for (uint8_t row = 0; row < FITNESS_ZONE_COUNT; ++row) {
    const uint8_t zone = (uint8_t)(FITNESS_ZONE_COUNT - row);
    const int16_t y = (int16_t)(118 + row * 14);
    const bool active = zone == active_zone;
    fill_rect(ctx, GRect(170, y, 20, 14),
              active ? COLOR_ZONES[zone - 1] : COLOR_STEEL);
    char label[2] = {(char)('0' + zone), '\0'};
    draw_text(ctx, label, GRect(170, y - 1, 20, 16), FONT_KEY_GOTHIC_14_BOLD,
              active ? COLOR_BLACK : COLOR_WHITE, GTextAlignmentCenter);
  }
}

static void draw_history(GContext *ctx, uint8_t active_zone) {
  const int16_t left = 28;
  const int16_t right = 166;
  draw_text(ctx, "200", GRect(2, 115, 25, 14), FONT_KEY_GOTHIC_14,
            COLOR_MUTED, GTextAlignmentLeft);
  draw_text(ctx, "60", GRect(3, 176, 23, 14), FONT_KEY_GOTHIC_14,
            COLOR_MUTED, GTextAlignmentLeft);
  graphics_context_set_stroke_color(ctx, COLOR_STEEL);
  for (uint16_t bpm = 80; bpm <= 200; bpm += 40) {
    const int16_t y = chart_y(bpm);
    draw_line(ctx, GPoint(left, y), GPoint(right, y));
  }
  bool have_previous = false;
  GPoint previous = GPointZero;
  for (int i = 0; i < CHART_MINUTES; ++i) {
    const uint8_t index = chart_physical_index((uint8_t)i);
    if (s_chart.bpm[index] == 0) {
      have_previous = false;
      continue;
    }
    const int16_t x = (int16_t)(left + (i * (right - left)) /
                                (CHART_MINUTES - 1));
    const GPoint point = GPoint(x, chart_y(s_chart.bpm[index]));
    const uint8_t point_zone =
        s_settings.configured
            ? fitness_classify_zone(&s_zone_bounds, s_chart.bpm[index])
            : 0;
    const GColor point_color =
        point_zone > 0 ? COLOR_ZONES[point_zone - 1] : COLOR_MUTED;
    if (have_previous) {
      graphics_context_set_stroke_color(ctx, point_color);
      graphics_context_set_stroke_width(ctx, 2);
      draw_line(ctx, previous, point);
      graphics_context_set_stroke_width(ctx, 1);
    }
    fill_rect(ctx, GRect(point.x - 1, point.y - 1, 3, 3), point_color);
    previous = point;
    have_previous = true;
  }
  draw_text(ctx, "-60m", GRect(left, 184, 45, 14), FONT_KEY_GOTHIC_14,
            COLOR_MUTED, GTextAlignmentLeft);
  draw_text(ctx, "NOW", GRect(124, 184, right - 124, 14),
            FONT_KEY_GOTHIC_14, COLOR_WHITE, GTextAlignmentRight);
  draw_zone_column(ctx, active_zone);
}

static void draw_main_screen(GContext *ctx, int32_t now) {
  char target[8];
  if (s_settings.configured && s_settings.target_zone > 0) {
    snprintf(target, sizeof(target), "Z%u", s_settings.target_zone);
  } else {
    snprintf(target, sizeof(target), "OFF");
  }
  draw_time_band(ctx, "GOAL", target, COLOR_GOAL);

  const uint8_t active_zone = s_hr_fresh ? s_stable_zone : 0;
  const GColor band = active_zone > 0 ? COLOR_ZONES[active_zone - 1] : COLOR_STEEL;
  const GColor ink = active_zone > 0 ? COLOR_BLACK : COLOR_WHITE;
  fill_rect(ctx, GRect(0, 69, 200, 42), band);
  char bpm[8] = "--";
  if (s_hr_fresh) {
    snprintf(bpm, sizeof(bpm), "%u", s_current_hr);
  }
  draw_text(ctx, bpm, GRect(8, 64, 130, 50), FONT_KEY_BITHAM_42_BOLD,
            ink, GTextAlignmentLeft);
  char zone[8] = "--";
  if (s_hr_fresh && active_zone > 0) {
    snprintf(zone, sizeof(zone), "Z%u", active_zone);
  } else if (s_hr_fresh && s_settings.configured &&
             s_session.accounting_state == ACCOUNT_BELOW_ZONE) {
    snprintf(zone, sizeof(zone), "REC");
  }
  draw_text(ctx, zone, GRect(137, 71, 54, 24), FONT_KEY_GOTHIC_18_BOLD,
            ink, GTextAlignmentRight);
  draw_text(ctx, "BPM", GRect(145, 92, 46, 16), FONT_KEY_GOTHIC_14_BOLD,
            ink, GTextAlignmentRight);

  draw_history(ctx, active_zone);
  char max_hr[8] = "--";
  if (s_session.max_hr > 0) {
    snprintf(max_hr, sizeof(max_hr), "%u", s_session.max_hr);
  }
  char streak[16] = "--";
  if (active_zone > 0 && s_zone_streak_started > 0) {
    format_duration(seconds_between(now, s_zone_streak_started), streak,
                    sizeof(streak));
  }
  draw_footer(ctx, "MAX", max_hr, "ZONE", streak, now);
}

static void draw_rest_screen(GContext *ctx, int32_t now) {
  char target[16];
  format_duration(seconds_between(s_rest.deadline, s_rest.started_at), target,
                  sizeof(target));
  draw_time_band(ctx, "REST", target, COLOR_WHITE);
  fill_rect(ctx, GRect(0, 69, 200, 49), COLOR_STEEL);
  char timer[18];
  const bool overtime = now >= s_rest.deadline;
  if (overtime) {
    char over[16];
    format_duration(seconds_between(now, s_rest.deadline), over, sizeof(over));
    snprintf(timer, sizeof(timer), "+%s", over);
  } else {
    format_duration(seconds_between(s_rest.deadline, now), timer, sizeof(timer));
  }
  draw_text(ctx, timer, GRect(8, 66, 150, 54), FONT_KEY_BITHAM_42_BOLD,
            COLOR_WHITE, GTextAlignmentLeft);
  draw_text(ctx, overtime ? "OVER" : "LEFT", GRect(145, 71, 46, 17),
            FONT_KEY_GOTHIC_14_BOLD, COLOR_WHITE, GTextAlignmentRight);

  fill_rect(ctx, GRect(10, 126, 180, 5), COLOR_DARK_STEEL);
  const uint32_t total = seconds_between(s_rest.deadline, s_rest.started_at);
  const uint32_t used = seconds_between(now, s_rest.started_at);
  const int progress = total ? (int)((used > total ? total : used) * 180U / total) : 180;
  fill_rect(ctx, GRect(10, 126, progress, 5), COLOR_WHITE);
  draw_text(ctx, overtime ? "TIMER ENDED" : "REST TIMER", GRect(10, 134, 180, 18),
            FONT_KEY_GOTHIC_14_BOLD, COLOR_WHITE, GTextAlignmentLeft);
  char bpm[8] = "--";
  if (s_hr_fresh) {
    snprintf(bpm, sizeof(bpm), "%u", s_current_hr);
  }
  draw_text(ctx, bpm, GRect(10, 150, 62, 35), FONT_KEY_BITHAM_30_BLACK,
            COLOR_WHITE, GTextAlignmentLeft);
  draw_text(ctx, "BPM", GRect(70, 162, 45, 17), FONT_KEY_GOTHIC_14,
            COLOR_MUTED, GTextAlignmentLeft);
  char zone[16] = "NO ZONE";
  if (s_hr_fresh && s_stable_zone > 0) {
    snprintf(zone, sizeof(zone), "ZONE %u", s_stable_zone);
  }
  draw_text(ctx, zone, GRect(115, 162, 76, 17), FONT_KEY_GOTHIC_14,
            COLOR_MUTED, GTextAlignmentRight);
  draw_text(ctx, s_rest.signal_available ? "BACK: NEXT SET" : "NO BG SIGNAL",
            GRect(10, 181, 180, 16), FONT_KEY_GOTHIC_14, COLOR_MUTED,
            GTextAlignmentLeft);
  char max_hr[8] = "--";
  if (s_session.max_hr > 0) {
    snprintf(max_hr, sizeof(max_hr), "%u", s_session.max_hr);
  }
  draw_footer(ctx, "MAX", max_hr, "SESSION", "ON", now);
}

static void draw_idle_screen(GContext *ctx, int32_t now) {
  draw_time_band(ctx, "GYM", "ZONES", COLOR_GOAL);
  fill_rect(ctx, GRect(0, 69, 200, 48), COLOR_STEEL);
  draw_text(ctx, "START", GRect(0, 67, 200, 52), FONT_KEY_BITHAM_42_BOLD,
            COLOR_WHITE, GTextAlignmentCenter);
  draw_text(ctx, "SELECT TO BEGIN", GRect(10, 130, 180, 20),
            FONT_KEY_GOTHIC_18_BOLD, COLOR_WHITE, GTextAlignmentCenter);
  draw_text(ctx, "UP: HRV", GRect(10, 155, 180, 17), FONT_KEY_GOTHIC_14,
            COLOR_MUTED, GTextAlignmentCenter);
  draw_text(ctx, "DOWN: START FIRST", GRect(10, 177, 180, 17),
            FONT_KEY_GOTHIC_14, COLOR_MUTED, GTextAlignmentCenter);
  draw_footer(ctx, "--", "--", "SESSION", "IDLE", now);
}

static void draw_summary_screen(GContext *ctx, int32_t now) {
  draw_time_band(ctx, s_summary.auto_finished ? "AUTO" : "LAST",
                 s_summary.auto_finished ? "FINISH" : "SAVED",
                 s_summary.auto_finished ? COLOR_GOAL : COLOR_WHITE);
  fill_rect(ctx, GRect(0, 69, 200, 43), COLOR_STEEL);
  char duration[16];
  format_duration(s_summary.duration_seconds, duration, sizeof(duration));
  draw_text(ctx, duration, GRect(0, 67, 200, 48), FONT_KEY_BITHAM_34_MEDIUM_NUMBERS,
            COLOR_WHITE, GTextAlignmentCenter);
  draw_text(ctx, "WORKOUT TIME", GRect(9, 119, 95, 16), FONT_KEY_GOTHIC_14,
            COLOR_MUTED, GTextAlignmentLeft);
  char max_label[20];
  if (s_summary.max_hr > 0) {
    snprintf(max_label, sizeof(max_label), "MAX %u BPM", s_summary.max_hr);
  } else {
    snprintf(max_label, sizeof(max_label), "MAX -- BPM");
  }
  draw_text(ctx, max_label, GRect(100, 119, 91, 16), FONT_KEY_GOTHIC_14,
            COLOR_WHITE, GTextAlignmentRight);
  fill_rect(ctx, GRect(9, 145, 182, 6), COLOR_DARK_STEEL);
  int16_t bar_x = 9;
  for (uint8_t i = 0; i < FITNESS_ZONE_COUNT; ++i) {
    const int16_t width = s_summary.duration_seconds
                              ? (int16_t)((uint64_t)s_summary.zone_seconds[i] * 182U /
                                          s_summary.duration_seconds)
                              : 0;
    if (width > 0 && bar_x < 191) {
      const int16_t clipped = bar_x + width > 191 ? 191 - bar_x : width;
      fill_rect(ctx, GRect(bar_x, 145, clipped, 6), COLOR_ZONES[i]);
      bar_x += clipped;
    }
  }
  char z[5][18];
  for (uint8_t i = 0; i < FITNESS_ZONE_COUNT; ++i) {
    char value[12];
    format_duration(s_summary.zone_seconds[i], value, sizeof(value));
    snprintf(z[i], sizeof(z[i]), "Z%u %s", i + 1, value);
  }
  draw_text(ctx, z[0], GRect(9, 156, 61, 15), FONT_KEY_GOTHIC_14,
            COLOR_WHITE, GTextAlignmentLeft);
  draw_text(ctx, z[1], GRect(70, 156, 61, 15), FONT_KEY_GOTHIC_14,
            COLOR_WHITE, GTextAlignmentCenter);
  draw_text(ctx, z[2], GRect(131, 156, 60, 15), FONT_KEY_GOTHIC_14,
            COLOR_WHITE, GTextAlignmentRight);
  draw_text(ctx, z[3], GRect(9, 169, 61, 15), FONT_KEY_GOTHIC_14,
            COLOR_WHITE, GTextAlignmentLeft);
  draw_text(ctx, z[4], GRect(70, 169, 61, 15), FONT_KEY_GOTHIC_14,
            COLOR_WHITE, GTextAlignmentCenter);
  char no_data_value[12];
  format_duration(s_summary.no_data_seconds, no_data_value,
                  sizeof(no_data_value));
  draw_text(ctx, "NO DATA", GRect(131, 169, 60, 15), FONT_KEY_GOTHIC_14,
            COLOR_MUTED, GTextAlignmentRight);
  draw_text(ctx, no_data_value, GRect(131, 184, 60, 15), FONT_KEY_GOTHIC_14,
            COLOR_MUTED, GTextAlignmentRight);
  char max_hr[8] = "--";
  if (s_summary.max_hr > 0) {
    snprintf(max_hr, sizeof(max_hr), "%u", s_summary.max_hr);
  }
  draw_footer(ctx, "MAX", max_hr, "SESSION",
              s_summary.auto_finished ? "AUTO" : "STOP", now);
}

static void draw_hrv_error(GContext *ctx, const char *title,
                           const char *line_1, const char *line_2) {
  fill_rect(ctx, GRect(0, 69, 200, 47), COLOR_STEEL);
  draw_text(ctx, "--", GRect(8, 65, 80, 53), FONT_KEY_BITHAM_42_BOLD,
            COLOR_WHITE, GTextAlignmentLeft);
  draw_text(ctx, "RMSSD", GRect(120, 73, 70, 17), FONT_KEY_GOTHIC_14_BOLD,
            COLOR_WHITE, GTextAlignmentRight);
  draw_text(ctx, "ms", GRect(145, 91, 45, 20), FONT_KEY_GOTHIC_18_BOLD,
            COLOR_WHITE, GTextAlignmentRight);
  draw_text(ctx, title, GRect(9, 121, 182, 20), FONT_KEY_GOTHIC_18_BOLD,
            COLOR_WHITE, GTextAlignmentLeft);
  draw_text(ctx, line_1, GRect(9, 143, 182, 15), FONT_KEY_GOTHIC_14,
            COLOR_MUTED, GTextAlignmentLeft);
  draw_text(ctx, line_2, GRect(9, 158, 182, 15), FONT_KEY_GOTHIC_14,
            COLOR_MUTED, GTextAlignmentLeft);
  draw_text(ctx, "PPG ESTIMATE", GRect(9, 173, 182, 15),
            FONT_KEY_GOTHIC_14_BOLD, COLOR_WHITE, GTextAlignmentLeft);
  draw_text(ctx, "NOT A READINESS SCORE", GRect(9, 185, 182, 14),
            FONT_KEY_GOTHIC_14, COLOR_MUTED, GTextAlignmentLeft);
}

static void draw_hrv_screen(GContext *ctx, int32_t now) {
  draw_time_band(ctx, "HRV", "PPG", COLOR_WHITE);
  if (s_hrv_view_state == HRV_VIEW_LOW_SIGNAL) {
    draw_hrv_error(ctx, "LOW SIGNAL", "PREVIOUS RESULT KEPT", "REPEAT AT REST");
  } else if (s_hrv_view_state == HRV_VIEW_NO_SIGNAL) {
    draw_hrv_error(ctx, "NO SIGNAL", "CHECK WATCH CONTACT", "TRY AGAIN AT REST");
  } else if (s_hrv_view_state == HRV_VIEW_UNSUPPORTED) {
    draw_hrv_error(ctx, "UNSUPPORTED", "FIRMWARE 4.32+", "HRV API UNAVAILABLE");
  } else if (s_hrv_view_state == HRV_VIEW_HEALTH_OFF) {
    draw_hrv_error(ctx, "HEALTH OFF", "ENABLE HEALTH ACCESS", "NO HRV REQUEST");
  } else if (s_hrv_store.latest_valid) {
    fill_rect(ctx, GRect(0, 69, 200, 47), COLOR_STEEL);
    char rmssd[12];
    snprintf(rmssd, sizeof(rmssd), "%u", s_hrv_store.latest.rmssd_ms);
    draw_text(ctx, rmssd, GRect(8, 65, 115, 53), FONT_KEY_BITHAM_42_BOLD,
              COLOR_WHITE, GTextAlignmentLeft);
    draw_text(ctx, "RMSSD", GRect(120, 73, 70, 17), FONT_KEY_GOTHIC_14_BOLD,
              COLOR_WHITE, GTextAlignmentRight);
    draw_text(ctx, "ms", GRect(145, 91, 45, 20), FONT_KEY_GOTHIC_18_BOLD,
              COLOR_WHITE, GTextAlignmentRight);
    uint32_t baseline = 0;
    char base[24] = "BASE --";
    char delta[28] = "DELTA --";
    if (hrv_baseline(&baseline)) {
      snprintf(base, sizeof(base), "BASE %lu", (unsigned long)baseline);
      const int32_t difference = (int32_t)s_hrv_store.latest.rmssd_ms -
                                 (int32_t)baseline;
      snprintf(delta, sizeof(delta), "DELTA %+ld ms", (long)difference);
    }
    draw_text(ctx, base, GRect(9, 122, 90, 17), FONT_KEY_GOTHIC_14_BOLD,
              COLOR_WHITE, GTextAlignmentLeft);
    draw_text(ctx, delta, GRect(96, 122, 95, 17), FONT_KEY_GOTHIC_14,
              COLOR_WHITE, GTextAlignmentRight);
    time_t measured = s_hrv_store.latest.measured_at;
    struct tm *local = localtime(&measured);
    char when[28] = "LAST --";
    if (local) {
      strftime(when, sizeof(when), "%d %b %H:%M", local);
    }
    draw_text(ctx, when, GRect(9, 142, 130, 16), FONT_KEY_GOTHIC_14,
              COLOR_MUTED, GTextAlignmentLeft);
    draw_text(ctx, "1 MIN", GRect(141, 142, 50, 16), FONT_KEY_GOTHIC_14,
              COLOR_MUTED, GTextAlignmentRight);
    draw_text(ctx, "PPG ESTIMATE", GRect(9, 165, 182, 17),
              FONT_KEY_GOTHIC_14_BOLD, COLOR_WHITE, GTextAlignmentLeft);
    draw_text(ctx, "NOT A READINESS SCORE", GRect(9, 183, 182, 16),
              FONT_KEY_GOTHIC_14, COLOR_MUTED, GTextAlignmentLeft);
  } else {
    if (s_session.active) {
      draw_hrv_error(ctx, "VIEW ONLY", "WORKOUT ACTIVE", "PAST RESULTS ONLY");
    } else {
      draw_hrv_error(ctx, "NO RESULT", "SELECT: MEASURE 1 MIN", "KEEP STILL AT REST");
    }
  }

  char middle_top[8] = "BASE";
  char middle_bottom[16] = "--";
  if (s_rest.active) {
    snprintf(middle_top, sizeof(middle_top), "REST");
    if (now >= s_rest.deadline) {
      char over[12];
      format_duration(seconds_between(now, s_rest.deadline), over, sizeof(over));
      snprintf(middle_bottom, sizeof(middle_bottom), "+%s", over);
    } else {
      format_duration(seconds_between(s_rest.deadline, now), middle_bottom,
                      sizeof(middle_bottom));
    }
  } else {
    uint32_t baseline;
    if (hrv_baseline(&baseline)) {
      snprintf(middle_bottom, sizeof(middle_bottom), "%lu", (unsigned long)baseline);
    }
  }
  draw_footer(ctx, middle_top, middle_bottom, "SESSION",
              s_session.active ? "ON" : "IDLE", now);
}

static void draw_hrv_measure_screen(GContext *ctx, int32_t now) {
  draw_time_band(ctx, "HRV", "1 MIN", COLOR_WHITE);
  fill_rect(ctx, GRect(0, 69, 200, 48), COLOR_STEEL);
  uint32_t remaining = WORKOUT_HRV_CAPTURE_SECONDS;
  uint32_t elapsed = 0;
  if (s_hrv_first_valid_at > 0) {
    elapsed = seconds_between(now, s_hrv_first_valid_at);
    remaining = elapsed >= WORKOUT_HRV_CAPTURE_SECONDS
                    ? 0
                    : WORKOUT_HRV_CAPTURE_SECONDS - elapsed;
  }
  char timer[16];
  format_duration(remaining, timer, sizeof(timer));
  draw_text(ctx, timer, GRect(0, 67, 200, 50), FONT_KEY_BITHAM_34_MEDIUM_NUMBERS,
            COLOR_WHITE, GTextAlignmentCenter);
  draw_text(ctx, s_hrv_first_valid_at ? "KEEP STILL" : "WAIT FOR SIGNAL",
            GRect(10, 125, 180, 20), FONT_KEY_GOTHIC_18_BOLD, COLOR_WHITE,
            GTextAlignmentCenter);
  fill_rect(ctx, GRect(10, 149, 180, 5), COLOR_DARK_STEEL);
  fill_rect(ctx, GRect(10, 149, (int)(elapsed > 60 ? 180 : elapsed * 3), 5),
            COLOR_WHITE);
  const FitnessHrvResult partial = fitness_hrv_calculate_rmssd(
      s_hrv_samples, s_hrv_sample_count);
  draw_text(ctx, "VALID PAIRS", GRect(10, 155, 110, 17), FONT_KEY_GOTHIC_14,
            COLOR_MUTED, GTextAlignmentLeft);
  char pairs[12];
  snprintf(pairs, sizeof(pairs), "%lu", (unsigned long)partial.valid_pairs);
  draw_text(ctx, pairs, GRect(120, 155, 70, 17), FONT_KEY_GOTHIC_14_BOLD,
            COLOR_WHITE, GTextAlignmentRight);
  draw_text(ctx, "PPG ESTIMATE", GRect(10, 171, 180, 15),
            FONT_KEY_GOTHIC_14_BOLD, COLOR_WHITE, GTextAlignmentLeft);
  draw_text(ctx, "NOT A READINESS SCORE", GRect(10, 185, 180, 14),
            FONT_KEY_GOTHIC_14, COLOR_MUTED, GTextAlignmentLeft);
  uint32_t baseline;
  char base[12] = "--";
  if (hrv_baseline(&baseline)) {
    snprintf(base, sizeof(base), "%lu", (unsigned long)baseline);
  }
  draw_footer(ctx, "BASE", base, "STATE", "MEASURE", now);
}

static void draw_confirm_screen(GContext *ctx, int32_t now) {
  draw_time_band(ctx, "SESSION", "FINISH?", COLOR_WHITE);
  fill_rect(ctx, GRect(0, 69, 200, 48), COLOR_STEEL);
  char elapsed[16];
  format_duration(active_elapsed(now), elapsed, sizeof(elapsed));
  draw_text(ctx, elapsed, GRect(0, 67, 200, 50), FONT_KEY_BITHAM_34_MEDIUM_NUMBERS,
            COLOR_WHITE, GTextAlignmentCenter);
  draw_text(ctx, "END WORKOUT?", GRect(10, 126, 180, 22),
            FONT_KEY_GOTHIC_18_BOLD, COLOR_WHITE, GTextAlignmentCenter);
  draw_text(ctx, "SELECT: FINISH", GRect(10, 154, 180, 17),
            FONT_KEY_GOTHIC_14_BOLD, COLOR_WHITE, GTextAlignmentCenter);
  draw_text(ctx, "UP / BACK: CANCEL", GRect(10, 178, 180, 17),
            FONT_KEY_GOTHIC_14, COLOR_MUTED, GTextAlignmentCenter);
  char max_hr[8] = "--";
  if (s_session.max_hr > 0) {
    snprintf(max_hr, sizeof(max_hr), "%u", s_session.max_hr);
  }
  draw_footer(ctx, "MAX", max_hr, "SESSION", "ON", now);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  (void)layer;
  s_canvas_coordinates = true;
  fill_rect(ctx, GRect(0, 28, PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT - 28),
            COLOR_BLACK);
  const int32_t now = now_seconds();
  switch (s_screen) {
    case SCREEN_HRV:
      draw_hrv_screen(ctx, now);
      break;
    case SCREEN_HRV_MEASURING:
      draw_hrv_measure_screen(ctx, now);
      break;
    case SCREEN_CONFIRM:
      draw_confirm_screen(ctx, now);
      break;
    case SCREEN_MAIN:
    default:
      if (s_session.active && s_rest.active) {
        draw_rest_screen(ctx, now);
      } else if (s_session.active) {
        draw_main_screen(ctx, now);
      } else if (s_summary.valid) {
        draw_summary_screen(ctx, now);
      } else {
        draw_idle_screen(ctx, now);
      }
      break;
  }
  if (s_toast_until > now && s_toast[0] != '\0') {
    fill_rect(ctx, GRect(22, 177, 156, 20), COLOR_DARK_STEEL);
    draw_text(ctx, s_toast, GRect(24, 178, 152, 18), FONT_KEY_GOTHIC_14_BOLD,
              COLOR_WHITE, GTextAlignmentCenter);
  }
  s_canvas_coordinates = false;
}

static void dots_update_proc(Layer *layer, GContext *ctx) {
  (void)layer;
  const time_t now = time(NULL);
  struct tm *local = localtime(&now);
  const int seconds = local ? local->tm_sec : 0;
  const int filled = seconds / 5 + 1;
  const uint8_t zone = s_session.active && s_hr_fresh ? s_stable_zone : 0;
  const GColor active_color = zone > 0 ? COLOR_ZONES[zone - 1] : COLOR_MUTED;
  for (int i = 0; i < 12; ++i) {
    const int x = 55 + (i / 2) * 16;
    const int y = 6 + (i % 2) * 10;
    fill_rect(ctx, GRect(x, y, 10, 6), i < filled ? active_color : COLOR_STEEL);
  }
}

static void open_confirm(void) {
  if (!s_session.active || s_screen == SCREEN_HRV_MEASURING) {
    return;
  }
  s_confirm_return_screen = s_screen;
  s_screen = SCREEN_CONFIRM;
  s_confirm_deadline = now_seconds() + CONFIRM_TIMEOUT_SECONDS;
  mark_canvas_dirty();
}

static void cancel_confirm(void) {
  if (s_screen != SCREEN_CONFIRM) {
    return;
  }
  s_screen = s_confirm_return_screen;
  s_confirm_deadline = 0;
  mark_canvas_dirty();
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  if (s_screen == SCREEN_HRV_MEASURING) {
    return;
  }
  if (s_screen == SCREEN_CONFIRM) {
    cancel_confirm();
  } else if (s_screen == SCREEN_HRV) {
    s_screen = SCREEN_MAIN;
    mark_canvas_dirty();
  } else {
    s_screen = SCREEN_HRV;
    mark_canvas_dirty();
  }
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  if (s_screen == SCREEN_HRV_MEASURING) {
    return;
  }
  if (s_screen == SCREEN_CONFIRM) {
    finish_session(now_seconds(), false);
    return;
  }
  if (s_session.active) {
    open_confirm();
  } else if (s_screen == SCREEN_HRV) {
    start_hrv_measurement();
  } else {
    start_session();
  }
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  if (s_screen == SCREEN_HRV_MEASURING || s_screen == SCREEN_CONFIRM) {
    return;
  }
  if (!s_session.active) {
    if (s_screen == SCREEN_MAIN) {
      show_toast("START FIRST");
    }
    return;
  }
  if (s_rest.active) {
    extend_rest();
    show_toast("REST +30 SEC");
  } else {
    start_rest();
    show_toast("REST STARTED");
  }
}

static void back_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  if (s_screen == SCREEN_HRV_MEASURING) {
    cancel_hrv_measurement();
  } else if (s_screen == SCREEN_CONFIRM) {
    cancel_confirm();
  } else if (s_screen == SCREEN_HRV) {
    s_screen = SCREEN_MAIN;
    mark_canvas_dirty();
  } else if (s_session.active && s_rest.active) {
    cancel_rest();
    show_toast("NEXT SET");
    mark_canvas_dirty();
  } else {
    window_stack_pop_all(true);
  }
}

static void click_config_provider(void *context) {
  (void)context;
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler);
}

static bool tuple_to_i32(const Tuple *tuple, int32_t *value) {
  if (!tuple || !value) {
    return false;
  }
  if (tuple->type == TUPLE_INT) {
    *value = tuple->value->int32;
    return true;
  }
  if (tuple->type == TUPLE_UINT && tuple->value->uint32 <= INT32_MAX) {
    *value = (int32_t)tuple->value->uint32;
    return true;
  }
  return false;
}

static bool apply_setting_i32(DictionaryIterator *iterator, uint32_t key,
                              int32_t minimum, int32_t maximum,
                              int32_t *out, bool *seen) {
  Tuple *tuple = dict_find(iterator, key);
  if (!tuple) {
    return true;
  }
  int32_t value;
  if (!tuple_to_i32(tuple, &value) || value < minimum || value > maximum) {
    return false;
  }
  *out = value;
  if (seen) {
    *seen = true;
  }
  return true;
}

static void inbox_received_handler(DictionaryIterator *iterator, void *context) {
  (void)context;
  SettingsV1 candidate = s_settings;
  bool zone_fields_seen = false;
  int32_t value;

  value = candidate.zone_mode + 1;
  if (!apply_setting_i32(iterator, MESSAGE_KEY_ZONE_MODE, 1, 3, &value,
                         &zone_fields_seen)) {
    goto rejected;
  }
  candidate.zone_mode = (uint8_t)(value - 1);
  value = candidate.max_hr;
  if (!apply_setting_i32(iterator, MESSAGE_KEY_MAX_HR, 100, 240, &value,
                         &zone_fields_seen)) {
    goto rejected;
  }
  candidate.max_hr = (uint16_t)value;
  value = candidate.age;
  if (!apply_setting_i32(iterator, MESSAGE_KEY_AGE, 14, 100, &value,
                         &zone_fields_seen)) {
    goto rejected;
  }
  candidate.age = (uint8_t)value;
  value = candidate.age_formula + 1;
  if (!apply_setting_i32(iterator, MESSAGE_KEY_AGE_FORMULA, 1, 2, &value,
                         &zone_fields_seen)) {
    goto rejected;
  }
  candidate.age_formula = (uint8_t)(value - 1);

  const uint32_t manual_keys[FITNESS_ZONE_COUNT] = {
      MESSAGE_KEY_ZONE_1_MIN, MESSAGE_KEY_ZONE_2_MIN, MESSAGE_KEY_ZONE_3_MIN,
      MESSAGE_KEY_ZONE_4_MIN, MESSAGE_KEY_ZONE_5_MIN,
  };
  for (uint8_t i = 0; i < FITNESS_ZONE_COUNT; ++i) {
    value = candidate.manual_lower[i];
    if (!apply_setting_i32(iterator, manual_keys[i], 30, 240, &value,
                           &zone_fields_seen)) {
      goto rejected;
    }
    candidate.manual_lower[i] = (uint16_t)value;
  }

  value = candidate.target_zone;
  if (!apply_setting_i32(iterator, MESSAGE_KEY_TARGET_ZONE, 0, 5, &value, NULL)) {
    goto rejected;
  }
  candidate.target_zone = (uint8_t)value;
  value = candidate.zone_vibes;
  if (!apply_setting_i32(iterator, MESSAGE_KEY_ZONE_VIBES, 0, 1, &value, NULL)) {
    goto rejected;
  }
  candidate.zone_vibes = (uint8_t)value;
  value = candidate.vibrations_enabled;
  if (!apply_setting_i32(iterator, MESSAGE_KEY_VIBRATIONS_ENABLED, 0, 1, &value,
                         NULL)) {
    goto rejected;
  }
  candidate.vibrations_enabled = (uint8_t)value;
  value = candidate.rest_preset_seconds;
  if (!apply_setting_i32(iterator, MESSAGE_KEY_REST_PRESET, 90, 300, &value,
                         NULL)) {
    goto rejected;
  }
  candidate.rest_preset_seconds = (uint16_t)value;
  if (candidate.rest_preset_seconds != 90 && candidate.rest_preset_seconds != 120 &&
      candidate.rest_preset_seconds != 180 && candidate.rest_preset_seconds != 300) {
    goto rejected;
  }

  if (zone_fields_seen) {
    candidate.configured = true;
  }
  if (!settings_are_valid(&candidate, zone_fields_seen)) {
    goto rejected;
  }

  s_settings = candidate;
  refresh_zone_bounds();
  save_settings();
  if (s_session.active) {
    const int32_t now = now_seconds();
    transition_account_state(ACCOUNT_NO_DATA, now);
    reset_runtime_hr();
  }
  show_toast("SETTINGS SAVED");
  mark_dots_dirty();
  return;

rejected:
  APP_LOG(APP_LOG_LEVEL_WARNING, "Rejected invalid settings transaction");
  show_toast("SETTINGS REJECTED");
}

static void wakeup_handler(WakeupId wakeup_id, int32_t cookie) {
  if (!s_rest.active || !rest_cookie_matches(cookie)) {
    return;
  }
  if (s_rest.wakeup_id == wakeup_id) {
    s_rest.wakeup_id = WAKEUP_INVALID_ID;
  }
  const int32_t now = now_seconds();
  rest_alert_if_due(now);
  if (s_rest.active && !s_rest.alerted && s_rest.deadline > now) {
    schedule_rest_wakeup();
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  (void)units_changed;
  const int32_t now = now_seconds();
  const int dot_bucket = tick_time->tm_sec / 5;
  if (dot_bucket != s_last_dot_bucket) {
    s_last_dot_bucket = dot_bucket;
    mark_dots_dirty();
  }

  if (s_session.active) {
    check_hr_stale(now);
    rest_alert_if_due(now);
    const bool minute_changed = chart_advance(now);
    if (minute_changed) {
      chart_backfill_health(now);
    }
    if (minute_changed || now / 60 != s_last_persisted_minute) {
      s_last_persisted_minute = now / 60;
      save_session();
      save_rest();
      save_chart();
    }
    mark_canvas_dirty();
  } else if (tick_time->tm_sec == 0) {
    mark_canvas_dirty();
  }

  if (s_screen == SCREEN_CONFIRM && s_confirm_deadline > 0 &&
      now >= s_confirm_deadline) {
    cancel_confirm();
  }
  if (s_screen == SCREEN_HRV_MEASURING) {
    const WorkoutHrvTransition transition =
        workout_hrv_poll(&s_hrv_lifecycle, now);
    sync_hrv_request_from_lifecycle();
    if (transition == WORKOUT_HRV_TRANSITION_WAIT_TIMEOUT) {
      s_hrv_view_state = HRV_VIEW_NO_SIGNAL;
      s_screen = SCREEN_HRV;
    } else if (transition == WORKOUT_HRV_TRANSITION_CAPTURE_READY) {
      finish_hrv_measurement();
    }
    mark_canvas_dirty();
  }
  if (s_toast_until > 0 && now >= s_toast_until) {
    s_toast_until = 0;
    s_toast[0] = '\0';
    mark_canvas_dirty();
  }
}

static void glance_reload_callback(AppGlanceReloadSession *session, size_t limit,
                                   void *context) {
  (void)context;
  if (limit == 0) {
    return;
  }
  AppGlanceSlice slice = {
      .layout = {
          .icon = APP_GLANCE_SLICE_DEFAULT_ICON,
          .subtitle_template_string = s_glance_subtitle,
      },
      .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION,
  };
  const AppGlanceResult result = app_glance_add_slice(session, slice);
  if (result != APP_GLANCE_RESULT_SUCCESS) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "AppGlance failed: %d", result);
  }
}

static void update_app_glance(void) {
  if (s_session.active) {
    char elapsed[16];
    format_duration(active_elapsed(now_seconds()), elapsed, sizeof(elapsed));
    snprintf(s_glance_subtitle, sizeof(s_glance_subtitle), "TIME %s", elapsed);
  } else if (s_summary.valid) {
    snprintf(s_glance_subtitle, sizeof(s_glance_subtitle), "LAST");
  } else {
    snprintf(s_glance_subtitle, sizeof(s_glance_subtitle), "START");
  }
  app_glance_reload(glance_reload_callback, NULL);
}

static WorkoutSessionLifecycle session_lifecycle_from_state(void) {
  WorkoutSessionLifecycle lifecycle;
  workout_session_lifecycle_init(&lifecycle);
  lifecycle.active = s_session.active;
  lifecycle.started_at = s_session.start_time;
  lifecycle.closed = s_session.closed_at > 0 &&
                     s_session.auto_finish_deadline > 0;
  lifecycle.closed_at = s_session.closed_at;
  lifecycle.auto_finish_deadline = s_session.auto_finish_deadline;
  return lifecycle;
}

static bool auto_finish_closed_session_if_due(int32_t now) {
  WorkoutSessionLifecycle lifecycle = session_lifecycle_from_state();
  if (!lifecycle.active || !lifecycle.closed ||
      now < lifecycle.auto_finish_deadline) {
    return false;
  }

  int64_t finished_at = 0;
  if (workout_session_reopen(&lifecycle, now, &finished_at) !=
      WORKOUT_REOPEN_AUTO_FINISHED) {
    return false;
  }
  finish_session((int32_t)finished_at, true);
  return true;
}

static void recover_session(int32_t now) {
  WorkoutSessionLifecycle lifecycle = session_lifecycle_from_state();
  int64_t finished_at = 0;
  const WorkoutReopenResult reopen_result =
      workout_session_reopen(&lifecycle, now, &finished_at);
  if (reopen_result == WORKOUT_REOPEN_AUTO_FINISHED) {
    finish_session((int32_t)finished_at, true);
    return;
  }
  if (reopen_result == WORKOUT_REOPEN_NO_ACTIVE_SESSION) {
    return;
  }

  session_account_to(now);
  s_session.closed_at = 0;
  s_session.auto_finish_deadline = 0;
  transition_account_state(ACCOUNT_NO_DATA, now);
  reset_runtime_hr();
  save_session();
}

static void recover_rest_wakeup(int32_t now, bool launched_by_wakeup,
                                WakeupId launch_id, int32_t launch_cookie) {
  if (!s_session.active || !s_rest.active) {
    if (s_rest.active) {
      cancel_rest();
    }
    return;
  }
  if (launched_by_wakeup && rest_cookie_matches(launch_cookie)) {
    if (s_rest.wakeup_id == launch_id) {
      s_rest.wakeup_id = WAKEUP_INVALID_ID;
    }
    rest_alert_if_due(now);
  } else if (s_rest.wakeup_id >= 0) {
    time_t scheduled_at;
    if (!wakeup_query((WakeupId)s_rest.wakeup_id, &scheduled_at)) {
      s_rest.wakeup_id = WAKEUP_INVALID_ID;
    }
  }
  rest_alert_if_due(now);
  if (!s_rest.alerted && s_rest.deadline > now && s_rest.wakeup_id < 0) {
    schedule_rest_wakeup();
  }
  save_rest();
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect root_bounds = layer_get_bounds(root);
  s_canvas_layer = layer_create(
      GRect(root_bounds.origin.x, root_bounds.origin.y + 28,
            root_bounds.size.w, root_bounds.size.h - 28));
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(root, s_canvas_layer);

  s_dots_layer = layer_create(GRect(0, 0, PBL_DISPLAY_WIDTH, 28));
  layer_set_update_proc(s_dots_layer, dots_update_proc);
  layer_add_child(root, s_dots_layer);
}

static void window_unload(Window *window) {
  (void)window;
  layer_destroy(s_dots_layer);
  s_dots_layer = NULL;
  layer_destroy(s_canvas_layer);
  s_canvas_layer = NULL;
}

static void exit_stale_wakeup(void *context) {
  (void)context;
  window_stack_pop_all(false);
}

static void init(void) {
  load_settings();
  load_persisted_state();
  refresh_zone_bounds();
  if (persist_exists(PERSIST_KEY_REST_GENERATION)) {
    s_rest_generation_seed =
        (uint16_t)persist_read_int(PERSIST_KEY_REST_GENERATION);
  } else {
    s_rest_generation_seed = (uint16_t)now_seconds();
  }
  if (s_rest.active && s_rest.generation > s_rest_generation_seed) {
    s_rest_generation_seed = s_rest.generation;
  }

  WakeupId launch_wakeup_id = WAKEUP_INVALID_ID;
  int32_t launch_cookie = 0;
  const bool launched_by_wakeup =
      launch_reason() == APP_LAUNCH_WAKEUP &&
      wakeup_get_launch_event(&launch_wakeup_id, &launch_cookie);
  const int32_t now = now_seconds();
  s_stale_wakeup_launch = launched_by_wakeup &&
                          (!s_rest.active || !rest_cookie_matches(launch_cookie));
  if (s_stale_wakeup_launch) {
    (void)auto_finish_closed_session_if_due(now);
  } else {
    recover_session(now);
    recover_rest_wakeup(now, launched_by_wakeup, launch_wakeup_id, launch_cookie);
  }
  update_app_glance();

  wakeup_service_subscribe(wakeup_handler);
#if defined(PBL_HEALTH)
  s_health_subscribed = health_service_events_subscribe(health_handler, NULL);
#else
  s_health_subscribed = false;
#endif
  update_health_accessibility();
  if (s_session.active && !s_stale_wakeup_launch) {
    set_hr_sampling(true);
    chart_backfill_health(now);
  }

  s_window = window_create();
  window_set_background_color(s_window, COLOR_BLACK);
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
      .load = window_load,
      .unload = window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(256, 64);
  s_last_persisted_minute = now / 60;
  s_last_dot_bucket = -1;
  if (s_stale_wakeup_launch) {
    app_timer_register(1, exit_stale_wakeup, NULL);
  }
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  stop_hrv_request();
  set_hr_sampling(false);

  const int32_t now = now_seconds();
  if (s_session.active && !s_stale_wakeup_launch) {
    session_account_to(now);
    transition_account_state(ACCOUNT_NO_DATA, now);
    WorkoutSessionLifecycle lifecycle = session_lifecycle_from_state();
    int64_t close_deadline = workout_session_close(&lifecycle, now);
    if (close_deadline > INT32_MAX) {
      close_deadline = INT32_MAX;
    }
    s_session.closed_at = (int32_t)lifecycle.closed_at;
    s_session.auto_finish_deadline = (int32_t)close_deadline;
    if (s_rest.active && s_rest.deadline > s_session.auto_finish_deadline) {
      cancel_rest_wakeup();
      save_rest();
    } else if (s_rest.active && !s_rest.alerted && s_rest.deadline > now &&
               s_rest.wakeup_id < 0) {
      schedule_rest_wakeup();
    }
    save_session();
    save_rest();
    save_chart();
  }
  update_app_glance();

  app_message_deregister_callbacks();
#if defined(PBL_HEALTH)
  if (s_health_subscribed) {
    health_service_events_unsubscribe();
  }
#endif
  window_destroy(s_window);
  s_window = NULL;
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

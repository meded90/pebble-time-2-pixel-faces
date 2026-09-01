#include <pebble.h>
#include <string.h>

#define ARM_DELAY_MS 650
#define APP_MESSAGE_ACK_TIMEOUT_MS 3000
#define BOOT_TIMEOUT_MS 5000
#define SUCCESS_TIMEOUT_MS 4000
#define RING_FRAME_MS 150
#define DEVICE_NAME_SIZE 32

typedef enum {
  StateBoot = 0,
  StateAuthRequired = 1,
  StateReady = 2,
  StateArming = 3,
  StateRequesting = 4,
  StateRingingAccepted = 5,
  StateOffline = 6,
  StateAuthExpired = 7,
  StateRateLimited = 8,
  StateError = 9,
} AppState;

typedef enum {
  CommandPlaySound = 1,
  CommandRefreshStatus = 2,
  CommandSelectDevice = 3,
} AppCommand;

typedef struct {
  const char *find;
  const char *connecting;
  const char *sign_in;
  const char *open_settings;
  const char *hold_select;
  const char *keep_holding;
  const char *release_cancel;
  const char *contacting;
  const char *sending;
  const char *sound_sent;
  const char *should_ring;
  const char *offline;
  const char *retry;
  const char *sign_in_again;
  const char *wait;
  const char *try_later;
  const char *failed;
  const char *switching;
} WatchStrings;

static const WatchStrings STRINGS_EN = {
  "Find my iPhone", "Connecting…", "Sign in required", "Open phone settings",
  "Hold Select", "Keep holding", "Release to cancel", "Contacting Apple", "Sending…",
  "Sound sent", "iPhone should ring", "Phone offline", "Select to retry",
  "Sign in again", "Wait 10 seconds", "Try again later", "Could not send", "Switching…"
};
static const WatchStrings STRINGS_RU = {
  "Найти iPhone", "Подключение…", "Нужен вход", "Откройте настройки",
  "Удерживайте Select", "Продолжайте", "Отпустите для отмены", "Связь с Apple", "Отправка…",
  "Сигнал отправлен", "iPhone зазвонит", "Телефон не в сети", "Select — повторить",
  "Войдите снова", "Подождите 10 сек", "Повторите позже", "Не удалось", "Переключение…"
};
static const WatchStrings STRINGS_UK = {
  "Знайти iPhone", "Підключення…", "Потрібен вхід", "Відкрийте налаштування",
  "Утримуйте Select", "Продовжуйте", "Відпустіть для скас.", "Зв’язок з Apple", "Надсилання…",
  "Сигнал надіслано", "iPhone задзвонить", "Телефон не в мережі", "Select — повторити",
  "Увійдіть знову", "Зачекайте 10 сек", "Спробуйте пізніше", "Не вдалося", "Перемикання…"
};
static const WatchStrings STRINGS_DE = {
  "iPhone finden", "Verbinden…", "Anmeldung nötig", "Handy-Einstellungen",
  "Select halten", "Weiter halten", "Loslassen: Abbruch", "Apple kontaktieren", "Senden…",
  "Ton gesendet", "iPhone klingelt", "Handy offline", "Select: erneut",
  "Neu anmelden", "10 Sek. warten", "Später versuchen", "Senden fehlgeschl.", "Wechseln…"
};
static const WatchStrings STRINGS_ES = {
  "Buscar mi iPhone", "Conectando…", "Inicia sesión", "Ajustes del teléfono",
  "Mantén Select", "Sigue pulsando", "Suelta para cancelar", "Contactando Apple", "Enviando…",
  "Sonido enviado", "El iPhone sonará", "Teléfono sin red", "Select: reintentar",
  "Inicia sesión", "Espera 10 segundos", "Intenta más tarde", "No se pudo enviar", "Cambiando…"
};
static const WatchStrings STRINGS_FR = {
  "Localiser l’iPhone", "Connexion…", "Connexion requise", "Réglages téléphone",
  "Maintenir Select", "Maintenez", "Relâchez pour annuler", "Contact avec Apple", "Envoi…",
  "Son envoyé", "L’iPhone va sonner", "Téléphone hors ligne", "Select : réessayer",
  "Reconnectez-vous", "Attendez 10 sec", "Réessayez plus tard", "Échec de l’envoi", "Changement…"
};
static const WatchStrings STRINGS_IT = {
  "Trova il mio iPhone", "Connessione…", "Accesso richiesto", "Impostazioni telefono",
  "Tieni Select", "Continua a tenere", "Rilascia per annullare", "Contatto Apple", "Invio…",
  "Suono inviato", "iPhone suonerà", "Telefono offline", "Select: riprova",
  "Accedi di nuovo", "Attendi 10 secondi", "Riprova più tardi", "Invio non riuscito", "Cambio…"
};
static const WatchStrings STRINGS_PT = {
  "Buscar meu iPhone", "Conectando…", "Inicie sessão", "Ajustes do telefone",
  "Segure Select", "Continue segurando", "Solte para cancelar", "Contatando Apple", "Enviando…",
  "Som enviado", "O iPhone vai tocar", "Telefone offline", "Select: tentar",
  "Entre novamente", "Aguarde 10 segundos", "Tente mais tarde", "Falha ao enviar", "Alternando…"
};

static const WatchStrings *s_strings = &STRINGS_EN;

static Window *s_window;
static BitmapLayer *s_background_layer;
static BitmapLayer *s_scene_layer;
static BitmapLayer *s_wave_layer;
static BitmapLayer *s_badge_layer;
static TextLayer *s_title_layer;
static TextLayer *s_device_layer;
static TextLayer *s_hint_layer;
static Layer *s_progress_layer;
static Layer *s_chrome_layer;

static GBitmap *s_background_bitmap;
static GBitmap *s_scene_bitmap;
static GBitmap *s_wave_bitmap;
static GBitmap *s_badge_bitmap;

static AppTimer *s_arm_timer;
static AppTimer *s_ack_timer;
static AppTimer *s_boot_timer;
static AppTimer *s_success_timer;
static AppTimer *s_ring_timer;
static AppState s_state = StateBoot;
static uint32_t s_request_id;
static uint8_t s_device_count;
static uint8_t s_device_index;
static uint8_t s_progress_percent;
static bool s_select_is_down;
static uint8_t s_ring_phase;
static char s_device_name[DEVICE_NAME_SIZE];

static bool locale_starts_with(const char *locale, const char *language) {
  return locale && strncmp(locale, language, strlen(language)) == 0;
}

static void select_language(void) {
  const char *locale = i18n_get_system_locale();
  if (locale_starts_with(locale, "ru") || locale_starts_with(locale, "be")) s_strings = &STRINGS_RU;
  else if (locale_starts_with(locale, "uk")) s_strings = &STRINGS_UK;
  else if (locale_starts_with(locale, "de")) s_strings = &STRINGS_DE;
  else if (locale_starts_with(locale, "es")) s_strings = &STRINGS_ES;
  else if (locale_starts_with(locale, "fr")) s_strings = &STRINGS_FR;
  else if (locale_starts_with(locale, "it")) s_strings = &STRINGS_IT;
  else if (locale_starts_with(locale, "pt")) s_strings = &STRINGS_PT;
  else s_strings = &STRINGS_EN;
}

static void cancel_timer(AppTimer **timer) {
  if (*timer) {
    app_timer_cancel(*timer);
    *timer = NULL;
  }
}

static void replace_bitmap(BitmapLayer *layer, GBitmap **current, uint32_t resource_id, GRect frame) {
  if (*current) {
    bitmap_layer_set_bitmap(layer, NULL);
    gbitmap_destroy(*current);
    *current = NULL;
  }
  if (!resource_id) {
    layer_set_hidden(bitmap_layer_get_layer(layer), true);
    return;
  }
  *current = gbitmap_create_with_resource(resource_id);
  bitmap_layer_set_bitmap(layer, *current);
  bitmap_layer_set_compositing_mode(layer, GCompOpSet);
  layer_set_frame(bitmap_layer_get_layer(layer), frame);
  layer_set_hidden(bitmap_layer_get_layer(layer), false);
}

static void set_badge(uint32_t resource_id) {
  replace_bitmap(s_badge_layer, &s_badge_bitmap, resource_id, GRect(162, 10, 30, 30));
}

static void set_scene(uint32_t scene_resource, GRect scene_frame,
                      uint32_t wave_resource, GRect wave_frame) {
  replace_bitmap(s_scene_layer, &s_scene_bitmap, scene_resource, scene_frame);
  replace_bitmap(s_wave_layer, &s_wave_bitmap, wave_resource, wave_frame);
}

static void progress_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  graphics_fill_rect(ctx, bounds, 2, GCornersAll);
  graphics_context_set_fill_color(ctx, GColorCyan);
  int16_t width = bounds.size.w * s_progress_percent / 100;
  graphics_fill_rect(ctx, GRect(0, 0, width, bounds.size.h), 2, GCornersAll);
}

static void chrome_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  graphics_fill_rect(ctx, GRect(6, 6, 154, 39), 8, GCornersAll);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(10, 196, 180, 24), 8, GCornersAll);
}

static void set_text(const char *title, const char *device, const char *hint) {
  text_layer_set_text(s_title_layer, title);
  text_layer_set_text(s_device_layer, device ? device : "");
  text_layer_set_text(s_hint_layer, hint);
}

static void schedule_ring_frame(void);

static void ring_frame_callback(void *context) {
  s_ring_timer = NULL;
  if (s_state != StateRingingAccepted) return;
  s_ring_phase = (s_ring_phase + 1) % 4;
  switch (s_ring_phase) {
    case 0:
      replace_bitmap(s_wave_layer, &s_wave_bitmap, RESOURCE_ID_IMAGE_WAVE_1, GRect(140, 111, 60, 64));
      break;
    case 1:
      replace_bitmap(s_wave_layer, &s_wave_bitmap, RESOURCE_ID_IMAGE_WAVE_2, GRect(140, 111, 60, 64));
      break;
    case 2:
      replace_bitmap(s_wave_layer, &s_wave_bitmap, RESOURCE_ID_IMAGE_WAVE_3, GRect(140, 111, 60, 64));
      break;
    default:
      replace_bitmap(s_wave_layer, &s_wave_bitmap, RESOURCE_ID_IMAGE_WAVE_2, GRect(140, 111, 60, 64));
      break;
  }
  schedule_ring_frame();
}

static void schedule_ring_frame(void) {
  cancel_timer(&s_ring_timer);
  s_ring_timer = app_timer_register(RING_FRAME_MS, ring_frame_callback, NULL);
}

static void apply_state(AppState state);

static void ack_timeout_callback(void *context) {
  s_ack_timer = NULL;
  if (s_state == StateBoot || s_state == StateReady ||
      s_state == StateRequesting) {
    apply_state(StateOffline);
  }
}

static void success_timeout_callback(void *context) {
  s_success_timer = NULL;
  if (s_state == StateRingingAccepted) apply_state(StateReady);
}

static void apply_state(AppState state) {
  cancel_timer(&s_ring_timer);
  cancel_timer(&s_success_timer);
  s_state = state;
  s_progress_percent = 0;
  layer_set_hidden(s_progress_layer, true);
  set_badge(0);

  switch (state) {
    case StateAuthRequired:
      set_scene(RESOURCE_ID_IMAGE_SCENE_FOREGROUND, GRect(24, 67, 145, 99), 0, GRect(0, 0, 1, 1));
      set_badge(RESOURCE_ID_IMAGE_BADGE_AUTH);
      set_text(s_strings->sign_in, "", s_strings->open_settings);
      break;
    case StateReady:
      set_scene(RESOURCE_ID_IMAGE_SCENE_FOREGROUND, GRect(24, 67, 145, 99), 0, GRect(0, 0, 1, 1));
      set_text(s_strings->find, s_device_name, s_strings->hold_select);
      break;
    case StateArming:
      set_scene(RESOURCE_ID_IMAGE_SCENE_FOREGROUND, GRect(24, 67, 145, 99),
                0, GRect(0, 0, 1, 1));
      set_text(s_strings->keep_holding, s_device_name, s_strings->release_cancel);
      layer_set_hidden(s_progress_layer, false);
      break;
    case StateRequesting:
      set_scene(RESOURCE_ID_IMAGE_SCENE_FOREGROUND, GRect(24, 67, 145, 99),
                RESOURCE_ID_IMAGE_WAVE_1, GRect(140, 111, 60, 64));
      set_text(s_strings->contacting, s_device_name, s_strings->sending);
      break;
    case StateRingingAccepted:
      set_scene(RESOURCE_ID_IMAGE_SCENE_FOREGROUND, GRect(24, 67, 145, 99),
                RESOURCE_ID_IMAGE_WAVE_2, GRect(140, 111, 60, 64));
      set_badge(RESOURCE_ID_IMAGE_BADGE_SUCCESS);
      set_text(s_strings->sound_sent, s_device_name, s_strings->should_ring);
      vibes_short_pulse();
      s_ring_phase = 0;
      schedule_ring_frame();
      s_success_timer = app_timer_register(SUCCESS_TIMEOUT_MS, success_timeout_callback, NULL);
      break;
    case StateOffline:
      set_scene(RESOURCE_ID_IMAGE_SCENE_FOREGROUND, GRect(24, 67, 145, 99), 0, GRect(0, 0, 1, 1));
      set_badge(RESOURCE_ID_IMAGE_BADGE_OFFLINE);
      set_text(s_strings->offline, "", s_strings->retry);
      break;
    case StateAuthExpired:
      set_scene(RESOURCE_ID_IMAGE_SCENE_FOREGROUND, GRect(24, 67, 145, 99), 0, GRect(0, 0, 1, 1));
      set_badge(RESOURCE_ID_IMAGE_BADGE_AUTH);
      set_text(s_strings->sign_in_again, "", s_strings->open_settings);
      break;
    case StateRateLimited:
      set_scene(RESOURCE_ID_IMAGE_SCENE_FOREGROUND, GRect(24, 67, 145, 99), 0, GRect(0, 0, 1, 1));
      set_badge(RESOURCE_ID_IMAGE_BADGE_ERROR);
      set_text(s_strings->wait, "", s_strings->try_later);
      break;
    case StateError:
      set_scene(RESOURCE_ID_IMAGE_SCENE_FOREGROUND, GRect(24, 67, 145, 99), 0, GRect(0, 0, 1, 1));
      set_badge(RESOURCE_ID_IMAGE_BADGE_ERROR);
      set_text(s_strings->failed, "", s_strings->retry);
      break;
    case StateBoot:
    default:
      set_scene(RESOURCE_ID_IMAGE_SCENE_FOREGROUND, GRect(24, 67, 145, 99), 0, GRect(0, 0, 1, 1));
      set_text(s_strings->find, "", s_strings->connecting);
      break;
  }
  layer_mark_dirty(s_progress_layer);
}

static void send_command(AppCommand command, uint32_t request_id) {
  DictionaryIterator *iterator;
  AppMessageResult result = app_message_outbox_begin(&iterator);
  if (result != APP_MSG_OK || !iterator) {
    apply_state(StateOffline);
    return;
  }
  cancel_timer(&s_ack_timer);
  dict_write_uint8(iterator, MESSAGE_KEY_COMMAND, command);
  dict_write_uint32(iterator, MESSAGE_KEY_REQUEST_ID, request_id);
  dict_write_cstring(iterator, MESSAGE_KEY_LOCALE, i18n_get_system_locale());
  if (command == CommandSelectDevice) {
    dict_write_uint8(iterator, MESSAGE_KEY_DEVICE_INDEX, s_device_index);
  }
  result = app_message_outbox_send();
  if (result != APP_MSG_OK) {
    apply_state(StateOffline);
    return;
  }
  s_ack_timer = app_timer_register(APP_MESSAGE_ACK_TIMEOUT_MS,
                                   ack_timeout_callback, NULL);
}

static void arm_progress_callback(void *context) {
  if (s_state != StateArming || !s_select_is_down) return;
  if (s_progress_percent < 100) {
    s_progress_percent = s_progress_percent > 90 ? 100 : s_progress_percent + 10;
    layer_mark_dirty(s_progress_layer);
    app_timer_register(ARM_DELAY_MS / 10, arm_progress_callback, NULL);
  }
}

static void arm_complete_callback(void *context) {
  s_arm_timer = NULL;
  if (s_state != StateArming || !s_select_is_down) return;
  s_request_id += 1;
  apply_state(StateRequesting);
  send_command(CommandPlaySound, s_request_id);
}

static void select_down_handler(ClickRecognizerRef recognizer, void *context) {
  s_select_is_down = true;
  if (s_state == StateReady) {
    apply_state(StateArming);
    s_progress_percent = 1;
    layer_mark_dirty(s_progress_layer);
    app_timer_register(ARM_DELAY_MS / 10, arm_progress_callback, NULL);
    cancel_timer(&s_arm_timer);
    s_arm_timer = app_timer_register(ARM_DELAY_MS, arm_complete_callback, NULL);
  } else if (s_state == StateOffline || s_state == StateError) {
    s_request_id += 1;
    apply_state(StateRequesting);
    send_command(CommandPlaySound, s_request_id);
  }
}

static void select_up_handler(ClickRecognizerRef recognizer, void *context) {
  s_select_is_down = false;
  if (s_state == StateArming) {
    cancel_timer(&s_arm_timer);
    apply_state(StateReady);
  }
}

static void change_device(int direction) {
  if (s_state != StateReady || s_device_count < 2) return;
  int next = (int)s_device_index + direction;
  if (next < 0) next = s_device_count - 1;
  if (next >= s_device_count) next = 0;
  s_device_index = (uint8_t)next;
  text_layer_set_text(s_device_layer, s_strings->switching);
  send_command(CommandSelectDevice, 0);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  change_device(-1);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  change_device(1);
}

static void back_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_state == StateError) {
    apply_state(StateReady);
    return;
  }
  window_stack_pop(true);
}

static void click_config_provider(void *context) {
  window_raw_click_subscribe(BUTTON_ID_SELECT, select_down_handler, select_up_handler, NULL);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler);
}

static void inbox_received_handler(DictionaryIterator *iterator, void *context) {
  cancel_timer(&s_boot_timer);
  Tuple *device_name = dict_find(iterator, MESSAGE_KEY_DEVICE_NAME);
  if (device_name && device_name->type == TUPLE_CSTRING) {
    strncpy(s_device_name, device_name->value->cstring, sizeof(s_device_name) - 1);
    s_device_name[sizeof(s_device_name) - 1] = '\0';
  }
  Tuple *count = dict_find(iterator, MESSAGE_KEY_DEVICE_COUNT);
  if (count) s_device_count = count->value->uint8;
  Tuple *index = dict_find(iterator, MESSAGE_KEY_DEVICE_INDEX);
  if (index) s_device_index = index->value->uint8;
  Tuple *state = dict_find(iterator, MESSAGE_KEY_STATE);
  if (state && state->value->uint8 <= StateError) apply_state((AppState)state->value->uint8);
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Inbox dropped: %d", reason);
  apply_state(StateOffline);
}

static void outbox_failed_handler(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  cancel_timer(&s_ack_timer);
  APP_LOG(APP_LOG_LEVEL_WARNING, "Outbox failed: %d", reason);
  apply_state(StateOffline);
}

static void outbox_sent_handler(DictionaryIterator *iterator, void *context) {
  cancel_timer(&s_ack_timer);
}

static void boot_timeout_callback(void *context) {
  s_boot_timer = NULL;
  if (s_state == StateBoot) apply_state(StateOffline);
}

static TextLayer *create_text_layer(GRect frame, GFont font, GTextAlignment alignment) {
  TextLayer *layer = text_layer_create(frame);
  text_layer_set_background_color(layer, GColorClear);
  text_layer_set_text_color(layer, GColorWhite);
  text_layer_set_font(layer, font);
  text_layer_set_text_alignment(layer, alignment);
  return layer;
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND_SOFA);
  s_background_layer = bitmap_layer_create(GRect(0, 0, 200, 228));
  bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
  layer_add_child(root, bitmap_layer_get_layer(s_background_layer));

  s_scene_layer = bitmap_layer_create(GRect(0, 0, 1, 1));
  s_wave_layer = bitmap_layer_create(GRect(0, 0, 1, 1));
  s_badge_layer = bitmap_layer_create(GRect(0, 0, 1, 1));
  layer_add_child(root, bitmap_layer_get_layer(s_scene_layer));
  layer_add_child(root, bitmap_layer_get_layer(s_wave_layer));

  s_chrome_layer = layer_create(GRect(0, 0, 200, 228));
  layer_set_update_proc(s_chrome_layer, chrome_update_proc);
  layer_add_child(root, s_chrome_layer);
  layer_add_child(root, bitmap_layer_get_layer(s_badge_layer));

  s_title_layer = create_text_layer(GRect(12, 7, 144, 20), fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GTextAlignmentLeft);
  s_device_layer = create_text_layer(GRect(12, 24, 144, 18), fonts_get_system_font(FONT_KEY_GOTHIC_14), GTextAlignmentLeft);
  text_layer_set_text_color(s_device_layer, GColorCyan);
  s_hint_layer = create_text_layer(GRect(12, 197, 176, 22), fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GTextAlignmentCenter);
  text_layer_set_text_color(s_hint_layer, GColorOxfordBlue);
  layer_add_child(root, text_layer_get_layer(s_title_layer));
  layer_add_child(root, text_layer_get_layer(s_device_layer));
  layer_add_child(root, text_layer_get_layer(s_hint_layer));

  s_progress_layer = layer_create(GRect(14, 221, 172, 5));
  layer_set_update_proc(s_progress_layer, progress_update_proc);
  layer_add_child(root, s_progress_layer);
  apply_state(StateBoot);
}

static void destroy_dynamic_bitmap(GBitmap **bitmap) {
  if (*bitmap) {
    gbitmap_destroy(*bitmap);
    *bitmap = NULL;
  }
}

static void window_unload(Window *window) {
  cancel_timer(&s_arm_timer);
  cancel_timer(&s_ack_timer);
  cancel_timer(&s_boot_timer);
  cancel_timer(&s_success_timer);
  cancel_timer(&s_ring_timer);
  destroy_dynamic_bitmap(&s_scene_bitmap);
  destroy_dynamic_bitmap(&s_wave_bitmap);
  destroy_dynamic_bitmap(&s_badge_bitmap);
  if (s_background_bitmap) gbitmap_destroy(s_background_bitmap);
  bitmap_layer_destroy(s_background_layer);
  bitmap_layer_destroy(s_scene_layer);
  bitmap_layer_destroy(s_wave_layer);
  bitmap_layer_destroy(s_badge_layer);
  text_layer_destroy(s_title_layer);
  text_layer_destroy(s_device_layer);
  text_layer_destroy(s_hint_layer);
  layer_destroy(s_chrome_layer);
  layer_destroy(s_progress_layer);
}

static void init(void) {
  select_language();
  s_device_name[0] = '\0';
  s_request_id = persist_exists(1) ? (uint32_t)persist_read_int(1) : 0;
  s_window = window_create();
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) { .load = window_load, .unload = window_unload });
  window_stack_push(s_window, true);

  app_message_register_inbox_received(inbox_received_handler);
  app_message_register_inbox_dropped(inbox_dropped_handler);
  app_message_register_outbox_failed(outbox_failed_handler);
  app_message_register_outbox_sent(outbox_sent_handler);
  app_message_open(512, 128);
  s_boot_timer = app_timer_register(BOOT_TIMEOUT_MS, boot_timeout_callback, NULL);
  send_command(CommandRefreshStatus, 0);
}

static void deinit(void) {
  persist_write_int(1, (int32_t)s_request_id);
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

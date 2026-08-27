#include <pebble.h>

#define DICTATION_BUFFER_SIZE 768
#define DISPLAY_BUFFER_SIZE 1024
#define RESULT_BUFFER_SIZE 560
#define ACTION_BUFFER_SIZE 184
#define ERROR_BUFFER_SIZE 52
#define BODY_TOP 66
#define BODY_HEIGHT 132
#define FOOTER_TOP 202
#define SCROLL_STEP 28
#define PERSIST_KEY_COMMAND_NONCE 9101

typedef enum {
  LocalStateWaitingForPhone,
  LocalStateReady,
  LocalStateDictating,
  LocalStateConfirm,
  LocalStateSending,
  LocalStateWorking,
  LocalStateCompleted,
  LocalStateNeedsAttention,
  LocalStateError,
  LocalStateNotConfigured,
} LocalState;

typedef enum {
  BridgeStateNotConfigured = 0,
  BridgeStateReady = 1,
  BridgeStateSending = 2,
  BridgeStateWorking = 3,
  BridgeStateCompleted = 4,
  BridgeStateNeedsAttention = 5,
  BridgeStateError = 6,
} BridgeState;

static Window *s_window;
static Layer *s_header_layer;
static Layer *s_body_clip_layer;
static TextLayer *s_title_layer;
static TextLayer *s_status_layer;
static TextLayer *s_body_text_layer;
static TextLayer *s_footer_layer;
static DictationSession *s_dictation_session;

static LocalState s_state = LocalStateWaitingForPhone;
static int16_t s_scroll_offset;
static int16_t s_max_scroll;
static char s_transcript[DICTATION_BUFFER_SIZE];
static char s_result_text[RESULT_BUFFER_SIZE];
static char s_action_summary[ACTION_BUFFER_SIZE];
static char s_error_code[ERROR_BUFFER_SIZE];
static char s_display_text[DISPLAY_BUFFER_SIZE];
static bool s_can_confirm;
static bool s_retry_phone;
static bool s_retry_transcript;
static uint32_t s_command_nonce;

static void copy_string(char *destination, size_t size, const char *source) {
  if (!destination || size == 0) {
    return;
  }
  if (!source) {
    destination[0] = '\0';
    return;
  }
  strncpy(destination, source, size - 1);
  destination[size - 1] = '\0';
}

static const char *state_title(LocalState state) {
  switch (state) {
    case LocalStateReady:
      return "WRIST AGENT";
    case LocalStateDictating:
      return "LISTENING";
    case LocalStateConfirm:
      return "SEND REQUEST?";
    case LocalStateSending:
      return "SENDING";
    case LocalStateWorking:
      return "AGENT WORKING";
    case LocalStateCompleted:
      return "DONE";
    case LocalStateNeedsAttention:
      return "ACTION NEEDED";
    case LocalStateError:
      return "TRY AGAIN";
    case LocalStateNotConfigured:
      return "SETUP NEEDED";
    case LocalStateWaitingForPhone:
    default:
      return "CONNECTING";
  }
}

static const char *state_status(LocalState state) {
  switch (state) {
    case LocalStateReady:
      return "VOICE -> CHATGPT WORKSPACE";
    case LocalStateDictating:
      return "USE THE SYSTEM DICTATION UI";
    case LocalStateConfirm:
      return "REVIEW THE EXACT COMMAND";
    case LocalStateSending:
      return "PHONE -> PRIVATE BRIDGE";
    case LocalStateWorking:
      return "TOOLS MAY STILL BE RUNNING";
    case LocalStateCompleted:
      return "RESULT RETURNED TO WATCH";
    case LocalStateNeedsAttention:
      return s_can_confirm ? "REVIEW BEFORE APPROVING" :
                             "OPEN CHATGPT ON YOUR PHONE";
    case LocalStateError:
      return s_error_code[0] ? s_error_code : "REQUEST FAILED";
    case LocalStateNotConfigured:
      return "OPEN APP SETTINGS ON PHONE";
    case LocalStateWaitingForPhone:
    default:
      return "WAITING FOR PEBBLEKIT";
  }
}

static const char *state_footer(LocalState state) {
  switch (state) {
    case LocalStateReady:
      return "SELECT  SPEAK";
    case LocalStateConfirm:
      return "SELECT SEND    BACK CANCEL";
    case LocalStateWorking:
    case LocalStateSending:
      return "SELECT  REFRESH";
    case LocalStateNeedsAttention:
      return s_can_confirm ? "SELECT APPROVE  BACK CANCEL" : "SELECT  REFRESH";
    case LocalStateCompleted:
      return "SELECT  NEW REQUEST";
    case LocalStateError:
      return s_retry_transcript ? "SELECT  RESEND COMMAND" :
             s_retry_phone ? "SELECT  RETRY PHONE" : "SELECT  NEW REQUEST";
    case LocalStateNotConfigured:
      return "PHONE  APP SETTINGS";
    case LocalStateDictating:
      return "SPEAK CLEARLY";
    case LocalStateWaitingForPhone:
    default:
      return "PHONE CONNECTION REQUIRED";
  }
}

static void set_body_text_for_state(LocalState state) {
  s_display_text[0] = '\0';
  switch (state) {
    case LocalStateReady:
      copy_string(
          s_display_text, sizeof(s_display_text),
          "Press SELECT, speak naturally, then accept the transcription.\n\n"
          "You will review it once more before it is sent. Dictation follows your phone language.");
      break;
    case LocalStateDictating:
      copy_string(s_display_text, sizeof(s_display_text),
                  "Listening through Pebble dictation...\n\nYour phone and internet connection are required.");
      break;
    case LocalStateConfirm:
      copy_string(s_display_text, sizeof(s_display_text), s_transcript);
      break;
    case LocalStateSending:
      copy_string(s_display_text, sizeof(s_display_text),
                  s_result_text[0] ? s_result_text : "Sending the accepted command securely...");
      break;
    case LocalStateWorking:
      copy_string(s_display_text, sizeof(s_display_text),
                  s_result_text[0] ? s_result_text : "Workspace Agent is working...");
      if (s_action_summary[0]) {
        size_t used = strlen(s_display_text);
        snprintf(s_display_text + used, sizeof(s_display_text) - used,
                 "\n\n%s", s_action_summary);
      }
      break;
    case LocalStateCompleted:
      copy_string(s_display_text, sizeof(s_display_text),
                  s_result_text[0] ? s_result_text : "Done.");
      if (s_action_summary[0]) {
        size_t used = strlen(s_display_text);
        snprintf(s_display_text + used, sizeof(s_display_text) - used,
                 "\n\nACTION\n%s", s_action_summary);
      }
      break;
    case LocalStateNeedsAttention:
      copy_string(s_display_text, sizeof(s_display_text),
                  s_result_text[0] ? s_result_text :
                  "Open the ChatGPT conversation to approve or continue the request.");
      if (s_action_summary[0]) {
        size_t used = strlen(s_display_text);
        snprintf(s_display_text + used, sizeof(s_display_text) - used,
                 "\n\n%s", s_action_summary);
      }
      break;
    case LocalStateError:
      copy_string(s_display_text, sizeof(s_display_text),
                  s_result_text[0] ? s_result_text : "The request failed. Check your phone connection and try again.");
      break;
    case LocalStateNotConfigured:
      copy_string(
          s_display_text, sizeof(s_display_text),
          "In the Pebble phone app, open Wrist Agent settings and enter:\n\n"
          "1. Your HTTPS bridge URL\n2. Its device token\n\n"
          "Never paste an OpenAI access token into watch settings.");
      break;
    case LocalStateWaitingForPhone:
    default:
      copy_string(s_display_text, sizeof(s_display_text),
                  "Starting PebbleKit JS and checking the private bridge settings...");
      break;
  }
}

static GColor state_accent(LocalState state) {
  switch (state) {
    case LocalStateCompleted:
      return GColorSpringBud;
    case LocalStateNeedsAttention:
      return GColorChromeYellow;
    case LocalStateError:
    case LocalStateNotConfigured:
      return GColorSunsetOrange;
    case LocalStateConfirm:
      return GColorCyan;
    default:
      return GColorPictonBlue;
  }
}

static void header_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GColor accent = state_accent(s_state);
  graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  graphics_context_set_fill_color(ctx, accent);
  graphics_fill_rect(ctx, GRect(0, bounds.size.h - 4, bounds.size.w, 4), 0, GCornerNone);

  graphics_context_set_stroke_color(ctx, accent);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_round_rect(ctx, GRect(13, 8, 17, 24), 8);
  graphics_draw_line(ctx, GPoint(9, 23), GPoint(9, 25));
  graphics_draw_line(ctx, GPoint(9, 25), GPoint(13, 30));
  graphics_draw_line(ctx, GPoint(30, 30), GPoint(34, 25));
  graphics_draw_line(ctx, GPoint(34, 25), GPoint(34, 23));
  graphics_draw_line(ctx, GPoint(21, 32), GPoint(21, 37));
  graphics_draw_line(ctx, GPoint(15, 37), GPoint(27, 37));
}

static void apply_scroll_offset(void) {
  if (!s_body_text_layer) {
    return;
  }
  if (s_scroll_offset < 0) {
    s_scroll_offset = 0;
  }
  if (s_scroll_offset > s_max_scroll) {
    s_scroll_offset = s_max_scroll;
  }
  Layer *text_layer = text_layer_get_layer(s_body_text_layer);
  GRect frame = layer_get_frame(text_layer);
  frame.origin.y = -s_scroll_offset;
  layer_set_frame(text_layer, frame);
}

static void refresh_layout(void) {
  text_layer_set_text(s_title_layer, state_title(s_state));
  text_layer_set_text(s_status_layer, state_status(s_state));
  text_layer_set_text(s_footer_layer, state_footer(s_state));
  set_body_text_for_state(s_state);
  text_layer_set_text(s_body_text_layer, s_display_text);

  Layer *text_layer = text_layer_get_layer(s_body_text_layer);
  layer_set_frame(text_layer, GRect(6, 0, 188, 720));
  GSize content_size = text_layer_get_content_size(s_body_text_layer);
  int16_t measured_height = content_size.h + 12;
  int16_t text_height = measured_height > BODY_HEIGHT
      ? measured_height
      : BODY_HEIGHT;
  layer_set_frame(text_layer, GRect(6, 0, 188, text_height));
  s_max_scroll = text_height > BODY_HEIGHT
      ? text_height - BODY_HEIGHT
      : 0;
  s_scroll_offset = 0;
  apply_scroll_offset();
  layer_mark_dirty(s_header_layer);
}

static void set_state(LocalState state) {
  s_state = state;
  refresh_layout();
}

static void send_refresh_request(void) {
  DictionaryIterator *iterator = NULL;
  AppMessageResult result = app_message_outbox_begin(&iterator);
  if (result != APP_MSG_OK || !iterator) {
    s_retry_phone = true;
    s_retry_transcript = false;
    copy_string(s_error_code, sizeof(s_error_code), "PHONE LINK");
    copy_string(s_result_text, sizeof(s_result_text), "Could not reach PebbleKit JS.");
    set_state(LocalStateError);
    return;
  }
  dict_write_uint8(iterator, MESSAGE_KEY_REFRESH_REQUEST, 1);
  result = app_message_outbox_send();
  if (result != APP_MSG_OK) {
    s_retry_phone = true;
    s_retry_transcript = false;
    copy_string(s_error_code, sizeof(s_error_code), "PHONE LINK");
    copy_string(s_result_text, sizeof(s_result_text), "Could not request an update from the phone.");
    set_state(LocalStateError);
  }
}

static void send_decision_request(uint8_t decision) {
  DictionaryIterator *iterator = NULL;
  AppMessageResult result = app_message_outbox_begin(&iterator);
  if (result != APP_MSG_OK || !iterator) {
    s_retry_phone = true;
    s_retry_transcript = false;
    copy_string(s_error_code, sizeof(s_error_code), "PHONE LINK");
    copy_string(s_result_text, sizeof(s_result_text), "Could not send the approval to the phone.");
    set_state(LocalStateError);
    return;
  }
  dict_write_uint8(iterator, MESSAGE_KEY_CONFIRM_REQUEST, decision);
  result = app_message_outbox_send();
  if (result != APP_MSG_OK) {
    s_retry_phone = true;
    s_retry_transcript = false;
    copy_string(s_error_code, sizeof(s_error_code), "PHONE LINK");
    copy_string(s_result_text, sizeof(s_result_text), "Could not send the approval to the phone.");
    set_state(LocalStateError);
    return;
  }
  copy_string(s_result_text, sizeof(s_result_text),
              decision == 1 ? "Approval sent. Waiting for the agent..." :
                              "Cancelling the proposed action...");
  s_can_confirm = false;
  set_state(LocalStateWorking);
}

static void send_transcript(void) {
  DictionaryIterator *iterator = NULL;
  AppMessageResult result = app_message_outbox_begin(&iterator);
  if (result != APP_MSG_OK || !iterator) {
    s_retry_transcript = true;
    s_retry_phone = false;
    copy_string(s_error_code, sizeof(s_error_code), "PHONE LINK");
    copy_string(s_result_text, sizeof(s_result_text), "Could not open the phone message queue.");
    set_state(LocalStateError);
    return;
  }
  dict_write_cstring(iterator, MESSAGE_KEY_TRANSCRIPT, s_transcript);
  dict_write_uint32(iterator, MESSAGE_KEY_COMMAND_NONCE, s_command_nonce);
  result = app_message_outbox_send();
  if (result != APP_MSG_OK) {
    s_retry_transcript = true;
    s_retry_phone = false;
    copy_string(s_error_code, sizeof(s_error_code), "PHONE LINK");
    copy_string(s_result_text, sizeof(s_result_text), "Could not send the command to the phone.");
    set_state(LocalStateError);
    return;
  }
  s_result_text[0] = '\0';
  s_action_summary[0] = '\0';
  s_error_code[0] = '\0';
  s_retry_transcript = false;
  set_state(LocalStateSending);
}

static void dictation_callback(DictationSession *session,
                               DictationSessionStatus status,
                               char *transcription,
                               void *context) {
  (void)session;
  (void)context;
  if (status == DictationSessionStatusSuccess && transcription && transcription[0]) {
    copy_string(s_transcript, sizeof(s_transcript), transcription);
    s_command_nonce += 1;
    if (s_command_nonce == 0) {
      s_command_nonce = 1;
    }
    persist_write_int(PERSIST_KEY_COMMAND_NONCE, (int32_t)s_command_nonce);
    set_state(LocalStateConfirm);
    return;
  }

  switch (status) {
    case DictationSessionStatusFailureConnectivityError:
      copy_string(s_result_text, sizeof(s_result_text),
                  "Dictation needs a connected phone and internet access.");
      copy_string(s_error_code, sizeof(s_error_code), "DICTATION NETWORK");
      break;
    case DictationSessionStatusFailureDisabled:
      copy_string(s_result_text, sizeof(s_result_text),
                  "Voice dictation is disabled in the Pebble phone service.");
      copy_string(s_error_code, sizeof(s_error_code), "DICTATION OFF");
      break;
    case DictationSessionStatusFailureNoSpeechDetected:
      copy_string(s_result_text, sizeof(s_result_text), "No speech was detected. Try again.");
      copy_string(s_error_code, sizeof(s_error_code), "NO SPEECH");
      break;
    case DictationSessionStatusFailureTranscriptionRejected:
      set_state(LocalStateReady);
      return;
    default:
      copy_string(s_result_text, sizeof(s_result_text), "Dictation did not finish. Try again.");
      copy_string(s_error_code, sizeof(s_error_code), "DICTATION");
      break;
  }
  set_state(LocalStateError);
}

static void start_dictation(void) {
  s_retry_phone = false;
  s_retry_transcript = false;
  if (!s_dictation_session) {
    s_dictation_session = dictation_session_create(
        DICTATION_BUFFER_SIZE, dictation_callback, NULL);
    if (s_dictation_session) {
      dictation_session_enable_confirmation(s_dictation_session, true);
      dictation_session_enable_error_dialogs(s_dictation_session, true);
    }
  }

  if (!s_dictation_session) {
    copy_string(s_result_text, sizeof(s_result_text),
                "Dictation is unavailable. Check the phone connection and voice service.");
    copy_string(s_error_code, sizeof(s_error_code), "NO DICTATION");
    set_state(LocalStateError);
    return;
  }

  DictationSessionStatus status = dictation_session_start(s_dictation_session);
  if (status != DictationSessionStatusSuccess) {
    copy_string(s_result_text, sizeof(s_result_text), "Could not start dictation.");
    copy_string(s_error_code, sizeof(s_error_code), "DICTATION START");
    set_state(LocalStateError);
    return;
  }
  set_state(LocalStateDictating);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  switch (s_state) {
    case LocalStateReady:
    case LocalStateCompleted:
      start_dictation();
      break;
    case LocalStateError:
      if (s_retry_transcript) {
        send_transcript();
      } else if (s_retry_phone) {
        send_refresh_request();
      } else {
        start_dictation();
      }
      break;
    case LocalStateConfirm:
      send_transcript();
      break;
    case LocalStateSending:
    case LocalStateWorking:
      send_refresh_request();
      break;
    case LocalStateNeedsAttention:
      if (s_can_confirm) {
        send_decision_request(1);
      } else {
        send_refresh_request();
      }
      break;
    default:
      break;
  }
}

static void back_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  if (s_state == LocalStateConfirm) {
    s_transcript[0] = '\0';
    set_state(LocalStateReady);
    return;
  }
  if (s_state == LocalStateNeedsAttention && s_can_confirm) {
    send_decision_request(2);
    return;
  }
  window_stack_pop(true);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  s_scroll_offset -= SCROLL_STEP;
  apply_scroll_offset();
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  s_scroll_offset += SCROLL_STEP;
  apply_scroll_offset();
}

static void click_config_provider(void *context) {
  (void)context;
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 120, up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 120, down_click_handler);
}

static void inbox_received_handler(DictionaryIterator *iterator, void *context) {
  (void)context;
  Tuple *result_tuple = dict_find(iterator, MESSAGE_KEY_RESULT_TEXT);
  Tuple *action_tuple = dict_find(iterator, MESSAGE_KEY_ACTION_SUMMARY);
  Tuple *error_tuple = dict_find(iterator, MESSAGE_KEY_ERROR_CODE);
  Tuple *state_tuple = dict_find(iterator, MESSAGE_KEY_AGENT_STATE);
  Tuple *vibrate_tuple = dict_find(iterator, MESSAGE_KEY_VIBRATE_ON_RESULT);
  Tuple *can_confirm_tuple = dict_find(iterator, MESSAGE_KEY_CAN_CONFIRM);
  Tuple *retry_tuple = dict_find(iterator, MESSAGE_KEY_RETRY_SAME_REQUEST);

  if (result_tuple && result_tuple->type == TUPLE_CSTRING) {
    copy_string(s_result_text, sizeof(s_result_text), result_tuple->value->cstring);
  }
  if (action_tuple && action_tuple->type == TUPLE_CSTRING) {
    copy_string(s_action_summary, sizeof(s_action_summary), action_tuple->value->cstring);
  }
  if (error_tuple && error_tuple->type == TUPLE_CSTRING) {
    copy_string(s_error_code, sizeof(s_error_code), error_tuple->value->cstring);
  }
  if (!state_tuple) {
    refresh_layout();
    return;
  }

  s_can_confirm = can_confirm_tuple && can_confirm_tuple->value->int32 != 0;
  s_retry_transcript = false;

  LocalState previous_state = s_state;
  int32_t bridge_state = state_tuple->value->int32;
  s_retry_phone = bridge_state == BridgeStateError && retry_tuple &&
      retry_tuple->value->int32 != 0;
  switch (bridge_state) {
    case BridgeStateNotConfigured:
      set_state(LocalStateNotConfigured);
      break;
    case BridgeStateReady:
      set_state(LocalStateReady);
      break;
    case BridgeStateSending:
      set_state(LocalStateSending);
      break;
    case BridgeStateWorking:
      set_state(LocalStateWorking);
      break;
    case BridgeStateCompleted:
      set_state(LocalStateCompleted);
      if (previous_state != LocalStateCompleted && vibrate_tuple && vibrate_tuple->value->int32) {
        vibes_short_pulse();
      }
      break;
    case BridgeStateNeedsAttention:
      set_state(LocalStateNeedsAttention);
      if (previous_state != LocalStateNeedsAttention && vibrate_tuple &&
          vibrate_tuple->value->int32) {
        vibes_double_pulse();
      }
      break;
    case BridgeStateError:
    default:
      set_state(LocalStateError);
      break;
  }
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
  (void)reason;
  (void)context;
  s_retry_phone = true;
  s_retry_transcript = false;
  copy_string(s_result_text, sizeof(s_result_text),
              "The phone response was too large or could not be read.");
  copy_string(s_error_code, sizeof(s_error_code), "PHONE DATA");
  set_state(LocalStateError);
}

static void outbox_failed_handler(DictionaryIterator *iterator,
                                  AppMessageResult reason,
                                  void *context) {
  (void)reason;
  (void)context;
  s_retry_transcript = iterator &&
      dict_find(iterator, MESSAGE_KEY_TRANSCRIPT) != NULL;
  s_retry_phone = !s_retry_transcript;
  copy_string(s_result_text, sizeof(s_result_text),
              "The command could not reach the connected phone.");
  copy_string(s_error_code, sizeof(s_error_code), "PHONE LINK");
  set_state(LocalStateError);
}

static TextLayer *create_text_layer(GRect frame, GFont font, GColor color,
                                    GTextAlignment alignment) {
  TextLayer *layer = text_layer_create(frame);
  text_layer_set_background_color(layer, GColorClear);
  text_layer_set_text_color(layer, color);
  text_layer_set_font(layer, font);
  text_layer_set_text_alignment(layer, alignment);
  return layer;
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  window_set_background_color(window, GColorBlack);

  s_header_layer = layer_create(GRect(0, 0, bounds.size.w, 46));
  layer_set_update_proc(s_header_layer, header_update_proc);
  layer_add_child(root, s_header_layer);

  s_title_layer = create_text_layer(
      GRect(43, 5, 151, 27), fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
      GColorWhite, GTextAlignmentLeft);
  layer_add_child(s_header_layer, text_layer_get_layer(s_title_layer));

  s_status_layer = create_text_layer(
      GRect(7, 47, 186, 19), fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
      GColorLightGray, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_status_layer));

  s_body_clip_layer = layer_create(GRect(0, BODY_TOP, bounds.size.w, BODY_HEIGHT));
  layer_set_clips(s_body_clip_layer, true);
  layer_add_child(root, s_body_clip_layer);

  s_body_text_layer = create_text_layer(
      GRect(6, 0, 188, BODY_HEIGHT), fonts_get_system_font(FONT_KEY_GOTHIC_18),
      GColorWhite, GTextAlignmentLeft);
  text_layer_set_overflow_mode(s_body_text_layer, GTextOverflowModeWordWrap);
  layer_add_child(s_body_clip_layer, text_layer_get_layer(s_body_text_layer));

  s_footer_layer = create_text_layer(
      GRect(0, FOOTER_TOP, bounds.size.w, 25),
      fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GColorBlack,
      GTextAlignmentCenter);
  text_layer_set_background_color(s_footer_layer, GColorWhite);
  layer_add_child(root, text_layer_get_layer(s_footer_layer));

  refresh_layout();
}

static void window_unload(Window *window) {
  (void)window;
  text_layer_destroy(s_footer_layer);
  text_layer_destroy(s_body_text_layer);
  layer_destroy(s_body_clip_layer);
  text_layer_destroy(s_status_layer);
  text_layer_destroy(s_title_layer);
  layer_destroy(s_header_layer);
}

static void init(void) {
  s_command_nonce = persist_exists(PERSIST_KEY_COMMAND_NONCE)
      ? (uint32_t)persist_read_int(PERSIST_KEY_COMMAND_NONCE)
      : (uint32_t)time(NULL);
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_set_click_config_provider(s_window, click_config_provider);

  app_message_register_inbox_received(inbox_received_handler);
  app_message_register_inbox_dropped(inbox_dropped_handler);
  app_message_register_outbox_failed(outbox_failed_handler);
  app_message_open(app_message_inbox_size_maximum(),
                   app_message_outbox_size_maximum());

  window_stack_push(s_window, true);
}

static void deinit(void) {
  if (s_dictation_session) {
    dictation_session_destroy(s_dictation_session);
  }
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

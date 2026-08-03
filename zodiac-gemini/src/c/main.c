#include <pebble.h>

static Window *s_window;
static BitmapLayer *s_background_layer;
static GBitmap *s_background_bitmap;

static void window_load(Window *window) {
  Layer *root_layer = window_get_root_layer(window);
  window_set_background_color(window, GColorWhite);

  s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
  if (!s_background_bitmap) {
    return;
  }

  s_background_layer = bitmap_layer_create(layer_get_bounds(root_layer));
  if (!s_background_layer) {
    return;
  }

  bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
  bitmap_layer_set_compositing_mode(s_background_layer, GCompOpSet);
  layer_add_child(root_layer, bitmap_layer_get_layer(s_background_layer));
}

static void window_unload(Window *window) {
  if (s_background_layer) {
    bitmap_layer_destroy(s_background_layer);
  }
  if (s_background_bitmap) {
    gbitmap_destroy(s_background_bitmap);
  }
}

static void init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "pebble.h"
#include "../src/c/intro_config.h"
static GBitmap frame, still;
static GBitmap *still_ptr = &still;
static AppTimer timer;
static unsigned now_ms, active_timers, live_frames, live_png, updates, frame_index;
static bool allocation_fails, decode_fails, timer_fails, short_read, invalid_offset;
static uint8_t buffer[INTRO_MAX_CHUNK], delta_buffer[INTRO_MAX_DELTA], pixels[45600];
static void *test_malloc(size_t size) { if(allocation_fails)return NULL;live_png++;return size==INTRO_MAX_CHUNK?buffer:delta_buffer; }
static void test_free(void *p) { if(p){assert((p==buffer||p==delta_buffer)&&live_png);live_png--;} }
void app_timer_cancel(AppTimer *p) { assert(p == &timer && active_timers); active_timers = 0; }
AppTimer *app_timer_register(uint32_t ms, void (*cb)(void *), void *data) {
  (void)cb; (void)data; assert(ms > 0 && !active_timers);
  if (timer_fails) return NULL;
  active_timers = 1; return &timer;
}
void bitmap_layer_set_bitmap(BitmapLayer *l, GBitmap *b) { l->bitmap = b; }
void gbitmap_destroy(GBitmap *b) { if(b==&still)return; assert(b == &frame && live_frames); live_frames--; }
GBitmap *gbitmap_create_with_resource(uint32_t id) { assert(id==RESOURCE_ID_IMAGE_BACKGROUND);return &still; }
void time_ms(time_t *s, uint16_t *m) { *s = now_ms / 1000; *m = now_ms % 1000; }
GBitmap *gbitmap_create_blank(GSize s,int fmt) { (void)fmt; assert(s.w==200&&s.h==228&&!live_frames);live_frames++;return &frame; }
uint8_t *gbitmap_get_data(GBitmap *b) { assert(b==&frame);return pixels; }
uint16_t gbitmap_get_bytes_per_row(GBitmap *b) { assert(b==&frame);return 200; }
bool intro_decode(const uint8_t *p,size_t n,uint8_t *d,size_t cap,uint8_t *dst,uint16_t stride) {
 assert(p==buffer&&n==1&&d==delta_buffer&&cap==INTRO_MAX_DELTA&&dst==pixels&&stride==200&&live_frames==1&&live_png==2);updates++;return !decode_fails;
}
ResHandle resource_get_handle(uint32_t id) { assert(id==RESOURCE_ID_INTRO_DATA);return 1; }
size_t resource_size(ResHandle r) { (void)r;return (INTRO_FRAMES+1)*4+INTRO_FRAMES; }
size_t resource_load_byte_range(ResHandle r,uint32_t offset,uint8_t *out,size_t n) {
  (void)r;if(short_read)return 0;
  if(n==8){frame_index=offset/4;assert(frame_index<INTRO_FRAMES);for(int j=0;j<2;j++){uint32_t v=invalid_offset?0:(INTRO_FRAMES+1)*4+frame_index+j;for(int k=0;k<4;k++)out[j*4+k]=v>>(k*8);}}
  return n;
}
void layer_mark_dirty(Layer *l) { assert(l); }
#define malloc test_malloc
#define free test_free
#include "../src/c/intro.c"
#undef malloc
#undef free
static BitmapLayer layer;
static void reset(void) {
  assert(!active_timers && !live_frames && !live_png);
  s_started = false; now_ms = 0; updates = 0;
  allocation_fails = decode_fails = timer_fails = short_read = invalid_offset = false;
  layer.bitmap = &still; still_ptr = &still;
}
static void idle(void) { assert(!active_timers && !live_frames && !live_png); assert(layer.bitmap == &still && still_ptr==&still); }
int main(void) {
  reset();intro_start(&layer,&still_ptr,&layer);assert(active_timers&&layer.bitmap==&frame);
  for(now_ms=INTRO_FRAME_MS;now_ms<=INTRO_FRAMES*INTRO_FRAME_MS;now_ms+=INTRO_FRAME_MS){active_timers=0;next_frame(NULL);}idle();assert(updates==INTRO_FRAMES);
  intro_start(&layer,&still_ptr,&layer);idle();assert(updates==INTRO_FRAMES);
  reset();intro_start(&layer,&still_ptr,&layer);intro_stop();intro_stop();intro_start(&layer,&still_ptr,&layer);idle();assert(updates==1);
  reset();allocation_fails=true;intro_start(&layer,&still_ptr,&layer);idle();assert(!updates);
  reset();decode_fails=true;intro_start(&layer,&still_ptr,&layer);idle();
  reset();timer_fails=true;intro_start(&layer,&still_ptr,&layer);idle();
  reset();short_read=true;intro_start(&layer,&still_ptr,&layer);idle();assert(!updates);
  reset();invalid_offset=true;intro_start(&layer,&still_ptr,&layer);idle();assert(!updates);
  reset();intro_start(&layer,&still_ptr,&layer);now_ms=3500;active_timers=0;next_frame(NULL);assert(frame_index==1);now_ms=100;active_timers=0;next_frame(NULL);assert(frame_index==2);intro_stop();idle();
  puts("PASS: one-shot stop, no replay, single-frame memory, sequential late-frame playback, allocation/decode/timer/resource failure cleanup");
}

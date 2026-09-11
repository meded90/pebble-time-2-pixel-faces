#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
typedef struct { int tag; } GBitmap;
typedef struct { int tag; } AppTimer;
typedef struct { GBitmap *bitmap; } BitmapLayer;
typedef BitmapLayer Layer;
typedef int ResHandle;
typedef struct { int w,h; } GSize;
#define GSize(w,h) ((GSize){w,h})
#define GBitmapFormat8Bit 0
#define GColorBlack ((struct {uint8_t argb;}){192})
#define RESOURCE_ID_INTRO_DATA 1
#define RESOURCE_ID_IMAGE_BACKGROUND 2
#define APP_LOG(...) ((void)0)
void app_timer_cancel(AppTimer *);
AppTimer *app_timer_register(uint32_t, void (*)(void *), void *);
void bitmap_layer_set_bitmap(BitmapLayer *, GBitmap *);
void gbitmap_destroy(GBitmap *);
void time_ms(time_t *, uint16_t *);
void layer_mark_dirty(Layer *);
GBitmap *gbitmap_create_with_resource(uint32_t);
GBitmap *gbitmap_create_blank(GSize,int);
uint8_t *gbitmap_get_data(GBitmap *);
uint16_t gbitmap_get_bytes_per_row(GBitmap *);
ResHandle resource_get_handle(uint32_t);
size_t resource_size(ResHandle);
size_t resource_load_byte_range(ResHandle,uint32_t,uint8_t *,size_t);

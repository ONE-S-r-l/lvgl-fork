/**
 * @file lv_draw_buf_private.h
 *
 */

#ifndef LV_DRAW_BUF_PRIVATE_H
#define LV_DRAW_BUF_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "lv_draw_buf.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

struct _lv_draw_buf_handlers_t {
    lv_draw_buf_malloc_cb_t buf_malloc_cb;
    lv_draw_buf_free_cb_t buf_free_cb;
    lv_draw_buf_copy_cb_t buf_copy_cb;
    lv_draw_buf_align_cb_t align_pointer_cb;
    lv_draw_buf_cache_operation_cb_t invalidate_cache_cb;
    lv_draw_buf_cache_operation_cb_t flush_cache_cb;
    lv_draw_buf_width_to_stride_cb_t width_to_stride_cb;
};

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Called internally to initialize the draw_buf_handlers in lv_global
 */
void lv_draw_buf_init_handlers(void);

/**
 * Create a dedicated memory pool for image-cache pixel payloads and route the
 * image draw buffer handlers' malloc/free to it. Called from lv_init() after the
 * draw units have wired their cache (flush/invalidate) callbacks.
 * Only compiled/defined when LV_IMAGE_CACHE_POOL_SIZE > 0.
 */
void lv_draw_buf_image_pool_init(void);

#if LV_IMAGE_CACHE_POOL_SIZE > 0 && LV_USE_MEM_MONITOR
#include "../stdlib/lv_mem.h"
/**
 * Fill a monitor struct with usage statistics of the dedicated image cache pool.
 * @param mon  pointer to an lv_mem_monitor_t to populate
 */
void lv_draw_buf_image_pool_monitor(lv_mem_monitor_t * mon);
#endif

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_BUF_PRIVATE_H*/

/**
 * @file lv_draw_buf_image_pool.c
 *
 * Dedicated memory pool for image-cache pixel payloads.
 *
 * On systems where the main LVGL pool (lv_malloc) must live in fast but small
 * internal memory, the decoded-image buffers managed by the image cache can be
 * placed in a separate, larger external region instead. Only the large pixel
 * payloads are moved: the lv_draw_buf_t descriptor, cache nodes and file names
 * keep using the main pool.
 *
 * This works because lv_bin_decoder.c routes every large pixel allocation
 * through the dedicated `image_cache_draw_buf_handlers` set, and each draw
 * buffer remembers the handlers that allocated it, so free is always matched.
 * Here we only override that handler set's malloc/free callbacks; the
 * copy/align/stride and (importantly) the renderer's flush/invalidate cache
 * callbacks are left untouched.
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_draw_buf_private.h"

#if LV_IMAGE_CACHE_POOL_SIZE > 0

#if LV_USE_STDLIB_MALLOC != LV_STDLIB_BUILTIN
    #error "LV_IMAGE_CACHE_POOL_SIZE requires LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN"
#endif

#include "../stdlib/builtin/lv_tlsf.h"
#include "../stdlib/lv_mem.h"
#if LV_USE_EMMC_DECODER
    #include "../libs/emmc_decoder/lv_emmc_decoder.h"   /*LV_EMMC_BLOCK_SIZE*/
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void * img_pool_malloc(size_t size, lv_color_format_t color_format);
static void img_pool_free(void * buf);
#if LV_USE_MEM_MONITOR
    static void img_pool_walker(void * ptr, size_t size, int used, void * user);
#endif

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_tlsf_t img_tlsf;
static size_t img_used;     /*Current bytes (TLSF block sizes) handed out from the pool*/
static size_t img_max_used; /*Peak of img_used*/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_buf_image_pool_init(void)
{
    img_tlsf = lv_tlsf_create_with_pool((void *)LV_IMAGE_CACHE_POOL_ADR, LV_IMAGE_CACHE_POOL_SIZE);
    if(img_tlsf == NULL) {
        LV_LOG_ERROR("Failed to create image cache pool (check LV_IMAGE_CACHE_POOL_ADR/SIZE "
                     "and that the region is memory-mapped)");
        return;
    }

    /*Override ONLY malloc/free. Keep copy/align/stride and the renderer's
     *flush/invalidate cache callbacks that were already wired by the draw units.*/
    lv_draw_buf_handlers_t * handlers = lv_draw_buf_get_image_handlers();
    handlers->buf_malloc_cb = img_pool_malloc;
    handlers->buf_free_cb = img_pool_free;
}

#if LV_USE_MEM_MONITOR
void lv_draw_buf_image_pool_monitor(lv_mem_monitor_t * mon)
{
    lv_memzero(mon, sizeof(lv_mem_monitor_t));
    if(img_tlsf == NULL) return;

    /*Walk the pool to get total/free/fragmentation, mirroring lv_mem_monitor.*/
    lv_tlsf_walk_pool(lv_tlsf_get_pool(img_tlsf), img_pool_walker, mon);

    if(mon->total_size > 0) {
        mon->used_pct = 100 - (uint32_t)((uint64_t)100U * mon->free_size / mon->total_size);
    }
    if(mon->free_size > 0) {
        mon->frag_pct = (uint32_t)((uint64_t)mon->free_biggest_size * 100U / mon->free_size);
        mon->frag_pct = 100 - mon->frag_pct;
    }
    mon->max_used = img_max_used;
}
#endif /*LV_USE_MEM_MONITOR*/

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void * img_pool_malloc(size_t size, lv_color_format_t color_format)
{
    LV_UNUSED(color_format);

    size_t requested = size; /*size LVGL asked for (the draw buffer's data_size)*/

#if LV_USE_EMMC_DECODER
    /*Round the payload up to the device block size so the eMMC decoder can transfer whole
     *blocks straight into this buffer (asset offsets are already block-aligned), with no
     *bounce buffer for a partial tail block. Costs up to LV_EMMC_BLOCK_SIZE-1 bytes/buffer.*/
    size = LV_ROUND_UP(size, LV_EMMC_BLOCK_SIZE);
#endif
    /*Mirror the default buf_malloc: allocate extra so lv_draw_buf_align() in
     *lv_draw_buf_create_ex() can always align within the returned block.*/
    size += LV_DRAW_BUF_ALIGN - 1;
    void * buf = lv_tlsf_malloc(img_tlsf, size);

    // /*Trace: requested data_size -> size asked of TLSF (rounded+aligned) -> actual block.*/
    // LV_LOG_USER("img pool malloc: requested=%u tlsf_req=%u block=%u -> %p",
    //             (unsigned)requested, (unsigned)size,
    //             (unsigned)(buf ? lv_tlsf_block_size(buf) : 0), buf);

    if(buf) {
        img_used += lv_tlsf_block_size(buf);
        if(img_used > img_max_used) img_max_used = img_used;
    }
    return buf;
}

static void img_pool_free(void * buf)
{
    if(buf) img_used -= lv_tlsf_block_size(buf);
    lv_tlsf_free(img_tlsf, buf);
}

#if LV_USE_MEM_MONITOR
static void img_pool_walker(void * ptr, size_t size, int used, void * user)
{
    LV_UNUSED(ptr);
    lv_mem_monitor_t * mon = user;
    size += lv_tlsf_alloc_overhead();
    mon->total_size += size;
    if(used) {
        mon->used_cnt++;
    }
    else {
        mon->free_cnt++;
        mon->free_size += size;
        if(size > mon->free_biggest_size) mon->free_biggest_size = size;
    }
}
#endif /*LV_USE_MEM_MONITOR*/

#endif /*LV_IMAGE_CACHE_POOL_SIZE > 0*/

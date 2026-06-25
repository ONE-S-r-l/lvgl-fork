/**
 * @file lv_emmc_decoder.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_emmc_decoder.h"

#if LV_USE_EMMC_DECODER

#include "../../draw/lv_image_decoder_private.h"
#include "../../draw/lv_draw_buf.h"
#include "../../draw/lv_image_dsc.h"
#include "../../stdlib/lv_string.h"
#include "../../stdlib/lv_sprintf.h"

/*********************
 *      DEFINES
 *********************/

#define DECODER_NAME    "EMMC"

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_emmc_read_cb_t emmc_read_cb;
static const uint8_t * assets_start;
static const uint8_t * assets_end;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_emmc_decoder_init(void)
{
    lv_image_decoder_t * decoder = lv_image_decoder_create();
    LV_ASSERT_MALLOC(decoder);
    if(decoder == NULL) {
        LV_LOG_WARN("Out of memory");
        return;
    }

    lv_image_decoder_set_info_cb(decoder, lv_emmc_decoder_info);
    lv_image_decoder_set_open_cb(decoder, lv_emmc_decoder_open);
    lv_image_decoder_set_close_cb(decoder, lv_emmc_decoder_close);

    decoder->name = DECODER_NAME;
}

void lv_emmc_decoder_set_source(lv_emmc_read_cb_t read_cb, const void * start, const void * end)
{
    emmc_read_cb = read_cb;
    assets_start = start;
    assets_end = end;
}

/**
 * Claim only variable sources whose payload pointer lies inside the asset window.
 * The header (cf/w/h/stride) is read straight from the in-flash descriptor.
 */
lv_result_t lv_emmc_decoder_info(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc,
                                 lv_image_header_t * header)
{
    LV_UNUSED(decoder);

    if(dsc->src_type != LV_IMAGE_SRC_VARIABLE) return LV_RESULT_INVALID;

    const lv_image_dsc_t * image = dsc->src;
    const uint8_t * data = image->data;
    if(data == NULL || assets_start == NULL) return LV_RESULT_INVALID;

    /*Decline images whose payload is not in the eMMC asset section*/
    if(data < assets_start || data >= assets_end) return LV_RESULT_INVALID;

    lv_memcpy(header, &image->header, sizeof(lv_image_header_t));
    return LV_RESULT_OK;
}

lv_result_t lv_emmc_decoder_open(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc)
{
    const lv_image_dsc_t * image = dsc->src;

    if(dsc->src_type != LV_IMAGE_SRC_VARIABLE || image == NULL) return LV_RESULT_INVALID;
    if(emmc_read_cb == NULL) {
        LV_LOG_ERROR("eMMC read callback not registered (lv_emmc_decoder_set_source)");
        return LV_RESULT_INVALID;
    }

    /*Use dsc->header for geometry: the core fills a computed stride into it after info_cb
     *(lv_image_decoder.c) when the descriptor itself carries stride 0 (v8-legacy images).
     *The on-device byte layout must match the geometry create_ex reproduces from
     *w/h/cf/stride, otherwise the GPU would read garbage.*/
    if(dsc->header.stride == 0) {
        LV_LOG_ERROR("eMMC asset has zero stride");
        return LV_RESULT_INVALID;
    }

    uint32_t offset = (uint32_t)((uintptr_t)image->data - (uintptr_t)assets_start);

    /*Allocate the destination in the image cache pool*/
    lv_draw_buf_t * decoded = lv_draw_buf_create_ex(lv_draw_buf_get_image_handlers(),
                                                    dsc->header.w, dsc->header.h, dsc->header.cf,
                                                    dsc->header.stride);
    if(decoded == NULL) {
        LV_LOG_ERROR("No memory for eMMC image (%" LV_PRIu32 " bytes)", (uint32_t)image->data_size);
        return LV_RESULT_INVALID;
    }

    /*Trace BOTH header sources to surface any inconsistency:
     * - image->header : the raw lv_image_dsc_t descriptor in flash (stride may be 0 for
     *                   v8-legacy assets); image->data_size is how many bytes we read.
     * - dsc->header   : the copy the core completed (computed stride) used to size the
     *                   destination buffer (decoded->data_size).
     * A data_size vs buf_size divergence means the raw read is being truncated by LV_MIN.*/
    LV_LOG_USER("EMMC decode src=%p data=%p offset=%" LV_PRIu32,
                (void *)image, (const void *)image->data, offset);
    LV_LOG_USER("  image->header: cf=%d %" LV_PRIu32 "x%" LV_PRIu32 " stride=%" LV_PRIu32
                " flags=0x%" LV_PRIx32 " data_size=%" LV_PRIu32,
                (int)image->header.cf, (uint32_t)image->header.w, (uint32_t)image->header.h,
                (uint32_t)image->header.stride, (uint32_t)image->header.flags,
                (uint32_t)image->data_size);
    LV_LOG_USER("  dsc->header  : cf=%d %" LV_PRIu32 "x%" LV_PRIu32 " stride=%" LV_PRIu32
                " flags=0x%" LV_PRIx32 " buf_size=%" LV_PRIu32 " -> %p%s",
                (int)dsc->header.cf, (uint32_t)dsc->header.w, (uint32_t)dsc->header.h,
                (uint32_t)dsc->header.stride, (uint32_t)dsc->header.flags,
                (uint32_t)decoded->data_size, (void *)decoded->data,
                (image->data_size != decoded->data_size) ? "  <-- SIZE MISMATCH" : "");

    /*Fetch the raw payload. Use LV_MIN like lv_draw_buf_dup_ex: create_ex sizes the buffer
     *from w/h/cf/stride which may differ from the descriptor's data_size (indexed+palette,
     *RGB565A8 two planes, legacy stride). NeMa renders the format natively, no conversion.*/
    uint32_t len = LV_MIN(image->data_size, decoded->data_size);
    if(!emmc_read_cb(offset, decoded->data, len)) {
        LV_LOG_ERROR("eMMC read failed at offset %" LV_PRIu32, offset);
        lv_draw_buf_destroy(decoded);
        return LV_RESULT_INVALID;
    }

    if(image->header.flags & LV_IMAGE_FLAGS_PREMULTIPLIED) {
        lv_draw_buf_set_flag(decoded, LV_IMAGE_FLAGS_PREMULTIPLIED);
    }

    dsc->decoded = decoded;

    lv_draw_buf_t * adjusted = lv_image_decoder_post_process(dsc, decoded);
    if(adjusted == NULL) {
        lv_draw_buf_destroy(decoded);
        return LV_RESULT_INVALID;
    }
    if(adjusted != decoded) {
        /*post_process allocated a new buffer (also from the image pool); drop the original*/
        lv_draw_buf_destroy(decoded);
        decoded = adjusted;
    }
    dsc->decoded = decoded;

    /*Copy user flags to the decoded image*/
    if(image->header.flags & LV_IMAGE_FLAGS_USER_MASK) {
        lv_draw_buf_set_flag(decoded, image->header.flags & LV_IMAGE_FLAGS_USER_MASK);
    }

    /*If caching is off, keep the buffer and free it on close*/
    if(dsc->args.no_cache || !lv_image_cache_is_enabled()) {
        dsc->user_data = decoded;
        return LV_RESULT_OK;
    }

    /*Hand the buffer to the image cache (it is freed via lv_draw_buf_destroy on eviction)*/
    lv_image_cache_data_t search_key;
    search_key.src_type = dsc->src_type;
    search_key.src = dsc->src;
    search_key.slot.size = decoded->data_size;

    lv_cache_entry_t * cache_entry = lv_image_decoder_add_to_cache(decoder, &search_key, decoded, NULL);
    if(cache_entry == NULL) {
        lv_draw_buf_destroy(decoded);
        return LV_RESULT_INVALID;
    }

    dsc->cache_entry = cache_entry;
    dsc->user_data = NULL; /*Cache owns the buffer now*/
    return LV_RESULT_OK;
}

void lv_emmc_decoder_close(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc)
{
    LV_UNUSED(decoder);

    /*Only set when the image was not handed to the cache (no_cache / cache disabled)*/
    if(dsc->user_data) {
        lv_draw_buf_destroy(dsc->user_data);
        dsc->user_data = NULL;
    }
}

#endif /*LV_USE_EMMC_DECODER*/

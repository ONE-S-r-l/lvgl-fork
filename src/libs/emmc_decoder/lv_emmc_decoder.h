/**
 * @file lv_emmc_decoder.h
 *
 * Image decoder that fetches image payloads from a raw (filesystem-less) block
 * device (eMMC/SD) into the image cache pool.
 *
 * Assets are referenced by their linker address (LV_IMAGE_SRC_VARIABLE): the
 * lv_image_dsc_t descriptors stay in memory-mapped flash, while the pixel payloads
 * (the `*_map` arrays) live on the block device as a flat binary blob. Each payload's
 * address inside the dedicated linker section equals its byte offset in the blob, so
 * no table-of-contents is needed: offset = data - assets_start.
 *
 * The decoder claims only variable sources whose `data` pointer falls inside the
 * registered asset window; everything else is declined and handled by the regular
 * decoders. Because NeMa GFX renders every format natively, the payload is read raw
 * into RAM with no pixel conversion.
 */

#ifndef LV_EMMC_DECODER_H
#define LV_EMMC_DECODER_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../../lv_conf_internal.h"

#if LV_USE_EMMC_DECODER

#include "../../draw/lv_image_decoder.h"

/**********************
 *      TYPEDEFS
 **********************/

/**
 * Board-provided block-device reader.
 * @param byte_offset  offset of the payload inside the asset blob (relative to its base)
 * @param dst          destination buffer (allocated from the image cache pool)
 * @param len          number of bytes to read
 * @return             true on success
 *
 * The board implementation adds the eMMC base block, issues the block read
 * (e.g. HAL_MMC_ReadBlocks_DMA), handles the unaligned tail block, and INVALIDATES
 * the CPU D-cache for [dst, dst+len) after the transfer (the DMA fills the cacheable
 * pool buffer behind the CPU).
 */
typedef bool (*lv_emmc_read_cb_t)(uint32_t byte_offset, void * dst, uint32_t len);

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Initialize the eMMC image decoder module. Call from lv_init() AFTER
 * lv_bin_decoder_init() so this decoder is consulted first for variable sources.
 */
void lv_emmc_decoder_init(void);

/**
 * Register the block reader and the asset address window. Must be called before any
 * eMMC-backed image is drawn (e.g. right after lv_init()).
 * @param read_cb       block-device reader (see lv_emmc_read_cb_t)
 * @param assets_start  start of the asset section (linker symbol __assets_start)
 * @param assets_end    end of the asset section   (linker symbol __assets_end)
 */
void lv_emmc_decoder_set_source(lv_emmc_read_cb_t read_cb, const void * assets_start,
                                const void * assets_end);

lv_result_t lv_emmc_decoder_info(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc,
                                 lv_image_header_t * header);
lv_result_t lv_emmc_decoder_open(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc);
void lv_emmc_decoder_close(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc);

#endif /*LV_USE_EMMC_DECODER*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_EMMC_DECODER_H*/

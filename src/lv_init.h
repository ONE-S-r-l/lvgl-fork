/**
 * @file lv_init.h
 *
 */

#ifndef LV_INIT_H
#define LV_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lv_conf_internal.h"
#include "misc/lv_types.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

 struct lv_init_config {
    bool init_g2d;
 };

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Initialize LVGL library.
 * Should be called before any other LVGL related function.
 */
void lv_init(const struct lv_init_config *config);

/**
 * Deinit the 'lv' library
 */
void lv_deinit();

/**
 * Returns whether the 'lv' library is currently initialized
 */
bool lv_is_initialized(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_INIT_H*/

#if LV_BUILD_TEST
#include <stdio.h>
#include "../../lvgl.h"

int main(void)
{
    struct lv_init_config config = { .init_g2d = false };
    lv_init(&config);
    return 0;
}
#endif

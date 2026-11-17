/* config.c -- CONFIG/CMD entry point. */
#include "config_ui.h"

extern void trs80_nio_init(void);

int main(void)
{
    trs80_nio_init();
    return config_ui_run();
}

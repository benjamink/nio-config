/* nio_selftest.c -- lists sd0:/ through the portable fnsvc code, printing "D name size" or
 * "F name size" per entry through @DSPLY. Checked by the boot repo's emulator test and MAME. */
#include "fnsvc.h"

extern void trs80_nio_init(void);

static char line[100];


static void show(void)
{
    __asm
    ld      hl,_line
    ld      a,10            ; @DSPLY
    rst     0x28
    __endasm;
}

static void on_entry(uint8_t is_dir, const char *name, uint32_t size, uint32_t mtime, void *ctx)
{
    char *p = line;
    uint16_t kb = (uint16_t) (size / 1024);
    (void) mtime; (void) ctx;
    *p++ = is_dir ? 'D' : 'F';
    *p++ = ' ';
    while (*name && p < line + 60)
        *p++ = *name++;
    *p++ = ' ';
    if (kb >= 100) *p++ = '0' + kb / 100 % 10;
    if (kb >= 10) *p++ = '0' + kb / 10 % 10;
    *p++ = '0' + kb % 10;
    *p++ = 'K';
    *p++ = 13;
    *p = 0;
    show();
}

int main(void)
{
    trs80_nio_init();
    if (!fnsvc_list_directory_flags("sd0:/", on_entry, 0)) {
        line[0] = 'E'; line[1] = '0' + fnsvc_last_error(); line[2] = 13; line[3] = 0;
        show();
        return 1;
    }
    line[0] = 'O'; line[1] = 'K'; line[2] = 13; line[3] = 0;
    show();
    return 0;
}

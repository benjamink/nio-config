/* keyprobe.c -- prints the code LS-DOS @KEY returns for each key, as "K=xx". Press Q (0x51) to quit.
 * Used once to fill trs80_keys.h; kept so the table can be re-checked on real hardware. */
static char line[] = "K=00\r";
static const char hex[] = "0123456789ABCDEF";

static unsigned char getkey(void)
{
    __asm
    ld      a,1             ; @KEY: wait for a key, A = its code
    rst     0x28
    ld      l,a
    ld      h,0
    __endasm;
}

static void show(void)
{
    __asm
    ld      hl,_line
    ld      a,10            ; @DSPLY
    rst     0x28
    __endasm;
}

int main(void)
{
    unsigned char k;
    do {
        k = getkey();
        line[2] = hex[k >> 4];
        line[3] = hex[k & 15];
        show();
    } while (k != 0x51);
    return 0;
}

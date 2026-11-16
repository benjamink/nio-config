/* hello.c -- toolchain proof: print through the LS-DOS @DSPLY SVC and return to LS-DOS. */
static const char msg[] = "hello from C\r";

int main(void)
{
    __asm
    ld      hl,_msg
    ld      a,10            ; @DSPLY
    rst     0x28
    __endasm;
    return 0;
}

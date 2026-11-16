; crt0.asm -- LS-DOS /CMD start-up for z88dk C programs (loaded at 3000h; HL = command line on entry).
; main() returns an int; it becomes the @EXIT return code.
        MODULE  crt0
        EXTERN  _main
        EXTERN  l_dcal
        PUBLIC  ls_exit
        PUBLIC  _cmdline
        SECTION CODE
start:
        ld      (_cmdline),hl
        ld      (saved_sp),sp
        call    _main
ls_exit:
        ld      sp,(saved_sp)
        ld      a,22            ; @EXIT, HL = return code
        rst     0x28
hang:   jr      hang
        SECTION data_compiler
_cmdline:  defw 0
saved_sp:  defw 0

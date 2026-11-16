; crt0.asm -- LS-DOS /CMD start-up for z88dk C programs (loaded at 3000h; HL = command line on entry).
; main() returns an int; it becomes the @EXIT return code.
; Section order puts bss last so the /CMD can stop before it (trimbss.sh); bss is cleared here.
        MODULE  crt0
        EXTERN  _main
        EXTERN  __bss_compiler_head, __tail
        PUBLIC  _cmdline
        PUBLIC  ls_exit
        ; Everything that is stored in the /CMD first, then every bss section (trimbss.sh checks this).
        SECTION CODE
        SECTION code_compiler
        SECTION code_clib
        SECTION code_string
        SECTION code_l_sccz80
        SECTION code_math
        SECTION code_error
        SECTION code_stdlib
        SECTION code_ctype
        SECTION rodata_compiler
        SECTION rodata_clib
        SECTION data_compiler
        SECTION data_clib
        SECTION bss_compiler
        SECTION bss_clib
        SECTION bss_error
        SECTION bss_stdlib

        SECTION CODE
start:
        ld      (_cmdline),hl
        ld      (saved_sp),sp
        ld      sp,0xD000       ; LS-DOS enters on a small system stack (SP is near 02DCh); C needs room. Grows down from the client at D000h
        ld      hl,__tail
        ld      de,__bss_compiler_head
        or      a
        sbc     hl,de           ; HL = bytes of bss (all bss sections follow bss_compiler)
        ld      b,h
        ld      c,l
        ld      a,b
        or      c
        jr      z,nobss
        ld      h,d
        ld      l,e
        ld      (hl),0
        dec     bc
        ld      a,b
        or      c
        jr      z,nobss
        inc     de
        ldir
nobss:
        call    _main
ls_exit:
        ld      sp,(saved_sp)
        ld      a,22            ; @EXIT, HL = return code
        rst     0x28
hang:   jr      hang

        SECTION data_compiler
_cmdline:  defw 0
saved_sp:  defw 0

; trs80_console_asm.asm -- LS-DOS SVC wrappers for trs80_console.c. Values are passed in the
; globals below (no C stack arguments), IX and IY are saved because SVCs may use them.
        SECTION code_compiler
        PUBLIC  _con_r, _con_c, _con_ch, _con_cmdp
        PUBLIC  _dos_drive_enabled_raw, _dos_exec_raw
        PUBLIC  _con_putc_raw, _con_cursor_raw, _con_key_raw

_con_putc_raw:                  ; @VDCTL B=2: character _con_ch at (_con_r,_con_c)
        push    ix
        push    iy
        ld      a,(_con_ch)
        ld      c,a
        ld      a,(_con_r)
        ld      h,a
        ld      a,(_con_c)
        ld      l,a
        ld      b,2
        ld      a,15
        rst     0x28
        pop     iy
        pop     ix
        ret

_con_cursor_raw:                ; @VDCTL B=3: move the cursor to (_con_r,_con_c)
        push    ix
        push    iy
        ld      a,(_con_r)
        ld      h,a
        ld      a,(_con_c)
        ld      l,a
        ld      b,3
        ld      a,15
        rst     0x28
        pop     iy
        pop     ix
        ret

_con_key_raw:                   ; @KEY: wait for a key; returns it in HL
        push    ix
        push    iy
        ld      a,1
        rst     0x28
        pop     iy
        pop     ix
        ld      l,a
        ld      h,0
        ret

_dos_drive_enabled_raw:         ; @GTDCT for the drive in _con_c; HL = 1 when its DCT is not the disabled stub (RET, C9h)
        push    ix
        push    iy
        ld      a,(_con_c)
        ld      c,a
        ld      a,81
        rst     0x28
        ld      a,(iy+0)
        pop     iy
        pop     ix
        cp      0xC9
        ld      hl,1
        ret     nz
        dec     l
        ret

_dos_exec_raw:                  ; @CMNDI: leave CONFIG and run the CR-terminated command at (_con_cmdp)
        ld      hl,(_con_cmdp)
        ld      a,24
        rst     0x28
        ret

        SECTION data_compiler
_con_cmdp: defw 0
_con_r:  defb 0
_con_c:  defb 0
_con_ch: defb 0

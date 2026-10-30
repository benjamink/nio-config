; Resident gates: BASIC is off, OS ROM on, ANTIC always uses main RAM.
        .export _bank_select_main, _bank_select_setup
        .export _setup_refresh, _draw_setup, _key_setup
        .import _setup_refresh_impl, _draw_setup_impl, _key_setup_impl
        .segment "COMMON_HIGH"
_bank_select_setup:
        lda #$E7
        sta $D301
        rts
_bank_select_main:
        lda #$E3
        sta $D301
        rts
_setup_refresh:
        jsr _bank_select_setup
        jsr _setup_refresh_impl
        jmp _bank_select_main
_draw_setup:
        jsr _bank_select_setup
        jsr _draw_setup_impl
        jmp _bank_select_main
_key_setup:
        pha
        jsr _bank_select_setup
        pla
        jsr _key_setup_impl
        jmp _bank_select_main

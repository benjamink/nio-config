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

; Browse executes in the main bank, so these gates and their zero-page
; pointers remain resident while the cache bank is selected. ANTIC remains
; mapped to main RAM and interrupts execute resident code.
        .exportzp _bc_addr, _bc_ptr, _bc_len
        .export _bank_cache_read, _bank_cache_write
        .segment "ZEROPAGE"
_bc_addr: .res 2
_bc_ptr:  .res 2
_bc_len:  .res 2
        .segment "COMMON_HIGH"
_bank_cache_read:
        jsr _bank_select_setup
        ldy #0
@loop:  lda _bc_len
        ora _bc_len+1
        beq @done
        lda (_bc_addr),y
        sta (_bc_ptr),y
        iny
        bne :+
        inc _bc_addr+1
        inc _bc_ptr+1
:       lda _bc_len
        bne :+
        dec _bc_len+1
:       dec _bc_len
        jmp @loop
@done:  jmp _bank_select_main
_bank_cache_write:
        jsr _bank_select_setup
        ldy #0
@loop:  lda _bc_len
        ora _bc_len+1
        beq @done
        lda (_bc_ptr),y
        sta (_bc_addr),y
        iny
        bne :+
        inc _bc_addr+1
        inc _bc_ptr+1
:       lda _bc_len
        bne :+
        dec _bc_len+1
:       dec _bc_len
        jmp @loop
@done:  jmp _bank_select_main

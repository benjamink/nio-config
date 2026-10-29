; Compose host, path and leaf without cc65's multi-argument stack marshalling.
; Output is capped at compose_cap including the terminator. Zero-page
; arguments and scratch are used only synchronously on the main thread.
        .exportzp _compose_out, _compose_host, _compose_path, _compose_leaf, _compose_cap
        .export _compose_run
        .segment "ZEROPAGE"
_compose_out:  .res 2
_compose_host: .res 2
_compose_path: .res 2
_compose_leaf: .res 2
_compose_cap:  .res 1
src:           .res 2
        .segment "RODATA"
prefix: .asciiz "tnfs://"
        .segment "CODE"
_compose_run:
        ldx #0
        ldy #0
        txa
        sta (_compose_out),y
        lda (_compose_host),y
        beq fail
        ; A colon before any slash/backslash identifies an existing scheme.
@scheme:
        lda (_compose_host),y
        beq @prefix
        cmp #':'
        beq @host
        cmp #'/'
        beq @prefix
        cmp #$5c
        beq @prefix
        iny
        bne @scheme
@prefix:
        lda #<prefix
        sta src
        lda #>prefix
        sta src+1
        jsr append
        bcc fail
@host:  lda _compose_host
        sta src
        lda _compose_host+1
        sta src+1
        jsr append
        bcc fail
        jsr slash
        bcc fail
        lda _compose_path
        sta src
        lda _compose_path+1
        sta src+1
        jsr append
        bcc fail
        ldy #0
        lda (_compose_leaf),y
        beq success
        jsr slash
        bcc fail
        lda _compose_leaf
        sta src
        lda _compose_leaf+1
        sta src+1
        jsr append
        bcc fail
success:
        lda #1
        ldx #0
        rts
fail:   lda #0
        tax
        rts

; X is current output length; append NUL-terminated src, terminating output.
append: ldy #0
@loop:  lda (src),y
        beq @end
        pha
        txa
        clc
        adc #1
        cmp _compose_cap
        bcs @full
        sty save_y
        txa
        tay
        pla
        sta (_compose_out),y
        inx
        ldy save_y
        iny
        bne @loop
@end:   txa
        tay
        lda #0
        sta (_compose_out),y
        sec
        rts
@full:  pla
        clc
        rts

; Append a slash if output is nonempty and does not already end with one.
slash:  cpx #0
        beq @ok
        txa
        tay
        dey
        lda (_compose_out),y
        cmp #'/'
        beq @ok
        txa
        clc
        adc #1
        cmp _compose_cap
        bcs @full
        txa
        tay
        lda #'/'
        sta (_compose_out),y
        inx
        iny
        lda #0
        sta (_compose_out),y
@ok:    sec
        rts
@full:  clc
        rts
        .segment "BSS"
save_y: .res 1

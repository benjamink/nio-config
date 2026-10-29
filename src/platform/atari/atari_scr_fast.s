; Screen text primitives with no C parameter passing: the caller (macros in
; atari_scr.h) stores its arguments straight into zero page and calls the
; routine, which writes ANTIC screen codes directly through a row address table.

        .export _scr_text_run, _scr_fill_run, _scr_u16_run, _scr_dec16_run
        .export _scr_theme_run
        .export _help_rows_run
        .exportzp _scr_zr, _scr_zc, _scr_zw, _scr_zinv, _scr_zch, _scr_zs, _scr_zv
        .import _scr_rowlo, _scr_rowhi
        .import _palette, _nxt_bg, _nxt_fg

        .segment "ZEROPAGE"
_scr_zr:   .res 1               ; row
_scr_zc:   .res 1               ; column
_scr_zw:   .res 1               ; width (cells)
_scr_zinv: .res 1               ; $80 draws the text inverse, 0 normal
_scr_zch:  .res 1               ; fill character
_scr_zs:   .res 2               ; source string
_scr_zv:   .res 2               ; 16-bit value to print
zd:        .res 2               ; destination cell
zt:        .res 1               ; scratch
zf:        .res 1
zn:        .res 1

        .segment "CODE"

; Row colour selection uses the same zero-page calling convention as drawing.
_scr_theme_run:
        lda     _scr_zch
        asl     a
        tax
        ldy     _scr_zr
        lda     _palette,x
        sta     _nxt_bg,y
        lda     _palette+1,x
        sta     _nxt_fg,y
        rts

; Draw packed help rows separated by '|', into the back screen.
_help_rows_run:
        lda #2
        sta _scr_zc
@row:   jsr setdst
        ldy #0
@char:  lda (_scr_zs),y
        beq @done
        cmp #'|'
        beq @next
        jsr conv
        sta (zd),y
        iny
        cpy #36
        bcc @char
        ; Skip any excess text until the next row delimiter.
@skip:  lda (_scr_zs),y
        beq @done
        cmp #'|'
        beq @next
        iny
        bne @skip
@next:  iny
        tya
        clc
        adc _scr_zs
        sta _scr_zs
        bcc :+
        inc _scr_zs+1
:       inc _scr_zr
        jmp @row
@done:  rts

; zd = address of (row, column)
setdst:
        ldx     _scr_zr
        lda     _scr_rowlo,x
        clc
        adc     _scr_zc
        sta     zd
        lda     _scr_rowhi,x
        adc     #0
        sta     zd+1
        rts

; ASCII (bit 7: inverse) -> screen code.  Characters 1..31 are the ATASCII
; graphics, which sit at screen codes 64..95.  Result in A; X, Y unchanged.
conv:
        sta     zt
        and     #$7F
        cmp     #$20
        bcc     @gfx
        cmp     #$60
        bcs     @done
        sec
        sbc     #$20
        bcs     @done           ; always
@gfx:   clc
        adc     #$40
@done:  sta     _scr_zch        ; (reused as scratch by the callers: they reload it)
        lda     zt
        and     #$80
        ora     _scr_zch
        rts

; void scr_text_run(void): draw _scr_zs (NUL terminated) in _scr_zw cells at
; (_scr_zr, _scr_zc), blank-padded; _scr_zinv inverts every cell.
_scr_text_run:
        jsr     setdst
        ldy     #0
        lda     _scr_zw
        beq     @out
@next:  lda     (_scr_zs),y
        beq     @pad
        jsr     conv
        eor     _scr_zinv
        sta     (zd),y
        iny
        cpy     _scr_zw
        bne     @next
@out:   rts
@pad:   lda     _scr_zinv       ; a blank is screen code 0
@fill:  sta     (zd),y
        iny
        cpy     _scr_zw
        bne     @fill
        rts

; void scr_fill_run(void): _scr_zw cells of character _scr_zch (ASCII, as for text).
_scr_fill_run:
        jsr     setdst
        lda     _scr_zch
        jsr     conv
        ldy     _scr_zw
        beq     @out
@loop:  dey
        sta     (zd),y
        bne     @loop
@out:   rts

; digits16: _scr_zv -> digit[0..4] (most significant first); X = Y = trashed.
digits16:
        ldx     #0
@pos:   ldy     #0              ; Y counts the subtractions of this power
@sub:   lda     _scr_zv
        sec
        sbc     pw_lo,x
        sta     zt
        lda     _scr_zv+1
        sbc     pw_hi,x
        bcc     @gotit          ; below zero: keep the remainder
        sta     _scr_zv+1
        lda     zt
        sta     _scr_zv
        iny
        bne     @sub            ; always
@gotit: tya
        sta     digit,x
        inx
        cpx     #5
        bne     @pos
        rts

; first significant digit index into zf (the last digit always counts)
firstdig:
        ldx     #0
@lead:  lda     digit,x
        bne     @have
        inx
        cpx     #4
        bne     @lead
@have:  stx     zf
        rts

; void scr_u16_run(void): _scr_zv in decimal, right aligned in _scr_zw cells
; (blank padded; high digits are dropped if it does not fit).
_scr_u16_run:
        jsr     digits16
        jsr     firstdig
        lda     #5
        sec
        sbc     zf
        sta     zn              ; digits to print
        cmp     _scr_zw
        bcc     @fit
        beq     @fit
        sec                     ; too wide: drop the high digits
        sbc     _scr_zw
        clc
        adc     zf
        sta     zf
        lda     _scr_zw
        sta     zn
@fit:   jsr     setdst
        ldy     #0
        lda     _scr_zw
        sec
        sbc     zn
        beq     @digits
        tax                     ; blanks first
        lda     #0
@blank: sta     (zd),y
        iny
        dex
        bne     @blank
@digits:ldx     zf
        cpx     #5
        beq     @end
@dig:   lda     digit,x
        clc
        adc     #$10            ; screen code of '0'
        sta     (zd),y
        iny
        inx
        cpx     #5
        bne     @dig
@end:   rts

; void scr_dec16_run(void): _scr_zv as ASCII digits written at _scr_zs, with at
; least _scr_zw digits (leading zeros), which advances _scr_zs past them.
_scr_dec16_run:
        jsr     digits16
        jsr     firstdig
        lda     #5
        sec
        sbc     _scr_zw         ; first digit to show for that minimum width
        bcc     @keep           ; (width over 5: show all five)
        cmp     zf
        bcs     @keep           ; the number is already that wide or wider
        sta     zf
@keep:  ldx     zf
        ldy     #0
@out:   lda     digit,x
        clc
        adc     #'0'
        sta     (_scr_zs),y
        iny
        inx
        cpx     #5
        bne     @out
        tya                     ; advance the destination
        clc
        adc     _scr_zs
        sta     _scr_zs
        bcc     @done
        inc     _scr_zs+1
@done:  rts

        .segment "BSS"
digit:  .res 5

        .segment "RODATA"
pw_lo:  .byte <10000, <1000, <100, <10, <1
pw_hi:  .byte >10000, >1000, >100, >10, >1

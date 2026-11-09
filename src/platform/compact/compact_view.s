; Clamp the selection and the top row for an 18-row list. Arguments are in
; zero page to avoid cc65's pointer and stack marshalling at every call.
        .exportzp _view_pos, _view_first, _view_total, _view_key
        .export _view_run, _view_move, _base_name
        .segment "ZEROPAGE"
_view_pos:   .res 2
_view_first: .res 2
_view_total: .res 2
_view_key:   .res 1
vp:          .res 2
vf:          .res 2
limit:       .res 2
bp:          .res 2
last:        .res 2
        .segment "CODE"
; Last component of a NUL-terminated URI, retaining a trailing slash.
_base_name:
        sta bp
        sta last
        stx bp+1
        stx last+1
        ldy #0
@scan: lda (bp),y
        beq @end
        cmp #'/'
        bne @advance
        iny
        lda (bp),y
        beq @end
        tya
        clc
        adc bp
        sta last
        lda bp+1
        adc #0
        sta last+1
        dey
@advance:
        inc bp
        bne @scan
        inc bp+1
        bne @scan
@end:  lda last
        ldx last+1
        rts
_view_run:
        ldy #0
        lda (_view_pos),y
        sta vp
        lda (_view_first),y
        sta vf
        iny
        lda (_view_pos),y
        sta vp+1
        lda (_view_first),y
        sta vf+1
        lda _view_total
        ora _view_total+1
        bne @nonzero
        sta vp
        sta vp+1
        sta vf
        sta vf+1
        jmp @store
@nonzero:
        lda vp
        cmp _view_total
        lda vp+1
        sbc _view_total+1
        bcc @posok
        sec
        lda _view_total
        sbc #1
        sta vp
        lda _view_total+1
        sbc #0
        sta vp+1
@posok:
        lda vp
        cmp vf
        lda vp+1
        sbc vf+1
        bcs @firstok
        lda vp
        sta vf
        lda vp+1
        sta vf+1
@firstok:
        ; vp - vf >= 18 => first = vp - 17
        sec
        lda vp
        sbc vf
        sta limit
        lda vp+1
        sbc vf+1
        bne @scroll
        lda limit
        cmp #18
        bcc @clamp
@scroll:
        sec
        lda vp
        sbc #17
        sta vf
        lda vp+1
        sbc #0
        sta vf+1
@clamp:
        sec
        lda _view_total
        sbc #18
        sta limit
        lda _view_total+1
        sbc #0
        sta limit+1
        bcs @limit
        lda #0
        sta limit
        sta limit+1
@limit:
        lda limit
        cmp vf
        lda limit+1
        sbc vf+1
        bcs @store
        lda limit
        sta vf
        lda limit+1
        sta vf+1
@store:
        ldy #0
        lda vp
        sta (_view_pos),y
        lda vf
        sta (_view_first),y
        iny
        lda vp+1
        sta (_view_pos),y
        lda vf+1
        sta (_view_first),y
        rts

; Cursor movement: return 1 for a recognised key, even at an edge.
_view_move:
        lda _view_total
        ora _view_total+1
        beq @invalid
        ldy #0
        lda (_view_pos),y
        sta vp
        iny
        lda (_view_pos),y
        sta vp+1
        lda _view_key
        cmp #$1c
        beq @up
        cmp #'-'
        beq @up
        cmp #$1d
        beq @down
        cmp #'='
        beq @down
        cmp #' '
        beq @page_down
        cmp #'b'
        beq @page_up
@invalid:
        lda #0
        tax
        rts
@up:
        lda vp
        ora vp+1
        beq @save
        lda #1
        bne @subtract
@page_up:
        lda #18
@subtract:
        sta limit
        sec
        lda vp
        sbc limit
        sta vp
        lda vp+1
        sbc #0
        sta vp+1
        bcs @save
        lda #0
        sta vp
        sta vp+1
        beq @save
@down:
        lda #1
        bne @add
@page_down:
        lda #18
@add:
        clc
        adc vp
        sta vp
        lda vp+1
        adc #0
        sta vp+1
        lda vp
        cmp _view_total
        lda vp+1
        sbc _view_total+1
        bcc @save
        sec
        lda _view_total
        sbc #1
        sta vp
        lda _view_total+1
        sbc #0
        sta vp+1
@save:
        ldy #0
        lda vp
        sta (_view_pos),y
        iny
        lda vp+1
        sta (_view_pos),y
        lda #1
        ldx #0
        rts

; ANTIC mode 2 (40x24) text screen whose colours are set per row by a display
; list interrupt.  The row colour tables are C arrays (atari_scr.c); the
; interrupt at the end of each row loads the next row's colours, so every row
; (title bar, selection bar, status bar, dialog) can be a different colour.

        .export _scr_vectors_set, _scr_vectors_off, _scr_coldstart
        .import _row_bg, _row_fg

VDSLST  = $0200
SDLSTL  = $0230
NMIEN   = $D40E
WSYNC   = $D40A
COLPF1  = $D017
COLPF2  = $D018
COLBK   = $D01A
DLIST   = $0600
COLDST  = $0244
VVBLKD  = $0224
XITVBV  = $E462

        .segment "CODE"

; void scr_vectors_set(void): point ANTIC at the display list built at $0600
; and enable the display list interrupt.
_scr_vectors_set:
        lda     #0
        sta     dlirow
        lda     #<dli
        sta     VDSLST
        lda     #>dli
        sta     VDSLST+1
        sei
        lda     #<vbi
        sta     VVBLKD
        lda     #>vbi
        sta     VVBLKD+1
        cli
        lda     #<DLIST
        sta     SDLSTL
        lda     #>DLIST
        sta     SDLSTL+1
        lda     #$C0
        sta     NMIEN
        rts

; void scr_vectors_off(void)
_scr_vectors_off:
        lda     #$40
        sta     NMIEN
        rts

; void scr_coldstart(void): reboot, as the reset key does.
_scr_coldstart:
        sei
        lda     #$40
        sta     NMIEN
        lda     #1
        sta     COLDST
        jmp     ($FFFC)

; Deferred vertical blank: every frame starts the row count again, so a frame
; that lost an interrupt (or started with the interrupt just enabled) cannot
; leave the colours out of step with the rows.
vbi:    lda     #0
        sta     dlirow
        jmp     XITVBV

dli:    pha
        txa
        pha
        ldx     dlirow
        inx
        cpx     #24
        bcc     @store
        ldx     #0
        stx     dlirow
        ldx     #23             ; below the last row keeps its colours
        bne     @set
@store: stx     dlirow
@set:   lda     _row_fg,x
        pha
        lda     _row_bg,x
        tax
        pla
        sta     WSYNC
        sta     COLPF1
        stx     COLPF2
        stx     COLBK
        pla
        tax
        pla
        rti

        .segment "BSS"
dlirow: .res    1

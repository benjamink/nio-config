; ANTIC mode 2 (40x24) text screen whose colours are set per row by a display
; list interrupt.  The row colour tables are C arrays (atari_scr.c); the
; interrupt at the end of each row loads the next row's colours, so every row
; (title bar, selection bar, status bar, dialog) can be a different colour.

        .export _scr_vectors_set, _scr_vectors_off, _scr_coldstart, _scr_sio_hook
        .export _scr_flat_on, _scr_flat_off, _scr_reading_on, _scr_reading
        .export _scr_nmi_mode, setcursor
        .import _row_bg, _row_fg, _nxt_bg, _nxt_fg, _lms_lo, _lms_hi, _scr_swap
        .import _scr_sio_bg, _scr_sio_fg
        .import fn_sio_vec

VDSLST  = $0200
SDLSTL  = $0230
NMIEN   = $D40E
WSYNC   = $D40A
COLPF1  = $D017
COLPF2  = $D018
COLBK   = $D01A
DLIST   = $0600
COLDST  = $0244
COLOR1  = $02C5
COLOR2  = $02C6
COLOR4  = $02C8
DRKMSK  = $004E
COLRSH  = $004F
SIOV    = $E459
        .ifdef  NO_DLI
NMI_ON  = $40
        .else
NMI_ON  = $C0
        .endif
VVBLKD  = $0224
XITVBV  = $E462

        .segment "ZEROPAGE"
next_fg: .res   1
next_bg: .res   1
dlirow:  .res   1

        .segment "CODE"

; cgetc normally restores and updates the OS text cursor through OLDADR.
; This app uses its own double-buffered screen; OLDADR may point into the app
; after loading, so touching the unused OS cursor can corrupt program memory.
setcursor:
        rts

; void scr_vectors_set(void): point ANTIC at the display list built at $0600
; and enable the display list interrupt.
_scr_vectors_set:
        lda     #0
        sta     dlirow
        sta     _scr_reading
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
        lda     #NMI_ON
        sta     _scr_nmi_mode
        sta     NMIEN
        rts

_scr_flat_on:
        lda     #$40
        sta     _scr_nmi_mode
        sta     NMIEN
        rts

_scr_flat_off:
        lda     #NMI_ON
        sta     _scr_nmi_mode
        sta     NMIEN
        rts

; Present the current Browse screen in body colours while the next page is
; fetched and drawn. VBI reenables row colours with the next buffer swap.
        .segment "COMMON_HIGH"
_scr_reading_on:
        jsr     _scr_flat_on
        lda     _scr_sio_bg
        sta     COLOR2
        sta     COLOR4
        sta     COLPF2
        sta     COLBK
        lda     _scr_sio_fg
        sta     COLOR1
        sta     COLPF1
        lda     #1
        sta     _scr_reading
        rts
        .segment "CODE"

; void scr_sio_hook(void): route the library's SIO calls through sio_nodli.
; Call it after fn_init(), which installs the plain SIOV vector.
_scr_sio_hook:
        lda     #<(sio_nodli-1)
        sta     fn_sio_vec
        lda     #>(sio_nodli-1)
        sta     fn_sio_vec+1
        rts

; SIO with display list interrupts off.  The row-colour interrupt stalls the
; CPU on every row, which makes the OS serial routines miss bytes on a real
; Atari (the FujiNet answers 'device error'), so only vertical blank runs
; while a call is in flight.
sio_nodli:
        lda     #$40
        sta     NMIEN
        lda     _scr_nmi_mode
        cmp     #$40
        beq     @transfer       ; flat screen already has its permanent colours
        lda     _scr_sio_bg     ; with the row colours stopped the screen would
        sta     COLOR2          ; show whatever colour was loaded last (grey):
        sta     COLOR4          ; make it the body blue for the transfer
        lda     _scr_sio_fg
        sta     COLOR1
@transfer:
        jsr     SIOV
        sty     siotmp
        lda     _scr_nmi_mode
        cmp     #$40
        beq     @restore_nmi    ; keep the existing flat colours across calls
        lda     _row_bg         ; back to the first row's colours
        sta     COLOR2
        sta     COLOR4
        lda     _row_fg
        sta     COLOR1
        lda     _scr_nmi_mode
@restore_nmi:
        sta     NMIEN
        ldy     siotmp
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
; leave the colours out of step with the rows.  When scr_present() has asked
; (scr_swap), it also switches the display list to the freshly drawn screen and
; its row colours, so a redraw appears in one frame.
vbi:    lda     #1
        sta     dlirow
        lda     _scr_swap
        beq     @done
        ldx     #0              ; new screen rows into the display list
        ldy     #4
@lms:   lda     _lms_lo,x
        sta     DLIST,y
        iny
        lda     _lms_hi,x
        sta     DLIST,y
        iny
        iny
        inx
        cpx     #24
        bne     @lms
        ldx     #23             ; and the row colours with them
@col:   lda     _nxt_bg,x
        sta     _row_bg,x
        lda     _nxt_fg,x
        sta     _row_fg,x
        dex
        bpl     @col
        lda     _row_bg
        sta     COLOR2
        sta     COLOR4
        eor     COLRSH
        and     DRKMSK
        sta     COLPF2
        sta     COLBK
        lda     _row_fg
        sta     COLOR1
        eor     COLRSH
        and     DRKMSK
        sta     COLPF1
        lda     #0
        sta     _scr_swap
        lda     _scr_reading
        beq     @done
        lda     #0
        sta     _scr_reading
        lda     #NMI_ON
        sta     _scr_nmi_mode
        sta     NMIEN
@done:  ; Prepare row 1 after the OS updates the attract colours.
        ldx     #1
        jsr     prepare
        jmp     XITVBV

; The two colours are ready before the DLI fires. Reach WSYNC early and
; write both registers immediately after the next scanline boundary;
; prepare the following row only after the timing-sensitive writes.
dli:    pha
        txa
        pha
        lda     next_fg
        ldx     next_bg
        sta     WSYNC
        sta     WSYNC           ; keep the existing band boundary one line later
        sta     COLPF1
        stx     COLPF2
        stx     COLBK
        ldx     dlirow
        inx
        cpx     #24
        bcc     :+
        dex
:       stx     dlirow
        jsr     prepare
        pla
        tax
        pla
        rti

prepare:
        lda     _row_fg,x
        eor     COLRSH
        and     DRKMSK
        sta     next_fg
        lda     _row_bg,x
        eor     COLRSH
        and     DRKMSK
        sta     next_bg
        rts

        .segment "BSS"
siotmp: .res    1
_scr_nmi_mode: .res 1
_scr_reading:  .res 1

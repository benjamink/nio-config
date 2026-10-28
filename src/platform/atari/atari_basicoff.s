; First chunk of the executable: it runs as soon as it is loaded, before the
; main program is read, and switches the BASIC cartridge ROM out so the
; program may load into the RAM under it ($A000-$BFFF) and on up to $C000,
; which the OS would otherwise keep for its screen (this program draws its own).
; XL/XE only; the bit is not wired on a 400/800.  It also moves MEMTOP to the
; top of that RAM:
; cc65's startup puts the C stack at MEMTOP, which with BASIC in is below the
; end of this program.

        .export __BASICOFF__

PORTB   = $D301
MEMTOP  = $02E5

        .segment "BASICOFF"
__BASICOFF__:
        lda     PORTB
        ora     #$02
        sta     PORTB
        lda     #$00
        sta     MEMTOP
        lda     #$C0
        sta     MEMTOP+1
        rts

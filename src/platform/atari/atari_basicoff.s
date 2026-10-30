; First chunk of the executable: it runs as soon as it is loaded, before the
; main program is read.
;
;  * It switches the BASIC cartridge ROM out (PORTB bit 1) so the program may
;    load into the RAM under it ($A000-$BFFF).  XL/XE only; the bit is not wired
;    on a 400/800.
;  * It sets BASICF so BASIC stays out after RESET, and RAMTOP/RAMSIZ to $C0
;    so the OS sees the RAM; on RESET the OS rebuilds its text screen at the
;    top of that RAM ($BC40-$BFFF), which is why the program stops at $BC00.
;  * It points MEMTOP at the program's limit: cc65's startup puts the C stack
;    there, which with BASIC in is below the end of this program.
;  * It points DOSVEC at the program's start, so RESET (which runs DOSVEC after
;    the OS has restarted) starts the application again instead of dropping to
;    BASIC or the memo pad.

        .export __BASICOFF__
        .import start

PORTB   = $D301
RAMTOP  = $006A
DOSVEC  = $000A
RAMSIZ  = $02E4
MEMTOP  = $02E5
BASICF  = $03F8

        .segment "BASICOFF"
__BASICOFF__:
        lda     PORTB
        ora     #$02
        sta     PORTB
        lda     #$01
        sta     BASICF
        lda     #$C0
        sta     RAMTOP
        sta     RAMSIZ
        lda     #$00
        sta     MEMTOP
        lda     #$BC
        sta     MEMTOP+1
        .ifdef ATARI_BANKED
        lda     #<banked_reset
        .else
        lda     #<start
        .endif
        sta     DOSVEC
        .ifdef ATARI_BANKED
        lda     #>banked_reset
        .else
        lda     #>start
        .endif
        sta     DOSVEC+1
        rts

; The OS may restore PORTB on warm reset.  This hook is always resident and
; reselects the same first 130XE-compatible bank used by the boot selector.
        .ifdef ATARI_BANKED
        .segment "COMMON_HIGH"
banked_reset:
        lda     #$E3
        sta     PORTB
        jmp     start
        .endif

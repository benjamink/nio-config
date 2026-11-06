; Leave the app for the demo on the boot disk, as LOAD"FUJIDEMO",8,1 and RUN would.
;
; The app has BASIC's zero page, and the program loads over the app. So this runs from the
; cassette buffer, which a load from a drive doesn't touch, puts the KERNAL and BASIC back
; as a reset leaves them, and then uses BASIC's own LOAD and RUN steps. A file that isn't
; there ends as it would when typed: an error, then READY.

        .export _c64_run_demo

CPU_PORT   = $01
CURLIN     = $39                ; BASIC's current line: $FF in its high byte is direct mode
VARTAB     = $2D                ; BASIC's end of program
IERROR     = $0300              ; BASIC's error vector, with the code in X

LINKPRG    = $A533              ; rechain the program's lines
RUNC       = $A659              ; start of program, then CLR
NEWSTT     = $A7AE              ; run the next statement
INITV      = $E453              ; set BASIC's vectors
INITCZ     = $E3BF              ; set BASIC's zero page and start of program
SCRTCH     = $A644              ; NEW: an empty program, and CLR. Without it, the variable
                                ; and string pointers are still the app's after a failed load

CINT       = $FF81
IOINIT     = $FF84
RESTOR     = $FF8A
SETMSG     = $FF90
SETLFS     = $FFBA
SETNAM     = $FFBD
LOAD       = $FFD5

DRIVE      = 8                  ; the boot disk
CASSETTE_BUFFER = $033C
CASSETTE_BUFFER_SIZE = 192

        .code

_c64_run_demo:
        sei
        ldx     #runner_end - runner - 1
@copy:  lda     image,x
        sta     runner,x
        dex
        bpl     @copy
        jmp     runner

image:
        .org    CASSETTE_BUFFER
runner:
        ; Not any earlier: the app's own code reaches past $A000, under BASIC.
        lda     #$37
        sta     CPU_PORT
        ldx     #$FB
        txs
        jsr     IOINIT
        jsr     RESTOR
        jsr     CINT
        cli
        jsr     INITV
        jsr     INITCZ
        jsr     SCRTCH
        lda     #$FF
        sta     CURLIN + 1
        lda     #$80            ; show SEARCHING and LOADING
        jsr     SETMSG
        lda     #name_end - name
        ldx     #<name
        ldy     #>name
        jsr     SETNAM
        lda     #1
        ldx     #DRIVE
        ldy     #1              ; to the address in the file
        jsr     SETLFS
        lda     #0              ; load, not verify
        jsr     LOAD
        bcs     @failed
        stx     VARTAB
        sty     VARTAB + 1
        lda     #0              ; as RUN does: a program prints no KERNAL messages
        jsr     SETMSG
        jsr     LINKPRG
        jsr     RUNC
        jmp     NEWSTT
@failed:
        tax
        jmp     (IERROR)
name:   .byte   "FUJIDEMO"
name_end:
runner_end:
        .reloc

        .assert runner_end - runner <= CASSETTE_BUFFER_SIZE, error, "runner too big for the cassette buffer"

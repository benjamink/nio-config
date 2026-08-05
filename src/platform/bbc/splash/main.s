;
; splash.s
;
; Minimal BBC Micro custom Mode 5 splash-screen POC.
;
; Program:
;     load address: &6931
;     execution:    &6931
;
; Raw image:
;     filename:     SCREEN
;     address:      &7100
;     size:         3840 bytes
;     dimensions:   160 x 96, Mode 5 screen-memory order
;
; Build with ca65/ld65; see splash.cfg below.
;

        .setcpu "6502"

; ---------------------------------------------------------------------------
; MOS entry points
; ---------------------------------------------------------------------------

OSWRCH  = $FFEE
OSBYTE  = $FFF4
OSCLI   = $FFF7

OSBYTE_READ_KEY = 129

; ---------------------------------------------------------------------------
; Hardware
; ---------------------------------------------------------------------------

CRTC_ADDR = $FE00
CRTC_DATA = $FE01

ULA_CTRL  = $FE20
ULA_PAL   = $FE21
ULA_CTRL_SHADOW = $0248

; ---------------------------------------------------------------------------
; Custom screen layout
; ---------------------------------------------------------------------------

SCREEN_START       = $7100
SCREEN_SIZE        = $0F00       ; 3840 bytes
SCREEN_END         = SCREEN_START + SCREEN_SIZE
LOAD_BUFFER        = $5800
LOAD_BUFFER_END    = LOAD_BUFFER + SCREEN_SIZE

SCREEN_CHAR_ADDR   = SCREEN_START / 8

SCREEN_CRTC_HI     = >SCREEN_CHAR_ADDR
SCREEN_CRTC_LO     = <SCREEN_CHAR_ADDR

SCREEN_ROWS        = 12          ; 12 * 8 = 96 raster lines

        .assert SCREEN_END = $8000, error, "Screen must end at &8000"
        .assert LOAD_BUFFER_END <= $6931, error, "Load buffer overlaps splash"
        .assert SCREEN_CRTC_HI = $0E, error, "Unexpected CRTC high byte"
        .assert SCREEN_CRTC_LO = $20, error, "Unexpected CRTC low byte"

; ---------------------------------------------------------------------------
; Program
; ---------------------------------------------------------------------------

        .segment "CODE"

        .export _start

_start:
        ;
        ; Discard the RETURN/type-ahead used to launch *SPLASH before making
        ; any MOS calls that could otherwise consume it as the splash key.
        ;
        ; OSBYTE 15 flushes the keyboard input buffer when X is zero.
        ;
        lda #15
        ldx #0
        jsr OSBYTE

        ; Put the display on the splash geometry with an all-black palette
        ; before disk I/O. The image is loaded into temporary RAM, so neither
        ; disk I/O nor a partially loaded bitmap can affect the visible screen.
        jsr set_video_blank

        ; Load the already converted raw Mode 5 file into temporary RAM.
        ;
        ; OSCLI does not require the leading '*' used at the BASIC prompt.
        ldx #<load_command
        ldy #>load_command
        jsr OSCLI

        ; Discard any launch type-ahead while the display is still black. This
        ; is deliberately done before the final video setup, rather than
        ; flushing input after the custom display is active.
        lda #15
        ldx #0
        jsr OSBYTE

        ; MOS disk I/O and OSRDCH may have restored their own video registers.
        ; Reapply the blank custom state before copying the image.
        jsr set_video_blank

        ; Copy the complete image into the actual CRTC display memory while
        ; the palette is still black. The visible palette is installed only
        ; after every byte is in place.
        ldx #15
        ldy #0
@copy_page:
        lda LOAD_BUFFER,y
        sta SCREEN_START,y
        iny
        bne @copy_page
        inc @copy_page+2      ; source high byte
        inc @copy_page+5      ; destination high byte
        dex
        bne @copy_page

        jsr set_video

.ifdef CONFIG_CHAIN
        ; Keep the splash visible while the main CONFIG application loads.
        ldx #<confnio_load_command
        ldy #>confnio_load_command
        jsr OSCLI
        ; The application owns the full BBC heap, including the splash range.
        ; Restore the MOS screen before handing over so its startup writes are
        ; never rendered through the splash CRTC geometry.
        jsr restore_mode7
        jmp CONFNIO_START
.else
        ; Poll for a new key instead of calling blocking OSRDCH. OSRDCH enters
        ; the MOS input wait path, which assumes that MOS owns the display and
        ; can clear/redraw through the active screen geometry. OSBYTE 129 with
        ; a zero timeout only checks the keyboard buffer and returns carry set
        ; when no key is available, leaving the custom display untouched.
@wait_key:
        lda #OSBYTE_READ_KEY
        ldx #0                  ; no wait: poll only
        ldy #0
        jsr OSBYTE
        bcs @wait_key

        ;
        ; Restore a conventional MOS-controlled screen before returning.
        ;
        lda #22                 ; VDU 22
        jsr OSWRCH

        lda #7                  ; MODE 7
        jsr OSWRCH

        rts
.endif

; ---------------------------------------------------------------------------
; Install the custom palette, CRTC geometry, and Mode 5 ULA format.
; ---------------------------------------------------------------------------

set_video:
        php
        sei

        ; Palette byte:
        ;   high nibble = logical palette index
        ;   low bits    = physical colour EOR 7
        ldx #0
@set_palette:
        lda palette_values,x
        sta ULA_PAL
        inx
        cpx #16
        bne @set_palette

        ; Standard PAL timing with a 12-row visible window at SCREEN_START.
        ldx #0
@set_crtc:
        stx CRTC_ADDR
        lda crtc_values,x
        sta CRTC_DATA
        inx
        cpx #14
        bne @set_crtc

        lda #$C4                 ; standard Mode 5 ULA control value
        sta ULA_CTRL_SHADOW     ; keep MOS IRQ cursor flashing in this mode
        sta ULA_CTRL

        plp
        rts

set_video_blank:
        php
        sei

        ldx #0
@set_black_palette:
        lda black_palette,x
        sta ULA_PAL
        inx
        cpx #16
        bne @set_black_palette

        ldx #0
@set_crtc:
        stx CRTC_ADDR
        lda crtc_values,x
        sta CRTC_DATA
        inx
        cpx #14
        bne @set_crtc

        lda #$C4
        sta ULA_CTRL_SHADOW
        sta ULA_CTRL

        plp
        rts

restore_mode7:
        lda #22                 ; VDU 22
        jsr OSWRCH
        lda #7                  ; MODE 7
        jmp OSWRCH

; ---------------------------------------------------------------------------
; CRTC register values R0 through R13
; ---------------------------------------------------------------------------
;
; Standard PAL-style Mode 5 geometry:
;
; R0  horizontal total                 63
; R1  horizontal displayed             40 byte-columns
; R2  horizontal sync position         49
; R3  sync widths                      36
; R4  vertical total                   38
; R5  vertical total adjustment         0
; R6  vertical displayed               12 rows = 96 lines
; R7  vertical sync position           34
; R8  interlace/display mode            1
; R9  scanlines per character - 1       7
; R10 cursor start                     32, cursor disabled
; R11 cursor end                        0
; R12 display address high            &0E
; R13 display address low             &20
;
; Keeping R4/R5/R7 around normal PAL values means the frame timing remains
; normal; the undisplayed portion becomes border rather than shortening the
; actual video frame.
;

crtc_values:
        .byte 63
        .byte 40
        .byte 49
        .byte 36
        .byte 38
        .byte 0
        .byte SCREEN_ROWS
        .byte 34
        .byte 1
        .byte 7
        .byte 32
        .byte 0
        .byte SCREEN_CRTC_HI
        .byte SCREEN_CRTC_LO

; Complete Mode 5 palette command table. The generated solid bytes use:

palette_values:
        ; Logical 0: black, physical colour 0 EOR 7 = 7
        ; Logical 1: red,   physical colour 1 EOR 7 = 6
        ; Logical 2: yellow,physical colour 3 EOR 7 = 4
        ; Logical 3: white, physical colour 7 EOR 7 = 0

        ; The physical colours are.
        ; 0 black
        ; 1 red
        ; 2 green
        ; 3 yellow
        ; 4 blue
        ; 5 magenta
        ; 6 cyan
        ; 7 white

        ; To embed them into the table, EOR the appropriate colour with 7 for the final digit, and place the index digit at the start (0..F)

        ; ULA palette entries 0–3
        .byte $07             ; 0:  logical 0 -> black
        .byte $17             ; 1:  logical 0 -> black
        .byte $26             ; 2:  logical 1 -> red
        .byte $36             ; 3:  logical 1 -> red

        ; ULA palette entries 4–7
        .byte $47             ; 4:  logical 0 -> black
        .byte $57             ; 5:  logical 0 -> black
        .byte $66             ; 6:  logical 1 -> red
        .byte $76             ; 7:  logical 1 -> red

        ; ULA palette entries 8–11
        .byte $84             ; 8:  logical 2 -> yellow
        .byte $94             ; 9:  logical 2 -> yellow
        .byte $A0             ; 10: logical 3 -> white
        .byte $B0             ; 11: logical 3 -> white

        ; ULA palette entries 12–15
        .byte $C4             ; 12: logical 2 -> yellow
        .byte $D4             ; 13: logical 2 -> yellow
        .byte $E0             ; 14: logical 3 -> white
        .byte $F0             ; 15: logical 3 -> white

; This is all black because every right hand nibble is 7 (black)
black_palette:
        .byte $07, $17, $27, $37
        .byte $47, $57, $67, $77
        .byte $87, $97, $A7, $B7
        .byte $C7, $D7, $E7, $F7

; ---------------------------------------------------------------------------
; OSCLI command
; ---------------------------------------------------------------------------

load_command:
        .byte "LOAD SCREEN 5800", 13

confnio_load_command:
.ifdef CONFIG_MASTER
        .byte "LOAD CONFNIO 0E00", 13
CONFNIO_START = $0E00
.else
        .byte "LOAD CONFNIO 1900", 13
CONFNIO_START = $1900
.endif

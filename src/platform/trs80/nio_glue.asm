; nio_glue.asm -- C-callable access to the fujinet-nio client (nioclient, entry table at 8000h,
; shared with FMOUNT/FLS; see fujinet/z80/nioclient.asm in the boot repo).
; Values go through globals, not the C stack. Interrupts are off for the exchange (the UART has
; no FIFO) and are turned back on afterwards.
        SECTION code_compiler
        PUBLIC  _nio_dev, _nio_cmd, _nio_ptr, _nio_len
        PUBLIC  _nio_err, _nio_stat, _nio_rptr, _nio_rlen
        PUBLIC  _nio_request_raw, _nio_uart_init

NIO_REQUEST equ 0x8003

; in: _nio_dev, _nio_cmd, _nio_ptr (payload), _nio_len
; out: _nio_err = 0 ok, else 1 timeout 2 bad SLIP 3 bad length 4 bad checksum 5 unsupported;
;      on success _nio_stat = NIO status, _nio_rptr/_nio_rlen = response payload in the client's buffer
_nio_request_raw:
        push    ix
        push    iy
        ld      a,(_nio_dev)
        ld      b,0
        ld      hl,(_nio_len)
        ex      de,hl
        ld      hl,(_nio_ptr)
        ld      a,(_nio_cmd)
        ld      b,a
        ld      a,(_nio_dev)
        di
        call    NIO_REQUEST
        ei
        jr      c,failed
        ld      (_nio_stat),a
        ld      (_nio_rptr),hl
        ld      (_nio_rlen),de
        xor     a
        ld      (_nio_err),a
        jr      done
failed: ld      (_nio_err),a
done:   pop     iy
        pop     ix
        ret

; Same UART set-up as FMOUNT and friends: 19200 baud, 8 data bits, odd parity, 1 stop.
; The value written to E8h matters in MAME (0 breaks it).
_nio_uart_init:
        ld      a,0x64
        out     (0xEA),a
        ld      a,0xFF
        out     (0xE9),a
        out     (0xE8),a
        ret

        SECTION data_compiler
_nio_dev:  defb 0
_nio_cmd:  defb 0
_nio_ptr:  defw 0
_nio_len:  defw 0
_nio_err:  defb 0
_nio_stat: defb 0
_nio_rptr: defw 0
_nio_rlen: defw 0

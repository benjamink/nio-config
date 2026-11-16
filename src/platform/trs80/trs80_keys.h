#ifndef TRS80_KEYS_H
#define TRS80_KEYS_H

/* Codes LS-DOS 6.3.1 @KEY returns on the Model 4/4P, measured in MAME with trs80/keyprobe.c.
 * The arrows share codes with editing keys: Left is Backspace (08h) and Right is Tab (09h),
 * and there is no separate TAB or BACKSPACE key. CTRL-K is 0Bh, the same as Up, so it cannot be used. */
#define KEY_UP     0x0B
#define KEY_DOWN   0x0A
#define KEY_LEFT   0x08
#define KEY_BKSP   0x08
#define KEY_RIGHT  0x09
#define KEY_TAB    0x09
#define KEY_SUP    0x1B   /* SHIFT+Up */
#define KEY_SDOWN  0x1A   /* SHIFT+Down */
#define KEY_SLEFT  0x18   /* SHIFT+Left */
#define KEY_SRIGHT 0x19   /* SHIFT+Right */
#define KEY_ENTER  0x0D
#define KEY_BREAK  0x80
#define KEY_F1     0x81
#define KEY_F2     0x82
#define KEY_F3     0x83

#endif

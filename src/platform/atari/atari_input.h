#ifndef ATARI_INPUT_H
#define ATARI_INPUT_H

#include <stdint.h>

/* ATASCII control codes as the keyboard handler returns them. */
#define K_UP    0x1C
#define K_DOWN  0x1D
#define K_LEFT  0x1E
#define K_RIGHT 0x1F
#define K_ESC   0x1B
#define K_TAB   0x7F
#define K_BS    0x7E
#define K_RET   0x9B
#define K_HELP  0x11            /* input_key maps the OS HELPFG flag to this */
/* Not a key: START or OPTION held. */
#define K_BOOT  0x01

/* Wait until the console keys, stick and keyboard are idle. */
void input_settle(void);
/* While input_key() waits, call fn every `step` jiffies once nothing has been
 * pressed for `delay` jiffies (the animation hook).  A key, stick or console
 * press restarts the delay. */
void input_set_idle(void (*fn)(void), uint16_t delay, uint8_t step);
/* Block for a key press: keyboard, joystick, or console switch. */
uint8_t input_key(void);

#endif

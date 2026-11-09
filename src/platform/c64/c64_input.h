#ifndef C64_INPUT_H
#define C64_INPUT_H

#include <stdint.h>

/* What input_key() returns besides printable ASCII. None of them falls in the
 * printable range, so the app's text entry can tell them apart. */
#define K_UP    0x1C
#define K_DOWN  0x1D
#define K_LEFT  0x1E
#define K_RIGHT 0x1F
#define K_ESC   0x1B
#define K_TAB   0x09
#define K_BS    0x08
#define K_RET   0x0D
#define K_HELP  0x11
#define K_BOOT  0x01
#define K_DEMO  0x02

/* Wait until the stick and keyboard are idle. */
void input_settle(void);
/* While input_key() waits, call fn every `step` jiffies once nothing has been
 * pressed for `delay` jiffies (the animation hook). A press restarts the delay. */
void input_set_idle(void (*fn)(void), uint16_t delay, uint8_t step);
/* Block for a key press: keyboard or joystick in port 2. */
uint8_t input_key(void);

#endif

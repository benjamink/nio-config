#include "atari_input.h"

#include <conio.h>
#include <stdint.h>

#define RTCLOK  (*(volatile uint8_t *) 0x14)
#define PORTA   (*(volatile uint8_t *) 0xD300)
#define TRIG0   (*(volatile uint8_t *) 0xD010)
#define CONSOL  (*(volatile uint8_t *) 0xD01F)
#define CH      (*(volatile uint8_t *) 0x02FC)

#define REPEAT_DELAY 18
#define REPEAT_RATE  4

static uint8_t held;
static uint8_t held_since;
static uint8_t last_fire;

/* One sample of the joystick and console switches as a key, or 0. */
static uint8_t sample(void)
{
  uint8_t stick;
  uint8_t consol;

  consol = (uint8_t) ~CONSOL;
  if (consol & 0x04)
    return K_BOOT;              /* OPTION */
  if (consol & 0x01)
    return K_BOOT;              /* START */
  if (consol & 0x02)
    return K_TAB;               /* SELECT */
  if (TRIG0 == 0)
    return K_RET;
  stick = PORTA;
  if (!(stick & 0x01))
    return K_UP;
  if (!(stick & 0x02))
    return K_DOWN;
  if (!(stick & 0x04))
    return K_LEFT;
  if (!(stick & 0x08))
    return K_RIGHT;
  return 0;
}

void input_settle(void)
{
  while (sample())
    ;
  held = 0;
  CH = 0xFF;
}

uint8_t input_key(void)
{
  uint8_t k;
  uint8_t now;

  for (;;) {
    if (kbhit())
      return (uint8_t) cgetc();

    k = sample();
    now = RTCLOK;
    if (!k) {
      held = 0;
      continue;
    }
    if (k != held) {
      held = k;
      held_since = now;
      last_fire = 0;
      return k;
    }
    /* console keys and the trigger do not auto-repeat */
    if (k == K_BOOT || k == K_TAB || k == K_RET)
      continue;
    if ((uint8_t) (now - held_since) >= REPEAT_DELAY &&
        (uint8_t) (now - last_fire) >= REPEAT_RATE) {
      last_fire = now;
      return k;
    }
  }
}

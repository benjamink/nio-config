#include "c64_input.h"

#include <cbm.h>
#include <stdint.h>

#define JIFFY_MID (*(volatile uint8_t *) 0xA1)
#define JIFFY_LOW (*(volatile uint8_t *) 0xA2)
/* Joystick in port 2: active low; reading it needs no keyboard column selected,
 * which the KERNAL's scan leaves as it is, so only the stick's bits are used. */
#define CIA1_PRA (*(volatile uint8_t *) 0xDC00)

#define STICK_UP    0x01
#define STICK_DOWN  0x02
#define STICK_LEFT  0x04
#define STICK_RIGHT 0x08
#define STICK_FIRE  0x10

/* PETSCII from GETIN. */
#define PET_STOP   0x03
#define PET_RETURN 0x0D
#define PET_DOWN   0x11
#define PET_HOME   0x13
#define PET_DEL    0x14
#define PET_RIGHT  0x1D
#define PET_POUND  0x5C
#define PET_ARROW_LEFT 0x5F  /* the key beside 1: the C64's usual Escape */
#define PET_F1     0x85
#define PET_F3     0x86
#define PET_F5     0x87
#define PET_F7     0x88
#define PET_UP     0x91
#define PET_LEFT   0x9D
#define PET_C_AT   0xA4      /* C= and @: reads as an underscore in a URI */

#define REPEAT_DELAY 18
#define REPEAT_RATE  4

static void (*idle_fn)(void);
static uint16_t idle_delay;
static uint8_t idle_step;

static uint8_t held;
static uint8_t held_since;
static uint8_t last_fire;

static uint16_t jiffies(void)
{
  uint8_t hi;
  uint8_t lo;

  do {
    hi = JIFFY_MID;
    lo = JIFFY_LOW;
  } while (hi != JIFFY_MID);
  return (uint16_t) ((hi << 8) | lo);
}

void input_set_idle(void (*fn)(void), uint16_t delay, uint8_t step)
{
  idle_fn = fn;
  idle_delay = delay;
  idle_step = step;
}

/* One sample of the joystick as a key, or 0. */
static uint8_t sample(void)
{
  uint8_t stick;

  stick = (uint8_t) ~CIA1_PRA;
  if (stick & STICK_FIRE)
    return K_RET;
  if (stick & STICK_UP)
    return K_UP;
  if (stick & STICK_DOWN)
    return K_DOWN;
  if (stick & STICK_LEFT)
    return K_LEFT;
  if (stick & STICK_RIGHT)
    return K_RIGHT;
  return 0;
}

/* A key from GETIN as the app's code: ASCII, or a K_ code; 0 to ignore it. */
static uint8_t from_petscii(uint8_t pet)
{
  switch (pet) {
  case PET_STOP:
  case PET_ARROW_LEFT: return K_ESC;
  case PET_RETURN:     return K_RET;
  case PET_DEL:        return K_BS;
  case PET_UP:         return K_UP;
  case PET_DOWN:       return K_DOWN;
  case PET_LEFT:       return K_LEFT;
  case PET_RIGHT:      return K_RIGHT;
  case PET_HOME:
  case PET_F3:         return K_TAB;
  case PET_F1:         return K_BOOT;
  case PET_F5:         return K_DEMO;
  case PET_F7:         return K_HELP;
  case PET_POUND:      return '\\';
  case PET_C_AT:       return '_';
  }
  if (pet >= 0x41 && pet <= 0x5A)        /* unshifted: lower case on this character set */
    return (uint8_t) (pet + 0x20);
  if (pet >= 0xC1 && pet <= 0xDA)        /* shifted: capitals */
    return (uint8_t) (pet - 0x80);
  if (pet >= 0x20 && pet <= 0x5B)
    return pet;
  if (pet == 0x5D)
    return pet;
  return 0;
}

void input_settle(void)
{
  while (sample())
    ;
  while (cbm_k_getin())
    ;
  held = 0;
}

uint8_t input_key(void)
{
  uint8_t k;
  uint8_t now;
  uint16_t start;
  uint16_t last_tick;
  uint16_t t;

  start = jiffies();
  last_tick = start;
  for (;;) {
    k = cbm_k_getin();
    if (k) {
      k = from_petscii(k);
      if (k)
        return k;
    }

    k = sample();
    now = JIFFY_LOW;
    if (!k) {
      held = 0;
      if (idle_fn) {
        t = jiffies();
        if ((uint16_t) (t - start) >= idle_delay &&
            (uint16_t) (t - last_tick) >= idle_step) {
          last_tick = t;
          idle_fn();
        }
      }
      continue;
    }
    if (k != held) {
      held = k;
      held_since = now;
      last_fire = 0;
      return k;
    }
    /* the fire button does not auto-repeat */
    if (k == K_RET)
      continue;
    if ((uint8_t) (now - held_since) >= REPEAT_DELAY &&
        (uint8_t) (now - last_fire) >= REPEAT_RATE) {
      last_fire = now;
      return k;
    }
  }
}

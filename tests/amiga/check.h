#ifndef AMIGA_TEST_CHECK_H
#define AMIGA_TEST_CHECK_H

#include <stdio.h>
#include <string.h>

extern int amiga_test_failures;
extern int amiga_test_checks;

#define CHECK(cond) do { \
  amiga_test_checks++; \
  if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    amiga_test_failures++; \
  } \
} while (0)

#define CHECK_STR(actual, expected) do { \
  const char *a_ = (actual); \
  const char *e_ = (expected); \
  amiga_test_checks++; \
  if (!a_ || strcmp(a_, e_) != 0) { \
    printf("FAIL %s:%d: %s == \"%s\" (got \"%s\")\n", __FILE__, __LINE__, \
           #actual, e_, a_ ? a_ : "(null)"); \
    amiga_test_failures++; \
  } \
} while (0)

#endif

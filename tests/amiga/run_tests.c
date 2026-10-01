/*
 * Host-side tests for the Amiga config-nio logic.  Run with
 * `make test-amiga-host` (all) or `make test-amiga-host ONLY=<name>`.
 */
#include "check.h"

int amiga_test_failures;
int amiga_test_checks;

#define AMIGA_TEST(name) void test_##name(void);
#include "tests.def"
#undef AMIGA_TEST

int main(int argc, char **argv)
{
  const char *only = argc > 1 && argv[1][0] ? argv[1] : NULL;

#define AMIGA_TEST(name) \
  if (!only || strcmp(only, #name) == 0) { \
    printf("-- %s\n", #name); \
    test_##name(); \
  }
#include "tests.def"
#undef AMIGA_TEST

  printf("amiga host tests: %d checks, %d failures\n",
         amiga_test_checks, amiga_test_failures);
  return amiga_test_failures ? 1 : 0;
}

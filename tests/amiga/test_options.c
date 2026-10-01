#include "check.h"
#include "amiga_options.h"

void test_options(void)
{
  amiga_options_t o;
  char longval[120];

  amiga_options_defaults(&o);
  CHECK_STR(o.fmount, "fmount");
  CHECK_STR(o.fumount, "fumount");
  CHECK_STR(o.script, "");
  CHECK_STR(o.result, "");

  CHECK(amiga_options_parse(&o, "fmount=DH0:C/fmount") == 1);
  CHECK_STR(o.fmount, "DH0:C/fmount");
  CHECK(amiga_options_parse(&o, "SCRIPT=NIO:accept.script") == 1);
  CHECK_STR(o.script, "NIO:accept.script");
  CHECK(amiga_options_parse(&o, "DONOTWAIT") == -1);
  CHECK(amiga_options_parse(&o, "(FUMOUNT=X:fumount)") == -1);
  CHECK_STR(o.fumount, "fumount");
  CHECK(amiga_options_parse(&o, "BOGUS=1") == -1);
  CHECK(amiga_options_parse(&o, "FMOUNT=") == 0);
  CHECK_STR(o.fmount, "DH0:C/fmount");

  memset(longval, 'x', sizeof(longval));
  memcpy(longval, "RESULT=", 7);
  longval[sizeof(longval) - 1] = 0;
  CHECK(amiga_options_parse(&o, longval) == 0);
  CHECK_STR(o.result, "");

  amiga_options_finish(&o);
  CHECK_STR(o.result, "RAM:config-nio.result");
  CHECK(amiga_options_parse(&o, "result=T:out") == 1);
  amiga_options_finish(&o);
  CHECK_STR(o.result, "T:out");
}

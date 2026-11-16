/* store_selftest.c -- host list, drive mappings and slot catalogue through the portable store
 * (config_nio_store.c) and the fujinet-nio-lib app-store/slot-catalogue calls. Prints one line
 * per check through @DSPLY; the boot repo's MAME test compares them. */
#include "config_nio.h"

#include <string.h>

extern void trs80_nio_init(void);

static config_nio_state_t st;
static char line[100];

static void show(void)
{
    __asm
    ld      hl,_line
    ld      a,10            ; @DSPLY
    rst     0x28
    __endasm;
}

static void say(const char *tag, const char *value)
{
    char *p = line;
    while (*tag) *p++ = *tag++;
    *p++ = '=';
    while (*value && p < line + 90) *p++ = *value++;
    *p++ = 13;
    *p = 0;
    show();
}

int main(void)
{
    config_nio_mapping_t m;
    config_nio_slot_t slot;
    char tmp[8];

    say("start", "");
    trs80_nio_init();
    memset(&st, 0, sizeof st);
    say("memset", "ok");
    config_nio_host_set(&st, 0, "sd0:/");
    config_nio_host_set(&st, 1, "tnfs://example.org/trs80");
    st.host_count = 2;
    m.valid = 1; m.slot = 2; m.readonly = 1;
    config_nio_mapping_set(&st, 1, &m);
    say("save_hosts", config_nio_save_hosts(&st) ? "ok" : "FAIL");
    say("save_maps", config_nio_save_mappings(&st) ? "ok" : "FAIL");
    say("write_slot", config_nio_write_slot(&st, 0, "host:/disks/work.dsk", "r") ? "ok" : "FAIL");

    memset(&st, 0, sizeof st);
    say("load", config_nio_load(&st) ? "ok" : "FAIL");
    say("host0", st.hosts[0]);
    say("host1", st.hosts[1]);
    config_nio_mapping_get(&st, 1, &m);
    tmp[0] = '0' + m.valid; tmp[1] = '0' + m.slot; tmp[2] = '0' + m.readonly; tmp[3] = 0;
    say("map1", tmp);
    memset(&slot, 0, sizeof slot);
    say("read_slot", config_nio_read_slot(0, &slot) ? "ok" : "FAIL");
    say("slot0", slot.uri);
    return 0;
}

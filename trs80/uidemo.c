/* uidemo.c -- exercises the console and widgets on the real LS-DOS: tab bar, a scrolling list
 * (SHIFT+Up/Down page), and a text field (A). BREAK quits. Used by the MAME test. */
#include "trs80_console.h"
#include "trs80_keys.h"
#include "ui_widgets.h"

static const char *const tabs[] = {"Hosts", "Browse", "Slots", "Setup"};
static char name[32];

static const char *item(uint16_t i, void *ctx)
{
    static char b[12] = "item 000";
    (void) ctx;
    b[5] = '0' + (i / 100) % 10;
    b[6] = '0' + (i / 10) % 10;
    b[7] = '0' + i % 10;
    return b;
}

int main(void)
{
    list_t l;
    int key;

    l.row = 3; l.col = 2; l.width = 40; l.height = 18;
    l.count = 50; l.sel = 0; l.top = 0; l.item = item; l.ctx = 0;
    con_clear();
    tabs_draw(1, tabs, 4, 1);
    for (;;) {
        list_draw(&l);
        msg_draw(23, name[0] ? name : "ready");
        key = con_getkey();
        if (key == KEY_BREAK)
            break;
        if (key == 'a' || key == 'A') {
            msg_draw(22, "Name:");
            if (field_edit(name, 30, 22, 8, 30))
                msg_draw(23, name);
            con_fill(22, 0, CON_COLS, ' ');
        } else {
            list_key(&l, key);
        }
    }
    con_clear();
    return 0;
}

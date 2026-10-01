#ifndef AMIGA_GUI_H
#define AMIGA_GUI_H

#include "amiga_ctl.h"
#include "amiga_options.h"

/* Opens the FujiNet Config window on the Workbench screen and runs until
 * the user quits (or the SCRIPT= file ends).  Returns the process rc. */
int amiga_gui_run(amiga_ctl_t *ctl, const amiga_options_t *opts);

/* Message requester (up to three lines separated by '\n'). */
void amiga_gui_fatal(const char *message);

#endif

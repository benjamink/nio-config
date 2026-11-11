#ifndef AMIGA_SPLASH_INCLUDED
#define AMIGA_SPLASH_INCLUDED

/* Startup splash: a small borderless window with the logo and a line of
 * status, shown while the program connects to the FujiNet and loads its
 * state, before the main window exists.  It is optional (SPLASH=OFF) and
 * never shown for SCRIPT runs.  Every call is safe when no splash is up. */

/* Shown at least this long (1/50 s) so a quick start is not just a flash. */
#define AMIGA_SPLASH_MIN_TICKS 50
/* Longest status line that fits under the logo. */
#define AMIGA_SPLASH_TEXT_MAX 24

void amiga_splash_open(const char *text);
void amiga_splash_text(const char *text);
/* Waits until the splash has been up for `ticks`, if it is up. */
void amiga_splash_wait(unsigned ticks);
void amiga_splash_close(void);

#endif

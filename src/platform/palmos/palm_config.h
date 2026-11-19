#ifndef PALM_CONFIG_H
#define PALM_CONFIG_H
#include "NioLib.h"

#define SetupForm 1000
#define HostsForm 1100
#define BrowseForm 1200
#define InfoForm 1300
#define EditForm 1400
#define WifiForm 1500
#define HostsTab 9001
#define InfoTab 9002
#define SetupTab 9003
#define MessageAlert 9100
#define InstallAlert 9101
#define ReplaceAlert 9102
#define ResetLinkAlert 9103

/* Retain the hardware-tested Palm sizes: larger replies regressed PRC reads
 * on Springboard despite the library's advertised packet capacity. Scratch
 * also holds directory pages; FileRead needs ten bytes beyond its data. */
#define PALM_TRANSFER_BYTES 256
#define PALM_SERVICE_BYTES 512

extern UInt16 libraryRef, nioResult, configFlags, serviceCapacity;
extern UInt32 clientToken;
extern Err palmError;
extern Boolean libraryOpen, clientOpen, dirty, servicesSupported;
extern NioLibConfig config;
extern NioLibTransportStatus transportStatus;
extern NioLibVersion libraryVersion;
extern const Char *message;

Boolean config_begin_services(void);
Boolean config_end_services(void);
void palm_message(const Char *text);
Boolean palm_page_event(EventType *event);
void palm_page_draw_tabs(UInt16 active);
Boolean palm_page_tab(EventType *event);
void palm_page_tick(void);
Boolean palm_join_pending(void);
void palm_page_stop(void);

#endif

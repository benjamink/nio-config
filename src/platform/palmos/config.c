/* Palm-only configuration front end. No protocol, transport or preferences
 * implementation: all settings and connection tests enter the installed ABI. */
#include "NioLib.h"

static UInt16 libraryRef;
static Boolean libraryLoaded, libraryOpen, clientOpen, dirty;
static UInt32 clientToken;
static Boolean usbSupported;
static NioLibConfig config;
static NioLibTransportStatus transportStatus;
static UInt16 configFlags, nioResult;
static Err palmError;
static const Char *message = "Opening NioLib...";

/* Kept separate from the form for compiled-code lifecycle/config tests. */
void config_stop(void)
{
    UInt16 remaining = 1;
    if (clientOpen) {
        NioLibCleanup cleanup;
        cleanup.size = sizeof(cleanup);
        palmError = NioLibEndClient(libraryRef, clientToken, &cleanup, &nioResult);
        if (palmError) return; /* Never remove a library still owning a session. */
        clientOpen = false;
    }
    if (libraryOpen) {
        palmError = NioLibClose(libraryRef, &remaining);
        if (palmError) return;
        libraryOpen = false;
    }
    if (libraryLoaded && !remaining && !SysLibTblEntry(libraryRef)->globalsP) {
        if (!SysLibRemove(libraryRef)) libraryLoaded = false;
    }
}

static Boolean valid_config(const NioLibConfig *p)
{
    return p->size == sizeof(*p) && p->version == nioConfigVersion &&
        (p->transport == nioTransportSpringboard || p->transport == nioTransportSerial ||
         (p->transport == nioTransportUSB && usbSupported)) &&
        !(p->serialOptions & ~nioSerialRTSCTS) &&
        (p->serialBaud == 9600 || p->serialBaud == 19200 || p->serialBaud == 38400 ||
         p->serialBaud == 57600 || p->serialBaud == 115200) &&
        !p->reserved[0] && !p->reserved[1] && !p->reserved[2];
}

static void status_read(void)
{
    transportStatus.size = sizeof(transportStatus);
    palmError = NioLibGetTransportStatus(libraryRef, &transportStatus);
}

void config_start(void)
{
    NioLibVersion version;
    UInt32 caps = 0;
    if (libraryOpen) return;
    palmError = SysLibFind(nioLibName, &libraryRef);
    if (palmError) {
        palmError = SysLibLoad(nioLibType, nioLibCreator, &libraryRef);
        if (palmError) { message = "Install NioLib first"; return; }
        libraryLoaded = true;
    }
    version.size = sizeof(version);
    palmError = NioLibGetVersion(libraryRef, &version);
    if (!palmError && (version.size != sizeof(version) || version.abiMajor != 1 ||
                       version.abiMinor < 2)) palmError = nioLibErrParam;
    if (!palmError) palmError = NioLibGetCapabilities(libraryRef, &caps);
    if (!palmError && (caps & (nioLibCapTransports | nioLibCapClock | nioLibCapClient)) !=
        (nioLibCapTransports | nioLibCapClock | nioLibCapClient)) palmError = nioLibErrParam;
    if (palmError) {
        message = "Needs NioLib ABI 1.2";
        if (libraryLoaded && !SysLibTblEntry(libraryRef)->globalsP && !SysLibRemove(libraryRef))
            libraryLoaded = false;
        return;
    }
    palmError = NioLibOpen(libraryRef);
    if (palmError) {
        message = "Cannot open NioLib";
        if (libraryLoaded && !SysLibTblEntry(libraryRef)->globalsP && !SysLibRemove(libraryRef))
            libraryLoaded = false;
        return;
    }
    usbSupported = (caps & nioLibCapUSB) != 0;
    libraryOpen = true;
    config.size = sizeof(config);
    palmError = NioLibGetConfig(libraryRef, &config, &configFlags);
    if (!palmError && !valid_config(&config)) palmError = nioLibErrParam;
    if (palmError) { message = "Cannot read settings"; return; }
    status_read();
    message = configFlags & nioConfigInvalid ? "Invalid saved settings; Save" :
              configFlags & nioConfigDefaulted ? "Default; not yet saved" : "Saved settings loaded";
}

void config_save(void)
{
    NioLibConfig check;
    UInt16 flags = 0;
    if (!libraryOpen) { message = "NioLib unavailable"; return; }
    if (!valid_config(&config)) { palmError = nioLibErrParam; message = "Invalid settings"; return; }
    palmError = NioLibSetConfig(libraryRef, &config);
    if (!palmError) {
        check.size = sizeof(check);
        palmError = NioLibGetConfig(libraryRef, &check, &flags);
        if (!palmError && (flags || MemCmp(&check, &config, sizeof(config))))
            palmError = nioLibErrGlobals;
    }
    if (palmError) { message = "Save failed (busy/invalid)"; return; }
    dirty = false; configFlags = 0;
    status_read();
    message = palmError ? "Saved; status unavailable" : "Saved and verified";
}

void config_test(void)
{
    UInt16 capacity, result = 0, endResult = 0;
    UInt8 timeLE[8];
    NioLibCleanup cleanup;
    Err endError;
    if (!libraryOpen) { message = "NioLib unavailable"; return; }
    if (dirty) { message = "Save changes before Test"; return; }
    nioResult = 0;
    palmError = NioLibBeginClient(libraryRef, &clientToken, &capacity, &result);
    nioResult = result;
    if (palmError || result) {
        Err beginError = palmError;
        message = "Selected link unavailable"; status_read();
        if (!palmError) palmError = beginError;
        return;
    }
    clientOpen = true;
    palmError = NioLibClockGet(libraryRef, clientToken, timeLE, &result);
    nioResult = result;
    cleanup.size = sizeof(cleanup);
    endError = NioLibEndClient(libraryRef, clientToken, &cleanup, &endResult);
    if (!endError) clientOpen = false;
    if (!palmError && endError) palmError = endError;
    if (!nioResult) nioResult = endResult;
    message = palmError || nioResult ? "Connection test failed" : "Clock reply: PASS";
    /* Preserve test errors; passive status never changes the tested backend. */
    { Err testError = palmError; status_read(); if (!palmError) palmError = testError; }
}

void config_action(UInt16 id)
{
    if (!libraryOpen) return;
    palmError = 0; nioResult = 0;
    switch (id) {
    case 1001: config.transport = nioTransportSpringboard; break;
    case 1002: config.transport = nioTransportSerial; break;
    case 1007:
        if (!usbSupported) { message = "USB needs newer NioLib"; return; }
        config.transport = nioTransportUSB; break;
    case 1003:
        if (config.transport != nioTransportSerial) return;
        switch (config.serialBaud) {
        case 9600: config.serialBaud = 19200; break;
        case 19200: config.serialBaud = 38400; break;
        case 38400: config.serialBaud = 57600; break;
        case 57600: config.serialBaud = 115200; break;
        default: config.serialBaud = 9600;
        } break;
    case 1004: if (config.transport != nioTransportSerial) return; config.serialOptions ^= nioSerialRTSCTS; break;
    case 1005: config_save(); return;
    case 1006: config_test(); return;
    default: return;
    }
    dirty = true; message = "Unsaved changes";
}

static void line(const Char *text, Coord y)
{ WinDrawChars(text, StrLen(text), 4, y); }
static void draw(void)
{
    Char buf[48], number[12];
    RectangleType area = {{0,18},{160,35}}, status = {{0,102},{160,58}};
    CtlSetValue(FrmGetObjectPtr(FrmGetActiveForm(), FrmGetObjectIndex(FrmGetActiveForm(),1001)),
                config.transport == nioTransportSpringboard);
    CtlSetValue(FrmGetObjectPtr(FrmGetActiveForm(), FrmGetObjectIndex(FrmGetActiveForm(),1002)),
                config.transport == nioTransportSerial);
    CtlSetValue(FrmGetObjectPtr(FrmGetActiveForm(), FrmGetObjectIndex(FrmGetActiveForm(),1007)),
                config.transport == nioTransportUSB);
    WinEraseRectangle(&area,0); WinEraseRectangle(&status,0);
    if (config.transport == nioTransportSerial) {
        StrCopy(buf,"Baud: "); StrIToA(number,config.serialBaud); StrCat(buf,number); line(buf,20);
        line(config.serialOptions & nioSerialRTSCTS ? "8N1; RTS/CTS" : "8N1; no flow control",32);
    } else line("Serial settings inactive",20);
    line(usbSupported ? "USB cradle supported" : "USB cradle: unavailable",44);
    line(message,104);
    StrCopy(buf,"Palm "); StrIToH(number,(UInt16)palmError); StrCat(buf,number); line(buf,116);
    StrCopy(buf,"NIO "); StrIToH(number,nioResult); StrCat(buf,number); line(buf,128);
    line(transportStatus.flags & nioStatusRecoveryRequired ? "Link reset/recovery required" :
         transportStatus.result ? "Selected backend unavailable" :
         transportStatus.flags & nioStatusAvailable ? "Local backend available" :
         "Availability not yet tested",140);
}
static Boolean handle(EventType *event)
{
    if (event->eType == frmOpenEvent) {
        FrmDrawForm(FrmGetActiveForm()); draw(); return true;
    }
    if (event->eType == ctlSelectEvent) {
        if (event->data.ctlSelect.controlID == 1006) {
            message = "Testing saved connection..."; draw();
        }
        config_action(event->data.ctlSelect.controlID); draw(); return true;
    }
    return false;
}
UInt32 PilotMain(UInt16 cmd, MemPtr params, UInt16 flags)
{
    EventType event; UInt16 error; FormPtr form;
    (void)params; (void)flags;
    if (cmd != sysAppLaunchCmdNormalLaunch) return 0;
    config_start(); FrmGotoForm(1000);
    do {
        EvtGetEvent(&event, evtWaitForever);
        if (!SysHandleEvent(&event) && !MenuHandleEvent(NULL,&event,&error)) {
            if (event.eType == frmLoadEvent) {
                form = FrmInitForm(event.data.frmLoad.formID); FrmSetActiveForm(form);
                FrmSetEventHandler(form,handle);
            } else FrmDispatchEvent(&event);
        }
    } while (event.eType != appStopEvent);
    config_stop(); FrmCloseAllForms(); return 0;
}

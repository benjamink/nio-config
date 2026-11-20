/* Palm configuration front end. Physical I/O and services belong to NioLib. */
#include "palm_config.h"

UInt16 libraryRef, serviceCapacity;
NioLibVersion libraryVersion;
Boolean servicesSupported;
static Boolean libraryLoaded;
Boolean libraryOpen, clientOpen, dirty;
UInt32 clientToken;
static Boolean usbSupported, netSupported;
static Boolean recoverySupported;
static const UInt32 serialBauds[] = {9600, 19200, 38400, 57600, 115200};
NioLibConfig config;
NioLibTransportStatus transportStatus;
UInt16 configFlags, nioResult;
Err palmError;
const Char *message = "Opening NioLib...";

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
         (p->transport == nioTransportUSB && usbSupported) ||
         (p->transport == nioTransportNetwork && netSupported)) &&
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
    libraryVersion = version;
    /* An unpublished Network-transport NioLib (implementations 9-12) said ABI 1.6 with nioLibCapNetLib's bit:
     * its traps from 19 on are not the 1.6 services. */
    servicesSupported = version.abiMinor >= 6 && (caps & nioLibCapServices) &&
        (version.abiMinor >= 7 || !(caps & nioLibCapNetLib));
    usbSupported = (caps & nioLibCapUSB) != 0;
    recoverySupported = version.abiMinor >= 5 && (caps & nioLibCapRecovery);
    netSupported = version.abiMinor >= 7 && (caps & nioLibCapNetLib) != 0;
    libraryOpen = true;
    config.size = sizeof(config);
    palmError = NioLibGetConfig(libraryRef, &config, &configFlags);
    if (!palmError && !valid_config(&config)) palmError = nioLibErrParam;
    if (palmError) { message = "Cannot read settings"; return; }
    status_read();
    message = palmError ? "Cannot read link status" :
              configFlags & nioConfigInvalid ? "Invalid saved settings; Save" :
              transportStatus.flags & nioStatusRecoveryRequired ? "Link recovery required; Reset" :
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
    /* BeginClient clears its output token even when it returns Busy. Retain
     * the existing token so shutdown can retry a Palm-level cleanup failure. */
    if (clientOpen) { message = "Previous cleanup incomplete"; return; }
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

static void config_reset(void)
{
    const Char *instruction;
    if (!recoverySupported) { message = "Reset needs NioLib ABI 1.5"; return; }
    if (dirty || configFlags & nioConfigInvalid) { message = "Save settings before Reset"; return; }
    if (clientOpen) { palmError = nioLibErrBusy; message = "End client before Reset"; return; }
    status_read();
    if (palmError) { message = "Cannot check link status"; return; }
    if (transportStatus.configFlags & nioConfigInvalid || transportStatus.selected != config.transport) {
        palmError = nioLibErrParam; message = "Reload saved settings first"; return;
    }
    if (transportStatus.flags & nioStatusActive) {
        palmError = nioLibErrBusy; message = "End client before Reset"; return;
    }
    if (!(transportStatus.flags & nioStatusRecoveryRequired)) {
        message = "No link recovery needed"; return;
    }
    if (config.transport == nioTransportUSB)
        instruction = "Stop the NIO host and physically disconnect/reconnect the USB cradle first.";
    else if (config.transport == nioTransportSerial)
        instruction = "Reset/restart the serial peer first to retire its old handles and buffered replies.";
    else
        instruction = "Reset/reinitialize the Springboard link and NIO host first to retire old operations.";
    /* Cancel is the alert default. The library independently checks idle/driver
     * ownership; confirmation is not evidence that a physical reset happened. */
    if (FrmCustomAlert(ResetLinkAlert, (Char *)instruction, "", "") != 1) {
        message = "Link reset cancelled"; return;
    }
    palmError = NioLibResetTransport(libraryRef, config.transport, nioRecoveryPeerReset);
    if (palmError) { message = "Link reset failed"; return; }
    status_read();
    if (!palmError && transportStatus.flags & nioStatusRecoveryRequired) palmError = nioLibErrGlobals;
    message = palmError ? "Cannot verify link reset" : "Link recovery confirmed";
}

void config_action(UInt16 id)
{
    if (!libraryOpen) return;
    if (palm_join_pending()) { message = "Wait for Wi-Fi join to finish"; return; }
    palmError = 0; nioResult = 0;
    switch (id) {
    case 1001: config.transport = nioTransportSpringboard; break;
    case 1002: config.transport = nioTransportSerial; break;
    case 1007:
        if (!usbSupported) { message = "USB needs newer NioLib"; return; }
        config.transport = nioTransportUSB; break;
    case 1009:
        if (!netSupported) { message = "Network needs NioLib 1.7"; return; }
        config.transport = nioTransportNetwork; break;
    case 1005: config_save(); return;
    case 1006: config_test(); return;
    case 1010: config_reset(); return;
    default: return;
    }
    dirty = true; message = "Unsaved changes";
}

void config_pick(UInt16 id, Int16 selection)
{
    if (!libraryOpen || config.transport != nioTransportSerial || palm_join_pending()) return;
    if (id == 1003 && selection >= 0 && selection < 5) {
        if (config.serialBaud == serialBauds[selection]) return;
        config.serialBaud = serialBauds[selection];
    } else if (id == 1004 && selection >= 0 && selection < 2) {
        UInt16 options = selection ? nioSerialRTSCTS : 0;
        if (config.serialOptions == options) return;
        config.serialOptions = options;
    } else return;
    palmError = 0; nioResult = 0;
    dirty = true; message = "Unsaved changes";
}

/* Services use the saved transport, with no owner held while editing settings. */
Boolean config_begin_services(void)
{
    if (!libraryOpen) { message = "NioLib unavailable"; return false; }
    if (!servicesSupported) {
        message = "Needs NioLib ABI 1.6 services";
        return false;
    }
    if (dirty || configFlags & nioConfigInvalid) {
        message = "Save Setup changes first";
        return false;
    }
    if (clientOpen) {
        message = "Previous cleanup incomplete";
        return false;
    }
    nioResult = 0;
    palmError = NioLibBeginClient(libraryRef, &clientToken, &serviceCapacity, &nioResult);
    if (palmError || nioResult) { message = "Selected link unavailable"; return false; }
    clientOpen = true;
    transportStatus.size = sizeof(transportStatus);
    palmError = NioLibGetTransportStatus(libraryRef, &transportStatus);
    if (palmError || transportStatus.maxPacket < 32) {
        if (!palmError) palmError = nioLibErrParam;
        config_end_services();
        message = "Cannot size service buffers";
        return false;
    }
    /* BeginClient capacity is session count. maxPacket includes framing. */
    serviceCapacity = transportStatus.maxPacket - 7;
    if (serviceCapacity > PALM_SERVICE_BYTES) serviceCapacity = PALM_SERVICE_BYTES;
    return true;
}

Boolean config_end_services(void)
{
    NioLibCleanup cleanup;
    Err error;
    UInt16 result = 0;
    if (!clientOpen) return !palmError && !nioResult;
    cleanup.size = sizeof(cleanup);
    error = NioLibEndClient(libraryRef, clientToken, &cleanup, &result);
    if (!error) clientOpen = false;
    if (!palmError) palmError = error;
    if (!nioResult) nioResult = result;
    return !palmError && !nioResult;
}

static void line(const Char *text, Coord y)
{ WinDrawChars(text, StrLen(text), 4, y); }
static void draw(void)
{
    FormType *form = FrmGetActiveForm();
    UInt16 i, baud = 4, length;
    const Char *text = message;
    Char buf[48];
    RectangleType status = {{2,108},{156,35}};
    CtlSetValue(FrmGetObjectPtr(FrmGetActiveForm(), FrmGetObjectIndex(FrmGetActiveForm(),1001)),
                config.transport == nioTransportSpringboard);
    CtlSetValue(FrmGetObjectPtr(FrmGetActiveForm(), FrmGetObjectIndex(FrmGetActiveForm(),1002)),
                config.transport == nioTransportSerial);
    CtlSetValue(FrmGetObjectPtr(FrmGetActiveForm(), FrmGetObjectIndex(FrmGetActiveForm(),1007)),
                config.transport == nioTransportUSB);
    CtlSetValue(FrmGetObjectPtr(form, FrmGetObjectIndex(form,1009)),
                config.transport == nioTransportNetwork);
    CtlSetEnabled(FrmGetObjectPtr(form, FrmGetObjectIndex(form,1007)), libraryOpen && usbSupported);
    CtlSetEnabled(FrmGetObjectPtr(form, FrmGetObjectIndex(form,1009)), libraryOpen && netSupported);
    CtlSetEnabled(FrmGetObjectPtr(form, FrmGetObjectIndex(form,1008)), libraryOpen && servicesSupported);
    CtlSetEnabled(FrmGetObjectPtr(form, FrmGetObjectIndex(form,1010)), libraryOpen && recoverySupported);
    for (i = 0; i < 4; i++) {
        UInt16 id = i < 2 ? 1003 + i : 1011 + i - 2;
        UInt16 index = FrmGetObjectIndex(form, id);
        if (libraryOpen && config.transport == nioTransportSerial) FrmShowObject(form, index);
        else FrmHideObject(form, index);
    }
    for (i = 0; i < 5; i++) if (serialBauds[i] == config.serialBaud) baud = i;
    LstSetSelection(FrmGetObjectPtr(form, FrmGetObjectIndex(form,1013)), baud);
    LstSetSelection(FrmGetObjectPtr(form, FrmGetObjectIndex(form,1014)),
        config.serialOptions & nioSerialRTSCTS ? 1 : 0);
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form,1003)),
        LstGetSelectionText(FrmGetObjectPtr(form, FrmGetObjectIndex(form,1013)), baud));
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form,1004)),
        LstGetSelectionText(FrmGetObjectPtr(form, FrmGetObjectIndex(form,1014)),
            config.serialOptions & nioSerialRTSCTS ? 1 : 0));
    WinEraseRectangle(&status,0);
    for (i = 0; i < 2 && *text; i++) {
        length = FntWordWrap((Char *)text, 152);
        if (!length) break;
        WinDrawChars(text, length, 4, 109 + i * 11);
        text += length;
    }
    if (palmError || nioResult) {
        StrPrintF(buf,"Palm %04x   NIO %u", (UInt16)palmError, nioResult); line(buf,132);
    }
}
static Boolean handle(EventType *event)
{
    if (palm_page_tab(event)) return true;
    if (FrmGetFormId(FrmGetActiveForm()) != SetupForm) return palm_page_event(event);
    if (event->eType == frmOpenEvent) {
        FrmDrawForm(FrmGetActiveForm()); palm_page_draw_tabs(SetupTab); draw(); return true;
    }
    if (event->eType == popSelectEvent &&
        ((event->data.popSelect.controlID == 1003 && event->data.popSelect.listID == 1013) ||
         (event->data.popSelect.controlID == 1004 && event->data.popSelect.listID == 1014))) {
        config_pick(event->data.popSelect.controlID, event->data.popSelect.selection);
        draw(); return true;
    }
    if (event->eType == ctlSelectEvent) {
        /* Let Palm's form handler open native popup lists. */
        if (event->data.ctlSelect.controlID == 1003 || event->data.ctlSelect.controlID == 1004)
            return false;
        if (event->data.ctlSelect.controlID == 1006) {
            message = "Testing saved connection..."; draw();
        }
        if (event->data.ctlSelect.controlID == 1008) FrmGotoForm(WifiForm);
        else { config_action(event->data.ctlSelect.controlID); draw(); }
        return true;
    }
    return false;
}
UInt32 PilotMain(UInt16 cmd, NioPalmLaunchPtr params, UInt16 flags)
{
    EventType event; UInt16 error; FormPtr form;
    (void)params; (void)flags;
    if (cmd != sysAppLaunchCmdNormalLaunch) return 0;
    config_start(); FrmGotoForm(HostsForm);
    do {
        EvtGetEvent(&event, palm_join_pending() ? SysTicksPerSecond() : evtWaitForever);
        if (event.eType != appStopEvent) palm_page_tick();
        if (!SysHandleEvent(&event) && !MenuHandleEvent(NULL,&event,&error)) {
            if (event.eType == frmLoadEvent) {
                form = FrmInitForm(event.data.frmLoad.formID); FrmSetActiveForm(form);
                FrmSetEventHandler(form,handle);
            } else FrmDispatchEvent(&event);
        }
    } while (event.eType != appStopEvent);
    palm_page_stop(); config_stop(); FrmCloseAllForms(); return 0;
}

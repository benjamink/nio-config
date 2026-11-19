/* Palm browser and installer adapted from fujinet-palm/apps/FujiConfig
 * (GPL-3.0). FileDevice encoding, transport and preferences stay in NioLib.
 * Bounded UI windows replace FujiConfig's whole-directory allocation. */
#include "browser.h"
#include <ExgMgr.h>
#include <SysEvtMgr.h>

/* SDK 3.1 uses mutable string/Handle declarations for these same traps. */
#ifdef NIO_PALMOS_SDK31
typedef Int PalmCardNo;
#else
typedef UInt16 PalmCardNo;
#endif

#define HOST_NS "config-nio"
#define HOST_KEY "hosts"
#define HostList 1101
#define HostScroll 1108
#define HostRows 7
#define BrowseList 1201
#define BrowseScroll 1209
#define EditField 1401
#define SsidField 1501
#define PasswordField 1502
#define ScanList 1503

Char palmHosts[PALM_HOSTS][PALM_HOST_LEN + 1];
UInt16 palmHostCount, palmHostSelection;
PalmEntry palmEntries[PALM_ROWS];
UInt16 palmEntryCount, palmBrowseStart;
UInt32 palmBrowseTotal;
Char palmPath[PALM_PATH_LEN];
static Char *hostChoices[PALM_HOSTS], *entryChoices[PALM_ROWS];
static Char entryLabels[PALM_ROWS][64];
static Boolean hostsLoaded, browsing, joinPending, joinPollFailed;
static UInt16 editSlot, scanCount, hostTop;
static UInt32 joinStarted, joinPolled;
static fn_wifi_scan_record_t scans[PALM_ROWS];
static Char *scanChoices[PALM_ROWS];
/* Caller-owned scratch must not overlap FileDevice/AppStore inputs/outputs. */
static UInt8 scratch[PALM_SERVICE_BYTES], transfer[PALM_TRANSFER_BYTES];
static fn_file_io_t fileIO = { scratch, sizeof(scratch) };
static Char hostText[PALM_HOST_TEXT + 1], currentUri[PALM_PATH_LEN];
static Char title[48];
static UInt16 tappedForm, tappedList;
static Int16 tappedSelection = noListSelection;
static Boolean secondTap;
#ifdef NIO_PALMOS_SDK31
static UInt32 tappedTick;
#endif

static void reset_taps(void)
{ tappedForm = tappedList = 0; tappedSelection = noListSelection; secondTap = false; }

static UInt16 get16(const UInt8 *p)
{ return (UInt16)(p[0] | ((UInt16)p[1] << 8)); }
static UInt32 get32(const UInt8 *p)
{ return get16(p) | ((UInt32)get16(p + 2) << 16); }

void palm_message(const Char *text)
{ FrmCustomAlert(MessageAlert, (Char *)text, "", ""); }

static void show_error(void)
{
    Char text[96];
    StrPrintF(text, "%s\nPalm %x; NIO %u", message, (UInt16)palmError, nioResult);
    palm_message(text);
}

static Boolean begin(void)
{
    if (!config_begin_services()) return false;
    if (serviceCapacity < 64) {
        nioResult = FN_ERR_UNSUPPORTED;
        config_end_services(); message = "Service packet is too small"; return false;
    }
    fileIO.capacity = serviceCapacity;
    return true;
}

static Boolean finish(const Char *failure)
{
    Boolean ok = config_end_services();
    if (!ok) message = failure;
    return ok;
}

/* Reject an oversized record rather than later saving a truncated host list. */
Boolean palm_parse_hosts(const Char *text, UInt16 length)
{
    UInt16 at = 0, count = 0, start, end, i;
    while (at < length) {
        start = at;
        while (at < length && text[at] != '\r' && text[at] != '\n') {
            if (!text[at]) return false;
            at++;
        }
        end = at;
        while (start < end && (text[start] == ' ' || text[start] == '\t')) start++;
        while (end > start && (text[end-1] == ' ' || text[end-1] == '\t')) end--;
        if (end > start) {
            if (end - start > PALM_HOST_LEN || count == PALM_HOSTS) return false;
            count++;
        }
        while (at < length && (text[at] == '\r' || text[at] == '\n')) at++;
    }
    /* Validate first; a malformed record cannot partially replace the model. */
    at = 0; count = 0;
    while (at < length) {
        start = at;
        while (at < length && text[at] != '\r' && text[at] != '\n') at++;
        end = at;
        while (start < end && (text[start] == ' ' || text[start] == '\t')) start++;
        while (end > start && (text[end-1] == ' ' || text[end-1] == '\t')) end--;
        if (end > start) {
            for (i = start; i < end; i++) palmHosts[count][i-start] = text[i];
            palmHosts[count][end-start] = 0;
            count++;
        }
        while (at < length && (text[at] == '\r' || text[at] == '\n')) at++;
    }
    palmHostCount = count;
    return true;
}

Boolean palm_hosts_load(void)
{
    fn_appstore_read_t read;
    UInt16 length = 0, want;
    hostsLoaded = false;
    if (!begin()) return false;
    for (;;) {
        want = sizeof(transfer);
        if (want > fileIO.capacity - 10) want = fileIO.capacity - 10;
        palmError = NioLibAppstoreRead(libraryRef, clientToken, &fileIO, HOST_NS,
            HOST_KEY, length, transfer, want, &read, &nioResult);
        if (palmError || nioResult) break;
        if (read.offset != length || read.bytes_read > want ||
            read.bytes_read > PALM_HOST_TEXT - length) {
            nioResult = FN_ERR_INVALID; break;
        }
        if (!(read.flags & FN_APPSTORE_READ_EXISTS)) {
            StrCopy(hostText, "sd0:/\nfujinet.diller.org\nfujinet.online\n");
            length = StrLen(hostText);
            break;
        }
        MemMove(hostText + length, transfer, read.bytes_read);
        length += read.bytes_read;
        if (read.flags & FN_APPSTORE_READ_EOF) break;
        if (!read.bytes_read) { nioResult = FN_ERR_IO; break; }
    }
    if (!finish("Cannot read shared hosts")) return false;
    if (!palm_parse_hosts(hostText, length)) {
        message = "Host list exceeds Palm limits"; return false;
    }
    hostsLoaded = true;
    hostTop = 0;
    return true;
}

Boolean palm_hosts_save(void)
{
    fn_appstore_write_t written;
    UInt16 i, n, length = 0, offset = 0, take;
    if (!hostsLoaded) { message = "Load hosts before editing"; return false; }
    for (i = 0; i < palmHostCount; i++) {
        n = StrLen(palmHosts[i]);
        MemMove(hostText + length, palmHosts[i], n);
        length += n; hostText[length++] = '\n';
    }
    if (!begin()) return false;
    do {
        take = length - offset;
        /* Leave room for namespace/key, offset and length plus protocol header. */
        if (take > fileIO.capacity - 40) take = fileIO.capacity - 40;
        palmError = NioLibAppstoreWrite(libraryRef, clientToken, &fileIO, HOST_NS,
            HOST_KEY, offset, (UInt8 *)hostText + offset, take, &written, &nioResult);
        if (palmError || nioResult) break;
        if (written.bytes_written != take) { nioResult = FN_ERR_IO; break; }
        offset += take;
    } while (offset < length);
    return finish("Cannot save shared hosts");
}

/* Bare host names mean TNFS, as on the other platforms. URI roots (including
 * sd0:/ and persist:///) are kept intact; path is relative to that root. */
Boolean palm_compose_uri(const Char *host, const Char *path, const Char *name,
                         Char *uri, UInt16 capacity)
{
    UInt16 n = StrLen(host), p = StrLen(path), leaf = StrLen(name), i;
    Boolean scheme = false;
    for (i = 0; i < n; i++) if (host[i] == ':' && host[i+1] == '/') scheme = true;
    if (!n || (UInt32)n + p + leaf + (scheme ? 0 : 7) + 2 > capacity) return false;
    StrCopy(uri, scheme ? "" : "tnfs://");
    StrCat(uri, host);
    n = StrLen(uri);
    if (uri[n-1] != '/') StrCat(uri, "/");
    StrCat(uri, path); StrCat(uri, name);
    return true;
}

/* ListPages returns borrowed wire bytes, not C structs. Validate every bound
 * before exposing an entry. A cut name can be displayed but never opened. */
Boolean palm_decode_pages(const fn_list_pages_t *page, UInt16 start)
{
    UInt16 pos = 0, end, i, j, n, before = palmEntryCount;
    const UInt8 *data = page->pages;
    PalmEntry *entry;
    if (page->start_page != start || page->page_size != 1 ||
        (UInt32)start + page->page_count > page->total ||
        page->page_count > page->pages_len / 3) return false;
    for (i = 0; i < page->page_count; i++) {
        if (page->pages_len - pos < 3) goto invalid;
        n = data[pos]; end = get16(data + pos + 1); pos += 3;
        if (n != 1 || end > page->pages_len - pos) goto invalid;
        end += pos;
        if (end - pos < 10) goto invalid;
        j = data[pos+1];
        if (!j || j > PALM_NAME_LEN || end - pos != j + 10) goto invalid;
        if (palmEntryCount < PALM_ROWS) {
            entry = &palmEntries[palmEntryCount];
            entry->flags = data[pos];
            MemMove(entry->name, data + pos + 2, j);
            entry->name[j] = 0;
            /* Remote basenames must not escape the current directory. */
            if (StrLen(entry->name) != j || StrChr(entry->name, '/') ||
                StrCompare(entry->name, ".") == 0 || StrCompare(entry->name, "..") == 0)
                goto invalid;
            entry->size = get32(data + pos + 2 + j);
            palmEntryCount++;
        }
        pos = end;
    }
    if (pos != page->pages_len) goto invalid;
    return true;
invalid:
    palmEntryCount = before;
    return false;
}

Boolean palm_browse_load(Boolean refresh)
{
    fn_list_pages_req_t request;
    fn_list_pages_t page;
    UInt16 start = palmBrowseStart;
    palmEntryCount = 0;
    if (!palm_compose_uri(palmHosts[palmHostSelection], palmPath, "", currentUri,
                          sizeof(currentUri))) {
        message = "Path is too long"; return false;
    }
    if (!begin()) return false;
    request.page_size = 1; request.max_payload = fileIO.capacity - 12;
    request.max_name = PALM_NAME_LEN; request.filter = NULL;
    request.flags = FN_LIST_SORT | FN_LIST_DIRS_FIRST | (refresh ? FN_LIST_REFRESH : 0);
    /* One entry per protocol page allows long names on small packets. Assemble
     * at most eight rows, fetching additional packets only when necessary. */
    do {
        request.start_page = start;
        palmError = NioLibFileListPages(libraryRef, clientToken, &fileIO, currentUri,
                                        &request, &page, &nioResult);
        if (palmError || nioResult) break;
        if (page.total > 65535UL || !palm_decode_pages(&page, start) ||
            (!page.page_count && (page.flags & FN_LIST_PAGES_MORE))) {
            nioResult = FN_ERR_IO; break;
        }
        palmBrowseTotal = page.total;
        if (page.page_count > 65535U - start) { nioResult = FN_ERR_IO; break; }
        start += page.page_count;
        request.flags &= ~FN_LIST_REFRESH;
    } while (palmEntryCount < PALM_ROWS && (page.flags & FN_LIST_PAGES_MORE));
    if (!finish("Cannot read this folder")) { palmEntryCount = 0; return false; }
    return true;
}

static void *object(UInt16 id)
{
    FormType *form = FrmGetActiveForm();
    return FrmGetObjectPtr(form, FrmGetObjectIndex(form, id));
}

static void text_line(const Char *text, Coord y)
{
    UInt16 n = StrLen(text);
    Int16 width = 156, length = n;
    Boolean fits;
    FntCharsInWidth(text, &width, &length, &fits);
    WinDrawChars(text, length, 2, y);
}

static void busy(const Char *text)
{
    RectangleType r = {{0,32},{160,12}};
    WinEraseRectangle(&r, 0); text_line(text, 33);
}

void palm_page_draw_tabs(UInt16 active)
{
    UInt16 id;
    for (id = HostsTab; id <= SetupTab; id++) CtlSetValue(object(id), id == active);
}

Boolean palm_page_tab(EventType *event)
{
    UInt16 id, form;
    if (event->eType != ctlSelectEvent) return false;
    id = event->data.ctlSelect.controlID;
    if (id < HostsTab || id > SetupTab) return false;
    reset_taps();
    form = id == HostsTab ? (browsing ? BrowseForm : HostsForm) :
           id == InfoTab ? InfoForm : SetupForm;
    if (FrmGetFormId(FrmGetActiveForm()) != form) FrmGotoForm(form);
    return true;
}

static void show_hosts(void)
{
    UInt16 i;
    reset_taps();
    for (i = 0; i < palmHostCount; i++) hostChoices[i] = palmHosts[i];
    LstSetListChoices(object(HostList), hostChoices, palmHostCount);
    if (hostTop > (palmHostCount > HostRows ? palmHostCount - HostRows : 0))
        hostTop = palmHostCount > HostRows ? palmHostCount - HostRows : 0;
    LstSetSelection(object(HostList), noListSelection);
    LstSetTopItem(object(HostList), hostTop);
    LstDrawList(object(HostList));
    SclSetScrollBar(object(HostScroll), hostTop, 0,
        palmHostCount > HostRows ? palmHostCount - HostRows : 0, HostRows);
    SclDrawScrollBar(object(HostScroll));
}

static void show_browse(void)
{
    UInt16 i;
    Char number[12];
    reset_taps();
    for (i = 0; i < palmEntryCount; i++) {
        StrCopy(entryLabels[i], palmEntries[i].flags & FN_LIST_ENTRY_DIR ? "/ " : "  ");
        StrNCat(entryLabels[i], palmEntries[i].name, sizeof(entryLabels[i]));
        entryChoices[i] = entryLabels[i];
    }
    LstSetListChoices(object(BrowseList), entryChoices, palmEntryCount);
    LstSetSelection(object(BrowseList), noListSelection);
    LstDrawList(object(BrowseList));
    StrIToA(number, palmBrowseStart + (palmEntryCount ? 1 : 0)); StrCopy(title, number);
    StrCat(title, "-"); StrIToA(number, palmBrowseStart + palmEntryCount); StrCat(title, number);
    StrCat(title, " of "); StrIToA(number, palmBrowseTotal); StrCat(title, number);
    busy(title);
    /* ScrollBar values are signed 16-bit. One unit is an eight-row window,
     * keeping even the largest supported directory within the native range. */
    SclSetScrollBar(object(BrowseScroll), palmBrowseStart / PALM_ROWS, 0,
        palmBrowseTotal ? (palmBrowseTotal - 1) / PALM_ROWS : 0, 1);
    SclDrawScrollBar(object(BrowseScroll));
    /* The full selected basename is shown in a details dialog before install. */
}

static void load_browse(Boolean refresh)
{
    busy("Reading folder...");
    if (!palm_browse_load(refresh)) show_error();
    show_browse();
}

static Boolean set_field(UInt16 id, const Char *value, UInt16 capacity)
{
    MemHandle h = MemHandleNew(capacity);
    if (h) {
        StrCopy(MemHandleLock(h), value); MemHandleUnlock(h);
#ifdef NIO_PALMOS_SDK31
        FldSetTextHandle(object(id), (Handle)h);
#else
        FldSetTextHandle(object(id), h);
#endif
        return true;
    }
    palm_message("Not enough memory for this field.");
    return false;
}

/* ---- Streaming installation (FujiConfig's ExgDBRead workflow) ---------- */
typedef struct {
    UInt32 offset, expected;
    UInt16 held, next;
    Boolean eof, declined, self, inUse;
    const Char *failure;
    UInt32 progressTick;
    UInt16 progressInterval;
} Install;

static void install_progress(UInt32 done, UInt32 total)
{
    Char text[48];
    const Char *part = text;
    UInt16 row, length;
    RectangleType count = {{8,50},{128,24}}, bar = {{8,80},{128,10}};
    RectangleType fill = {{9,81},{0,8}};
    StrPrintF(text, "%lu of %lu bytes", done, total);
    WinEraseRectangle(&count, 0);
    for (row = 0; row < 2 && *part; row++) {
        length = FntWordWrap((Char *)part, 128);
        if (!length) break;
        WinDrawChars(part, length, 8, 50 + row * 11);
        part += length;
    }
    WinEraseRectangle(&bar, 0);
    WinDrawRectangleFrame(simpleFrame, &bar);
    if (done >= total) fill.extent.x = 126;
    else {
        /* Keep the product in 32 bits for large FileStat sizes. Discarded
         * low bits are far below one meter pixel at those sizes. */
        if (total > 0x01ffffffUL) { done >>= 7; total >>= 7; }
        fill.extent.x = done * 126 / total;
    }
    if (fill.extent.x) WinDrawRectangle(&fill, 0);
}

static FormType *install_popup(const Char *name, UInt32 total)
{
    FormType *popup = FrmInitForm(1600);
    UInt16 row, length;
    if (!popup) return NULL;
    FrmSetActiveForm(popup); FrmDrawForm(popup);
    for (row = 0; row < 2 && *name; row++) {
        length = FntWordWrap((Char *)name, 128);
        if (!length) break;
        WinDrawChars(name, length, 8, 22 + row * 11);
        name += length;
    }
    install_progress(0, total);
    return popup;
}

/* FujiConfig retried lost read replies. These explicit-offset reads are
 * idempotent, but an unresolved transport operation must be recovered by the
 * operator, never by silently replaying it. Do not retry timeouts or writes. */
static void install_chunk(UInt32 offset, UInt16 length, fn_file_read_t *read)
{
    NioLibTransportStatus status;
    UInt16 attempt;
    for (attempt = 0; attempt < 3; attempt++) {
        palmError = NioLibFileRead(libraryRef, clientToken, &fileIO, currentUri,
            offset, transfer, length, read, &nioResult);
        if (palmError || nioResult != FN_ERR_IO || attempt == 2) return;
        status.size = sizeof(status);
        if (NioLibGetTransportStatus(libraryRef, &status) ||
            status.flags & nioStatusRecoveryRequired) return;
    }
}

static Err install_read(void *data, UInt32 *size, void *user)
{
    Install *in = user;
    fn_file_read_t read;
    UInt32 done = 0, want = *size, now;
    UInt16 take, maximum = sizeof(transfer), request;
    if (maximum > fileIO.capacity - 10) maximum = fileIO.capacity - 10;
    while (done < want) {
        if (!in->held) {
            if (in->eof || palmError || nioResult) break;
            /* ExgDBRead may ask for more than the last resource contains.
             * The stat size bounds the stream even if a full final packet
             * only sets TRUNCATED, without EOF. Never probe beyond that size. */
            if (in->offset == in->expected) { in->eof = true; break; }
            if (in->offset > in->expected) {
                in->failure = "Invalid stream position"; nioResult = FN_ERR_IO; break;
            }
            request = maximum;
            if (in->expected - in->offset < request) request = in->expected - in->offset;
            install_chunk(in->offset, request, &read);
            if (palmError || nioResult) { in->failure = "FileDevice read failed"; break; }
            if (read.offset != in->offset || read.bytes_read > request ||
                in->offset > in->expected || read.bytes_read > in->expected - in->offset ||
                (!read.bytes_read && !(read.flags & FN_FILE_EOF))) {
                in->failure = "Invalid read offset or length";
                nioResult = FN_ERR_IO; break;
            }
            in->held = read.bytes_read; in->next = 0;
            in->offset += read.bytes_read;
            in->eof = in->offset == in->expected || (read.flags & FN_FILE_EOF) != 0;
            if (in->eof && in->offset != in->expected) {
                in->failure = "File ended before its stat size";
                nioResult = FN_ERR_IO; break;
            }
            now = TimGetTicks();
            if (!in->progressInterval) in->progressInterval = SysTicksPerSecond();
            /* Keep first/final progress visible, but avoid UI and auto-off
             * traps on every chunk while a fast transfer is in progress. */
            if (in->offset == read.bytes_read || in->offset == in->expected ||
                now - in->progressTick >= in->progressInterval) {
                in->progressTick = now;
                EvtResetAutoOffTimer();
                install_progress(in->offset, in->expected);
            }
            continue;
        }
        take = want - done < in->held ? (UInt16)(want - done) : in->held;
        MemMove((UInt8 *)data + done, transfer + in->next, take);
        in->next += take; in->held -= take; done += take;
    }
    *size = done;
    return palmError || nioResult ? exgErrUnknown : errNone;
}

static Boolean install_replace(const Char *name, UInt16 version, PalmCardNo card,
                               LocalID db, void *user)
{
    Install *in = user;
    UInt16 ourCard;
    LocalID ours;
    (void)version;
    if ((db & 1) == 0) return true; /* A RAM copy may shadow a ROM database. */
    if (!SysCurAppDatabase(&ourCard, &ours) && card == ourCard && db == ours) {
        in->self = true; return false;
    }
    if (FrmCustomAlert(ReplaceAlert, (Char *)name, "", "")) { in->declined = true; return false; }
    if (DmDeleteDatabase(card, db)) { in->inUse = true; return false; }
    return true;
}

static Boolean ends_with(const Char *name, const Char *suffix)
{
    UInt16 n = StrLen(name), s = StrLen(suffix);
    return n >= s && !StrCaselessCompare(name + n - s, suffix);
}

static void install_file(const PalmEntry *entry)
{
    Install in;
    Err error;
    LocalID db = 0;
    UInt32 type = 0, result;
    Boolean reset = false;
    fn_file_stat_t stat;
    Err readError;
    UInt16 readResult;
    Boolean cleaned;
    FormType *previous, *popup;
    if (!ends_with(entry->name, ".prc") && !ends_with(entry->name, ".pdb") &&
        !ends_with(entry->name, ".pqa")) {
        palm_message("Only .prc, .pdb and .pqa files install on a Palm."); return;
    }
    if (FrmCustomAlert(InstallAlert, (Char *)entry->name, "", "")) return;
    if (!begin()) { show_error(); return; }
    palmError = NioLibFileStat(libraryRef, clientToken, &fileIO, currentUri, &stat, &nioResult);
    if (!palmError && !nioResult &&
        (!(stat.flags & FN_FILE_EXISTS) || stat.size_bytes_high || stat.size_bytes < 78 ||
         (stat.flags & FN_FILE_IS_DIR)))
        nioResult = FN_ERR_INVALID;
    if (palmError || nioResult) { finish("Cannot inspect this file"); show_error(); return; }
    MemSet(&in, sizeof(in), 0); in.expected = stat.size_bytes;
    previous = FrmGetActiveForm();
    popup = install_popup(entry->name, in.expected);
    if (!popup) {
        finish("Cannot create installation window");
        palm_message("Not enough memory for the installation window."); return;
    }
    error = ExgDBRead(install_read, install_replace, &in, &db, 0, &reset, true);
    FrmEraseForm(popup); FrmDeleteForm(popup); FrmSetActiveForm(previous);
    readError = palmError; readResult = nioResult;
    cleaned = finish("Installation cleanup failed");
    if (in.declined) return;
    if (in.self) palm_message("NIO Config cannot replace itself while running.");
    else if (in.inUse) palm_message("The installed database is in use.");
    else if (readError || readResult) {
        Char detail[160];
        StrPrintF(detail, "%s at %lu of %lu bytes.\nPalm %x; NIO %u",
            in.failure ? in.failure : "File read failed", in.offset, in.expected,
            (UInt16)readError, readResult);
        palm_message(detail);
    } else if (error) {
        StrPrintF(title, "Installation failed: Palm %x", (UInt16)error); palm_message(title);
    } else if (!cleaned) {
        palm_message("Database installed, but link cleanup failed. Check Setup before another operation.");
        show_error();
    } else {
        if (!DmDatabaseInfo(0, db, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                             &type, NULL) && type == sysFileTApplication)
            SysAppLaunch(0, db, 0, sysAppLaunchCmdSyncNotify, NULL, &result);
        palm_message(reset ? "Installed. Reset the Palm to finish." : "Installed on this Palm.");
    }
}

static void host_open(Int16 selected)
{
    if (!hostsLoaded) { palm_message("Refresh hosts before editing or browsing."); return; }
    if (selected < 0 || selected >= palmHostCount) { palm_message("Select a host first."); return; }
    reset_taps();
    palmHostSelection = selected; palmPath[0] = 0; palmBrowseStart = 0;
    browsing = true; FrmGotoForm(BrowseForm);
}

static void browse_open(void)
{
    Int16 selected = LstGetSelection(object(BrowseList));
    PalmEntry *entry;
    UInt16 n;
    if (selected < 0 || selected >= palmEntryCount) return;
    entry = &palmEntries[selected];
    if (entry->flags & FN_LIST_ENTRY_NAME_CUT) {
        palm_message("This name was truncated; it cannot be opened safely."); return;
    }
    if (!palm_compose_uri(palmHosts[palmHostSelection], palmPath, entry->name,
                          currentUri, sizeof(currentUri))) {
        palm_message("Path is too long."); return;
    }
    if (entry->flags & FN_LIST_ENTRY_DIR) {
        n = StrLen(palmPath);
        if (n + StrLen(entry->name) + 2 > sizeof(palmPath)) {
            palm_message("Path is too long."); return;
        }
        StrCat(palmPath, entry->name); StrCat(palmPath, "/");
        palmBrowseStart = 0; load_browse(false);
    } else { install_file(entry); show_browse(); }
}

/* ---- Info and Wi-Fi -------------------------------------------------- */
/* Optional commands may be unsupported. A failed link/reply is different:
 * stop the batch, preserve its error, and identify the failed service. */
static Boolean info_result(const Char *failure)
{
    if (!palmError && (!nioResult || nioResult == FN_ERR_UNSUPPORTED)) return true;
    finish(failure); show_error();
    return false;
}

static void info_load(void)
{
    fn_fuji_info_t fuji;
    fn_wifi_status_t wifi;
    fn_wifi_config_t saved;
    fn_wifi_adapter_info_t adapter;
    Char line[96];
    UInt16 y = 46;
    busy("Reading FujiNet...");
    StrPrintF(line, "NioLib ABI %u.%u", libraryVersion.abiMajor, libraryVersion.abiMinor);
    text_line(line, y); y += 11;
    if (!begin()) { show_error(); return; }
    palmError = NioLibFujiGetInfo(libraryRef, clientToken, &fuji, &nioResult);
    if (!info_result("Firmware read failed")) return;
    if (!palmError && !nioResult) {
        StrCopy(line, "FW: "); StrCat(line, fuji.firmware); text_line(line, y); y += 11;
        StrCopy(line, "Build: "); StrCat(line, fuji.profile); text_line(line, y); y += 11;
    } else { text_line("Firmware/build unavailable", y); y += 22; }
    palmError = NioLibWifiGetConfig(libraryRef, clientToken, &saved, &nioResult);
    if (!info_result("Wi-Fi config read failed")) return;
    if (!palmError && !nioResult) {
        StrCopy(line, "SSID: "); StrCat(line, saved.ssid); text_line(line, y);
    } else text_line("SSID unavailable", y);
    y += 11;
    palmError = NioLibWifiGetStatus(libraryRef, clientToken, &wifi, &nioResult);
    if (!info_result("Wi-Fi status read failed")) return;
    if (!palmError && !nioResult) {
        StrPrintF(line, "%s; RSSI %d", wifi.link_state == 2 ? "Connected" :
            wifi.link_state == 1 ? "Connecting" : wifi.link_state == 3 ? "Failed" : "Disconnected",
            (Int16)wifi.rssi);
        text_line(line, y); y += 11;
        StrCopy(line, "IP: "); StrCat(line, wifi.ip); text_line(line, y); y += 11;
        StrCopy(line, "GW: "); StrCat(line, wifi.gateway); text_line(line, y); y += 11;
        StrCopy(line, "DNS: "); StrCat(line, wifi.dns); text_line(line, y); y += 11;
    } else { text_line("Network status unavailable", y); y += 44; }
    palmError = NioLibWifiGetAdapterInfo(libraryRef, clientToken, &adapter, &nioResult);
    if (!info_result("MAC read failed")) return;
    if (!palmError && !nioResult && adapter.mac.valid) {
        StrPrintF(line, "MAC %02x:%02x:%02x:%02x:%02x:%02x", adapter.mac.bytes[0],
            adapter.mac.bytes[1], adapter.mac.bytes[2], adapter.mac.bytes[3],
            adapter.mac.bytes[4], adapter.mac.bytes[5]); text_line(line, y);
    } else text_line("MAC unavailable", y);
    /* Optional calls may be unsupported independently. Cleanup still runs. */
    palmError = 0; nioResult = 0;
    if (!finish("Info cleanup failed")) show_error();
    StrPrintF(line, "%s; packet %u", config.transport == nioTransportUSB ? "USB" :
        config.transport == nioTransportSerial ? "Serial" : "Springboard", transportStatus.maxPacket);
    busy(line);
}

static void wipe_password(void)
{
    FieldType *field = object(PasswordField);
    Char *text = FldGetTextPtr(field);
    if (text) MemSet(text, StrLen(text), 0);
    FldDelete(field, 0, FldGetTextLength(field));
    FldDrawField(field);
}

static void wifi_ssid(const Char *ssid)
{
    FieldType *field = object(SsidField);
    FldDelete(field, 0, FldGetTextLength(field));
    FldInsert(field, (Char *)ssid, StrLen(ssid));
    FldDrawField(field);
}

static void wifi_load(void)
{
    fn_wifi_config_t saved;
    fn_wifi_status_t status;
    Boolean haveStatus;
    busy("Reading Wi-Fi settings...");
    if (!begin()) { show_error(); busy("Wi-Fi unavailable"); return; }
    palmError = NioLibWifiGetConfig(libraryRef, clientToken, &saved, &nioResult);
    if (palmError || nioResult) {
        finish("Cannot read Wi-Fi settings"); show_error(); busy("Wi-Fi unavailable"); return;
    }
    wifi_ssid(saved.ssid);
    palmError = NioLibWifiGetStatus(libraryRef, clientToken, &status, &nioResult);
    haveStatus = !palmError && !nioResult;
    if (!palmError && nioResult == FN_ERR_UNSUPPORTED) nioResult = 0;
    if (!finish("Cannot read Wi-Fi status")) { show_error(); busy("SSID loaded; status failed"); return; }
    StrCopy(title, !haveStatus ? "SSID loaded" : status.link_state == 2 ? "Connected" :
        status.link_state == 1 ? "Connecting" : status.link_state == 3 ? "Wi-Fi failed" : "Disconnected");
    if (saved.password_present) StrCat(title, "; key not shown");
    busy(title);
}

static void wifi_scan(void)
{
    UInt8 count = 0, more = 0;
    UInt16 i;
    if (joinPending) { palm_message("Wait for Wi-Fi join to finish."); return; }
    busy("Scanning Wi-Fi...");
    if (!begin()) { show_error(); busy("Scan unavailable"); return; }
    palmError = NioLibWifiScan(libraryRef, clientToken, 0, PALM_ROWS, scans, PALM_ROWS,
        &count, &more, scratch, fileIO.capacity, &nioResult);
    if (!finish("Wi-Fi scan unavailable")) { show_error(); busy("Scan unavailable"); return; }
    if (count > PALM_ROWS) { palm_message("Invalid scan response."); return; }
    scanCount = count;
    for (i = 0; i < scanCount; i++) scanChoices[i] = scans[i].ssid;
    LstSetListChoices(object(ScanList), scanChoices, scanCount);
    LstSetSelection(object(ScanList), noListSelection); LstDrawList(object(ScanList));
    busy(scanCount ? "Select a network" : "No networks found");
    if (more) palm_message("More networks exist. Enter an unlisted SSID above.");
}

static void wifi_join(void)
{
    const Char *ssid = FldGetTextPtr(object(SsidField));
    const Char *password = FldGetTextPtr(object(PasswordField));
    Boolean accepted;
    if (joinPending) { palm_message("A Wi-Fi join is already pending."); return; }
    if (!ssid || !*ssid) { palm_message("Enter or scan for a network name."); return; }
    if (!begin()) { wipe_password(); show_error(); return; }
    palmError = NioLibWifiTryJoin(libraryRef, clientToken, ssid, password ? password : "", &nioResult);
    accepted = !palmError && !nioResult;
    /* Firmware owns an accepted trial even if ending our session fails. */
    if (accepted) {
        joinPending = true; joinPollFailed = false;
        joinStarted = joinPolled = TimGetTicks();
    }
    wipe_password();
    if (!finish(accepted ? "Wi-Fi join accepted; cleanup failed" : "Wi-Fi join was refused")) {
        show_error(); joinPollFailed = accepted; return;
    }
    palm_message("Joining Wi-Fi. Previous credentials are restored if the join fails.");
}

Boolean palm_join_pending(void) { return joinPending; }

void palm_page_tick(void)
{
    UInt8 state;
    UInt32 now;
    Boolean complete;
    if (!joinPending) return;
    now = TimGetTicks();
    if ((UInt32)(now - joinPolled) < SysTicksPerSecond()) return;
    joinPolled = now;
    if ((UInt32)(now - joinStarted) >= (UInt32)SysTicksPerSecond() * 60) {
        joinPending = false; palm_message("Wi-Fi result unavailable. Refresh Info to check."); return;
    }
    if (!begin()) {
        if (!joinPollFailed) show_error();
        joinPollFailed = true; return;
    }
    palmError = NioLibWifiGetJoinStatus(libraryRef, clientToken, &state, &nioResult);
    complete = !palmError && !nioResult &&
        (state == FN_WIFI_JOIN_CONNECTED || state == FN_WIFI_JOIN_FAILED);
    if (complete) joinPending = false;
    if (!finish("Cannot check Wi-Fi join")) {
        if (!joinPollFailed) show_error();
        joinPollFailed = true; return;
    }
    joinPollFailed = false;
    if (complete) {
        palm_message(state == FN_WIFI_JOIN_CONNECTED ? "Wi-Fi connected." :
            "Wi-Fi failed; restoring previous network.");
    }
}

void palm_page_stop(void)
{
    /* Palm owns field handles, but erase credentials before closing the form. */
    if (FrmGetActiveForm() && FrmGetFormId(FrmGetActiveForm()) == WifiForm) wipe_password();
    joinPending = false;
}

Boolean palm_page_event(EventType *event)
{
    UInt16 form = FrmGetFormId(FrmGetActiveForm()), id, n;
    Int16 selected;
    const Char *text;
    Char saved[PALM_HOST_LEN + 1];
    if (event->eType == penDownEvent && (form == HostsForm || form == BrowseForm)) {
        /* Palm supplies tapCount on penDown, not on the generated list event.
         * Wait for lstSelect (pen release) before opening a form or dialog. */
#ifdef NIO_PALMOS_SDK31
        /* OS 3.1 has no tapCount field. Match the same row within half a
         * second; the actual action still waits for list selection. */
        secondTap = tappedForm == form && (UInt32)(TimGetTicks() - tappedTick) <= SysTicksPerSecond() / 2;
#else
        secondTap = event->tapCount >= 2;
#endif
        return false;
    }
    if (event->eType == frmCloseEvent) reset_taps();
    if (event->eType == frmCloseEvent && form == WifiForm) wipe_password();
    if (event->eType == frmOpenEvent) {
        reset_taps();
        if (form == EditForm) {
            if (!set_field(EditField, editSlot < palmHostCount ? palmHosts[editSlot] : "", PALM_HOST_LEN + 1)) {
                FrmGotoForm(HostsForm); return true;
            }
        } else if (form == WifiForm) {
            if (!set_field(SsidField, "", FN_WIFI_MAX_SSID + 1) ||
                !set_field(PasswordField, "", FN_WIFI_MAX_PASSWORD + 1)) {
                FrmGotoForm(SetupForm); return true;
            }
            scanCount = 0; LstSetListChoices(object(ScanList), scanChoices, 0);
        }
        FrmDrawForm(FrmGetActiveForm());
        if (form == HostsForm || form == BrowseForm) palm_page_draw_tabs(HostsTab);
        else if (form == InfoForm) palm_page_draw_tabs(InfoTab);
        else if (form == WifiForm) palm_page_draw_tabs(SetupTab);
        if (form == HostsForm) {
            /* Opening the app must never enter a synchronous remote call:
             * a missing/misconfigured host must not prevent reaching Setup. */
            busy(hostsLoaded ? "Shared FujiNet hosts" : "Tap Refresh to load hosts");
            show_hosts();
        } else if (form == BrowseForm) load_browse(false);
        else if (form == InfoForm) info_load();
        else if (form == WifiForm) wifi_load();
        else if (form == EditForm) FrmSetFocus(FrmGetActiveForm(), FrmGetObjectIndex(FrmGetActiveForm(), EditField));
        return true;
    }
    if (event->eType == sclExitEvent) {
        UInt16 bar = event->data.sclExit.scrollBarID;
        Int16 value = event->data.sclExit.newValue;
        reset_taps();
        if (form == HostsForm && bar == HostScroll) {
            hostTop = value < 0 ? 0 : value;
            show_hosts(); return true;
        }
        if (form == BrowseForm && bar == BrowseScroll) {
            UInt16 last = palmBrowseTotal ? (palmBrowseTotal - 1) / PALM_ROWS : 0;
            UInt16 page = value < 0 ? 0 : value;
            if (page > last) page = last;
            if ((UInt16)(page * PALM_ROWS) != palmBrowseStart) {
                palmBrowseStart = page * PALM_ROWS; load_browse(false);
            }
            return true;
        }
    }
    if (event->eType == lstSelectEvent &&
        ((form == HostsForm && event->data.lstSelect.listID == HostList) ||
         (form == BrowseForm && event->data.lstSelect.listID == BrowseList))) {
        Boolean open;
        id = event->data.lstSelect.listID;
        selected = event->data.lstSelect.selection;
        open = secondTap && tappedForm == form && tappedList == id && tappedSelection == selected;
        secondTap = false;
        if (selected < 0 || selected >= (form == HostsForm ? palmHostCount : palmEntryCount)) {
            reset_taps(); return true;
        }
        tappedForm = form; tappedList = id; tappedSelection = selected;
#ifdef NIO_PALMOS_SDK31
        tappedTick = TimGetTicks();
#endif
        LstSetSelection(object(id), selected);
        if (open) {
            reset_taps();
            if (form == HostsForm) host_open(selected);
            else browse_open();
        }
        return true;
    }
    if (form == WifiForm && event->eType == lstSelectEvent && event->data.lstSelect.listID == ScanList) {
        selected = event->data.lstSelect.selection;
        if (selected >= 0 && selected < scanCount) {
            wifi_ssid(scans[selected].ssid);
        }
        return true;
    }
    if (event->eType != ctlSelectEvent) return false;
    reset_taps();
    id = event->data.ctlSelect.controlID;
    if (form == HostsForm) {
        selected = LstGetSelection(object(HostList));
        if (id == 1105) {
            busy("Reading shared hosts...");
            if (!palm_hosts_load()) show_error();
            busy(hostsLoaded ? "Shared FujiNet hosts" : "Tap Refresh to retry");
            show_hosts(); return true;
        }
        if (id == 1106 || id == 1107) {
            hostTop = id == 1106 ? (hostTop > HostRows ? hostTop - HostRows : 0) : hostTop + HostRows;
            show_hosts(); return true;
        }
        if (!hostsLoaded) { palm_message("Refresh hosts before editing or browsing."); return true; }
        if (id == 1104) {
            if (palmHostCount == PALM_HOSTS) { palm_message("Eight hosts are already configured."); return true; }
            editSlot = palmHostCount; FrmGotoForm(EditForm); return true;
        }
        if (selected < 0 || selected >= palmHostCount) { palm_message("Select a host first."); return true; }
        if (id == 1102) {
            host_open(selected);
        } else if (id == 1103) { editSlot = selected; FrmGotoForm(EditForm); }
    } else if (form == BrowseForm) {
        if (id == 1202) browse_open();
        else if (id == 1203) {
            n = StrLen(palmPath);
            if (n) { n--; while (n && palmPath[n-1] != '/') n--; palmPath[n] = 0; palmBrowseStart = 0; load_browse(false); }
        } else if (id == 1204) { browsing = false; FrmGotoForm(HostsForm); }
        else if (id == 1205 && palmBrowseStart) {
            palmBrowseStart = palmBrowseStart > PALM_ROWS ? palmBrowseStart - PALM_ROWS : 0; load_browse(false);
        } else if (id == 1206 && palmEntryCount &&
                   (UInt32)palmBrowseStart + palmEntryCount < palmBrowseTotal) {
            palmBrowseStart += palmEntryCount; load_browse(false);
        } else if (id == 1207) { palmBrowseStart = 0; load_browse(true); }
        else if (id == 1208) {
            if (palm_compose_uri(palmHosts[palmHostSelection], palmPath, "", currentUri, sizeof(currentUri)))
                palm_message(currentUri);
        }
    } else if (form == EditForm) {
        if (id == 1403) FrmGotoForm(HostsForm);
        else if (id == 1402) {
            text = FldGetTextPtr(object(EditField));
            if (!text) text = "";
            if (StrChr(text, '\n') || StrChr(text, '\r')) { palm_message("Enter one host or URI."); return true; }
            if (StrLen(text) > PALM_HOST_LEN || (*text && (text[0] == ' ' || text[StrLen(text)-1] == ' '))) {
                palm_message("Remove spaces at the start or end of the host."); return true;
            }
            StrCopy(saved, palmHosts[editSlot]);
            StrCopy(palmHosts[editSlot], text);
            n = palmHostCount;
            if (!*text && editSlot < palmHostCount) {
                UInt16 i;
                for (i = editSlot; i + 1 < palmHostCount; i++) StrCopy(palmHosts[i], palmHosts[i+1]);
                palmHostCount--;
            } else if (*text && editSlot == palmHostCount) palmHostCount++;
            if (!palm_hosts_save()) {
                /* Reload after a possibly partial remote write; don't claim success. */
                if (palmHostCount < n) {
                    UInt16 i;
                    for (i = n - 1; i > editSlot; i--) StrCopy(palmHosts[i], palmHosts[i-1]);
                }
                palmHostCount = n; StrCopy(palmHosts[editSlot], saved);
                hostsLoaded = false; show_error();
            }
            FrmGotoForm(HostsForm);
        }
    } else if (form == InfoForm && id == 1301) {
        FrmDrawForm(FrmGetActiveForm()); palm_page_draw_tabs(InfoTab); info_load();
    } else if (form == WifiForm) {
        if (id == 1504) wifi_scan();
        else if (id == 1505) wifi_join();
        else if (id == 1506) FrmGotoForm(SetupForm);
    }
    return true;
}

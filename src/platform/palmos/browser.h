#ifndef PALM_BROWSER_H
#define PALM_BROWSER_H
#include "palm_config.h"

#define PALM_HOSTS 8
#define PALM_HOST_LEN 240
#define PALM_PATH_LEN 1024
#define PALM_NAME_LEN 220
#define PALM_ROWS 8
#define PALM_HOST_TEXT (PALM_HOSTS * (PALM_HOST_LEN + 1))

typedef struct {
    Char name[PALM_NAME_LEN + 1];
    UInt32 size;
    UInt8 flags;
} PalmEntry;

/* Pure bounded model operations, also exercised by compiled m68k tests. */
Boolean palm_parse_hosts(const Char *text, UInt16 length);
Boolean palm_compose_uri(const Char *host, const Char *path, const Char *name,
                         Char *uri, UInt16 capacity);
Boolean palm_decode_pages(const fn_list_pages_t *page, UInt16 start);
Boolean palm_hosts_load(void);
Boolean palm_hosts_save(void);
Boolean palm_browse_load(Boolean refresh);

#endif

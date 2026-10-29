/* Differential test of the Atari assembly URI composer, run under sim65. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "atari_core.h"

static char actual[202], expected[202];
static char long_host[189];

static uint8_t append_ref(char *dst, uint8_t cap, uint8_t *pos, const char *src)
{
    while (*src) {
        if ((uint8_t)(*pos + 1) >= cap) return 0;
        dst[(*pos)++] = *src++;
    }
    dst[*pos] = 0;
    return 1;
}

static uint8_t compose_ref(char *dst, uint8_t cap, const char *host,
                           const char *path, const char *leaf)
{
    const char *p = host;
    uint8_t pos = 0, scheme = 0;
    dst[0] = 0;
    if (!*host) return 0;
    while (*p && *p != '/' && *p != '\\') {
        if (*p++ == ':') { scheme = 1; break; }
    }
    if (!scheme && !append_ref(dst, cap, &pos, "tnfs://")) return 0;
    if (!append_ref(dst, cap, &pos, host)) return 0;
    if (pos && dst[pos - 1] != '/' && !append_ref(dst, cap, &pos, "/")) return 0;
    if (!append_ref(dst, cap, &pos, path)) return 0;
    if (*leaf) {
        if (pos && dst[pos - 1] != '/' && !append_ref(dst, cap, &pos, "/")) return 0;
        if (!append_ref(dst, cap, &pos, leaf)) return 0;
    }
    return 1;
}

int main(void)
{
    static const char *const hosts[] = { "", "host", "host:/", "tnfs://server/root",
        "example.org", "abc/def:ghi", "abc\\def:ghi", "x:" };
    static const char *const paths[] = { "", "games/", "games", "nested/folder/" };
    static const char *const leaves[] = { "", "file.atr", "a" };
    uint8_t h, p, l, cap, result, reference;
    for (h = 0; h < sizeof(hosts) / sizeof(hosts[0]); ++h)
        for (p = 0; p < sizeof(paths) / sizeof(paths[0]); ++p)
            for (l = 0; l < sizeof(leaves) / sizeof(leaves[0]); ++l)
                for (cap = 1; cap <= 64; ++cap) {
                    memset(actual, 0x55, sizeof(actual));
                    memset(expected, 0x55, sizeof(expected));
                    result = core_compose(actual + 1, cap, hosts[h], paths[p], leaves[l]);
                    reference = compose_ref(expected + 1, cap, hosts[h], paths[p], leaves[l]);
                    if (result != reference || (result && strcmp(actual + 1, expected + 1)) ||
                        actual[0] != 0x55 || actual[cap + 1] != 0x55) {
                        printf("compose mismatch h=%u p=%u l=%u cap=%u\n", h, p, l, cap);
                        return 1;
                    }
                }
    memset(long_host, 'x', 188);
    long_host[188] = 0;
    if (!core_compose(actual, 197, long_host, "", "") || strlen(actual) != 196 ||
        core_compose(actual, 196, long_host, "", "")) {
        puts("196-character URI boundary failed");
        return 1;
    }
    puts("Atari URI composer: differential and bounds checks passed");
    return 0;
}

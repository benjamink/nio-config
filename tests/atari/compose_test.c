/* Differential test of the Atari assembly URI composer under soft65c02. */
#include <stdint.h>
#include <string.h>
#include "../../src/platform/atari/atari_core.h"

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

/* The harness calls test_func directly; main satisfies the cc65 runtime. */
int main(void) { return 0; }
uint8_t test_result[5];
uint8_t test_host;
uint8_t test_path;
uint8_t test_leaf;

void test_func(void)
{
    static const char *const hosts[] = { "", "host", "host:/", "tnfs://server/root",
        "example.org", "abc/def:ghi", "abc\\def:ghi", "x:" };
    static const char *const paths[] = { "", "games/", "games", "nested/folder/" };
    static const char *const leaves[] = { "", "file.atr", "a" };
    uint8_t h, p, l, cap, result, reference;
    h = test_host;
    test_result[0] = 0;
    p = test_path;
    l = test_leaf;
    for (cap = 1; cap <= 64; ++cap) {
                    memset(actual, 0x55, (uint16_t)cap + 2);
                    memset(expected, 0x55, (uint16_t)cap + 2);
                    result = core_compose(actual + 1, cap, hosts[h], paths[p], leaves[l]);
                    reference = compose_ref(expected + 1, cap, hosts[h], paths[p], leaves[l]);
                    if (result != reference || (result && strcmp(actual + 1, expected + 1)) ||
                        actual[0] != 0x55 || actual[cap + 1] != 0x55) {
                        test_result[0] = 1;
                        test_result[1] = h;
                        test_result[2] = p;
                        test_result[3] = l;
                        test_result[4] = cap;
                        return;
                    }
                }
    if (h == 7 && p == 3 && l == 2) {
        memset(long_host, 'x', 188);
        long_host[188] = 0;
        if (!core_compose(actual, 197, long_host, "", "") || strlen(actual) != 196 ||
            core_compose(actual, 196, long_host, "", "")) {
            test_result[0] = 2;
            return;
        }
    }
    if (++test_leaf == 3) {
        test_leaf = 0;
        if (++test_path == 4) {
            test_path = 0;
            ++test_host;
        }
    }
}

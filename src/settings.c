#define _POSIX_C_SOURCE 200809L
#include <cog_tui/settings.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


static int clean(const char *s, size_t cap)
{
    size_t n = strnlen(s, cap);
    if (n == 0 || n == cap) return 0;
    for (size_t i = 0; i < n; ++i)
        if ((unsigned char)s[i] < 32u || (unsigned char)s[i] == 127u) return 0;
    return 1;
}
static int name_start(unsigned char c)
{ return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

cog_result_t cog_tui_settings_defaults(cog_tui_settings_t *s)
{
    if (!s) return COG_ERROR_NULL_PTR;
    memset(s, 0, sizeof(*s));
    strcpy(s->provider, "openrouter");
    strcpy(s->base_url, "https://openrouter.ai/api/v1");
    strcpy(s->credential_ref, "env:OPENROUTER_API_KEY");
    return COG_OK;
}
cog_result_t cog_tui_settings_validate(const cog_tui_settings_t *s)
{
    if (!s) return COG_ERROR_NULL_PTR;
    if (!clean(s->provider, sizeof(s->provider)) || !clean(s->model, sizeof(s->model)) ||
        !clean(s->base_url, sizeof(s->base_url)) || !clean(s->credential_ref, sizeof(s->credential_ref)))
        return COG_ERROR_INVALID_ARG;
    if (strcmp(s->provider, "openrouter")) return COG_ERROR_UNSUPPORTED;
    if (strncmp(s->base_url, "https://", 8) || !s->base_url[8] || s->base_url[8] == '/' ||
        strpbrk(s->base_url, "@?# \\")) return COG_ERROR_INVALID_ARG;
    if (strncmp(s->credential_ref, "env:", 4) || !name_start((unsigned char)s->credential_ref[4]))
        return COG_ERROR_INVALID_ARG;
    for (size_t i = 5; s->credential_ref[i]; ++i) {
        unsigned char c = (unsigned char)s->credential_ref[i];
        if (!name_start(c) && !(c >= '0' && c <= '9')) return COG_ERROR_INVALID_ARG;
    }
    return COG_OK;
}
static int safe_file(const struct stat *s)
{ return S_ISREG(s->st_mode) && s->st_uid == geteuid() && !(s->st_mode & 0077); }

cog_result_t cog_tui_settings_load(const char *path, cog_tui_settings_t *out)
{
    if (!path || !out) return COG_ERROR_NULL_PTR;
    if (!*path) return COG_ERROR_INVALID_ARG;
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) return errno == ENOENT ? COG_ERROR_NOT_FOUND :
        (errno == ELOOP || errno == EACCES ? COG_ERROR_DENIED : COG_ERROR_IO);
    struct stat st;
    if (fstat(fd, &st)) { close(fd); return COG_ERROR_IO; }
    if (!safe_file(&st)) { close(fd); return COG_ERROR_DENIED; }
    char data[8193]; size_t used = 0;
    while (used < sizeof(data)) {
        ssize_t n = read(fd, data + used, sizeof(data) - used);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { close(fd); return COG_ERROR_IO; }
        if (!n) break;
        used += (size_t)n;
    }
    if (close(fd)) return COG_ERROR_IO;
    if (used > 8192) return COG_ERROR_LIMIT;
    if (memchr(data, 0, used) || memchr(data, '\r', used)) return COG_ERROR_PROTOCOL;
    data[used] = 0;
    cog_tui_settings_t s = {0}; unsigned seen = 0;
    char *p = data;
    while (*p) {
        char *end = strchr(p, '\n');
        if (end) *end = 0;
        char *equal = strchr(p, '=');
        if (!equal) return COG_ERROR_PROTOCOL;
        *equal++ = 0;
        unsigned bit; char *dest = NULL; size_t cap = 0;
        if (!strcmp(p, "version")) { bit = 1; if (strcmp(equal, "1")) return COG_ERROR_PROTOCOL; }
        else if (!strcmp(p, "provider")) { bit = 2; dest = s.provider; cap = sizeof(s.provider); }
        else if (!strcmp(p, "model")) { bit = 4; dest = s.model; cap = sizeof(s.model); }
        else if (!strcmp(p, "base_url")) { bit = 8; dest = s.base_url; cap = sizeof(s.base_url); }
        else if (!strcmp(p, "credential_ref")) { bit = 16; dest = s.credential_ref; cap = sizeof(s.credential_ref); }
        else return COG_ERROR_PROTOCOL;
        if (seen & bit) return COG_ERROR_PROTOCOL;
        seen |= bit;
        if (dest) {
            size_t n = strlen(equal);
            if (n >= cap) return COG_ERROR_LIMIT;
            memcpy(dest, equal, n + 1);
        }
        if (!end) break;
        p = end + 1;
    }
    if (seen != 31 || cog_tui_settings_validate(&s) != COG_OK) return COG_ERROR_PROTOCOL;
    *out = s; return COG_OK;
}

cog_result_t cog_tui_settings_save(const char *path, const cog_tui_settings_t *s)
{
    if (!path || !s) return COG_ERROR_NULL_PTR;
    if (!*path) return COG_ERROR_INVALID_ARG;
    cog_result_t r = cog_tui_settings_validate(s);
    if (r != COG_OK) return r;
    char parent[4096]; const char *base = strrchr(path, '/');
    if (base) {
        size_t n = (size_t)(base - path);
        if (n >= sizeof(parent)) return COG_ERROR_INVALID_ARG;
        if (!n) strcpy(parent, "/"); else { memcpy(parent, path, n); parent[n] = 0; }
        ++base;
    } else { strcpy(parent, "."); base = path; }
    if (!*base || !strcmp(base, ".") || !strcmp(base, "..")) return COG_ERROR_INVALID_ARG;
    int dir = open(parent, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (dir < 0) return errno == ELOOP || errno == ENOTDIR || errno == EACCES ? COG_ERROR_DENIED : COG_ERROR_IO;
    struct stat st;
    if (fstat(dir, &st)) { close(dir); return COG_ERROR_IO; }
    if (st.st_uid != geteuid() || (st.st_mode & 0077)) { close(dir); return COG_ERROR_DENIED; }
    if (!fstatat(dir, base, &st, AT_SYMLINK_NOFOLLOW)) {
        if (!safe_file(&st)) { close(dir); return COG_ERROR_DENIED; }
    } else if (errno != ENOENT) { close(dir); return COG_ERROR_IO; }
    char bytes[4096];
    int count = snprintf(bytes, sizeof(bytes), "version=1\nprovider=%s\nmodel=%s\nbase_url=%s\ncredential_ref=%s\n",
        s->provider, s->model, s->base_url, s->credential_ref);
    if (count < 0 || (size_t)count >= sizeof(bytes)) { close(dir); return COG_ERROR_INVALID_ARG; }
    char temp[80]; int fd = -1;
    for (unsigned i = 0; i < 100; ++i) {
        (void)snprintf(temp, sizeof(temp), ".cog-tui-%ld-%u.tmp", (long)getpid(), i);
        fd = openat(dir, temp, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (fd >= 0 || errno != EEXIST) break;
    }
    if (fd < 0) { close(dir); return COG_ERROR_IO; }
    r = COG_OK; size_t sent = 0;
    while (sent < (size_t)count) {
        ssize_t n = write(fd, bytes + sent, (size_t)count - sent);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { r = COG_ERROR_IO; break; }
        sent += (size_t)n;
    }
    if (r == COG_OK && fsync(fd)) r = COG_ERROR_IO;
    if (close(fd) && r == COG_OK) r = COG_ERROR_IO;
    if (r == COG_OK && renameat(dir, temp, dir, base)) r = COG_ERROR_IO;
    if (r != COG_OK) (void)unlinkat(dir, temp, 0);
    else if (fsync(dir)) r = COG_ERROR_INDETERMINATE;
    (void)close(dir);
    return r;
}

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/_iovec.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <errno.h>

#define IOVEC_ENTRY(x) { (void*)(x), (x) ? strlen(x) + 1 : 0 }
#define IOVEC_SIZE(x)  (sizeof(x) / sizeof(struct iovec))

typedef struct notify_request {
    char unused[45];
    char message[3075];
} notify_request_t;

extern "C" {
    int sceUserServiceInitialize(void*);
    int sceUserServiceGetForegroundUser(uint32_t*);
    void sceUserServiceTerminate(void);

    int sceAppInstUtilInitialize(void);
    int sceAppInstUtilTerminate(void);
    int sceAppInstUtilAppInstallTitleDir(const char* title_id,
                                        const char* install_path,
                                        void* reserved);
    int sceKernelSendNotificationRequest(int, notify_request_t*, size_t, int);
}

/* -------------------------------------------------- */

static void notify(const char* fmt, ...) {
    notify_request_t req = {};
    va_list args;
    va_start(args, fmt);
    vsnprintf(req.message, sizeof(req.message) - 1, fmt, args);
    va_end(args);
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

/* -------------------------------------------------- */

static int is_mounted(const char* path) {
    struct statfs sfs;
    if (statfs(path, &sfs) != 0)
        return 0;

    return strcmp(sfs.f_fstypename, "nullfs") == 0;
}

/* -------------------------------------------------- */

static int extract_json_string(const char* json, const char* key,
                               char* out, size_t out_size) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);

    const char* p = strstr(json, search);
    if (!p) return -1;

    p = strchr(p + strlen(search), ':');
    if (!p) return -1;

    while (*++p && isspace(*p));
    if (*p != '"') return -1;
    p++;

    size_t i = 0;
    while (i < out_size - 1 && p[i] && p[i] != '"') {
        out[i] = p[i];
        i++;
    }
    out[i] = '\0';
    return 0;
}

/* -------------------------------------------------- */

static int read_title_id_from_sfo(const char* path,
                                 char* title_id,
                                 size_t size) {
    FILE* f = fopen(path, "rb");
    if (!f) return -1;

    uint32_t magic;
    fread(&magic, 4, 1, f);
    if (magic != 0x00464653) {
        fclose(f);
        return -1;
    }

    uint32_t key_off, data_off;
    uint16_t num_entries;

    fseek(f, 0x08, SEEK_SET);
    fread(&key_off, 4, 1, f);
    fread(&data_off, 4, 1, f);
    fread(&num_entries, 2, 1, f);

    for (int i = 0; i < num_entries; i++) {
        uint16_t key_offset;
        uint32_t data_offset;

        fseek(f, key_off + i * 16 + 0x08, SEEK_SET);
        fread(&key_offset, 2, 1, f);
        fread(&data_offset, 4, 1, f);

        char key[32] = {};
        fseek(f, key_off + key_offset, SEEK_SET);
        fgets(key, sizeof(key), f);

        if (!strcmp(key, "TITLE_ID")) {
            fseek(f, data_off + data_offset, SEEK_SET);
            fgets(title_id, size, f);
            title_id[strcspn(title_id, "\0\r\n")] = '\0';
            fclose(f);
            return 0;
        }
    }

    fclose(f);
    return -1;
}

/* -------------------------------------------------- */

static int get_title_id(char* title_id, size_t size) {
    char cwd[PATH_MAX];
    char path[PATH_MAX];

    if (!getcwd(cwd, sizeof(cwd)))
        return -1;

    snprintf(path, sizeof(path), "%s/sce_sys/param.json", cwd);
    FILE* f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (len > 0 && len < 1024 * 1024) {
            char* buf = (char*)malloc(len + 1);
            if (buf) {
                fread(buf, 1, len, f);
                buf[len] = '\0';

                if (extract_json_string(buf, "titleId", title_id, size) == 0 ||
                    extract_json_string(buf, "title_id", title_id, size) == 0) {
                    free(buf);
                    fclose(f);
                    return 0;
                }
                free(buf);
            }
        }
        fclose(f);
    }

    snprintf(path, sizeof(path), "%s/sce_sys/param.sfo", cwd);
    return read_title_id_from_sfo(path, title_id, size);
}

/* -------------------------------------------------- */

static int remount_system_ex(void) {
    struct iovec iov[] = {
        IOVEC_ENTRY("from"),      IOVEC_ENTRY("/dev/ssd0.system_ex"),
        IOVEC_ENTRY("fspath"),    IOVEC_ENTRY("/system_ex"),
        IOVEC_ENTRY("fstype"),    IOVEC_ENTRY("exfatfs"),
        IOVEC_ENTRY("large"),     IOVEC_ENTRY("yes"),
        IOVEC_ENTRY("timezone"),  IOVEC_ENTRY("static"),
        IOVEC_ENTRY("async"),     IOVEC_ENTRY(NULL),
        IOVEC_ENTRY("ignoreacl"), IOVEC_ENTRY(NULL),
    };
    return nmount(iov, IOVEC_SIZE(iov), MNT_UPDATE);
}

/* -------------------------------------------------- */

static int mount_nullfs(const char* src, const char* dst) {
    struct iovec iov[] = {
        IOVEC_ENTRY("fstype"), IOVEC_ENTRY("nullfs"),
        IOVEC_ENTRY("from"),   IOVEC_ENTRY(src),
        IOVEC_ENTRY("fspath"), IOVEC_ENTRY(dst),
    };
    return nmount(iov, IOVEC_SIZE(iov), 0);
}

/* -------------------------------------------------- */

static int copy_dir(const char* src, const char* dst) {
    if (mkdir(dst, 0755) && errno != EEXIST) {
        printf("mkdir failed for %s\n", dst);
        return -1;
    }

    DIR* d = opendir(src);
    if (!d) {
        printf("opendir failed for %s\n", src);
        return -1;
    }

    struct dirent* e;
    char ss[PATH_MAX], dd[PATH_MAX];
    struct stat st;

    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;

        snprintf(ss, sizeof(ss), "%s/%s", src, e->d_name);
        snprintf(dd, sizeof(dd), "%s/%s", dst, e->d_name);

        if (stat(ss, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            copy_dir(ss, dd);
        } else {
            FILE* fs = fopen(ss, "rb");
            if (!fs) {
                printf("Can't open source file: %s\n", ss);
                continue;
            }

            FILE* fd = fopen(dd, "wb");
            if (!fd) {
                printf("Can't open dest file: %s\n", dd);
                fclose(fs);
                continue;
            }

            char buf[8192];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), fs)) > 0)
                fwrite(buf, 1, n, fd);

            fclose(fd);
            fclose(fs);
        }
    }
    closedir(d);
    return 0;
}

/* -------------------------------------------------- */

int main(void) {
    char cwd[PATH_MAX];
    char title_id[12] = {};
    char system_ex_app[PATH_MAX];
    char user_app_dir[PATH_MAX];
    char user_sce_sys[PATH_MAX];
    char src_sce_sys[PATH_MAX];
    char mount_lnk_path[PATH_MAX];

    if (!getcwd(cwd, sizeof(cwd))) {
        notify("Failed to get cwd");
        return -1;
    }

    if (get_title_id(title_id, sizeof(title_id))) {
        notify("Failed to get Title ID");
        printf("Failed to get Title ID\n");
        return -1;
    }

    notify("Installing %s...", title_id);
    printf("ID:   %s\n", title_id);
    printf("Path: %s\n", cwd);

    snprintf(system_ex_app, sizeof(system_ex_app),
             "/system_ex/app/%s", title_id);

    mkdir(system_ex_app, 0755);

    if (is_mounted(system_ex_app)) {
        printf("Unmounting existing mount: %s\n", system_ex_app);
        unmount(system_ex_app, 0);
    }

    remount_system_ex();

    if (mount_nullfs(cwd, system_ex_app)) {
        notify("mount_nullfs failed");
        printf("mount_nullfs failed\n");
        return -1;
    }

    snprintf(user_app_dir, sizeof(user_app_dir),
             "/user/app/%s", title_id);
    snprintf(user_sce_sys, sizeof(user_sce_sys),
             "%s/sce_sys", user_app_dir);

    mkdir(user_app_dir, 0755);
    mkdir(user_sce_sys, 0755);

    snprintf(src_sce_sys, sizeof(src_sce_sys),
             "%s/sce_sys", cwd);

    if (copy_dir(src_sce_sys, user_sce_sys)) {
        notify("Warning: Failed to copy sce_sys");
        printf("copy_dir failed - icon/title may be missing\n");
    }

    sceAppInstUtilInitialize();
    if (sceAppInstUtilAppInstallTitleDir(title_id, "/user/app/", 0)) {
        notify("Install failed");
        printf("sceAppInstUtilAppInstallTitleDir failed\n");
        return -1;
    }
	
    snprintf(mount_lnk_path, sizeof(mount_lnk_path), "/user/app/%s/mount.lnk", title_id);

    FILE* f = fopen(mount_lnk_path, "w");
    if (!f) {
        //notify("Warning: Failed to create mount.lnk");
        //printf("Warning: fopen failed for %s: %s\n", mount_lnk_path, strerror(errno));
    } else {
        fprintf(f, "%s", cwd);
        fclose(f);
        //printf("Created mount link: %s -> %s\n", mount_lnk_path, cwd);
    }

    notify("%s Installed & Ready!", title_id);
    printf("%s successfully installed.\n", title_id);
    printf("Icon should appear on home screen.\n");

    return 0;
}
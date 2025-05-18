#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/_iovec.h>
#include <sys/mount.h>
#include <sys/stat.h>

#define IOVEC_ENTRY(x) {x ? (char *)x : 0, x ? strlen(x) + 1 : 0}
#define IOVEC_SIZE(x) (sizeof(x) / sizeof(struct iovec))

typedef struct app_launch_ctx
{
  uint32_t structsize;
  uint32_t user_id;
  uint32_t app_opt;
  uint64_t crash_report;
  uint32_t check_flag;
} app_launch_ctx_t;

extern "C"
{
  int sceUserServiceInitialize(void *);
  int sceUserServiceGetForegroundUser(uint32_t *);
  void sceUserServiceTerminate(void);
  int sceSystemServiceLaunchApp(const char *, char **, app_launch_ctx_t *);
}

int remount_system_ex(void)
{
  struct iovec iov[] = {
      IOVEC_ENTRY("from"),
      IOVEC_ENTRY("/dev/ssd0.system_ex"),
      IOVEC_ENTRY("fspath"),
      IOVEC_ENTRY("/system_ex"),
      IOVEC_ENTRY("fstype"),
      IOVEC_ENTRY("exfatfs"),
      IOVEC_ENTRY("large"),
      IOVEC_ENTRY("yes"),
      IOVEC_ENTRY("timezone"),
      IOVEC_ENTRY("static"),
      IOVEC_ENTRY("async"),
      IOVEC_ENTRY(NULL),
      IOVEC_ENTRY("ignoreacl"),
      IOVEC_ENTRY(NULL),
  };

  return nmount(iov, IOVEC_SIZE(iov), MNT_UPDATE);
}

int mount_nullfs(const char *src, const char *dst)
{
  struct iovec iov[] = {
      IOVEC_ENTRY("fstype"),
      IOVEC_ENTRY("nullfs"),
      IOVEC_ENTRY("from"),
      IOVEC_ENTRY(src),
      IOVEC_ENTRY("fspath"),
      IOVEC_ENTRY(dst),
  };

  return nmount(iov, IOVEC_SIZE(iov), 0);
}

int endswith(const char *string, const char *suffix)
{
  size_t suffix_len = strlen(suffix);
  size_t string_len = strlen(string);

  if (string_len < suffix_len)
  {
    return 0;
  }

  return strncmp(string + string_len - suffix_len, suffix, suffix_len) != 0;
}

int chmod_bins(const char *path)
{
  char buf[PATH_MAX + 1];
  struct dirent *entry;
  struct stat st;
  DIR *dir;

  if (stat(path, &st) != 0)
  {
    return -1;
  }

  if (endswith(path, ".prx") || endswith(path, ".sprx") || endswith(path, "/eboot.bin"))
  {
    chmod(path, 0755);
  }

  if (S_ISDIR(st.st_mode))
  {
    dir = opendir(path);
    while (1)
    {
      entry = readdir(dir);
      if (entry == nullptr)
      {
        break;
      }

      if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
      {
        continue;
      }

      sprintf(buf, "%s/%s", path, entry->d_name);
      chmod_bins(buf);
    }

    closedir(dir);
  }

  return 0;
}

int main(int argc, char *argv[])
{
  app_launch_ctx_t ctx = {0};
  char src[PATH_MAX + 1];
  char dst[PATH_MAX + 1];
  const char *title_id;

  if (argc < 2)
  {
    printf("Usage: %s TITLE_ID\n", argv[0]);
    return -1;
  }

  title_id = argv[1];
  getcwd(src, PATH_MAX);

  strcpy(dst, "/system_ex/app/");
  strcat(dst, title_id);

  sceUserServiceInitialize(0);
  sceUserServiceGetForegroundUser(&ctx.user_id);

  if (access(dst, F_OK) != 0)
  {
    remount_system_ex();
    mkdir(dst, 0755);
  }

  mount_nullfs(src, dst);
  chmod_bins(src);

  return sceSystemServiceLaunchApp(title_id, &argv[2], &ctx);
}

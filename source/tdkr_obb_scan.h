/* tdkr_obb_scan.h -- which file in the obb folder is the main / the patch
 * expansion file, whatever the player called it. Shared by the game program
 * (tdkr_obb.c) and the launcher (launcher/source/tdkr_launcher.c): plain
 * libc, no libnx. MIT.
 *
 * The game's own names are datakrhm.obb and patchkrhm.obb; Google's are
 * main.<version>.<package>.obb and patch.<version>.<package>.obb. A name
 * starting with "patch" is the patch; any other .obb is the main file. Of
 * several, the exact game name wins, else the last by name (the newest
 * version).
 */
#ifndef TDKR_OBB_SCAN_H
#define TDKR_OBB_SCAN_H

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static inline int tdkr_obb_exact(const char *n) {
  return !strcasecmp(n, "datakrhm.obb") || !strcasecmp(n, "patchkrhm.obb");
}

/* 1 and dir/<name> in out when the folder has the file for the role. */
static inline int tdkr_obb_scan(const char *dir, int patch, char *out, size_t cap) {
  DIR *d = opendir(dir);
  if (!d)
    return 0;
  char best[256] = "";
  struct dirent *e;
  while ((e = readdir(d))) {
    const char *n = e->d_name;
    size_t l = strlen(n);
    if (l < 5 || l >= sizeof best || n[0] == '.' || strcasecmp(n + l - 4, ".obb"))
      continue;
    if ((strncasecmp(n, "patch", 5) == 0) != (patch != 0))
      continue;
    if (!best[0] || tdkr_obb_exact(n) || (!tdkr_obb_exact(best) && strcmp(n, best) > 0))
      memcpy(best, n, l + 1);
  }
  closedir(d);
  if (!best[0])
    return 0;
  snprintf(out, cap, "%s/%s", dir, best);
  return 1;
}

#endif /* TDKR_OBB_SCAN_H */

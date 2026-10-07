/* tdkr_obb.c -- the expansion files, read from <game folder>/obb.
 *
 * The game looks for its data (datakrhm.obb, and the patch, patchkrhm.obb)
 * through Java (sGetOBBFilename) and, by name, in its own folders. Whatever
 * it asks for ends in the player's obb folder: the runtime already sends
 * /sdcard/Android/obb/<package>/... there (dcr_path.c); port_path_fixup
 * covers the rest -- an .obb that is not there under that name (the player
 * kept Google's main.<n>.<package>.obb, the game asks for datakrhm.obb, or
 * the other way round) is answered with the file of the same role. A file
 * that does exist under the name asked for is never replaced.
 * Without any .obb (extracted 'files' tree in data/files instead), the
 * engine still asks for data under obb/ (its res dir): obb/<...> falls back
 * to data/files/<...> when that exists. MIT.
 */
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "dcr_path.h"
#include "tdkr.h"
#include "tdkr_obb_scan.h"
#include "util.h"

int tdkr_obb_find(int patch, char *out, size_t cap) {
  char dir[DCR_PATH_MAX];
  snprintf(dir, sizeof dir, "%s/obb", dcr_game_root());
  return tdkr_obb_scan(dir, patch, out, cap);
}

/* Trace the game's data opens (limited): .gla zones/textures and the OBB.
 * The runtime's default only traces the APK; without this the zone streaming
 * crash leaves no [io]/[path] lines at all. */
int dcr_path_traced(const char *p) {
  if (!p)
    return 0;
  return strstr(p, ".apk") || strstr(p, ".gla") || strstr(p, ".obb") || strstr(p, "textures") ||
         strstr(p, "welcome") != NULL;
}

const char *port_path_fixup(const char *real, char *out, size_t cap) {
  /* No-OBB installs keep the extracted tree in data/files (data/, textures/,
   * welcome/...) while the engine asks for it under obb/ (its res dir). Send
   * obb/<...> to data/files/<...> when that is what exists. .obb files
   * themselves are handled below and never redirected. */
  {
    char obb[DCR_PATH_MAX];
    snprintf(obb, sizeof obb, "%s/obb/", dcr_game_root());
    size_t n = strlen(obb);
    if (!strncmp(real, obb, n)) {
      struct stat st;
      if (stat(real, &st) != 0) {
        char alt[DCR_PATH_MAX];
        snprintf(alt, sizeof alt, "%s/data/files/%s", dcr_game_root(), real + n);
        if (stat(alt, &st) == 0) {
          static int logged_alt;
          if (!logged_alt++)
            debugPrintf("[obb] %s -> %s (extracted data)\n", real, alt);
          snprintf(out, cap, "%s", alt);
          return out;
        }
      } else {
        return real;
      }
    }
  }
  size_t l = strlen(real);
  if (l < 5 || strcasecmp(real + l - 4, ".obb"))
    return real;
  struct stat st;
  if (stat(real, &st) == 0)
    return real; /* it is there under the name asked for */
  const char *base = strrchr(real, '/');
  base = base ? base + 1 : real;
  int patch = strncasecmp(base, "patch", 5) == 0;
  char found[DCR_PATH_MAX];
  if (!tdkr_obb_find(patch, found, sizeof found))
    return real;
  static int logged[2];
  if (!logged[patch]++)
    debugPrintf("[obb] %s -> %s\n", real, found);
  snprintf(out, cap, "%s", found);
  return out;
}

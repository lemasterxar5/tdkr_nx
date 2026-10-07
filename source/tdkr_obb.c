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

/* Menu vs gameplay, seen by the wrapper: the engine opens its zones as
 * l_<name>.gla (l_menu, l_gothamcity, l_batcave, ...). l_menu is the start
 * menu (and its shop); every other l_* zone is gameplay. l_#blank (loading
 * placeholder) and non-zone files keep whatever was there. Boot starts in
 * the menu, so the stick never drags a finger over menu buttons. */
static volatile int g_zone_menu = 1;

void tdkr_zone_note(const char *path) {
  if (!path)
    return;
  const char *base = strrchr(path, '/');
  base = base ? base + 1 : path;
  if (strncasecmp(base, "l_", 2) != 0)
    return;
  size_t l = strlen(base);
  if (l < 5)
    return;
  if (strcasecmp(base + l - 4, ".gla") != 0)
    return;
  if (!strncasecmp(base, "l_menu", 6)) {
    if (!g_zone_menu)
      debugPrintf("[zone] menu (%s): keys + touch screen\n", base);
    g_zone_menu = 1;
  } else if (!strncasecmp(base, "l_#blank", 8)) {
    /* loading placeholder: keep the current side */
  } else {
    if (g_zone_menu)
      debugPrintf("[zone] gameplay (%s): full mapping\n", base);
    g_zone_menu = 0;
  }
}

int tdkr_zone_is_menu(void) { return g_zone_menu; }

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
  tdkr_zone_note(real);
  /* Path answers are stable for the run (the OBB set and the extracted tree
   * do not move), but the engine re-asks dozens of times a frame: a 32-entry
   * LRU skips the string matching and the stat()s on a hit. */
  #define FIXUP_CACHE 32
  static struct {
    char req[DCR_PATH_MAX];
    char ans[DCR_PATH_MAX];
    int same; /* answer is the request itself */
    int has;
  } g_fixcache[FIXUP_CACHE];
  static unsigned g_fixnext;
  for (int i = 0; i < FIXUP_CACHE; i++)
    if (g_fixcache[i].has && !strcmp(g_fixcache[i].req, real)) {
      if (g_fixcache[i].same)
        return real;
      snprintf(out, cap, "%s", g_fixcache[i].ans);
      return out;
    }
  /* Too long to key safely: answer without caching. */
  size_t rlen = strlen(real);
  int cacheable = rlen < sizeof g_fixcache[0].req;
  char fixed[DCR_PATH_MAX];
  int is_fixed = 0;
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
          snprintf(fixed, sizeof fixed, "%s", alt);
          is_fixed = 1;
        }
      }
    }
  }
  if (!is_fixed) {
    size_t l = strlen(real);
    if (l < 5 || strcasecmp(real + l - 4, ".obb")) {
      /* unchanged */
    } else {
      struct stat st;
      if (stat(real, &st) != 0) {
        const char *base = strrchr(real, '/');
        base = base ? base + 1 : real;
        int patch = strncasecmp(base, "patch", 5) == 0;
        char found[DCR_PATH_MAX];
        if (tdkr_obb_find(patch, found, sizeof found)) {
          static int logged[2];
          if (!logged[patch]++)
            debugPrintf("[obb] %s -> %s\n", real, found);
          snprintf(fixed, sizeof fixed, "%s", found);
          is_fixed = 1;
        }
      }
    }
  }
  {
    unsigned slot = g_fixnext++ % FIXUP_CACHE;
    if (cacheable) {
      memcpy(g_fixcache[slot].req, real, rlen + 1);
      g_fixcache[slot].same = !is_fixed;
      if (is_fixed)
        memcpy(g_fixcache[slot].ans, fixed, strlen(fixed) + 1);
      g_fixcache[slot].has = 1;
    }
  }
  if (!is_fixed)
    return real;
  snprintf(out, cap, "%s", fixed);
  return out;
}

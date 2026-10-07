/* tdkr_launcher.c -- the launcher's checks for the expansion file: the
 * game needs its .obb in <game folder>/obb besides the APK. MIT. */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "launcher.h"
#include "rt_settings.h"
#include "tdkr_obb_scan.h"

#define OBB_DIR "sdmc:" PORT_ROOT_PATH "/obb"

int port_launcher_check(char *status, size_t scap, char *help, size_t hcap) {
  char path[512];
  if (tdkr_obb_scan(OBB_DIR, 0, path, sizeof path)) {
    const char *name = strrchr(path, '/');
    snprintf(status, scap, "OBB: %s%s", name ? name + 1 : path,
             tdkr_obb_scan(OBB_DIR, 1, path, sizeof path) ? " (+ patch)" : "");
    return 0;
  }
  /* No .obb: the extracted data (the game's "files" folder) is enough. */
  struct stat st;
  if (stat("sdmc:" PORT_ROOT_PATH "/data/files/data", &st) == 0) {
    snprintf(status, scap, "Data: extracted (data/files)");
    return 0;
  }
  snprintf(status, scap, "OBB: MISSING");
  snprintf(help, hcap,
           "Copy the game's expansion file (datakrhm.obb, and patchkrhm.obb if you\n"
           "have it) to:\n  " PORT_ROOT_PATH "/obb/\n"
           "or the extracted 'files' folder (data, textures...) to:\n  " PORT_ROOT_PATH "/data/files/");
  return 1;
}

void port_launcher_instructions(void) {
  printf("  1. put the APK of your own " PORT_APK_DESC "\n"
         "     (any file name) in " PORT_ROOT_PATH "\n"
         "     and its .obb files in " PORT_ROOT_PATH "/obb\n");
}

/* port_config.h -- The Dark Knight Rises (Gameloft, com.gameloft.android.AMAZ.GloftKRAS
 * 1.1.6, armeabi-v7a): the port's settings for the android32 runtime.
 *
 * Macros only (the assembler and the launcher read this too).
 *
 * Folder on the SD card (PORT_ROOT_PATH = "/switch/" PORT_NAME):
 *   sdmc:/switch/tdkr_nx/
 *     tdkr_nx.nro      the launcher
 *     <any name>.apk   the player's APK (found by its manifest and libKRAS.so)
 *     obb/             the expansion files (datakrhm.obb, patchkrhm.obb, or
 *                      main.*.obb / patch.*.obb: any name that tdkr_obb.c can tell)
 *     config.ini, debug.log, libKRAS.so (unpacked on the first start), data/
 * MIT.
 */
#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H

#define PORT_TITLE   "The Dark Knight Rises"
#define PORT_NAME    "tdkr_nx"
#define PORT_PACKAGE "com.gameloft.android.AMAZ.GloftKRAS"
#define PORT_BANNER  "tdkr_nx: The Dark Knight Rises (Gameloft GLF engine, armeabi-v7a)"

#define PORT_APK_DEFAULT_NAME "tdkr.apk"
#define PORT_APK_DESC "The Dark Knight Rises 1.1.6 (" PORT_PACKAGE ", armeabi-v7a)"
/* The APK, under any name: it must hold the game's library and be this package. */
#define PORT_APK_ROLES \
  {.what = "the game", .name = PORT_APK_DEFAULT_NAME, \
   .need = (const char *const[]){"lib/armeabi-v7a/libKRAS.so", NULL}, .package = PORT_PACKAGE}

#define PORT_LAUNCHER_START_NOTE "(the first start unpacks the game's library from the APK)"

/* libKRAS.so: highest p_vaddr + p_memsz = 0xc1d4a0 (12.1 MB). */
#define PORT_SO_REGION_BYTES (16u * 1024 * 1024)

/* Sequential archive streaming (.gla zones/textures): coalesce the small
 * reads (a8retry pattern), 32 KB stdio buffering instead of 1 KB. */
#define RT_IO_READAHEAD   1
#define RT_IO_PATTERN_STATS 1
#define RT_STDIO_READ_BUF 32768

/* Loads run inside one frame (step()): the loop cannot poll then, so a
 * watcher thread boosts the CPU for a frame past 50 ms, until it ends.
 * Load-only: gameplay frames never trigger it. Off with
 * [performance] boost_cpu_when_loading. */
#define RT_BOOST_WATCH_THREAD 1

/* The game has no controller support of its own: one player. */
#define RT_PAD_MAX_PLAYERS 1

#endif /* PORT_CONFIG_H */

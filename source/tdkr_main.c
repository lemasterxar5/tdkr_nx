/* tdkr_main.c -- The Dark Knight Rises: from the APK to the frame loop.
 *
 * port_load: the setup unpacks libKRAS.so and the class list from the APK
 * (the first start, and whenever the APK changes), the OBB is checked, the
 * library is loaded, relocated, bound to the runtime's shims and sealed as
 * code. port_run: the library's constructors and JNI_OnLoad, then what the
 * Java activity did: GL2JNILib.init, Device/SUtils nativeInit (they call back
 * into setupPaths/setPaths), a GLES 2 context, initGL, resize, onResume --
 * and every frame step() and the swap. libKRAS.so has no
 * EGL of its own: tdkr_gl.c is its GLSurfaceView. MIT.
 */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <switch.h>

#include "dcr_path.h"
#include "dcr_setup.h"
#include "bionic_io.h"
#include "dcr_boost.h"
#include "error.h"
#include "gl_layer.h"
#include "rt_applet.h"
#include "rt_boot.h"
#include "rt_cfg.h"
#include "rt_settings.h"
#include "rt_window.h"
#include "so_util.h"
#include "tdkr.h"
#include "util.h"
#include "watchdog.h"

static so_module g_mod;
static volatile int g_running;

/* The first start: libKRAS.so (the bar from 50 to 800 by bytes), the class
 * list (800-950). */
static const char *const k_libs[] = {"libKRAS.so"};
const RtSetupPlan port_setup_plan = {
    .libs = k_libs,
    .nlibs = 1,
    .libs_what = "Unpacking the game's library",
    .libs_p0 = 50,
    .libs_p1 = 800,
    .classes_p0 = 800,
    .classes_p1 = 950,
};

uint64_t dcr_boot_frames(void) { return dcr_gl_frames(); }

int port_load(const char *apk) {
  char obb[DCR_PATH_MAX];
  if (tdkr_obb_find(0, obb, sizeof obb)) {
    debugPrintf("[boot] OBB: %s\n", obb);
    if (tdkr_obb_find(1, obb, sizeof obb))
      debugPrintf("[boot] OBB patch: %s\n", obb);
  } else {
    /* No .obb: accept the already-extracted data (the game's "files" folder:
     * data/, textures/, welcome/, d_o_w_n_l_o_a_d_e_d...) in <game folder>/data/files. */
    char probe[DCR_PATH_MAX];
    struct stat st;
    rt_root_path(probe, sizeof probe, "data/files/data");
    if (stat(probe, &st) != 0)
      fatal_error("No game data found.\n\n"
                  "Put the game's .obb in\n%s/obb\n\n"
                  "or the extracted 'files' folder (data, textures, welcome...) in\n"
                  "%s/data/files",
                  dcr_game_root(), dcr_game_root());
    debugPrintf("[boot] no .obb; using extracted data in %s\n", probe);
  }

  dcr_setup_from_apk(apk);

  char lib[DCR_PATH_MAX];
  rt_root_path(lib, sizeof lib, "libKRAS.so");
  int rc = so_load(&g_mod, lib, NULL, PORT_SO_REGION_BYTES);
  if (rc)
    fatal_error("libKRAS.so could not be loaded (%d).\n\n%s", rc, port_apk_help());
  if (so_relocate(&g_mod))
    fatal_error("libKRAS.so: relocation failed.");
  int left = so_resolve(&g_mod, NULL, 0, 0);
  if (left)
    debugPrintf("[boot] WARNING: %d import(s) of libKRAS.so unresolved\n", left);
  so_fix_kuser_helpers(&g_mod);
  so_finalize(&g_mod);
  so_flush_caches(&g_mod);
  return 0;
}

#define NAT(field, sym, required)                                                          \
  do {                                                                                     \
    g_nat.field = (void *)so_try_find_addr_rx(&g_mod, "Java_com_gameloft_glf_GL2JNILib_" sym); \
    if (required && !g_nat.field)                                                          \
      fatal_error("libKRAS.so has no GL2JNILib." sym);                                     \
    debugPrintf("[boot] native GL2JNILib." sym ": %s\n", g_nat.field ? "found" : "MISSING"); \
  } while (0)

static void find_natives(void) {
  NAT(setPaths, "setPaths", 1);
  NAT(init, "init", 1);
  NAT(initViewSettings, "InitViewSettings", 0);
  NAT(initGL, "initGL", 0);
  NAT(resize, "resize", 1);
  NAT(step, "step", 1);
  NAT(stateChanged, "stateChanged", 0);
  NAT(onResume, "onResume", 0);
  NAT(onPause, "onPause", 0);
  NAT(destroy, "destroy", 0);
  NAT(touchEvent, "touchEvent", 0);
  NAT(onKeyDown, "OnKeyDown", 0);
  NAT(onKeyUp, "OnKeyUp", 0);
  NAT(splashMsg, "OnSplashScreenMessage", 0);
  NAT(setXperiaPlay, "nativeSetXperiaPlay", 0);
  NAT(slideChanged, "nativeIsSlideChanged", 0);
  NAT(powerAConnected, "nativeSetPowerAConnected", 0);
  NAT(powerARightJoy, "nativeSetPowerARightJoystick", 0);
  if (!g_nat.touchEvent)
    debugPrintf("[boot] WARNING: GL2JNILib.touchEvent not found: touch and gamepad will do nothing\n");
}

/* The helper classes' static nativeInit(): Java's static initialisers. */
static void helper_native_init(const char *sym, const char *cls) {
  void (*fn)(void *, void *) = (void (*)(void *, void *))so_try_find_addr_rx(&g_mod, sym);
  if (fn)
    fn(g_jni_env, jni_class(cls)->obj);
}

void port_focus_lost(void) {
  if (!g_running)
    return;
  tdkr_audio_pause(1);
  if (g_nat.onPause)
    g_nat.onPause(g_jni_env, g_lib_cls);
  if (g_nat.stateChanged)
    g_nat.stateChanged(g_jni_env, g_lib_cls, 0);
}

void port_focus_gained(void) {
  if (!g_running)
    return;
  if (g_nat.stateChanged)
    g_nat.stateChanged(g_jni_env, g_lib_cls, 1);
  if (g_nat.onResume)
    g_nat.onResume(g_jni_env, g_lib_cls);
  tdkr_audio_pause(0);
}

/* HOME and sleep freeze the whole process; the runtime's clocks find each
 * freeze (whether or not focus messages came). What Android does around it:
 * the focus lost, then back. */
void port_process_frozen(unsigned count) {
  debugPrintf("[game] the process was held (HOME menu or sleep; freeze %u)\n", count);
  port_focus_lost();
  port_focus_gained();
}

void port_run(void) {
  so_execute_init_array(&g_mod); /* System.loadLibrary: the constructors ... */
  jni_init();
  tdkr_java_patch_monitors(); /* synchronized blocks lock for real from here on */
  void *env = g_jni_env;
  g_lib_cls = jni_class(TDKR_CLS_LIB)->obj;
  find_natives();

  jint (*on_load)(void *, void *) = (jint(*)(void *, void *))so_try_find_addr_rx(&g_mod, "JNI_OnLoad");
  if (on_load) /* ... then JNI_OnLoad */
    debugPrintf("[boot] JNI_OnLoad -> 0x%x\n", (unsigned)on_load(g_jni_vm, NULL));

  tdkr_input_init();

  /* GL2JNIActivity.onCreate / GLSurfaceView: init caches the JNI IDs and makes
   * the view; Device/SUtils nativeInit call back into setupPaths -> setPaths.
   * setPaths needs glf::App (crash.log: far 0x100d4, App+0x100d0 with App ==
   * NULL), and init alone does NOT create it -- the 202610061943 build still
   * crashed in Device nativeInit after init. So every setPaths (the callbacks
   * and the explicit one) stays deferred until initGL+resize ran; if App is
   * created there, the game proceeds, otherwise the next crash/log tells us. */
  g_nat.init(env, g_lib_cls);
  /* Announce after init (not before): init-time resets must not wipe it.
   * Binds depend on the flag: rebuild them now that it is final. */
  tdkr_gamepad_native_init();
  tdkr_input_refresh();
  helper_native_init("Java_com_gameloft_android_AMAZ_GloftKRAS_GLUtils_SUtils_nativeInit",
                     TDKR_CLS_BASE "/GLUtils/SUtils");
  helper_native_init("Java_com_gameloft_android_AMAZ_GloftKRAS_GLUtils_Device_nativeInit",
                     TDKR_CLS_BASE "/GLUtils/Device");
  if (g_nat.initViewSettings)
    g_nat.initViewSettings(env, g_lib_cls);
  if (!tdkr_egl_ensure())
    fatal_error("No GLES 2 context could be made.\nSee debug.log.");

  /* GLSurfaceView.Renderer: onSurfaceCreated, onSurfaceChanged. */
  int w = 1280, h = 720;
  dcr_window_size(&w, &h);
  if (g_nat.initGL)
    g_nat.initGL(env, g_lib_cls);
  g_nat.resize(env, g_lib_cls, w, h);
  /* App should exist from here on: allow setPaths now (also replays the
   * setupPaths callbacks that were deferred during the nativeInits above). */
  g_tdkr_inited = 1;
  tdkr_call_set_paths();
  g_running = 1;
  port_focus_gained(); /* stateChanged(1), onResume */
  dcr_watchdog_start();
  tdkr_gpu_boost_apply();
  debugPrintf("[boot] the game is running (%dx%d)\n", w, h);
  int show_fps = rt_config_value("display.show_fps", 1);
  u64 fps_t0 = armGetSystemTick();
  unsigned fps_n = 0;
  u64 flush_t0 = fps_t0;
  int log_quiet = 0;
  int first_frame = 1;

  while (!rt_exit_requested() && appletMainLoop()) {
    rt_applet_poll();
    if (!rt_focused()) {
      dcr_boost_idle(); /* HOME/sleep: no frame runs, none of it is a load */
      svcSleepThread(50000000ll);
      continue;
    }
    tdkr_input_poll();
    g_nat.step(env, g_lib_cls);
    tdkr_egl_swap();
    dcr_boost_poll(); /* a past-50 ms frame (a load) boosts until it ends */
    if (first_frame) {
      first_frame = 0;
      dcr_boost_launch_end(); /* start-up boost ends at the first picture */
    }
    /* Past the boot (180 pictures), the log goes to a RAM ring instead of
     * the SD card on every line: fewer stalls while playing. The ring is
     * written out every 10 s, and by the watchdog and the crash paths. */
    if (!log_quiet && dcr_gl_frames() > 180) {
      log_quiet = 1;
      dcr_io_report_readahead();
      dcr_io_report_patterns();
      log_set_quiet(1);
    }
    if (log_quiet && armTicksToNs(armGetSystemTick() - flush_t0) >= 10000000000ull) {
      flush_t0 = armGetSystemTick();
      tdkr_gpu_boost_tick(); /* dock/undock and option changes */
      dcr_boost_report(); /* boosted frames, if any are new */
      log_flush_ring();
    }
    if (show_fps) {
      /* Time-based (not frame-count-based): at load-time frame rates a
       * 120-frame window would take minutes to report. */
      fps_n++;
      u64 ns = armTicksToNs(armGetSystemTick() - fps_t0);
      if (ns >= 2000000000ull) {
        debugPrintf("[fps] %.1f (%llu ms/frame)\n", (double)fps_n * 1e9 / (double)ns,
                    (unsigned long long)(ns / 1000000ull / (fps_n ? fps_n : 1)));
        fps_t0 = armGetSystemTick();
        fps_n = 0;
      }
    }
  }
  rt_applet_stop();
  tdkr_gpu_boost_remove();
  if (log_quiet)
    log_set_quiet(0); /* writes out what the ring holds */
  log_flush_ring();
  g_running = 0; /* no onPause/destroy: the process ends, and the engine's teardown
                  * runs against threads and contexts that are going away */
}

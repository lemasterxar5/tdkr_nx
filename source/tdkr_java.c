/* tdkr_java.c -- the Java the engine talks to.
 *
 * Gameloft's GLF engine (libKRAS.so) calls back into three Java places:
 *   com/gameloft/glf/GL2JNIActivity   the activity: its view, the device,
 *                                     the stored ints, the OBB path ...
 *                                     (GL2JNILib answers the same: it is its
 *                                     "subclass" in the table below)
 *   .../GloftKRAS/GLUtils/SUtils, /Device, /SplashScreenActivity, /iab/...
 *   android/media/AudioTrack          the sound (tdkr_audio.c)
 * Every method the engine calls that is not here is answered by the runtime
 * with a default (0, false, null, ""), and logged once: [debug]
 * log_java_calls shows them all. Online services (Gameloft Live, the store,
 * the web view, ads) answer "nothing there". MIT.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "dcr_path.h"
#include "dcr_setup.h"
#include "rt_applet.h"
#include "rt_cfg.h"
#include "rt_window.h"
#include "tdkr.h"
#include "util.h"

#define ACT TDKR_CLS_ACT
#define LIB TDKR_CLS_LIB
#define AF "com/gameloft/glf/af"
#define SUTILS TDKR_CLS_BASE "/GLUtils/SUtils"
#define DEVICE TDKR_CLS_BASE "/GLUtils/Device"
#define SPLASH TDKR_CLS_BASE "/SplashScreenActivity"
#define IAB TDKR_CLS_BASE "/iab/InAppBilling"

TdkrNative g_nat;
void *g_lib_cls;
/* 1 once GL2JNILib.init has run (it creates glf::App). setupPaths arriving
 * earlier (Device/SUtils nativeInit re-enter it) must not call setPaths:
 * the lib would dereference App == NULL (far 0x100d4). */
int g_tdkr_inited;

/* ---------------------------------------------------------------- paths */
void tdkr_call_set_paths(void) {
  if (!g_nat.setPaths)
    return;
  if (!g_tdkr_inited) {
    debugPrintf("[java] setupPaths before App is ready: paths deferred\n");
    return;
  }
  debugPrintf("[java] setPaths res=%s home=%s tmp=%s\n", TDKR_OBB_ANDROID_DIR, TDKR_FILES_DIR,
              TDKR_CACHE_DIR);
  JObj *r = jni_str(TDKR_OBB_ANDROID_DIR), *h = jni_str(TDKR_FILES_DIR), *t = jni_str(TDKR_CACHE_DIR);
  g_nat.setPaths(g_jni_env, g_lib_cls, r, h, t);
  jni_release(r);
  jni_release(h);
  jni_release(t);
}

static jvalue h_setupPaths(JObj *self, const jvalue *a, const JMethod *m) {
  tdkr_call_set_paths();
  return jv_none();
}

/* The engine's Xperia/MOGA gamepad side (a8retry pattern): with the slide
 * open and a controller announced, OnKeyDown/Up route through the transfer
 * tables and the right stick lands in the game's own globals. The flags are
 * the lib's own; calling the natives is safe any time after load. */
int g_tdkr_native_pad;

void tdkr_gamepad_native_init(void) {
  tdkr_gamepad_set_native(rt_config_value("controls.gamepad_native", 1));
}

void tdkr_gamepad_set_native(int on) {
  on = on ? 1 : 0;
  g_tdkr_native_pad = on;
  if (g_nat.setXperiaPlay)
    g_nat.setXperiaPlay(g_jni_env, g_lib_cls, on);
  if (g_nat.slideChanged)
    g_nat.slideChanged(g_jni_env, g_lib_cls, on);
  if (g_nat.powerAConnected)
    g_nat.powerAConnected(g_jni_env, g_lib_cls, on);
  debugPrintf("[input] native gamepad %s (xperia=%d slide=%d powera=%d rjoy=%d keys=%d touch=%p)\n",
              on ? "on" : "off", !!g_nat.setXperiaPlay, !!g_nat.slideChanged, !!g_nat.powerAConnected,
              !!g_nat.powerARightJoy, !!g_nat.onKeyDown && !!g_nat.onKeyUp, (void *)g_nat.touchEvent);
}

/* A held right stick, as the game reads it (a8retry's 12%/85% curve). */
void tdkr_gamepad_stick(float x, float y) {
  static float lx = 9, ly = 9;
  float ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
  float nx = ax < 0.12f ? 0.0f : ax > 0.85f ? 1.0f : (ax - 0.12f) / (0.85f - 0.12f);
  float ny = ay < 0.12f ? 0.0f : ay > 0.85f ? 1.0f : (ay - 0.12f) / (0.85f - 0.12f);
  nx = x < 0 ? -nx : nx;
  ny = y < 0 ? -ny : ny;
  if (nx == lx && ny == ly)
    return;
  lx = nx;
  ly = ny;
  if (g_nat.powerARightJoy)
    g_nat.powerARightJoy(g_jni_env, g_lib_cls, nx, ny);
}

/* The expansion file: its Android path (the runtime sends it to <game folder>/obb). */
static jvalue h_getOBB(JObj *self, const jvalue *a, const JMethod *m) {
  int patch = a[0].z;
  char found[DCR_PATH_MAX];
  const char *name = patch ? "patchkrhm.obb" : "datakrhm.obb";
  if (tdkr_obb_find(patch, found, sizeof found) && strrchr(found, '/'))
    name = strrchr(found, '/') + 1;
  return jv_l(jni_str_fmt("%s/%s", TDKR_OBB_ANDROID_DIR, name));
}

/* ------------------------------------------------------------ resources */
/* getResource(name): a file of the APK's assets, as bytes. */
static Mutex g_zip_lock;
static mz_zip_archive g_zip;
static int g_zip_state; /* 0 not tried, 1 open, -1 failed */

static jvalue h_getResource(JObj *self, const jvalue *a, const JMethod *m) {
  const char *name = jni_utf(a[0].l);
  void *p = NULL;
  size_t sz = 0;
  mutexLock(&g_zip_lock);
  if (!g_zip_state) {
    memset(&g_zip, 0, sizeof g_zip);
    g_zip_state = mz_zip_reader_init_file(&g_zip, dcr_apk_path(), 0) ? 1 : -1;
  }
  if (g_zip_state > 0) {
    char path[300];
    snprintf(path, sizeof path, "assets/%s", name);
    p = mz_zip_reader_extract_file_to_heap(&g_zip, path, &sz, 0);
    if (!p)
      p = mz_zip_reader_extract_file_to_heap(&g_zip, name, &sz, 0);
  }
  mutexUnlock(&g_zip_lock);
  if (!p) {
    debugPrintf("[java] getResource(%s): not in the APK\n", name);
    return jv_l(NULL);
  }
  JObj *arr = jni_array('B', (jsize)sz);
  if (arr && arr->a.data)
    memcpy(arr->a.data, p, sz);
  mz_free(p);
  return jv_l(arr);
}

/* --------------------------------------------------------- stored ints */
#define MAX_INTS 64
static struct {
  char key[48];
  int val;
} g_ints[MAX_INTS];
static int g_nints, g_ints_loaded;
static Mutex g_ints_lock;

static void ints_path(char *out, size_t cap) { rt_root_path(out, cap, "data/stored_ints.txt"); }

static void ints_load(void) {
  if (g_ints_loaded)
    return;
  g_ints_loaded = 1;
  char path[320];
  ints_path(path, sizeof path);
  size_t len = 0;
  uint8_t *buf = rt_read_whole(path, &len);
  if (!buf)
    return;
  for (char *line = strtok((char *)buf, "\n"); line && g_nints < MAX_INTS; line = strtok(NULL, "\n")) {
    char *sp = strrchr(line, ' ');
    if (!sp || sp == line || sp - line >= (int)sizeof g_ints[0].key)
      continue;
    memcpy(g_ints[g_nints].key, line, (size_t)(sp - line));
    g_ints[g_nints++].val = atoi(sp + 1);
  }
  free(buf);
}

static jvalue h_getStoredInt(JObj *self, const jvalue *a, const JMethod *m) {
  const char *k = jni_utf(a[0].l);
  int v = a[1].i;
  mutexLock(&g_ints_lock);
  ints_load();
  for (int i = 0; i < g_nints; i++)
    if (!strcmp(g_ints[i].key, k))
      v = g_ints[i].val;
  mutexUnlock(&g_ints_lock);
  return jv_i(v);
}

static jvalue h_putStoredInt(JObj *self, const jvalue *a, const JMethod *m) {
  const char *k = jni_utf(a[0].l);
  if (!*k || strlen(k) >= sizeof g_ints[0].key)
    return jv_none();
  mutexLock(&g_ints_lock);
  ints_load();
  int i = 0;
  while (i < g_nints && strcmp(g_ints[i].key, k))
    i++;
  if (i == g_nints && g_nints < MAX_INTS)
    snprintf(g_ints[g_nints++].key, sizeof g_ints[0].key, "%s", k);
  if (i < g_nints) {
    g_ints[i].val = a[1].i;
    char buf[MAX_INTS * 64], path[320];
    size_t n = 0;
    for (int j = 0; j < g_nints; j++)
      n += (size_t)snprintf(buf + n, sizeof buf - n, "%s %d\n", g_ints[j].key, g_ints[j].val);
    ints_path(path, sizeof path);
    rt_write_atomic(path, buf, n);
  }
  mutexUnlock(&g_ints_lock);
  return jv_none();
}

/* -------------------------------------------------------------- the view */
static jvalue h_setViewSettings(JObj *self, const jvalue *a, const JMethod *m) {
  tdkr_egl_view_settings(a[0].i, a[1].i, a[2].i, a[3].i);
  return jv_none();
}
static jvalue h_createView(JObj *self, const jvalue *a, const JMethod *m) {
  tdkr_egl_ensure();
  return jv_none();
}
static jvalue h_setCurrentContext(JObj *self, const jvalue *a, const JMethod *m) {
  return jv_z(tdkr_egl_set_current(a[0].i));
}
static jvalue h_winW(JObj *self, const jvalue *a, const JMethod *m) {
  int w = 1280, h = 720;
  dcr_window_size(&w, &h);
  return jv_i(w);
}
static jvalue h_winH(JObj *self, const jvalue *a, const JMethod *m) {
  int w = 1280, h = 720;
  dcr_window_size(&w, &h);
  return jv_i(h);
}

/* ------------------------------------------------------------- the device */
static jvalue h_manufacturer(JObj *self, const jvalue *a, const JMethod *m) { return jv_l(jni_str("NVIDIA")); }
static jvalue h_deviceName(JObj *self, const jvalue *a, const JMethod *m) { return jv_l(jni_str("SHIELD Android TV")); }
static jvalue h_firmware(JObj *self, const jvalue *a, const JMethod *m) { return jv_l(jni_str("6.0.1")); }
static jvalue h_cores(JObj *self, const jvalue *a, const JMethod *m) { return jv_i(4); }
static jvalue h_cpu_khz(JObj *self, const jvalue *a, const JMethod *m) { return jv_i(1785000); }
static jvalue h_ram_max(JObj *self, const jvalue *a, const JMethod *m) { return jv_i(3072); }
static jvalue h_ram_free(JObj *self, const jvalue *a, const JMethod *m) { return jv_i(2048); }
static jvalue h_disk(JObj *self, const jvalue *a, const JMethod *m) { return jv_i(2000000); }
static jvalue h_mac(JObj *self, const jvalue *a, const JMethod *m) { return jv_l(jni_str("02:00:00:00:00:00")); }
static jvalue h_udid(JObj *self, const jvalue *a, const JMethod *m) { return jv_l(jni_str("tdkrnx0000000001")); }
/* Xperia slide / ZEUS phone: open when the native pad is on (its flags are
 * announced to the lib at boot); shut otherwise. */
static jvalue h_xperia(JObj *self, const jvalue *a, const JMethod *m) { return jv_z(g_tdkr_native_pad); }
static jvalue h_lang(JObj *self, const jvalue *a, const JMethod *m) { return jv_l(jni_str("en")); }
static jvalue h_useragent(JObj *self, const jvalue *a, const JMethod *m) {
  return jv_l(jni_str("Mozilla/5.0 (Linux; Android 6.0.1; SHIELD Android TV Build/MRA58K) AppleWebKit/537.36"));
}
static jvalue h_millis(JObj *self, const jvalue *a, const JMethod *m) {
  jlong ms = (jlong)(armTicksToNs(armGetSystemTick()) / 1000000ull);
  return m->ret == 'J' ? jv_j(ms) : jv_i((jint)ms);
}
static jvalue h_exit(JObj *self, const jvalue *a, const JMethod *m) {
  rt_request_exit();
  return jv_none();
}
static jvalue h_prefstring(JObj *self, const jvalue *a, const JMethod *m) {
  return jv_l(a[1].l ? jni_retain(a[1].l) : jni_str(""));
}
static jvalue h_package(JObj *self, const jvalue *a, const JMethod *m) { return jv_l(jni_str(PORT_PACKAGE)); }
static jvalue h_savefolder(JObj *self, const jvalue *a, const JMethod *m) { return jv_l(jni_str(TDKR_FILES_DIR)); }
static jvalue h_context(JObj *self, const jvalue *a, const JMethod *m) { return jv_l(jni_singleton("android/content/Context")); }

/* ---------------------------------------------------------------- tables */
const JMethodDef jni_method_defs[] = {
    /* the activity */
    {ACT, "getResource", NULL, h_getResource},
    {ACT, "setupPaths", NULL, h_setupPaths},
    {ACT, "createView", NULL, h_createView},
    {ACT, "setViewSettings", NULL, h_setViewSettings},
    {ACT, "setCurrentContext", NULL, h_setCurrentContext},
    {ACT, "GetWindowWidth", NULL, h_winW},
    {ACT, "GetWindowHeight", NULL, h_winH},
    {ACT, "enableAccelerometer", NULL, jni_h_void},
    {ACT, "JGetFreeDiskSpace", NULL, h_disk},
    {ACT, "GetManufacturer", NULL, h_manufacturer},
    {ACT, "GetDeviceName", NULL, h_deviceName},
    {ACT, "GetDeviceFirmware", NULL, h_firmware},
    {ACT, "JGetNumberOfCores", NULL, h_cores},
    {ACT, "JGetMaxCPUSpeed", NULL, h_cpu_khz},
    {ACT, "JGetCurrentCPUSpeed", NULL, h_cpu_khz},
    {ACT, "JGetMaxAvailableRam", NULL, h_ram_max},
    {ACT, "JGetFreeRam", NULL, h_ram_free},
    {ACT, "isWifiOn", NULL, jni_h_false},
    {ACT, "IsWifiEnabled", NULL, jni_h_false},
    {ACT, "sIsKeyboardVisible", NULL, jni_h_false},
    {ACT, "sGetOBBFilename", NULL, h_getOBB},
    {ACT, "sGetMAC", NULL, h_mac},
    {ACT, "sGetUDID", NULL, h_udid},
    {ACT, "sGetPhoneLanguage", NULL, h_lang},
    {ACT, "sGetStoredInt", NULL, h_getStoredInt},
    {ACT, "sPutStoredInt", NULL, h_putStoredInt},
    {ACT, "sGetMilliseconds", NULL, h_millis},
    {ACT, "sExitGame", NULL, h_exit},
    /* online services, ads, the keyboard, the screen: nothing to do */
    {ACT, "sGLLiveLaunch", NULL, jni_h_void},
    {ACT, "sGLLiveNotifyTrophy", NULL, jni_h_void},
    {ACT, "sGLLiveWelcome", NULL, jni_h_void},
    {ACT, "sIGPLaunch", NULL, jni_h_void},
    {ACT, "sLaunchWelcomeScreen", NULL, jni_h_void},
    {ACT, "sBrowserLaunch", NULL, jni_h_void},
    {ACT, "sShowKeyboard", NULL, jni_h_void},
    {ACT, "sShowToast", NULL, jni_h_void},
    {ACT, "sShowAlert", NULL, jni_h_void},
    {ACT, "sKeepScreenOn", NULL, jni_h_void},
    {ACT, "sSendToBackground", NULL, jni_h_void},
    {ACT, "FullScreenToggleShowBar", NULL, jni_h_void},
    {ACT, "FullScreenToggleHideBar", NULL, jni_h_void},
    {ACT, "EnableOrientation", NULL, jni_h_void},
    {ACT, "SetOrientation", NULL, jni_h_void},
    /* the game's own helper classes */
    {SUTILS, "getPreferenceString", NULL, h_prefstring},
    {SUTILS, "getPackage", NULL, h_package},
    {SUTILS, "getSaveFolder", NULL, h_savefolder},
    {SUTILS, "getContext", NULL, h_context},
    {DEVICE, "getUserAgent", NULL, h_useragent},
    {DEVICE, "getAndroidId", NULL, h_udid},
    {DEVICE, "getPackage", NULL, h_package},
    {DEVICE, "getContext", NULL, h_context},
    {DEVICE, "shareInfo", NULL, jni_h_void},
    {LIB, "isSlideEnabled", NULL, h_xperia},
    {LIB, "isZEUSDevice", NULL, h_xperia},
    {AF, "isSlideEnabled", NULL, h_xperia},
    {AF, "isZEUSDevice", NULL, h_xperia},
    {SPLASH, NULL, NULL, jni_h_zero},
    {IAB, NULL, NULL, jni_h_zero},
    {"android/os/Process", "setThreadPriority", NULL, jni_h_void},
    /* the sound */
    {"android/media/AudioTrack", "<init>", "(IIIIII)V", tdkr_at_init},
    {"android/media/AudioTrack", "getMinBufferSize", NULL, tdkr_at_min_buffer},
    {"android/media/AudioTrack", "getNativeOutputSampleRate", NULL, tdkr_at_native_rate},
    {"android/media/AudioTrack", "play", NULL, tdkr_at_play},
    {"android/media/AudioTrack", "pause", NULL, tdkr_at_pause},
    {"android/media/AudioTrack", "stop", NULL, tdkr_at_stop},
    {"android/media/AudioTrack", "release", NULL, tdkr_at_release},
    {"android/media/AudioTrack", "write", NULL, tdkr_at_write},
    {"android/media/AudioTrack", "getPlayState", NULL, tdkr_at_state},
    {NULL, NULL, NULL, NULL},
};

const JFieldDef jni_field_defs[] = {
    {SUTILS, "SDFolder", NULL, 0, "/sdcard/gameloft/games/GloftKRAS"},
    {SUTILS, "mPreferencesName", NULL, 0, "GloftKRAS"},
    {DEVICE, "SDFolder", NULL, 0, "/sdcard/gameloft/games/GloftKRAS"},
    {DEVICE, "mPreferencesName", NULL, 0, "GloftKRAS"},
    {NULL, NULL, NULL, 0, NULL},
};

const char *const jni_class_supers[][2] = {
    {LIB, ACT},
    {TDKR_CLS_BASE "/GameActivity", ACT},
    {ACT, "android/app/Activity"},
    {NULL, NULL},
};

/* Classes a phone running this APK would NOT have (used only while classes.txt,
 * the APK's own class list, is missing): the Google / Amazon services. */
const char *const jni_missing_classes[] = {
    "com/google/android/gms/*", "com/android/vending/*", "com/amazon/*", NULL,
};

/* -------------------------------------------------------------- monitors */
/* JNIEnv's MonitorEnter and MonitorExit, which the runtime answers without
 * locking anything (jni_core.c: both return 0). The Dead Space port found
 * what that costs: an engine whose loading threads share buffers through a
 * monitor reads each other's bytes -- a model that loads as nothing, a size
 * of a gigabyte, a fault somewhere in the loader on some starts and not on
 * others. libKRAS.so streams its zones and textures the same way; one lock
 * per object locked (few, never let go), patched into the JNI table's
 * standard slots (217/218: the runtime's enum is the standard 233-entry
 * JNINativeInterface). Uncontended it costs one mutex round-trip. */
#define JNI_MONITOR_ENTER 217 /* JNINativeInterface's slots */
#define JNI_MONITOR_EXIT  218
#define MAX_MONITORS 16

static struct {
  const void *obj;
  RMutex lock;
} g_monitors[MAX_MONITORS];
static int g_nmonitors;
static Mutex g_monitors_lock;

static RMutex *monitor_of(const void *obj) {
  mutexLock(&g_monitors_lock);
  int i = 0;
  while (i < g_nmonitors && g_monitors[i].obj != obj)
    i++;
  if (i == MAX_MONITORS) {
    i--; /* more objects than locks: the last one is shared */
  } else if (i == g_nmonitors) {
    g_monitors[i].obj = obj;
    rmutexInit(&g_monitors[i].lock);
    g_nmonitors++;
    debugPrintf("[java] a monitor for %p\n", obj);
  }
  mutexUnlock(&g_monitors_lock);
  return &g_monitors[i].lock;
}

static int monitor_enter(void *env, void *obj) {
  (void)env;
  rmutexLock(monitor_of(obj));
  return 0;
}

static int monitor_exit(void *env, void *obj) {
  (void)env;
  rmutexUnlock(monitor_of(obj));
  return 0;
}

/* After jni_init(): the engine's synchronized blocks lock for real. */
void tdkr_java_patch_monitors(void) {
  mutexInit(&g_monitors_lock);
  void **env = *(void ***)g_jni_env; /* the function table */
  env[JNI_MONITOR_ENTER] = (void *)monitor_enter;
  env[JNI_MONITOR_EXIT] = (void *)monitor_exit;
}

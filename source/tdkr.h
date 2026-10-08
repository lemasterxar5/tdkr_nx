/* tdkr.h -- what the port's files share. MIT. */
#ifndef TDKR_H
#define TDKR_H

#include <stddef.h>
#include <stdint.h>

#include "jni.h"

/* The Java classes the engine calls. */
#define TDKR_CLS_ACT  "com/gameloft/glf/GL2JNIActivity"
#define TDKR_CLS_LIB  "com/gameloft/glf/GL2JNILib"
#define TDKR_CLS_BASE "com/gameloft/android/AMAZ/GloftKRAS"

/* ---------------------------------------------------------------- tdkr_obb.c */
/* The expansion file of a role (0 main, 1 patch) in <game folder>/obb: its
 * path (SD, with "sdmc:") in out; 1 when there is one. */
int tdkr_obb_find(int patch, char *out, size_t cap);
/* Menu vs gameplay as the wrapper sees it (zone files): 1 in l_menu. */
void tdkr_zone_note(const char *path);
int tdkr_zone_is_menu(void);
/* The Android paths the game gets from Java (setPaths), as it would have
 * them: /sdcard/Android/obb/<package> is <game folder>/obb. */
#define TDKR_OBB_ANDROID_DIR "/sdcard/Android/obb/" PORT_PACKAGE
#define TDKR_FILES_DIR "/data/data/" PORT_PACKAGE "/files"
#define TDKR_CACHE_DIR "/data/data/" PORT_PACKAGE "/cache"

/* ---------------------------------------------------------------- tdkr_gl.c */
/* The EGL display, config, window surface and context the Java side of the
 * engine (GLSurfaceView) would have made. Idempotent. 1 when the main
 * context is current on the calling thread. */
int tdkr_egl_ensure(void);
/* setViewSettings(): what the engine asked for (pixel bits, depth, stencil, CSAA). */
void tdkr_egl_view_settings(int pixel, int depth, int stencil, int csaa);
/* setCurrentContext(id): 0 the main context, 1.. the extra shared ones. */
int tdkr_egl_set_current(int id);
/* eglSwapBuffers on the window surface (through the runtime's frame hooks). */
void tdkr_egl_swap(void);
/* The centring crosshair, drawn over the finished frame (gameplay only). */
void tdkr_crosshair_draw(void);

/* ---------------------------------------------------------------- tdkr_audio.c */
/* android.media.AudioTrack over audout; the handlers are in tdkr_java.c's table. */
JNI_H_DECL(tdkr_at_init);
JNI_H_DECL(tdkr_at_min_buffer);
JNI_H_DECL(tdkr_at_native_rate);
JNI_H_DECL(tdkr_at_play);
JNI_H_DECL(tdkr_at_pause);
JNI_H_DECL(tdkr_at_stop);
JNI_H_DECL(tdkr_at_release);
JNI_H_DECL(tdkr_at_write);
JNI_H_DECL(tdkr_at_state);
void tdkr_audio_pause(int paused);

/* ---------------------------------------------------------------- tdkr_input.c */
/* Called once per frame, before step(): the touch screen as touchEvent()s and
 * the buttons as key events. */
void tdkr_input_init(void);
void tdkr_input_refresh(void); /* rebuild binds after a native toggle/init */
void tdkr_input_poll(void);

/* ---------------------------------------------------------------- tdkr_java.c */
/* The engine's entry points (libKRAS.so's Java_com_gameloft_glf_GL2JNILib_*),
 * resolved by tdkr_main.c. Static natives: (env, class, args...). */
typedef struct TdkrNative {
  void (*setPaths)(void *env, void *cls, void *res, void *home, void *tmp);
  void (*init)(void *env, void *cls);
  void (*initViewSettings)(void *env, void *cls);
  void (*initGL)(void *env, void *cls);
  void (*resize)(void *env, void *cls, int w, int h);
  void (*step)(void *env, void *cls);
  void (*stateChanged)(void *env, void *cls, int active);
  void (*onResume)(void *env, void *cls);
  void (*onPause)(void *env, void *cls);
  void (*destroy)(void *env, void *cls);
  void (*touchEvent)(void *env, void *cls, int action, int x, int y, int id);
  void (*onKeyDown)(void *env, void *cls, int key);
  void (*onKeyUp)(void *env, void *cls, int key);
  void (*splashMsg)(void *env, void *cls, void *msg);
  void (*setXperiaPlay)(void *env, void *cls, int on);
  void (*slideChanged)(void *env, void *cls, int open);
  void (*powerAConnected)(void *env, void *cls, int connected);
  void (*powerARightJoy)(void *env, void *cls, float x, float y);
} TdkrNative;
extern TdkrNative g_nat;
extern void *g_lib_cls; /* the GL2JNILib class object, as the natives take it */
extern int g_tdkr_inited; /* 1 once GL2JNILib.init has created glf::App */
extern int g_tdkr_native_pad; /* [controls] gamepad_native */

/* Announce an Xperia (slide open) + PowerA controller to the lib. Safe any
 * time after load; needs nothing from Java. */
void tdkr_gamepad_native_init(void);
/* Switch the announce on/off at runtime (menus vs gameplay). */
void tdkr_gamepad_set_native(int on);
/* Right stick state for the game's own globals (no-op unless announced). */
void tdkr_gamepad_stick(float x, float y);

/* The paths handed to setPaths(), by the first call and by Java's setupPaths(). */
void tdkr_call_set_paths(void);

/* After jni_init(): MonitorEnter/Exit lock for real (the runtime's are
 * no-ops; loading threads would read each other's shared buffers). */
void tdkr_java_patch_monitors(void);

/* ---------------------------------------------------------------- tdkr_gpu.c */
/* Handheld GPU boost (460.8 MHz, MEM/CPU stock; docked is untouched). */
void tdkr_gpu_boost_apply(void);  /* once, when the game is up */
void tdkr_gpu_boost_tick(void);   /* dock/undock and option changes */
void tdkr_gpu_boost_remove(void); /* back to the saved rate */

#endif /* TDKR_H */

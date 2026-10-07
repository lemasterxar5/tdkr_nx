/* tdkr_gl.c -- the EGL side of the engine's Java activity.
 *
 * libKRAS.so imports no egl*: Gameloft's GLF engine draws into the context
 * that Java's GLSurfaceView made, and asks Java for it through
 *   setViewSettings(pixel, depth, stencil, csaa, ...)   what it wants
 *   createView()                                        make it
 *   setCurrentContext(i)                                bind context i here
 * and the swap after each step() is GLSurfaceView's. This file is that Java:
 * one window surface on the default NWindow with the main context, extra
 * shared contexts made on demand (a loader thread's setCurrentContext(1)),
 * and the swap. It calls the runtime's own EGL shims (gl_mesa.c: b_egl*), so
 * the frame hooks, the boost and the frame count see every frame. MIT.
 */
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <switch.h>

#include "gl_layer.h"
#include "rt_cfg.h"
#include "rt_settings.h"
#include "rt_window.h"
#include "tdkr.h"
#include "util.h"

/* gl_mesa.c / gl_null.c */
EGLDisplay b_eglGetDisplay(EGLNativeDisplayType d);
EGLBoolean b_eglInitialize(EGLDisplay d, EGLint *maj, EGLint *min);
EGLBoolean b_eglChooseConfig(EGLDisplay d, const EGLint *attrs, EGLConfig *cfgs, EGLint cap, EGLint *num);
EGLContext b_eglCreateContext(EGLDisplay d, EGLConfig c, EGLContext share, const EGLint *attrs);
EGLSurface b_eglCreateWindowSurface(EGLDisplay d, EGLConfig c, EGLNativeWindowType w, const EGLint *attrs);
EGLBoolean b_eglMakeCurrent(EGLDisplay d, EGLSurface dr, EGLSurface rd, EGLContext c);
EGLBoolean b_eglSwapBuffers(EGLDisplay d, EGLSurface s);
EGLBoolean b_eglSwapInterval(EGLDisplay d, EGLint i);
EGLint b_eglGetError(void);

static Mutex g_lock;
static EGLDisplay g_dpy = EGL_NO_DISPLAY;
static EGLConfig g_cfg;
static EGLSurface g_surf = EGL_NO_SURFACE;
static EGLContext g_ctx[1];
static int g_ready;
static struct { int pixel, depth, stencil, csaa; } g_vs = {32, 24, 8, 0};

void tdkr_egl_view_settings(int pixel, int depth, int stencil, int csaa) {
  g_vs.pixel = pixel;
  g_vs.depth = depth;
  g_vs.stencil = stencil;
  g_vs.csaa = csaa;
  debugPrintf("[gl] the engine asks for pixel %d, depth %d, stencil %d, CSAA %d\n", pixel, depth, stencil, csaa);
}

/* Display, config, surface and the main context; g_lock held. */
static int make_main(void) {
  g_dpy = b_eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint maj = 0, min = 0, n = 0;
  if (!b_eglInitialize(g_dpy, &maj, &min)) {
    debugPrintf("[gl] eglInitialize failed 0x%x\n", b_eglGetError());
    return 0;
  }
  eglBindAPI(EGL_OPENGL_ES_API);
  /* RGB 8-8-8 whatever pixelSize says (Mesa's window formats are 32-bit); the
   * depth and stencil as asked, never less than 16 / more than 24 bits. */
  EGLint depth = g_vs.depth > 16 ? 24 : 16;
  EGLint attrs[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
                    EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, depth, EGL_STENCIL_SIZE, g_vs.stencil > 0 ? 8 : 0,
                    EGL_NONE};
  if (!b_eglChooseConfig(g_dpy, attrs, &g_cfg, 1, &n) || n < 1) {
    debugPrintf("[gl] no EGL config (0x%x)\n", b_eglGetError());
    return 0;
  }
  g_surf = b_eglCreateWindowSurface(g_dpy, g_cfg, 0, NULL);
  static const EGLint ctx_attrs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  g_ctx[0] = b_eglCreateContext(g_dpy, g_cfg, EGL_NO_CONTEXT, ctx_attrs);
  if (g_surf == EGL_NO_SURFACE || g_ctx[0] == EGL_NO_CONTEXT) {
    debugPrintf("[gl] surface %p / context %p not made (0x%x)\n", g_surf, g_ctx[0], b_eglGetError());
    return 0;
  }
  /* the screen refreshes at 60 Hz: interval 1 = 60 fps, 2 = 30 fps ([display] frame_rate) */
  b_eglSwapInterval(g_dpy, rt_config_int("display", "frame_rate") == 1 ? 2 : 1);
  g_ready = 1;
  debugPrintf("[gl] EGL %d.%d: window surface, GLES 2 context (depth %d, stencil %d)\n", (int)maj, (int)min,
              (int)depth, g_vs.stencil > 0 ? 8 : 0);
  return 1;
}

int tdkr_egl_ensure(void) {
  mutexLock(&g_lock);
  int ok = g_ready || make_main();
  if (ok && eglGetCurrentContext() != g_ctx[0])
    ok = b_eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx[0]) == EGL_TRUE;
  mutexUnlock(&g_lock);
  return ok;
}

int tdkr_egl_set_current(int id) {
  if (id <= 0)
    return tdkr_egl_ensure();
  /* The platform has window surfaces only and one thread may hold the window's
   * context: extra (loader-thread) contexts are refused and the engine, which
   * handles "failed setting context", loads on its main thread. The engine is
   * not told to start loader threads (setNumExtraContext is never called). */
  static int logged;
  if (!logged++)
    debugPrintf("[gl] setCurrentContext(%d) refused (no extra contexts)\n", id);
  return 0;
}

void tdkr_egl_swap(void) {
  if (g_ready) {
    tdkr_crosshair_draw();
    b_eglSwapBuffers(g_dpy, g_surf);
  }
}

/* A small centring crosshair in the middle of the screen (gameplay only):
 * four scissored clears over the finished frame, engine GL state saved and
 * put back. [display] crosshair disables it. */
void tdkr_crosshair_draw(void) {
  static int on = -1;
  if (on < 0)
    on = rt_config_bool("display", "crosshair");
  if (!on || tdkr_zone_is_menu())
    return;
  if (eglGetCurrentContext() != g_ctx[0])
    return;
  int w = 1280, h = 720;
  dcr_window_size(&w, &h);
  GLboolean se = glIsEnabled(GL_SCISSOR_TEST);
  GLint box[4];
  GLfloat cc[4];
  glGetIntegerv(GL_SCISSOR_BOX, box);
  glGetFloatv(GL_COLOR_CLEAR_VALUE, cc);
  int cx = w / 2, cy = h / 2;
  glEnable(GL_SCISSOR_TEST);
  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  glScissor(cx - 6, cy - 1, 4, 2);
  glClear(GL_COLOR_BUFFER_BIT);
  glScissor(cx + 2, cy - 1, 4, 2);
  glClear(GL_COLOR_BUFFER_BIT);
  glScissor(cx - 1, cy - 6, 2, 4);
  glClear(GL_COLOR_BUFFER_BIT);
  glScissor(cx - 1, cy + 2, 2, 4);
  glClear(GL_COLOR_BUFFER_BIT);
  glScissor(box[0], box[1], box[2], box[3]);
  glClearColor(cc[0], cc[1], cc[2], cc[3]);
  if (!se)
    glDisable(GL_SCISSOR_TEST);
}

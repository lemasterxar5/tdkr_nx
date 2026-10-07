/* tdkr_input.c -- the Switch's inputs for the engine.
 *
 * The game is played on a touch screen; the Joy-Cons drive it through the
 * reusable touchmap module (touchmap.h): the left stick or D-pad drags the
 * virtual joystick, the right stick swipes to look, and every button taps a
 * right-side control and/or sends an Android key. Positions are 1280x720
 * config.ini values (see tdkr_config.c); the real touch screen shares the
 * same finger pool. Driving uses a slider bar: use the touch screen there.
 * MIT.
 */
#include <math.h>
#include <string.h>
#include <switch.h>

#include "rt_cfg.h"
#include "rt_pad.h"
#include "rt_window.h"
#include "tdkr.h"
#include "touchmap.h"
#include "util.h"

static PadState g_pad;
static u64 g_prev;
static TmPad g_tm;
static TmBind g_binds[20];
static u32 g_seen[16];
static int g_nseen;
static int g_keylog;
static int g_invert_y;
static u64 g_skeys;

static void build_binds(void);

static void tm_touch(int action, int x, int y, int id) {
  if (g_nat.touchEvent)
    g_nat.touchEvent(g_jni_env, g_lib_cls, action, x, y, id);
}

static void tm_key(int keycode, int down) {
  if (g_keylog < 100) {
    g_keylog++;
    debugPrintf("[input] key %d %s\n", keycode, down ? "down" : "up");
  }
  if (down && g_nat.onKeyDown)
    g_nat.onKeyDown(g_jni_env, g_lib_cls, keycode);
  if (!down && g_nat.onKeyUp)
    g_nat.onKeyUp(g_jni_env, g_lib_cls, keycode);
}

void tdkr_input_init(void) {
  hidInitializeTouchScreen();
  rt_pad_setup(1, 1);
  /* Any controller (handheld Joy-Cons included): slot 0 alone is empty in
   * handheld mode, which left sticks AND buttons dead silent. */
  padInitializeDefault(&g_pad);
  tm_init(tm_touch, tm_key);
  build_binds();
}

static void build_binds(void) {
  int touch = rt_config_value("controls.gamepad_touch", 1);
  int keys = rt_config_bool("controls", "gamepad_keys");
  g_invert_y = rt_config_value("controls.gamepad_invert_y", 1);
  int ax = rt_config_value("controls.touch_btn_attack_x", 1150);
  int ay = rt_config_value("controls.touch_btn_attack_y", 550);
  int jx = rt_config_value("controls.touch_btn_jump_x", 1030);
  int jy = rt_config_value("controls.touch_btn_jump_y", 610);
  int cx = rt_config_value("controls.touch_btn_counter_x", 1150);
  int cy = rt_config_value("controls.touch_btn_counter_y", 420);
  int ux = rt_config_value("controls.touch_btn_use_x", 1030);
  int uy = rt_config_value("controls.touch_btn_use_y", 480);

  /* Binds: menus as keys always; gameplay as taps (touch) and, with the
   * native pad announced, as SHIELD key codes (A96 B97 X99 Y100 L102 R103
   * ZL104 ZR105 StickL106 StickR107 Plus108 Minus4 D-pad 19-22; A also 23). */
  int native = g_tdkr_native_pad;
  int n = 0;
  if (native) {
    g_binds[n++] = (TmBind){HidNpadButton_Up, -1, -1, keys ? 19 : -1};
    g_binds[n++] = (TmBind){HidNpadButton_Down, -1, -1, keys ? 20 : -1};
    g_binds[n++] = (TmBind){HidNpadButton_Left, -1, -1, keys ? 21 : -1};
    g_binds[n++] = (TmBind){HidNpadButton_Right, -1, -1, keys ? 22 : -1};
    g_binds[n++] = (TmBind){HidNpadButton_Plus, -1, -1, keys ? 108 : -1};
    g_binds[n++] = (TmBind){HidNpadButton_Minus, -1, -1, keys ? 4 : -1};
    g_binds[n++] = (TmBind){HidNpadButton_A, jx, jy, 96};
    g_binds[n++] = (TmBind){HidNpadButton_A, -1, -1, 23};
    g_binds[n++] = (TmBind){HidNpadButton_R, ax, ay, 103};
    g_binds[n++] = (TmBind){HidNpadButton_StickR, ax, ay, 107};
    g_binds[n++] = (TmBind){HidNpadButton_B, ax, ay, 97};
    g_binds[n++] = (TmBind){HidNpadButton_L, jx, jy, 102};
    g_binds[n++] = (TmBind){HidNpadButton_X, cx, cy, 99};
    g_binds[n++] = (TmBind){HidNpadButton_ZR, cx, cy, 105};
    g_binds[n++] = (TmBind){HidNpadButton_Y, ux, uy, 100};
    g_binds[n++] = (TmBind){HidNpadButton_ZL, ux, uy, 104};
    g_binds[n++] = (TmBind){HidNpadButton_StickL, ux, uy, 106};
  } else {
    g_binds[n++] = (TmBind){HidNpadButton_Up, -1, -1, keys ? 19 : -1};
    g_binds[n++] = (TmBind){HidNpadButton_Down, -1, -1, keys ? 20 : -1};
    g_binds[n++] = (TmBind){HidNpadButton_Left, -1, -1, keys ? 21 : -1};
    g_binds[n++] = (TmBind){HidNpadButton_Right, -1, -1, keys ? 22 : -1};
    g_binds[n++] = (TmBind){HidNpadButton_Plus, -1, -1, keys ? 82 : -1};
    g_binds[n++] = (TmBind){HidNpadButton_Minus, -1, -1, keys ? 4 : -1};
    if (touch) {
      g_binds[n++] = (TmBind){HidNpadButton_A, jx, jy, 23};
      g_binds[n++] = (TmBind){HidNpadButton_R, ax, ay, -1};
      g_binds[n++] = (TmBind){HidNpadButton_StickR, ax, ay, -1};
      g_binds[n++] = (TmBind){HidNpadButton_B, ax, ay, -1};
      g_binds[n++] = (TmBind){HidNpadButton_L, jx, jy, -1};
      g_binds[n++] = (TmBind){HidNpadButton_X, cx, cy, -1};
      g_binds[n++] = (TmBind){HidNpadButton_ZR, cx, cy, -1};
      g_binds[n++] = (TmBind){HidNpadButton_Y, ux, uy, -1};
      g_binds[n++] = (TmBind){HidNpadButton_ZL, ux, uy, -1};
      g_binds[n++] = (TmBind){HidNpadButton_StickL, ux, uy, -1};
    } else {
      g_binds[n++] = (TmBind){HidNpadButton_A, -1, -1, 23};
      g_binds[n++] = (TmBind){HidNpadButton_B, -1, -1, 4};
    }
  }
  g_tm.enable = touch || keys;
  g_tm.touch = touch;
  g_tm.cam_touch = touch && !native;
  g_tm.joy_x = rt_config_value("controls.touch_joy_x", 170);
  g_tm.joy_y = rt_config_value("controls.touch_joy_y", 540);
  g_tm.joy_r = rt_config_value("controls.touch_joy_r", 90);
  g_tm.cam_speed = rt_config_value("controls.touch_cam_speed", 16);
  g_tm.cam_r = rt_config_value("controls.touch_cam_r", 120);
  g_tm.binds = g_binds;
  g_tm.nbinds = n;
  tm_pad_config(&g_tm);
  debugPrintf("[input] touchEvent=%p vtouch=%d keys=%d native=%d binds=%d joy=(%d,%d,r%d)\n",
              (void *)g_nat.touchEvent, touch, keys, g_tdkr_native_pad, n, g_tm.joy_x, g_tm.joy_y,
              g_tm.joy_r);
}

static void touches(void) {
  HidTouchScreenState st;
  memset(&st, 0, sizeof st);
  if (hidGetTouchScreenStates(&st, 1) < 1)
    return;
  int w = 1280, h = 720;
  dcr_window_size(&w, &h);
  u32 cur[16];
  int ncur = 0;
  for (s32 i = 0; i < st.count && i < 16; i++) {
    const HidTouchState *t = &st.touches[i];
    int x = (int)((u64)t->x * (u64)w / 1280), y = (int)((u64)t->y * (u64)h / 720);
    tm_move(t->finger_id, x, y);
    if (ncur < 16)
      cur[ncur++] = t->finger_id;
  }
  for (int k = 0; k < g_nseen; k++) {
    int gone = 1;
    for (int i = 0; i < ncur; i++)
      if (cur[i] == g_seen[k])
        gone = 0;
    if (gone)
      tm_up(g_seen[k]);
  }
  memcpy(g_seen, cur, sizeof(u32) * (size_t)ncur);
  g_nseen = ncur;
}

void tdkr_input_poll(void) {
  touches();
  padUpdate(&g_pad);
  float sticks[4] = {0, 0, 0, 0};
  u64 now = rt_pad_read(&g_pad, sticks);
  u64 down = now & ~g_prev, up = g_prev & ~now;
  g_prev = now;
  int w = 1280, h = 720;
  dcr_window_size(&w, &h);
  /* Hot toggle: Minus+R3 flips native announce (gameplay) vs raw keys
   * (shop/menus). The combo's own edges are swallowed first. */
  if (((down & HidNpadButton_StickR) && (now & HidNpadButton_Minus)) ||
      ((down & HidNpadButton_Minus) && (now & HidNpadButton_StickR))) {
    tdkr_gamepad_set_native(!g_tdkr_native_pad);
    build_binds();
    debugPrintf("[input] mode: %s\n", g_tdkr_native_pad ? "native (gameplay)" : "raw (menus)");
    down &= ~(u64)(HidNpadButton_StickR | HidNpadButton_Minus);
    up &= ~(u64)(HidNpadButton_StickR | HidNpadButton_Minus);
  }
  tm_pad_poll(now, down, up, sticks, w, h);
  if (g_tdkr_native_pad)
    tdkr_gamepad_stick(sticks[2], g_invert_y ? -sticks[3] : sticks[3]);
  /* Force it: the left stick also holds the native arrows (8-way), the same
   * channel the D-pad uses. Either the drag or the keys move Batman. */
  {
    float lx = sticks[0], ly = sticks[1];
    u64 want = 0;
    if (lx > 0.5f)
      want |= HidNpadButton_Right;
    else if (lx < -0.5f)
      want |= HidNpadButton_Left;
    if (ly > 0.5f)
      want |= HidNpadButton_Up;
    else if (ly < -0.5f)
      want |= HidNpadButton_Down;
    if (lx > -0.35f && lx < 0.35f)
      want &= ~(u64)(HidNpadButton_Left | HidNpadButton_Right);
    if (ly > -0.35f && ly < 0.35f)
      want &= ~(u64)(HidNpadButton_Up | HidNpadButton_Down);
    /* The physical D-pad already sends its own keys: don't double them. */
    want &= ~(now & (HidNpadButton_Up | HidNpadButton_Down | HidNpadButton_Left | HidNpadButton_Right));
    u64 got = want & ~g_skeys, lost = g_skeys & ~want;
    g_skeys = want;
    if (got & HidNpadButton_Up)
      tm_key(19, 1);
    if (got & HidNpadButton_Down)
      tm_key(20, 1);
    if (got & HidNpadButton_Left)
      tm_key(21, 1);
    if (got & HidNpadButton_Right)
      tm_key(22, 1);
    if (lost & HidNpadButton_Up)
      tm_key(19, 0);
    if (lost & HidNpadButton_Down)
      tm_key(20, 0);
    if (lost & HidNpadButton_Left)
      tm_key(21, 0);
    if (lost & HidNpadButton_Right)
      tm_key(22, 0);
  }
}

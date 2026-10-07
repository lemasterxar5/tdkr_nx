/* tdkr_input.c -- the Switch's inputs for the engine.
 *
 * The game is played on a touch screen; the Joy-Cons drive it through the
 * reusable touchmap module (touchmap.h): the left stick or D-pad drags the
 * virtual joystick (STICK_WALK_*), the right stick looks through the game's
 * own PowerA globals (STICK_ROTATE_*), and every button taps a right-side
 * control and/or sends an Android key. Positions are 1280x720 config.ini
 * values (see tdkr_config.c); the real touch screen shares the same finger
 * pool. The D-pad is arrows (the original's cruceta movement) in menus and
 * gameplay; the left stick holds those same arrows 8-way, plus the drag.
 *
 * Menu vs gameplay is automatic, as in the Dead Space port: the wrapper sees
 * which zone the engine opened (tdkr_obb.c: l_menu is the start menu, every
 * other l_* zone is Gotham), plus pause panels inside gameplay zones (Plus
 * opens one, B goes back, a deflected stick means gameplay again). Where no
 * full mapping runs there are no virtual fingers at all -- keys plus the
 * touch screen -- so sticks and D-pad can never land on menu buttons (the
 * tech shop). Driving is its own independent modal: holding ZL turns the left stick's X into the slider
 * drag (walk and ride never fight: while held the stick stops driving the
 * joystick). MIT.
 */
#include <math.h>
#include <string.h>
#include <switch.h>

#include "rt_cfg.h"
#include "rt_applet.h"
#include "rt_pad.h"
#include "rt_window.h"
#include "tdkr.h"
#include "touchmap.h"
#include "util.h"

static u64 g_prev;
static TmPad g_tm;
static TmBind g_binds[28];
static u32 g_seen[16];
static int g_nseen;
static int g_keylog;
static int g_invert_y;
static int g_drive_x, g_drive_y, g_drive_r, g_drive_on;
static int g_drive_toggle = 1, g_drive_latch;
static u64 g_skeys;
static int g_applied = 1; /* tables currently loaded: 1 keys-only, 0 full */
static int g_paused;      /* pause/menu panel open inside a gameplay zone */
#define VDRIVE_KEY 0x80000030u

static void build_binds(int nomove);

static void input_sampler(void *arg);

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
  /* NOTE: no pad state here; the sampler thread owns the only PadState, so
   * slow frames never lose edges (see input_sampler). */
  tm_init(tm_touch, tm_key);
  g_applied = tdkr_zone_is_menu();
  g_paused = 0;
  build_binds(g_applied);
  static Thread s_sampler;
  if (R_FAILED(threadCreate(&s_sampler, input_sampler, NULL, NULL, 0x4000, 0x30, -2)) ||
      R_FAILED(threadStart(&s_sampler)))
    debugPrintf("[input] no sampler thread: quick taps may be lost in slow frames\n");
}

static void build_binds(int nomove) {
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
  int sx = rt_config_value("controls.touch_btn_sneak_x", 1090);
  int sy = rt_config_value("controls.touch_btn_sneak_y", 350);
  int gx = rt_config_value("controls.touch_btn_grapnel_x", 1210);
  int gy = rt_config_value("controls.touch_btn_grapnel_y", 470);
  int qx = rt_config_value("controls.touch_btn_qte_x", 640);
  int qy = rt_config_value("controls.touch_btn_qte_y", 360);
  g_drive_x = rt_config_value("controls.touch_drive_x", 640);
  g_drive_y = rt_config_value("controls.touch_drive_y", 620);
  g_drive_r = rt_config_value("controls.touch_drive_r", 180);
  g_drive_toggle = rt_config_value("controls.touch_drive_toggle", 1);

  /* One table, same order in menu and gameplay (held buttons survive the
   * switch): movement and menu arrows as keys, gameplay taps only on
   * A/B/X/Y/L/R/ZR, the rest key-only. Button -> touch icon from the game's
   * own prompts (strings.gla: "press the X button to attack", "tap the
   * counterattack icon or press Triangle (= Y) to block", "tap the jump
   * icon" for A, "right trigger" for the grapnel gun): X attacks, Y
   * counters, R taps the grapnel, ZR alone presses the centred giant-circle
   * button (QTE spam, doors, takedowns: placed at runtime, harmless when
   * hidden). Native mode uses SHIELD codes
   * (A96 B97 X99 Y100 L102 R103 ZL104 ZR105 StickL106 StickR107 Plus108
   * Minus4 D-pad 19-22; A also 23 to confirm menus, B also 4 to go back in
   * menus). In the menu there are no taps at all: keys plus the touch
   * screen, so no finger can land on menu buttons. */
  int native = g_tdkr_native_pad;
  int n = 0;
  int tap = touch && !nomove; /* gameplay taps */
  g_binds[n++] = (TmBind){HidNpadButton_Up, -1, -1, keys ? 19 : -1};
  g_binds[n++] = (TmBind){HidNpadButton_Down, -1, -1, keys ? 20 : -1};
  g_binds[n++] = (TmBind){HidNpadButton_Left, -1, -1, keys ? 21 : -1};
  g_binds[n++] = (TmBind){HidNpadButton_Right, -1, -1, keys ? 22 : -1};
  g_binds[n++] = (TmBind){HidNpadButton_Plus, -1, -1, keys ? (native ? 108 : 82) : -1};
  g_binds[n++] = (TmBind){HidNpadButton_Minus, -1, -1, keys ? 4 : -1};
  g_binds[n++] = (TmBind){HidNpadButton_A, tap ? jx : -1, tap ? jy : -1, keys ? (native ? 96 : 23) : -1};
  g_binds[n++] = (TmBind){HidNpadButton_A, -1, -1, keys ? 23 : -1};
  g_binds[n++] = (TmBind){HidNpadButton_B, tap ? ax : -1, tap ? ay : -1, keys ? (native ? 97 : 4) : -1};
  g_binds[n++] = (TmBind){HidNpadButton_X, tap ? ax : -1, tap ? ay : -1, keys ? (native ? 99 : -1) : -1};
  g_binds[n++] = (TmBind){HidNpadButton_Y, tap ? cx : -1, tap ? cy : -1, keys ? (native ? 100 : -1) : -1};
  g_binds[n++] = (TmBind){HidNpadButton_L, tap ? sx : -1, tap ? sy : -1, keys ? (native ? 102 : -1) : -1};
  g_binds[n++] = (TmBind){HidNpadButton_R, tap ? gx : -1, tap ? gy : -1, keys ? (native ? 103 : -1) : -1};
  g_binds[n++] = (TmBind){HidNpadButton_ZL, -1, -1, keys ? (native ? 104 : -1) : -1};
  /* ZR is the crosshair's action button: whatever the centred giant circle
   * is (QTE spam, doors, takedowns), ZR presses it. Dedicated, so no other
   * button's tap can conflict with it. */
  g_binds[n++] = (TmBind){HidNpadButton_ZR, tap ? qx : -1, tap ? qy : -1, keys ? (native ? 105 : -1) : -1};
  g_binds[n++] = (TmBind){HidNpadButton_StickL, -1, -1, keys ? (native ? 106 : -1) : -1};
  g_binds[n++] = (TmBind){HidNpadButton_StickR, -1, -1, keys ? (native ? 107 : -1) : -1};
  /* Y's second finger: the use/interact icon (context talks, hacks and
   * pickups share the InteractButton slot with the counter prompt family). */
  g_binds[n++] = (TmBind){HidNpadButton_Y, tap ? ux : -1, tap ? uy : -1, -1};
  if (nomove)
    g_binds[n++] = (TmBind){HidNpadButton_B, -1, -1, keys ? 4 : -1}; /* menu back */
  g_tm.enable = touch || keys;
  /* No virtual fingers in the menu (keys + touch screen only). */
  g_tm.touch = touch && !nomove;
  g_tm.cam_touch = touch && !nomove && !native;
  g_tm.joy_x = rt_config_value("controls.touch_joy_x", 170);
  g_tm.joy_y = rt_config_value("controls.touch_joy_y", 540);
  g_tm.joy_r = rt_config_value("controls.touch_joy_r", 90);
  g_tm.cam_speed = rt_config_value("controls.touch_cam_speed", 16);
  g_tm.cam_r = rt_config_value("controls.touch_cam_r", 120);
  g_tm.binds = g_binds;
  g_tm.nbinds = n;
  tm_pad_config(&g_tm);
  debugPrintf("[input] touchEvent=%p vtouch=%d keys=%d native=%d nomove=%d binds=%d joy=(%d,%d,r%d)\n",
              (void *)g_nat.touchEvent, touch, keys, g_tdkr_native_pad, nomove, n, g_tm.joy_x,
              g_tm.joy_y, g_tm.joy_r);
}

void tdkr_input_refresh(void) { build_binds(g_applied); }

/* A 120 Hz sampler: quick taps between two slow frames would otherwise never
 * exist (edges computed from one poll to the next miss press+release). The
 * snapshots carry pad + touchscreen state; poll() replays them in order, so
 * every transition reaches the engine. No JNI here, ever. */
#define SNAP_RING 128
typedef struct {
  u64 buttons;
  float sticks[4];
  u8 nt;
  u32 tid[10];
  s32 tx[10], ty[10];
} Snap;
static Snap g_ring[SNAP_RING];
static volatile uint32_t g_head, g_tail;
static PadState g_thread_pad;

static void input_sampler(void *arg) {
  (void)arg;
  padInitializeDefault(&g_thread_pad);
  u64 quiet_since = armGetSystemTick();
  for (;;) {
    if (rt_focused()) {
      padUpdate(&g_thread_pad);
      float sticks[4] = {0, 0, 0, 0};
      u64 b = rt_pad_read(&g_thread_pad, sticks);
      HidTouchScreenState st;
      memset(&st, 0, sizeof st);
      hidGetTouchScreenStates(&st, 1);
      int active = b || st.count;
      for (int i = 0; !active && i < 4; i++)
        if (sticks[i] > 0.05f || sticks[i] < -0.05f)
          active = 1;
      u64 now = armGetSystemTick();
      /* Idle 5 s: sample at 20 Hz instead of 120 Hz (battery/heat). */
      if (active)
        quiet_since = now;
      else if (armTicksToNs(now - quiet_since) > 5000000000ull) {
        svcSleepThread(50000000ll);
        continue;
      }
      uint32_t h = __atomic_load_n(&g_head, __ATOMIC_RELAXED);
      uint32_t nn = (h + 1) & (SNAP_RING - 1);
      if (nn != __atomic_load_n(&g_tail, __ATOMIC_ACQUIRE)) {
        Snap *s = &g_ring[h];
        s->buttons = b;
        memcpy(s->sticks, sticks, sizeof sticks);
        s->nt = st.count > 10 ? 10 : (u8)st.count;
        for (int i = 0; i < s->nt; i++) {
          s->tid[i] = st.touches[i].finger_id;
          s->tx[i] = st.touches[i].x;
          s->ty[i] = st.touches[i].y;
        }
        __atomic_store_n(&g_head, nn, __ATOMIC_RELEASE);
      }
    }
    svcSleepThread(8000000ll);
  }
}

static void touches_from(const Snap *s, int w, int h) {
  u32 cur[16];
  int ncur = 0;
  for (int i = 0; i < s->nt && ncur < 16; i++) {
    int x = (int)((u64)(uint32_t)s->tx[i] * (u64)w / 1280);
    int y = (int)((u64)(uint32_t)s->ty[i] * (u64)h / 720);
    tm_move(s->tid[i], x, y);
    cur[ncur++] = s->tid[i];
    /* A fresh real finger in the pause corner (top-left 150x150, where the
     * pause button lives): the pause menu was opened by touch. Only while
     * the full mapping runs; the next snapshot switches tables. */
    if (!g_paused && !g_applied) {
      int was = 0;
      for (int k = 0; k < g_nseen; k++)
        if (g_seen[k] == s->tid[i])
          was = 1;
      if (!was && s->tx[i] < 150 && s->ty[i] < 150)
        g_paused = 1;
    }
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
  int w = 1280, h = 720;
  dcr_window_size(&w, &h);
  /* Replay snapshots in order (no edge lost); collapse runs of identical
   * ones (idle menus) into their last: same edges, one processing. */
  for (;;) {
    uint32_t t = __atomic_load_n(&g_tail, __ATOMIC_RELAXED);
    if (t == __atomic_load_n(&g_head, __ATOMIC_ACQUIRE))
      break;
    uint32_t last = t;
    for (;;) {
      uint32_t nx = (last + 1) & (SNAP_RING - 1);
      if (nx == __atomic_load_n(&g_head, __ATOMIC_ACQUIRE))
        break;
      const Snap *a = &g_ring[last], *b = &g_ring[nx];
      if (a->buttons != b->buttons || a->nt != b->nt ||
          memcmp(a->sticks, b->sticks, sizeof a->sticks) != 0)
        break;
      int same = 1;
      for (int i = 0; i < a->nt; i++)
        if (a->tid[i] != b->tid[i] || a->tx[i] != b->tx[i] || a->ty[i] != b->ty[i]) {
          same = 0;
          break;
        }
      if (!same)
        break;
      last = nx;
    }
    const Snap *s = &g_ring[last];
    u64 now = s->buttons;
    u64 down = now & ~g_prev, up = g_prev & ~now;
    g_prev = now;
    touches_from(s, w, h);
    /* Menu vs gameplay, as the wrapper sees it: the zone (l_menu is the
     * start menu) plus pause panels inside gameplay zones (Plus opens the
     * pause menu, Minus opens panels, B goes back, a deflected stick means
     * the player wants to move). On a change, lift everything the other
     * side held (Dead Space pattern), then switch tables. Held pad buttons
     * stay held (no new edges); the synthetic stick arrows are released
     * explicitly. A confirming a menu entry never switches: submenus stay
     * finger-free too. */
    {
      if (down & HidNpadButton_Plus)
        g_paused = 1;
      if (down & HidNpadButton_Minus)
        g_paused = 1;
      if (down & HidNpadButton_B)
        g_paused = 0;
      {
        float ex = s->sticks[0] < 0 ? -s->sticks[0] : s->sticks[0];
        float ey = s->sticks[1] < 0 ? -s->sticks[1] : s->sticks[1];
        if (ex > 0.5f || ey > 0.5f)
          g_paused = 0;
      }
      int nomove = tdkr_zone_is_menu() || g_paused;
      if (nomove != g_applied) {
        g_applied = nomove;
        tm_pad_reset();
        g_drive_latch = 0;
        if (g_drive_on) {
          g_drive_on = 0;
          tm_up(VDRIVE_KEY);
        }
        if (g_skeys) {
          if (g_skeys & HidNpadButton_Up)
            tm_key(19, 0);
          if (g_skeys & HidNpadButton_Down)
            tm_key(20, 0);
          if (g_skeys & HidNpadButton_Left)
            tm_key(21, 0);
          if (g_skeys & HidNpadButton_Right)
            tm_key(22, 0);
          g_skeys = 0;
        }
        build_binds(nomove);
        debugPrintf("[input] %s\n", nomove ? (g_paused ? "pause panel: keys + touch screen, no virtual fingers"
                                                       : "menu: keys + touch screen, no virtual fingers")
                                           : "gameplay: full mapping (stick, camera, taps, drive)");
      }
    }
    /* Driving (slider bar) is a latch: tapping ZL enters/leaves drive mode,
     * no holding (or hold it the whole time with touch_drive_toggle=false).
     * In drive mode the left stick's X steers the slider with a gentle expo
     * curve instead of the joystick, so the two fingers never fight; the
     * slider finger stays put (no engage/release flicker). ZL keeps its own
     * key: the modal and the button are independent. */
    if (g_drive_toggle && (down & HidNpadButton_ZL)) {
      g_drive_latch = !g_drive_latch;
      debugPrintf("[input] drive mode %s\n", g_drive_latch ? "on (ZL to leave)" : "off");
      if (!g_drive_latch && g_drive_on) {
        g_drive_on = 0;
        tm_up(VDRIVE_KEY);
      }
    }
    int drive = (g_drive_toggle ? g_drive_latch : (now & HidNpadButton_ZL) != 0) && g_tm.touch;
    float dsticks[4] = {drive ? 0.0f : s->sticks[0], drive ? 0.0f : s->sticks[1], s->sticks[2],
                        s->sticks[3]};
    tm_pad_poll(now, down, up, dsticks, w, h);
    if (g_tdkr_native_pad)
      tdkr_gamepad_stick(s->sticks[2], g_invert_y ? -s->sticks[3] : s->sticks[3]);
    {
      /* Expo steering: dead centre, fine near the middle, full throw at the
       * ends (the slider bar is wide: linearity weaves). */
      float lx = s->sticks[0];
      float ax = lx < 0 ? -lx : lx;
      float steer = 0.0f;
      if (ax > 0.08f) {
        float m = (ax - 0.08f) / 0.92f;
        steer = (lx < 0 ? -1.0f : 1.0f) * (m * m * 0.65f + m * 0.35f);
      }
      if ((!drive || !g_tm.touch) && g_drive_on) {
        g_drive_on = 0;
        tm_up(VDRIVE_KEY);
      }
      if (!g_drive_on && drive && g_tm.touch) {
        g_drive_on = 1;
        tm_down(VDRIVE_KEY, g_drive_x * w / 1280, g_drive_y * h / 720);
      }
      if (g_drive_on) {
        int r = g_drive_r < 20 ? 20 : g_drive_r;
        tm_move(VDRIVE_KEY, g_drive_x * w / 1280 + (int)(steer * r * w / 1280), g_drive_y * h / 720);
      }
    }
    /* The stick also holds the native arrows (8-way), like the D-pad: the
     * proven movement channel. The drag stays too. */
    {
      float lx = s->sticks[0], ly = s->sticks[1];
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
    __atomic_store_n(&g_tail, (last + 1) & (SNAP_RING - 1), __ATOMIC_RELEASE);
  }
}

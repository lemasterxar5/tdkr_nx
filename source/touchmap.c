/* touchmap.c -- the shared finger pool and the pad driver. MIT. */
#include <math.h>
#include <string.h>
#include <switch.h>

#include "touchmap.h"
#include "util.h"

#define MAX_SLOTS 24

static tm_touch_fn g_touch;
static tm_key_fn g_key;
static int g_log_max = 200, g_logged;

static struct {
  int used;
  uint32_t key;
  int x, y;
} g_slot[MAX_SLOTS];

static TmPad g_pad_cfg;
static int g_joy_on, g_cam_on, g_cam_x, g_cam_y;

void tm_init(tm_touch_fn touch, tm_key_fn key) {
  g_touch = touch;
  g_key = key;
}

int tm_log_cap(int max_lines) {
  int old = g_log_max;
  g_log_max = max_lines;
  return old;
}

static void emit(int action, int x, int y, int id) {
  if (g_logged < g_log_max) {
    g_logged++;
    debugPrintf("[input] touch a=%d x=%d y=%d id=%d\n", action, x, y, id);
  }
  if (g_touch)
    g_touch(action, x, y, id);
}

static int acquire(uint32_t key) {
  for (int k = 0; k < MAX_SLOTS; k++)
    if (g_slot[k].used && g_slot[k].key == key)
      return k;
  for (int k = 0; k < MAX_SLOTS; k++)
    if (!g_slot[k].used) {
      g_slot[k].used = 1;
      g_slot[k].key = key;
      g_slot[k].x = g_slot[k].y = -1;
      return k;
    }
  return -1;
}

static int find(uint32_t key) {
  for (int k = 0; k < MAX_SLOTS; k++)
    if (g_slot[k].used && g_slot[k].key == key)
      return k;
  return -1;
}

int tm_down(uint32_t key, int x, int y) {
  int s = acquire(key);
  if (s < 0)
    return -1;
  g_slot[s].x = x, g_slot[s].y = y;
  emit(0, x, y, s);
  return s;
}

int tm_move(uint32_t key, int x, int y) {
  int s = acquire(key);
  if (s < 0)
    return -1;
  if (g_slot[s].x < 0) {
    g_slot[s].x = x, g_slot[s].y = y;
    emit(0, x, y, s);
  } else if (x != g_slot[s].x || y != g_slot[s].y) {
    g_slot[s].x = x, g_slot[s].y = y;
    emit(2, x, y, s);
  }
  return s;
}

int tm_up(uint32_t key) {
  int s = find(key);
  if (s < 0)
    return -1;
  emit(1, g_slot[s].x, g_slot[s].y, s);
  g_slot[s].used = 0;
  return s;
}

#define VJOY_KEY 0x80000000u
#define VCAM_KEY 0x80000001u
#define VBIND_KEY(i) (0x80000010u | (uint32_t)(i))

void tm_pad_config(const TmPad *p) {
  if (p)
    g_pad_cfg = *p;
  else
    memset(&g_pad_cfg, 0, sizeof g_pad_cfg);
}

/* Zone/menu change: lift the held drag fingers so nothing stays pressed. */
void tm_pad_reset(void) {
  if (g_joy_on) {
    g_joy_on = 0;
    tm_up(VJOY_KEY);
  }
  if (g_cam_on) {
    g_cam_on = 0;
    tm_up(VCAM_KEY);
  }
}

/* 1280x720-space point to window pixels. */
static int scX(int x, int w) { return (int)((int64_t)x * w / 1280); }
static int scY(int y, int h) { return (int)((int64_t)y * h / 720); }

void tm_pad_poll(u64 now, u64 down, u64 up, const float *sticks, int win_w, int win_h) {
  if (!g_pad_cfg.enable)
    return;
  float lx = sticks[0], ly = sticks[1], rx = sticks[2], ry = sticks[3];
  /* The D-pad drives the joystick at full tilt when the stick rests. */
  if (lx * lx + ly * ly < 0.09f) {
    float dx = ((now & HidNpadButton_Right) ? 1.0f : 0.0f) - ((now & HidNpadButton_Left) ? 1.0f : 0.0f);
    float dy = ((now & HidNpadButton_Up) ? 1.0f : 0.0f) - ((now & HidNpadButton_Down) ? 1.0f : 0.0f);
    if (dx != 0.0f || dy != 0.0f) {
      float m = sqrtf(dx * dx + dy * dy);
      lx = dx / m;
      ly = dy / m;
    }
  }
  float lm = sqrtf(lx * lx + ly * ly), rm = sqrtf(rx * rx + ry * ry);

  /* Left input: hold-and-drag the widget (hysteresis 0.25/0.15). */
  if (!g_joy_on && lm > 0.25f && g_pad_cfg.touch) {
    g_joy_on = 1;
    tm_down(VJOY_KEY, scX(g_pad_cfg.joy_x, win_w), scY(g_pad_cfg.joy_y, win_h));
  }
  if (g_joy_on) {
    if (lm < 0.15f || !g_pad_cfg.touch) {
      g_joy_on = 0;
      tm_up(VJOY_KEY);
    } else {
      int r = g_pad_cfg.joy_r < 10 ? 10 : g_pad_cfg.joy_r;
      tm_move(VJOY_KEY, scX(g_pad_cfg.joy_x, win_w) + (int)(lx * r * win_w / 1280),
              scY(g_pad_cfg.joy_y, win_h) - (int)(ly * r * win_h / 720));
    }
  }

  /* Right stick: swipe to look, ratcheting back past cam_r (unless a
   * native stick owns it: cam_touch 0). */
  if (!g_cam_on && rm > 0.25f && g_pad_cfg.cam_touch && g_pad_cfg.touch) {
    g_cam_on = 1;
    g_cam_x = 640;
    g_cam_y = 360;
    tm_down(VCAM_KEY, scX(g_cam_x, win_w), scY(g_cam_y, win_h));
  }
  if (g_cam_on) {
    if (rm < 0.15f || !g_pad_cfg.touch || !g_pad_cfg.cam_touch) {
      g_cam_on = 0;
      tm_up(VCAM_KEY);
    } else {
      int sp = g_pad_cfg.cam_speed < 1 ? 1 : g_pad_cfg.cam_speed;
      int rr = g_pad_cfg.cam_r < 20 ? 20 : g_pad_cfg.cam_r;
      g_cam_x += (int)(rx * sp);
      g_cam_y -= (int)(ry * sp);
      int dx = g_cam_x - 640, dy = g_cam_y - 360;
      if (dx * dx + dy * dy > rr * rr) {
        tm_up(VCAM_KEY);
        g_cam_x = 640;
        g_cam_y = 360;
        tm_down(VCAM_KEY, scX(g_cam_x, win_w), scY(g_cam_y, win_h));
      } else {
        tm_move(VCAM_KEY, scX(g_cam_x, win_w), scY(g_cam_y, win_h));
      }
    }
  }

  /* Buttons: taps and/or keys (nothing to do with no edges). */
  if (down | up)
    for (int i = 0; i < g_pad_cfg.nbinds; i++) {
      const TmBind *b = &g_pad_cfg.binds[i];
      if (g_pad_cfg.touch && b->tap_x >= 0) {
        if (down & b->btn)
          tm_down(VBIND_KEY(i), scX(b->tap_x, win_w), scY(b->tap_y, win_h));
        if (up & b->btn)
          tm_up(VBIND_KEY(i));
      }
      if ((down & b->btn) && b->key >= 0 && g_key)
        g_key(b->key, 1);
      if ((up & b->btn) && b->key >= 0 && g_key)
        g_key(b->key, 0);
    }
}

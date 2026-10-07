/* touchmap.h -- gamepad -> touch/key mapper for android32 Switch ports.
 *
 * The pattern of the Vita mapping tables (a button list with what each one
 * does), rebuilt for touch games: physical buttons become taps (and/or
 * Android key events), the sticks become drags, all through one shared finger
 * pool so ids stay small (0-15) and never collide between the touch screen
 * and the pad. A port fills a TmPad (usually from config.ini) and calls
 * tm_pad_poll() every frame; its own touch screen uses tm_down/move/up.
 * The engine callbacks: touch(action 0 down, 1 up, 2 move, window pixels,
 * id) and key(android keycode, down 1/0). MIT.
 */
#ifndef TOUCHMAP_H
#define TOUCHMAP_H

#include <stdint.h>
#include <switch.h>

typedef void (*tm_touch_fn)(int action, int x, int y, int id);
typedef void (*tm_key_fn)(int keycode, int down);

void tm_init(tm_touch_fn touch, tm_key_fn key);
/* [input] log lines left (capped); call with 0 to mute. Returns the old cap. */
int tm_log_cap(int max_lines);

/* Shared finger pool. Real fingers key by anything below 0x80000000u. */
int tm_down(uint32_t key, int x, int y); /* new finger -> id, or -1 when full */
int tm_move(uint32_t key, int x, int y); /* down if fresh, else move -> id/-1 */
int tm_up(uint32_t key);                 /* up at its last spot -> id/-1 */

/* One physical button: a tap (tap_x < 0: none) and/or a key (key < 0: none). */
typedef struct {
  u64 btn;
  int tap_x, tap_y; /* 1280x720 space, scaled to the window */
  int key;          /* Android keycode */
} TmBind;

typedef struct {
  int enable;      /* the pad drives anything at all */
  int touch;       /* taps and stick drags (keys always follow their binds) */
  int joy_x, joy_y, joy_r; /* left stick / D-pad widget (1280x720 space) */
  int cam_speed, cam_r;    /* right stick look (touch swipe) */
  int cam_touch;   /* 1: swipe for the right stick; 0: a native stick owns it */
  const TmBind *binds;
  int nbinds;
} TmPad;

void tm_pad_config(const TmPad *p);
/* sticks: lx, ly, rx, ry, y up. D-pad in `now` drives the joystick too. */
void tm_pad_poll(u64 now, u64 down, u64 up, const float *sticks, int win_w, int win_h);
/* Lift the held stick-drag fingers (menu/gameplay change). */
void tm_pad_reset(void);

#endif /* TOUCHMAP_H */

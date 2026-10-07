/* tdkr_config.c -- config.ini's options, on the runtime's INI engine. MIT. */
#include <stddef.h>

#include "rt_cfg.h"

static const CfgOpt k_opts[] = {
    CFG_ROW_RESOLUTION("720", CFG_HELP_RESOLUTION),
    {"display", "frame_rate", "60",
     "Frames per second: 60, or 30 (the Switch's GPU and battery work half as hard,\n"
     "# which helps in handheld mode; the game's own timing does not change).",
     CFG_CHOICE, "60,30", NULL},
    {"display", "show_fps", "true", "Log the frame rate every 2 seconds ([fps] in debug.log).",
     CFG_BOOL, NULL, NULL},
    {"performance", "boost_cpu_when_loading", "false",
     "Stock clocks only: the CPU boost to 1785 MHz while loading is off, so the\n"
     "# Switch keeps its own frequencies (handheld CPU 1020 / GPU 307.2 / MEM\n"
     "# 1331.2 MHz, docked CPU 1020 / GPU 768 / MEM 1600 MHz).",
     CFG_BOOL, NULL, NULL},
    {"controls", "gamepad_keys", "true",
     "The buttons act as Android keys in the menus: D-pad = arrows, + = Menu,\n"
     "# - = Back (A/B send keys only while gamepad_touch is off).",
     CFG_BOOL, NULL, NULL},
    {"controls", "gamepad_touch", "true",
     "The Joy-Cons drive the touch screen: left stick = the virtual joystick,\n"
     "# right stick = look, A/B/X/Y/L/R/ZL/ZR = taps on the right-side buttons.\n"
     "# Positions below are in 1280x720 pixels; tune them if a tap misses.",
     CFG_BOOL, NULL, NULL},
    {"controls", "gamepad_native", "true",
     "Native gamepad (Xperia slide + PowerA announced; SHIELD key codes and\n"
     "# the right stick in the game's globals). Minus+R3 flips to raw keys\n"
     "# (shop/menus) and back at runtime.",
     CFG_BOOL, NULL, NULL},
    {"controls", "gamepad_invert_y", "true",
     "Flip the right stick's vertical axis (on: pushing up looks down).",
     CFG_BOOL, NULL, NULL},
    {"controls", "touch_joy_x", "170", "Virtual joystick centre (left stick down).",
     CFG_INT, NULL, NULL, 0, 0, 0, 0},
    {"controls", "touch_joy_y", "540", "Virtual joystick centre (left stick down).",
     CFG_INT, NULL, NULL, 0, 0, 0, 0},
    {"controls", "touch_joy_r", "90", "Virtual joystick travel (left stick at full tilt).",
     CFG_INT, NULL, NULL, 0, 0, 0, 0},
    {"controls", "touch_btn_attack_x", "1150", "Attack button tap (B, R).", CFG_INT, NULL, NULL, 0,
     0, 0, 0},
    {"controls", "touch_btn_attack_y", "550", "Attack button tap (B, R).", CFG_INT, NULL, NULL, 0,
     0, 0, 0},
    {"controls", "touch_btn_jump_x", "1030", "Jump button tap (A, L).", CFG_INT, NULL, NULL, 0, 0,
     0, 0},
    {"controls", "touch_btn_jump_y", "610", "Jump button tap (A, L).", CFG_INT, NULL, NULL, 0, 0,
     0, 0},
    {"controls", "touch_btn_counter_x", "1150", "Counter button tap (X, ZR).", CFG_INT, NULL,
     NULL, 0, 0, 0, 0},
    {"controls", "touch_btn_counter_y", "420", "Counter button tap (X, ZR).", CFG_INT, NULL,
     NULL, 0, 0, 0, 0},
    {"controls", "touch_btn_use_x", "1030", "Interact button tap (Y, ZL).", CFG_INT, NULL, NULL, 0,
     0, 0, 0},
    {"controls", "touch_btn_use_y", "480", "Interact button tap (Y, ZL).", CFG_INT, NULL, NULL, 0,
     0, 0, 0},
    {"controls", "touch_cam_speed", "16", "Look speed: pixels per frame at full right-stick tilt.",
     CFG_INT, NULL, NULL, 0, 0, 0, 0},
    {"controls", "touch_cam_r", "120", "Look radius before the swipe finger ratchets back.",
     CFG_INT, NULL, NULL, 0, 0, 0, 0},
    CFG_ROW_GL_SELFTEST(NULL),
    CFG_ROW_BOOT_LOG(CFG_HELP_BOOT_LOG, NULL),
    CFG_ROW_LOG_JNI(CFG_HELP_LOG_JNI, NULL),
};

static const CfgMigrate k_migrate[] = {
    /* Stock clocks from build 2 on: an existing config.ini holding true moves
     * back to false. */
    {"performance", "boost_cpu_when_loading", "true", "false", 2},
    /* Inverted camera Y from build 3 on (a8retry/TASM2 negate it too). */
    {"controls", "gamepad_invert_y", "false", "true", 3},
    /* Raw menu keys from build 4 on: the native transfer remaps the D-pad
     * and shop-like menus only answer the raw arrows. */
    {"controls", "gamepad_native", "true", "false", 4},
    /* Native back on from build 5: menus get Minus+R3 instead. */
    {"controls", "gamepad_native", "false", "true", 5},
};

static const CfgTable k_table = {
    .opts = k_opts,
    .nopts = CFG_COUNT(k_opts),
    .migrate = k_migrate,
    .nmigrate = CFG_COUNT(k_migrate),
    .version = 5,
};

void dcr_config_load(void) { rt_config_load(&k_table); }

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
    {"display", "crosshair", "true", "A small centring crosshair in the middle of the screen (gameplay).",
     CFG_BOOL, NULL, NULL},
    {"performance", "boost_cpu_when_loading", "true",
     "CPU at 1785 MHz inside loading frames (the launch, zone loads): a frame\n"
     "# past 50 ms boosts until it ends (Horizon FastLoad, as retail loadings\n"
     "# do). Gameplay frames never boost, so the Switch keeps stock clocks\n"
     "# while playing.",
     CFG_BOOL, NULL, NULL},
    {"controls", "gamepad_keys", "true",
     "The buttons act as Android keys in the menus: D-pad = arrows, + = Menu,\n"
     "# - = Back (A/B send keys only while gamepad_touch is off).",
     CFG_BOOL, NULL, NULL},
    {"controls", "gamepad_touch", "true",
     "The Joy-Cons drive the touch screen in gameplay: left stick = the virtual\n"
     "# joystick, A/B/X/Y/L/R = taps on the right-side buttons. In the start\n"
     "# menu there are no virtual fingers (keys + touch screen only), detected\n"
     "# automatically from the loaded zone. 1280x720 pixels; tune if a tap misses.",
     CFG_BOOL, NULL, NULL},
    {"controls", "gamepad_native", "true",
     "Native gamepad (Xperia slide + PowerA announced; SHIELD key codes and\n"
     "# the right stick in the game's globals). Off: raw keys (Menu/Back)\n"
     "# and swipe look instead.",
     CFG_BOOL, NULL, NULL},
    {"controls", "gamepad_invert_y", "true",
     "Flip the right stick's vertical axis (on: pushing up looks down).",
     CFG_BOOL, NULL, NULL},
    {"controls", "touch_joy_x", "158", "Virtual joystick centre (left stick down).",
     CFG_INT, NULL, NULL, 0, 0, 0, 0},
    {"controls", "touch_joy_y", "585", "Virtual joystick centre (left stick down).",
     CFG_INT, NULL, NULL, 0, 0, 0, 0},
    {"controls", "touch_joy_r", "95", "Virtual joystick travel (left stick at full tilt).",
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
    {"controls", "touch_btn_sneak_x", "1090", "Sneak button tap (L, estimates).", CFG_INT, NULL,
     NULL, 0, 0, 0, 0},
    {"controls", "touch_btn_sneak_y", "350", "Sneak button tap (L, estimates).", CFG_INT, NULL,
     NULL, 0, 0, 0, 0},
    {"controls", "touch_btn_grapnel_x", "1210", "Grapnel button tap (R, estimates).", CFG_INT, NULL,
     NULL, 0, 0, 0, 0},
    {"controls", "touch_btn_grapnel_y", "470", "Grapnel button tap (R, estimates).", CFG_INT, NULL,
     NULL, 0, 0, 0, 0},
    {"controls", "touch_btn_qte_x", "640", "Crosshair action tap (ZR: doors, windshield, QTE).",
     CFG_INT, NULL, NULL, 0, 0, 0, 0},
    {"controls", "touch_btn_qte_y", "360", "Crosshair action tap (ZR: doors, windshield, QTE).",
     CFG_INT, NULL, NULL, 0, 0, 0, 0},
    {"controls", "touch_drive_x", "640", "Driving slider centre (left stick X).", CFG_INT, NULL,
     NULL, 0, 0, 0, 0},
    {"controls", "touch_drive_y", "620", "Driving slider centre (left stick X).", CFG_INT, NULL,
     NULL, 0, 0, 0, 0},
    {"controls", "touch_drive_r", "180", "Driving slider travel at full tilt.", CFG_INT, NULL,
     NULL, 0, 0, 0, 0},
    {"controls", "touch_drive_toggle", "true",
     "Tap ZL to enter/leave drive mode (no holding). false: hold ZL to drive, as before.",
     CFG_BOOL, NULL, NULL},
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
    /* Real widget geometry from build 6 (gameswf_normalres.gla stream 1: the
     * joystick sits at (118,520) of 960x640 = (158,585) of 1280x720 with a
     * radius of 84 (~95); pushing forward used to leave it. The driving
     * slider (podSlider) is 280 wide: full tilt must reach its ends. */
    {"controls", "touch_joy_x", "170", "158", 6},
    {"controls", "touch_joy_y", "540", "585", 6},
    {"controls", "touch_joy_r", "90", "95", 6},
    {"controls", "touch_drive_r", "120", "180", 6},
    /* Load-only CPU boost from build 7 (Dead Space pattern). */
    {"performance", "boost_cpu_when_loading", "false", "true", 7},
};

static const CfgTable k_table = {
    .opts = k_opts,
    .nopts = CFG_COUNT(k_opts),
    .migrate = k_migrate,
    .nmigrate = CFG_COUNT(k_migrate),
    .version = 7,
};

void dcr_config_load(void) { rt_config_load(&k_table); }

/* tdkr_gpu.c -- the handheld GPU boost, as the Switch recomp ports do it.
 *
 * Handheld stock is GPU 307.2 MHz; 460.8 MHz is an official handheld profile
 * (UnleashedRecomp-NX, sa2-nx: GPU up, MEM staying at 1331.2). CPU and MEM
 * are untouched, and docked needs nothing (768 MHz already). The request
 * lives in a clkrst session held for the run; undocking/docking (or the
 * option) is followed by the frame loop's tick. [performance]
 * gpu_boost_handheld disables it. MIT.
 */
#include <switch.h>

#include "rt_cfg.h"
#include "tdkr.h"
#include "util.h"

#define GPU_HANDHELD_BOOST_HZ 460800000u

static ClkrstSession g_clk;
static int g_have, g_inited;
static u32 g_saved;
static int g_saved_ok;

static int want(void) { return rt_config_value("performance.gpu_boost_handheld", 1); }

void tdkr_gpu_boost_apply(void) {
  if (!want() || g_have)
    return;
  if (appletGetOperationMode() != AppletOperationMode_Handheld)
    return;
  if (!g_inited) {
    g_inited = 1;
    if (R_FAILED(clkrstInitialize())) {
      debugPrintf("[gpu] no clkrst: stock GPU clocks\n");
      return;
    }
  }
  if (R_FAILED(clkrstOpenSession(&g_clk, PcvModuleId_GPU, 3))) {
    debugPrintf("[gpu] GPU session failed: stock GPU clocks\n");
    return;
  }
  u32 cur = 0;
  if (R_SUCCEEDED(clkrstGetClockRate(&g_clk, &cur)) && cur >= GPU_HANDHELD_BOOST_HZ) {
    debugPrintf("[gpu] handheld GPU already %u MHz: keeping it\n", (unsigned)(cur / 1000000));
    clkrstCloseSession(&g_clk);
    return;
  }
  g_saved = cur;
  g_saved_ok = cur != 0;
  if (R_FAILED(clkrstSetClockRate(&g_clk, GPU_HANDHELD_BOOST_HZ))) {
    debugPrintf("[gpu] GPU 460.8 MHz refused: stock GPU clocks\n");
    clkrstCloseSession(&g_clk);
    return;
  }
  g_have = 1;
  debugPrintf("[gpu] handheld GPU %u -> 460.8 MHz (CPU/MEM stock)\n", (unsigned)(cur / 1000000));
}

void tdkr_gpu_boost_remove(void) {
  if (!g_have)
    return;
  g_have = 0;
  if (g_saved_ok)
    clkrstSetClockRate(&g_clk, g_saved);
  clkrstCloseSession(&g_clk);
  debugPrintf("[gpu] GPU boost off\n");
}

void tdkr_gpu_boost_tick(void) {
  if (!want()) {
    tdkr_gpu_boost_remove();
    return;
  }
  if (appletGetOperationMode() == AppletOperationMode_Handheld)
    tdkr_gpu_boost_apply();
  else
    tdkr_gpu_boost_remove();
}

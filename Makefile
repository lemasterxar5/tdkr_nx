#---------------------------------------------------------------------------------
# The Dark Knight Rises -- Nintendo Switch wrapper (32-bit / AArch32), on the
# android32 runtime (runtime/). Ships no game code and no game data: the
# player's own APK and OBB are read at run time. Build with ./build.sh.
#
# Output: tdkr_nx.nsp (the 32-bit program: ExeFS main + main.npdm) and
# tdkr_nx.build; ./launcher/build.sh then makes tdkr_nx.nro around them.
#---------------------------------------------------------------------------------
TARGET               := tdkr_nx
PORT_NPDM_PROGRAM_ID := 0x0100000000001030

include runtime/runtime.mk

# Optimisation, as the Switch recomp ports ship it (UnleashedRecomp-NX,
# sa2-nx final builds): -O3 over the runtime's -O2 for this CPU-bound
# wrapper, and --gc-sections so -ffunction-sections/-fdata-sections drop
# dead code (dcr32.ld KEEPs crt0/init/fini: safe). LTO (-flto=balanced
# -fipa-pta, their final builds too) stays one uncomment away: it needs a
# clean Docker rebuild to verify.
CFLAGS  += -O3
LDFLAGS += -Wl,--gc-sections
#CFLAGS  += -flto -fno-fat-lto-objects -flto-partition=balanced -fipa-pta
#LDFLAGS += -flto -fno-fat-lto-objects -flto-partition=balanced -fipa-pta

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

#!/bin/sh
# Builds tdkr_nx.nsp in the AArch32 Switch toolchain container (Docker needed).
#   ./build.sh            the wrapper (then ./launcher/build.sh for the NRO)
#   ./build.sh clean
#   ./build.sh rt-files   what is built from where
# The patched libnx32 is the one in ./libnx32 (set DCR_LIBNX32 to use another);
# Mesa (mesa32) and ffmpeg32 are in ./portlibs32.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
export DCR_LIBNX32="${DCR_LIBNX32:-$HERE/libnx32}"
exec "$HERE/runtime/tools/docker_build.sh" "$@"

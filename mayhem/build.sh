#!/usr/bin/env bash
#
# mayhem/build.sh — builds Chocolate Doom (upstream CMake) twice, then links small harnesses and the
# known-answer probe against the resulting object files (z_native.c instead of the zone allocator).
#   mayhem/build/fuzz   sanitized (+SanCov, fuzzer-no-link): fuzz harnesses and -standalone reproducers
#   mayhem/build/tests  normal flags: the KAT probe used by mayhem/test.sh
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
B="$SRC/mayhem/build"
mkdir -p "$B"

# Upstream shifts a promoted byte into the sign bit (`data[0] << 24`, net_packet.c/i_swap.h) and casts
# callbacks between signatures; these benign patterns would trip on nearly every input, so only
# shift-base and function are relaxed. All of ASan and every other UBSan check stays halting.
RELAX="-fno-sanitize=shift-base,function"
SDL_CFLAGS="$(pkg-config --cflags sdl2 SDL2_mixer SDL2_net)"

configure_build() { # <dir> <cflags> <ldflags> [compiler]
  cmake -S "$SRC" -B "$1" -G "Unix Makefiles" \
    -DCMAKE_C_COMPILER="${4:-$CC}" \
    -DCMAKE_C_FLAGS="$2" \
    -DCMAKE_EXE_LINKER_FLAGS="$3" \
    -DCMAKE_BUILD_TYPE= >/dev/null
  cmake --build "$1" -j"$MAYHEM_JOBS" --target chocolate-doom
}

# link_with <builddir> <output> <compile/link flags...> -- <extra sources/objects...>
# Re-links chocolate-doom's own link line without main() (i_main.c) and the zone allocator.
link_with() {
  local dir="$1" out="$2" cc="${LINK_CC:-$CC}"; shift 2
  local flags=() extra=()
  while [ "$1" != "--" ]; do flags+=("$1"); shift; done; shift
  extra=("$@")
  local sdir="$dir/src" objs=() libs=() tok
  # link.txt: "<cc> <flags> -o chocolate-doom <objs> <libs/-l...>"
  read -r -a toks <<<"$(cat "$sdir/CMakeFiles/chocolate-doom.dir/link.txt")"
  local i=0 skip=0
  for tok in "${toks[@]:1}"; do
    tok="${tok//\"/}"
    if [ "$skip" = 1 ]; then skip=0; continue; fi
    case "$tok" in
      -o|-Xlinker) skip=1 ;;
      CMakeFiles/chocolate-doom.dir/i_main.c.o|CMakeFiles/chocolate-doom.dir/z_zone.c.o) ;;
      CMakeFiles/chocolate-doom.dir/*.o) objs+=("$sdir/$tok") ;;
      -fsanitize=*|-g|-gdwarf-*|-fno-*) ;;
      -*) libs+=("$tok") ;;
      /*) libs+=("$tok") ;;
      *) libs+=("$sdir/$tok") ;;
    esac
  done
  ( cd "$sdir" && $cc "${flags[@]}" -I"$SRC/src" -I"$dir" -I"$SRC/textscreen" -I"$SRC/opl" -I"$SRC/pcsound" $SDL_CFLAGS \
      -c "$SRC/src/z_native.c" -o "$dir/z_native.o" )
  $cc "${flags[@]}" -I"$SRC/src" -I"$dir" -I"$SRC/textscreen" -I"$SRC/opl" -I"$SRC/pcsound" $SDL_CFLAGS \
      "${extra[@]}" "$dir/z_native.o" "${objs[@]}" -Wl,--start-group "${libs[@]}" -Wl,--end-group -lm -o "$out"
}

# ---- 1) sanitized tree + harnesses ------------------------------------------------------------
FUZZ="$B/fuzz"
FZ_FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS $RELAX -fsanitize=fuzzer-no-link"
configure_build "$FUZZ" "$FZ_FLAGS" "$SANITIZER_FLAGS"

$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$B/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/standalone_main.o"

H_FLAGS=($SANITIZER_FLAGS $DEBUG_FLAGS $RELAX -fsanitize=fuzzer-no-link)
for t in fuzz_mus2mid fuzz_midifile fuzz_net_packet; do
  link_with "$FUZZ" "$SRC/$t" "${H_FLAGS[@]}" $LIB_FUZZING_ENGINE -- "$SRC/mayhem/$t.c" "$B/lsan_off.o"
  link_with "$FUZZ" "$SRC/$t-standalone" "${H_FLAGS[@]}" -- "$SRC/mayhem/$t.c" "$B/standalone_main.o" "$B/lsan_off.o"
done

# ---- 1b) AFL tree + file-input harnesses -------------------------------------------------------
# Upstream's I_Error() exits the process on malformed input, so these two are file-input targets
# (`afl: true`): afl-clang-fast supplies the coverage Mayhem reads, and a normal exit() is not a crash.
# One build, no -standalone: the binary runs by itself on a file.
export AFL_QUIET=1
AFLD="$B/afl"
AFL_FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS $RELAX"
configure_build "$AFLD" "$AFL_FLAGS" "$SANITIZER_FLAGS" afl-clang-fast
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$B/lsan_off_afl.o"
for t in fuzz_deh fuzz_wad; do
  LINK_CC=afl-clang-fast link_with "$AFLD" "$SRC/$t" $AFL_FLAGS -- "$SRC/mayhem/$t.c" "$B/lsan_off_afl.o"
done

# ---- 2) normal-flags tree + known-answer probe -------------------------------------------------
TESTS="$B/tests"
configure_build "$TESTS" "-O1 $COVERAGE_FLAGS" "$COVERAGE_FLAGS"
link_with "$TESTS" "$B/kat" -O1 $COVERAGE_FLAGS -- "$SRC/mayhem/kat.c"

#!/usr/bin/env bash
#
# mayhem/test.sh — RUN the known-answer probe (mayhem/kat.c, built by mayhem/build.sh from upstream's own
# MUS/MIDI/WAD/DEH/net-packet code) over fixed inputs and compare its output to exact expected values.
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}


KAT="$SRC/mayhem/build/kat"
FX="$SRC/mayhem/kat"
[ -x "$KAT" ] || { echo "missing $KAT (mayhem/build.sh builds it)" >&2; emit_ctrf "chocolate-doom-kat" 0 1 0; exit 1; }

passed=0; failed=0
# check <name> <expected stdout> <args...>
check() {
  local name="$1" want="$2"; shift 2
  local got
  got="$("$KAT" "$@" 2>/dev/null | grep -v -e '^zone memory' -e '^ loading')"
  if [ "$got" == "$want" ]; then passed=$((passed+1)); echo "PASS $name"
  else failed=$((failed+1)); echo "FAIL $name"; echo "  want: $want"; echo "  got:  $got"; fi
}

check mus2mid \
  "mid 38: 4d 54 68 64 00 00 00 06 00 00 00 01 00 46 4d 54 72 6b 00 00 00 10 00 b0 7b 00 00 90 3c 64 0a 80 3c 00 0a ff 2f 00" \
  mus "$FX/kat.mus"
check midifile \
  "tracks=2 division=96
track 0: [dt=0 meta=51 len=3] [dt=0 meta=2f len=0]
track 1: [dt=0 ev=90 ch=0 p1=60 p2=100] [dt=96 ev=80 ch=0 p1=60 p2=0] [dt=0 meta=2f len=0]" \
  midi "$FX/kat.mid"
check wad \
  "numlumps=3
lump 0 FOO size=5
lump 1 BAR size=4
lump 2 MAP01 size=0
FOO=hello BAR_index=1 NOPE_index=-1" \
  wad "$FX/kat.wad"
check dehacked \
  "player_health=250 frame3_tics=9 maxammo0=321" \
  deh "$FX/kat.deh"
check net-packet \
  "int16=4660 str=doom sint8=-1 int8=254 more=0" \
  net
check midifile-rejects-garbage \
  "midi: load failed" \
  midi "$FX/kat.deh"

emit_ctrf "chocolate-doom-kat" "$passed" "$failed" 0

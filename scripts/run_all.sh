#!/usr/bin/env bash
# scripts/run_all.sh
#
# Builds the Nova compiler, runs all four test suites, then runs every
# program in programs/.  Prints a PASS/FAIL line for each item and a
# final summary.
#
# Usage:
#   bash scripts/run_all.sh          # from repo root
#   bash scripts/run_all.sh --no-build   # skip the cmake build step

set -uo pipefail

# ── Paths ──────────────────────────────────────────────────────────────────────

REPO="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$REPO/build"

# On Windows/MSYS2 executables have .exe and need the MinGW runtime in PATH.
# On Linux/macOS they don't.
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        EXT=".exe"
        export PATH="/c/msys64/ucrt64/bin:$PATH"
        ;;
    *)
        EXT=""
        ;;
esac

NOVA="$BUILD/nova${EXT}"

SKIP_BUILD=0
for arg in "$@"; do
    [[ "$arg" == "--no-build" ]] && SKIP_BUILD=1
done

# ── Colours (disabled when stdout is not a terminal) ──────────────────────────

if [ -t 1 ]; then
    GRN=$'\033[0;32m'
    RED=$'\033[0;31m'
    BLD=$'\033[1m'
    DIM=$'\033[2m'
    RST=$'\033[0m'
else
    GRN='' RED='' BLD='' DIM='' RST=''
fi

# ── Bookkeeping ────────────────────────────────────────────────────────────────

pass_n=0
fail_n=0
declare -a report=()   # one entry per item; printed in Summary

mark_pass() {
    pass_n=$((pass_n + 1))
    report+=("  ${GRN}PASS${RST}  $1")
}

mark_fail() {
    fail_n=$((fail_n + 1))
    report+=("  ${RED}FAIL${RST}  $1")
}

# ── Helpers ────────────────────────────────────────────────────────────────────

section() {
    printf "\n${BLD}── %s${RST}\n" "$1"
}

# indent TEXT PREFIX — print each line of TEXT with PREFIX prepended.
# Pass "-" as TEXT to read from stdin (usable as a pipe filter).
indent() {
    local prefix="${2:-    }"
    if [ "$1" = "-" ]; then
        sed "s/^/$prefix/"
    else
        printf '%s\n' "$1" | sed "s/^/$prefix/"
    fi
}

# ── 1. Build ───────────────────────────────────────────────────────────────────

section "Build"

if [ "$SKIP_BUILD" -eq 1 ]; then
    printf "  ${DIM}(skipped via --no-build)${RST}\n"
else
    build_out=$(cmake --build "$BUILD" 2>&1)
    build_rc=$?
    if [ $build_rc -eq 0 ]; then
        printf "  ${GRN}✓${RST}  cmake --build succeeded\n"
    else
        printf "  ${RED}✗${RST}  cmake --build FAILED — cannot continue\n"
        indent "$build_out" "      "
        exit 1
    fi
fi

# ── 2. Test suites ─────────────────────────────────────────────────────────────

section "Test Suites"

SUITES=(test_lexer test_parser test_analyzer test_codegen)

for exe in "${SUITES[@]}"; do
    suite_out=$("$BUILD/$exe${EXT}" 2>&1)
    suite_rc=$?

    # Confirm "ALL TESTS PASSED" in output (defence against silent failure)
    if [ $suite_rc -eq 0 ] && printf '%s' "$suite_out" | grep -q "ALL TESTS PASSED"; then
        n=$(printf '%s' "$suite_out" | awk '/^Passed:/{print $2}')
        printf "  ${GRN}✓${RST}  %-22s  %s assertions\n" "$exe" "$n"
        mark_pass "$exe  ($n assertions)"
    else
        printf "  ${RED}✗${RST}  $exe\n"
        # Show individual failing checks
        printf '%s\n' "$suite_out" | grep '\[FAIL\]' | head -15 | indent - "        "
        mark_fail "$exe"
    fi
done

# ── 3. Programs ────────────────────────────────────────────────────────────────

section "Programs"

for prog in "$REPO/programs"/*.nova; do
    name=$(basename "$prog")
    prog_out=$("$NOVA" "$prog" 2>&1)
    prog_rc=$?

    if [ $prog_rc -eq 0 ]; then
        printf "  ${GRN}✓${RST}  $name\n"
        if [ -n "$prog_out" ]; then
            # Show up to 6 lines of output; summarise the rest
            total_lines=$(printf '%s\n' "$prog_out" | wc -l)
            printf '%s\n' "$prog_out" | head -6 | indent - "        "
            if [ "$total_lines" -gt 6 ]; then
                printf "        ${DIM}… (%d more lines)${RST}\n" $((total_lines - 6))
            fi
        fi
        mark_pass "$name"
    else
        printf "  ${RED}✗${RST}  $name\n"
        indent "$prog_out" "        "
        mark_fail "$name"
    fi
done

# ── Summary ────────────────────────────────────────────────────────────────────

section "Summary"
printf '\n'
for entry in "${report[@]}"; do
    printf '%b\n' "$entry"
done

total=$((pass_n + fail_n))
printf '\n'
printf "  Total: %d   ${GRN}Passed: %d${RST}   ${RED}Failed: %d${RST}\n" \
       "$total" "$pass_n" "$fail_n"
printf '\n'

if [ "$fail_n" -eq 0 ]; then
    printf "  ${GRN}${BLD}ALL PASSED${RST}\n"
    exit 0
else
    printf "  ${RED}${BLD}%d FAILED${RST}\n" "$fail_n"
    exit 1
fi

#!/usr/bin/env sh
set -u

CPPCHECK="${CPPCHECK:-cppcheck}"
INFER="${INFER:-infer}"
CMAKE="${CMAKE:-cmake}"

if ! command -v "$CPPCHECK" >/dev/null 2>&1; then
    echo "cppcheck is required for static analysis." >&2
    exit 127
fi

if ! command -v "$INFER" >/dev/null 2>&1; then
    echo "Infer is required for static analysis." >&2
    exit 127
fi

if ! command -v "$CMAKE" >/dev/null 2>&1; then
    echo "CMake is required to generate the Infer compilation database." >&2
    exit 127
fi

analysis_dir="$(mktemp -d "${TMPDIR:-/tmp}/isotpc-static-analysis.XXXXXX")"
trap 'rm -rf "$analysis_dir"' EXIT HUP INT TERM

if ! "$CMAKE" \
    -S . \
    -B "$analysis_dir/build" \
    -DCMAKE_BUILD_TYPE=Debug \
    -Disotpc_STATIC_LIBRARY=ON \
    -Disotpc_MAX_CAN_FRAME_SIZE=64 \
    -Disotpc_DEFAULT_TX_DL=32 \
    -Disotpc_ENABLE_CAN_SEND_FLAGS=ON \
    -Disotpc_ENABLE_CAN_FD_BRS=ON \
    -Disotpc_ENABLE_CAN_SEND_ARG=ON \
    -Disotpc_ENABLE_TRANSCEIVE_EVENTS=ON \
    -Disotpc_ENABLE_STREAMING=ON; then
    echo "Unable to generate the Infer compilation database." >&2
    exit 1
fi

analysis_status=0

"$CPPCHECK" --version
# Cppcheck intentionally does not need system headers to analyze this translation unit.
# unusedFunction and staticFunction refer to public entry points used by library consumers.
# Forced macro exploration can also produce unnamed callback declarations when callbacks are disabled.
# Some of these false positives are version-specific; do not fail when an older cppcheck does not emit them.
if ! "$CPPCHECK" \
    --enable=all \
    --inconclusive \
    --check-level=exhaustive \
    --force \
    --error-exitcode=1 \
    --inline-suppr \
    --std=c99 \
    --suppress=missingIncludeSystem \
    --suppress=unmatchedSuppression \
    --suppress=unusedFunction \
    --suppress=staticFunction \
    --suppress=funcArgNamesDifferentUnnamed \
    isotp.c; then
    analysis_status=1
fi

"$INFER" --version
if ! "$INFER" \
    --results-dir "$analysis_dir/infer-out" \
    --fail-on-issue \
    --compilation-database "$analysis_dir/build/compile_commands.json"; then
    analysis_status=1
fi

exit "$analysis_status"

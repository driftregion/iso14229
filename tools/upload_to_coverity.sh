#!/bin/bash
# Builds iso14229 under the Coverity Build Tool and packages cov-int for
# upload to scan.coverity.com. Requires cov-build on PATH.
#
# Usage: tools/coverity_scan.sh [bazel target]
#
# To also upload, set COVERITY_TOKEN, COVERITY_EMAIL and COVERITY_PROJECT,
# e.g.:
#   COVERITY_TOKEN=... COVERITY_EMAIL=... \
#   COVERITY_PROJECT=driftregion%2Fiso14229 tools/coverity_scan.sh

set -e

TARGET="${1:-//:iso14229}"
OUT_DIR="cov-int"
TARBALL="iso14229-cov-int.tgz"

command -v cov-build >/dev/null 2>&1 || {
    echo "cov-build not found on PATH; install the Coverity Build Tool" >&2
    exit 1
}

rm -rf "$OUT_DIR" "$TARBALL"
bazel clean --expunge

cov-build --dir "$OUT_DIR" bazel build --spawn_strategy=standalone "$TARGET"

if grep -q "No files were emitted" "$OUT_DIR/build-log.txt"; then
    echo "cov-build captured no compiler invocations; see $OUT_DIR/build-log.txt" >&2
    exit 1
fi

tar czf "$TARBALL" "$OUT_DIR"
echo "wrote $TARBALL"

if [ -n "$COVERITY_TOKEN" ] && [ -n "$COVERITY_EMAIL" ] && [ -n "$COVERITY_PROJECT" ]; then
    VERSION="$(git rev-parse --short HEAD)"
    curl --form token="$COVERITY_TOKEN" \
        --form email="$COVERITY_EMAIL" \
        --form file=@"$TARBALL" \
        --form version="$VERSION" \
        --form description="tools/coverity_scan.sh" \
        "https://scan.coverity.com/builds?project=$COVERITY_PROJECT"
else
    echo "set COVERITY_TOKEN, COVERITY_EMAIL and COVERITY_PROJECT to upload automatically"
fi

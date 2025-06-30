#!/bin/bash

commit_hash=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
echo "STABLE_SCM_REVISION ${commit_hash}"
echo "STABLE_SCM_DIRTY $(git diff --quiet || echo ".dirty")"
echo "STABLE_README_VERSION $(sed -n '/^## [0-9]\+\.[0-9]\+\.[0-9]\+/ { s/^## \([0-9]\+\.[0-9]\+\.[0-9]\+\).*/\1/; p; q }' README.md)"
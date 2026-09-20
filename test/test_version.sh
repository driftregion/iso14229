#!/bin/bash

# test that all version listings are consistent

VERSION=$(head -n1 VERSION)
ISO14229_H_VERSION=$(sed -n 's/^#define UDS_LIB_VERSION "\(.*\)"/\1/p' iso14229.h)
CHANGELOG_VERSION=$(sed -n '/^## [0-9]\+\.[0-9]\+\.[0-9]\+/ { s/^## \([0-9]\+\.[0-9]\+\.[0-9]\+\).*/\1/; p; q }' CHANGELOG)
DOXYGEN_VERSION=$(sed -n 's/^PROJECT_NUMBER\s*=\s*"\(.*\)"/\1/p' Doxyfile)

if [ "$VERSION" != "$ISO14229_H_VERSION" ]; then
    echo "Version mismatch:"
    echo "  VERSION: $VERSION"
    echo "  iso14229.h: $ISO14229_H_VERSION"
    echo "try:"
    echo "  make update_srcs"
    exit 1
fi

if [ "$VERSION" != "$CHANGELOG_VERSION" ]; then
    echo "Version mismatch:"
    echo "  VERSION: $VERSION"
    echo "  CHANGELOG: $CHANGELOG_VERSION"
    exit 1
fi

if [ "$VERSION" != "$DOXYGEN_VERSION" ]; then
    echo "Version mismatch:"
    echo "  VERSION: $VERSION"
    echo "  Doxyfile: $DOXYGEN_VERSION"
    exit 1
fi

echo "All version listings are consistent: $VERSION"
exit 0

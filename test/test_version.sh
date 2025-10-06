#!/bin/bash

# test that all version listings are consistent

VERSION_H_VERSION=$(sed -n 's/^#define UDS_LIB_VERSION "\(.*\)"/\1/p' $1)
README_VERSION=$(sed -n '/^## [0-9]\+\.[0-9]\+\.[0-9]\+/ { s/^## \([0-9]\+\.[0-9]\+\.[0-9]\+\).*/\1/; p; q }' README.md)
DOXYGEN_VERSION=$(sed -n 's/^PROJECT_NUMBER\s*=\s*"\(.*\)"/\1/p' Doxyfile)

if [ "$VERSION_H_VERSION" != "$README_VERSION" ]; then
    echo "Version mismatch:"
    echo "  $1: $VERSION_H_VERSION"
    echo "  README.md: $README_VERSION"
    exit 1
fi

if [ "$VERSION_H_VERSION" != "$DOXYGEN_VERSION" ]; then
    echo "Version mismatch:"
    echo "  $1: $VERSION_H_VERSION"
    echo "  Doxyfile: $DOXYGEN_VERSION"
    exit 1
fi

echo "All version listings are consistent: $VERSION_H_VERSION"
exit 0

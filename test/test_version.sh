#!/bin/bash

# test that the version in src/version.h matches that in README.md changelog

VERSION_H_VERSION=$(sed -n 's/^#define UDS_VERSION "\(.*\)"/\1/p' $1)
README_VERSION=$(sed -n '/^## [0-9]\+\.[0-9]\+\.[0-9]\+/ { s/^## \([0-9]\+\.[0-9]\+\.[0-9]\+\).*/\1/; p; q }' README.md)

if [ "$VERSION_H_VERSION" != "$README_VERSION" ]; then
    echo "Version mismatch:"
    echo "  $1: $VERSION_H_VERSION"
    echo "  README.md: $README_VERSION"
    exit 1
fi
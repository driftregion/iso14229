#!/bin/bash

VERSION=`cat bazel-bin/VERSION`

gh release create $VERSION \
"bazel-bin/iso14229.zip#iso14229.zip" \
"bazel-bin/iso14229.c#iso14229.c" \
"bazel-bin/iso14229.h#iso14229.h" \
"README.md#README.md" \
"AUTHORS.txt#AUTHORS.txt" \
"LICENSE#LICENSE" \
--generate-notes \
--title $VERSION 
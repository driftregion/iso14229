#!/bin/bash

VERSION=`cat bazel-bin/VERSION`

gh release create $VERSION \
"bazel-bin/iso14229.zip#iso14229.zip" \
"bazel-bin/src/iso14229.c#iso14229.c" \
"bazel-bin/src/iso14229.h#iso14229.h" \
"README.md#README.md" \
"CHANGELOG#CHANGELOG" \
"AUTHORS.txt#AUTHORS.txt" \
"LICENSE#LICENSE" \
--generate-notes \
--title $VERSION 
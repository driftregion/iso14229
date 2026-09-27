#!/bin/bash

set -euo pipefail

FULL_CORPUS_DIR=test/fuzz_outputs/corpus
NEW_CORPUS_DIR=test/fuzz_outputs/corpus_new

bazel build --config=fuzz //test:fuzz_server 

mkdir -p $NEW_CORPUS_DIR

# minimize the full corpus while still preserving the new 
bazel-bin/test/fuzz_server_bin \
-artifact_prefix=test/fuzz_outputs \
-merge=1  \
$NEW_CORPUS_DIR \
$FULL_CORPUS_DIR

BEFORE=$(du -sh $FULL_CORPUS_DIR | cut -f 1)
AFTER=$(du -sh $NEW_CORPUS_DIR | cut -f 1)

BEFORE_COUNT=$(find $FULL_CORPUS_DIR -type f | wc -l)
AFTER_COUNT=$(find $NEW_CORPUS_DIR -type f | wc -l)

echo Corpus changed from $BEFORE \($BEFORE_COUNT files\) to $AFTER \($AFTER_COUNT files\)

REPLY=n
if [ -t 0 ]; then
    read -r -p "Replace $FULL_CORPUS_DIR with $NEW_CORPUS_DIR? [y/N] " REPLY
fi

case "$REPLY" in
    [yY]|[yY][eE][sS])
        rm -rf $FULL_CORPUS_DIR
        mv $NEW_CORPUS_DIR $FULL_CORPUS_DIR
        echo Replaced $FULL_CORPUS_DIR
        ;;
    *)
        echo Kept original corpus. Reduced corpus left in $NEW_CORPUS_DIR
        ;;
esac
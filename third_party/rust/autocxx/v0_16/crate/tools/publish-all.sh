#!/bin/bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/.."

DIRS="$DIR/parser $DIR/engine $DIR/macro $DIR $DIR/gen/build $DIR/gen/cmd"

for CRATE in $DIRS; do
  pushd $CRATE
  echo "Publish: $CRATE"
  cargo publish
  popd
  sleep 30 # sometimes crates.io takes a moment, and our
           # crates are interdependent.
done

#!/bin/bash
#
# Copyright 2023 The Fuchsia Authors
#
# Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
# <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
# license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
# This file may not be copied, modified, or distributed except according to
# those terms.

# This script is a thin wrapper around Cargo that provides human-friendly
# toolchain names which are automatically translated to the toolchain versions
# we have pinned in CI.
#
#   cargo.sh --version <toolchain-name> # looks up the version for the named toolchain
#   cargo.sh +<toolchain-name> [...]    # runs cargo commands with the named toolchain
#   cargo.sh +all [...]                 # runs cargo commands with each toolchain
#
# The meta-toolchain "all" instructs this script to run the provided command
# once for each toolchain (msrv, stable, nightly).
#
# A common task that is especially annoying to perform by hand is to update
# trybuild's stderr files. Using this script:
#
#   TRYBUILD=overwrite ./cargo.sh +all test --workspace

set -eo pipefail

function print-usage-and-exit {
  echo "Usage:"                          >&2
  echo "  $0 --version <toolchain-name>" >&2
  echo "  $0 +<toolchain-name> [...]"    >&2
  echo "  $0 +all [...]"    >&2
  exit 1
}

[[ $# -gt 0 ]] || print-usage-and-exit

function pkg-meta {
  # NOTE(#547): We set `CARGO_TARGET_DIR` here because `cargo metadata`
  # sometimes causes the `cargo-metadata` crate to be rebuilt from source using
  # the default toolchain. This has the effect of clobbering any existing build
  # artifacts from whatever toolchain the user has specified (e.g., `+nightly`),
  # causing the subsequent `cargo` invocation to rebuild unnecessarily. By
  # specifying a separate build directory here, we ensure that this never
  # clobbers the build artifacts used by the later `cargo` invocation.
  CARGO_TARGET_DIR=target/cargo-sh cargo metadata --format-version 1 | jq -r ".packages[] | select(.name == \"zerocopy\").$1"
}

function lookup-version {
  VERSION="$1"
  case "$VERSION" in
    msrv)
      pkg-meta rust_version
      ;;
    stable)
      pkg-meta 'metadata.ci."pinned-stable"'
      ;;
    nightly)
      pkg-meta 'metadata.ci."pinned-nightly"'
      ;;
    *)
      echo "Unrecognized toolchain name: '$VERSION' (options are 'msrv', 'stable', 'nightly')" >&2
      return 1
      ;;
  esac
}

function get-rustflags {
  [ "$1" == nightly ] && echo "--cfg __INTERNAL_USE_ONLY_NIGHLTY_FEATURES_IN_TESTS"
}

function prompt {
  PROMPT="$1"
  YES="$2"
  while true; do
    read -p "$PROMPT " yn
    case "$yn" in
      [Yy]) $YES; return $?; ;;
      [Nn])       return 1;  ;;
      *)          break;     ;;
    esac
  done
}

case "$1" in
  # cargo.sh --version <toolchain-name>
  --version)
    [[ $# -eq 2 ]] || print-usage-and-exit
    lookup-version "$2"
    ;;
  # cargo.sh +all [...]
  +all)
    echo "[cargo.sh] warning: running the same command for each toolchain (msrv, stable, nightly)" >&2
    for toolchain in msrv stable nightly; do
      echo "[cargo.sh] running with toolchain: $toolchain" >&2
      $0 "+$toolchain" ${@:2}
    done
    exit 0
    ;;
  # cargo.sh +<toolchain-name> [...]
  +*)
    TOOLCHAIN="$(lookup-version ${1:1})"

    cargo "+$TOOLCHAIN" version &>/dev/null && \
    rustup "+$TOOLCHAIN" component list | grep '^rust-src (installed)$' >/dev/null || {
      echo "[cargo.sh] missing either toolchain '$TOOLCHAIN' or component 'rust-src'" >&2
      # If we're running in a GitHub action, then it's better to bail than to
      # hang waiting for input we're never going to get.
      [ -z ${GITHUB_RUN_ID+x} ] || exit 1
      prompt "[cargo.sh] would you like to install toolchain '$TOOLCHAIN' and component 'rust-src' via 'rustup'?" \
        "rustup toolchain install $TOOLCHAIN -c rust-src"
    } || exit 1

    RUSTFLAGS="$(get-rustflags ${1:1}) $RUSTFLAGS" cargo "+$TOOLCHAIN" ${@:2}
    ;;
  *)
    print-usage-and-exit
    ;;
esac

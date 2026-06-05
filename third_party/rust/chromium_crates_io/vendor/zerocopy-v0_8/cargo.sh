#!/usr/bin/env bash
#
# Copyright 2024 The Fuchsia Authors
#
# Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
# <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
# license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
# This file may not be copied, modified, or distributed except according to
# those terms.

set -eo pipefail

# When running inside the Docker container in CI, we don't copy `tools/.cargo`.
CONFIG_ARGS=()
if [[ -f "tools/.cargo/config.toml" ]]; then
  CONFIG_ARGS=("--config" "tools/.cargo/config.toml")
fi

# Build `cargo-zerocopy` without any RUSTFLAGS or CARGO_TARGET_DIR set in the
# environment
env -u RUSTFLAGS -u CARGO_TARGET_DIR cargo +stable build "${CONFIG_ARGS[@]}" --manifest-path tools/cargo-zerocopy/Cargo.toml -p cargo-zerocopy -q
./tools/target/debug/cargo-zerocopy "$@"

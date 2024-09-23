#!/bin/bash

set -ex

cmd="${1:-test}"

# Run with each feature
# * --each-feature includes both default/no-default features
# * --optional-deps is needed for serde feature
cargo hack "${cmd}" --each-feature --optional-deps
# Run with all features
cargo "${cmd}" --all-features

if [[ "${RUST_VERSION}" == "nightly"* ]]; then
    # Check benchmarks
    cargo check --benches

    # Check minimal versions
    cargo clean
    cargo update -Zminimal-versions
    cargo check --all-features
fi

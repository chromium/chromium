#!/usr/bin/env bash
#
# Copyright 2026 The Fuchsia Authors
#
# Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
# <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
# license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
# This file may not be copied, modified, or distributed except according to
# those terms.

set -e

if [ $# -ne 1 ]; then
    echo "Usage: $0 <version>" >&2
    exit 1
fi

VERSION="$1"

# Update the package version in the Anneal crate's manifest. This is the
# authoritative version for the crate.
sed -i -e "s/^version = \"[0-9a-zA-Z\.-]*\"/version = \"$VERSION\"/" anneal/Cargo.toml

# Update the installation instructions in the README to reflect the new version.
# This ensures that users copying instructions get the latest version.
sed -i -e "s/cargo install cargo-anneal@[0-9a-zA-Z\.-]*/cargo install cargo-anneal@$VERSION/" anneal/README.md

# Update Cargo.lock to reflect the version change in Cargo.toml. We must run
# this in the anneal subdirectory because it is a separate workspace with its
# own lockfile.
cd anneal
cargo generate-lockfile

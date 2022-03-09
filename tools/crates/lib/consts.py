# python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re

CRATES_IO_VIEW = "https://crates.io/crates/{crate}"
CRATES_IO_DOWNLOAD = "https://static.crates.io/crates/{crate}/{crate}-{version}.crate"

# Allowed licenses, in the format they are specified in Cargo.toml files from
# crates.io, and the format to write to README.chromium.
ALLOWED_LICENSES = [
    # ("Cargo.toml string", "License for README.chromium")
    ("Apache-2.0", "Apache 2.0"),
    ("MIT OR Apache-2.0", "Apache 2.0"),
    ("MIT/Apache-2.0", "Apache 2.0"),
    ("Apache-2.0 / MIT", "Apache 2.0"),
    ("Apache-2.0 OR MIT", "Apache 2.0"),
    ("Apache-2.0/MIT", "Apache 2.0"),
    ("MIT", "MIT"),
    ("Unlicense OR MIT", "MIT"),
    ("Unlicense/MIT", "MIT"),
    ("Apache-2.0 OR BSL-1.0", "Apache 2.0"),
    ("BSD-3-Clause", "BSD 3-Clause"),
    ("ISC", "ISC"),
]

# The subdirectory where crates are found, relative to the current working
# directory where the tool is run (i.e. `os.getcwd()`).
THIRD_PARTY = ["third_party", "rust"]

# Where to place the extracted crate inside the version epoch directory. If
# empty, it will be extracted directly to the epoch directory.
CRATE_INNER_DIR = ["crate"]

# Template for generating README.chromium files.
README_CHROMIUM = """Name: {crate_name}
URL: {url}
Description: {description}
Version: {version}
Security Critical: {security}
License: {license}
"""

# Crates that can not be depended one. Dependencies should be removed from
# Cargo.toml files. Each one comes with a reason.
BLOCKED_CRATES = {
    "cc":
    "C/C++ code should be build by a GN rule, not from Rust code directly. See "
    + os.path.join(*(THIRD_PARTY + ["cc", "README.md"])),
}

# A Regex for parsing the output of `cargo tree`. This matches the dependencies
# and reports their name, version, if they are a proc macro, and their enabled
# features.
_CARGO_DEPS = \
    r"(?:├──|└──) (?P<name>.*?) v(?P<version>[0-9]+.[0-9]+.[0-9]+)" \
    r"(?P<isprocmacro> \(proc-macro\))?" \
    r"(?: \((?P<path>[\/].*?)\))?" \
    r"(?: (?P<features>[^( ][^ ]*))?(?: \(\*\))?"
CARGO_DEPS_REGEX = re.compile(_CARGO_DEPS)

FAKE_EMPTY_CARGO_TOML = """[package]
name = "fake"
version = "0.0.0"
"""

# Header at the top of BUILD.gn files. The {year} is substituted with the
# appropriate year.
GN_HEADER = \
"""# Copyright {year} The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import("//build/rust/cargo_crate.gni")

"""
_GN_HEADER_YEAR = r"^# Copyright( \(c\))? (?P<year>[0-9]+) " \
    r"The Chromium Authors\. All rights reserved\."
GN_HEADER_YEAR_REGEX = re.compile(_GN_HEADER_YEAR)

# Comment on the build_native_rust_unit_tests field in BUILD.gn file output.
GN_TESTS_COMMENT = \
"""# Unit tests skipped. Generate with --with-tests to include them."""

# Comment on the visibility field in BUILD.gn file output.
GN_VISIBILITY_COMMENT = \
"""# Only for usage from third-party crates. Add the crate to
# third_party.toml to use it from first-party code."""

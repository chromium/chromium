# Rust third-party code

This directory contains all third-party Rust code, and sometimes thin wrappers
around it for C++ intertop.

## Crates.io

Crates that come from [crates.io](https://crates.io) are found in
`//third_party/rust/chromium_crates_io`, and are all vendored into the
Chromium git repository. They are managed through Cargo rules and with
the `gnrt` tool. See [`//docs/rust.md`](../../docs/rust.md) for how to
bring in new third-party libraries or update them.

The GN rules and README.chromium files for these crates are written by
the `gnrt` tool and should not be edited by hand.

### Directory structure

We store GN rules for each third-party crate in a directory of the same name.
Under that directory a folder named based on the crate epoch version is
created. This limits first-party usage of a crate to only one version within
each epoch.
If the crate's version has a major version greater-than 0, then that is used
as its version folder, such as `v1`. Otherwise, the name includes all
leading zeros in the version, such as `v0_3`.

For example, GN rules for the `tutelage` crate at version **1.4.3** would be
stored at
```sh
//third_party/rust/tutelage/v1
```

Whereas GN rules for the verion **0.2.8** version would be stored at
```sh
//third_party/rust/tutelage/v0_2
```

## Other sources

Third-party Rust libraries that are not distributed through [crates.io](
https://crates.io) are uncommon. But they may live under
`//third_party/rust/crate_name` directly, as a git submodule,
with GN rules written for them by hand.

## OWNERS

We do not require OWNERS in each crate's directory at this time, but this
will be revisited in the future.

## Review process

Rust libraries must go through the [3rd-party review process](
../../docs/adding_to_third_party.md).
See the [review of the `toml` crate](
https://groups.google.com/u/1/a/chromium.org/g/security/c/K686pSg-gZc/m/Pn2QzqahAwAJ)
for an example of a Rust security review.

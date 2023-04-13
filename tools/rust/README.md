# //tools/rust

This directory contains scripts for building, packaging, and distributing the
Rust toolchain (the Rust compiler, and also C++/Rust FFI tools like
[Crubit](https://github.com/google/crubit)).

[TOC]


## Background

Like with Clang, Chromium uses bleeding edge Rust tooling. We track the upstream
projects' latest development as closely as possible. However, Chromium cannot
use official Rust builds for various reasons which require us to match the Rust
LLVM backend version with the Clang we use.

It would not be reasonable to build the tooling for every Chromium build, so we
build it centrally (with the scripts here) and distribute it for all to use
(also fetched with the scripts here).


## Rust build overview

Each Rust package is built from an Rust git, usually from HEAD directly, along
with the current Clang/LLVM revision in use in Chromium. Hence a new Rust
package must be built whenever either Rust or Clang is updated. When building
Rust we also build additional tools such as clippy and rustfmt, and interop
tools including bindgen and crubit.

The Rust build also includes building LLVM for rustc to use, and Clang for
bindgen and crubit to use.

The `*_upload_clang` and `*_upload_rust` trybots are used to build Clang and
Rust respectively from the revisions specified in the Chromium source tree.
These are uploaded to a storage bucket when the build succeeds. After being
copied from staging to production by a developer (see
[cs/copy_staging_to_prod_and_goma.sh](
http://cs/copy_staging_to_prod_and_goma.sh)), they can then be fetched by
`gclient sync`.

The `update_rust.py` script is used by `gclient sync` to fetch the Rust
toolchain for the revisions specified in the script.

## Rolling Rust

Follow the directions in [//docs/updating_clang.md](
../../docs/updating_clang.md) to roll Clang and Rust together. To just
roll Rust on its own, use the `--skip-clang` argument when running
`upload_revision.py`.

The upload_revision.py script will update the revision of Rust to be
built and used in `update_rust.py` and will start the trybots that
will build the Rust toolchain.

After the build has succeeded and the new toolchain has been copied to
production, the CQ will run trybots to verify that our code still builds
and tests pass with the new Rust toolchain.

### Possible failure: Missing dependencies

`build_rust.py` will vendor all dependencies before starting the build. To do
this it first initializes git submodules. Then it runs `cargo vendor`. However
some parts of the compiler build are excluded from the top level Cargo.toml
workspace. Thus it passes `--sync dir` for a number of subdirectories, based
on [dist.rs, the nightly tarball packager](
https://github.com/rust-lang/rust/blob/master/src/bootstrap/dist.rs#L986-L995).

If another Cargo.toml is required in the future, and not part of the workspace
it would produce missing dependencies, and the set of directories in
`build_rust.py` would need to be updated.

### Local development

To build the Rust toolchain locally, run `//tools/rust/build_rust.py`. It
has additional flags to skip steps if you're making local changes and want
to retry a build. The script will produce its outputs in
`//third_party/rust-toolchain/`, which is the same place that `gclient sync`
places them.

Building the `rust_build_tests` GN target is a good way to quickly verify the
toolchain is working.

## Rolling Crubit tools

Steps to roll the Crubit tools (e.g. `rs_bindings_from_cc` tool)
to a new version:

- Locally, update `CRUBIT_REVISION` in `update_rust.py`.
  (Update `CRUBIT_SUB_REVISION` when the build or packaging is changed, but
  the upstream Rust revision we build from is not changed.)

- Locally, update `crubit_revision` in `//DEPS`, so that it matches
  the revision from the previous bullet item.

- Run manual tests locally (see the "Building and testing the tools locally"
  section below).
  TODO(https://crbug.com/1329611): These manual steps should
  be made obsolete once Rust-specific tryjobs cover Crubit
  tests.

## Building and testing Crubit locally

### Prerequisites

#### Bazel

`build_crubit.py` depends on Bazel.

To get Bazel, ensure that you have `checkout_bazel` set in your `.gclient` file
and then rerun `gclient sync`:

```sh
$ cat ../.gclient
solutions = [
  {
    "name": "src",
    "url": "https://chromium.googlesource.com/chromium/src.git",
    ...
    "custom_vars": {
      "checkout_bazel": True,
      "checkout_crubit": True,
    },
  },
]
```

### Building

Just run `tools/rust/build_crubit.py`. So far `build_crubit.py` has only been
tested on Linux hosts.

### Deploying

`build_crubit.py` will copy files into the directory specified in the
(optional) `--install-to` cmdline parameter - for example:

```
$ tools/rust/build_crubit.py --install-to=third_party/rust-toolchain/bin/
```

### Testing

Crubit tests are under `//build/rust/tests/test_rs_bindings_from_cc`.  Until
Crubit is built on the bots, the tests are commented out in
`//build/rust/tests/BUILD.gn`, but they should still be built and run before
rolling Crubit.  TODO(https://crbug.com/1329611): Rephrase this paragraph
after Crubit is built and tested on the bots.

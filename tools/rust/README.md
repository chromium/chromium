# //tools/rust

This directory contains scripts for building, packaging, and distributing the
Rust toolchain (the Rust compiler, and also C++/Rust FFI tools like
[Crubit](https://github.com/google/crubit)).


## Rolling Rust compiler and other Rust tools

Steps to roll the Rust compiler (and other Rust tools like `rustfmt`) to
a new version:
- Locally, update `RUST_REVISION` in `update_rust.py`.
  (Update `RUST_SUB_REVISION` when the build or packaging is changed, but
  the upstream Rust revision we build from is not changed.)
- Follow the general roll process below


## Rolling Crubit tools

Steps to roll the Crubit tools (e.g. `rs_bindings_from_cc` tool)
to a new version:

- Locally, update `CRUBIT_REVISION` in `build_crubit.py`.
  (Update `CRUBIT_SUB_REVISION` when the build or packaging is changed, but
  the upstream Rust revision we build from is not changed.)
  TODO(https://crbug.com/1329611): Move `CRUBIT_REVISION` to
  `update_rust.py` (see WIP CL: https://crrev.com/c/3718281).

- Locally, update `crubit_revision` in `//DEPS`, so that it matches
  the revision from the previous bullet item.

- Run manual tests locally (see the "Building and testing the tools locally"
  section below).
  TODO(https://crbug.com/1329611): These manual steps should
  be made obsolete once Rust-specific tryjobs cover Crubit
  tests.

- Follow the general roll process below


## General roll process

- Author a CL that updates `CRUBIT_REVISION` and/or `RUST_REVISION`
  (see one of the sections above for details).

- Upload the CL to Gerrit.
  Ask [CQ](//docs/infra/cq.md) for Rust-specific tryjobs in the CL description:
  `Cq-Include-Tryjobs:
  luci.chromium.try:linux-rust-x64-rel,linux-rust-x64-dbg,android-rust-arm-rel,android-rust-arm-dbg`.

- Run `linux_upload_clang` tryjob.  This will run `package_rust.py` which will
  `build_rust.py` and then upload a new version of the Rust toolchain package to
  the staging bucket.
  TODO(https://crbug.com/1329611): In the future this will also run
  `build_crubit.py` - we should update the docs once that happens.

- Move the new toolchain package from staging to prod.
  (A small set of people have the permission for this.  There are
  some Google-internal docs with more details.)

- Run CQ (see also `Cq-Include-Tryjobs` suggested above).
  This step will test that `gclient sync` can fetch the new Rust toolchain
  package + that the new package works fine in Chromium build.

- Land the CL.  The new package won't be used outside of this CL
  until this step.


## Building and testing the tools locally

### Prerequisites

`build_crubit.py` depends on having a locally-built LLVM/Clang:
`tools/clang/scripts/build.py --bootstrap --without-android --without-fuchsia`.
Among other things this prerequisite is required to generate
`third_party/llvm-bootstrap-install` where `build_crubit.py` will look for
LLVM/Clang libs and headers).

`build_crubit.py` depends on Bazel.
TODO(https://crbug.com/1329611): Provide instructions for using `checkout_bazel`
in `.gclient` after https://crrev.com/c/3763581 lands.

So far `build_crubit.py` has only been tested on Linux hosts.

### Building

Just run `tools/rust/build_rust.py` or `tools/rust/build_crubit.py`.

### Deploying

`build_rust.py` by default copies the newly build executables/binaries into
`//third_party/rust-toolchain`.  Currently a manual step is needed to copy
Crubit binaries (note that the `_impl` suffix has been dropped from the binary
name during the copy - this is expected):

```sh
cp \
    third_party/crubit/bazel-bin/rs_bindings_from_cc/rs_bindings_from_cc_impl \
    third_party/rust-toolchain/rs_bindings_from_cc
```

### Testing

Ensure that `args.gn` contains `enable_rust = true` and then build
`//build/rust/tests` directory - e.g. `ninja -C out/rust build/rust/tests`.

Native Rust tests from `//build/rust` can be run via
`out/rust/bin/run_build_rust_tests`.

Crubit tests are under `//build/rust/tests/test_rs_bindings_from_cc`.  Until
Crubit is built on the bots, the tests are commented out in
`//build/rust/tests/BUILD.gn`, but they should still be built and run before
rolling Crubit.  TODO(https://crbug.com/1329611): Rephrase this paragraph
after Crubit is built and tested on the bots.


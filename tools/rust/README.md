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

Similar to the Clang package which exists as a tarball that is unpacked into
`third_party/llvm-build`, the Rust package exists as a tarball that is unpacked
into `third_party/rust-toolchain`.

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

### An overview of what is updated in a Rust roll

During Rust packaging, the upstream Rust sources are checked out into
`third_party/rust-src`.

During a Rust roll, a couple of things get updated. The most obvious one is
various toolchain binaries like `rustc` that live in
`third_party/rust-toolchain/bin`. These are the direct outputs of a Rust
toolchain build.

We also update the Rust standard library. We actually provide two copies of the
standard library in Chromium: a prebuilt version only for use in host tools
(e.g. build scripts, proc macros), and a version built from source as part of
the normal Chromium build for use in target artifacts. These are the same
version of the standard library that the Rust toolchain revision provides.

The reason we have a prebuilt version of the standard library for use in host
tools is that they are often loaded into `rustc` as a module, so to be safe we
use the same prebuilts that the toolchain linked against. These are copied from
the Rust toolchain build to
`third_party/rust-toolchain/lib/rustlib/$PLATFORM/lib/*.rlib`. We use these
when the gn arg `rust_prebuilt_stdlib` is true, which is manually set to true
for gn host toolchains.

The sources of the standard library we build from source for target artifacts
live in `third_party/rust-toolchain/lib/rustlib/src/rust`. These are copied
from `third_party/rust-src`. Since Chromium uses gn as its build system, we
need some way to translate build files from Rust's build system, cargo, to gn
rules. This is the responsibility of `gnrt`, which is a Chromium-specific tool
that lives in [`tools/crates/gnrt`](https://crsrc.org/c/tools/crates/gnrt/),
written in Rust. `gnrt gen` takes a cargo workspace, runs `cargo metadata` on
it to get information about sources and dependencies, and outputs gn rules
corresponding to the cargo build rules. Rust has a
[`sysroot`](https://github.com/rust-lang/rust/tree/master/library/sysroot)
crate roughly corresponding to a top level cargo workspace we want for the
standard library. However, we want a couple of customizations without having to
patch the Rust sources, so we have another crate
[`fake_root`](https://crsrc.org/c/build/rust/std/fake_root/) above that depends
on `sysroot`.
[`tools/rust/gnrt_stdlib.py`](https://crsrc.org/c/tools/rust/gnrt_stdlib.py)
fetches and invokes the pinned `cargo` (see [`rustc` bootstrapping
explanation](https://rustc-dev-guide.rust-lang.org/building/bootstrapping/what-bootstrapping-does.html),
"pinned" is the "stage0" toolchain) to build and run `gnrt` with `fake_root` as
the base workspace, generating an updated
[`build/rust/std/rules/BUILD.gn`](https://crsrc.org/c/build/rust/std/rules/BUILD.gn)
that has gn rules for the new standard library sources. For convenience when
rolling Rust, this is one big `BUILD.gn` file as opposed to multiple files per
crate. Note that because we do not ship cargo build files in
`third_party/rust-toolchain`, we must run `gnrt` against `third_party/rust-src`
instead of `third_party/rust-toolchain`. But end users do not have
`third_party/rust-src` checked out, so we must rewrite the
`third_party/rust-src` paths to the copies of the sources in
`third_party/rust-toolchain/lib/rustlib/src/rust`, which is checked out by end
users as part of the Rust toolchain.

As an aside, `gnrt` is also used to generate gn build rules for
non-standard-library Rust packages in `third_party/rust` used in Chromium's
build. It uses
[`third_party/rust/chromium_crates_io`](https://crsrc.org/c/third_party/rust/chromium_crates_io)
as the base workspace and vendors sources into
`third_party/rust/chromium_crates_io/vendor`. The `--for-std` argument to `gnrt
gen` does different things for creating gn rules for the standard library
versus for various non-standard-library packages, such as producing a single
BUILD.gn file.

### Possible failure: Missing sources or inputs

A build error when building the stdlib in Chromium may look like:
```
FAILED: local_rustc_sysroot/lib/rustlib/x86_64-unknown-linux-gnu/lib/libstd.rlib
...build command...
ERROR: file not in GN sources: ../../third_party/rust-toolchain/lib/rustlib/src/rust/library/std/src/../../portable-simd/crates/std_float/src/lib.rs
```

Or:
```
FAILED: local_rustc_sysroot/lib/rustlib/x86_64-unknown-linux-gnu/lib/libstd.rlib
...build command...
ERROR: file not in GN inputs: ../../third_party/rust-toolchain/lib/rustlib/src/rust/library/std/src/../../stdarch/crates/core_arch/src/core_arch_docs.md
```

When building the stdlib in Chromium, the GN rules must have every rust source
or other input file that makes up the crate listed in the `sources` and
`inputs` GN variables. gnrt will walk the directory tree from the root of the
crate and put every relevant file into the set. But sometimes a crate includes
modules from paths outside the crate root's directory tree, with a path
directive such as
```rs
#[path = "../../stuff.rs"]
mod stuff;
```
or will `include!()` a file from another path, which is common for `.md` files:
```rs
include!("../../other_place.md")
```

The first error is saying the source file `std_float/src/lib.rs` did not
appear in the `sources` variable. The `../../` part of the path shows that
this is outside the crate root's directory tree. The second error is saying
that `core_arch/src/core_arch_docs.md` did not appear in the `inputs` variable.

To fix the error:
* Determine the path that is missing, relative to the crate root. In the above
  example this is `../../portable-simd/crates/std_float/src`. We could also use
  `../../portable-simd` or anything in between, though that would add a lot
  more sources to the GN rules than is necessary in this case. It's best to
  point to the directory of the module root (where the `lib.rs` or `mod.rs`
  is located).
* Download the roll CL (on Gerrit, click on the 3 dots in the upper right
  corner and click on "Download patch").
* Find the failing build target crate's rules in
  `//build/rust/std/gnrt_config.toml`. The failing crate in the above example
  is `libstd.rlib`, so we want the `[crate.std]` section of the config file.
* Determine if the target being built is a library or a build script. Build
  script targets end with the suffix `_build_script`. For example:
  ```
  [13627/84339] RUST(BIN) clang_x64_for_rust_host_build_tools/compiler_builtins_compiler_builtins_vunknown_build_script
  python3 ../../build/rust/rustc_wrapper.py --rustc=../../third_party/rust-toolchain/bin/rustc --depfi...(too long)
  ERROR: file not in GN sources: ../../third_party/rust-toolchain/lib/rustlib/src/rust/library/vendor/compiler_builtins-0.1.123/configure.rs
  ```
* Determine if the missing file should go in `sources` or `inputs`.
  * For `sources`, add the path to a `extra_src_roots` list in the crate's
    rules. For the above example, we could add
    `extra_src_roots = ['../../portable-simd/crates/std_float/src']`.
    * Or if it was a build script target, then
      `extra_build_script_src_roots = ['../../portable-simd/crates/std_float/src']`.
  * For `inputs`, add the path to a `extra_input_roots` list in the crate's
    rules. For the above example, we could add
    `extra_input_roots = ['../../stdarch/crates/core_arch/src']`.
    * Or if it was a build script target, then
      `extra_build_script_input_roots = ['../../stdarch/crates/core_arch/src']`.
* With the roll CL checked out, run `gclient sync`.

*** note
NOTE: `gclient sync` will download the version of the rust toolchain from the
roll CL. In order for this to work, the upload_rust bots should've completed and
`copy_staging_to_prod_and_goma.sh should've been run.
***

* Run `tools/rust/gnrt_stdlib.py` to use gnrt to rebuild the stdlib GN rules
  using the updated config.

*** note
NOTE: All gnrt_config options are found in
[//tools/crates/gnrt/lib/config.rs](https://source.chromium.org/chromium/chromium/src/+/main:tools/crates/gnrt/lib/config.rs).
The `CrateConfig` type has the various per-crate config options.
***

### Generating `BUILD.gn` files for stdlib crates

If the build structure changes in any way during a roll, the GN files need
to be regenerated.

#### Simple way:

Run `tools/rust/gnrt_stdlib.py`.

#### Longer way

This requires Rust to be installed and available in your system, typically
through [https://rustup.rs](https://rustup.rs).

To generate `BUILD.gn` files for the crates with the `gnrt` tool:
1. Change directory to the root `src/` dir of Chromium.
1. Build `gnrt` to run on host machine: `cargo build --release --manifest-path
   tools/crates/gnrt/Cargo.toml --target-dir out/gnrt`.
1. Ensure you have a checkout of the Rust source tree in `third_party/rust-src`
   which can be done with `tools/rust/build_rust.py --sync-for-gnrt`.
1. Run `gnrt` with the `gen` action:
   `out/gnrt/release/gnrt gen --for-std third_party/rust-src`.

This will generate the `//build/rust/std/rules/BUILD.gn` file, with the changes
visible in `git status` and can be added with `git add`.

## Local development

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
  TODO(crbug.com/40226863): These manual steps should
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
rolling Crubit.  TODO(crbug.com/40226863): Rephrase this paragraph
after Crubit is built and tested on the bots.

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

Each Rust package is built from an official Rust nightly source release and a
corresponding LLVM revision. Hence a new Rust package must be built whenever
either Rust or Clang is updated.

Chromium's Clang build process leaves behind several artifacts needed for the
Rust build. Each Rust build begins after a Clang build and uses these artifacts,
which include `clang`, LLVM libraries, etc.

A special CI job is used to build Clang and Rust from the revisions specified in
the Chromium source tree. These are uploaded to a storage bucket. After being
manually blessed by a developer, they can then be fetched by Chromium checkouts.

Scripts are provided in tree to fetch the packages for the specified revisions.

Whenever a Chromium checkout is updated, `gclient` hooks will update the
toolchain packages to match the revisions listed in tree.


## Updating Rust

### Set the revision

First, pick a new instance of the [CIPD `rust_src`
package](https://chrome-infra-packages.appspot.com/p/chromium/third_party/rust_src/+/).
Click it and copy the version ID (e.g. `version:2@2022-01-01`) to the `//DEPS` entry for
`src/third_party/rust_src/src`. For example (though don't change the other parts
of the entry):

```
  'src/third_party/rust_src/src': {
    'packages': [
      {
        'package': 'chromium/third_party/rust_src',
        'version': 'version:2@2022-01-01',
      },
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_rust_toolchain_deps or use_rust',
  },
```

Similarly, update the `RUST_REVISION` named in `//tools/rust/update_rust.py`,
removing dashes from the date (e.g. `version:2@2022-01-01` becomes
`RUST_REVISION = '20220101'`). Reset `RUST_SUB_REVISION = 1`.

Run the following to check for changes to Rust's `src/stage0.json`, which
contains revisions of upstream binaries to be fetched and used in the Rust
build:

```
tools/rust/build_rust.py --verify-stage0-hash
```

If it exists without printing anything, the stage0 hash is up-to-date and
nothing needs to be done. Otherwise, it will print the actual hash like so:

```
...
Actual hash:   6b1c61d494ad447f41c8ae3b9b3239626eecac00e0f0b793b844e0761133dc37
...
```

...in which case you should check the `stage0.json` changes for trustworthiness
(criteria TBD) and then update `STAGE0_JSON_SHA256` in update_rust.py with the
new hash. Re-run the above and confirm it succeeds.


### Optional: build locally and run tests

This step is not strictly necessary since the CI tooling will catch any errors.
But the CI build process is slow and this can save some time.

To fetch the new Rust sources, and avoid errors during `gclient sync`:
1. Ensure your `.gclient` file has `checkout_rust_toolchain_deps` set to `True`,
but avoid setting `use_rust` to `True`. The latter will try to download the
compiled rustc but as you've just updated the version there is no compiled rustc
to download so it will fail.
1. Additionally, to aid in testing, turn off rust support in GN, with
`enable_rust = false`, since it requires the presence of a Rust toolchain, but
building the toolchain destroys your local toolchain until the build succeeds.

Running this will do a full build and provide a local toolchain that works for
the host machine, albeit not the same as the CI-provided one:

```
tools/rust/build_rust.py --fetch-llvm-libs --use-final-llvm-build-dir
```

To do a full build, first build Clang locally (TODO: provide instructions) then
simply run `tools/rust/build_rust.py` with no arguments.

However, for most cases simply doing

```
tools/rust/build_rust.py --fetch-llvm-libs --use-final-llvm-build-dir --run-xpy -- build --stage 1 library/std
```

will catch most errors and will be fast.

### Upload CL and build package

Upload a CL with the changes, which in the simplest case will only have two
changes: one line in `//DEPS`, one line in `//tools/rust/update_rust.py`. Add the
following line to the end of the CL description, which ensures the new toolchain
will be tested on appropriate Rust tryjobs:

```
Cq-Include-Trybots: luci.chromium.try:android-rust-arm-dbg,android-rust-arm-rel,linux-rust-x64-dbg,linux-rust-x64-rel
```

From Gerrit run the `linux_upload_clang` tryjob on the CL and wait for it to
finish. Check that it's successful; it is **not** sufficient to check the
result in Gerrit as a Rust failure will not surface here. Check the build page
(e.g.
https://ci.chromium.org/ui/p/chromium/builders/try/linux_upload_clang/2611/overview)
and confirm the "package rust" step succeeded. If it did not, further
investigation is needed.

After the package is built, a developer with permissions must bless the package
for use. As of writing this is anyone in [Clang
OWNERS](/tools/clang/scripts/OWNERS) or collinbaker@chromium.org.


### Submit CL

Once the package has been uploaded and blessed, it is ready to be fetched from
any Chromium checkout.

Submit the CL. CQ tryjobs will use the specified toolchain package
version. Any build failures will need to be investigated, as these indicate
breaking changes to Rust.


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


## Building and testing the tools locally

### Prerequisites

#### LLVM/Clang build

`build_crubit.py` depends on having a locally-built LLVM/Clang:
`tools/clang/scripts/build.py --bootstrap --without-android --without-fuchsia`.
Among other things this prerequisite is required to generate
`third_party/llvm-bootstrap-install` where `build_crubit.py` will look for
LLVM/Clang libs and headers).

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
      "use_rust": True,
    },
  },
]
```

#### Supported host platforms

So far `build_crubit.py` has only been tested on Linux hosts.

### Building

Just run `tools/rust/build_rust.py` or `tools/rust/build_crubit.py`.

### Deploying

`build_rust.py` by default copies the newly build executables/binaries into
`//third_party/rust-toolchain`.

`build_crubit.py` will copy files into the directory specified in the
(optional) `--install-to` cmdline parameter - for example:

```
$ tools/rust/build_crubit.py --install-to=third_party/rust-toolchain/bin/
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

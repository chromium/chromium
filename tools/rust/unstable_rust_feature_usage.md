# Unstable Rust usage in Chromium

## Policy

Unstable features are **unsupported** by default in Chromium. Any use of an
unstable language or library feature should be agreed upon by the Rust toolchain
team before enabling it.

Since Chromium imports the Rust toolchain at its HEAD and builds it in a
nightly-like configuration, it is technically possible to depend on unstable
features. However, unstable features often change in a backwards-incompatible
way without a warning. If such incompatible changes are introduced, importing a
new version of toolchain now requires the owner to fix forward, instead of being
an automated process. This makes toolchain upgrades prohibitively difficult.

When an exception is required, consider:

-   Whether the unstable feature brings significant value that is unattainable
    in stable alternatives
-   The risk of breaking changes to the feature
-   Ways to fallback in case a backward-incompatible toolchain change is
    introduced

## Exceptions

This section maintains a list of exceptions from the policy above:

- `#![feature(portable_simd)]` needed by the ETC1 encoder (see
  [the discussion in the doc here](https://docs.google.com/document/d/1lh9x43gtqXFh5bP1LeYevWj0EcIRgIu0XGahHY08aeY/edit?tab=t.0)
    - `//ui/android/texture_compressor`
    - `//third_party/rust/bytemuck`
- `#![feature(linkage)]` and `#![feature(rustc_attrs)]` needed to use
  PartitionAlloc from Rust (removal is tracked by https://crbug.com/410596442)
    - `//build/rust/allocator`
- `maybe_uninit_write_slice`, `assert_matches`, `maybe_uninit_slice`:
  Grandfathered-in exception in prototype code (i.e. not used and not shipping)
    - `mojo/public/rust/...`

### How to allow unstable features in BUILD.gn

Example `BUILD.gn` for first-party code:

    ```
    # BUILD.gn:
    rust_static_library("some_target_name") {
      configs -= [ "//build/config/compiler:disallow_unstable_features" ]
      rustflags = [ "-Zallow-features=portable_simd" ]
    }
    ```

Example `gnrt_config.toml` entry for a third-party crate
(run `tools/crates/run_gnrt.py gen` to regenerate the crate's `BUILD.gn` file):

    ```
    # gnrt_config.toml:
    [crate.bytemuck.extra_kv]
    allow_unstable_features = ["portable_simd"]
    ```

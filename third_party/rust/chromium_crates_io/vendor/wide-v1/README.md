[![License:Zlib](https://img.shields.io/badge/License-Zlib-brightgreen.svg)](https://opensource.org/licenses/Zlib)
![Minimum Rust Version](https://img.shields.io/badge/Min%20Rust-1.89-green.svg)
[![crates.io](https://img.shields.io/crates/v/wide.svg)](https://crates.io/crates/wide)
[![docs.rs](https://docs.rs/wide/badge.svg)](https://docs.rs/wide/)

# wide

A crate to help you go wide.

Specifically, this has portable "wide" data types that do their best to be SIMD when possible. It's a near drop-in replacement for [std::simd](https://doc.rust-lang.org/stable/std/simd/index.html).

On `x86`, `x86_64`, `wasm32` and `aarch64 neon` this is implemented with explicit
intrinsic usage (via [safe_arch](https://docs.rs/safe_arch)), and on other
architectures this is done by carefully writing functions so that LLVM hopefully
does the right thing. When Rust stabilizes more explicit intrinsics then they
can go into `safe_arch` and then they can get used here.

## Enabling SIMD instructions in your build

### Aarch64 (64-bit ARM) 

Aarch64 always enables the NEON extension, so `wide` can always take advantage of SIMD on this platform.

### WASM

SIMD is an optional extension for WASM, but it is [supported by all modern browsers](https://caniuse.com/wasm-simd).

To enable SIMD in your build you need to set `RUSTFLAGS="-C target-feature=+simd128"`, e.g.:
```
RUSTFLAGS="-C target-feature=+simd128" cargo build --target wasm32-wasip1
```

### x86

Rust i686 and x86_64 targets guarantee only the presence of basic operations on 128-bit vectors (SSE2). If you need anything else, you need to explicitly enable the relevant SIMD extensions at build time. For example, this will use all SIMD extensions available on your CPU:
```
RUSTFLAGS='-C target-cpu=native' cargo build --release
```
However, attempting to use an instruction that is not supported by the CPU will crash the program or lead to undefined behavior. Therefore distributing binaries built with SIMD extensions enabled is not recommended.

**Note:** `wide` only supports detecting the available SIMD extensions at build time. Runtime feature detection via [`is_x86_feature_detected!`](https://doc.rust-lang.org/stable/std/macro.is_x86_feature_detected.html) or crates like [multiversion](https://crates.io/crates/multiversion) do not work with `wide`.

## Rust Version Policy

* The `rust-version` entry of the crate's `Cargo.toml` will be kept accurate to
  the required Rust compiler version.
* A bump in `rust-version` will be released as a change in either the major or
  minor crate version number, but never as part of a patch release. This way,
  users stuck on an old toolchain can always get necessary patch updates for
  their major.minor version of the crate.
* If your build uses Resolver 3 (or later) this will be fine. If you're using a
  Resolver earlier than 3 then **you are responsible** for pinning a maximum
  crate version when you're using an old Rust version.

# RustFFT

[![CI](https://github.com/ejmahler/RustFFT/workflows/CI/badge.svg)](https://github.com/ejmahler/RustFFT/actions?query=workflow%3ACI)
[![](https://img.shields.io/crates/v/rustfft.svg)](https://crates.io/crates/rustfft)
[![](https://img.shields.io/crates/l/rustfft.svg)](https://crates.io/crates/rustfft)
[![](https://docs.rs/rustfft/badge.svg)](https://docs.rs/rustfft/)
![minimum rustc 1.61](https://img.shields.io/badge/rustc-1.61+-red.svg)

RustFFT is a high-performance, SIMD-accelerated FFT library written in pure Rust. It can compute FFTs of any size, including prime-number sizes, in O(nlogn) time.

## Usage

```rust
// Perform a forward FFT of size 1234
use rustfft::{FftPlanner, num_complex::Complex};

let mut planner = FftPlanner::<f32>::new();
let fft = planner.plan_fft_forward(1234);

let mut buffer = vec![Complex{ re: 0.0, im: 0.0 }; 1234];

fft.process(&mut buffer);
```

## SIMD acceleration

### x86_64 Targets

RustFFT supports the AVX instruction set for increased performance. No special code is needed to activate AVX: Simply plan a FFT using the `FftPlanner` on a machine that supports the `avx` and `fma` CPU features, and RustFFT will automatically switch to faster AVX-accelerated algorithms.

For machines that do not have AVX, RustFFT also supports the SSE4.1 instruction set. As for AVX, this is enabled automatically when using the FftPlanner. If both AVX and SSE4.1 support are enabled, the planner will automatically choose the fastest available instruction set.

### AArch64 Targets

RustFFT supports the NEON instruction set in 64-bit Arm, AArch64. As with AVX and SSE, no special code is needed to activate NEON-accelerated code paths: Simply plan a FFT using the `FftPlanner` on an AArch64 target, and RustFFT will automatically switch to faster NEON-accelerated algorithms.

### WebAssembly Targets

RustFFT supports [the fixed-width SIMD extension for WebAssembly](https://github.com/WebAssembly/spec/blob/main/proposals/simd/SIMD.md). Just like AVX, SSE, and NEON, no special code is needed to take advantage of this code path: All you need to do is plan a FFT using the `FftPlanner`.

**Note:** There is an important caveat when compiling WASM SIMD accelerated code: Unlike AVX, SSE, and NEON, WASM does not allow dynamic feature detection. Because of this limitation, RustFFT **cannot** detect CPU features and automatically switch to WASM SIMD accelerated algorithms. Instead, it unconditionally uses the SIMD code path if the `wasm_simd` crate feature is enabled. Read more about this limitation [in the official Rust docs](https://doc.rust-lang.org/1.75.0/core/arch/wasm32/index.html#simd).

## Feature Flags

### x86_64 Targets

The features `avx` and `sse` are enabled by default. On x86_64, these features enable compilation of AVX and SSE accelerated code.

Disabling them reduces compile time and binary size.

On other platforms than x86_64, these features do nothing and RustFFT will behave like they are not set.

### AArch64 Targets

The `neon` is enabled by default. On AArch64, this feature enables compilation of Neon-accelerated code.

Disabling it reduces compile time and binary size.

On other platforms than AArch64, this feature does nothing and RustFFT will behave like it is not set.

### WebAssembly Targets

The feature `wasm_simd` is disabled by default. On the WASM platform, this feature enables compilation of WASM SIMD accelerated code.

To execute binaries compiled with `wasm_simd`, you need a [target browser or runtime which supports `fixed-width SIMD`](https://webassembly.org/roadmap/).
If you run your SIMD accelerated code on an unsupported platform, WebAssembly will specify a [trap](https://webassembly.github.io/spec/core/intro/overview.html#trap) leading to immediate execution cancelation.

On other platforms than WASM, this feature does nothing and RustFFT will behave like it is not set.

## Stability/Future Breaking Changes

The latest version is 6.2 - Version 5.0 was released at the beginning of 2022 and contains several breaking API changes from previous versions. For users on very old version of RustFFT, check out the [Upgrade Guide](/UpgradeGuide4to5.md) for a walkthrough of the changes RustFFT 5.0 requires to upgrade. In the interest of stability, we're committing to making no more breaking changes for 3 years, aka until 2024.

This policy has one exception: We currently re-export pre-1.0 versions of the [num-complex](https://crates.io/crates/num-complex) and [num-traits](https://crates.io/crates/num-traits) crates. In the interest of avoiding ecosystem fragmentation, we will keep up with these crates even if it requires major version bumps. When those crates release new major versions, we will upgrade as soon as possible, which will require a major version change of our own. In these situations, the version increase of num-complex/num-traits will be the only breaking change in the release.

### Supported Rust Version

RustFFT requires rustc 1.61 or newer. Minor releases of RustFFT may upgrade the MSRV(minimum supported Rust version) to a newer version of rustc.
However, if we need to increase the MSRV, the new Rust version must have been released at least six months ago.

## License

Licensed under either of

- Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
- MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally
submitted for inclusion in the work by you, as defined in the Apache-2.0
license, shall be dual licensed as above, without any additional terms or
conditions.

Before submitting a PR, please make sure to run `cargo fmt`.

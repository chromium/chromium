# JPEG XL in Rust (`jxl-rs`)

[![CI](https://github.com/libjxl/jxl-rs/actions/workflows/post_merge.yml/badge.svg)](https://github.com/libjxl/jxl-rs/actions/workflows/post_merge.yml)
[![Crates.io](https://img.shields.io/crates/v/jxl.svg)](https://crates.io/crates/jxl)
[![Docs.rs](https://docs.rs/jxl/badge.svg)](https://docs.rs/jxl)
[![Discord](https://img.shields.io/discord/794206087879852103?label=Discord&logo=discord)](https://discord.gg/DqkQgDRTFu)

<img src="https://raw.githubusercontent.com/libjxl/libjxl/main/doc/jxl.svg" width="100" align="right" alt="JXL logo">

`jxl-rs` is a high-performance, conforming, and memory-safe JPEG XL decoder
written in Rust.

It is the JPEG XL decoder implementation used in **Google Chrome / Chromium**
and **Mozilla Firefox**.

JPEG XL was standardized in 2022 as [ISO/IEC 18181](https://jpeg.org/jpegxl/workplan.html).
While [`libjxl`](https://github.com/libjxl/libjxl) (C++) is the official ISO/IEC
18181-4 reference software, `jxl-rs` is a pure-Rust implementation
developed within the JPEG XL project. It is not (yet) an official reference
implementation, it aims for full specification conformance, memory safety, and
high decoding performance across diverse hardware architectures.

The performance of `jxl-rs` closely matches (and sometimes exceeds) the C++ reference
implementation, while having improved support for progressive rendering and lower
memory requirements.

## Safety

The vast majority of the code uses safe Rust. Where `unsafe` is needed for
performance benefits (including SIMD), we require unsafe code to have extensive
safety comments and to be reviewed by a non-author Unsafe Rust expert.

## Workspace Crates

- [`jxl`](jxl/): Core JPEG XL decoder library.
- [`jxl_cli`](jxl_cli/): CLI decoding and benchmarking tool.
- [`jxl_cms`](jxl_cms/), [`jxl_simd`](jxl_simd/), [`jxl_transforms`](jxl_transforms/), [`jxl_macros`](jxl_macros/): Internal crates for color management, SIMD acceleration, transforms, and macros.

## Usage

```bash
# Build
cargo build --release --bin jxl_cli

# Decode an image (PNG, PPM, PGM, APNG, EXR)
target/release/jxl_cli input.jxl output.png

# Print image metadata
target/release/jxl_cli input.jxl --info

# Benchmark decoding speed
target/release/jxl_cli input.jxl --speedtest --num-reps 10
```

The `jxl` crate is also available on [crates.io](https://crates.io/crates/jxl)
for use as a library.

## Testing

```bash
# Run workspace tests
cargo test --release --all

# Run Shuttle concurrency tests
cargo test --release --features shuttle shuttle
```

To run property-based tests with extended budget:
```bash
ARBTEST_BUDGET_MS=10000 cargo test --release --all
SHUTTLE_ITERATIONS=100 cargo nextest run --features shuttle -- shuttle
```

## Conformance & Bug Reports

We strive to decode all conformant JPEG XL bitstreams correctly. If
you encounter an image that decodes with the reference implementation
`djxl` (from [`libjxl`](https://github.com/libjxl/libjxl)) but
fails or renders incorrectly with `jxl-rs`, please [open an
issue](https://github.com/libjxl/jxl-rs/issues/new).

## Community & Contact

- **Main C++ Repository**: [libjxl/libjxl](https://github.com/libjxl/libjxl)
- **Discord**: Join the [JPEG XL Discord Server](https://discord.gg/DqkQgDRTFu)
- **JPEG XL Website**: [jpeg.org/jpegxl](https://jpeg.org/jpegxl) & [jpegxl.info](https://jpegxl.info)
- **Contributing**: Please see [CONTRIBUTING.md](CONTRIBUTING.md) for contribution guidelines and CLA requirements.
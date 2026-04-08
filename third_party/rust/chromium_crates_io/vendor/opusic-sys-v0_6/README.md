# opusic-sys

[![Rust](https://github.com/DoumanAsh/opusic-sys/actions/workflows/rust.yml/badge.svg)](https://github.com/DoumanAsh/opusic-sys/actions/workflows/rust.yml)
[![Crates.io](https://img.shields.io/crates/v/opusic-sys.svg)](https://crates.io/crates/opusic-sys)
[![Documentation](https://docs.rs/opusic-sys/badge.svg)](https://docs.rs/crate/opusic-sys/)

Bindings to [libopus](https://github.com/xiph/opus)

Target version [1.5.2](https://github.com/xiph/opus/releases/tag/v1.5.2)

This crate has the same license requirements as C source code.

All modifications to the source code are described in [opus.patch](https://github.com/DoumanAsh/opusic-sys/blob/master/opus.patch)

High level bindings: [opusic-c](https://github.com/DoumanAsh/opusic-c)

## Setup

By default, `libopus` is bundled, this can be deactivated using `default-features=false`.
To explicitly enable bundling, enable feature `bundled`.

If feature `bundled` is *not* enabled, then by default `$PATH` is searched for `libopus`.
Alternatively, environment variable `OPUS_LIB_DIR` can be set to link against a specific library. (ex. `/usr/lib`)

You can specify environment variable `OPUS_LIB_STATIC=true` to indicate preference for static linkage during dynamic lookup

## Android build

When building for android, library requires presence of env variable `ANDROID_NDK_HOME` in order to supply
cmake with toolchain file and correct target arch.

## Re-generate bindings

The feature `build-bindgen` is used to generate bindings.

To use it set env variable `LIBCLANG_PATH` to directory that contains clang binaries

## Requirements

- `cmake` - when building with `bundled` feature

### Optional

- `ninja` - When present, build script, if `bundled` feature enabled, defaults to use corresponding CMake's generator

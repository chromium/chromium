# symphonia-adapter-libopus

[![crates.io](https://img.shields.io/crates/v/symphonia-adapter-libopus?logo=rust)](https://crates.io/crates/symphonia-adapter-libopus)
[![docs.rs](https://img.shields.io/docsrs/symphonia-adapter-libopus?logo=rust)](https://docs.rs/symphonia-adapter-libopus)
![license](https://img.shields.io/badge/License-MIT%20or%20Apache%202-green.svg)
[![CI](https://github.com/aschey/symphonia-adapters/actions/workflows/ci.yml/badge.svg)](https://github.com/aschey/symphonia-adapters/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/aschey/symphonia-adapters/branch/main/graph/badge.svg?token=pF3FhV8OUt)](https://app.codecov.io/gh/aschey/symphonia-adapters)
![GitHub repo size](https://img.shields.io/github/repo-size/aschey/symphonia-adapters)
![Lines of Code](https://aschey.tech/tokei/github/aschey/symphonia-adapters)

Adapter for using [libopus](https://github.com/DoumanAsh/opusic-sys) with
Symphonia. Symphonia currently does not have native Opus support, so this crate
can provide it until a first-party solution is available.

See the [libopus binding documentation](https://crates.io/crates/opusic-sys) for
details on how to configure linking libopus.

## Usage

```rust
use symphonia_core::codecs::CodecRegistry;
use symphonia_adapter_libopus::OpusDecoder;

let mut codec_registry = CodecRegistry::new();
codec_registry.register_all::<OpusDecoder>();
// register other codecs

// use codec_registry created above instead of symphonia::default::get_codecs();
```

## Linking & Bundling

By default `libopus` will be compiled and bundled into the resulting binary.

To disable this, set `default-features = false`. Or to explicitly enable bundling add feature `bundled`.

## License

This crate is licensed under either the MIT and Apache 2.0 license, at your
choice.

libopus and opusic-sys are licensed under the
[opus license](https://opus-codec.org/license/).

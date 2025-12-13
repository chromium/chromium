zip
========

[![Build Status](https://github.com/zip-rs/zip2/actions/workflows/ci.yaml/badge.svg)](https://github.com/Pr0methean/zip/actions?query=branch%3Amaster+workflow%3ACI)
[![Crates.io version](https://img.shields.io/crates/v/zip.svg)](https://crates.io/crates/zip)

[Documentation](https://docs.rs/zip/latest/zip/)

Info
----


A zip library for rust which supports reading and writing of simple ZIP files. Formerly hosted at
https://github.com/zip-rs/zip2.

Supported compression formats:

* stored (i.e. none)
* deflate
* deflate64 (decompression only)
* bzip2
* zstd
* lzma (decompression only)
* xz
* ppmd

Currently unsupported zip extensions:

* Multi-disk

Features
--------

The features available are:

* `aes-crypto`: Enables decryption of files which were encrypted with AES. Supports AE-1 and AE-2 methods.
* `deflate`: Enables compressing and decompressing an unspecified implementation (that may change in future versions) of
  the deflate compression algorithm, which is the default for zip files. Supports compression quality 1..=264.
* `deflate-flate2`: Combine this with any `flate2` feature flag that enables a back-end, to support deflate compression
  at quality 1..=9.
* `deflate-zopfli`: Enables deflating files with the `zopfli` library (used when compression quality is 10..=264). This
  is the most effective `deflate` implementation available, but also among the slowest.
* `deflate64`: Enables the deflate64 compression algorithm. Only decompression is supported.
* `lzma`: Enables the LZMA compression algorithm. Only decompression is supported.
* `bzip2`: Enables the BZip2 compression algorithm.
* `ppmd`: Enables the PPMd compression algorithm.
* `time`: Enables features using the [time](https://github.com/rust-lang-deprecated/time) crate.
* `chrono`: Enables converting last-modified `zip::DateTime` to and from `chrono::NaiveDateTime`.
* `jiff-02`: Enables converting last-modified `zip::DateTime` to and from `jiff::civil::DateTime`.
* `nt-time`: Enables returning timestamps stored in the NTFS extra field as `nt_time::FileTime`.
* `xz`: Enables the XZ compression algorithm.
* `zstd`: Enables the Zstandard compression algorithm.

By default `aes-crypto`, `bzip2`, `deflate`, `deflate64`, `lzma`, `ppmd`, `time`, `xz` and `zstd` are enabled.

MSRV
----

Our current Minimum Supported Rust Version is **1.83**. When adding features,
we will follow these guidelines:

- We will always support a minor Rust version that has been stable for at least 6 months.
- Any change to the MSRV will be accompanied with a **minor** version bump.

Examples
--------

See the [examples directory](examples) for:

* How to write a file to a zip.
* How to write a directory of files to a zip (using [walkdir](https://github.com/BurntSushi/walkdir)).
* How to extract a zip file.
* How to extract a single file from a zip.
* How to read a zip from the standard input.
* How to append a directory to an existing archive

Fuzzing
-------

Fuzzing support is through [cargo afl](https://rust-fuzz.github.io/book/afl.html). To install cargo afl:

```bash
cargo install cargo-afl
```

To start fuzzing zip extraction:

```bash
cargo +nightly afl build --all-features --manifest-path fuzz_read/Cargo.toml
cargo +nightly afl run fuzz_read/target/debug/fuzz_read
```

To start fuzzing zip creation:

```bash
cargo +nightly afl build --all-features --manifest-path fuzz_write/Cargo.toml
cargo +nightly afl run fuzz_write/target/debug/fuzz_write
```

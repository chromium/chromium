# fdeflate

[![crates.io](https://img.shields.io/crates/v/fdeflate.svg)](https://crates.io/crates/fdeflate)
[![Documentation](https://docs.rs/fdeflate/badge.svg)](https://docs.rs/fdeflate)
[![Build Status](https://img.shields.io/github/actions/workflow/status/image-rs/fdeflate/rust.yml?label=Rust%20CI)](https://github.com/image-rs/fdeflate/actions)

A fast deflate implementation.

This crate contains an optimized implementation of the deflate algorithm tuned to compress PNG
images. It is compatible with standard zlib, but make a bunch of simplifying assumptions that
drastically improve encoding performance:

- Exactly one block per deflate stream.
- No distance codes except for run length encoding of zeros.
- A single fixed huffman tree trained on a large corpus of PNG images.
- All huffman codes are <= 12 bits.

It also contains a fast decompressor that supports arbitrary zlib streams but does especially
well on streams that meet the above assumptions.

### Inspiration

The algorithms in this crate take inspiration from multiple sources:
* [fpnge](https://github.com/veluca93/fpnge)
* [zune-inflate](https://github.com/etemesi254/zune-image/tree/main/zune-inflate)
* [RealTime Data Compression blog](https://fastcompression.blogspot.com/2015/10/huffman-revisited-part-4-multi-bytes.html)

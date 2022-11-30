# miniz_oxide

A pure rust replacement for the [miniz](https://github.com/richgel999/miniz) DEFLATE/zlib encoder/decoder.
The main intention of this crate is to be used as a back-end for the [flate2](https://github.com/alexcrichton/flate2-rs), but it can also be used on it's own. Using flate2 with the ```rust_backend``` feature provides an easy to use streaming API for miniz_oxide.

Requires at least rust 1.34.

## Usage
Simple compression/decompression:
```rust

extern crate miniz_oxide;

use miniz_oxide::inflate::decompress_to_vec;
use miniz_oxide::deflate::compress_to_vec;

fn roundtrip(data: &[u8]) {
    let compressed = compress_to_vec(data, 6);
    let decompressed = decompress_to_vec(decompressed.as_slice()).expect("Failed to decompress!");
}

```

# Zune-JPEG

A fast, correct and safe jpeg decoder in pure Rust.

## Usage

The library provides a simple-to-use API for jpeg decoding
and an ability to add options to influence decoding.

### Example

```Rust
// Import the library
use zune_jpeg::JpegDecoder;
use std::fs::read;

fn main()->Result<(),DecoderErrors> {
    // load some jpeg data
    let file_contents = BufReader::new(std::fs::File::open("a_jpeg.file").unwrap());
    // load the decoder
    let mut decoder = JpegDecoder::new(file_contents);
    // decode to pixels
    let mut pixels = decoder.decode().unwrap();
}
```

The decoder supports more manipulations via `DecoderOptions`,
see additional documentation in the library.

### Incremental input

`JpegDecoder` can be retried on the same decoder when the underlying reader can
see more bytes later. Callers should treat `DecodeErrors::is_recoverable_eof()`
as the signal to feed more input and retry; any other error is a hard decode
failure.

After `decode_headers()` succeeds, `info()` and `output_buffer_size()` are
available. During `decode_into()`, the same decoder and output buffer must be
kept across retries. If scan decoding returns recoverable EOF,
`decoded_output_bytes()` and `decoded_scanlines()` report the stable prefix of
the output buffer that can be displayed or copied before retrying.

By default, row checkpoints are recorded only after a previous scan decode
attempt, so one-shot decoding keeps the lowest-overhead path. Call
`set_incremental_mode(true)` before the first `decode_into()` attempt when the
caller expects input to arrive incrementally; this records checkpoints during
baseline Huffman scans and enables progressive preview preservation on the first
progressive decode attempt.

Fine-grained row checkpoints currently apply within baseline Huffman scan
bodies, including baseline multi-SOS / non-interleaved images. Those images may
still report no stable output rows until the later component scans have been
decoded and final assembly has run.

Scan checkpoints store only scalar resume state: stream position, next MCU
row/column, restart countdown, SOS parameters, DC predictors, and bitstream
state. Coefficients for already-decoded component scans stay on the decoder
across retries. For multi-SOS images this means a retry can continue inside the
current component scan, then decode later component scans, but output rows are
not considered stable until final assembly has all component data.

For progressive JPEGs, incremental scan preservation can render completed scans
as full-frame previews from committed coefficient data. The preview APIs report
that replaceable frame separately from the stable final-output prefix, which
remains zero until all scans complete. When preservation is enabled, the active
scan decodes into scratch coefficient storage and is committed only after the
scan completes. No decoder-owned preview pixel buffer is kept; recoverable EOF
may render committed coefficients into the caller-provided output slice.
Internal buffers are raw DCT coefficient planes, while preview bytes are already
IDCT-processed, upsampled, and converted into the requested output colorspace.

On recoverable EOF, callers can use the same output buffer in two ways:

- `decoded_output_bytes()` / `decoded_scanlines()` describe stable final pixels.
  These bytes will not need to be corrected by later retries. For progressive
  JPEGs, this stable prefix remains zero until all scans complete.
- `decoded_preview_output_bytes()` / `decoded_preview_scanlines()` describe a
  progressive preview assembled from completed scans. These methods return
  `None` for non-progressive images and `Some(0)` for progressive images before
  the first preview is rendered.
- `decoded_scans()` reports how many progressive scans are represented in the
  current preview. It returns `None` for non-progressive images; if the value is
  unchanged across retries, no newer preview has been produced.

Use the preview methods only after `decode_into()` returns a recoverable EOF. A
positive preview byte count means the same output buffer now contains a complete
provisional frame in `pixels[..preview_bytes]`. Treat that frame as replaceable:
repaint it when `decoded_scans()` increases, then keep retrying with the same
decoder and output buffer until `decode_into()` returns `Ok(())`. Do not append
preview bytes to the final output stream; they are a display surface separate
from the stable prefix reported by `decoded_output_bytes()`.

```Rust
use zune_core::bytestream::ZCursor;
use zune_jpeg::JpegDecoder;

let mut decoder = JpegDecoder::new(ZCursor::new(&jpeg_bytes));

loop {
  match decoder.decode_headers() {
    Ok(()) => break,
    Err(error) if error.is_recoverable_eof() => {
      // Make more input bytes visible to the same reader, then retry.
    }
    Err(error) => return Err(error)
  }
}

let mut pixels = vec![0; decoder.output_buffer_size().unwrap()];
decoder.set_incremental_mode(true);
let mut displayed_preview_scans = 0;

loop {
  match decoder.decode_into(&mut pixels) {
    Ok(()) => break,
    Err(error) if error.is_recoverable_eof() => {
      let stable_bytes = decoder.decoded_output_bytes().unwrap_or(0);
      let stable_scanlines = decoder.decoded_scanlines().unwrap_or(0);
      let preview_bytes = decoder.decoded_preview_output_bytes().unwrap_or(0);
      let preview_scanlines = decoder.decoded_preview_scanlines().unwrap_or(0);
      let preview_scans = decoder.decoded_scans().unwrap_or(0);

      if stable_bytes > 0 && stable_scanlines > 0 {
        // Display or copy the stable prefix in `pixels[..stable_bytes]`.
      } else if preview_bytes > 0
        && preview_scanlines > 0
        && preview_scans > displayed_preview_scans
      {
        // Repaint the replaceable preview in `pixels[..preview_bytes]`.
        displayed_preview_scans = preview_scans;
      }

      // Feed more input, then retry with the same decoder and `pixels` buffer.
    }
    Err(error) => return Err(error)
  }
}
```

## Goals

The implementation aims to have the following goals achieved,
in order of importance

1. Safety - Do not segfault on errors or invalid input. Panics are okay, but
   should be fixed when reported. `unsafe` is only used for SIMD intrinsics,
   and can be turned off entirely both at compile time and at runtime.
2. Speed - Get the data as quickly as possible, which means
    1. Platform intrinsics code where justifiable
    2. Carefully written platform independent code that allows the
       compiler to vectorize it.
    3. Regression tests.
    4. Watch the memory usage of the program
3. Usability - Provide utility functions like different color conversions functions.

## Non-Goals

- Bit identical results with libjpeg/libjpeg-turbo will never be an aim of this library.
  Jpeg is a lossy format with very few parts specified by the standard
  (i.e it doesn't give a reference upsampling and color conversion algorithm)

## Features

- [x] A Pretty fast 8*8 integer IDCT.
- [x] Fast Huffman Decoding
- [x] Fast color convert functions.
- [x] Support for extended colorspaces like GrayScale and RGBA
- [X] Single-threaded decoding.
- [X] Support for four component JPEGs, and esoteric color schemes like CYMK
- [X] Support for `no_std`
- [X] BGR/BGRA decoding support.

## Crate Features

| feature | on  | Capabilities                                                                                |
|---------|-----|---------------------------------------------------------------------------------------------|
| `x86`   | yes | Enables `x86` specific instructions, specifically `avx` and `sse` for accelerated decoding. |
| `std`   | yes | Enable linking to the `std` crate                                                           |
| `arith` | no  | Enable decoding of images using arithmetic coding for coefficients                          |

Note that the `x86` features are automatically disabled on platforms that aren't x86 during compile
time hence there is no need to disable them explicitly if you are targeting such a platform.

## Using in a `no_std` environment

The crate can be used in a `no_std` environment with the `alloc` feature.

But one is required to link to a working allocator for whatever environment the decoder
will be running on

## Debug vs release

The decoder heavily relies on platform specific intrinsics, namely AVX2 and SSE to gain speed-ups in decoding,
but they [perform poorly](https://godbolt.org/z/vPq57z13b) in debug builds. To get reasonable performance even
when compiling your program in debug mode, add this to your `Cargo.toml`:

```toml
# `zune-jpeg` package will be always built with optimizations
[profile.dev.package.zune-jpeg]
opt-level = 3
```

## Benchmarks

The library tries to be at fast as [libjpeg-turbo] while being as safe as possible.
Platform specific intrinsics help get speed up intensive operations ensuring we can almost
match [libjpeg-turbo] speeds but speeds are always +- 10 ms of this library.

For more up-to-date benchmarks, see the online repo with
benchmarks [here](https://etemesi254.github.io/assets/criterion/report/index.html)


[libjpeg-turbo]:https://github.com/libjpeg-turbo/libjpeg-turbo/

[image-rs/jpeg-decoder]:https://github.com/image-rs/jpeg-decoder/tree/master/src

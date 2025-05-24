# Texture Compressor

This directory contains a compressor for GPU texture formats. It is primarily
meant for compressing thumbnails to save memory. Currently, only the ETC1 format
is supported, and this is only used on Android.

## Goals

  * Fast: Compressing a 4K screenshot takes less than 100ms (roughly 100
    megapixels/s). Currently, it can achieve 220 megapixels/s on x64 desktop.
  * Safe: Free of memory safety issues and suitable for running in the
    privileged browser process.
  * Portable: SIMD code shared across x86 (SSE) and ARM (NEON). Easy to modify
    and validate for correctness.

## Algorithm

Vectorization is done by assigning each 4x4 SIMD lane to a different block. This
minimizes cross-lane shuffle operations (which are typically architecture
dependent) and makes the code significantly more portable.

Compression of ETC1 is done by first searching through the flip / no-flip and
individual / differential space to decide on the quantization of average, then
searching through selector tables and values for each subblock.

Detailed comments about the algorithm can be found in the source.

## Similar projects

  * [etcpak][etcpak] heavily inspired this project. Most of the algorithm
    (except vectorization scheme) and math tricks are borrowed from etcpak.
  * [ISPC Texture Compressor][ispctexcomp] influenced the vectorization scheme.
    Unlike this project, the compressor is written in ISPC and requires a
    dedicated compiler.
  * [AOSP ETC1 library][aospetc1] also uses a similar search algorithm and is
    easier to read, thanks to the scalar implementation.

While reading the code, the [ETC1 specification][etc1spec] should also be
useful.

## (Currently) Missing Pieces

This project is functional, but still a work-in-progress. The following pieces
are missing:

  * Testing coverage
  * Dynamic dispatch (multiversioning) and AVX2 support
  * Running on stable toolchain (using the `wide` crate instead of
    `#[feature(portable_simd)]`)

[etcpak]: https://github.com/wolfpld/etcpak
[ispctexcomp]: https://github.com/GameTechDev/ISPCTextureCompressor
[aospetc1]: https://cs.android.com/android/platform/superproject/main/+/main:frameworks/native/opengl/libs/ETC1/
[etc1spec]: https://registry.khronos.org/DataFormat/specs/1.1/dataformat.1.1.html#ETC1

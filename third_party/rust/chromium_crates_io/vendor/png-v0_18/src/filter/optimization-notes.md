Auto-vectorization notes
========================

[2023/01 @okaneco] - Notes on optimizing decoding filters

Links:
[PR]: https://github.com/image-rs/image-png/pull/382
[SWAR]: http://aggregate.org/SWAR/over.html
[AVG]: http://aggregate.org/MAGIC/#Average%20of%20Integers

#382 heavily refactored and optimized the following filters making the
implementation nonobvious. These comments function as a summary of that
PR with an explanation of the choices made below.

#382 originally started with trying to optimize using a technique called
SWAR, SIMD Within a Register. SWAR uses regular integer types like `u32`
and `u64` as SIMD registers to perform vertical operations in parallel,
usually involving bit-twiddling. This allowed each `BytesPerPixel` (bpp)
pixel to be decoded in parallel: 3bpp and 4bpp in a `u32`, 6bpp and 8pp
in a `u64`. The `Sub` filter looked like the following code block, `Avg`
was similar but used a bitwise average method from [AVG]:
```
// See "Unpartitioned Operations With Correction Code" from [SWAR]
fn swar_add_u32(x: u32, y: u32) -> u32 {
    // 7-bit addition so there's no carry over the most significant bit
    let n = (x & 0x7f7f7f7f) + (y & 0x7f7f7f7f); // 0x7F = 0b_0111_1111
    // 1-bit parity/XOR addition to fill in the missing MSB
    n ^ (x ^ y) & 0x80808080                     // 0x80 = 0b_1000_0000
}

let mut prev =
    u32::from_ne_bytes([current[0], current[1], current[2], current[3]]);
for chunk in current[4..].chunks_exact_mut(4) {
    let cur = u32::from_ne_bytes([chunk[0], chunk[1], chunk[2], chunk[3]]);
    let new_chunk = swar_add_u32(cur, prev);
    chunk.copy_from_slice(&new_chunk.to_ne_bytes());
    prev = new_chunk;
}
```
While this provided a measurable increase, @fintelia found that this idea
could be taken even further by unrolling the chunks component-wise and
avoiding unnecessary byte-shuffling by using byte arrays instead of
`u32::from|to_ne_bytes`. The bitwise operations were no longer necessary
so they were reverted to their obvious arithmetic equivalent. Lastly,
`TryInto` was used instead of `copy_from_slice`. The `Sub` code now
looked like this (with asserts to remove `0..bpp` bounds checks):
```
assert!(len > 3);
let mut prev = [current[0], current[1], current[2], current[3]];
for chunk in current[4..].chunks_exact_mut(4) {
    let new_chunk = [
        chunk[0].wrapping_add(prev[0]),
        chunk[1].wrapping_add(prev[1]),
        chunk[2].wrapping_add(prev[2]),
        chunk[3].wrapping_add(prev[3]),
    ];
    *TryInto::<&mut [u8; 4]>::try_into(chunk).unwrap() = new_chunk;
    prev = new_chunk;
}
```
The compiler was able to optimize the code to be even faster and this
method even sped up Paeth filtering! Assertions were experimentally
added within loop bodies which produced better instructions but no
difference in speed. Finally, the code was refactored to remove manual
slicing and start the previous pixel chunks with arrays of `[0; N]`.
```
let mut prev = [0; 4];
for chunk in current.chunks_exact_mut(4) {
    let new_chunk = [
        chunk[0].wrapping_add(prev[0]),
        chunk[1].wrapping_add(prev[1]),
        chunk[2].wrapping_add(prev[2]),
        chunk[3].wrapping_add(prev[3]),
    ];
    *TryInto::<&mut [u8; 4]>::try_into(chunk).unwrap() = new_chunk;
    prev = new_chunk;
}
```
While we're not manually bit-twiddling anymore, a possible takeaway from
this is to "think in SWAR" when dealing with small byte arrays. Unrolling
array operations and performing them component-wise may unlock previously
unavailable optimizations from the compiler, even when using the
`chunks_exact` methods for their potential auto-vectorization benefits.

`std::simd` notes
=================

In the past we have experimented with `std::simd` for unfiltering.  This
experiment was removed in https://github.com/image-rs/image-png/pull/585
because:

* The crate's microbenchmarks showed that `std::simd` didn't have a
  significant advantage over auto-vectorization for most filters, except
  for Paeth unfiltering - see
  https://github.com/image-rs/image-png/pull/414#issuecomment-1736655668
* In the crate's microbenchmarks `std::simd` seemed to help with Paeth
  unfiltering only on x86/x64, with mixed results on ARM - see
  https://github.com/image-rs/image-png/pull/539#issuecomment-2512748043
* In Chromium end-to-end microbenchmarks `std::simd` either didn't help
  or resulted in a small regression (as measured on x64).  See
  https://crrev.com/c/6090592.
* Field trial data from some "real world" scenarios shows that
  performance can be quite good without relying on `std::simd` - see
  https://github.com/image-rs/image-png/discussions/562#discussioncomment-13303307

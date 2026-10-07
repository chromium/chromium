# `wide` Changelog

## 1.7.1

* [#327](https://github.com/Lokathor/wide/pull/327): Fixed `i/u32x4::to_bitmask`
  incorrect result in fallback path

## 1.7

* Added `Select` trait for arbitrary types in the `select` method
* Added support for `select` with mixed types (e.g., selecting `f32x4` values
  with `u32x4` mask)
* Added `from_i16x16` and `From<i16x16>` to `i32x16`.
* Added `from_i8x32` and `From<i8x32>` to `i16x32`.
* Added runtime-index shuffle functions. Previous functions `swizzle` and
  `swizzle_relaxed` have been deprecated. See
  [PR #317](https://github.com/lokathor/wide/pull/317) for more information.
* Optimize `u8xL` shifts by a constant on AVX by using shift instructions on
  `u16x(L/2)`, followed by masking out the bits shifted across bytes.
  See [PR #319](https://github.com/Lokathor/wide/pull/319), which supersedes
  [#313](https://github.com/Lokathor/wide/pull/313).
* Added `from_ne_bytes` and `to_ne_bytes` to all SIMD vector types. See
  [this issue](https://github.com/Lokathor/wide/issues/144) for more
  information.
* Added `u8x64` and `i8x64` types. See
  [this PR](https://github.com/Lokathor/wide/issues/318).
* Fixed the `signum` documentation: the docs previously misstated the grammar
  and the set of inputs that map to `1.0`/`-1.0`. The behavior itself was
  already correct and matches [`f32::signum`].
* Optimized float `signum`. This changes the bit-patterns of returned NaNs.
* Optimized `copysign`.
* Added `add_mul_lo` and `add_mul_hi` for `u32xN` and `u64xN`: a fused
  multiply-add over the low `W` bits of each lane, keeping the low or high half
  of the product. `u64xN` uses `vpmadd52lo/hi` on AVX-512-IFMA when `W == 52`.
* Added `unpack_lo` and `unpack_hi` for `u32xN` and `u64xN`.

## 1.6.1

* Reverted a compile time optimization that triggered a compiler bug. See
  [this issue](https://github.com/Lokathor/wide/issues/303) for more
  information.
* Fixed code path errors that resulted when `avx512f` was enabled without
  `avx512dq`, which is a valid potential build. CI checks have been added for
  this combination to prevent recurrence.

## 1.6.0

* **Potential Change:** `i64xN` functions `to_bitmask`, `any`, `all` and `none`
  no longer guarantee that the sign-bit is used to decide whether an element is
  considered "true" or "false". This is being counted as a "bug" in the docs for
  previously being too specific.
* Added cast functions `cast_unsigned`, `cast_signed`, `to_bits`, `from_bits`.
* Added `u8x32`/`i8x32` `swizzle` and `swizzle_relaxed`: a full-width 32-entry
  byte table lookup (`vpermb` on AVX-512-VBMI, `vqtbl2` on NEON, emulated on
  AVX2/SSSE3).
* Fixed `i8x32`/`u8x32` `swizzle_half` on AVX2: out-of-range indices now
  correctly zero their output lane (previously leaked `self[..][0]`).
* Made `to_array`, `as_array` and `as_mut_array` available in const contexts.
* Added four unbounded-shift functions for integers.
* Renamed float function `pow_{simd-type-name}` to `powf_simd` and deprecated `powf`.
* Added conversions between `wide` types and native intrinsics SIMD types.
* Added `reduce_mul` for integers.
* Added integer functions `reduce_mul` and `mul_keep_low_high`.
* Added overflowing arithmetic for integers.
* Added float function `round_ties_even`.
* Fixed bugs in the fallback paths of `any`, `all`, `none` and `fast_clamp`.
* Added support for shifting SIMD vectors by values of `isize` and `usize` and
  SIMD vectors of "opposite-signedness".
* Deprecated `blend` and replaced it with `select` and `bitselect`.
* Fixed bug in `powf`.
* Fixed `clamp` and added guarantees for `fast_clamp`.
* Added small optimizations to `saturating_add/sub`.
* Fixed shift operators overflow behavior.
* Fixed `round` which previously behaved like `round_ties_even`.
* Fixed `UpperExp` formatting for floats.
* Renamed integer function `mul_widen` to `widening_mul` and added it for
  remaining types.
* Added integer function `mul_keep_high` for remaining types.
* Added missing `#[must_use]` annotations
* Updated documenattion.

## 1.5.0

* Added several functions and trait implementations that previously were only
  implemented for some types inconsistently.
* Added inherent `simd_*` comparison functions and deprecated the `Cmp*` traits.
* Added integer `Div` and `Rem` implementations.
* Added integer functions `clamp`, `saturating_mul` and `saturating_div`.
* Added signed integer functions `is_positive` and `signum`.
* Added additional float constants.
* Fixed slight bug in `f64xN` functions `round` and `round_int`.
* Fixed `simd_ne` NaN behavior for floats.
* Fixed bug in `u64x8::simd_lt`.
* Fixed bug in `i64x8` and `u64x8` function `to_bitmask`
* Fixed bugs in `u32x16` functions `any`, `all` and `none`.
* Optimized signed integer function `is_negative`.

## 1.4.0

* Added more float functions.
* Implemented `Rem` for float types.
* Renamed float function `sign_bit` to `is_sign_negative` and added
  `is_sign_positive`.
* Corrected [several bugs](https://github.com/Lokathor/wide/pull/261) with
  non-finite inputs for various math functions.

## 1.3.0

* Fixes the behaviour of `f32x16` functions `is_finite` and `round_int`. They
  previously gave incorrect output in some cases.
* Added more `f32x16` functionality.

## 1.2.0

* added reduce operations and dot to `i16x32`

## 1.1.2

* Use native NEON intrinsics for `f32x4::blend` and `f64x2::blend` on aarch64,
  improving performance by using a single `vbslq` instruction instead of the
  generic 3-operation fallback.

## 1.1.1

* Further improvements to the `Neg` impls on non-x86 targets.

## 1.1.0

* add `i64x2::min`, `i64x2::max`.
* add `u64x2::min`, `u64x2::max`.
* add `u64x8::min`.

## 1.0.3

* fix floating point negation edge cases, which also allows optimizations in
  more cases.

## 1.0.2

* fix edge case where `wide` was using the wrong avx512 sub-features to select
  when `safe_arch` functions could be called, causing build errors.

## 1.0.1

* initial stable version

## 0.8.3

* `to_bitmask` implemented for all unsigned int types.

## 0.8.2

* Fixed additional build errors on wasm targets.

## 0.8.1

* Fixed type errors in the fallback implementations of some methods that
  completely prevented compilation of the crate on some targets.

## 0.8.0

* **Breaking:**
  * `move_mask` returns unsigned values now.
  * `move_mask` and many other methods renamed to better align with the standard
    library portable simd types. See [issue
    209](https://github.com/Lokathor/wide/issues/209) for details.

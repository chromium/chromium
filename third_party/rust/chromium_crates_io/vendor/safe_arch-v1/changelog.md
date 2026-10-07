# `safe_arch`

## 1.2

* add an `avx512ifma` module with `add_mul_low_u52_*` / `add_mul_high_u52_*`
  (`vpmadd52luq` / `vpmadd52huq`) wrappers for `m128i`, `m256i` and `m512i`
* add `shuffle_abv_i32_all_m128i` / `shuffle_abv_i64_all_m128i` /
  `shuffle_abv_i32_all_m256i` / `shuffle_abv_i64_all_m256i` /
  `shuffle_abv_i64_all_m512i` (`vpermt2d` / `vpermt2q`) wrappers
* add `unpack_low_i64_m512i` / `unpack_high_i64_m512i` wrappers
* fix `mul_i32_wide_m512i` and `mul_u32_wide_m512i` being gated on `avx512dq`
  when the intrinsics they wrap only require `avx512f`

## 1.1

* add permute_i8_m256i / permute_i8_m512i (vpermb) wrappers

## 1.0.0

* Initial stable version.

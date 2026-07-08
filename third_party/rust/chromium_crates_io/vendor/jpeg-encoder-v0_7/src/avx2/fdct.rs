/*
 * Ported from mozjpeg / jfdctint-avx2.asm to rust
 * Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * Copyright (C) 2009, 2016, 2018, 2020, D. R. Commander.
 *
 * Based on the x86 SIMD extension for IJG JPEG library
 * Copyright (C) 1999-2006, MIYASAKA Masaru.
 */

#[cfg(target_arch = "x86")]
use core::arch::x86::{
    __m256i, _mm256_add_epi16, _mm256_add_epi32, _mm256_loadu_si256, _mm256_madd_epi16,
    _mm256_packs_epi32, _mm256_permute2x128_si256, _mm256_permute4x64_epi64, _mm256_set_epi16,
    _mm256_set_epi32, _mm256_sign_epi16, _mm256_slli_epi16, _mm256_srai_epi16, _mm256_srai_epi32,
    _mm256_storeu_si256, _mm256_sub_epi16, _mm256_unpackhi_epi16, _mm256_unpackhi_epi32,
    _mm256_unpacklo_epi16, _mm256_unpacklo_epi32,
};

#[cfg(target_arch = "x86_64")]
use core::arch::x86_64::{
    __m256i, _mm256_add_epi16, _mm256_add_epi32, _mm256_loadu_si256, _mm256_madd_epi16,
    _mm256_packs_epi32, _mm256_permute2x128_si256, _mm256_permute4x64_epi64, _mm256_set_epi16,
    _mm256_set_epi32, _mm256_sign_epi16, _mm256_slli_epi16, _mm256_srai_epi16, _mm256_srai_epi32,
    _mm256_storeu_si256, _mm256_sub_epi16, _mm256_unpackhi_epi16, _mm256_unpackhi_epi32,
    _mm256_unpacklo_epi16, _mm256_unpacklo_epi32,
};

const CONST_BITS: i32 = 13;
const PASS1_BITS: i32 = 2;

// FIX(0.298631336)
const F_0_298: i16 = 2446;
// FIX(0.390180644)
const F_0_390: i16 = 3196;
// FIX(0.541196100)
const F_0_541: i16 = 4433;
// FIX(0.765366865)
const F_0_765: i16 = 6270;
//FIX(0.899976223)
const F_0_899: i16 = 7373;
//FIX(1.175875602)
const F_1_175: i16 = 9633;
//FIX(1.501321110)
const F_1_501: i16 = 12299;
//FIX(1.847759065)
const F_1_847: i16 = 15137;
//FIX(1.961570560)
const F_1_961: i16 = 16069;
//FIX(2.053119869)
const F_2_053: i16 = 16819;
//FIX(2.562915447)
const F_2_562: i16 = 20995;
//FIX(3.072711026)
const F_3_072: i16 = 25172;

const DESCALE_P1: i32 = CONST_BITS - PASS1_BITS;
const DESCALE_P2: i32 = CONST_BITS + PASS1_BITS;

#[inline(always)]
pub fn fdct_avx2(data: &mut [i16; 64]) {
    unsafe {
        fdct_avx2_internal(data);
    }
}

#[target_feature(enable = "avx2")]
fn fdct_avx2_internal(data: &mut [i16; 64]) {
    #[target_feature(enable = "avx2")]
    #[allow(non_snake_case)]
    #[inline]
    fn PW_F130_F054_MF130_F054() -> __m256i {
        _mm256_set_epi16(
            F_0_541,
            F_0_541 - F_1_847,
            F_0_541,
            F_0_541 - F_1_847,
            F_0_541,
            F_0_541 - F_1_847,
            F_0_541,
            F_0_541 - F_1_847,
            F_0_541,
            F_0_541 + F_0_765,
            F_0_541,
            F_0_541 + F_0_765,
            F_0_541,
            F_0_541 + F_0_765,
            F_0_541,
            F_0_541 + F_0_765,
        )
    }

    #[target_feature(enable = "avx2")]
    #[allow(non_snake_case)]
    #[inline]
    fn PW_MF078_F117_F078_F117() -> __m256i {
        _mm256_set_epi16(
            F_1_175,
            F_1_175 - F_0_390,
            F_1_175,
            F_1_175 - F_0_390,
            F_1_175,
            F_1_175 - F_0_390,
            F_1_175,
            F_1_175 - F_0_390,
            F_1_175,
            F_1_175 - F_1_961,
            F_1_175,
            F_1_175 - F_1_961,
            F_1_175,
            F_1_175 - F_1_961,
            F_1_175,
            F_1_175 - F_1_961,
        )
    }

    #[target_feature(enable = "avx2")]
    #[allow(non_snake_case)]
    #[inline]
    fn PW_MF060_MF089_MF050_MF256() -> __m256i {
        _mm256_set_epi16(
            -F_2_562,
            F_2_053 - F_2_562,
            -F_2_562,
            F_2_053 - F_2_562,
            -F_2_562,
            F_2_053 - F_2_562,
            -F_2_562,
            F_2_053 - F_2_562,
            -F_0_899,
            F_0_298 - F_0_899,
            -F_0_899,
            F_0_298 - F_0_899,
            -F_0_899,
            F_0_298 - F_0_899,
            -F_0_899,
            F_0_298 - F_0_899,
        )
    }

    #[target_feature(enable = "avx2")]
    #[allow(non_snake_case)]
    #[inline]
    fn PW_F050_MF256_F060_MF089() -> __m256i {
        _mm256_set_epi16(
            -F_0_899,
            F_1_501 - F_0_899,
            -F_0_899,
            F_1_501 - F_0_899,
            -F_0_899,
            F_1_501 - F_0_899,
            -F_0_899,
            F_1_501 - F_0_899,
            -F_2_562,
            F_3_072 - F_2_562,
            -F_2_562,
            F_3_072 - F_2_562,
            -F_2_562,
            F_3_072 - F_2_562,
            -F_2_562,
            F_3_072 - F_2_562,
        )
    }

    #[target_feature(enable = "avx2")]
    #[allow(non_snake_case)]
    #[inline]
    fn PD_DESCALE_P(first_pass: bool) -> __m256i {
        if first_pass {
            _mm256_set_epi32(
                1 << (DESCALE_P1 - 1),
                1 << (DESCALE_P1 - 1),
                1 << (DESCALE_P1 - 1),
                1 << (DESCALE_P1 - 1),
                1 << (DESCALE_P1 - 1),
                1 << (DESCALE_P1 - 1),
                1 << (DESCALE_P1 - 1),
                1 << (DESCALE_P1 - 1),
            )
        } else {
            _mm256_set_epi32(
                1 << (DESCALE_P2 - 1),
                1 << (DESCALE_P2 - 1),
                1 << (DESCALE_P2 - 1),
                1 << (DESCALE_P2 - 1),
                1 << (DESCALE_P2 - 1),
                1 << (DESCALE_P2 - 1),
                1 << (DESCALE_P2 - 1),
                1 << (DESCALE_P2 - 1),
            )
        }
    }

    #[target_feature(enable = "avx2")]
    #[allow(non_snake_case)]
    #[inline]
    fn PW_DESCALE_P2X() -> __m256i {
        _mm256_set_epi32(
            1 << (PASS1_BITS - 1),
            1 << (PASS1_BITS - 1),
            1 << (PASS1_BITS - 1),
            1 << (PASS1_BITS - 1),
            1 << (PASS1_BITS - 1),
            1 << (PASS1_BITS - 1),
            1 << (PASS1_BITS - 1),
            1 << (PASS1_BITS - 1),
        )
    }

    // In-place 8x8x16-bit matrix transpose using AVX2 instructions
    #[target_feature(enable = "avx2")]
    #[inline]
    fn do_transpose(
        i1: __m256i,
        i2: __m256i,
        i3: __m256i,
        i4: __m256i,
    ) -> (__m256i, __m256i, __m256i, __m256i) {
        //i1=(00 01 02 03 04 05 06 07  40 41 42 43 44 45 46 47)
        //i2=(10 11 12 13 14 15 16 17  50 51 52 53 54 55 56 57)
        //i3=(20 21 22 23 24 25 26 27  60 61 62 63 64 65 66 67)
        //i4=(30 31 32 33 34 35 36 37  70 71 72 73 74 75 76 77)

        let t5 = _mm256_unpacklo_epi16(i1, i2);
        let t6 = _mm256_unpackhi_epi16(i1, i2);
        let t7 = _mm256_unpacklo_epi16(i3, i4);
        let t8 = _mm256_unpackhi_epi16(i3, i4);

        // transpose coefficients(phase 1)
        // t1=(00 10 01 11 02 12 03 13  40 50 41 51 42 52 43 53)
        // t2=(04 14 05 15 06 16 07 17  44 54 45 55 46 56 47 57)
        // t3=(20 30 21 31 22 32 23 33  60 70 61 71 62 72 63 73)
        // t4=(24 34 25 35 26 36 27 37  64 74 65 75 66 76 67 77)

        let t1 = _mm256_unpacklo_epi32(t5, t7);
        let t2 = _mm256_unpackhi_epi32(t5, t7);
        let t3 = _mm256_unpacklo_epi32(t6, t8);
        let t4 = _mm256_unpackhi_epi32(t6, t8);

        // transpose coefficients(phase 2)
        // t5=(00 10 20 30 01 11 21 31  40 50 60 70 41 51 61 71)
        // t6=(02 12 22 32 03 13 23 33  42 52 62 72 43 53 63 73)
        // t7=(04 14 24 34 05 15 25 35  44 54 64 74 45 55 65 75)
        // t8=(06 16 26 36 07 17 27 37  46 56 66 76 47 57 67 77)

        (
            _mm256_permute4x64_epi64(t1, 0x8D),
            _mm256_permute4x64_epi64(t2, 0x8D),
            _mm256_permute4x64_epi64(t3, 0xD8),
            _mm256_permute4x64_epi64(t4, 0xD8),
        )
    }

    // In-place 8x8x16-bit accurate integer forward DCT using AVX2 instructions
    #[target_feature(enable = "avx2")]
    #[inline]
    fn do_dct(
        first_pass: bool,
        i1: __m256i,
        i2: __m256i,
        i3: __m256i,
        i4: __m256i,
    ) -> (__m256i, __m256i, __m256i, __m256i) {
        let t5 = _mm256_sub_epi16(i1, i4); // data1_0 - data6_7 = tmp6_7
        let t6 = _mm256_add_epi16(i1, i4); // data1_0 + data6_7 = tmp1_0
        let t7 = _mm256_add_epi16(i2, i3); // data3_2 + data4_5 = tmp3_2
        let t8 = _mm256_sub_epi16(i2, i3); // data3_2 - data4_5 = tmp4_5

        // Even part

        let t6 = _mm256_permute2x128_si256(t6, t6, 0x01); // t6=tmp0_1
        let t1 = _mm256_add_epi16(t6, t7); // t1 = tmp0_1 + tmp3_2 = tmp10_11
        let t6 = _mm256_sub_epi16(t6, t7); // t6 = tmp0_1 - tmp3_2 = tmp13_12

        let t7 = _mm256_permute2x128_si256(t1, t1, 0x01); // t7 = tmp11_10
        let t1 = _mm256_sign_epi16(
            t1,
            _mm256_set_epi16(-1, -1, -1, -1, -1, -1, -1, -1, 1, 1, 1, 1, 1, 1, 1, 1),
        ); // tmp10_neg11

        let t7 = _mm256_add_epi16(t7, t1); // t7 = (tmp10 + tmp11)_(tmp10 - tmp11)

        let t1 = if first_pass {
            _mm256_slli_epi16(t7, PASS1_BITS)
        } else {
            let t7 = _mm256_add_epi16(t7, PW_DESCALE_P2X());
            _mm256_srai_epi16(t7, PASS1_BITS)
        };

        // (Original)
        // z1 = (tmp12 + tmp13) * 0.541196100;
        // data2 = z1 + tmp13 * 0.765366865;
        // data6 = z1 + tmp12 * -1.847759065;
        //
        // (This implementation)
        // data2 = tmp13 * (0.541196100 + 0.765366865) + tmp12 * 0.541196100;
        // data6 = tmp13 * 0.541196100 + tmp12 * (0.541196100 - 1.847759065);

        let t7 = _mm256_permute2x128_si256(t6, t6, 0x01); // t7 = tmp12_13
        let t2 = _mm256_unpacklo_epi16(t6, t7);
        let t6 = _mm256_unpackhi_epi16(t6, t7);

        let t2 = _mm256_madd_epi16(t2, PW_F130_F054_MF130_F054()); // t2 = data2_6L
        let t6 = _mm256_madd_epi16(t6, PW_F130_F054_MF130_F054()); // t6 = data2_6H

        let t2 = _mm256_add_epi32(t2, PD_DESCALE_P(first_pass));
        let t6 = _mm256_add_epi32(t6, PD_DESCALE_P(first_pass));

        let t2 = if first_pass {
            _mm256_srai_epi32(t2, DESCALE_P1)
        } else {
            _mm256_srai_epi32(t2, DESCALE_P2)
        };
        let t6 = if first_pass {
            _mm256_srai_epi32(t6, DESCALE_P1)
        } else {
            _mm256_srai_epi32(t6, DESCALE_P2)
        };

        let t3 = _mm256_packs_epi32(t2, t6); // t6 = data2_6

        // Odd part

        let t7 = _mm256_add_epi16(t8, t5); // t7 = tmp4_5 + tmp6_7 = z3_4

        // (Original)
        // z5 = (z3 + z4) * 1.175875602;
        // z3 = z3 * -1.961570560;
        // z4 = z4 * -0.390180644;
        // z3 += z5;
        // z4 += z5;
        //
        // (This implementation)
        // z3 = z3 * (1.175875602 - 1.961570560) + z4 * 1.175875602;
        // z4 = z3 * 1.175875602 + z4 * (1.175875602 - 0.390180644);

        let t2 = _mm256_permute2x128_si256(t7, t7, 0x01); // t2 = z4_3
        let t6 = _mm256_unpacklo_epi16(t7, t2);
        let t7 = _mm256_unpackhi_epi16(t7, t2);

        let t6 = _mm256_madd_epi16(t6, PW_MF078_F117_F078_F117()); // t6 = z3_4L
        let t7 = _mm256_madd_epi16(t7, PW_MF078_F117_F078_F117()); // t7 = z3_4H

        // (Original)
        // z1 = tmp4 + tmp7;
        // z2 = tmp5 + tmp6;
        // tmp4 = tmp4 * 0.298631336;
        // tmp5 = tmp5 * 2.053119869;
        // tmp6 = tmp6 * 3.072711026;
        // tmp7 = tmp7 * 1.501321110;
        // z1 = z1 * -0.899976223;
        // z2 = z2 * -2.562915447;
        // data7 = tmp4 + z1 + z3;
        // data5 = tmp5 + z2 + z4;
        // data3 = tmp6 + z2 + z3;
        // data1 = tmp7 + z1 + z4;
        //
        // (This implementation)
        // tmp4 = tmp4 * (0.298631336 - 0.899976223) + tmp7 * -0.899976223;
        // tmp5 = tmp5 * (2.053119869 - 2.562915447) + tmp6 * -2.562915447;
        // tmp6 = tmp5 * -2.562915447 + tmp6 * (3.072711026 - 2.562915447);
        // tmp7 = tmp4 * -0.899976223 + tmp7 * (1.501321110 - 0.899976223);
        // data7 = tmp4 + z3;
        // data5 = tmp5 + z4;
        // data3 = tmp6 + z3;
        // data1 = tmp7 + z4;

        let t4 = _mm256_permute2x128_si256(t5, t5, 0x01); // t4 = tmp7_6
        let t2 = _mm256_unpacklo_epi16(t8, t4);
        let t4 = _mm256_unpackhi_epi16(t8, t4);

        let t2 = _mm256_madd_epi16(t2, PW_MF060_MF089_MF050_MF256()); //t2 = tmp4_5L
        let t4 = _mm256_madd_epi16(t4, PW_MF060_MF089_MF050_MF256()); // t4 = tmp4_5H

        let t2 = _mm256_add_epi32(t2, t6); // t2 = data7_5L
        let t4 = _mm256_add_epi32(t4, t7); // t4 = data7_5H

        let t2 = _mm256_add_epi32(t2, PD_DESCALE_P(first_pass));
        let t4 = _mm256_add_epi32(t4, PD_DESCALE_P(first_pass));

        let t2 = if first_pass {
            _mm256_srai_epi32(t2, DESCALE_P1)
        } else {
            _mm256_srai_epi32(t2, DESCALE_P2)
        };
        let t4 = if first_pass {
            _mm256_srai_epi32(t4, DESCALE_P1)
        } else {
            _mm256_srai_epi32(t4, DESCALE_P2)
        };

        let t4 = _mm256_packs_epi32(t2, t4); // t4 = data7_5

        let t2 = _mm256_permute2x128_si256(t8, t8, 0x01); // t2 = tmp5_4

        let t8 = _mm256_unpacklo_epi16(t5, t2);
        let t5 = _mm256_unpackhi_epi16(t5, t2);

        let t8 = _mm256_madd_epi16(t8, PW_F050_MF256_F060_MF089()); // t8 = tmp6_7L
        let t5 = _mm256_madd_epi16(t5, PW_F050_MF256_F060_MF089()); // t5 = tmp6_7H

        let t8 = _mm256_add_epi32(t8, t6); // t8 = data3_1L
        let t5 = _mm256_add_epi32(t5, t7); // t5 = data3_1H

        let t8 = _mm256_add_epi32(t8, PD_DESCALE_P(first_pass));
        let t5 = _mm256_add_epi32(t5, PD_DESCALE_P(first_pass));

        let t8 = if first_pass {
            _mm256_srai_epi32(t8, DESCALE_P1)
        } else {
            _mm256_srai_epi32(t8, DESCALE_P2)
        };
        let t5 = if first_pass {
            _mm256_srai_epi32(t5, DESCALE_P1)
        } else {
            _mm256_srai_epi32(t5, DESCALE_P2)
        };

        let t2 = _mm256_packs_epi32(t8, t5); // t2 = data3_1

        (t1, t2, t3, t4)
    }

    let ymm4 = avx_load(&data[0..16]);
    let ymm5 = avx_load(&data[16..32]);
    let ymm6 = avx_load(&data[32..48]);
    let ymm7 = avx_load(&data[48..64]);

    // ---- Pass 1: process rows.
    // ymm4=(00 01 02 03 04 05 06 07  10 11 12 13 14 15 16 17)
    // ymm5=(20 21 22 23 24 25 26 27  30 31 32 33 34 35 36 37)
    // ymm6=(40 41 42 43 44 45 46 47  50 51 52 53 54 55 56 57)
    // ymm7=(60 61 62 63 64 65 66 67  70 71 72 73 74 75 76 77)

    let ymm0 = _mm256_permute2x128_si256(ymm4, ymm6, 0x20);
    let ymm1 = _mm256_permute2x128_si256(ymm4, ymm6, 0x31);
    let ymm2 = _mm256_permute2x128_si256(ymm5, ymm7, 0x20);
    let ymm3 = _mm256_permute2x128_si256(ymm5, ymm7, 0x31);

    // ymm0=(00 01 02 03 04 05 06 07  40 41 42 43 44 45 46 47)
    // ymm1=(10 11 12 13 14 15 16 17  50 51 52 53 54 55 56 57)
    // ymm2=(20 21 22 23 24 25 26 27  60 61 62 63 64 65 66 67)
    // ymm3=(30 31 32 33 34 35 36 37  70 71 72 73 74 75 76 77)

    let (ymm0, ymm1, ymm2, ymm3) = do_transpose(ymm0, ymm1, ymm2, ymm3);
    let (ymm0, ymm1, ymm2, ymm3) = do_dct(true, ymm0, ymm1, ymm2, ymm3);

    // ---- Pass 2: process columns.

    let ymm4 = _mm256_permute2x128_si256(ymm1, ymm3, 0x20); // ymm4=data3_7
    let ymm1 = _mm256_permute2x128_si256(ymm1, ymm3, 0x31); // ymm1=data1_5

    let (ymm0, ymm1, ymm2, ymm4) = do_transpose(ymm0, ymm1, ymm2, ymm4);
    let (ymm0, ymm1, ymm2, ymm4) = do_dct(false, ymm0, ymm1, ymm2, ymm4);

    let ymm3 = _mm256_permute2x128_si256(ymm0, ymm1, 0x30); // ymm3=data0_1
    let ymm5 = _mm256_permute2x128_si256(ymm2, ymm1, 0x20); // ymm5=data2_3
    let ymm6 = _mm256_permute2x128_si256(ymm0, ymm4, 0x31); // ymm6=data4_5
    let ymm7 = _mm256_permute2x128_si256(ymm2, ymm4, 0x21); // ymm7=data6_7

    avx_store(ymm3, &mut data[0..16]);
    avx_store(ymm5, &mut data[16..32]);
    avx_store(ymm6, &mut data[32..48]);
    avx_store(ymm7, &mut data[48..64]);
}

/// Safe wrapper for an unaligned AVX load
#[target_feature(enable = "avx2")]
#[inline]
fn avx_load(input: &[i16]) -> __m256i {
    assert!(input.len() == 16);
    assert!(core::mem::size_of::<[i16; 16]>() == core::mem::size_of::<__m256i>());
    // SAFETY: we've checked sizes above. The load is unaligned, so no alignment requirements.
    unsafe { _mm256_loadu_si256(input.as_ptr() as *const __m256i) }
}

/// Safe wrapper for an unaligned AVX store
#[target_feature(enable = "avx2")]
#[inline]
fn avx_store(input: __m256i, output: &mut [i16]) {
    assert!(output.len() == 16);
    assert!(core::mem::size_of::<[i16; 16]>() == core::mem::size_of::<__m256i>());
    // SAFETY: we've checked sizes above. The load is unaligned, so no alignment requirements.
    unsafe { _mm256_storeu_si256(output.as_mut_ptr() as *mut __m256i, input) }
}

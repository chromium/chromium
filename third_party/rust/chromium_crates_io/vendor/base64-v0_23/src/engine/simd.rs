//! SIMD-accelerated engines for the standard and URL-safe alphabets.
//!
//! These are gated behind the `simd-unsafe` feature because they use `unsafe`. Three engines are
//! provided:
//!
//! - `Simd` detects the best available instruction set at runtime and falls back to the scalar
//!   [`GeneralPurpose`] engine when none is available. It requires `std` for the detection.
//! - `Avx2` and `Neon` target a specific instruction set with no runtime detection, so they can
//!   be used in `no_std` builds when the target is known to support the instructions.
//!
//! Only the STANDARD and URL_SAFE alphabets are accelerated (they share indices `0..=61` and differ
//! only at `62`/`63`). Each engine therefore has dedicated `standard` / `url_safe` constructors
//! rather than taking an arbitrary [`Alphabet`](crate::alphabet::Alphabet); use [`GeneralPurpose`]
//! for any other alphabet.
//!
//! The kernels follow Wojciech Mula's vectorized base64 algorithms
//! (<http://0x80.pl/notesen/2016-01-17-sse-base64-decoding.html> and the companion encoding note).
//! AVX2 uses the multiply-based bit (de)interleave; NEON, which lacks the relevant multiplies, uses
//! the shift/mask variant. Both share the same per-alphabet lookup tables, expressed as the
//! associated constants of the `SimdAlphabet` trait so the kernels can inline them.
#![allow(unsafe_code)]

use crate::alphabet::Symbol;
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
use crate::{
    engine::{
        general_purpose::{
            decode::decode_helper, encode_helper, GeneralPurpose, GeneralPurposeConfig,
            GeneralPurposeEstimate,
        },
        DecodeMetadata, Engine,
    },
    DecodeSliceError,
};

/// A base64 alphabet family the SIMD kernels can accelerate.
///
/// Carries the per-alphabet lookup tables as associated constants. Private (hence sealed);
/// implemented only by [`Standard`] and [`UrlSafe`], selected at runtime by [`SimdKind`].
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
trait SimdAlphabet {
    /// pshufb encode table: maps a reduced 6-bit index to `ascii - index`.
    const ENCODE_LUT: [i8; 16];
    /// pshufb decode table indexed by the high nibble; added to the byte to produce its 6-bit value.
    const DECODE_SHIFT_LUT: [i8; 16];
    /// pshufb decode table indexed by the low nibble; bit `hi` marks `(hi, lo)` as a valid symbol.
    const DECODE_MASK_LUT: [u8; 16];
    /// The high-`62`/`63` symbol whose shift needs the fixup below (`+`/`-`).
    const DECODE_FIXUP_CHAR: i8;
    /// The shift applied to [`DECODE_FIXUP_CHAR`](Self::DECODE_FIXUP_CHAR).
    const DECODE_FIXUP_SHIFT: i8;
}

/// The STANDARD alphabet family (`+`/`/` at 62/63).
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
enum Standard {}

/// The URL_SAFE alphabet family (`-`/`_` at 62/63).
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
enum UrlSafe {}

#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
impl SimdAlphabet for Standard {
    const ENCODE_LUT: [i8; 16] = [
        65, 71, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -19, -16, 0, 0,
    ];
    const DECODE_SHIFT_LUT: [i8; 16] = [0, 0, 19, 4, -65, -65, -71, -71, 0, 0, 0, 0, 0, 0, 0, 0];
    const DECODE_MASK_LUT: [u8; 16] = [
        0xA8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF0, 0x54, 0x50, 0x50, 0x50,
        0x54,
    ];
    const DECODE_FIXUP_CHAR: i8 = 0x2F;
    const DECODE_FIXUP_SHIFT: i8 = 16;
}

#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
impl SimdAlphabet for UrlSafe {
    const ENCODE_LUT: [i8; 16] = [
        65, 71, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -17, 32, 0, 0,
    ];
    const DECODE_SHIFT_LUT: [i8; 16] = [0, 0, 17, 4, -65, -65, -71, -71, 0, 0, 0, 0, 0, 0, 0, 0];
    const DECODE_MASK_LUT: [u8; 16] = [
        0xA8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF0, 0x50, 0x50, 0x54, 0x50,
        0x70,
    ];
    const DECODE_FIXUP_CHAR: i8 = 0x5F;
    const DECODE_FIXUP_SHIFT: i8 = -32;
}

/// Which accelerated alphabet family an engine uses. Selects the kernel monomorphization at runtime.
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum SimdKind {
    Standard,
    UrlSafe,
}

/// Minimum input length before a SIMD path is used. Below these the setup cost outweighs the gain;
/// encode needs more data than decode to break even.
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
const SIMD_MIN_INPUT_ENCODE: usize = 128;
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
const SIMD_MIN_INPUT_DECODE: usize = 64;

/// pshufb decode validity table: maps a high nibble to a single set bit. Shared by both alphabets.
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
const BITPOS_LUT: [u8; 16] = [1, 2, 4, 8, 16, 32, 64, 128, 0, 0, 0, 0, 0, 0, 0, 0];

#[cfg(target_arch = "x86_64")]
mod avx2 {
    use super::{SimdAlphabet, BITPOS_LUT, SIMD_MIN_INPUT_DECODE, SIMD_MIN_INPUT_ENCODE};
    use core::arch::x86_64::*;

    /// Encode leading whole 24-byte input groups (24 in -> 32 out per iteration).
    ///
    /// # Safety
    ///
    /// The running CPU must support AVX2.
    #[target_feature(enable = "avx2")]
    pub(super) unsafe fn encode_bulk<A: SimdAlphabet>(
        input: &[u8],
        output: &mut [u8],
    ) -> (usize, usize) {
        if input.len() < SIMD_MIN_INPUT_ENCODE {
            return (0, 0);
        }
        let lut_data = A::ENCODE_LUT;
        // SAFETY: `lut_data` is a 16-byte array; `_mm_loadu_si128` reads exactly 16 bytes from it.
        let lut = _mm256_broadcastsi128_si256(_mm_loadu_si128(lut_data.as_ptr().cast()));
        #[rustfmt::skip]
        let shuf = _mm256_setr_epi8(
            1, 0, 2, 1, 4, 3, 5, 4, 7, 6, 8, 7, 10, 9, 11, 10,
            1, 0, 2, 1, 4, 3, 5, 4, 7, 6, 8, 7, 10, 9, 11, 10,
        );
        let mask_hi = _mm256_set1_epi32(0x0fc0_fc00_u32 as i32);
        let mul_hi = _mm256_set1_epi32(0x0400_0040);
        let mask_lo = _mm256_set1_epi32(0x003f_03f0);
        let mul_lo = _mm256_set1_epi32(0x0100_0010);
        let const51 = _mm256_set1_epi8(51);
        let const25 = _mm256_set1_epi8(25);

        let mut i = 0usize;
        let mut o = 0usize;
        // A whole 32-byte vector is read but only 24 bytes are consumed, so 32 input bytes must be
        // available; the store writes 32 output bytes.
        while i + 32 <= input.len() && o + 32 <= output.len() {
            // SAFETY: the loop guard ensures `input[i..i+32]` is in bounds, so this reads 32 valid
            // bytes; `loadu` has no alignment requirement.
            let data = _mm256_loadu_si256(input.as_ptr().add(i).cast());
            // TODO https://arxiv.org/abs/1704.00605 doesn't seem to require this perm step, which
            // costs 3 cycles, and is slightly different in the reduce phase as well.
            // Rearrange dwords so low lane = bytes[0..16], high lane = bytes[12..28].
            let perm = _mm256_permutevar8x32_epi32(data, _mm256_setr_epi32(0, 1, 2, 3, 3, 4, 5, 6));
            let inb = _mm256_shuffle_epi8(perm, shuf);

            let t0 = _mm256_and_si256(inb, mask_hi);
            let t1 = _mm256_mulhi_epu16(t0, mul_hi);
            let t2 = _mm256_and_si256(inb, mask_lo);
            let t3 = _mm256_mullo_epi16(t2, mul_lo);
            let indices = _mm256_or_si256(t1, t3); // one 6-bit value (0..=63) per byte

            let reduced = _mm256_subs_epu8(indices, const51);
            let gt25 = _mm256_cmpgt_epi8(indices, const25);
            let reduced = _mm256_sub_epi8(reduced, gt25);
            let ascii = _mm256_add_epi8(indices, _mm256_shuffle_epi8(lut, reduced));

            // SAFETY: the loop guard ensures `output[o..o+32]` is in bounds; `storeu` writes 32
            // bytes with no alignment requirement.
            _mm256_storeu_si256(output.as_mut_ptr().add(o).cast(), ascii);
            i += 24;
            o += 32;
        }
        (i, o)
    }

    /// Decode leading whole 32-byte input blocks (32 in -> 24 out per iteration).
    ///
    /// # Safety
    ///
    /// The running CPU must support AVX2.
    #[target_feature(enable = "avx2")]
    pub(super) unsafe fn decode_bulk<A: SimdAlphabet>(
        input: &[u8],
        quads_end: usize,
        output: &mut [u8],
    ) -> (usize, usize) {
        if quads_end < SIMD_MIN_INPUT_DECODE {
            return (0, 0);
        }
        let shift_data = A::DECODE_SHIFT_LUT;
        let mask_data = A::DECODE_MASK_LUT;
        // SAFETY: each LUT is a 16-byte array read in full by `_mm_loadu_si128`.
        let shift_lut = _mm256_broadcastsi128_si256(_mm_loadu_si128(shift_data.as_ptr().cast()));
        let mask_lut = _mm256_broadcastsi128_si256(_mm_loadu_si128(mask_data.as_ptr().cast()));
        let bitpos_lut = _mm256_broadcastsi128_si256(_mm_loadu_si128(BITPOS_LUT.as_ptr().cast()));
        let low_nibble_mask = _mm256_set1_epi8(0x0f);
        let fixup_char = _mm256_set1_epi8(A::DECODE_FIXUP_CHAR);
        let fixup_shift = _mm256_set1_epi8(A::DECODE_FIXUP_SHIFT);
        let zero = _mm256_setzero_si256();
        let merge_mul1 = _mm256_set1_epi32(0x0140_0140);
        let merge_mul2 = _mm256_set1_epi32(0x0001_1000);
        #[rustfmt::skip]
        let pack_shuf = _mm256_setr_epi8(
            2, 1, 0, 6, 5, 4, 10, 9, 8, 14, 13, 12, -1, -1, -1, -1,
            2, 1, 0, 6, 5, 4, 10, 9, 8, 14, 13, 12, -1, -1, -1, -1,
        );
        let lane_compact = _mm256_setr_epi32(0, 1, 2, 4, 5, 6, 6, 6);

        let mut i = 0usize;
        let mut o = 0usize;
        // Need 32 input bytes available to load; the store writes exactly 24 output bytes.
        while i + 32 <= quads_end && o + 24 <= output.len() {
            // SAFETY: `i + 32 <= quads_end <= input.len()`, so this reads 32 in-bounds bytes;
            // `loadu` needs no alignment.
            let data = _mm256_loadu_si256(input.as_ptr().add(i).cast());

            let hi_nibbles = _mm256_and_si256(_mm256_srli_epi32(data, 4), low_nibble_mask);
            let lo_nibbles = _mm256_and_si256(data, low_nibble_mask);

            let m = _mm256_shuffle_epi8(mask_lut, lo_nibbles);
            let bit = _mm256_shuffle_epi8(bitpos_lut, hi_nibbles);
            let non_match = _mm256_cmpeq_epi8(_mm256_and_si256(m, bit), zero);
            if _mm256_movemask_epi8(non_match) != 0 {
                // Invalid byte in this block; let the scalar decoder report the exact offset.
                break;
            }

            let sh = _mm256_shuffle_epi8(shift_lut, hi_nibbles);
            let eq_fixup = _mm256_cmpeq_epi8(data, fixup_char);
            let shift = _mm256_blendv_epi8(sh, fixup_shift, eq_fixup);
            let values = _mm256_add_epi8(data, shift); // 6-bit value per byte

            let merged = _mm256_maddubs_epi16(values, merge_mul1);
            let packed = _mm256_madd_epi16(merged, merge_mul2);
            let shuffled = _mm256_shuffle_epi8(packed, pack_shuf);
            let compact = _mm256_permutevar8x32_epi32(shuffled, lane_compact);

            // Store exactly 24 bytes (16 + 8); a wider store would clobber an oversized output.
            let lo = _mm256_castsi256_si128(compact);
            let hi = _mm256_extracti128_si256(compact, 1);
            // SAFETY: the loop guard ensures `o + 24 <= output.len()`, so the 16-byte store at `o`
            // and the 8-byte store at `o + 16` are both in bounds; neither needs alignment.
            _mm_storeu_si128(output.as_mut_ptr().add(o).cast(), lo);
            _mm_storel_epi64(output.as_mut_ptr().add(o + 16).cast(), hi);
            i += 32;
            o += 24;
        }
        (i, o)
    }
}

#[cfg(target_arch = "aarch64")]
mod neon {
    use super::{SimdAlphabet, BITPOS_LUT, SIMD_MIN_INPUT_DECODE, SIMD_MIN_INPUT_ENCODE};
    use core::arch::aarch64::*;

    /// Encode leading whole 12-byte input groups (12 in -> 16 out per iteration).
    ///
    /// # Safety
    ///
    /// The running CPU must support NEON.
    #[target_feature(enable = "neon")]
    pub(super) unsafe fn encode_bulk<A: SimdAlphabet>(
        input: &[u8],
        output: &mut [u8],
    ) -> (usize, usize) {
        if input.len() < SIMD_MIN_INPUT_ENCODE {
            return (0, 0);
        }
        let lut_data = A::ENCODE_LUT;
        let split_bytes: [u8; 16] = [1, 0, 2, 1, 4, 3, 5, 4, 7, 6, 8, 7, 10, 9, 11, 10];
        // SAFETY: `split_bytes` and `lut_data` are 16-byte arrays, each read in full by `vld1q_u8`.
        let split_shuf = vld1q_u8(split_bytes.as_ptr());
        let translate = vld1q_u8(lut_data.as_ptr().cast());
        // note the in-memory LE byte order will affect how these match the shuffled data bytes
        let m1 = vdupq_n_u32(0x0000_fc00);
        let m2 = vdupq_n_u32(0x0000_03f0);
        let m3 = vdupq_n_u32(0x0fc0_0000);
        let m4 = vdupq_n_u32(0x003f_0000);
        let c51 = vdupq_n_u8(51);
        let c25 = vdupq_n_s8(25);

        let mut i = 0usize;
        let mut o = 0usize;
        // Reads a full 16-byte vector but consumes only 12 bytes, so 16 must be readable; writes
        // exactly 16 output bytes.
        while i + 16 <= input.len() && o + 16 <= output.len() {
            // SAFETY: the loop guard ensures `input[i..i+16]` is in bounds, so this reads 16 valid
            // bytes.
            let data = vld1q_u8(input.as_ptr().add(i));
            let x0 = vreinterpretq_u32_u8(vqtbl1q_u8(data, split_shuf));
            // data now is 4 32-bit words in x0, each with the 3 bytes to encode in that word:
            // 1 0 2 1
            // 4 3 5 4
            // 7 6 8 7
            // a 9 b a

            // select bits in chunks of 6 into their own bytes, treating the input as a bit sequence

            // select the left 6 bits of [0, 3, 6, 9] (in byte 1 of the words) and shift until the
            // 6 bits are the low 6 bits of byte 0, then cast to be in 2 byte words
            let x1 = vshrq_n_u16::<10>(vreinterpretq_u16_u32(vandq_u32(x0, m1)));
            // right 2 bits of [0 3 6 9], left 4 bits of [1 4 7 a], shifted to low bits
            let x2 = vshlq_n_u16::<4>(vreinterpretq_u16_u32(vandq_u32(x0, m2)));
            // right 4 bits of [1 4 7 a], left 2 bits of [2 5 8 b]
            let x3 = vshrq_n_u16::<6>(vreinterpretq_u16_u32(vandq_u32(x0, m3)));
            // right 6 bits of [2 5 8 b]
            let x4 = vshlq_n_u16::<8>(vreinterpretq_u16_u32(vandq_u32(x0, m4)));
            let indices = vreinterpretq_u8_u16(vorrq_u16(vorrq_u16(x1, x2), vorrq_u16(x3, x4)));

            // reduce the 6-bit index to a translate-LUT index, then add the offset to get ascii
            let reduced = vqsubq_u8(indices, c51);
            let gt25 = vcgtq_s8(vreinterpretq_s8_u8(indices), c25);
            let reduced = vsubq_u8(reduced, gt25); // subtracting 0xFF adds 1 where index > 25
            let ascii = vaddq_u8(indices, vqtbl1q_u8(translate, reduced));

            // SAFETY: the loop guard ensures `output[o..o+16]` is in bounds, so this writes 16 bytes
            // in bounds.
            vst1q_u8(output.as_mut_ptr().add(o), ascii);
            i += 12;
            o += 16;
        }
        (i, o)
    }

    /// Decode leading whole 16-byte input blocks (16 in -> 12 out per iteration).
    ///
    /// # Safety
    ///
    /// The running CPU must support NEON.
    #[target_feature(enable = "neon")]
    pub(super) unsafe fn decode_bulk<A: SimdAlphabet>(
        input: &[u8],
        quads_end: usize,
        output: &mut [u8],
    ) -> (usize, usize) {
        if quads_end < SIMD_MIN_INPUT_DECODE {
            return (0, 0);
        }
        // SAFETY: each LUT is a 16-byte array read in full by `vld1q_u8`.
        let shift_lut = vld1q_u8(A::DECODE_SHIFT_LUT.as_ptr().cast());
        let mask_lut = vld1q_u8(A::DECODE_MASK_LUT.as_ptr());
        let bitpos_lut = vld1q_u8(BITPOS_LUT.as_ptr());
        let low_nibble_mask = vdupq_n_u8(0x0f);
        let fixup_char_v = vdupq_n_u8(A::DECODE_FIXUP_CHAR as u8);
        let fixup_shift_v = vdupq_n_u8(A::DECODE_FIXUP_SHIFT as u8);
        let zero = vdupq_n_u8(0);
        let mm1 = vdupq_n_u32(0x003f_003f);
        let mm2 = vdupq_n_u32(0x3f00_3f00);
        let out_mask = vdupq_n_u32(0x00ff_ffff);
        let pack_bytes: [u8; 16] = [
            2, 1, 0, 6, 5, 4, 10, 9, 8, 14, 13, 12, 0x80, 0x80, 0x80, 0x80,
        ];
        // SAFETY: `pack_bytes` is a 16-byte array read in full by `vld1q_u8`.
        let pack_shuf = vld1q_u8(pack_bytes.as_ptr());

        let mut i = 0usize;
        let mut o = 0usize;
        // Need 16 input bytes to load; writes exactly 12 output bytes.
        while i + 16 <= quads_end && o + 12 <= output.len() {
            // SAFETY: `i + 16 <= quads_end <= input.len()`, so this reads 16 in-bounds bytes.
            let data = vld1q_u8(input.as_ptr().add(i));
            let hi = vshrq_n_u8::<4>(data);
            let lo = vandq_u8(data, low_nibble_mask);

            let m = vqtbl1q_u8(mask_lut, lo);
            let bit = vqtbl1q_u8(bitpos_lut, hi);
            let non_match = vceqq_u8(vandq_u8(m, bit), zero);
            if vmaxvq_u8(non_match) != 0 {
                // Invalid byte in this block; let the scalar decoder report the exact offset.
                break;
            }

            let sh = vqtbl1q_u8(shift_lut, hi);
            let eq_fixup = vceqq_u8(data, fixup_char_v);
            let shift = vbslq_u8(eq_fixup, fixup_shift_v, sh);
            let values = vaddq_u8(data, shift); // {00aaaaaa|00bbbbbb|00cccccc|00dddddd} x4

            // merge 4x6 bits -> 3 bytes per quad via shift/mask (no multiplies on NEON)
            let v = vreinterpretq_u32_u8(values);
            let x1 = vandq_u32(v, mm1); // {00aaaaaa|00000000|00cccccc|00000000}
            let x2 = vandq_u32(v, mm2); // {00000000|00bbbbbb|00000000|00dddddd}
            let x3 = vorrq_u32(vshlq_n_u32::<18>(x1), vshrq_n_u32::<10>(x1));
            let x4 = vorrq_u32(vshlq_n_u32::<4>(x2), vshrq_n_u32::<24>(x2));
            let merged = vandq_u32(vorrq_u32(x3, x4), out_mask);
            let packed = vqtbl1q_u8(vreinterpretq_u8_u32(merged), pack_shuf); // 12 bytes in [0..12)

            // Store exactly 12 bytes (8 + 4); a wider store would clobber an oversized output.
            // SAFETY: the loop guard ensures `o + 12 <= output.len()`, so the 8-byte store at `o`
            // and the 4-byte store at `o + 8` are both in bounds.
            vst1_u8(output.as_mut_ptr().add(o), vget_low_u8(packed));
            // Can't use vst1q_lane_u32 to directly write the last lane as it requires 4-byte
            // alignment, which we don't have.
            // Could also shift words and then use vst1_u8 again, but this way is more direct and
            // doesn't double-write the middle word.
            let tail = vgetq_lane_u32::<2>(vreinterpretq_u32_u8(packed));
            core::ptr::write_unaligned(output.as_mut_ptr().add(o + 8).cast::<u32>(), tail);

            i += 16;
            o += 12;
        }
        (i, o)
    }
}

/// Dispatch the AVX2 encode kernel to the right alphabet monomorphization.
///
/// # Safety
///
/// The running CPU must support AVX2 (the requirement is forwarded to [`avx2::encode_bulk`]).
#[cfg(target_arch = "x86_64")]
#[inline]
unsafe fn avx2_encode(kind: SimdKind, input: &[u8], output: &mut [u8]) -> (usize, usize) {
    match kind {
        // SAFETY: this function's contract guarantees AVX2, which is all the kernel requires.
        SimdKind::Standard => avx2::encode_bulk::<Standard>(input, output),
        SimdKind::UrlSafe => avx2::encode_bulk::<UrlSafe>(input, output),
    }
}

/// Dispatch the AVX2 decode kernel to the right alphabet monomorphization.
///
/// # Safety
///
/// The running CPU must support AVX2 (the requirement is forwarded to [`avx2::decode_bulk`]).
#[cfg(target_arch = "x86_64")]
#[inline]
unsafe fn avx2_decode(
    kind: SimdKind,
    input: &[u8],
    quads_end: usize,
    output: &mut [u8],
) -> (usize, usize) {
    match kind {
        // SAFETY: this function's contract guarantees AVX2, which is all the kernel requires.
        SimdKind::Standard => avx2::decode_bulk::<Standard>(input, quads_end, output),
        SimdKind::UrlSafe => avx2::decode_bulk::<UrlSafe>(input, quads_end, output),
    }
}

/// Dispatch the NEON encode kernel to the right alphabet monomorphization.
///
/// # Safety
///
/// The running CPU must support NEON.
#[cfg(target_arch = "aarch64")]
#[inline]
unsafe fn neon_encode(kind: SimdKind, input: &[u8], output: &mut [u8]) -> (usize, usize) {
    match kind {
        // SAFETY: this function's contract guarantees NEON, which is all the kernel requires.
        SimdKind::Standard => neon::encode_bulk::<Standard>(input, output),
        SimdKind::UrlSafe => neon::encode_bulk::<UrlSafe>(input, output),
    }
}

/// Dispatch the NEON decode kernel to the right alphabet monomorphization.
///
/// # Safety
///
/// The running CPU must support NEON.
#[cfg(target_arch = "aarch64")]
#[inline]
unsafe fn neon_decode(
    kind: SimdKind,
    input: &[u8],
    quads_end: usize,
    output: &mut [u8],
) -> (usize, usize) {
    match kind {
        // SAFETY: this function's contract guarantees NEON, which is all the kernel requires.
        SimdKind::Standard => neon::decode_bulk::<Standard>(input, quads_end, output),
        SimdKind::UrlSafe => neon::decode_bulk::<UrlSafe>(input, quads_end, output),
    }
}

/// Which instruction set an engine dispatches to.
#[cfg(all(feature = "std", any(target_arch = "x86_64", target_arch = "aarch64")))]
#[derive(Clone, Copy, Debug)]
enum Backend {
    Scalar,
    #[cfg(target_arch = "x86_64")]
    Avx2,
    #[cfg(target_arch = "aarch64")]
    Neon,
}

/// A base64 engine that uses the best SIMD instruction set detected at runtime, falling back to the
/// scalar [`GeneralPurpose`] engine.
///
/// Requires the `std` feature (for runtime CPU-feature detection) and an `x86_64` or `aarch64`
/// target. On other targets, use [`GeneralPurpose`] directly. Only the STANDARD and URL_SAFE
/// alphabets are accelerated, so it is constructed with [`Simd::standard`] / [`Simd::url_safe`].
#[cfg(all(feature = "std", any(target_arch = "x86_64", target_arch = "aarch64")))]
#[derive(Debug, Clone)]
pub struct Simd {
    inner: GeneralPurpose,
    kind: SimdKind,
    backend: Backend,
}

#[cfg(all(feature = "std", any(target_arch = "x86_64", target_arch = "aarch64")))]
impl Simd {
    /// Create a `Simd` engine for the STANDARD alphabet, detecting the instruction set once.
    #[must_use]
    pub fn standard(config: GeneralPurposeConfig) -> Self {
        Self::new(SimdKind::Standard, &crate::alphabet::STANDARD, config)
    }

    /// Create a `Simd` engine for the URL_SAFE alphabet, detecting the instruction set once.
    #[must_use]
    pub fn url_safe(config: GeneralPurposeConfig) -> Self {
        Self::new(SimdKind::UrlSafe, &crate::alphabet::URL_SAFE, config)
    }

    fn new(
        kind: SimdKind,
        alphabet: &crate::alphabet::Alphabet,
        config: GeneralPurposeConfig,
    ) -> Self {
        #[cfg(target_arch = "x86_64")]
        let backend = if std::is_x86_feature_detected!("avx2") {
            Backend::Avx2
        } else {
            Backend::Scalar
        };
        #[cfg(target_arch = "aarch64")]
        let backend = if std::arch::is_aarch64_feature_detected!("neon") {
            Backend::Neon
        } else {
            Backend::Scalar
        };

        Self {
            inner: GeneralPurpose::new(alphabet, config),
            kind,
            backend,
        }
    }
}

#[cfg(all(feature = "std", any(target_arch = "x86_64", target_arch = "aarch64")))]
impl Engine for Simd {
    type Config = GeneralPurposeConfig;
    type DecodeEstimate = GeneralPurposeEstimate;

    fn internal_encode(&self, input: &[u8], output: &mut [u8]) -> usize {
        let kind = self.kind;
        match self.backend {
            Backend::Scalar => self.inner.internal_encode(input, output),
            #[cfg(target_arch = "x86_64")]
            Backend::Avx2 => encode_helper(self.inner.encode_table(), input, output, |i, o| {
                // SAFETY: the Avx2 backend is only selected when AVX2 was detected.
                unsafe { avx2_encode(kind, i, o) }
            }),
            #[cfg(target_arch = "aarch64")]
            Backend::Neon => encode_helper(self.inner.encode_table(), input, output, |i, o| {
                // SAFETY: the Neon backend is only selected when NEON was detected.
                unsafe { neon_encode(kind, i, o) }
            }),
        }
    }

    fn internal_decoded_len_estimate(&self, input_len: usize) -> Self::DecodeEstimate {
        self.inner.internal_decoded_len_estimate(input_len)
    }

    fn internal_decode(
        &self,
        input: &[u8],
        output: &mut [u8],
        estimate: Self::DecodeEstimate,
    ) -> Result<DecodeMetadata, DecodeSliceError> {
        let kind = self.kind;
        match self.backend {
            Backend::Scalar => self.inner.internal_decode(input, output, estimate),
            #[cfg(target_arch = "x86_64")]
            Backend::Avx2 => decode_helper(
                input,
                &estimate,
                output,
                self.inner.decode_table(),
                self.inner.config().decode_allow_trailing_bits(),
                self.inner.padding(),
                self.inner.config().decode_padding_mode(),
                // SAFETY: the Avx2 backend is only selected when AVX2 was detected.
                |i, end, o| unsafe { avx2_decode(kind, i, end, o) },
            ),
            #[cfg(target_arch = "aarch64")]
            Backend::Neon => decode_helper(
                input,
                &estimate,
                output,
                self.inner.decode_table(),
                self.inner.config().decode_allow_trailing_bits(),
                self.inner.padding(),
                self.inner.config().decode_padding_mode(),
                // SAFETY: the Neon backend is only selected when NEON was detected.
                |i, end, o| unsafe { neon_decode(kind, i, end, o) },
            ),
        }
    }

    fn config(&self) -> &Self::Config {
        self.inner.config()
    }

    fn padding(&self) -> Symbol {
        self.inner.padding()
    }
}

/// A base64 engine that unconditionally uses AVX2, without runtime detection.
///
/// This works in `no_std` builds. Because it does not check for AVX2 support, it must only be used
/// on a CPU that has it. Only the STANDARD and URL_SAFE alphabets are accelerated, so it is
/// constructed with the [`Avx2::standard`] / [`Avx2::url_safe`] (checked) or
/// [`Avx2::standard_unchecked`] / [`Avx2::url_safe_unchecked`] constructors.
#[cfg(target_arch = "x86_64")]
#[derive(Debug, Clone)]
pub struct Avx2 {
    inner: GeneralPurpose,
    kind: SimdKind,
}

#[cfg(target_arch = "x86_64")]
impl Avx2 {
    /// Create an `Avx2` engine for the STANDARD alphabet if the running CPU supports AVX2, else
    /// `None`.
    ///
    /// Requires the `std` feature for the detection; in `no_std` use [`Avx2::standard_unchecked`].
    #[cfg(feature = "std")]
    #[must_use]
    pub fn standard(config: GeneralPurposeConfig) -> Option<Self> {
        if std::is_x86_feature_detected!("avx2") {
            // SAFETY: AVX2 support was just verified.
            Some(unsafe { Self::standard_unchecked(config) })
        } else {
            None
        }
    }

    /// Create an `Avx2` engine for the URL_SAFE alphabet if the running CPU supports AVX2, else
    /// `None`.
    ///
    /// Requires the `std` feature for the detection; in `no_std` use [`Avx2::url_safe_unchecked`].
    #[cfg(feature = "std")]
    #[must_use]
    pub fn url_safe(config: GeneralPurposeConfig) -> Option<Self> {
        if std::is_x86_feature_detected!("avx2") {
            // SAFETY: AVX2 support was just verified.
            Some(unsafe { Self::url_safe_unchecked(config) })
        } else {
            None
        }
    }

    /// Create an `Avx2` engine for the STANDARD alphabet without checking for AVX2 support.
    ///
    /// # Safety
    ///
    /// The CPU that will run encode/decode must support AVX2. Using the engine on a CPU without
    /// AVX2 is undefined behavior.
    #[must_use]
    pub const unsafe fn standard_unchecked(config: GeneralPurposeConfig) -> Self {
        Self {
            inner: GeneralPurpose::new(&crate::alphabet::STANDARD, config),
            kind: SimdKind::Standard,
        }
    }

    /// Create an `Avx2` engine for the URL_SAFE alphabet without checking for AVX2 support.
    ///
    /// # Safety
    ///
    /// The CPU that will run encode/decode must support AVX2. Using the engine on a CPU without
    /// AVX2 is undefined behavior.
    #[must_use]
    pub const unsafe fn url_safe_unchecked(config: GeneralPurposeConfig) -> Self {
        Self {
            inner: GeneralPurpose::new(&crate::alphabet::URL_SAFE, config),
            kind: SimdKind::UrlSafe,
        }
    }
}

#[cfg(target_arch = "x86_64")]
impl Engine for Avx2 {
    type Config = GeneralPurposeConfig;
    type DecodeEstimate = GeneralPurposeEstimate;

    fn internal_encode(&self, input: &[u8], output: &mut [u8]) -> usize {
        let kind = self.kind;
        encode_helper(self.inner.encode_table(), input, output, |i, o| {
            // SAFETY: constructing this engine asserts AVX2 support.
            unsafe { avx2_encode(kind, i, o) }
        })
    }

    fn internal_decoded_len_estimate(&self, input_len: usize) -> Self::DecodeEstimate {
        self.inner.internal_decoded_len_estimate(input_len)
    }

    fn internal_decode(
        &self,
        input: &[u8],
        output: &mut [u8],
        estimate: Self::DecodeEstimate,
    ) -> Result<DecodeMetadata, DecodeSliceError> {
        let kind = self.kind;
        decode_helper(
            input,
            &estimate,
            output,
            self.inner.decode_table(),
            self.inner.config().decode_allow_trailing_bits(),
            self.inner.padding(),
            self.inner.config().decode_padding_mode(),
            // SAFETY: constructing this engine asserts AVX2 support.
            |i, end, o| unsafe { avx2_decode(kind, i, end, o) },
        )
    }

    fn config(&self) -> &Self::Config {
        self.inner.config()
    }

    fn padding(&self) -> Symbol {
        self.inner.padding()
    }
}

/// A base64 engine that unconditionally uses NEON, without runtime detection.
///
/// This engine is available on aarch64 targets compiled with NEON support and works in `no_std`.
#[cfg(target_arch = "aarch64")]
#[derive(Debug, Clone)]
pub struct Neon {
    inner: GeneralPurpose,
    kind: SimdKind,
}

#[cfg(target_arch = "aarch64")]
impl Neon {
    /// Create a `Neon` engine for the STANDARD alphabet on a target compiled with NEON support.
    #[must_use]
    pub const fn standard(config: GeneralPurposeConfig) -> Self {
        Self {
            inner: GeneralPurpose::new(&crate::alphabet::STANDARD, config),
            kind: SimdKind::Standard,
        }
    }

    /// Create a `Neon` engine for the URL_SAFE alphabet on a target compiled with NEON support.
    #[must_use]
    pub const fn url_safe(config: GeneralPurposeConfig) -> Self {
        Self {
            inner: GeneralPurpose::new(&crate::alphabet::URL_SAFE, config),
            kind: SimdKind::UrlSafe,
        }
    }
}

#[cfg(target_arch = "aarch64")]
impl Engine for Neon {
    type Config = GeneralPurposeConfig;
    type DecodeEstimate = GeneralPurposeEstimate;

    fn internal_encode(&self, input: &[u8], output: &mut [u8]) -> usize {
        let kind = self.kind;
        encode_helper(self.inner.encode_table(), input, output, |i, o| {
            // SAFETY: this module is only compiled for targets with NEON enabled.
            unsafe { neon_encode(kind, i, o) }
        })
    }

    fn internal_decoded_len_estimate(&self, input_len: usize) -> Self::DecodeEstimate {
        self.inner.internal_decoded_len_estimate(input_len)
    }

    fn internal_decode(
        &self,
        input: &[u8],
        output: &mut [u8],
        estimate: Self::DecodeEstimate,
    ) -> Result<DecodeMetadata, DecodeSliceError> {
        let kind = self.kind;
        decode_helper(
            input,
            &estimate,
            output,
            self.inner.decode_table(),
            self.inner.config().decode_allow_trailing_bits(),
            self.inner.padding(),
            self.inner.config().decode_padding_mode(),
            // SAFETY: this module is only compiled for targets with NEON enabled.
            |i, end, o| unsafe { neon_decode(kind, i, end, o) },
        )
    }

    fn config(&self) -> &Self::Config {
        self.inner.config()
    }

    fn padding(&self) -> Symbol {
        self.inner.padding()
    }
}

//! Specialized checksum code for the x86 CPU architecture, based on the efficient algorithm described
//! in the following whitepaper:
//!
//! Gopal, V., Ozturk, E., Guilford, J., Wolrich, G., Feghali, W., Dixon, M., & Karakoyunlu, D. (2009).
//! _Fast CRC computation for generic polynomials using PCLMULQDQ instruction_. Intel.
//! (Mirror link: <https://fossies.org/linux/zlib-ng/doc/crc-pclmulqdq.pdf>, accessed 2024-05-20)
//!
//! Throughout the code, this work is referred to as "the paper".
//!
//! On top of the 128-bit `PCLMULQDQ` implementation, two wider variants use `VPCLMULQDQ` to fold
//! several independent 128-bit streams per instruction: an `AVX2` variant over 256-bit `YMM`
//! registers (8 streams) and an `AVX-512` variant over 512-bit `ZMM` registers (16 streams). Both
//! rely on `VPCLMULQDQ` intrinsics stabilized in Rust 1.89 and are only compiled when the
//! `stable_vpclmulqdq` cfg is set by `build.rs`, leaving the crate MSRV unchanged otherwise. The
//! best variant supported by the running CPU is chosen at runtime.

#[cfg(target_arch = "x86")]
use core::arch::x86 as arch;
#[cfg(target_arch = "x86_64")]
use core::arch::x86_64 as arch;

/// Which SIMD implementation to use, chosen once at construction from the CPU's features.
#[derive(Clone, Copy)]
enum Kind {
    /// 128-bit `PCLMULQDQ`, fold by 4.
    Sse,
    /// 256-bit `VPCLMULQDQ`, 8 streams.
    #[cfg(stable_vpclmulqdq)]
    Avx2,
    /// 512-bit `VPCLMULQDQ`, 16 streams.
    #[cfg(stable_vpclmulqdq)]
    Avx512,
}

#[derive(Clone)]
pub struct State {
    state: u32,
    kind: Kind,
}

impl State {
    #[cfg(not(feature = "std"))]
    fn detect() -> Option<Kind> {
        if cfg!(target_feature = "pclmulqdq")
            && cfg!(target_feature = "sse2")
            && cfg!(target_feature = "sse4.1")
            && cfg!(target_feature = "ssse3")
        {
            #[cfg(stable_vpclmulqdq)]
            {
                if cfg!(target_feature = "avx512f") && cfg!(target_feature = "vpclmulqdq") {
                    return Some(Kind::Avx512);
                }
                if cfg!(target_feature = "avx2") && cfg!(target_feature = "vpclmulqdq") {
                    return Some(Kind::Avx2);
                }
            }

            return Some(Kind::Sse);
        }

        None
    }

    #[cfg(feature = "std")]
    fn detect() -> Option<Kind> {
        if is_x86_feature_detected!("pclmulqdq")
            && is_x86_feature_detected!("sse2")
            && is_x86_feature_detected!("sse4.1")
            && is_x86_feature_detected!("ssse3")
        {
            #[cfg(stable_vpclmulqdq)]
            {
                if is_x86_feature_detected!("avx512f") && is_x86_feature_detected!("vpclmulqdq") {
                    return Some(Kind::Avx512);
                }
                if is_x86_feature_detected!("avx2") && is_x86_feature_detected!("vpclmulqdq") {
                    return Some(Kind::Avx2);
                }
            }

            return Some(Kind::Sse);
        }

        None
    }

    pub fn new(state: u32) -> Option<Self> {
        // SAFETY: `detect` only returns a `Kind` whose instructions the CPU supports.
        Self::detect().map(|kind| Self { state, kind })
    }

    pub fn update(&mut self, buf: &[u8]) {
        // SAFETY: `State::new` ensured the CPU supports the instructions for `self.kind`.
        self.state = unsafe {
            match self.kind {
                Kind::Sse => calculate(self.state, buf),
                #[cfg(stable_vpclmulqdq)]
                Kind::Avx2 => calculate_avx2(self.state, buf),
                #[cfg(stable_vpclmulqdq)]
                Kind::Avx512 => calculate_avx512(self.state, buf),
            }
        }
    }

    pub fn finalize(self) -> u32 {
        self.state
    }

    pub fn reset(&mut self) {
        self.state = 0;
    }

    pub fn combine(&mut self, other: u32, amount: u64) {
        self.state = crate::combine::combine(self.state, other, amount);
    }
}

const K1: i64 = 0x154442bd4;
const K2: i64 = 0x1c6e41596;
const K3: i64 = 0x1751997d0;
const K4: i64 = 0x0ccaa009e;
const K5: i64 = 0x163cd6124;

const P_X: i64 = 0x1DB710641;
const U_PRIME: i64 = 0x1F7011641;

// The wider kernels have progressively more streams to initialize and collapse. Keep their
// crossovers conservative so short inputs do not pay that fixed cost.
const MIN_FOLD_BY_4_BYTES: usize = 128;
#[cfg(stable_vpclmulqdq)]
const MIN_AVX512_BYTES: usize = 2 * 1024;

// Fold constants for the wider strides. For a fold by `D` bits the pair is
// `(reflect(x^(D+32) mod P), reflect(x^(D-32) mod P))`, as for `K1`/`K2` (D = 512) and
// `K3`/`K4` (D = 128). AVX2 uses 8 streams (D = 1024), AVX-512 uses 16 streams (D = 2048).
#[cfg(stable_vpclmulqdq)]
const K_1024_LOW: i64 = 0x1e88ef372;
#[cfg(stable_vpclmulqdq)]
const K_1024_HIGH: i64 = 0x14a7fe880;
#[cfg(stable_vpclmulqdq)]
const K_2048_LOW: i64 = 0x11542778a;
#[cfg(stable_vpclmulqdq)]
const K_2048_HIGH: i64 = 0x1322d1430;

#[target_feature(
    enable = "pclmulqdq",
    enable = "sse2",
    enable = "sse4.1",
    enable = "ssse3"
)]
unsafe fn calculate(crc: u32, mut data: &[u8]) -> u32 {
    // Below 16 bytes there isn't even a single full block to load, so use the scalar fallback.
    if data.len() < 16 {
        return crate::baseline::update_fast_16(crc, data);
    }

    // For 16..127 bytes a single-accumulator fold-by-1 is enough; the fold-by-4 setup below only
    // pays off once there are several 64-byte groups.
    if data.len() < MIN_FOLD_BY_4_BYTES {
        let mut x = get(&mut data);
        x = arch::_mm_xor_si128(x, arch::_mm_cvtsi32_si128(!crc as i32));
        return reduce_128_to_crc(x, data);
    }

    // Step 1: fold by 4 loop
    let mut x3 = get(&mut data);
    let mut x2 = get(&mut data);
    let mut x1 = get(&mut data);
    let mut x0 = get(&mut data);

    // fold in our initial value, part of the incremental crc checksum
    x3 = arch::_mm_xor_si128(x3, arch::_mm_cvtsi32_si128(!crc as i32));

    let k1k2 = arch::_mm_set_epi64x(K2, K1);
    while data.len() >= 64 {
        x3 = reduce128(x3, get(&mut data), k1k2);
        x2 = reduce128(x2, get(&mut data), k1k2);
        x1 = reduce128(x1, get(&mut data), k1k2);
        x0 = reduce128(x0, get(&mut data), k1k2);
    }

    let k3k4 = arch::_mm_set_epi64x(K4, K3);
    let mut x = reduce128(x3, x2, k3k4);
    x = reduce128(x, x1, k3k4);
    x = reduce128(x, x0, k3k4);

    reduce_128_to_crc(x, data)
}

/// 256-bit `VPCLMULQDQ` variant: 8 streams across four `YMM` registers (two lanes each), folding
/// two streams per carry-less multiply.
#[cfg(stable_vpclmulqdq)]
#[allow(clippy::incompatible_msrv)] // intrinsics are gated to rustc >= 1.89 by build.rs
#[target_feature(
    enable = "pclmulqdq",
    enable = "sse2",
    enable = "sse4.1",
    enable = "ssse3",
    enable = "avx",
    enable = "avx2",
    enable = "vpclmulqdq"
)]
unsafe fn calculate_avx2(crc: u32, mut data: &[u8]) -> u32 {
    // Too small for the wide loop; use the 128-bit path.
    if data.len() < 256 {
        return calculate(crc, data);
    }

    // First 128 bytes as 8 streams (four YMM registers, two lanes each).
    let mut v0 = get256(&mut data);
    let mut v1 = get256(&mut data);
    let mut v2 = get256(&mut data);
    let mut v3 = get256(&mut data);

    // Fold the initial CRC into the lowest-offset stream.
    v0 = arch::_mm256_xor_si256(
        v0,
        arch::_mm256_castsi128_si256(arch::_mm_cvtsi32_si128(!crc as i32)),
    );

    let k = arch::_mm256_set_epi64x(K_1024_HIGH, K_1024_LOW, K_1024_HIGH, K_1024_LOW);
    while data.len() >= 128 {
        v0 = reduce256(v0, get256(&mut data), k);
        v1 = reduce256(v1, get256(&mut data), k);
        v2 = reduce256(v2, get256(&mut data), k);
        v3 = reduce256(v3, get256(&mut data), k);
    }

    // Collapse the 8 streams, in increasing byte offset, folding each into the next by 128 bits.
    let k3k4 = arch::_mm_set_epi64x(K4, K3);
    let mut x = arch::_mm256_castsi256_si128(v0);
    x = reduce128(x, arch::_mm256_extracti128_si256(v0, 1), k3k4);
    x = reduce128(x, arch::_mm256_castsi256_si128(v1), k3k4);
    x = reduce128(x, arch::_mm256_extracti128_si256(v1, 1), k3k4);
    x = reduce128(x, arch::_mm256_castsi256_si128(v2), k3k4);
    x = reduce128(x, arch::_mm256_extracti128_si256(v2, 1), k3k4);
    x = reduce128(x, arch::_mm256_castsi256_si128(v3), k3k4);
    x = reduce128(x, arch::_mm256_extracti128_si256(v3, 1), k3k4);

    reduce_128_to_crc(x, data)
}

/// 512-bit `VPCLMULQDQ` variant: 16 streams across four `ZMM` registers (four lanes each), folding
/// four streams per carry-less multiply.
#[cfg(stable_vpclmulqdq)]
#[allow(clippy::incompatible_msrv)] // intrinsics are gated to rustc >= 1.89 by build.rs
#[target_feature(
    enable = "pclmulqdq",
    enable = "sse2",
    enable = "sse4.1",
    enable = "ssse3",
    enable = "avx",
    enable = "avx2",
    enable = "vpclmulqdq",
    enable = "avx512f"
)]
unsafe fn calculate_avx512(crc: u32, mut data: &[u8]) -> u32 {
    // Use a conservative crossover because the 16-stream setup and collapse are costly near 1 KiB.
    if data.len() < MIN_AVX512_BYTES {
        return calculate_avx2(crc, data);
    }

    // First 256 bytes as 16 streams (four ZMM registers, four lanes each).
    let mut v0 = get512(&mut data);
    let mut v1 = get512(&mut data);
    let mut v2 = get512(&mut data);
    let mut v3 = get512(&mut data);

    // Fold the initial CRC into the lowest-offset stream.
    v0 = arch::_mm512_xor_si512(
        v0,
        arch::_mm512_castsi128_si512(arch::_mm_cvtsi32_si128(!crc as i32)),
    );

    let k = arch::_mm512_set_epi64(
        K_2048_HIGH,
        K_2048_LOW,
        K_2048_HIGH,
        K_2048_LOW,
        K_2048_HIGH,
        K_2048_LOW,
        K_2048_HIGH,
        K_2048_LOW,
    );
    while data.len() >= 256 {
        v0 = reduce512(v0, get512(&mut data), k);
        v1 = reduce512(v1, get512(&mut data), k);
        v2 = reduce512(v2, get512(&mut data), k);
        v3 = reduce512(v3, get512(&mut data), k);
    }

    // Collapse the 16 streams, in increasing byte offset, folding each into the next by 128 bits.
    let k3k4 = arch::_mm_set_epi64x(K4, K3);
    let mut x = arch::_mm512_castsi512_si128(v0);
    x = reduce128(x, arch::_mm512_extracti32x4_epi32(v0, 1), k3k4);
    x = reduce128(x, arch::_mm512_extracti32x4_epi32(v0, 2), k3k4);
    x = reduce128(x, arch::_mm512_extracti32x4_epi32(v0, 3), k3k4);
    x = reduce128(x, arch::_mm512_castsi512_si128(v1), k3k4);
    x = reduce128(x, arch::_mm512_extracti32x4_epi32(v1, 1), k3k4);
    x = reduce128(x, arch::_mm512_extracti32x4_epi32(v1, 2), k3k4);
    x = reduce128(x, arch::_mm512_extracti32x4_epi32(v1, 3), k3k4);
    x = reduce128(x, arch::_mm512_castsi512_si128(v2), k3k4);
    x = reduce128(x, arch::_mm512_extracti32x4_epi32(v2, 1), k3k4);
    x = reduce128(x, arch::_mm512_extracti32x4_epi32(v2, 2), k3k4);
    x = reduce128(x, arch::_mm512_extracti32x4_epi32(v2, 3), k3k4);
    x = reduce128(x, arch::_mm512_castsi512_si128(v3), k3k4);
    x = reduce128(x, arch::_mm512_extracti32x4_epi32(v3, 1), k3k4);
    x = reduce128(x, arch::_mm512_extracti32x4_epi32(v3, 2), k3k4);
    x = reduce128(x, arch::_mm512_extracti32x4_epi32(v3, 3), k3k4);

    // A substantial remainder is faster through SSE's complete fold-by-4 kernel than through the
    // serial fold-by-1 tail below. Finalizing and restarting is valid because CRC updates compose.
    if data.len() >= MIN_FOLD_BY_4_BYTES {
        let crc = reduce_128_to_crc(x, &[]);
        return calculate(crc, data);
    }

    reduce_128_to_crc(x, data)
}

/// Folds any remaining 16-byte chunks into `x`, then folds a final partial (`< 16` byte) block
/// with a byte-shift, reduces from 128 to 32 bits with a Barrett reduction, and returns the CRC.
/// Shared by all of the fold implementations.
#[target_feature(
    enable = "pclmulqdq",
    enable = "sse2",
    enable = "sse4.1",
    enable = "ssse3"
)]
unsafe fn reduce_128_to_crc(mut x: arch::__m128i, mut data: &[u8]) -> u32 {
    let k3k4 = arch::_mm_set_epi64x(K4, K3);

    // Fold by 1 over any remaining whole 16-byte blocks.
    while data.len() >= 16 {
        x = reduce128(x, get(&mut data), k3k4);
    }

    // Fold a final partial block of `n` (1..=15) bytes. The last `n` bytes of the accumulator are
    // shifted out (`overflow`) and folded back by 128 bits, while `x` is shifted down to make room
    // for the `n` new bytes, which are byte-aligned into the vacated high lanes. The shuffle masks
    // are built at runtime from the byte length, avoiding a lookup table.
    let n = data.len();
    if n > 0 {
        let seq = arch::_mm_setr_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
        let shl = arch::_mm_add_epi8(seq, arch::_mm_set1_epi8(n as i8 - 16));
        let shr = arch::_mm_xor_si128(shl, arch::_mm_set1_epi8(-128));

        let overflow = arch::_mm_shuffle_epi8(x, shl);
        x = arch::_mm_shuffle_epi8(x, shr);

        let mut part = [0u8; 16];
        part[..n].copy_from_slice(data);
        let part = arch::_mm_loadu_si128(part.as_ptr() as *const arch::__m128i);
        x = arch::_mm_xor_si128(x, arch::_mm_shuffle_epi8(part, shl));

        x = reduce128(overflow, x, k3k4);
    }

    // Perform step 3, reduction from 128 bits to 64 bits. This is
    // significantly different from the paper and basically doesn't follow it
    // at all. It's not really clear why, but implementations of this algorithm
    // in Chrome/Linux diverge in the same way. It is beyond me why this is
    // different than the paper, maybe the paper has like errata or something?
    // Unclear.
    //
    // It's also not clear to me what's actually happening here and/or why, but
    // algebraically what's happening is:
    //
    // x = (x[0:63] • K4) ^ x[64:127]           // 96 bit result
    // x = ((x[0:31] as u64) • K5) ^ x[32:95]   // 64 bit result
    //
    // It's... not clear to me what's going on here. The paper itself is pretty
    // vague on this part but definitely uses different constants at least.
    // It's not clear to me, reading the paper, where the xor operations are
    // happening or why things are shifting around. This implementation...
    // appears to work though!
    let x = arch::_mm_xor_si128(
        arch::_mm_clmulepi64_si128(x, k3k4, 0x10),
        arch::_mm_srli_si128(x, 8),
    );
    let x = arch::_mm_xor_si128(
        arch::_mm_clmulepi64_si128(
            arch::_mm_and_si128(x, arch::_mm_set_epi32(0, 0, 0, !0)),
            arch::_mm_set_epi64x(0, K5),
            0x00,
        ),
        arch::_mm_srli_si128(x, 4),
    );

    // Perform a Barrett reduction from our now 64 bits to 32 bits. The
    // algorithm for this is described at the end of the paper, and note that
    // this also implements the "bit reflected input" variant.
    let pu = arch::_mm_set_epi64x(U_PRIME, P_X);

    // T1(x) = ⌊(R(x) % x^32)⌋ • μ
    let t1 = arch::_mm_clmulepi64_si128(
        arch::_mm_and_si128(x, arch::_mm_set_epi32(0, 0, 0, !0)),
        pu,
        0x10,
    );
    // T2(x) = ⌊(T1(x) % x^32)⌋ • P(x)
    let t2 = arch::_mm_clmulepi64_si128(
        arch::_mm_and_si128(t1, arch::_mm_set_epi32(0, 0, 0, !0)),
        pu,
        0x00,
    );
    // We're doing the bit-reflected variant, so get the upper 32-bits of the
    // 64-bit result instead of the lower 32-bits.
    //
    // C(x) = R(x) ^ T2(x) / x^32
    !(arch::_mm_extract_epi32(arch::_mm_xor_si128(x, t2), 1) as u32)
}

#[inline]
unsafe fn reduce128(a: arch::__m128i, b: arch::__m128i, keys: arch::__m128i) -> arch::__m128i {
    let t1 = arch::_mm_clmulepi64_si128(a, keys, 0x00);
    let t2 = arch::_mm_clmulepi64_si128(a, keys, 0x11);
    arch::_mm_xor_si128(arch::_mm_xor_si128(b, t1), t2)
}

#[cfg(stable_vpclmulqdq)]
#[allow(clippy::incompatible_msrv)] // intrinsics are gated to rustc >= 1.89 by build.rs
#[target_feature(enable = "avx2", enable = "vpclmulqdq")]
#[inline]
unsafe fn reduce256(a: arch::__m256i, b: arch::__m256i, keys: arch::__m256i) -> arch::__m256i {
    let t1 = arch::_mm256_clmulepi64_epi128(a, keys, 0x00);
    let t2 = arch::_mm256_clmulepi64_epi128(a, keys, 0x11);
    arch::_mm256_xor_si256(arch::_mm256_xor_si256(b, t1), t2)
}

#[cfg(stable_vpclmulqdq)]
#[allow(clippy::incompatible_msrv)] // intrinsics are gated to rustc >= 1.89 by build.rs
#[target_feature(enable = "avx512f", enable = "vpclmulqdq")]
#[inline]
unsafe fn reduce512(a: arch::__m512i, b: arch::__m512i, keys: arch::__m512i) -> arch::__m512i {
    let t1 = arch::_mm512_clmulepi64_epi128(a, keys, 0x00);
    let t2 = arch::_mm512_clmulepi64_epi128(a, keys, 0x11);
    arch::_mm512_xor_si512(arch::_mm512_xor_si512(b, t1), t2)
}

unsafe fn get(a: &mut &[u8]) -> arch::__m128i {
    debug_assert!(a.len() >= 16);
    let r = arch::_mm_loadu_si128(a.as_ptr() as *const arch::__m128i);
    *a = &a[16..];
    r
}

#[cfg(stable_vpclmulqdq)]
#[target_feature(enable = "avx")]
#[inline]
unsafe fn get256(a: &mut &[u8]) -> arch::__m256i {
    debug_assert!(a.len() >= 32);
    let r = arch::_mm256_loadu_si256(a.as_ptr() as *const arch::__m256i);
    *a = &a[32..];
    r
}

#[cfg(stable_vpclmulqdq)]
#[allow(clippy::incompatible_msrv)] // intrinsics are gated to rustc >= 1.89 by build.rs
#[target_feature(enable = "avx512f")]
#[inline]
unsafe fn get512(a: &mut &[u8]) -> arch::__m512i {
    debug_assert!(a.len() >= 64);
    let r = arch::_mm512_loadu_si512(a.as_ptr() as *const _);
    *a = &a[64..];
    r
}

#[cfg(test)]
mod test {
    quickcheck::quickcheck! {
        fn check_against_baseline(init: u32, chunks: Vec<(Vec<u8>, usize)>) -> bool {
            let mut baseline = super::super::super::baseline::State::new(init);
            let mut pclmulqdq = super::State::new(init).expect("not supported");
            for (chunk, mut offset) in chunks {
                // simulate random alignments by offsetting the slice by up to 15 bytes
                offset &= 0xF;
                if chunk.len() <= offset {
                    baseline.update(&chunk);
                    pclmulqdq.update(&chunk);
                } else {
                    baseline.update(&chunk[offset..]);
                    pclmulqdq.update(&chunk[offset..]);
                }
            }
            pclmulqdq.finalize() == baseline.finalize()
        }
    }

    // Exercises the wide fold paths across the sizes and alignments where the strategy switches.
    #[test]
    fn check_large_inputs_against_baseline() {
        let mut data = vec![0u8; 8200];
        let mut s: u32 = 0x1234_5678;
        for b in data.iter_mut() {
            s = s.wrapping_mul(1_664_525).wrapping_add(1_013_904_223);
            *b = (s >> 24) as u8;
        }

        for &len in &[
            0usize, 1, 15, 16, 17, 63, 64, 127, 128, 129, 255, 256, 257, 511, 512, 513, 1023, 1024,
            1025, 2047, 2048, 2049, 2175, 2176, 2303, 2304, 4096, 8192, 8199,
        ] {
            for &offset in &[0usize, 1, 3, 7, 8, 15] {
                if offset + len > data.len() {
                    continue;
                }
                let slice = &data[offset..offset + len];
                let mut baseline = super::super::super::baseline::State::new(0);
                baseline.update(slice);
                let mut specialized = super::State::new(0).expect("not supported");
                specialized.update(slice);
                assert_eq!(
                    specialized.finalize(),
                    baseline.finalize(),
                    "mismatch for len={len} offset={offset}",
                );
            }
        }
    }
}

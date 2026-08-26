//! Exhaustive differential correctness tests for the SIMD-accelerated implementations.
//!
//! These compare the public `Hasher` (which selects the best implementation the running CPU
//! supports) against an independent, table-free, bitwise CRC-32 reference across every length that
//! crosses an implementation threshold, at several alignments, with several initial states, and
//! for randomized streaming (multi-`update`) splits.

use crc32fast::Hasher;

/// Independent, bitwise CRC-32/ISO-HDLC reference. `state` uses the same convention as `Hasher`
/// (the finalized CRC so far; 0 for a fresh hash).
fn crc32_ref(state: u32, data: &[u8]) -> u32 {
    const POLY: u32 = 0xEDB8_8320;
    let mut c = !state;
    for &byte in data {
        c ^= byte as u32;
        for _ in 0..8 {
            let mask = (c & 1).wrapping_neg();
            c = (c >> 1) ^ (POLY & mask);
        }
    }
    !c
}

/// Deterministic, well-mixed test buffer (independent of the reference/impl).
fn make_buf(len: usize) -> Vec<u8> {
    let mut v = vec![0u8; len];
    let mut s: u64 = 0x9E37_79B9_7F4A_7C15;
    for b in v.iter_mut() {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        *b = (s >> 33) as u8;
    }
    v
}

#[test]
fn reference_matches_known_vectors() {
    // Anchors the independent reference to the canonical CRC-32 check values.
    assert_eq!(crc32_ref(0, b"123456789"), 0xCBF4_3926);
    assert_eq!(crc32_ref(0, b""), 0x0000_0000);
    assert_eq!(
        crc32_ref(0, b"The quick brown fox jumps over the lazy dog"),
        0x414F_A339
    );
    // And that hash() agrees with it, i.e. the same convention.
    assert_eq!(crc32fast::hash(b"123456789"), 0xCBF4_3926);
}

#[test]
fn exhaustive_lengths_alignments_inits() {
    // Base buffer big enough for the largest length plus the largest alignment offset.
    let max_len = 1100usize;
    let base = make_buf(max_len + 64);

    let inits = [0u32, 0x1234_5678, 0xFFFF_FFFF, 0x0000_0001];
    let aligns = [0usize, 1, 2, 3, 7, 8, 15, 16, 31];

    for &align in &aligns {
        for len in 0..=max_len {
            let data = &base[align..align + len];
            for &init in &inits {
                let mut h = Hasher::new_with_initial(init);
                h.update(data);
                assert_eq!(
                    h.finalize(),
                    crc32_ref(init, data),
                    "single update mismatch: len={len} align={align} init={init:#010x}"
                );
            }
        }
    }
}

#[test]
fn large_sizes() {
    for &len in &[
        2047usize,
        2048,
        2049,
        2175,
        2176,
        2303,
        2304,
        4096,
        4097,
        8192,
        16 * 1024,
        16 * 1024 + 123,
        64 * 1024,
        1024 * 1024 + 7,
    ] {
        let data = make_buf(len);
        for &init in &[0u32, 0xDEAD_BEEF] {
            let mut h = Hasher::new_with_initial(init);
            h.update(&data);
            assert_eq!(
                h.finalize(),
                crc32_ref(init, &data),
                "large mismatch: len={len} init={init:#010x}"
            );
        }
    }
}

#[test]
fn streaming_random_splits() {
    // Verifies state continuity across many update() calls, with split points that frequently land
    // both below and above the SIMD thresholds.
    let data = make_buf(9000);
    let mut rng: u64 = 0xD1B5_4A32_D192_ED03;
    let mut next = |bound: usize| -> usize {
        rng ^= rng << 13;
        rng ^= rng >> 7;
        rng ^= rng << 17;
        (rng as usize) % bound
    };

    for _ in 0..2000 {
        let init = next(u32::MAX as usize) as u32;
        let mut pos = 0usize;
        let mut h = Hasher::new_with_initial(init);
        while pos < data.len() {
            let remaining = data.len() - pos;
            let chunk = if next(4) == 0 {
                1 + next(remaining.min(300))
            } else {
                1 + next(remaining.min(600))
            };
            let end = (pos + chunk).min(data.len());
            h.update(&data[pos..end]);
            pos = end;
        }
        assert_eq!(
            h.finalize(),
            crc32_ref(init, &data),
            "streaming mismatch: init={init:#010x}"
        );
    }
}

#[test]
fn combine_matches_reference() {
    // Cross-checks Hasher::combine against a straight single-pass reference over the concatenation.
    let a = make_buf(3000);
    let b = make_buf(2500);
    // b uses the same generator, so perturb it to be distinct.
    let b: Vec<u8> = b.iter().map(|x| x ^ 0xA5).collect();

    let mut ha = Hasher::new();
    ha.update(&a);
    let mut hb = Hasher::new();
    hb.update(&b);
    ha.combine(&hb);

    let mut concat = a.clone();
    concat.extend_from_slice(&b);
    assert_eq!(ha.finalize(), crc32_ref(0, &concat));
}

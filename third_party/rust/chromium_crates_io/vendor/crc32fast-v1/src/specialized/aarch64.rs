use core::arch::aarch64 as arch;

#[derive(Clone)]
pub struct State(());

impl State {
    #[cfg(not(feature = "std"))]
    pub fn new() -> Option<Self> {
        if cfg!(target_feature = "crc") {
            // SAFETY: The conditions above ensure that all
            //         required instructions are supported by the CPU.
            Some(Self(()))
        } else {
            None
        }
    }

    #[cfg(feature = "std")]
    pub fn new() -> Option<Self> {
        if std::arch::is_aarch64_feature_detected!("crc") {
            // SAFETY: The condition above ensures that all
            //         required instructions are supported by the CPU.
            Some(Self(()))
        } else {
            None
        }
    }

    pub fn update(&self, state: u32, buf: &[u8]) -> u32 {
        // SAFETY: Calling a `#[target_feature]` function is only unsafe because the CPU might lack
        // that feature, and a `State` only exists if `new` found the `crc` feature.
        unsafe { calculate(state, buf) }
    }
}

// Bytes per stream in the 3-way loops. Merging the streams appends zero bytes to the earlier CRC
// states, which for a fixed length is a multiplication by a constant: four lookups in a table built
// at compile time. Shorter blocks would help cores with a slow `crc32x` (Apple M1), but cost a
// little on cores where one chain already keeps up (Neoverse N2), so the 3-way loop starts at
// 3 * SHORT = 1.5 KiB, as before.
const LONG: usize = 32768;
const SHORT: usize = 512;

// The 3-way loop advances each chain four words (32 bytes) at a time, so blocks leave no remainder.
const _: () = assert!(LONG % 32 == 0 && SHORT % 32 == 0);

static LONG_SHIFT: [[u32; 256]; 4] = crate::combine::shift_table(LONG as u64);
static SHORT_SHIFT: [[u32; 256]; 4] = crate::combine::shift_table(SHORT as u64);

/// Reads the `i`-th 8-byte word of `b` as a little-endian `u64`, the byte order `crc32x` expects.
#[inline(always)]
fn quad(b: &[u8], i: usize) -> u64 {
    u64::from_le_bytes(b[i * 8..i * 8 + 8].try_into().unwrap())
}

// target_feature is necessary to allow rustc to inline the crc32* wrappers
#[target_feature(enable = "crc")]
unsafe fn calculate(crc: u32, data: &[u8]) -> u32 {
    // Split the input into unaligned leading bytes, 8-byte-aligned words, and trailing bytes.
    let head = data.as_ptr().align_offset(8).min(data.len());
    let (pre_quad, rest) = data.split_at(head);
    let (mut quads, post_quad) = rest.split_at(rest.len() / 8 * 8);

    // Unaligned leading bytes, then switch to the internal (bit-inverted) state.
    let mut c0 = !crc_bytes(crc, pre_quad);

    // A single `crc32d` chain is latency-bound: each instruction depends on the previous result.
    // While enough words remain, advance three independent chains over three consecutive blocks of
    // `n` bytes. B and C start from a zero state, so appending them to A is linear: shift A's state
    // past B's bytes and XOR in B's state, then the same for C.
    for &(n, shift) in &[(LONG, &LONG_SHIFT), (SHORT, &SHORT_SHIFT)] {
        while quads.len() >= 3 * n {
            let (a, rest) = quads.split_at(n);
            let (b, rest) = rest.split_at(n);
            let (c, rest) = rest.split_at(n);
            quads = rest;

            let mut c1 = 0;
            let mut c2 = 0;
            for ((qa, qb), qc) in a
                .chunks_exact(32)
                .zip(b.chunks_exact(32))
                .zip(c.chunks_exact(32))
            {
                for k in 0..4 {
                    c0 = arch::__crc32d(c0, quad(qa, k));
                    c1 = arch::__crc32d(c1, quad(qb, k));
                    c2 = arch::__crc32d(c2, quad(qc, k));
                }
            }
            c0 = crate::combine::shift(shift, c0) ^ c1;
            c0 = crate::combine::shift(shift, c0) ^ c2;
        }
    }

    // Remaining words, then unaligned trailing bytes.
    let r = crc_quads(!c0, quads);
    crc_bytes(r, post_quad)
}

/// CRCs a byte slice whose length is a multiple of 8, a word at a time.
#[target_feature(enable = "crc")]
unsafe fn crc_quads(crc: u32, quads: &[u8]) -> u32 {
    let mut c = !crc;

    // unrolling increases performance by a lot
    let mut quad_iter = quads.chunks_exact(64);
    for chunk in &mut quad_iter {
        c = arch::__crc32d(c, quad(chunk, 0));
        c = arch::__crc32d(c, quad(chunk, 1));
        c = arch::__crc32d(c, quad(chunk, 2));
        c = arch::__crc32d(c, quad(chunk, 3));
        c = arch::__crc32d(c, quad(chunk, 4));
        c = arch::__crc32d(c, quad(chunk, 5));
        c = arch::__crc32d(c, quad(chunk, 6));
        c = arch::__crc32d(c, quad(chunk, 7));
    }
    c = quad_iter
        .remainder()
        .chunks_exact(8)
        .fold(c, |acc, q| arch::__crc32d(acc, quad(q, 0)));

    !c
}

#[target_feature(enable = "crc")]
unsafe fn crc_bytes(crc: u32, bytes: &[u8]) -> u32 {
    let c = bytes.iter().fold(!crc, |acc, &b| arch::__crc32b(acc, b));
    !c
}

#[cfg(test)]
mod test {
    quickcheck::quickcheck! {
        fn check_against_baseline(init: u32, chunks: Vec<(Vec<u8>, usize)>) -> bool {
            let mut baseline = init;
            let mut aarch64 = init;
            let spec = super::State::new().expect("not supported");

            for (chunk, mut offset) in chunks {
                // simulate random alignments by offsetting the slice by up to 15 bytes
                offset &= 0xF;
                if chunk.len() <= offset {
                    baseline = crate::baseline::update_fast_16(baseline, &chunk);
                    aarch64 = spec.update(aarch64, &chunk);
                } else {
                    baseline = crate::baseline::update_fast_16(baseline, &chunk[offset..]);
                    aarch64 = spec.update(aarch64, &chunk[offset..]);
                }
            }
            aarch64 == baseline
        }
    }

    // Exercises the 3-way interleaved path across the sizes and alignments where it switches.
    #[test]
    fn check_large_inputs_against_baseline() {
        let mut data = vec![0u8; 8200];
        let mut s: u32 = 0x1234_5678;
        for byte in data.iter_mut() {
            s = s.wrapping_mul(1_664_525).wrapping_add(1_013_904_223);
            *byte = (s >> 24) as u8;
        }

        for &len in &[
            0usize, 1, 7, 8, 15, 16, 63, 64, 127, 128, 1535, 1536, 1537, 4096, 8192, 8199,
        ] {
            for &offset in &[0usize, 1, 3, 7, 8, 15] {
                if offset + len > data.len() {
                    continue;
                }
                let slice = &data[offset..offset + len];
                let baseline = crate::baseline::update_fast_16(0, slice);
                let spec = super::State::new().expect("not supported");
                let specialized = spec.update(0, slice);

                assert_eq!(
                    specialized, baseline,
                    "mismatch for len={len} offset={offset}",
                );
            }
        }
    }
}

use crate::BytesPerPixel;

// This code path is used on non-x86_64 architectures but we allow dead code
// for the test module to be able to access it.
#[allow(dead_code)]
pub(super) fn filter_paeth(a: u8, b: u8, c: u8) -> u8 {
    // On ARM this algorithm performs much better than the one above adapted from stb,
    // and this is the better-studied algorithm we've always used here,
    // so we default to it on all non-x86 platforms.
    let pa = (i16::from(b) - i16::from(c)).abs();
    let pb = (i16::from(a) - i16::from(c)).abs();
    let pc = ((i16::from(a) - i16::from(c)) + (i16::from(b) - i16::from(c))).abs();

    let mut out = a;
    let mut min = pa;

    if pb < min {
        min = pb;
        out = b;
    }
    if pc < min {
        out = c;
    }

    out
}

pub(super) fn filter_paeth_stbi(a: i16, b: i16, c: i16) -> u8 {
    // Decoding optimizes better with this algorithm than with `filter_paeth`
    //
    // This formulation looks very different from the reference in the PNG spec, but is
    // actually equivalent and has favorable data dependencies and admits straightforward
    // generation of branch-free code, which helps performance significantly.
    //
    // Adapted from public domain PNG implementation:
    // https://github.com/nothings/stb/blob/5c205738c191bcb0abc65c4febfa9bd25ff35234/stb_image.h#L4657-L4668
    let thresh = c * 3 - (a + b);
    let lo = a.min(b);
    let hi = a.max(b);
    let t0 = if hi <= thresh { lo } else { c };
    let t1 = if thresh <= lo { hi } else { t0 };
    t1 as u8
}

pub(super) fn filter_paeth_fpnge(a: u8, b: u8, c: u8) -> u8 {
    // This is an optimized version of the paeth filter from the PNG specification, proposed by
    // Luca Versari for [FPNGE](https://www.lucaversari.it/FJXL_and_FPNGE.pdf). It operates
    // entirely on unsigned 8-bit quantities, making it more conducive to vectorization.
    //
    //     p = a + b - c
    //     pa = |p - a| = |a + b - c - a| = |b - c| = max(b, c) - min(b, c)
    //     pb = |p - b| = |a + b - c - b| = |a - c| = max(a, c) - min(a, c)
    //     pc = |p - c| = |a + b - c - c| = |(b - c) + (a - c)| = ...
    //
    // Further optimizing the calculation of `pc` a bit tricker. However, notice that:
    //
    //        a > c && b > c
    //    ==> (a - c) > 0 && (b - c) > 0
    //    ==> pc > (a - c) && pc > (b - c)
    //    ==> pc > |a - c| && pc > |b - c|
    //    ==> pc > pb && pc > pa
    //
    // Meaning that if `c` is smaller than `a` and `b`, the value of `pc` is irrelevant. Similar
    // reasoning applies if `c` is larger than the other two inputs. Assuming that `c >= b` and
    // `c <= b` or vice versa:
    //
    //     pc = ||b - c| - |a - c|| =  |pa - pb| = max(pa, pb) - min(pa, pb)
    //
    let pa = b.max(c) - c.min(b);
    let pb = a.max(c) - c.min(a);
    let pc = if (a < c) == (c < b) {
        pa.max(pb) - pa.min(pb)
    } else {
        255
    };

    if pa <= pb && pa <= pc {
        a
    } else if pb <= pc {
        b
    } else {
        c
    }
}

#[allow(unreachable_code)]
#[cfg(not(target_arch = "x86_64"))]
pub(super) fn unfilter(tbpp: BytesPerPixel, previous: &[u8], current: &mut [u8]) {
    // Paeth filter pixels:
    // C B D
    // A X
    match tbpp {
        BytesPerPixel::One => {
            let mut a_bpp = [0; 1];
            let mut c_bpp = [0; 1];
            for (chunk, b_bpp) in current.chunks_exact_mut(1).zip(previous.chunks_exact(1)) {
                let new_chunk = [chunk[0].wrapping_add(filter_paeth(a_bpp[0], b_bpp[0], c_bpp[0]))];
                *TryInto::<&mut [u8; 1]>::try_into(chunk).unwrap() = new_chunk;
                a_bpp = new_chunk;
                c_bpp = b_bpp.try_into().unwrap();
            }
        }
        BytesPerPixel::Two => {
            let mut a_bpp = [0; 2];
            let mut c_bpp = [0; 2];
            for (chunk, b_bpp) in current.chunks_exact_mut(2).zip(previous.chunks_exact(2)) {
                let new_chunk = [
                    chunk[0].wrapping_add(filter_paeth(a_bpp[0], b_bpp[0], c_bpp[0])),
                    chunk[1].wrapping_add(filter_paeth(a_bpp[1], b_bpp[1], c_bpp[1])),
                ];
                *TryInto::<&mut [u8; 2]>::try_into(chunk).unwrap() = new_chunk;
                a_bpp = new_chunk;
                c_bpp = b_bpp.try_into().unwrap();
            }
        }
        BytesPerPixel::Three => {
            #[cfg(all(feature = "unstable", not(target_vendor = "apple")))]
            {
                // Results in PR: https://github.com/image-rs/image-png/pull/632
                // Approximately 30% better on Arm Cortex A520, 7%
                // regression on Arm Cortex X4. Switched off on Apple
                // Silicon due to 10-12% regression.
                super::simd::paeth_unfilter_3bpp(current, previous);
                return;
            }
            let mut a_bpp = [0; 3];
            let mut c_bpp = [0; 3];

            let mut previous = &previous[..previous.len() / 3 * 3];
            let current_len = current.len();
            let mut current = &mut current[..current_len / 3 * 3];

            while let ([c0, c1, c2, c_rest @ ..], [p0, p1, p2, p_rest @ ..]) = (current, previous) {
                current = c_rest;
                previous = p_rest;

                *c0 = c0.wrapping_add(filter_paeth(a_bpp[0], *p0, c_bpp[0]));
                *c1 = c1.wrapping_add(filter_paeth(a_bpp[1], *p1, c_bpp[1]));
                *c2 = c2.wrapping_add(filter_paeth(a_bpp[2], *p2, c_bpp[2]));

                a_bpp = [*c0, *c1, *c2];
                c_bpp = [*p0, *p1, *p2];
            }
        }
        BytesPerPixel::Four => {
            #[cfg(feature = "unstable")]
            {
                // Results in PR: https://github.com/image-rs/image-png/pull/633
                // No change on Apple Silicon, 42% better on Arm Cortex A520,
                // 10% better on Arm Cortex X4.
                super::simd::paeth_unfilter_4bpp(current, previous);
                return;
            }

            let mut a_bpp = [0; 4];
            let mut c_bpp = [0; 4];

            let mut previous = &previous[..previous.len() & !3];
            let current_len = current.len();
            let mut current = &mut current[..current_len & !3];

            while let ([c0, c1, c2, c3, c_rest @ ..], [p0, p1, p2, p3, p_rest @ ..]) =
                (current, previous)
            {
                current = c_rest;
                previous = p_rest;

                *c0 = c0.wrapping_add(filter_paeth(a_bpp[0], *p0, c_bpp[0]));
                *c1 = c1.wrapping_add(filter_paeth(a_bpp[1], *p1, c_bpp[1]));
                *c2 = c2.wrapping_add(filter_paeth(a_bpp[2], *p2, c_bpp[2]));
                *c3 = c3.wrapping_add(filter_paeth(a_bpp[3], *p3, c_bpp[3]));

                a_bpp = [*c0, *c1, *c2, *c3];
                c_bpp = [*p0, *p1, *p2, *p3];
            }
        }
        BytesPerPixel::Six => {
            let mut a_bpp = [0; 6];
            let mut c_bpp = [0; 6];
            for (chunk, b_bpp) in current.chunks_exact_mut(6).zip(previous.chunks_exact(6)) {
                let new_chunk = [
                    chunk[0].wrapping_add(filter_paeth(a_bpp[0], b_bpp[0], c_bpp[0])),
                    chunk[1].wrapping_add(filter_paeth(a_bpp[1], b_bpp[1], c_bpp[1])),
                    chunk[2].wrapping_add(filter_paeth(a_bpp[2], b_bpp[2], c_bpp[2])),
                    chunk[3].wrapping_add(filter_paeth(a_bpp[3], b_bpp[3], c_bpp[3])),
                    chunk[4].wrapping_add(filter_paeth(a_bpp[4], b_bpp[4], c_bpp[4])),
                    chunk[5].wrapping_add(filter_paeth(a_bpp[5], b_bpp[5], c_bpp[5])),
                ];
                *TryInto::<&mut [u8; 6]>::try_into(chunk).unwrap() = new_chunk;
                a_bpp = new_chunk;
                c_bpp = b_bpp.try_into().unwrap();
            }
        }
        BytesPerPixel::Eight => {
            let mut a_bpp = [0; 8];
            let mut c_bpp = [0; 8];
            for (chunk, b_bpp) in current.chunks_exact_mut(8).zip(previous.chunks_exact(8)) {
                let new_chunk = [
                    chunk[0].wrapping_add(filter_paeth(a_bpp[0], b_bpp[0], c_bpp[0])),
                    chunk[1].wrapping_add(filter_paeth(a_bpp[1], b_bpp[1], c_bpp[1])),
                    chunk[2].wrapping_add(filter_paeth(a_bpp[2], b_bpp[2], c_bpp[2])),
                    chunk[3].wrapping_add(filter_paeth(a_bpp[3], b_bpp[3], c_bpp[3])),
                    chunk[4].wrapping_add(filter_paeth(a_bpp[4], b_bpp[4], c_bpp[4])),
                    chunk[5].wrapping_add(filter_paeth(a_bpp[5], b_bpp[5], c_bpp[5])),
                    chunk[6].wrapping_add(filter_paeth(a_bpp[6], b_bpp[6], c_bpp[6])),
                    chunk[7].wrapping_add(filter_paeth(a_bpp[7], b_bpp[7], c_bpp[7])),
                ];
                *TryInto::<&mut [u8; 8]>::try_into(chunk).unwrap() = new_chunk;
                a_bpp = new_chunk;
                c_bpp = b_bpp.try_into().unwrap();
            }
        }
    }
}

/// The x86_64 functions avoid casting between u8xN and i16xN SIMD
/// representations when possible by maintaining [i16; BPP] arrays
/// between iterations instead of [u8; BPP].
#[allow(unreachable_code)]
#[cfg(target_arch = "x86_64")]
pub(super) fn unfilter(tbpp: BytesPerPixel, previous: &[u8], current: &mut [u8]) {
    // Paeth filter pixels:
    // C B D
    // A X
    match tbpp {
        BytesPerPixel::One => {
            const BPP: usize = 1;
            let mut a_bpp = [0; BPP];
            let mut c_bpp = [0; BPP];

            for (c, p) in current
                .chunks_exact_mut(BPP)
                .zip(previous.chunks_exact(BPP))
            {
                for i in 0..BPP {
                    c[i] = c[i].wrapping_add(filter_paeth_stbi(a_bpp[i], p[i] as i16, c_bpp[i]));
                }

                a_bpp = [c[0] as i16];
                c_bpp = [p[0] as i16];
            }
        }
        BytesPerPixel::Two => {
            const BPP: usize = 2;
            let mut a_bpp = [0; BPP];
            let mut c_bpp = [0; BPP];

            for (c, p) in current
                .chunks_exact_mut(BPP)
                .zip(previous.chunks_exact(BPP))
            {
                for i in 0..BPP {
                    c[i] = c[i].wrapping_add(filter_paeth_stbi(a_bpp[i], p[i] as i16, c_bpp[i]));
                }

                a_bpp = [c[0] as i16, c[1] as i16];
                c_bpp = [p[0] as i16, p[1] as i16];
            }
        }
        BytesPerPixel::Three => {
            #[cfg(feature = "unstable")]
            {
                // Results in PR: https://github.com/image-rs/image-png/pull/632
                // 23% better on an Epyc 7B13, 10% on a Zen 3 part.
                // ~30% when targeting x86-64-v2.
                super::simd::paeth_unfilter_3bpp(current, previous);
                return;
            }
            const BPP: usize = 3;
            let mut a_bpp = [0; BPP];
            let mut c_bpp = [0; BPP];

            for (c, p) in current
                .chunks_exact_mut(BPP)
                .zip(previous.chunks_exact(BPP))
            {
                for i in 0..BPP {
                    c[i] = c[i].wrapping_add(filter_paeth_stbi(a_bpp[i], p[i] as i16, c_bpp[i]));
                }

                a_bpp = [c[0] as i16, c[1] as i16, c[2] as i16];
                c_bpp = [p[0] as i16, p[1] as i16, p[2] as i16];
            }
        }
        BytesPerPixel::Four => {
            #[cfg(feature = "unstable")]
            {
                // Results in PR: https://github.com/image-rs/image-png/pull/633
                // May be slightly faster on AMD EPYC 7B13.
                super::simd::paeth_unfilter_4bpp(current, previous);
                return;
            }
            const BPP: usize = 4;
            let mut a_bpp = [0; BPP];
            let mut c_bpp = [0; BPP];

            for (c, p) in current
                .chunks_exact_mut(BPP)
                .zip(previous.chunks_exact(BPP))
            {
                for i in 0..BPP {
                    c[i] = c[i].wrapping_add(filter_paeth_stbi(a_bpp[i], p[i] as i16, c_bpp[i]));
                }

                a_bpp = [c[0] as i16, c[1] as i16, c[2] as i16, c[3] as i16];
                c_bpp = [p[0] as i16, p[1] as i16, p[2] as i16, p[3] as i16];
            }
        }
        BytesPerPixel::Six => {
            const BPP: usize = 6;
            let mut a_bpp = [0; BPP];
            let mut c_bpp = [0; BPP];

            for (c, p) in current
                .chunks_exact_mut(BPP)
                .zip(previous.chunks_exact(BPP))
            {
                for i in 0..BPP {
                    c[i] = c[i].wrapping_add(filter_paeth_stbi(a_bpp[i], p[i] as i16, c_bpp[i]));
                }

                a_bpp = [
                    c[0] as i16,
                    c[1] as i16,
                    c[2] as i16,
                    c[3] as i16,
                    c[4] as i16,
                    c[5] as i16,
                ];
                c_bpp = [
                    p[0] as i16,
                    p[1] as i16,
                    p[2] as i16,
                    p[3] as i16,
                    p[4] as i16,
                    p[5] as i16,
                ];
            }
        }
        BytesPerPixel::Eight => {
            const BPP: usize = 8;
            let mut a_bpp = [0; BPP];
            let mut c_bpp = [0; BPP];

            for (c, p) in current
                .chunks_exact_mut(BPP)
                .zip(previous.chunks_exact(BPP))
            {
                for i in 0..BPP {
                    c[i] = c[i].wrapping_add(filter_paeth_stbi(a_bpp[i], p[i] as i16, c_bpp[i]));
                }

                a_bpp = [
                    c[0] as i16,
                    c[1] as i16,
                    c[2] as i16,
                    c[3] as i16,
                    c[4] as i16,
                    c[5] as i16,
                    c[6] as i16,
                    c[7] as i16,
                ];
                c_bpp = [
                    p[0] as i16,
                    p[1] as i16,
                    p[2] as i16,
                    p[3] as i16,
                    p[4] as i16,
                    p[5] as i16,
                    p[6] as i16,
                    p[7] as i16,
                ];
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    #[ignore] // takes ~20s without optimizations
    fn paeth_impls_are_equivalent() {
        for a in 0..=255 {
            for b in 0..=255 {
                for c in 0..=255 {
                    let baseline = filter_paeth(a, b, c);
                    let fpnge = filter_paeth_fpnge(a, b, c);
                    let stbi = filter_paeth_stbi(a as i16, b as i16, c as i16);

                    assert_eq!(baseline, fpnge);
                    assert_eq!(baseline, stbi);
                }
            }
        }
    }
}

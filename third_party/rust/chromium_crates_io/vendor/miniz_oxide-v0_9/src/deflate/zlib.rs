use crate::deflate::core::deflate_flags::{TDEFL_GREEDY_PARSING_FLAG, TDEFL_RLE_MATCHES};

const DEFAULT_CM: u8 = 8;
const _DEFAULT_CINFO: u8 = 7 << 4;
const _DEFAULT_FDICT: u8 = 0;
const _DEFAULT_CMF: u8 = DEFAULT_CM | _DEFAULT_CINFO;
// CMF used for RLE (technically it uses a window size of 0 but the lowest that
// can be specified in the header corresponds to a window size of 1 << (0 + 8)
// aka 256.
const _MIN_CMF: u8 = DEFAULT_CM; // | 0
/// The 16-bit value consisting of CMF and FLG must be divisible by this to be
/// valid.
const FCHECK_DIVISOR: u8 = 31;

/// Generate FCHECK from CMF and FLG (without FCKECH )so that they are correct
/// according to the specification, i.e (CMF*256 + FCHK) % 31 = 0.
/// Returns flg with the FCHKECK bits added (any existing FCHECK bits are
/// ignored).
#[inline]
fn add_fcheck(cmf: u8, flg: u8) -> u8 {
    let rem = ((usize::from(cmf) * 256) + usize::from(flg)) % usize::from(FCHECK_DIVISOR);

    // Clear existing FCHECK if any
    let flg = flg & 0b11100000;

    // Casting is safe as rem can't overflow since it is a value mod 31
    // We can simply add the value to flg as (31 - rem) will never be above 2^5
    flg + (FCHECK_DIVISOR - rem as u8)
}

#[inline]
const fn zlib_level_from_flags(flags: u32) -> u8 {
    use crate::deflate::core::NUM_PROBES;

    let num_probes = flags & super::MAX_PROBES_MASK;
    if (flags & TDEFL_GREEDY_PARSING_FLAG != 0) || (flags & TDEFL_RLE_MATCHES != 0) {
        if num_probes <= 1 {
            0
        } else {
            1
        }
    } else if num_probes >= NUM_PROBES[9] as u32 {
        3
    } else {
        2
    }
}

/// Get the zlib header for the level using the default window size and no
/// dictionary.
#[inline]
fn header_from_level(level: u8, window_bits: u8) -> [u8; 2] {
    // bits 0 to 3 compression method (always 8)
    // bits 4 to 7, log 2 of window size - 7
    let cmf = DEFAULT_CM | (window_bits.saturating_sub(8) << 4);
    [cmf, add_fcheck(cmf, level << 6)]
}

/// Create a zlib header from the given compression flags.
/// Only level is considered.
#[inline]
pub fn header_from_flags(flags: u32, window_bits: u8) -> [u8; 2] {
    let level = zlib_level_from_flags(flags);
    header_from_level(level, window_bits)
}

#[cfg(test)]
mod test {
    use crate::shared::MZ_DEFAULT_WINDOW_BITS;
    #[test]
    fn zlib() {
        use super::super::*;
        use super::*;

        let test_level = |level, expected| {
            let flags = create_comp_flags_from_zip_params(
                level,
                MZ_DEFAULT_WINDOW_BITS,
                CompressionStrategy::Default as i32,
            );
            assert_eq!(zlib_level_from_flags(flags), expected);
        };

        assert_eq!(zlib_level_from_flags(DEFAULT_FLAGS), 2);
        test_level(0, 0);
        test_level(1, 0);
        test_level(2, 1);
        test_level(3, 1);
        for i in 4..=8 {
            test_level(i, 2)
        }
        test_level(9, 3);
        test_level(10, 3);
    }

    #[test]
    fn test_header() {
        let header = super::header_from_level(3, 8);
        assert_eq!(((usize::from(header[0]) * 256) + usize::from(header[1])) % 31, 0);
    }
}

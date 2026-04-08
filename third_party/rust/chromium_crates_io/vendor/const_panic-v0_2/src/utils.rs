//! Utility functions

use crate::debug_str_fmt::ForEscaping;

#[cfg(feature = "rust_1_64")]
#[cfg(test)]
mod utils_1_64_tests;

#[cfg(feature = "non_basic")]
mod non_basic_utils;

#[cfg(feature = "non_basic")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
pub use self::non_basic_utils::*;

/// Computes the minimum of `l` and `r`
pub const fn min_usize(l: usize, r: usize) -> usize {
    if l < r {
        l
    } else {
        r
    }
}

/// Computes the maximum of `l` and `r`
pub const fn max_usize(l: usize, r: usize) -> usize {
    if l > r {
        l
    } else {
        r
    }
}

#[derive(Copy, Clone)]
pub(crate) struct TailShortString<const LEN: usize> {
    start: u8,
    buffer: [u8; LEN],
}

pub(crate) type PreFmtString = TailShortString<{ string_cap::PREFMT }>;

pub(crate) mod string_cap {
    /// The capacity of a [`ShortString`](crate::fmt::ShortString).
    #[cfg(feature = "non_basic")]
    pub const TINY: usize = 16;

    // the TailShortString that's stored in PanicVal
    pub(crate) const PREFMT: usize = 21;

    // length of string to alternate binary format a 64 bit integer
    pub(crate) const MEDIUM: usize = 66;

    // length of string to alternate binary format a 128 bit integer
    pub(crate) const LARGE: usize = 130;
}

impl<const LEN: usize> TailShortString<LEN> {
    ///
    /// # Safety
    ///
    /// `buffer` must be valid utf8 starting from the `start` index.
    #[inline(always)]
    pub(crate) const unsafe fn new(start: u8, buffer: [u8; LEN]) -> Self {
        Self { start, buffer }
    }

    pub(crate) const fn len(&self) -> usize {
        LEN - self.start as usize
    }

    pub(crate) const fn ranged(&self) -> RangedBytes<&[u8]> {
        RangedBytes {
            start: self.start as usize,
            end: LEN,
            bytes: &self.buffer,
        }
    }
}

////////////////////////////////////////////////////////

#[repr(packed)]
#[derive(Copy)]
pub(crate) struct Packed<T>(pub(crate) T);

impl<T: Copy> Clone for Packed<T> {
    fn clone(&self) -> Self {
        *self
    }
}

////////////////////////////////////////////////////////

#[derive(Copy, Clone)]
pub(crate) struct RangedBytes<B> {
    pub(crate) start: usize,
    pub(crate) end: usize,
    pub(crate) bytes: B,
}

impl<B> RangedBytes<B> {
    pub(crate) const fn len(&self) -> usize {
        self.end - self.start
    }
}
impl RangedBytes<&'static [u8]> {
    pub const EMPTY: Self = RangedBytes {
        start: 0,
        end: 0,
        bytes: &[],
    };
}

////////////////////////////////////////////////////////

#[derive(Copy, Clone)]
pub(crate) enum Sign {
    Positive,
    Negative = 1,
}

#[derive(Copy, Clone)]
pub(crate) enum WasTruncated {
    Yes(usize),
    No,
}

impl WasTruncated {
    pub(crate) const fn get_length(self, len: usize) -> usize {
        match self {
            WasTruncated::Yes(x) => x,
            WasTruncated::No => len,
        }
    }
}

const fn is_char_boundary(b: u8) -> bool {
    (b as i8) >= -0x40
}

// truncates a utf8-encoded string to the character before the `truncate_to` index
//
pub(crate) const fn truncated_str_len(
    ranged: RangedBytes<&[u8]>,
    truncate_to: usize,
) -> WasTruncated {
    if ranged.len() <= truncate_to {
        WasTruncated::No
    } else {
        let mut i = ranged.start + truncate_to;
        while i != ranged.start {
            // if it's a non-continuation byte, break
            if is_char_boundary(ranged.bytes[i]) {
                break;
            }
            i -= 1;
        }

        WasTruncated::Yes(i - ranged.start)
    }
}

pub(crate) const fn truncated_debug_str_len(
    ranged: RangedBytes<&[u8]>,
    truncate_to: usize,
) -> WasTruncated {
    let blen = ranged.end;

    // `* 4` because the longest escape is written like `\xNN` which is 4 bytes
    // `+ 2` for the quote characters
    if blen * 4 + 2 <= truncate_to {
        WasTruncated::No
    } else if truncate_to == 0 {
        WasTruncated::Yes(0)
    } else {
        let mut i = ranged.start;
        // = 1 for opening quote char
        let mut fmtlen = 1;
        loop {
            let next_i = next_char_boundary(ranged, min_usize(i + 1, ranged.end));

            let mut j = i;
            while j < next_i {
                fmtlen += ForEscaping::byte_len(ranged.bytes[j]);
                j += 1;
            }

            if fmtlen > truncate_to {
                break;
            } else if next_i == ranged.end {
                i = next_i;
                break;
            } else {
                i = next_i;
            }
        }

        if i == blen && fmtlen < truncate_to {
            WasTruncated::No
        } else {
            WasTruncated::Yes(i - ranged.start)
        }
    }
}

const fn next_char_boundary(ranged: RangedBytes<&[u8]>, mut i: usize) -> usize {
    while i < ranged.end && !is_char_boundary(ranged.bytes[i]) {
        i += 1;
    }
    i
}

#[cfg_attr(feature = "test", derive(Debug, PartialEq))]
pub(crate) struct StartAndBytes<const LEN: usize> {
    pub start: u8,
    pub bytes: [u8; LEN],
}

#[track_caller]
pub(crate) const fn tail_byte_array<const TO: usize>(
    len: usize,
    input: &[u8],
) -> StartAndBytes<TO> {
    assert!(len <= TO);

    let mut bytes = [0u8; TO];

    let start = TO - len;
    let mut i = start;
    let mut j = 0;
    while j < len {
        bytes[i] = input[j];
        i += 1;
        j += 1;
    }

    assert!(start < 256);
    StartAndBytes {
        start: start as u8,
        bytes,
    }
}

////////////////////////////////////////////////////////////////////////////////

/// Const equivalent of `&buffer[..upto]` with saturating indexing.
///
/// "saturating indexing" means that if `upto > buffer.len()`,
/// then this returns all of `buffer` instead of panicking.
///
/// # Example
///
/// ```rust
/// use const_panic::utils::bytes_up_to;
///
/// const BYTES: &[u8] = &[3, 5, 8, 13, 21, 34, 55, 89];
///
/// const SLICE: &[u8] = bytes_up_to(BYTES, 4);
/// assert_eq!(SLICE, &[3, 5, 8, 13][..]);
///
/// const WHOLE: &[u8] = bytes_up_to(BYTES, usize::MAX);
/// assert_eq!(WHOLE, &[3, 5, 8, 13, 21, 34, 55, 89][..]);
///
/// ```
pub const fn bytes_up_to(buffer: &[u8], upto: usize) -> &[u8] {
    if upto > buffer.len() {
        return buffer;
    }

    #[cfg(not(feature = "rust_1_64"))]
    {
        let mut to_truncate = buffer.len() - upto;
        let mut out: &[u8] = buffer;

        while to_truncate != 0 {
            if let [rem @ .., _] = out {
                out = rem;
            }
            to_truncate -= 1;
        }

        if out.len() != upto {
            panic!("BUG!")
        }

        out
    }

    #[cfg(feature = "rust_1_64")]
    {
        // SAFETY: the above conditional ensures that `upto` doesn't
        // create a partially-dangling slice
        unsafe { core::slice::from_raw_parts(buffer.as_ptr(), upto) }
    }
}

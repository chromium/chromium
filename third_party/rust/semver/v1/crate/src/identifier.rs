// This module implements Identifier, a short-optimized string allowed to
// contain only the ASCII characters hyphen, dot, 0-9, A-Z, a-z.
//
// As of mid-2021, the distribution of pre-release lengths on crates.io is:
//
//     length  count         length  count         length  count
//        0  355929            11      81            24       2
//        1     208            12      48            25       6
//        2     236            13      55            26      10
//        3    1909            14      25            27       4
//        4    1284            15      15            28       1
//        5    1742            16      35            30       1
//        6    3440            17       9            31       5
//        7    5624            18       6            32       1
//        8    1321            19      12            36       2
//        9     179            20       2            37     379
//       10      65            23      11
//
// and the distribution of build metadata lengths is:
//
//     length  count         length  count         length  count
//        0  364445             8    7725            18       1
//        1      72             9      16            19       1
//        2       7            10      85            20       1
//        3      28            11      17            22       4
//        4       9            12      10            26       1
//        5      68            13       9            27       1
//        6      73            14      10            40       5
//        7      53            15       6
//
// Therefore it really behooves us to be able to use the entire 8 bytes of a
// pointer for inline storage. For both pre-release and build metadata there are
// vastly more strings with length exactly 8 bytes than the sum over all lengths
// longer than 8 bytes.
//
// To differentiate the inline representation from the heap allocated long
// representation, we'll allocate heap pointers with 2-byte alignment so that
// they are guaranteed to have an unset least significant bit. Then in the repr
// we store for pointers, we rotate a 1 into the most significant bit of the
// most significant byte, which is never set for an ASCII byte.
//
// Inline repr:
//
//     0xxxxxxx 0xxxxxxx 0xxxxxxx 0xxxxxxx 0xxxxxxx 0xxxxxxx 0xxxxxxx 0xxxxxxx
//
// Heap allocated repr:
//
//     1ppppppp pppppppp pppppppp pppppppp pppppppp pppppppp pppppppp pppppppp 0
//     ^ most significant bit   least significant bit of orig ptr, rotated out ^
//
// Since the most significant bit doubles as a sign bit for the similarly sized
// signed integer type, the CPU has an efficient instruction for inspecting it,
// meaning we can differentiate between an inline repr and a heap allocated repr
// in one instruction. Effectively an inline repr always looks like a positive
// i64 while a heap allocated repr always looks like a negative i64.
//
// For the inline repr, we store \0 padding on the end of the stored characters,
// and thus the string length is readily determined efficiently by a cttz (count
// trailing zeros) or bsf (bit scan forward) instruction.
//
// For the heap allocated repr, the length is encoded as a base-128 varint at
// the head of the allocation.
//
// Empty strings are stored as an all-1 bit pattern, corresponding to -1i64.
// Consequently the all-0 bit pattern is never a legal representation in any
// repr, leaving it available as a niche for downstream code. For example this
// allows size_of::<Version>() == size_of::<Option<Version>>().

use crate::alloc::alloc::{alloc, dealloc, Layout};
#[cfg(no_from_ne_bytes)]
use crate::backport::FromNeBytes;
use core::mem;
use core::num::{NonZeroU64, NonZeroUsize};
use core::ptr;
use core::slice;
use core::str;

#[repr(transparent)]
pub(crate) struct Identifier {
    repr: NonZeroU64,
}

impl Identifier {
    const EMPTY: NonZeroU64 = unsafe { NonZeroU64::new_unchecked(!0) };

    pub(crate) const fn empty() -> Self {
        // `mov rax, -1`
        Identifier { repr: Self::EMPTY }
    }

    // SAFETY: string must be ASCII and not contain \0 bytes.
    pub(crate) unsafe fn new_unchecked(string: &str) -> Self {
        let len = string.len();
        let repr = match len as u64 {
            0 => Self::EMPTY,
            1..=8 => {
                let mut bytes = [0u8; 8];
                // SAFETY: string is big enough to read len bytes, bytes is big
                // enough to write len bytes, and they do not overlap.
                unsafe { ptr::copy_nonoverlapping(string.as_ptr(), bytes.as_mut_ptr(), len) };
                // SAFETY: it's nonzero because the input string was at least 1
                // byte of ASCII and did not contain \0.
                unsafe { NonZeroU64::new_unchecked(u64::from_ne_bytes(bytes)) }
            }
            9..=0xff_ffff_ffff_ffff => {
                // SAFETY: len is in a range that does not contain 0.
                let size = bytes_for_varint(unsafe { NonZeroUsize::new_unchecked(len) }) + len;
                let align = 2;
                // SAFETY: align is not zero, align is a power of two, and
                // rounding size up to align does not overflow usize::MAX.
                let layout = unsafe { Layout::from_size_align_unchecked(size, align) };
                // SAFETY: layout's size is nonzero.
                let ptr = unsafe { alloc(layout) };
                let mut write = ptr;
                let mut varint_remaining = len;
                while varint_remaining > 0 {
                    // SAFETY: size is bytes_for_varint(len) bytes + len bytes.
                    // This is writing the first bytes_for_varint(len) bytes.
                    unsafe { ptr::write(write, varint_remaining as u8 | 0x80) };
                    varint_remaining >>= 7;
                    // SAFETY: still in bounds of the same allocation.
                    write = unsafe { write.add(1) };
                }
                // SAFETY: size is bytes_for_varint(len) bytes + len bytes. This
                // is writing to the last len bytes.
                unsafe { ptr::copy_nonoverlapping(string.as_ptr(), write, len) };
                ptr_to_repr(ptr)
            }
            0x100_0000_0000_0000..=0xffff_ffff_ffff_ffff => {
                unreachable!("please refrain from storing >64 petabytes of text in semver version");
            }
            #[cfg(no_exhaustive_int_match)] // rustc <1.33
            _ => unreachable!(),
        };
        Identifier { repr }
    }

    pub(crate) fn is_empty(&self) -> bool {
        // `cmp rdi, -1` -- basically: `repr as i64 == -1`
        self.repr == Self::EMPTY
    }

    fn is_inline(&self) -> bool {
        // `test rdi, rdi` -- basically: `repr as i64 >= 0`
        self.repr.get() >> 63 == 0
    }

    fn is_empty_or_inline(&self) -> bool {
        // `cmp rdi, -2` -- basically: `repr as i64 > -2`
        self.is_empty() || self.is_inline()
    }

    pub(crate) fn as_str(&self) -> &str {
        if self.is_empty() {
            ""
        } else if self.is_inline() {
            // SAFETY: repr is in the inline representation.
            unsafe { inline_as_str(&self.repr) }
        } else {
            // SAFETY: repr is in the heap allocated representation.
            unsafe { ptr_as_str(&self.repr) }
        }
    }
}

impl Clone for Identifier {
    fn clone(&self) -> Self {
        let repr = if self.is_empty_or_inline() {
            self.repr
        } else {
            let ptr = repr_to_ptr(self.repr);
            // SAFETY: ptr is one of our own heap allocations.
            let len = unsafe { decode_len(ptr) };
            let size = bytes_for_varint(len) + len.get();
            let align = 2;
            // SAFETY: align is not zero, align is a power of two, and rounding
            // size up to align does not overflow usize::MAX. This is just
            // duplicating a previous allocation where all of these guarantees
            // were already made.
            let layout = unsafe { Layout::from_size_align_unchecked(size, align) };
            // SAFETY: layout's size is nonzero.
            let clone = unsafe { alloc(layout) };
            // SAFETY: new allocation cannot overlap the previous one (this was
            // not a realloc). The argument ptrs are readable/writeable
            // respectively for size bytes.
            unsafe { ptr::copy_nonoverlapping(ptr, clone, size) }
            ptr_to_repr(clone)
        };
        Identifier { repr }
    }
}

impl Drop for Identifier {
    fn drop(&mut self) {
        if self.is_empty_or_inline() {
            return;
        }
        let ptr = repr_to_ptr_mut(self.repr);
        // SAFETY: ptr is one of our own heap allocations.
        let len = unsafe { decode_len(ptr) };
        let size = bytes_for_varint(len) + len.get();
        let align = 2;
        // SAFETY: align is not zero, align is a power of two, and rounding
        // size up to align does not overflow usize::MAX. These guarantees were
        // made when originally allocating this memory.
        let layout = unsafe { Layout::from_size_align_unchecked(size, align) };
        // SAFETY: ptr was previously allocated by the same allocator with the
        // same layout.
        unsafe { dealloc(ptr, layout) }
    }
}

impl PartialEq for Identifier {
    fn eq(&self, rhs: &Self) -> bool {
        if self.is_empty_or_inline() {
            // Fast path (most common)
            self.repr == rhs.repr
        } else if rhs.is_empty_or_inline() {
            false
        } else {
            // SAFETY: both reprs are in the heap allocated representation.
            unsafe { ptr_as_str(&self.repr) == ptr_as_str(&rhs.repr) }
        }
    }
}

// We use heap pointers that are 2-byte aligned, meaning they have an
// insignificant 0 in the least significant bit. We take advantage of that
// unneeded bit to rotate a 1 into the most significant bit to make the repr
// distinguishable from ASCII bytes.
fn ptr_to_repr(ptr: *mut u8) -> NonZeroU64 {
    // `mov eax, 1`
    // `shld rax, rdi, 63`
    let repr = (ptr as u64 | 1).rotate_right(1);

    // SAFETY: the most significant bit of repr is known to be set, so the value
    // is not zero.
    unsafe { NonZeroU64::new_unchecked(repr) }
}

// Shift out the 1 previously placed into the most significant bit of the least
// significant byte. Shift in a low 0 bit to reconstruct the original 2-byte
// aligned pointer.
fn repr_to_ptr(repr: NonZeroU64) -> *const u8 {
    // `lea rax, [rdi + rdi]`
    (repr.get() << 1) as *const u8
}

fn repr_to_ptr_mut(repr: NonZeroU64) -> *mut u8 {
    repr_to_ptr(repr) as *mut u8
}

// Compute the length of the inline string, assuming the argument is in short
// string representation. Short strings are stored as 1 to 8 nonzero ASCII
// bytes, followed by \0 padding for the remaining bytes.
fn inline_len(repr: NonZeroU64) -> NonZeroUsize {
    // Rustc >=1.53 has intrinsics for counting zeros on a non-zeroable integer.
    // On many architectures these are more efficient than counting on ordinary
    // zeroable integers (bsf vs cttz). On rustc <1.53 without those intrinsics,
    // we count zeros in the u64 rather than the NonZeroU64.
    #[cfg(no_nonzero_bitscan)]
    let repr = repr.get();

    #[cfg(target_endian = "little")]
    let zero_bits_on_string_end = repr.leading_zeros();
    #[cfg(target_endian = "big")]
    let zero_bits_on_string_end = repr.trailing_zeros();

    let nonzero_bytes = 8 - zero_bits_on_string_end as usize / 8;

    // SAFETY: repr is nonzero, so it has at most 63 zero bits on either end,
    // thus at least one nonzero byte.
    unsafe { NonZeroUsize::new_unchecked(nonzero_bytes) }
}

// SAFETY: repr must be in the inline representation, i.e. at least 1 and at
// most 8 nonzero ASCII bytes padded on the end with \0 bytes.
unsafe fn inline_as_str(repr: &NonZeroU64) -> &str {
    let ptr = repr as *const NonZeroU64 as *const u8;
    let len = inline_len(*repr).get();
    // SAFETY: we are viewing the nonzero ASCII prefix of the inline repr's
    // contents as a slice of bytes. Input/output lifetimes are correctly
    // associated.
    let slice = unsafe { slice::from_raw_parts(ptr, len) };
    // SAFETY: the string contents are known to be only ASCII bytes, which are
    // always valid UTF-8.
    unsafe { str::from_utf8_unchecked(slice) }
}

// Decode varint. Varints consist of between one and eight base-128 digits, each
// of which is stored in a byte with most significant bit set. Adjacent to the
// varint in memory there is guaranteed to be at least 9 ASCII bytes, each of
// which has an unset most significant bit.
//
// SAFETY: ptr must be one of our own heap allocations, with the varint header
// already written.
unsafe fn decode_len(ptr: *const u8) -> NonZeroUsize {
    // SAFETY: There is at least one byte of varint followed by at least 9 bytes
    // of string content, which is at least 10 bytes total for the allocation,
    // so reading the first two is no problem.
    let [first, second] = unsafe { ptr::read(ptr as *const [u8; 2]) };
    if second < 0x80 {
        // SAFETY: the length of this heap allocated string has been encoded as
        // one base-128 digit, so the length is at least 9 and at most 127. It
        // cannot be zero.
        unsafe { NonZeroUsize::new_unchecked((first & 0x7f) as usize) }
    } else {
        return unsafe { decode_len_cold(ptr) };

        // Identifiers 128 bytes or longer. This is not exercised by any crate
        // version currently published to crates.io.
        #[cold]
        #[inline(never)]
        unsafe fn decode_len_cold(mut ptr: *const u8) -> NonZeroUsize {
            let mut len = 0;
            let mut shift = 0;
            loop {
                // SAFETY: varint continues while there are bytes having the
                // most significant bit set, i.e. until we start hitting the
                // ASCII string content with msb unset.
                let byte = unsafe { *ptr };
                if byte < 0x80 {
                    // SAFETY: the string length is known to be 128 bytes or
                    // longer.
                    return unsafe { NonZeroUsize::new_unchecked(len) };
                }
                // SAFETY: still in bounds of the same allocation.
                ptr = unsafe { ptr.add(1) };
                len += ((byte & 0x7f) as usize) << shift;
                shift += 7;
            }
        }
    }
}

// SAFETY: repr must be in the heap allocated representation, with varint header
// and string contents already written.
unsafe fn ptr_as_str(repr: &NonZeroU64) -> &str {
    let ptr = repr_to_ptr(*repr);
    let len = unsafe { decode_len(ptr) };
    let header = bytes_for_varint(len);
    let slice = unsafe { slice::from_raw_parts(ptr.add(header), len.get()) };
    // SAFETY: all identifier contents are ASCII bytes, which are always valid
    // UTF-8.
    unsafe { str::from_utf8_unchecked(slice) }
}

// Number of base-128 digits required for the varint representation of a length.
fn bytes_for_varint(len: NonZeroUsize) -> usize {
    #[cfg(no_nonzero_bitscan)] // rustc <1.53
    let len = len.get();

    let usize_bits = mem::size_of::<usize>() * 8;
    let len_bits = usize_bits - len.leading_zeros() as usize;
    (len_bits + 6) / 7
}

use core::num::NonZeroUsize;

/// Returns the amount of padding we must insert after `len` bytes to ensure
/// that the following address will satisfy `align` (measured in bytes).
///
/// e.g., if `len` is 9, then `padding_needed_for(len, 4)` returns 3, because
/// that is the minimum number of bytes of padding required to get a 4-aligned
/// address (assuming that the corresponding memory block starts at a 4-aligned
/// address).
///
/// The return value of this function has no meaning if `align` is not a
/// power-of-two.
///
/// # Panics
///
/// May panic if `align` is not a power of two.
//
// TODO(#419): Replace `len` with a witness type for region size.
#[allow(unused)]
#[inline(always)]
pub(crate) const fn padding_needed_for(len: usize, align: NonZeroUsize) -> usize {
    // Rounded up value is:
    //   len_rounded_up = (len + align - 1) & !(align - 1);
    // and then we return the padding difference: `len_rounded_up - len`.
    //
    // We use modular arithmetic throughout:
    //
    // 1. align is guaranteed to be > 0, so align - 1 is always
    //    valid.
    //
    // 2. `len + align - 1` can overflow by at most `align - 1`,
    //    so the &-mask with `!(align - 1)` will ensure that in the
    //    case of overflow, `len_rounded_up` will itself be 0.
    //    Thus the returned padding, when added to `len`, yields 0,
    //    which trivially satisfies the alignment `align`.
    //
    // (Of course, attempts to allocate blocks of memory whose
    // size and padding overflow in the above manner should cause
    // the allocator to yield an error anyway.)

    let align = align.get();
    debug_assert!(align.is_power_of_two());
    let len_rounded_up = len.wrapping_add(align).wrapping_sub(1) & !align.wrapping_sub(1);
    len_rounded_up.wrapping_sub(len)
}

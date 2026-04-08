use crate::{FmtArg, PanicVal};

/// For coercing a `&[PanicVal<'_>; LEN]` into a `&[PanicVal<'_>]`.
pub const fn panicvals_id<'a, 'b, const LEN: usize>(
    array: &'b [PanicVal<'a>; LEN],
) -> &'b [PanicVal<'a>] {
    array
}

/// Flattens a `&[&[PanicVal<'a>]]` into a `[PanicVal<'a>; LEN]`.
///
/// If `LEN` is greater than the amount of `PanicVal`s in the slices,
/// this fills the remaining array with [`PanicVal::EMPTY`].
///
/// # Panics
///
/// Panics if the amount of `PanicVal`s in the slices is greater than `LEN`.
///
pub const fn flatten_panicvals<'a, const LEN: usize>(
    mut input: &[&[PanicVal<'a>]],
) -> [PanicVal<'a>; LEN] {
    let mut out = [PanicVal::EMPTY; LEN];
    let mut len = 0usize;

    while let [mut outer, ref rinput @ ..] = *input {
        while let [arg, ref router @ ..] = *outer {
            out[len] = arg;
            len += 1;
            outer = router;
        }
        input = rinput
    }

    out
}

/// Gets the maximum value between `l` and `r`
///
/// # Example
///
/// ```rust
/// use const_panic::utils::max_usize;
///
/// assert_eq!(max_usize(5, 3), 5);
/// assert_eq!(max_usize(5, 8), 8);
///
/// ```
pub const fn max_usize(l: usize, r: usize) -> usize {
    if l > r {
        l
    } else {
        r
    }
}

/// Gets the maximum value in `slice`, returns `0` if the slice is empty.
///
/// # Example
///
/// ```rust
/// use const_panic::utils::slice_max_usize;
///
/// assert_eq!(slice_max_usize(&[]), 0);
/// assert_eq!(slice_max_usize(&[3]), 3);
/// assert_eq!(slice_max_usize(&[5, 3]), 5);
/// assert_eq!(slice_max_usize(&[5, 8, 3]), 8);
/// assert_eq!(slice_max_usize(&[5, 13, 8, 3]), 13);
///
/// ```
pub const fn slice_max_usize(mut slice: &[usize]) -> usize {
    let mut max = 0;

    while let [x, ref rem @ ..] = *slice {
        max = max_usize(max, x);
        slice = rem;
    }

    max
}

#[doc(hidden)]
#[track_caller]
pub const fn assert_flatten_panicvals_length(expected_larger: usize, actual_value: usize) {
    if actual_value > expected_larger {
        crate::concat_panic(&[&[
            PanicVal::write_str("length passed to flatten_panicvals macro ("),
            PanicVal::from_usize(expected_larger, FmtArg::DISPLAY),
            PanicVal::write_str(") is smaller than the computed length ("),
            PanicVal::from_usize(actual_value, FmtArg::DISPLAY),
            PanicVal::write_str(")"),
        ]]);
    }
}

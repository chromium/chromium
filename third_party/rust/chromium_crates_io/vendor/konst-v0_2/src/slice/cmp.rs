#[doc(inline)]
pub use crate::slice::{
    cmp_bytes as cmp_slice_u8, cmp_option_bytes as cmp_option_slice_u8, eq_bytes as eq_slice_u8,
    eq_option_bytes as eq_option_slice_u8,
};

use core::cmp::Ordering;

__declare_slice_cmp_fns! {
    import_path = "konst",

    (,,, u16, eq_slice_u16, cmp_slice_u16,)
    (,,, u32, eq_slice_u32, cmp_slice_u32,)
    (,,, u64, eq_slice_u64, cmp_slice_u64,)
    (,,, u128, eq_slice_u128, cmp_slice_u128,)
    (,,, usize, eq_slice_usize, cmp_slice_usize,)

    (,,, i8, eq_slice_i8, cmp_slice_i8,)
    (,,, i16, eq_slice_i16, cmp_slice_i16,)
    (,,, i32, eq_slice_i32, cmp_slice_i32,)
    (,,, i64, eq_slice_i64, cmp_slice_i64,)
    (,,, i128, eq_slice_i128, cmp_slice_i128,)
    (,,, isize, eq_slice_isize, cmp_slice_isize,)

    (,,, bool, eq_slice_bool, cmp_slice_bool,)
    (,,, char, eq_slice_char, cmp_slice_char,)
}

__delegate_const_eq! {
    skip_coerce;

    /// Compares two `&[&str]` for equality.
    pub const fn eq_slice_str(ref l: &[&str], r: &[&str]) -> bool {
        crate::const_eq_for!(slice; l, r, crate::eq_str)
    }
}

__delegate_const_ord! {
    skip_coerce;

    /// Compares two `&[&str]`, returning the order of `left` relative to `right`.
    pub const fn cmp_slice_str(ref left: &[&str], right: &[&str]) -> Ordering {
        crate::const_cmp_for!(slice; left, right, crate::cmp_str)
    }
}

__delegate_const_eq! {
    skip_coerce;

    /// Compares two `&[&[u8]]` for equality.
    pub const fn eq_slice_bytes(ref l: &[&[u8]], r: &[&[u8]]) -> bool {
        crate::const_eq_for!(slice; l, r, eq_slice_u8)
    }
}

__delegate_const_ord! {
    skip_coerce;

    /// Compares two `&[&[u8]]`, returning the order of `left` relative to `right`.
    pub const fn cmp_slice_bytes(ref left: &[&[u8]], right: &[&[u8]]) -> Ordering {
        crate::const_cmp_for!(slice; left, right, cmp_slice_u8)
    }
}

__declare_fns_with_docs! {
    (Option<&'a [u16]>, (eq_option_slice_u16, cmp_option_slice_u16))
    (Option<&'a [u32]>, (eq_option_slice_u32, cmp_option_slice_u32))
    (Option<&'a [u64]>, (eq_option_slice_u64, cmp_option_slice_u64))
    (Option<&'a [u128]>, (eq_option_slice_u128, cmp_option_slice_u128))
    (Option<&'a [usize]>, (eq_option_slice_usize, cmp_option_slice_usize))
    (Option<&'a [i8]>, (eq_option_slice_i8, cmp_option_slice_i8))
    (Option<&'a [i16]>, (eq_option_slice_i16, cmp_option_slice_i16))
    (Option<&'a [i32]>, (eq_option_slice_i32, cmp_option_slice_i32))
    (Option<&'a [i64]>, (eq_option_slice_i64, cmp_option_slice_i64))
    (Option<&'a [i128]>, (eq_option_slice_i128, cmp_option_slice_i128))
    (Option<&'a [isize]>, (eq_option_slice_isize, cmp_option_slice_isize))
    (Option<&'a [bool]>, (eq_option_slice_bool, cmp_option_slice_bool))
    (Option<&'a [char]>, (eq_option_slice_char, cmp_option_slice_char))

    docs(default)

    macro = __impl_option_cmp_fns!(
        for['a,]
        params(l, r)
        eq_comparison = crate::polymorphism::CmpWrapper(l).const_eq(r),
        cmp_comparison = crate::polymorphism::CmpWrapper(l).const_cmp(r),
        parameter_copyability = copy,
    ),
}

__declare_fns_with_docs! {
    (Option<&'a [&'b str]>, (eq_option_slice_str, cmp_option_slice_str))
    (Option<&'a [&'b [u8]]>, (eq_option_slice_bytes, cmp_option_slice_bytes))

    docs(default)

    macro = __impl_option_cmp_fns!(
        for['a, 'b,]
        params(l, r)
        eq_comparison = crate::polymorphism::CmpWrapper(l).const_eq(r),
        cmp_comparison = crate::polymorphism::CmpWrapper(l).const_cmp(r),
        parameter_copyability = copy,
    ),
}

use core::cmp::Ordering;

macro_rules! declare_int_cmp_fn {
    (
        ($type:ty, ($cmp_fn_name:ident))
        docs( $docs_eq:expr, $docs_cmp:expr, )
    ) => {
        crate::__delegate_const_ord! {
            #[doc = $docs_cmp]
            pub const fn $cmp_fn_name(copy left: $type, right: $type) -> Ordering {
                cmp_int!(left, right)
            }
        }
    };
}

__declare_fns_with_docs! {
    (u8, (cmp_u8))
    (u16, (cmp_u16))
    (u32, (cmp_u32))
    (u64, (cmp_u64))
    (u128, (cmp_u128))
    (usize, (cmp_usize))

    (i8, (cmp_i8))
    (i16, (cmp_i16))
    (i32, (cmp_i32))
    (i64, (cmp_i64))
    (i128, (cmp_i128))
    (isize, (cmp_isize))

    (bool, (cmp_bool))
    (char, (cmp_char))

    docs(default)

    macro = declare_int_cmp_fn!(),
}

__declare_fns_with_docs! {
    (Option<u8>, (eq_option_u8, cmp_option_u8))
    (Option<i8>, (eq_option_i8, cmp_option_i8))
    (Option<u16>, (eq_option_u16, cmp_option_u16))
    (Option<i16>, (eq_option_i16, cmp_option_i16))
    (Option<u32>, (eq_option_u32, cmp_option_u32))
    (Option<i32>, (eq_option_i32, cmp_option_i32))
    (Option<u64>, (eq_option_u64, cmp_option_u64))
    (Option<i64>, (eq_option_i64, cmp_option_i64))
    (Option<u128>, (eq_option_u128, cmp_option_u128))
    (Option<i128>, (eq_option_i128, cmp_option_i128))
    (Option<usize>, (eq_option_usize, cmp_option_usize))
    (Option<isize>, (eq_option_isize, cmp_option_isize))
    (Option<bool>, (eq_option_bool, cmp_option_bool))
    (Option<char>, (eq_option_char, cmp_option_char))

    docs(default)

    macro = __impl_option_cmp_fns!(
        params(l, r)
        eq_comparison = l == r,
        cmp_comparison = cmp_int!(l, r),
        parameter_copyability = copy,
    ),
}

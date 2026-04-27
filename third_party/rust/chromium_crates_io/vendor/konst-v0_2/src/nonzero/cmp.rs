use core::{
    cmp::Ordering,
    num::{
        NonZeroI128, NonZeroI16, NonZeroI32, NonZeroI64, NonZeroI8, NonZeroIsize, NonZeroU128,
        NonZeroU16, NonZeroU32, NonZeroU64, NonZeroU8, NonZeroUsize,
    },
};

macro_rules! declare_nonzero_integers {
    (
        ($type:ty, ($eq_fn_name:ident, $cmp_fn_name:ident))

        docs( $docs_eq:expr, $docs_cmp:expr, )
    ) => {
        __delegate_const_eq! {
            #[doc = $docs_eq]
            pub const fn $eq_fn_name(copy left: $type, right: $type) -> bool {
                left.get() == right.get()
            }
        }

        __delegate_const_ord! {
            #[doc = $docs_cmp]
            pub const fn $cmp_fn_name(copy left: $type, right: $type) -> Ordering {
                cmp_int!(left.get(), right.get())
            }
        }
    };
}

__declare_fns_with_docs! {
    (NonZeroU8, (eq_nonzerou8, cmp_nonzerou8))
    (NonZeroI8, (eq_nonzeroi8, cmp_nonzeroi8))
    (NonZeroU16, (eq_nonzerou16, cmp_nonzerou16))
    (NonZeroI16, (eq_nonzeroi16, cmp_nonzeroi16))
    (NonZeroU32, (eq_nonzerou32, cmp_nonzerou32))
    (NonZeroI32, (eq_nonzeroi32, cmp_nonzeroi32))
    (NonZeroU64, (eq_nonzerou64, cmp_nonzerou64))
    (NonZeroI64, (eq_nonzeroi64, cmp_nonzeroi64))
    (NonZeroU128, (eq_nonzerou128, cmp_nonzerou128))
    (NonZeroI128, (eq_nonzeroi128, cmp_nonzeroi128))
    (NonZeroUsize, (eq_nonzerousize, cmp_nonzerousize))
    (NonZeroIsize, (eq_nonzeroisize, cmp_nonzeroisize))

    docs(default)

    macro = declare_nonzero_integers!(),
}

__declare_fns_with_docs! {
    (Option<NonZeroU8>, (eq_option_nonzerou8, cmp_option_nonzerou8))
    (Option<NonZeroI8>, (eq_option_nonzeroi8, cmp_option_nonzeroi8))
    (Option<NonZeroU16>, (eq_option_nonzerou16, cmp_option_nonzerou16))
    (Option<NonZeroI16>, (eq_option_nonzeroi16, cmp_option_nonzeroi16))
    (Option<NonZeroU32>, (eq_option_nonzerou32, cmp_option_nonzerou32))
    (Option<NonZeroI32>, (eq_option_nonzeroi32, cmp_option_nonzeroi32))
    (Option<NonZeroU64>, (eq_option_nonzerou64, cmp_option_nonzerou64))
    (Option<NonZeroI64>, (eq_option_nonzeroi64, cmp_option_nonzeroi64))
    (Option<NonZeroU128>, (eq_option_nonzerou128, cmp_option_nonzerou128))
    (Option<NonZeroI128>, (eq_option_nonzeroi128, cmp_option_nonzeroi128))
    (Option<NonZeroUsize>, (eq_option_nonzerousize, cmp_option_nonzerousize))
    (Option<NonZeroIsize>, (eq_option_nonzeroisize, cmp_option_nonzeroisize))

    docs(default)

    macro = __impl_option_cmp_fns!(
        params(l, r)
        eq_comparison = l.get() == r.get(),
        cmp_comparison = cmp_int!(l.get(), r.get()),
        parameter_copyability = copy,
    ),
}

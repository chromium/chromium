use core::{
    cmp::Ordering,
    marker::{PhantomData, PhantomPinned},
};

__delegate_const_eq! {
    /// Compares two `Ordering` for equality.
    #[inline]
    pub const fn eq_ordering(copy left: Ordering, right: Ordering) -> bool {
        left as i8 == right as i8
    }
}

__delegate_const_ord! {
    /// Compares two `Ordering`, returning the ordering of `left` relative to `right`.
    #[inline]
    pub const fn cmp_ordering(copy left: Ordering, right: Ordering) -> Ordering {
        cmp_int!(left as i8, right as i8)
    }
}

__declare_fns_with_docs! {
    (Option<Ordering>, (eq_option_ordering, cmp_option_ordering))

    docs(default)

    macro = __impl_option_cmp_fns!(
        params(l, r)
        eq_comparison = eq_ordering(l, r),
        cmp_comparison = cmp_ordering(l, r),
        parameter_copyability = copy,
    ),
}

macro_rules! impl_for_marker_traits{
    (
        ($type:ty, ($eq_fn_name:ident, $cmp_fn_name:ident) $(, for[$($generic:tt)*] )? )

        docs( $docs_eq:expr, $docs_cmp:expr, )
    ) => {
        __delegate_const_eq! {
            $(for[$($generic)*] )?
            #[doc = $docs_eq]
            #[inline]
            pub const fn $eq_fn_name(copy _l: $type, _r: $type) -> bool {
                true
            }
        }

        __delegate_const_ord! {
            $(for[$($generic)*] )?
            #[doc = $docs_cmp]
            #[inline]
            pub const fn $cmp_fn_name(copy _l: $type, _r: $type) -> Ordering {
                Ordering::Equal
            }
        }
    }
}

__declare_fns_with_docs! {
    (PhantomData<T>, (eq_phantomdata, cmp_phantomdata), for[T,])
    (PhantomPinned, (eq_phantompinned, cmp_phantompinned))

    docs(default)

    macro = impl_for_marker_traits!(),
}

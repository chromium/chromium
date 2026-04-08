use core::mem::ManuallyDrop;

#[repr(C)]
pub union Transmuter<F, T> {
    pub from: ManuallyDrop<F>,
    pub to: ManuallyDrop<T>,
}

#[macro_export]
#[doc(hidden)]
macro_rules! __priv_transmute {
    ($from:ty, $to:ty, $value:expr) => {{
        $crate::__::ManuallyDrop::into_inner(
            $crate::utils::Transmuter::<$from, $to> {
                from: $crate::__::ManuallyDrop::new($value),
            }
            .to,
        )
    }};
}


macro_rules! conditionally_const {
    (
        feature = $feature:literal;
        
        $( #[$meta:meta] )*
        $vis:vis fn $fn_name:ident $([$($generics:tt)*])? (
            $($params:tt)*
        ) -> $ret:ty 
        $block:block
    ) => (
        $(#[$meta])*
        #[cfg(feature = $feature)]
        $vis const fn $fn_name $(<$($generics)*>)? ($($params)*) -> $ret $block

        $(#[$meta])*
        #[cfg(not(feature = $feature))]
        $vis fn $fn_name $(<$($generics)*>)? ($($params)*) -> $ret $block
    )
} pub(crate) use conditionally_const;



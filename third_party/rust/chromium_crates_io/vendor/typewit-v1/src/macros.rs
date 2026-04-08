pub(crate) mod generics_parsing;
pub(crate) mod simple_type_witness_macro;
mod type_fn_macro;

mod inj_type_fn_macro;

mod polymatch;

mod type_ne_macro;

#[cfg(not(feature = "proc_macros"))]
#[doc(hidden)]
#[macro_export]
macro_rules! __impl_with_span {
    (
        $impl_span:tt 
        ($($impl_attrs:tt)*) 
        ($($impl_trait:tt)*) 
        ($($impl_type:tt)*) 
        ($($impl_where:tt)*) 
        ($($impl_assoc_items:tt)*) 
    ) => {
        $($impl_attrs)*
        impl $($impl_trait)* for $($impl_type)*
        $($impl_where)*
        {
            $($impl_assoc_items)*
        }
    };
}
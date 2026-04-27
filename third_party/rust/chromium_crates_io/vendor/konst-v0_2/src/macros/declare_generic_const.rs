macro_rules! declare_generic_const {
    (
        $(
            $(#[$meta:meta])*
            for[$($gen_param:tt)*]
            $vis:vis const $name:ident [$($lt:lifetime,)* $($ty:ident),* $(; $($const:ident),*)? ]
            :$ret:ty = $value:expr;
        )*
    ) => {
        $(
            $(#[$meta])*
            #[allow(non_camel_case_types)]
            $vis struct $name<$($gen_param)*>(
                core::marker::PhantomData<(
                    $( core::marker::PhantomData<&$lt ()>, )*
                    $( core::marker::PhantomData<$ty>, )*
                )>
            );

            impl<$($gen_param)*> $name<$($lt,)* $($ty,)* $($($const,)*)? > {
                /// The value that this constructs.
                $vis const V: $ret = $value;
            }
        )*
    };
}

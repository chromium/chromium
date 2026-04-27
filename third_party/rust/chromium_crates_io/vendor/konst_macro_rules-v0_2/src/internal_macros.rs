#[doc(hidden)]
#[macro_export]
macro_rules! iterator_shared {
    (
        is_forward = $is_forward:ident,
        item = $Item:ty,
        iter_forward = $Self:ty,
        $(iter_reversed = $Rev:path,)?
        next($self:ident) $next_block:block,
        $(next_back $next_back_block:block,)?
        fields = $fields:tt,
    ) => {
        /// Creates a clone of this iterator
        pub const fn copy(&self) -> Self {
            let Self $fields = *self;
            Self $fields
        }

        $(
            /// Reverses the iterator
            pub const fn rev(self) -> $crate::__choose!($is_forward $Rev $Self) {
                let Self $fields = self;
                type Type<T> = T;
                Type::<$crate::__choose!($is_forward $Rev $Self)> $fields
            }
        )?

        /// Advances the iterator and returns the next value.
        pub const fn next(mut $self) -> Option<($Item, Self)> {
            $crate::__choose!{
                $is_forward
                $next_block
                $($next_back_block)?
            }
        }

        $(
            /// Removes and returns an element from the end of the iterator.
            pub const fn next_back(mut $self) -> Option<($Item, Self)> {
                $crate::__choose!{
                    $is_forward
                    $next_back_block
                    $next_block
                }
            }
        )?
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! __choose {
    (true $then:tt $($else:tt)*) => {
        $then
    };
    (false $then:tt $else:tt) => {
        $else
    };
}

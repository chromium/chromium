/// For implementing const comparison semi-manually.
///
/// # Impls
///
/// This macro implements [`ConstCmpMarker`] for all the `impl`d types,
/// and outputs the methods/associated constants in each of the listed impls.
///
/// # Example
///
/// ### Generic type
///
/// This demonstrates how you can implement equality and ordering comparison for a generic struct.
///
/// ```rust
/// use konst::{const_cmp, const_eq, impl_cmp, try_equal};
///
/// use std::{
///     cmp::Ordering,
///     marker::PhantomData,
/// };
///
/// pub struct Tupled<T>(u32, T);
///
/// impl_cmp!{
///     impl[T] Tupled<PhantomData<T>>
///     where[ T: 'static ];
///
///     impl[] Tupled<bool>;
///     impl Tupled<Option<bool>>;
///     
///     pub const fn const_eq(&self, other: &Self) -> bool {
///         const_eq!(self.0, other.0) &&
///         const_eq!(self.1, other.1)
///     }
///     pub const fn const_cmp(&self, other: &Self) -> Ordering {
///         try_equal!(const_cmp!(self.0, other.0));
///         try_equal!(const_cmp!(self.1, other.1))
///     }
/// }
///
/// const CMPS: [(Ordering, bool); 4] = {
///     let foo = Tupled(3, PhantomData::<u32>);
///     let bar = Tupled(5, PhantomData::<u32>);
///     
///     [
///         (const_cmp!(foo, foo), const_eq!(foo, foo)),
///         (const_cmp!(foo, bar), const_eq!(foo, bar)),
///         (const_cmp!(bar, foo), const_eq!(bar, foo)),
///         (const_cmp!(bar, bar), const_eq!(bar, bar)),
///     ]
/// };
///
/// assert_eq!(
///     CMPS,
///     [
///         (Ordering::Equal, true),
///         (Ordering::Less, false),
///         (Ordering::Greater, false),
///         (Ordering::Equal, true),
///     ]
/// );
///
/// ```
///
/// ### Enum
///
/// This demonstrates how you can implement equality and ordering comparison for an enum.
///
/// ```rust
/// use konst::{const_cmp, const_eq, impl_cmp, try_equal};
///
/// use std::cmp::Ordering;
///
/// pub enum Enum {
///     Tupled(u32, u32),
///     Unit,
/// }
///
/// impl_cmp!{
///     impl Enum;
///     
///     pub const fn const_eq(&self, other: &Self) -> bool {
///         match (self, other) {
///             (Self::Tupled(l0,l1), Self::Tupled(r0, r1)) => *l0 == *r0 && *l1 == *r1,
///             (Self::Unit, Self::Unit) => true,
///             _ => false,
///         }
///     }
///     pub const fn const_cmp(&self, other: &Self) -> Ordering {
///         match (self, other) {
///             (Self::Tupled(l0,l1), Self::Tupled(r0, r1)) => {
///                 try_equal!(const_cmp!(*l0, *r0));
///                 try_equal!(const_cmp!(*l1, *r1))
///             }
///             (Self::Tupled{..}, Self::Unit) => Ordering::Less,
///             (Self::Unit, Self::Unit) => Ordering::Equal,
///             (Self::Unit, Self::Tupled{..}) => Ordering::Greater,
///         }
///     }
/// }
///
/// const CMPS: [(Ordering, bool); 4] = {
///     let foo = Enum::Tupled(3, 5);
///     let bar = Enum::Unit;
///     
///     [
///         (const_cmp!(foo, foo), const_eq!(foo, foo)),
///         (const_cmp!(foo, bar), const_eq!(foo, bar)),
///         (const_cmp!(bar, foo), const_eq!(bar, foo)),
///         (const_cmp!(bar, bar), const_eq!(bar, bar)),
///     ]
/// };
///
/// assert_eq!(
///     CMPS,
///     [
///         (Ordering::Equal, true),
///         (Ordering::Less, false),
///         (Ordering::Greater, false),
///         (Ordering::Equal, true),
///     ]
/// );
///
/// ```
///
/// [`ConstCmpMarker`]: ./polymorphism/trait.ConstCmpMarker.html
///
#[cfg(feature = "cmp")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "cmp")))]
#[macro_export]
macro_rules! impl_cmp {
    (
        $($rem:tt)*
    ) => (
        $crate::__impl_cmp_recursive!{
            impls[
            ]
            tokens[$($rem)*]
        }
    );
    (
        $($rem:tt)*
    ) => (
        $crate::__impl_cmp_recursive!{
            impls[
            ]
            tokens[$($rem)*]
        }
    );
}

#[doc(hidden)]
#[macro_export]
macro_rules! __impl_cmp_recursive{
    (
        impls[$($impls:tt)*]

        tokens[
            $(#[$impl_attr:meta])*
            impl[$($impl_:tt)*] $type:ty
            $(where[ $($where:tt)* ])?;

            $($rem:tt)*
        ]
    ) => (
        $crate::__impl_cmp_recursive!{

            impls[
                $($impls)*
                (
                    $(#[$impl_attr])*
                    impl[$($impl_)*] $type
                    where[ $($($where)*)? ];
                )
            ]
            tokens[
                $($rem)*
            ]
        }
    );
    // The same as the above macro branch, but it doesn't require the `[]` in `impl[]`
    (
        impls[$($impls:tt)*]

        tokens[
            $(#[$impl_attr:meta])*
            impl $type:ty
            $(where[ $($where:tt)* ])?;

            $($rem:tt)*
        ]
    ) => (
        $crate::__impl_cmp_recursive!{

            impls[
                $($impls)*
                (
                    $(#[$impl_attr])*
                    impl[] $type
                    where[ $($($where)*)? ];
                )
            ]
            tokens[
                $($rem)*
            ]
        }
    );
    (
        impls [ $( $an_impl:tt )+ ]
        tokens $stuff:tt
    ) => (
        $(
            $crate::__impl_cmp_impl!{
                $an_impl
                $stuff
            }
        )+
    );
}

#[doc(hidden)]
#[macro_export]
macro_rules! __impl_cmp_impl {
    (
        (
            $(#[$impl_attr:meta])*
            impl[$($impl_:tt)*] $type:ty
            where[ $($where:tt)* ];
        )
        [ $($everything:tt)* ]
    )=>{
        $(#[$impl_attr])*
        impl<$($impl_)*> $crate::__::ConstCmpMarker for $type
        where
            $($where)*
        {
            type Kind = $crate::__::IsNotStdKind;
            type This = Self;
        }

        $(#[$impl_attr])*
        impl<$($impl_)*> $type
        where
            $($where)*
        {
            $($everything)*
        }
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! __impl_cmp_self_ty {
    ($self:ty, /*is_std_type*/ true )=>{
        $crate::__::PWrapper<$self>
    };
    ($self:ty, /*is_std_type*/ false )=>{
        $self
    };

}

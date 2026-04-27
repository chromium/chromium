#[doc(hidden)]
#[macro_export]
macro_rules!  __priv_delegate_const_inner_fn{
    (
        $(skip_coerce $(@$_skip:tt@)?;)?

        $(for[$($implg:tt)*])?
        $(#[$attr:meta])*
        pub const fn $func:ident $(<$($fnlt:lifetime),* $(,)?>)?(
            $($idents:ident)* : $l_ty:ty,
            $rhs:ident: $r_ty:ty $(,)*
        ) -> $ret:ty $block:block
    )=>{
        $(#[$attr])*
        pub const fn $func<$($($fnlt,)*)? $($($implg)*)?>(
            $crate::__priv_get_pati_ident!($($idents)*): $l_ty,
            $rhs: $r_ty,
        ) -> $ret $block
    }
}

#[cfg(feature = "cmp")]
#[doc(hidden)]
#[macro_export]
macro_rules!  __priv_delegate_const_inner_cmpwrapper{
    (
        ($docs:expr, $cw_method:ident, $returns:ty)

        $(skip_coerce $(@$_skip:tt@)?;)*

        $( for[$($implg:tt)*] )?
        $(#[$attr:meta])*
        pub const fn $func:ident $(<$($fnlt:lifetime),* $(,)?>)?(
            $($idents:ident)* : $l_ty:ty,
            $rhs:ident: $r_ty:ty $(,)*
        ) -> $ret:ty $block:block
    ) => {
        $crate::__priv_std_kind_impl!{
            $(skip_coerce $(@$_skip@)?;)*
            impl[$($($implg)*)?] $l_ty
        }

        impl<$($($implg)*)?> $crate::__::CmpWrapper<$l_ty> {
            #[doc = $docs]
            #[inline]
            pub const fn $cw_method<$($($fnlt,)*)?>(
                &self,
                other: $crate::__priv_ref_if_nonref!(($($idents)*) $r_ty),
            ) -> $returns {
                $func(
                    $crate::__priv_copy_if_nonref!(($($idents)*) self.0),
                    $crate::__priv_deref_if_nonref!(($($idents)*) other)
                )
            }
        }
    }
}

/// `__delegate_const_eq` allows:
/// - defining a free function,
/// - defining an inherent `cosnt_eq` method on CmpWrapper that delegates to that free function.
/// - ConstCmpMarker impl for the first parameter type
/// - Add a coerce inhenrent method for IsAConstCmpMarker
///
#[cfg(not(feature = "cmp"))]
#[doc(hidden)]
#[macro_export]
macro_rules! __delegate_const_eq{
    ( $($input:tt)* )=>{
        $crate::__priv_delegate_const_inner_fn!{ $($input)* }
    }
}

#[cfg(feature = "cmp")]
#[doc(hidden)]
#[macro_export]
macro_rules!  __delegate_const_eq{
    ( $($input:tt)* )=>{
        $crate::__priv_delegate_const_inner_fn!{ $($input)* }

        $crate::__priv_delegate_const_inner_cmpwrapper!{
            (
                "Compares `self` and `other` for equality.",
                const_eq,
                bool
            )

            $($input)*
        }
    };
}

#[cfg(not(feature = "cmp"))]
#[doc(hidden)]
#[macro_export]
macro_rules! __delegate_const_ord{
    ($($input:tt)*)=>{
        $crate::__priv_delegate_const_inner_fn!{ $($input)* }
    }
}

#[cfg(feature = "cmp")]
#[doc(hidden)]
#[macro_export]
macro_rules! __delegate_const_ord{
    ( $($input:tt)* )=>{
        $crate::__priv_delegate_const_inner_fn!{ $($input)* }

        $crate::__priv_delegate_const_inner_cmpwrapper!{
            (
                "Compares `self` and `other` for ordering.",
                const_cmp,
                $crate::__::Ordering
            )

            skip_coerce;

            $($input)*
        }
    };
}

#[cfg(feature = "cmp")]
#[doc(hidden)]
#[macro_export]
macro_rules! __priv_copy_if_nonref {
    ((ref $ident:ident) $expr:expr) => {
        &$expr
    };
    ((copy $ident:ident) $expr:expr) => {
        $expr
    };
}
#[cfg(feature = "cmp")]
#[doc(hidden)]
#[macro_export]
macro_rules! __priv_deref_if_nonref {
    ((ref $ident:ident) $expr:expr) => {
        $expr
    };
    ((copy $ident:ident) $expr:expr) => {
        *$expr
    };
}

#[cfg(feature = "cmp")]
#[doc(hidden)]
#[macro_export]
macro_rules! __priv_ref_if_nonref {
    ((ref $ident:ident) $ty:ty) => {
        $ty
    };
    ((copy $ident:ident) $ty:ty) => {
        &$ty
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! __priv_get_pati_ident {
    (ref $ident:ident) => {
        $ident
    };
    (copy $ident:ident) => {
        $ident
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! __priv_std_kind_impl {
    (
        impl[$($impl:tt)*] $self:ty
        $(where[ $($where_:tt)* ])?
    )=>{
        impl<$($impl)*> $crate::__::ConstCmpMarker for $self
        where
            $($($where_)*)?
        {
            type Kind = $crate::__::IsStdKind;
            type This = Self;
        }

        impl<$($impl)* __T> $crate::__::IsAConstCmpMarker<$crate::__::IsStdKind, $self, __T>
        where
            $($($where_)*)?
        {
            ///
            #[inline(always)]
            pub const fn coerce(self, reference: &$self) -> $crate::__::CmpWrapper<$self> {
                $crate::__::CmpWrapper(*reference)
            }
        }
    };
    (skip_coerce $($anything:tt)*)=>{};
}

/// Coerces `reference` to a type that has a `const_eq` or `const_cmp` method.
///
/// # Behavior
///
/// This requires arguments to implement the [`ConstCmpMarker`] trait.
///
/// When a type from the standard library is passed,
/// this wraps it inside a [`CmpWrapper`],
/// which declares `const_eq` and `const_cmp` methods for many standard library types.
///
/// When a user-defined type is used, this evaluates to a reference to the passed in value,
/// dereferencing it as necessary.
///
/// # Limitations
///
/// The parameter(s) must be concrete types, and have a fully inferred type.
/// eg: if you pass an integer literal it must have a suffix to indicate its type.
///
/// # Example
///
/// ```rust
/// use konst::{
///     polymorphism::CmpWrapper,
///     coerce_to_cmp, impl_cmp,
/// };
///
/// struct Unit;
///
/// impl_cmp!{
///     impl Unit;
///     
///     pub const fn const_eq(&self, other: &Self) -> bool {
///         true
///     }
/// }
///
/// let wrapper: CmpWrapper<i32> = coerce_to_cmp!(0i32);
/// assert!( wrapper.const_eq(&0));
/// assert!(!wrapper.const_eq(&1));
///
/// let unit: &Unit = coerce_to_cmp!(Unit);
/// assert!( unit.const_eq(&Unit));
///
///
///
///
///
///
/// ```
///
/// [`ConstCmpMarker`]: ./polymorphism/trait.ConstCmpMarker.html
/// [`CmpWrapper`]: ./polymorphism/struct.CmpWrapper.html
///
#[cfg(feature = "cmp")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "cmp")))]
#[macro_export]
macro_rules! coerce_to_cmp {
    ($reference:expr $(,)*) => {{
        match $reference {
            ref reference => {
                let marker = $crate::__::IsAConstCmpMarker::NEW;
                if false {
                    marker.infer_type(reference);
                }
                marker.coerce(marker.unreference(reference))
            }
        }
    }};
    ($left:expr, $right:expr $(,)*) => {{
        match (&$left, &$right) {
            (left, right) => {
                let l_marker = $crate::__::IsAConstCmpMarker::NEW;
                let r_marker = $crate::__::IsAConstCmpMarker::NEW;
                if false {
                    l_marker.infer_type(left);
                    r_marker.infer_type(right);
                }
                (
                    l_marker.coerce(l_marker.unreference(left)),
                    r_marker.unreference(right),
                )
            }
        }
    }};
}

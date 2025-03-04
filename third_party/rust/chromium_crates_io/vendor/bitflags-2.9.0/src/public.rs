//! Generate the user-facing flags type.
//!
//! The code here belongs to the end-user, so new trait implementations and methods can't be
//! added without potentially breaking users.

/// Declare the user-facing bitflags struct.
///
/// This type is guaranteed to be a newtype with a `bitflags`-facing type as its single field.
#[macro_export]
#[doc(hidden)]
macro_rules! __declare_public_bitflags {
    (
        $(#[$outer:meta])*
        $vis:vis struct $PublicBitFlags:ident
    ) => {
        $(#[$outer])*
        $vis struct $PublicBitFlags(<$PublicBitFlags as $crate::__private::PublicFlags>::Internal);
    };
}

/// Implement functions on the public (user-facing) bitflags type.
///
/// We need to be careful about adding new methods and trait implementations here because they
/// could conflict with items added by the end-user.
#[macro_export]
#[doc(hidden)]
macro_rules! __impl_public_bitflags_forward {
    (
        $(#[$outer:meta])*
        $PublicBitFlags:ident: $T:ty, $InternalBitFlags:ident
    ) => {
        $crate::__impl_bitflags! {
            $(#[$outer])*
            $PublicBitFlags: $T {
                fn empty() {
                    Self($InternalBitFlags::empty())
                }

                fn all() {
                    Self($InternalBitFlags::all())
                }

                fn bits(f) {
                    f.0.bits()
                }

                fn from_bits(bits) {
                    match $InternalBitFlags::from_bits(bits) {
                        $crate::__private::core::option::Option::Some(bits) => $crate::__private::core::option::Option::Some(Self(bits)),
                        $crate::__private::core::option::Option::None => $crate::__private::core::option::Option::None,
                    }
                }

                fn from_bits_truncate(bits) {
                    Self($InternalBitFlags::from_bits_truncate(bits))
                }

                fn from_bits_retain(bits) {
                    Self($InternalBitFlags::from_bits_retain(bits))
                }

                fn from_name(name) {
                    match $InternalBitFlags::from_name(name) {
                        $crate::__private::core::option::Option::Some(bits) => $crate::__private::core::option::Option::Some(Self(bits)),
                        $crate::__private::core::option::Option::None => $crate::__private::core::option::Option::None,
                    }
                }

                fn is_empty(f) {
                    f.0.is_empty()
                }

                fn is_all(f) {
                    f.0.is_all()
                }

                fn intersects(f, other) {
                    f.0.intersects(other.0)
                }

                fn contains(f, other) {
                    f.0.contains(other.0)
                }

                fn insert(f, other) {
                    f.0.insert(other.0)
                }

                fn remove(f, other) {
                    f.0.remove(other.0)
                }

                fn toggle(f, other) {
                    f.0.toggle(other.0)
                }

                fn set(f, other, value) {
                    f.0.set(other.0, value)
                }

                fn intersection(f, other) {
                    Self(f.0.intersection(other.0))
                }

                fn union(f, other) {
                    Self(f.0.union(other.0))
                }

                fn difference(f, other) {
                    Self(f.0.difference(other.0))
                }

                fn symmetric_difference(f, other) {
                    Self(f.0.symmetric_difference(other.0))
                }

                fn complement(f) {
                    Self(f.0.complement())
                }
            }
        }
    };
}

/// Implement functions on the public (user-facing) bitflags type.
///
/// We need to be careful about adding new methods and trait implementations here because they
/// could conflict with items added by the end-user.
#[macro_export]
#[doc(hidden)]
macro_rules! __impl_public_bitflags {
    (
        $(#[$outer:meta])*
        $BitFlags:ident: $T:ty, $PublicBitFlags:ident {
            $(
                $(#[$inner:ident $($args:tt)*])*
                const $Flag:tt = $value:expr;
            )*
        }
    ) => {
        $crate::__impl_bitflags! {
            $(#[$outer])*
            $BitFlags: $T {
                fn empty() {
                    Self(<$T as $crate::Bits>::EMPTY)
                }

                fn all() {
                    let mut truncated = <$T as $crate::Bits>::EMPTY;
                    let mut i = 0;

                    $(
                        $crate::__bitflags_expr_safe_attrs!(
                            $(#[$inner $($args)*])*
                            {{
                                let flag = <$PublicBitFlags as $crate::Flags>::FLAGS[i].value().bits();

                                truncated = truncated | flag;
                                i += 1;
                            }}
                        );
                    )*

                    let _ = i;
                    Self::from_bits_retain(truncated)
                }

                fn bits(f) {
                    f.0
                }

                fn from_bits(bits) {
                    let truncated = Self::from_bits_truncate(bits).0;

                    if truncated == bits {
                        $crate::__private::core::option::Option::Some(Self(bits))
                    } else {
                        $crate::__private::core::option::Option::None
                    }
                }

                fn from_bits_truncate(bits) {
                    Self(bits & Self::all().bits())
                }

                fn from_bits_retain(bits) {
                    Self(bits)
                }

                fn from_name(name) {
                    $(
                        $crate::__bitflags_flag!({
                            name: $Flag,
                            named: {
                                $crate::__bitflags_expr_safe_attrs!(
                                    $(#[$inner $($args)*])*
                                    {
                                        if name == $crate::__private::core::stringify!($Flag) {
                                            return $crate::__private::core::option::Option::Some(Self($PublicBitFlags::$Flag.bits()));
                                        }
                                    }
                                );
                            },
                            unnamed: {},
                        });
                    )*

                    let _ = name;
                    $crate::__private::core::option::Option::None
                }

                fn is_empty(f) {
                    f.bits() == <$T as $crate::Bits>::EMPTY
                }

                fn is_all(f) {
                    // NOTE: We check against `Self::all` here, not `Self::Bits::ALL`
                    // because the set of all flags may not use all bits
                    Self::all().bits() | f.bits() == f.bits()
                }

                fn intersects(f, other) {
                    f.bits() & other.bits() != <$T as $crate::Bits>::EMPTY
                }

                fn contains(f, other) {
                    f.bits() & other.bits() == other.bits()
                }

                fn insert(f, other) {
                    *f = Self::from_bits_retain(f.bits()).union(other);
                }

                fn remove(f, other) {
                    *f = Self::from_bits_retain(f.bits()).difference(other);
                }

                fn toggle(f, other) {
                    *f = Self::from_bits_retain(f.bits()).symmetric_difference(other);
                }

                fn set(f, other, value) {
                    if value {
                        f.insert(other);
                    } else {
                        f.remove(other);
                    }
                }

                fn intersection(f, other) {
                    Self::from_bits_retain(f.bits() & other.bits())
                }

                fn union(f, other) {
                    Self::from_bits_retain(f.bits() | other.bits())
                }

                fn difference(f, other) {
                    Self::from_bits_retain(f.bits() & !other.bits())
                }

                fn symmetric_difference(f, other) {
                    Self::from_bits_retain(f.bits() ^ other.bits())
                }

                fn complement(f) {
                    Self::from_bits_truncate(!f.bits())
                }
            }
        }
    };
}

/// Implement iterators on the public (user-facing) bitflags type.
#[macro_export]
#[doc(hidden)]
macro_rules! __impl_public_bitflags_iter {
    (
        $(#[$outer:meta])*
        $BitFlags:ident: $T:ty, $PublicBitFlags:ident
    ) => {
        $(#[$outer])*
        impl $BitFlags {
            /// Yield a set of contained flags values.
            ///
            /// Each yielded flags value will correspond to a defined named flag. Any unknown bits
            /// will be yielded together as a final flags value.
            #[inline]
            pub const fn iter(&self) -> $crate::iter::Iter<$PublicBitFlags> {
                $crate::iter::Iter::__private_const_new(
                    <$PublicBitFlags as $crate::Flags>::FLAGS,
                    $PublicBitFlags::from_bits_retain(self.bits()),
                    $PublicBitFlags::from_bits_retain(self.bits()),
                )
            }

            /// Yield a set of contained named flags values.
            ///
            /// This method is like [`iter`](#method.iter), except only yields bits in contained named flags.
            /// Any unknown bits, or bits not corresponding to a contained flag will not be yielded.
            #[inline]
            pub const fn iter_names(&self) -> $crate::iter::IterNames<$PublicBitFlags> {
                $crate::iter::IterNames::__private_const_new(
                    <$PublicBitFlags as $crate::Flags>::FLAGS,
                    $PublicBitFlags::from_bits_retain(self.bits()),
                    $PublicBitFlags::from_bits_retain(self.bits()),
                )
            }
        }

        $(#[$outer:meta])*
        impl $crate::__private::core::iter::IntoIterator for $BitFlags {
            type Item = $PublicBitFlags;
            type IntoIter = $crate::iter::Iter<$PublicBitFlags>;

            fn into_iter(self) -> Self::IntoIter {
                self.iter()
            }
        }
    };
}

/// Implement traits on the public (user-facing) bitflags type.
#[macro_export]
#[doc(hidden)]
macro_rules! __impl_public_bitflags_ops {
    (
        $(#[$outer:meta])*
        $PublicBitFlags:ident
    ) => {

        $(#[$outer])*
        impl $crate::__private::core::fmt::Binary for $PublicBitFlags {
            fn fmt(
                &self,
                f: &mut $crate::__private::core::fmt::Formatter,
            ) -> $crate::__private::core::fmt::Result {
                let inner = self.0;
                $crate::__private::core::fmt::Binary::fmt(&inner, f)
            }
        }

        $(#[$outer])*
        impl $crate::__private::core::fmt::Octal for $PublicBitFlags {
            fn fmt(
                &self,
                f: &mut $crate::__private::core::fmt::Formatter,
            ) -> $crate::__private::core::fmt::Result {
                let inner = self.0;
                $crate::__private::core::fmt::Octal::fmt(&inner, f)
            }
        }

        $(#[$outer])*
        impl $crate::__private::core::fmt::LowerHex for $PublicBitFlags {
            fn fmt(
                &self,
                f: &mut $crate::__private::core::fmt::Formatter,
            ) -> $crate::__private::core::fmt::Result {
                let inner = self.0;
                $crate::__private::core::fmt::LowerHex::fmt(&inner, f)
            }
        }

        $(#[$outer])*
        impl $crate::__private::core::fmt::UpperHex for $PublicBitFlags {
            fn fmt(
                &self,
                f: &mut $crate::__private::core::fmt::Formatter,
            ) -> $crate::__private::core::fmt::Result {
                let inner = self.0;
                $crate::__private::core::fmt::UpperHex::fmt(&inner, f)
            }
        }

        $(#[$outer])*
        impl $crate::__private::core::ops::BitOr for $PublicBitFlags {
            type Output = Self;

            /// The bitwise or (`|`) of the bits in two flags values.
            #[inline]
            fn bitor(self, other: $PublicBitFlags) -> Self {
                self.union(other)
            }
        }

        $(#[$outer])*
        impl $crate::__private::core::ops::BitOrAssign for $PublicBitFlags {
            /// The bitwise or (`|`) of the bits in two flags values.
            #[inline]
            fn bitor_assign(&mut self, other: Self) {
                self.insert(other);
            }
        }

        $(#[$outer])*
        impl $crate::__private::core::ops::BitXor for $PublicBitFlags {
            type Output = Self;

            /// The bitwise exclusive-or (`^`) of the bits in two flags values.
            #[inline]
            fn bitxor(self, other: Self) -> Self {
                self.symmetric_difference(other)
            }
        }

        $(#[$outer])*
        impl $crate::__private::core::ops::BitXorAssign for $PublicBitFlags {
            /// The bitwise exclusive-or (`^`) of the bits in two flags values.
            #[inline]
            fn bitxor_assign(&mut self, other: Self) {
                self.toggle(other);
            }
        }

        $(#[$outer])*
        impl $crate::__private::core::ops::BitAnd for $PublicBitFlags {
            type Output = Self;

            /// The bitwise and (`&`) of the bits in two flags values.
            #[inline]
            fn bitand(self, other: Self) -> Self {
                self.intersection(other)
            }
        }

        $(#[$outer])*
        impl $crate::__private::core::ops::BitAndAssign for $PublicBitFlags {
            /// The bitwise and (`&`) of the bits in two flags values.
            #[inline]
            fn bitand_assign(&mut self, other: Self) {
                *self = Self::from_bits_retain(self.bits()).intersection(other);
            }
        }

        $(#[$outer])*
        impl $crate::__private::core::ops::Sub for $PublicBitFlags {
            type Output = Self;

            /// The intersection of a source flags value with the complement of a target flags value (`&!`).
            ///
            /// This method is not equivalent to `self & !other` when `other` has unknown bits set.
            /// `difference` won't truncate `other`, but the `!` operator will.
            #[inline]
            fn sub(self, other: Self) -> Self {
                self.difference(other)
            }
        }

        $(#[$outer])*
        impl $crate::__private::core::ops::SubAssign for $PublicBitFlags {
            /// The intersection of a source flags value with the complement of a target flags value (`&!`).
            ///
            /// This method is not equivalent to `self & !other` when `other` has unknown bits set.
            /// `difference` won't truncate `other`, but the `!` operator will.
            #[inline]
            fn sub_assign(&mut self, other: Self) {
                self.remove(other);
            }
        }

        $(#[$outer])*
        impl $crate::__private::core::ops::Not for $PublicBitFlags {
            type Output = Self;

            /// The bitwise negation (`!`) of the bits in a flags value, truncating the result.
            #[inline]
            fn not(self) -> Self {
                self.complement()
            }
        }

        $(#[$outer])*
        impl $crate::__private::core::iter::Extend<$PublicBitFlags> for $PublicBitFlags {
            /// The bitwise or (`|`) of the bits in each flags value.
            fn extend<T: $crate::__private::core::iter::IntoIterator<Item = Self>>(
                &mut self,
                iterator: T,
            ) {
                for item in iterator {
                    self.insert(item)
                }
            }
        }

        $(#[$outer])*
        impl $crate::__private::core::iter::FromIterator<$PublicBitFlags> for $PublicBitFlags {
            /// The bitwise or (`|`) of the bits in each flags value.
            fn from_iter<T: $crate::__private::core::iter::IntoIterator<Item = Self>>(
                iterator: T,
            ) -> Self {
                use $crate::__private::core::iter::Extend;

                let mut result = Self::empty();
                result.extend(iterator);
                result
            }
        }
    };
}

/// Implement constants on the public (user-facing) bitflags type.
#[macro_export]
#[doc(hidden)]
macro_rules! __impl_public_bitflags_consts {
    (
        $(#[$outer:meta])*
        $PublicBitFlags:ident: $T:ty {
            $(
                $(#[$inner:ident $($args:tt)*])*
                const $Flag:tt = $value:expr;
            )*
        }
    ) => {
        $(#[$outer])*
        impl $PublicBitFlags {
            $(
                $crate::__bitflags_flag!({
                    name: $Flag,
                    named: {
                        $(#[$inner $($args)*])*
                        #[allow(
                            deprecated,
                            non_upper_case_globals,
                        )]
                        pub const $Flag: Self = Self::from_bits_retain($value);
                    },
                    unnamed: {},
                });
            )*
        }

        $(#[$outer])*
        impl $crate::Flags for $PublicBitFlags {
            const FLAGS: &'static [$crate::Flag<$PublicBitFlags>] = &[
                $(
                    $crate::__bitflags_flag!({
                        name: $Flag,
                        named: {
                            $crate::__bitflags_expr_safe_attrs!(
                                $(#[$inner $($args)*])*
                                {
                                    #[allow(
                                        deprecated,
                                        non_upper_case_globals,
                                    )]
                                    $crate::Flag::new($crate::__private::core::stringify!($Flag), $PublicBitFlags::$Flag)
                                }
                            )
                        },
                        unnamed: {
                            $crate::__bitflags_expr_safe_attrs!(
                                $(#[$inner $($args)*])*
                                {
                                    #[allow(
                                        deprecated,
                                        non_upper_case_globals,
                                    )]
                                    $crate::Flag::new("", $PublicBitFlags::from_bits_retain($value))
                                }
                            )
                        },
                    }),
                )*
            ];

            type Bits = $T;

            fn bits(&self) -> $T {
                $PublicBitFlags::bits(self)
            }

            fn from_bits_retain(bits: $T) -> $PublicBitFlags {
                $PublicBitFlags::from_bits_retain(bits)
            }
        }
    };
}

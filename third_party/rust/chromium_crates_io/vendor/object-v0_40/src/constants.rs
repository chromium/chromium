// Make macros available without import.
// This simplifies the recursive macro usage. Without it we would either need to use
// full paths in the recursive calls or import the macros manually.
#![macro_use]
#![cfg_attr(not(feature = "read"), allow(unused_macros))]

#[cfg(feature = "names")]
use core::ops::{BitAnd, Not};

/// Represents a newtype that wraps a primitive value.
///
/// This allows operations on the inner value of the newtypes.
///
/// This is also used as a bound on the endian-encoded types to automatically convert them to
/// and from newtypes if needed.
pub trait Wrap {
    /// The type of the inner value.
    type Inner;

    /// Constructs `Self` from an inner value.
    fn from_inner(inner: Self::Inner) -> Self;

    /// Consumes `self`, returning the inner value.
    fn into_inner(self) -> Self::Inner;
}

macro_rules! wrap {
    ($outer:ident, $inner:ident) => {
        impl crate::constants::Wrap for $outer {
            type Inner = $inner;
            fn into_inner(self) -> $inner {
                self.0
            }
            fn from_inner(inner: $inner) -> Self {
                Self(inner)
            }
        }
    };
    ($primitive:ident) => {
        impl crate::constants::Wrap for $primitive {
            type Inner = $primitive;
            fn into_inner(self) -> $primitive {
                self
            }
            fn from_inner(inner: $primitive) -> Self {
                inner
            }
        }
    };
}

wrap!(u8);
wrap!(u16);
wrap!(u32);
wrap!(u64);
wrap!(usize);
wrap!(i16);
wrap!(i32);
wrap!(i64);

/// The names and values for a set of constants with a given type.
#[cfg(feature = "names")]
#[derive(Debug, Default)]
pub struct ConstantNames<T: Wrap + 'static> {
    pub(crate) next: Option<&'static ConstantNames<T>>,
    pub(crate) entries: &'static [(T::Inner, &'static str)],
}

#[cfg(feature = "names")]
impl<T: Wrap> ConstantNames<T> {
    /// Get the name of the first constant with the given value.
    pub fn name(&self, value: T) -> Option<&'static str>
    where
        T::Inner: PartialEq,
    {
        let value = value.into_inner();
        let mut next = Some(self);
        while let Some(names) = next {
            for entry in names.entries {
                if entry.0 == value {
                    return Some(entry.1);
                }
            }
            next = names.next;
        }
        None
    }
}

/// A masked group of entries in a [`FlagNames`].
///
/// An entry is set when `(value & mask) == sub_value`
#[cfg(feature = "names")]
pub(crate) struct FlagGroup<T: Wrap> {
    pub(crate) mask: T::Inner,
    pub(crate) name: fn(T) -> Option<&'static str>,
}

#[cfg(feature = "names")]
impl<T: Wrap> core::fmt::Debug for FlagGroup<T>
where
    T::Inner: core::fmt::Debug,
{
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("FlagGroup")
            .field("mask", &self.mask)
            .field("name", &self.name)
            .finish()
    }
}

/// The names and values for flags in a bitfield.
///
/// Contains two kinds of entries:
/// - Independent bits: set when `(value & bit) == bit`
/// - Masked groups: set when `(value & mask) == sub_value`
#[cfg(feature = "names")]
#[derive(Default)]
pub struct FlagNames<T: Wrap + 'static> {
    pub(crate) next: Option<&'static FlagNames<T>>,
    /// Independent bit flags.
    pub(crate) bits: &'static [(T::Inner, &'static str)],
    /// Masked groups.
    pub(crate) groups: &'static [FlagGroup<T>],
}

#[cfg(feature = "names")]
impl<T: Wrap> core::fmt::Debug for FlagNames<T>
where
    T::Inner: core::fmt::Debug,
{
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("FlagNames")
            .field("next", &self.next)
            .field("bits", &self.bits)
            .field("groups", &self.groups)
            .finish()
    }
}

#[cfg(feature = "names")]
impl<T: Wrap> FlagNames<T>
where
    T::Inner: Copy + PartialEq + BitAnd<Output = T::Inner> + Not<Output = T::Inner>,
{
    /// Calls `f` for each named group or bit that matches `value`.
    ///
    /// Returns the remaining unmatched bits, or an error
    /// if `f` fails for any match.
    pub fn try_names<F, E>(&self, value: T, mut f: F) -> Result<T::Inner, E>
    where
        F: FnMut(T::Inner, &'static str) -> Result<(), E>,
    {
        let mut unmatched = value.into_inner();
        let mut next = Some(self);
        while let Some(names) = next {
            for group in names.groups {
                let masked = unmatched & group.mask;
                if let Some(name) = (group.name)(T::from_inner(masked)) {
                    f(masked, name)?;
                    unmatched = unmatched & !group.mask;
                }
            }
            next = names.next;
        }
        self.try_bit_names(T::from_inner(unmatched), f)
    }

    /// Calls `f` for each bit that matches `value`.
    ///
    /// Returns the remaining unmatched bits, or an error
    /// if `f` fails for any match.
    pub fn try_bit_names<F, E>(&self, value: T, mut f: F) -> Result<T::Inner, E>
    where
        F: FnMut(T::Inner, &'static str) -> Result<(), E>,
    {
        let mut unmatched = value.into_inner();
        let mut next = Some(self);
        while let Some(names) = next {
            for &(bit, name) in names.bits {
                if unmatched & bit == bit {
                    f(bit, name)?;
                    unmatched = unmatched & !bit;
                }
            }
            next = names.next;
        }
        Ok(unmatched)
    }

    /// Calls `f` for each named group or bit that matches `value`.
    ///
    /// Returns the remaining unmatched bits.
    pub fn names<F>(&self, value: T, mut f: F) -> T::Inner
    where
        F: FnMut(T::Inner, &'static str),
    {
        unwrap_infallible(self.try_names(value, |v, n| {
            f(v, n);
            Ok(())
        }))
    }

    /// Calls `f` for each bit that matches `value`.
    ///
    /// Returns the remaining unmatched bits.
    pub fn bit_names<F>(&self, value: T, mut f: F) -> T::Inner
    where
        F: FnMut(T::Inner, &'static str),
    {
        unwrap_infallible(self.try_bit_names(value, |v, n| {
            f(v, n);
            Ok(())
        }))
    }

    /// Find the first name that matches part of `value`.
    ///
    /// Returns the matched bits and the name.
    pub fn name(&self, value: T) -> Option<(T, &'static str)> {
        self.try_names(value, |v, n| Err((T::from_inner(v), n)))
            .err()
    }
}

#[cfg(feature = "names")]
fn unwrap_infallible<T>(r: Result<T, core::convert::Infallible>) -> T {
    match r {
        Ok(v) => v,
        Err(e) => match e {},
    }
}

#[cfg_attr(not(feature = "read"), allow(dead_code))]
#[cfg(feature = "names")]
pub(crate) fn flag_debug<T>(
    value: T,
    f: &mut core::fmt::Formatter<'_>,
    names: &FlagNames<T>,
) -> core::fmt::Result
where
    T: Wrap,
    T::Inner: core::fmt::LowerHex
        + Default
        + Copy
        + PartialEq
        + BitAnd<Output = T::Inner>
        + Not<Output = T::Inner>,
{
    let mut first = true;
    let unmatched = names.try_names(value, |_, name| {
        if !first {
            f.write_str(" | ")?;
        }
        first = false;
        f.write_str(name)
    })?;
    if unmatched != T::Inner::default() {
        if !first {
            f.write_str(" | ")?;
        }
        write!(f, "0x{:x}", unmatched)?;
    } else if first {
        write!(f, "0")?;
    }
    Ok(())
}

/// Define a set of related constant definitions, such as for an architecture.
///
/// Defines a struct with method `names` that returns a `struct Names` containing
/// fields set to the given definitions. The caller should have defined `struct Names`
/// with matching fields.
///
/// An optional parent struct can be specified to inherit definitions via `next` chaining.
/// For example, if `Base` is the parent then `consts name: type = { ... };` is expanded
/// similar to `constant_names!(name: type = Base::names_value().name + { ... })`.
/// The parent is often a set of constant definitions that are common to all architectures.
///
/// Usage:
/// ```text
/// names! {
///     struct Base;             // or: struct Arch(Base);
///     // Anonymous inline definitions.
///     consts name: type = { ... }; // Uses constant_names!()
///     flags name: type = { ... }; // Uses flag_names!()
///     // Reference constants defined elsewhere.
///     consts name = VAR;
///     flags name = VAR;
///     // Define module-level VAR and reference it. Optional doc comments and visibility.
///     /// Doc comment.
///     consts name = pub VAR: type = { ... };
///     flags name = VAR: type = { ... };
/// }
/// ```
macro_rules! names {
    ($(#[$meta:meta])* struct $struct:ident$(($parent:ident))?;
        $(
            $(#[$varname_meta:meta])*
            $kind:ident $method:ident
                $(= $vis:vis $varname:ident)?
                $(: $outer:ident$(($inner:ident))? = $body:tt)?;
        )*
    ) => {
        #[cfg(feature = "names")]
        $(#[$meta])*
        #[derive(Debug, Clone, Copy)]
        struct $struct;

        #[cfg(feature = "names")]
        impl $struct {
            const fn names_value() -> Names {
                #[allow(clippy::needless_update)]
                Names {
                    $($method: names!(@ref $method ($($varname)?) ($($body)?)),)*
                    $(..$parent::names_value())?
                }
            }

            // Not used for inheritance-only structs.
            #[allow(unused)]
            const fn names() -> &'static Names {
                static C: Names = $struct::names_value();
                &C
            }

            // Emit struct methods for anonymous inlines.
            names! { @impl_methods ($($parent)?) $(
                $kind $method ($($varname)?) ($($outer $($inner)?)?) ($($body)?)
            )* }
        }

        // Emit module-level statics for named inlines.
        names! { @statics ($($parent)?) $(
            $kind $method ($(#[$varname_meta])*) ($($vis $varname)?) ($($outer $($inner)?)?) ($($body)?)
        )* }

        // Emit module-level constants.
        $(names! { @consts $kind ($($outer $($inner)?)?) ($($body)?) })*
    };

    // Struct methods returning anonymous ConstantNames/FlagNames static.
    (@impl_methods $parent:tt $($kind:ident $method:ident $varname:tt $type:tt $body:tt)*) => {
        $(names! { @impl_method $kind $method $varname $type (names!(@impl_next $method $parent)) $body })*
    };
    (@impl_method $kind:ident $method:ident ($varname:ident) $type:tt $next:tt $body:tt) => {};
    (@impl_method consts $method:ident () $type:tt $next:tt ($body:tt)) => {
        const fn $method() -> &'static crate::constants::ConstantNames<newtype!(@type $type)> {
            constant_names! { @static () NAMES $type $next $body }
            &NAMES
        }
    };
    (@impl_method flags $method:ident () $type:tt $next:tt ($body:tt)) => {
        const fn $method() -> &'static crate::constants::FlagNames<newtype!(@type $type)> {
            flag_names! { @static () NAMES $type $next $body }
            &NAMES
        }
    };

    // Module-level ConstantNames/FlagNames statics.
    (@statics $parent:tt $($kind:ident $method:ident $meta:tt $varname:tt $type:tt $body:tt)*) => {
        $(names! { @static $kind $meta $varname $type (names!(@impl_next $method $parent)) $body })*
    };
    (@static $kind:ident () () $type:tt $next:tt $body:tt) => {};
    (@static $kind:ident () $varname:tt $type:tt $next:tt ()) => {};
    (@static consts $meta:tt ($vis:vis $varname:ident) $type:tt $next:tt ($body:tt)) => {
        constant_names! { @static $meta $vis $varname $type $next $body }
    };
    (@static flags $meta:tt ($vis:vis $varname:ident) $type:tt $next:tt ($body:tt)) => {
        flag_names! { @static $meta $vis $varname $type $next $body }
    };

    // Value of `ConstantNames::next` or `FlagNames::next`.
    (@impl_next $method:ident ()) => { None };
    (@impl_next $method:ident ($parent:ident)) => { Some($parent::names_value().$method) };

    // Value of a field in `Names`.
    // - Named (reference or inline): use the module-level static directly.
    // - Anonymous inline: call the const fn that hides the local NAMES static.
    // - Neither varname nor body: invalid.
    (@ref $method:ident ($varname:ident) $body:tt) => { &$varname };
    (@ref $method:ident () ($body:tt)) => { Self::$method() };
    (@ref $method:ident () ()) => {
        compile_error!(concat!(
            "`names!`: `", stringify!($method),
            "` must specify either a body `= { ... }` or a reference `= NAME`"
        ))
    };

    // `pub const` values if required.
    (@consts $kind:tt $type:tt ()) => {};
    (@consts consts $type:tt ($body:tt)) => { constant_names! { @consts $type $body } };
    (@consts flags $type:tt ($body:tt)) => { flag_names! { @consts $type $body } };
}

/// Create a static `ConstantNames` definition, and `pub const` definitions for the values.
///
/// Usage:
/// ```text
/// constant_names!(varname: type = { NAME = value, ... });
/// ```
///
/// Extend another `ConstantNames`:
/// ```text
/// constant_names!(varname: type = NAMES + { NAME = value, ... });
/// ```
macro_rules! constant_names {
    ($(#[$meta:meta])* $vis:vis $varname:ident: $outer:ident$(($inner:ident))? = $($next:ident +)? { $($body:tt)* }) => {
        constant_names! { @static ($(#[$meta])*) $vis $varname ($outer $($inner)?) (constant_names!(@next $($next)?)) { $($body)* } }
        constant_names! { @consts ($outer $($inner)?) { $($body)* } }
    };
    (@next) => { None };
    (@next $next:ident) => { Some(&$next) };
    (@static ($(#[$meta:meta])*) $vis:vis $varname:ident $type:tt ($next:expr) {
        $($(#[$entry_meta:meta])* $name:ident = $value:expr),* $(,)?
    }) => {
        $(#[$meta])*
        #[cfg(feature = "names")]
        $vis static $varname: crate::constants::ConstantNames<newtype!(@type $type)> = crate::constants::ConstantNames {
            next: $next,
            entries: &[$(($value, stringify!($name)),)*],
        };
    };
    (@consts $type:tt {
        $($(#[$meta:meta])* $name:ident = $value:expr),* $(,)?
    }) => {
        $($(#[$meta])* pub const $name: newtype!(@type $type) = newtype!(@value $type $value);)*
    };
}

/// Create a static `FlagNames` definition, and `pub const` definitions for the values.
///
/// Usage:
/// ```text
/// flag_names!(varname: type = { NAME = value, ... });
/// ```
///
/// Extend another `FlagNames`
/// ```text
/// flag_names!(varname: type = NAMES + { NAME = value, ... });
/// ```
///
/// Specify a subfield using a mask and a `ConstantNames` for the subfield values.
/// If a `FlagNames<T>` references a `ConstantNames<U>`, then it requires `U: From<T>`.
/// ```text
/// MASK_NAME = mask_value => NAMES,
/// ```
///
/// The mask name is optional:
/// ```text
/// _ = mask_value => NAMES,
/// ```
macro_rules! flag_names {
    ($(#[$meta:meta])* $vis:vis $varname:ident: $outer:ident$(($inner:ident))? = $($next:ident +)? { $($body:tt)* }) => {
        flag_names! { @static ($(#[$meta])*) $vis $varname ($outer $($inner)?) (flag_names!(@next $($next)?)) { $($body)* } }
        flag_names! { @consts ($outer $($inner)?) { $($body)* } }
    };
    (@next) => { None };
    (@next $next:ident) => { Some(&$next) };
    (@static ($(#[$meta:meta])*) $vis:vis $varname:ident $type:tt ($next:expr) { $($body:tt)* }) => {
        $(#[$meta])*
        #[cfg(feature = "names")]
        $vis static $varname: crate::constants::FlagNames<newtype!(@type $type)> = flag_names! {
            @build_static ($type $next) [] [] $($body)*
        };
    };
    (@consts $type:tt { $($body:tt)* }) => {
        flag_names! { @build_consts $type $($body)* }
    };

    // Terminal: emit the value
    (@build_static ($type:tt $next:expr) [$($bits:tt)*] [$($groups:tt)*]) => {
        crate::constants::FlagNames {
            next: $next,
            bits: &[$($bits)*],
            groups: &[$($groups)*],
        }
    };

    // Bit entry (NAME = VAL,)
    (@build_static ($type:tt $next:tt) [$($bits:tt)*] [$($groups:tt)*]
        $(#[$_meta:meta])* $name:ident = $value:expr,
        $($rest:tt)*
    ) => {
        flag_names! {
            @build_static ($type $next)
            [$($bits)* ($value, stringify!($name)),]
            [$($groups)*]
            $($rest)*
        }
    };

    // Group entry (NAME = MASK => { ... },)
    (@build_static ($type:tt $next:tt) [$($bits:tt)*] [$($groups:tt)*]
        $(#[$_meta:meta])* $name:ident = $value:expr => $entry:expr,
        $($rest:tt)*
    ) => {
        flag_names! {
            @build_static ($type $next)
            [$($bits)*]
            [$($groups)* (flag_names!(@flaggroup $value => $entry)),]
            $($rest)*
        }
    };

    // Nameless group entry (_ = MASK => { ... },)
    (@build_static ($type:tt $next:tt) [$($bits:tt)*] [$($groups:tt)*]
        $(#[$_meta:meta])* _ = $value:expr => $entry:expr,
        $($rest:tt)*
    ) => {
        flag_names! {
            @build_static ($type $next)
            [$($bits)*]
            [$($groups)* (flag_names!(@flaggroup $value => $entry)),]
            $($rest)*
        }
    };
    (@flaggroup $value:expr => $entry:expr) => {
        crate::constants::FlagGroup {
            mask: $value,
            name: |v| $entry.name(v.into()),
        }
    };

    // Terminal
    (@build_consts $type:tt) => {};

    // Bit entry (NAME = VAL,)
    (@build_consts $type:tt
        $(#[$meta:meta])* $name:ident = $value:expr,
        $($rest:tt)*
    ) => {
        $(#[$meta])* pub const $name: newtype!(@type $type) = newtype!(@value $type $value);
        flag_names! { @build_consts $type $($rest)* }
    };

    // Named group entry (NAME = MASK => { ... },)
    (@build_consts $type:tt
        $(#[$meta:meta])* $name:ident = $value:expr => $entry:expr,
        $($rest:tt)*
    ) => {
        $(#[$meta])* pub const $name: newtype!(@inner $type) = $value;
        flag_names! { @build_consts $type $($rest)* }
    };

    // Nameless group entry (_ = MASK => { ... },)
    (@build_consts $type:tt
        _ = $value:expr => $entry:expr,
        $($rest:tt)*
    ) => {
        flag_names! { @build_consts $type $($rest)* }
    };
}

macro_rules! newtype {
    ($(#[$meta:meta])* struct $outer:ident($inner:ident);) => {
        $(#[$meta])*
        #[derive(Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
        pub struct $outer(pub $inner);

        impl core::fmt::LowerHex for $outer {
            fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
                core::fmt::LowerHex::fmt(&self.0, f)
            }
        }

        impl core::fmt::UpperHex for $outer {
            fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
                core::fmt::UpperHex::fmt(&self.0, f)
            }
        }

        wrap!($outer, $inner);
    };

    // Newtype helpers for other macros.
    (@type ($outer:ident $inner:ident)) => { $outer };
    (@type ($type:ident)) => { $type };
    (@inner ($outer:ident $inner:ident)) => { $inner };
    (@inner ($type:ident)) => { $type };
    (@value ($outer:ident $inner:ident) $value:expr) => { $outer($value) };
    (@value ($type:ident) $value:expr) => { $value };
}

/// Create `pub const` definitions for newtype values.
///
/// Does not define a `ConstantNames` for the values. This is intended
/// for values that we don't need to print names for.
///
/// Usage:
/// ```text
/// newtype_consts!(type = { NAME = value, ... });
/// ```
macro_rules! newtype_consts {
    ($type:ident = {
        $($(#[$meta:meta])* $name:ident = $value:expr),* $(,)?
    }) => {
        $($(#[$meta])* pub const $name: $type = $type($value);)*
    };
}

/// Define primary constant names for a newtype.
///
/// Create `$varname` using `constant_names!`, and then define `pub const $outer::NAMES`
/// and `pub fn $outer::name`, as well as `Debug` and `Display` implementations.
macro_rules! newtype_constant_names {
    // No names.
    ($outer:ident($inner:ident) = {}) => {
        newtype_constant_names!(@impl $outer($inner));

        impl core::fmt::Debug for $outer {
            fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
                self.0.fmt(f)
            }
        }
    };
    ($varname:ident: $outer:ident($inner:ident) = $($next:ident +)? { $($body:tt)+ }) => {
        constant_names!($varname: $outer($inner) = $($next +)? { $($body)+ });
        newtype_constant_names!(@impl $outer($inner));

        #[cfg(feature = "names")]
        impl $outer {
            pub const NAMES: &'static crate::constants::ConstantNames<$outer> = &$varname;

            pub fn name(self) -> Option<&'static str> {
                $varname.name(self)
            }
        }

        impl core::fmt::Debug for $outer {
            fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
                #[cfg(feature = "names")]
                if let Some(name) = $varname.name(*self) {
                    return f.write_str(name);
                }
                self.0.fmt(f)
            }
        }
    };
    (@impl $outer:ident($inner:ident)) => {
        impl core::fmt::Display for $outer {
            fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
                core::fmt::Display::fmt(&self.0, f)
            }
        }
    };
}

/// Define primary flag names for a newtype.
///
/// Create `$varname` using `constant_names!`, and then define `pub const $outer::NAMES`
/// as well as `Debug` and `Display` implementations.
///
/// Also implement various methods and traits that are useful for working with flags.
macro_rules! newtype_flag_names {
    // No names.
    ($outer:ident($inner:ident) = {}) => {
        newtype_flag_names!(@impl $outer($inner));

        impl core::fmt::Debug for $outer {
            fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
                if self.0 == 0 {
                    write!(f, "0")
                } else {
                    write!(f, "0x{:x}", self.0)
                }
            }
        }
    };
    ($varname:ident: $outer:ident($inner:ident) = $($next:ident +)? { $($body:tt)+ }) => {
        flag_names!($varname: $outer($inner) = $($next +)? { $($body)+ });
        newtype_flag_names!(@impl $outer($inner));

        #[cfg(feature = "names")]
        impl $outer {
            pub const NAMES: &'static crate::constants::FlagNames<$outer> = &$varname;
        }

        impl core::fmt::Debug for $outer {
            fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
                #[cfg(feature = "names")]
                if true {
                    return crate::constants::flag_debug(*self, f, &$varname);
                }
                if self.0 == 0 {
                    write!(f, "0")
                } else {
                    write!(f, "0x{:x}", self.0)
                }
            }
        }
    };
    (@impl $outer:ident($inner:ident)) => {
        impl core::fmt::Display for $outer {
            fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
                core::fmt::Display::fmt(&self.0, f)
            }
        }

        impl core::ops::BitAnd for $outer {
            type Output = $outer;
            fn bitand(self, rhs: $outer) -> $outer {
                $outer(self.0 & rhs.0)
            }
        }

        impl core::ops::BitAndAssign for $outer {
            fn bitand_assign(&mut self, rhs: $outer) {
                self.0 &= rhs.0;
            }
        }

        impl core::ops::BitOr for $outer {
            type Output = $outer;
            fn bitor(self, rhs: $outer) -> $outer {
                $outer(self.0 | rhs.0)
            }
        }

        impl core::ops::BitOrAssign for $outer {
            fn bitor_assign(&mut self, rhs: $outer) {
                self.0 |= rhs.0;
            }
        }

        impl core::ops::BitXor for $outer {
            type Output = $outer;
            fn bitxor(self, rhs: $outer) -> $outer {
                $outer(self.0 ^ rhs.0)
            }
        }

        impl core::ops::BitXorAssign for $outer {
            fn bitxor_assign(&mut self, rhs: $outer) {
                self.0 ^= rhs.0;
            }
        }

        impl $outer {
            /// Returns true if all bits set in `other` are set in `self`.
            pub fn contains(self, other: $outer) -> bool {
                self.0 & other.0 == other.0
            }

            /// Returns true if any bit set in `other` is set in `self`.
            pub fn intersects(self, other: $outer) -> bool {
                self.0 & other.0 != 0
            }

            /// Returns self with the specified flags set.
            pub const fn with(self, other: $outer) -> Self {
                Self(self.0 | other.0)
            }

            /// Returns self with the specified flags cleared.
            pub const fn without(self, other: $outer) -> Self {
                Self(self.0 & !other.0)
            }

            /// Set the specified flags.
            pub fn insert(&mut self, other: $outer) {
                self.0 |= other.0;
            }

            /// Clear the specified flags.
            pub fn remove(&mut self, other: $outer) {
                self.0 &= !other.0;
            }
        }
    };
}

#[cfg(test)]
mod tests {
    #[cfg(feature = "names")]
    use super::{ConstantNames, FlagNames};

    /// Example macro usage.
    ///
    /// Expand with:
    /// cargo expand constants::tests --lib --tests --features names
    #[test]
    #[allow(unused)]
    fn macros() {
        newtype!(
            struct Foo(u32);
        );
        newtype_constant_names!(FOO: Foo(u32) = {
            FOO_A = 0,
            FOO_B = 1,
        });
        newtype!(
            struct Bar(u32);
        );
        newtype_flag_names!(BAR: Bar(u32) = {
            // Individual bits.
            BIT_0 = 0x1,
            BIT_1 = 0x2,
            BIT_3 = 0x4,
            // Named mask.
            BAR_MASK = 0xf0 => BAR_FIELD,
            // Unnamed mask.
            _ = 0xff00 => BAZ_FIELD,
        });
        constant_names!(BAR_FIELD_BASE: Bar(u32) = {
            BAR_A = 0x10,
            BAR_B = 0x20,
            BAR_C = 0x30,
        });
        constant_names!(BAR_FIELD: Bar(u32) = BAR_FIELD_BASE + {
            BAR_D = 0x40,
        });
        newtype!(
            struct Baz(u8);
        );
        newtype_constant_names!(BAZ_FIELD: Baz(u8) = {
            BAZ_A = 0x1,
            BAZ_B = 0x2,
        });
        impl From<Baz> for Bar {
            fn from(value: Baz) -> Self {
                Bar(u32::from(value.0) << 8)
            }
        }
        impl From<Bar> for Baz {
            fn from(value: Bar) -> Self {
                Baz((value.0 >> 8) as u8)
            }
        }
        #[cfg(feature = "names")]
        struct Names {
            foo: &'static ConstantNames<Foo>,
            bar: &'static FlagNames<Bar>,
            quux: &'static ConstantNames<u64>,
        }
        names! {
            struct Base;
            consts foo = FOO;
            flags bar = BAR;
            // Inline constant definitions.
            consts quux: u64 = {
                QUUX_A = 1,
                QUUX_B = 1,
            };
        }
        names! {
            struct Arch(Base);
            // Does not inherit from Base::foo directly
            // (but the FOO_ARCH definition below does inherit FOO).
            consts foo = FOO_ARCH;
            // BAR_ARCH is a module-level static that inherits names from Base::bar.
            flags bar = BAR_ARCH: Bar(u32) = {
                BIT_4 = 0x8,
            };
        }
        constant_names!(FOO_ARCH: Foo(u32) = FOO + {
            FOO_ARCH_A = 100,
        });

        #[cfg(feature = "names")]
        {
            let names = Arch::names();
            assert_eq!(names.foo.name(FOO_A), Some("FOO_A"));
            assert_eq!(names.foo.name(FOO_ARCH_A), Some("FOO_ARCH_A"));
            assert_eq!(names.bar.name(BIT_1), Some((BIT_1, "BIT_1")));
            assert_eq!(names.bar.name(BIT_4), Some((BIT_4, "BIT_4")));
            assert_eq!(names.bar.name(BIT_1 | BIT_4), Some((BIT_4, "BIT_4")));
            assert_eq!(names.bar.name(BAR_B), Some((BAR_B, "BAR_B")));
            assert_eq!(names.bar.name(BAR_D), Some((BAR_D, "BAR_D")));
            assert_eq!(names.bar.name(BAZ_B.into()), Some((BAZ_B.into(), "BAZ_B")));
        }
    }
}

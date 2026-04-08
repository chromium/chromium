//! Formatting-related items
//!
//! Panic formatting for custom types can be done in these ways
//! (in increasing order of verbosity):
//! - Using the [`PanicFmt` derive] macro
//! (requires the opt-in `"derive"` feature)
//! - Using the [`impl_panicfmt`] macro
//! (requires the default-enabled `"non_basic"` feature)
//! - Using the [`flatten_panicvals`] macro
//! (requires the default-enabled `"non_basic"` feature)
//! - Manually implementing the [`PanicFmt`] trait as described in its docs.
//!
//! [`PanicFmt` derive]: derive@crate::PanicFmt
//! [`PanicFmt`]: trait@crate::fmt::PanicFmt
//! [`impl_panicfmt`]: crate::impl_panicfmt
//! [`flatten_panicvals`]: crate::flatten_panicvals

#[cfg(feature = "non_basic")]
mod non_basic_fmt;

#[cfg(feature = "non_basic")]
mod fmt_compressed;

pub mod char_formatting;

#[cfg(feature = "non_basic")]
pub use self::{fmt_compressed::PackedFmtArg, non_basic_fmt::*};

use crate::wrapper::StdWrapper;

use core::marker::PhantomData;

use typewit::{Identity, TypeEq};

/// Trait for types that can be formatted by const panics.
///
/// # Implementor
///
/// Implementors are expected to also define this inherent method to format the type:
/// ```rust
/// # use const_panic::fmt::{FmtArg, IsCustomType, PanicFmt};
/// # use const_panic::PanicVal;
/// # struct Foo;
/// # impl Foo {
/// const fn to_panicvals<'a>(&'a self, f: FmtArg) -> [PanicVal<'a>; <Self as PanicFmt>::PV_COUNT]
/// # { loop{} }
/// # }
/// # impl PanicFmt for Foo {
/// #   type This = Self;
/// #   type Kind = IsCustomType;
/// #   const PV_COUNT: usize = 1;
/// # }
/// ```
/// The returned [`PanicVal`](crate::PanicVal) can also be `PanicVal<'static>`.
///
/// # Implementation examples
///
/// This trait can be implemented in these ways (in increasing order of verbosity):
/// - Using the [`PanicFmt` derive] macro
/// (requires the opt-in `"derive"` feature)
/// - Using the [`impl_panicfmt`](impl_panicfmt#examples) macro
/// (requires the default-enabled `"non_basic"` feature)
/// - Using the [`flatten_panicvals`](flatten_panicvals#examples) macro
/// (requires the default-enabled `"non_basic"` feature)
/// - Using no macros at all
///
/// ### Macro-less impl
///
/// Implementing this trait for a simple enum without using macros.
///
#[cfg_attr(feature = "non_basic", doc = "```rust")]
#[cfg_attr(not(feature = "non_basic"), doc = "```ignore")]
/// use const_panic::{ArrayString, FmtArg, PanicFmt, PanicVal};
///
/// // `ArrayString` requires the "non_basic" crate feature (enabled by default),
/// // everything else in this example works with no enabled crate features.
/// assert_eq!(
///     ArrayString::<99>::concat_panicvals(&[
///         &Foo::Bar.to_panicvals(FmtArg::DEBUG),
///         &[PanicVal::write_str(",")],
///         &Foo::Baz.to_panicvals(FmtArg::DEBUG),
///         &[PanicVal::write_str(",")],
///         &Foo::Qux.to_panicvals(FmtArg::DEBUG),
///     ]).unwrap(),
///     "Bar,Baz,Qux",
/// );
///
///
/// enum Foo {
///     Bar,
///     Baz,
///     Qux,
/// }
///
/// impl PanicFmt for Foo {
///     type This = Self;
///     type Kind = const_panic::IsCustomType;
///     const PV_COUNT: usize = 1;
/// }
///
/// impl Foo {
///     pub const fn to_panicvals(self, _: FmtArg) -> [PanicVal<'static>; Foo::PV_COUNT] {
///         match self {
///             Self::Bar => [PanicVal::write_str("Bar")],
///             Self::Baz => [PanicVal::write_str("Baz")],
///             Self::Qux => [PanicVal::write_str("Qux")],
///         }
///     }
/// }
///
/// ```
/// [`PanicFmt` derive]: derive@crate::PanicFmt
/// [`PanicFmt`]: trait@crate::fmt::PanicFmt
/// [`impl_panicfmt`]: crate::impl_panicfmt
/// [`flatten_panicvals`]: crate::flatten_panicvals
pub trait PanicFmt {
    /// The type after dereferencing all references.
    ///
    /// User-defined types should generally set this to `Self`.
    type This: ?Sized;
    /// Whether this is a user-defined type or standard library type.
    ///
    /// User-defined types should generally set this to [`IsCustomType`].
    type Kind;

    /// The length of the array returned in `Self::to_panicvals`
    /// (an inherent method that formats the type for panic messages).
    const PV_COUNT: usize;

    /// A marker type that proves that `Self` implements `PanicFmt`.
    ///
    /// Used by const_panic macros to coerce both standard library and
    /// user-defined types into some type that has a `to_panicvals` method.
    const PROOF: IsPanicFmt<Self, Self::This, Self::Kind> = IsPanicFmt::NEW;
}

impl<'a, T: PanicFmt + ?Sized> PanicFmt for &'a T {
    type This = T::This;
    type Kind = T::Kind;
    const PV_COUNT: usize = T::PV_COUNT;
}

/// Marker type used as the [`PanicFmt::Kind`] associated type for std types.
pub struct IsStdType;

/// Marker type used as the [`PanicFmt::Kind`] for user-defined types.
pub struct IsCustomType;

/// A marker type that proves that `S` implements
/// [`PanicFmt<This = T, Kind = K>`](PanicFmt).
///
/// Used by const_panic macros to coerce both standard library and
/// user-defined types into some type that has a `to_panicvals` method.
///
pub struct IsPanicFmt<S: ?Sized, T: ?Sized, K> {
    self_: PhantomData<fn() -> S>,
    this: PhantomData<fn() -> T>,
    kind: PhantomData<fn() -> K>,
    _priv: (),
}

impl<T: PanicFmt + ?Sized> IsPanicFmt<T, T::This, T::Kind> {
    /// Constucts an `IsPanicFmt`
    pub const NEW: Self = Self {
        self_: PhantomData,
        this: PhantomData,
        kind: PhantomData,
        _priv: (),
    };
}

impl<S: ?Sized, T: ?Sized, K> IsPanicFmt<S, T, K> {
    /// Infers the `S` type parameter with the argument.
    ///
    /// Because the only ways to construct `IsPanicFmt`
    /// use `IsPanicFmt<S, S::This, S::Kind>`,
    /// the other type parameters are inferred along with `S`.
    pub const fn infer(self, _: &S) -> Self {
        self
    }

    /// For coercing `&T` to `StdWrapper<&T>`.
    pub const fn coerce<'a>(self, x: &'a T) -> CoerceReturnOutput<&'a T, K>
    where
        // hack to make this bound work in 1.57.0:
        // K: CoerceReturn<&'a T>,
        // (before trait bounds were officially supported)
        <K as Identity>::Type: CoerceReturn<&'a T>,
    {
        match <K as CoerceReturn<&'a T>>::__COERCE_TO_WITNESS {
            __CoerceToWitness::IsStdType(te) => te.to_left(StdWrapper(x)),
            __CoerceToWitness::IsCustomType(te) => te.to_left(x),
        }
    }
}

impl<S: ?Sized, T: ?Sized, K> Copy for IsPanicFmt<S, T, K> {}
impl<S: ?Sized, T: ?Sized, K> Clone for IsPanicFmt<S, T, K> {
    fn clone(&self) -> Self {
        *self
    }
}

/////////////////////////////////////////////////////////////////////

/// Computes the type that the `T` argument is converted into by [`IsPanicFmt::coerce`].
pub type CoerceReturnOutput<T, K> = <K as CoerceReturn<T>>::CoerceTo;

/// Computes the type that the `T` argument is converted into by [`IsPanicFmt::coerce`].
///
/// This trait is sealed, it's implemented by [`IsStdType`] and [`IsCustomType`],
/// and cannot be implemented by any other type.
pub trait CoerceReturn<T>: Sized {
    /// The type that the `T` argument is converted into.
    type CoerceTo;

    #[doc(hidden)]
    const __COERCE_TO_WITNESS: __CoerceToWitness<T, Self>;
}

impl<T> CoerceReturn<T> for IsStdType {
    type CoerceTo = StdWrapper<T>;

    #[doc(hidden)]
    const __COERCE_TO_WITNESS: __CoerceToWitness<T, Self> =
        __CoerceToWitness::IsStdType(TypeEq::NEW);
}
impl<T> CoerceReturn<T> for IsCustomType {
    type CoerceTo = T;

    #[doc(hidden)]
    const __COERCE_TO_WITNESS: __CoerceToWitness<T, Self> =
        __CoerceToWitness::IsCustomType(TypeEq::NEW);
}

#[doc(hidden)]
pub enum __CoerceToWitness<T, K: CoerceReturn<T>> {
    #[non_exhaustive]
    IsStdType(TypeEq<<K as CoerceReturn<T>>::CoerceTo, StdWrapper<T>>),

    #[non_exhaustive]
    IsCustomType(TypeEq<<K as CoerceReturn<T>>::CoerceTo, T>),
}

/////////////////////////////////////////////////////////////////////

/// Carries all of the configuration for formatting functions.
///
/// # Example
///
#[cfg_attr(feature = "non_basic", doc = "```rust")]
#[cfg_attr(not(feature = "non_basic"), doc = "```ignore")]
/// use const_panic::{ArrayString, FmtArg, StdWrapper};
///
/// // `StdWrapper` wraps references to std types to provide their `to_panicvals` methods
/// const ARRAY: &[&str] = &["3", "foo\nbar", "\0qux"];
///
/// // Debug formatting
/// assert_eq!(
///     const_panic::concat_!(FmtArg::DEBUG; ARRAY),
///     r#"["3", "foo\nbar", "\x00qux"]"#
/// );
///
/// // Alternate-Debug formatting
/// assert_eq!(
///     const_panic::concat_!(FmtArg::ALT_DEBUG; ARRAY),
///     concat!(
///         "[\n",
///         "    \"3\",\n",
///         "    \"foo\\nbar\",\n",
///         "    \"\\x00qux\",\n",
///         "]",
///     )
/// );
///
/// // Display formatting
/// assert_eq!(
///     const_panic::concat_!(FmtArg::DISPLAY; ARRAY),
///     "[3, foo\nbar, \x00qux]"
/// );
///
/// // Alternate-Display formatting
/// assert_eq!(
///     const_panic::concat_!(FmtArg::ALT_DISPLAY; ARRAY),
///     concat!(
///         "[\n",
///         "    3,\n",
///         "    foo\n",
///         "bar,\n",
///         "    \x00qux,\n",
///         "]",
///     )
/// );
///
/// ```
#[derive(Debug, Copy, Clone, PartialEq)]
pub struct FmtArg {
    /// How much indentation is needed for a field/array element.
    ///
    /// Indentation is used by [`fmt::Delimiter`](crate::fmt::Delimiter)
    /// and by [`fmt::Separator`](crate::fmt::Separator),
    /// when the [`is_alternate` field](#structfield.is_alternate) flag is enabled.
    pub indentation: u8,
    /// Whether alternate formatting is being used.
    pub is_alternate: bool,
    /// Whether this is intended to be `Display` or `Debug` formatted.
    pub fmt_kind: FmtKind,
    /// What integers are formatted as: decimal, hexadecimal, or binary.
    pub number_fmt: NumberFmt,
}

impl FmtArg {
    /// A `FmtArg` with no indentation and `Display` formatting.
    pub const DISPLAY: Self = Self {
        indentation: 0,
        fmt_kind: FmtKind::Display,
        is_alternate: false,
        number_fmt: NumberFmt::Decimal,
    };

    /// A `FmtArg` with alternate `Display` formatting, starting with no indentation.
    pub const ALT_DISPLAY: Self = Self::DISPLAY.set_alternate(true);

    /// A `FmtArg` with `Debug` formatting and no indentation.
    pub const DEBUG: Self = Self::DISPLAY.set_debug();

    /// A `FmtArg` with alternate `Debug` formatting, starting with no indentation.
    pub const ALT_DEBUG: Self = Self::DEBUG.set_alternate(true);

    /// A `FmtArg` with `Debug` and `Binary` formatting and no indentation.
    pub const BIN: Self = Self::DISPLAY.set_bin();

    /// A `FmtArg` with alternate `Debug` and `Binary` formatting,
    /// starting with no indentation.
    pub const ALT_BIN: Self = Self::BIN.set_alternate(true);

    /// A `FmtArg` with `Debug` and `Hexadecimal` formatting and no indentation.
    pub const HEX: Self = Self::DISPLAY.set_hex();

    /// A `FmtArg` with alternate `Debug` and `Hexadecimal` formatting,
    /// starting with no indentation.
    pub const ALT_HEX: Self = Self::HEX.set_alternate(true);

    /// Sets whether alternate formatting is enabled
    pub const fn set_alternate(mut self, is_alternate: bool) -> Self {
        self.is_alternate = is_alternate;
        self
    }

    /// Changes the formatting to `Display`.
    pub const fn set_display(mut self) -> Self {
        self.fmt_kind = FmtKind::Display;
        self
    }

    /// Changes the formatting to `Debug`.
    pub const fn set_debug(mut self) -> Self {
        self.fmt_kind = FmtKind::Debug;
        self
    }

    /// Changes the formatting to `Debug`, and number formatting to `Hexadecimal`.
    pub const fn set_hex(mut self) -> Self {
        self.fmt_kind = FmtKind::Debug;
        self.number_fmt = NumberFmt::Hexadecimal;
        self
    }

    /// Changes the formatting to `Debug`, and number formatting to `Binary`.
    pub const fn set_bin(mut self) -> Self {
        self.fmt_kind = FmtKind::Debug;
        self.number_fmt = NumberFmt::Binary;
        self
    }
}

#[cfg(feature = "non_basic")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "non_basic")))]
impl FmtArg {
    /// Increments the indentation by [`INDENTATION_STEP`] spaces.
    pub const fn indent(mut self) -> Self {
        self.indentation += INDENTATION_STEP;
        self
    }

    /// Decrement the indentation by [`INDENTATION_STEP`] spaces.
    pub const fn unindent(mut self) -> Self {
        self.indentation = self.indentation.saturating_sub(INDENTATION_STEP);
        self
    }
}

////////////////////////////////////////////////////////////////////////////////

/// What kind of formatting to do, either `Display` or `Debug`.
#[non_exhaustive]
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum FmtKind {
    /// `Debug` formatting
    Debug = 0,
    /// `Display` formatting
    Display = 1,
}

////////////////////////////////////////////////////////////////////////////////

/// What integers are formatted as.
#[non_exhaustive]
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum NumberFmt {
    /// Formatted as decimal.
    Decimal = 0,
    /// Formatted as binary, eg: `101`, `0b110`.
    Binary = 1,
    /// Formatted as hexadecimal, eg: `FAD`, `0xDE`.
    Hexadecimal = 2,
}

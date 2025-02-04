// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::Error;
use writeable::{Part, TryWriteable};

#[cfg(feature = "alloc")]
use alloc::{borrow::Cow, boxed::Box};

/// A borrowed item in a [`Pattern`]. Items are either string literals or placeholders.
///
/// [`Pattern`]: crate::Pattern
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
#[allow(clippy::exhaustive_enums)] // Part of core data model
pub enum PatternItem<'a, T> {
    /// A placeholder of the type specified on this [`PatternItem`].
    Placeholder(T),
    /// A string literal. This can occur in one of three places:
    ///
    /// 1. Between the start of the string and the first placeholder (prefix)
    /// 2. Between two placeholders (infix)
    /// 3. Between the final placeholder and the end of the string (suffix)
    Literal(&'a str),
}

/// A borrowed-or-owned item in a [`Pattern`]. Items are either string literals or placeholders.
///
/// ✨ *Enabled with the `alloc` Cargo feature.*
///
/// [`Pattern`]: crate::Pattern
#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
#[allow(clippy::exhaustive_enums)] // Part of core data model
#[cfg(feature = "alloc")]
#[cfg_attr(feature = "serde", derive(serde::Deserialize, serde::Serialize))]
pub enum PatternItemCow<'a, T> {
    /// A placeholder of the type specified on this [`PatternItemCow`].
    Placeholder(T),
    /// A string literal. This can occur in one of three places:
    ///
    /// 1. Between the start of the string and the first placeholder (prefix)
    /// 2. Between two placeholders (infix)
    /// 3. Between the final placeholder and the end of the string (suffix)
    #[cfg_attr(feature = "serde", serde(borrow))]
    Literal(Cow<'a, str>),
}

#[cfg(feature = "alloc")]
impl<'a, T, U> From<PatternItem<'a, U>> for PatternItemCow<'a, T>
where
    T: From<U>,
{
    fn from(value: PatternItem<'a, U>) -> Self {
        match value {
            PatternItem::Placeholder(t) => Self::Placeholder(t.into()),
            PatternItem::Literal(s) => Self::Literal(Cow::Borrowed(s)),
        }
    }
}

/// Types that implement backing data models for [`Pattern`] implement this trait.
///
/// The trait has no public methods and is not implementable outside of this crate.
///
/// [`Pattern`]: crate::Pattern
// Debug so that `#[derive(Debug)]` on types generic in `PatternBackend` works
pub trait PatternBackend: crate::private::Sealed + 'static + core::fmt::Debug {
    /// The type to be used as the placeholder key in code.
    type PlaceholderKey<'a>;

    /// Cowable version of the type to be used as the placeholder key in code.
    // Note: it is not good practice to feature-gate trait methods, but this trait is sealed
    #[cfg(feature = "alloc")]
    type PlaceholderKeyCow<'a>;

    /// The type of error that the [`TryWriteable`] for this backend can return.
    type Error<'a>;

    /// The unsized type of the store required for this backend, usually `str` or `[u8]`.
    #[doc(hidden)] // TODO(#4467): Should be internal
    type Store: ?Sized + PartialEq + core::fmt::Debug;

    /// The iterator type returned by [`Self::try_from_items`].
    #[doc(hidden)] // TODO(#4467): Should be internal
    type Iter<'a>: Iterator<Item = PatternItem<'a, Self::PlaceholderKey<'a>>>;

    /// Checks a store for validity, returning an error if invalid.
    #[doc(hidden)] // TODO(#4467): Should be internal
    fn validate_store(store: &Self::Store) -> Result<(), Error>;

    /// Constructs a store from pattern items.
    #[doc(hidden)]
    // TODO(#4467): Should be internal
    // Note: it is not good practice to feature-gate trait methods, but this trait is sealed
    #[cfg(feature = "alloc")]
    fn try_from_items<
        'cow,
        'ph,
        I: Iterator<Item = Result<PatternItemCow<'cow, Self::PlaceholderKeyCow<'ph>>, Error>>,
    >(
        items: I,
    ) -> Result<Box<Self::Store>, Error>;

    /// Iterates over the pattern items in a store.
    #[doc(hidden)] // TODO(#4467): Should be internal
    fn iter_items(store: &Self::Store) -> Self::Iter<'_>;

    /// The store for the empty pattern, used to implement `Default`
    #[doc(hidden)] // TODO(#4467): Should be internal
    fn empty() -> &'static Self::Store;
}

/// Default annotation for the literal portion of a pattern.
///
/// For more information, see [`PlaceholderValueProvider`]. For an example, see [`Pattern`].
///
/// [`Pattern`]: crate::Pattern
pub const PATTERN_LITERAL_PART: Part = Part {
    category: "pattern",
    value: "literal",
};

/// Default annotation for the placeholder portion of a pattern.
///
/// For more information, see [`PlaceholderValueProvider`]. For an example, see [`Pattern`].
///
/// [`Pattern`]: crate::Pattern
pub const PATTERN_PLACEHOLDER_PART: Part = Part {
    category: "pattern",
    value: "placeholder",
};

/// Trait implemented on collections that can produce [`TryWriteable`]s for interpolation.
///
/// This trait determines the [`Part`]s produced by the writeable. In this crate, implementations
/// of this trait default to using [`PATTERN_LITERAL_PART`] and [`PATTERN_PLACEHOLDER_PART`].
pub trait PlaceholderValueProvider<K> {
    type Error;

    type W<'a>: TryWriteable<Error = Self::Error>
    where
        Self: 'a;

    const LITERAL_PART: Part;

    /// Returns the [`TryWriteable`] to substitute for the given placeholder
    /// and the [`Part`] representing it.
    fn value_for(&self, key: K) -> (Self::W<'_>, Part);
}

impl<'b, K, T> PlaceholderValueProvider<K> for &'b T
where
    T: PlaceholderValueProvider<K> + ?Sized,
{
    type Error = T::Error;
    type W<'a>
        = T::W<'a>
    where
        T: 'a,
        'b: 'a;
    const LITERAL_PART: Part = T::LITERAL_PART;
    fn value_for(&self, key: K) -> (Self::W<'_>, Part) {
        (*self).value_for(key)
    }
}

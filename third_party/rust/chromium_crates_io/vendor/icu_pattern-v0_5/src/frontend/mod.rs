// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[cfg(feature = "databake")]
mod databake;
#[cfg(feature = "serde")]
pub(crate) mod serde;
#[cfg(feature = "zerovec")]
mod zerovec;

use crate::Error;
#[cfg(feature = "alloc")]
use crate::Parser;
#[cfg(feature = "alloc")]
use crate::ParserOptions;
use crate::common::*;
#[cfg(feature = "alloc")]
use alloc::{borrow::ToOwned, boxed::Box, string::String};
#[cfg(feature = "alloc")]
use core::str::FromStr;
use core::{convert::Infallible, fmt, marker::PhantomData};
use writeable::{PartsWrite, TryWriteable, Writeable, adapters::TryWriteableInfallibleAsWriteable};

/// A string pattern with placeholders.
///
/// There are 2 generic parameters: `Backend` and `Store`.
///
/// # Backend
///
/// This determines the nature of placeholders and serialized encoding of the pattern.
///
/// The following backends are available:
///
/// - [`SinglePlaceholder`] for patterns with up to one placeholder: `"{0} days ago"`
/// - [`DoublePlaceholder`] for patterns with up to two placeholders: `"{0} days, {1} hours ago"`
/// - [`MultiNamedPlaceholder`] for patterns with named placeholders: `"{name} sent you a message"`
///
/// # Format to Parts
///
/// [`Pattern`] propagates [`Part`]s from inner writeables. In addition, it supports annotating
/// [`Part`]s for individual literals or placeholders via the [`PlaceholderValueProvider`] trait.
///
/// # Examples
///
/// Interpolating a [`SinglePlaceholder`] pattern:
///
/// ```
/// use core::str::FromStr;
/// use icu_pattern::Pattern;
/// use icu_pattern::SinglePlaceholder;
/// use writeable::assert_writeable_eq;
///
/// let pattern = Pattern::<SinglePlaceholder>::try_from_str(
///     "Hello, {0}!",
///     Default::default(),
/// )
/// .unwrap();
///
/// assert_writeable_eq!(pattern.interpolate(["Alice"]), "Hello, Alice!");
/// ```
///
/// [`SinglePlaceholder`]: crate::SinglePlaceholder
/// [`DoublePlaceholder`]: crate::DoublePlaceholder
/// [`MultiNamedPlaceholder`]: crate::MultiNamedPlaceholder
/// [`Part`]: writeable::Part
#[cfg_attr(feature = "yoke", derive(yoke::Yokeable))]
#[repr(transparent)]
pub struct Pattern<B: PatternBackend> {
    _backend: PhantomData<B>,
    /// The encoded storage
    pub store: B::Store,
}

impl<B: PatternBackend> PartialEq for Pattern<B> {
    fn eq(&self, other: &Self) -> bool {
        self.store == other.store
    }
}

impl<B: PatternBackend> fmt::Debug for Pattern<B> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Pattern")
            .field("_backend", &self._backend)
            .field("store", &&self.store)
            .finish()
    }
}

impl<B: PatternBackend> Default for &'static Pattern<B> {
    fn default() -> Self {
        Pattern::from_ref_store_unchecked(B::empty())
    }
}

#[cfg(feature = "alloc")]
impl<B: PatternBackend> Default for Box<Pattern<B>>
where
    Box<B::Store>: From<&'static B::Store>,
{
    fn default() -> Self {
        Pattern::from_boxed_store_unchecked(Box::from(B::empty()))
    }
}

#[test]
fn test_defaults() {
    assert_eq!(
        Box::<Pattern::<crate::SinglePlaceholder>>::default(),
        Pattern::try_from_items(core::iter::empty()).unwrap()
    );
    assert_eq!(
        Box::<Pattern::<crate::DoublePlaceholder>>::default(),
        Pattern::try_from_items(core::iter::empty()).unwrap()
    );
    assert_eq!(
        Box::<Pattern::<crate::MultiNamedPlaceholder>>::default(),
        Pattern::try_from_items(core::iter::empty()).unwrap()
    );
}

#[cfg(feature = "alloc")]
impl<B: PatternBackend> ToOwned for Pattern<B>
where
    Box<B::Store>: for<'a> From<&'a B::Store>,
{
    type Owned = Box<Pattern<B>>;

    fn to_owned(&self) -> Self::Owned {
        Self::from_boxed_store_unchecked(Box::from(&self.store))
    }
}

#[cfg(feature = "alloc")]
impl<B: PatternBackend> Clone for Box<Pattern<B>>
where
    Box<B::Store>: for<'a> From<&'a B::Store>,
{
    fn clone(&self) -> Self {
        Pattern::from_boxed_store_unchecked(Box::from(&self.store))
    }
}

impl<B: PatternBackend> Pattern<B> {
    #[cfg(feature = "alloc")]
    pub(crate) fn from_boxed_store_unchecked(store: Box<B::Store>) -> Box<Self> {
        // Safety: Box::into_raw fulfils Box::from_raw's requirements, as Pattern<B> is
        // repr(transparent) over B::Store, and does not have further validity constraints
        unsafe { Box::from_raw(Box::into_raw(store) as *mut Self) }
    }

    #[doc(hidden)] // databake
    pub const fn from_ref_store_unchecked(store: &B::Store) -> &Self {
        // Safety: Pattern is repr(transparent) over B::Store
        unsafe { &*(store as *const B::Store as *const Self) }
    }

    #[doc(hidden)] // B::Store is doc(hidden)
    pub fn from_ref_store(store: &B::Store) -> Result<&Self, Error> {
        B::validate_store(store)?;
        Ok(Self::from_ref_store_unchecked(store))
    }
}

#[cfg(feature = "alloc")]
impl<B> Pattern<B>
where
    B: PatternBackend,
{
    /// Creates a pattern from an iterator of pattern items.
    ///
    /// ✨ *Enabled with the `alloc` Cargo feature.*
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_pattern::Pattern;
    /// use icu_pattern::PatternItemCow;
    /// use icu_pattern::SinglePlaceholder;
    /// use icu_pattern::SinglePlaceholderKey;
    /// use std::borrow::Cow;
    ///
    /// Pattern::<SinglePlaceholder>::try_from_items(
    ///     [
    ///         PatternItemCow::Placeholder(SinglePlaceholderKey::Singleton),
    ///         PatternItemCow::Literal(Cow::Borrowed(" days")),
    ///     ]
    ///     .into_iter(),
    /// )
    /// .expect("valid pattern items");
    /// ```
    pub fn try_from_items<'a, I>(items: I) -> Result<Box<Self>, Error>
    where
        I: Iterator<Item = PatternItemCow<'a, B::PlaceholderKeyCow<'a>>>,
    {
        let store = B::try_from_items(items.map(Ok))?;
        #[cfg(debug_assertions)]
        match B::validate_store(core::borrow::Borrow::borrow(&store)) {
            Ok(()) => (),
            Err(e) => {
                debug_assert!(false, "{e:?}");
            }
        };
        Ok(Self::from_boxed_store_unchecked(store))
    }
}

#[cfg(feature = "alloc")]
impl<'a, B> Pattern<B>
where
    B: PatternBackend,
    B::PlaceholderKeyCow<'a>: FromStr,
    <B::PlaceholderKeyCow<'a> as FromStr>::Err: fmt::Debug,
{
    /// Creates a pattern by parsing a syntax string.
    ///
    /// ✨ *Enabled with the `alloc` Cargo feature.*
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_pattern::Pattern;
    /// use icu_pattern::SinglePlaceholder;
    ///
    /// // Create a pattern from a valid string:
    /// Pattern::<SinglePlaceholder>::try_from_str("{0} days", Default::default())
    ///     .expect("valid pattern");
    ///
    /// // Error on an invalid pattern:
    /// Pattern::<SinglePlaceholder>::try_from_str("{0 days", Default::default())
    ///     .expect_err("mismatched braces");
    /// ```
    pub fn try_from_str(pattern: &str, options: ParserOptions) -> Result<Box<Self>, Error> {
        let parser = Parser::new(pattern, options);
        let store = B::try_from_items(parser)?;
        #[cfg(debug_assertions)]
        match B::validate_store(core::borrow::Borrow::borrow(&store)) {
            Ok(()) => (),
            Err(e) => {
                debug_assert!(false, "{e:?} for pattern {pattern:?}");
            }
        };
        Ok(Self::from_boxed_store_unchecked(store))
    }
}

impl<B> Pattern<B>
where
    B: PatternBackend,
{
    /// Returns an iterator over the [`PatternItem`]s in this pattern.
    pub fn iter(&self) -> impl Iterator<Item = PatternItem<'_, B::PlaceholderKey<'_>>> + '_ {
        B::iter_items(&self.store)
    }

    /// Returns a [`TryWriteable`] that interpolates items from the given replacement provider
    /// into this pattern string.
    pub fn try_interpolate<'a, P>(
        &'a self,
        value_provider: P,
    ) -> impl TryWriteable<Error = P::Error> + fmt::Display + 'a
    where
        P: PlaceholderValueProvider<B::PlaceholderKey<'a>> + 'a,
    {
        WriteablePattern::<B, P> {
            store: &self.store,
            value_provider,
        }
    }

    #[cfg(feature = "alloc")]
    /// Interpolates the pattern directly to a string, returning the string or an error.
    ///
    /// In addition to the error, the lossy fallback string is returned in the failure case.
    ///
    /// ✨ *Enabled with the `alloc` Cargo feature.*
    pub fn try_interpolate_to_string<'a, P>(
        &'a self,
        value_provider: P,
    ) -> Result<String, (P::Error, String)>
    where
        P: PlaceholderValueProvider<B::PlaceholderKey<'a>> + 'a,
    {
        self.try_interpolate(value_provider)
            .try_write_to_string()
            .map(|s| s.into_owned())
            .map_err(|(e, s)| (e, s.into_owned()))
    }

    /// Returns a [`Writeable`] that interpolates items from the given replacement provider
    /// into this pattern string.
    ///
    /// This is defined only on infallible inputs.
    pub fn interpolate<'a, P>(&'a self, value_provider: P) -> impl Writeable + fmt::Display + 'a
    where
        P: PlaceholderValueProvider<B::PlaceholderKey<'a>, Error = Infallible> + 'a,
    {
        TryWriteableInfallibleAsWriteable(WriteablePattern::<B, P> {
            store: &self.store,
            value_provider,
        })
    }

    #[cfg(feature = "alloc")]
    /// Interpolates the pattern directly to a string.
    ///
    /// This is defined only on infallible inputs.
    ///
    /// ✨ *Enabled with the `alloc` Cargo feature.*
    pub fn interpolate_to_string<'a, P>(&'a self, value_provider: P) -> String
    where
        P: PlaceholderValueProvider<B::PlaceholderKey<'a>, Error = Infallible> + 'a,
    {
        self.interpolate(value_provider)
            .write_to_string()
            .into_owned()
    }
}

struct WriteablePattern<'a, B: PatternBackend, P> {
    store: &'a B::Store,
    value_provider: P,
}

impl<'a, B, P> TryWriteable for WriteablePattern<'a, B, P>
where
    B: PatternBackend,
    P: PlaceholderValueProvider<B::PlaceholderKey<'a>>,
{
    type Error = P::Error;

    fn try_write_to_parts<S: PartsWrite + ?Sized>(
        &self,
        sink: &mut S,
    ) -> Result<Result<(), Self::Error>, fmt::Error> {
        let mut error = None;
        let it = B::iter_items(self.store);
        #[cfg(debug_assertions)]
        let (size_hint, mut actual_len) = (it.size_hint(), 0);
        for item in it {
            match item {
                PatternItem::Literal(s) => {
                    self.value_provider.map_literal(s).write_to_parts(sink)?;
                }
                PatternItem::Placeholder(key) => {
                    let element_writeable = self.value_provider.value_for(key);
                    if let Err(e) = element_writeable.try_write_to_parts(sink)? {
                        // Keep the first error if there was one
                        error.get_or_insert(e);
                    }
                }
            }
            #[cfg(debug_assertions)]
            {
                actual_len += 1;
            }
        }
        #[cfg(debug_assertions)]
        {
            debug_assert!(actual_len >= size_hint.0);
            if let Some(max_len) = size_hint.1 {
                debug_assert!(actual_len <= max_len);
            }
        }
        if let Some(e) = error {
            Ok(Err(e))
        } else {
            Ok(Ok(()))
        }
    }

    fn writeable_length_hint(&self) -> writeable::LengthHint {
        let mut length_hint = writeable::LengthHint::exact(0);
        let it = B::iter_items(self.store);
        for item in it {
            match item {
                PatternItem::Literal(s) => {
                    length_hint += self.value_provider.map_literal(s).writeable_length_hint();
                }
                PatternItem::Placeholder(key) => {
                    length_hint += self.value_provider.value_for(key).writeable_length_hint();
                }
            }
        }
        length_hint
    }
}

impl<'a, B, P> fmt::Display for WriteablePattern<'a, B, P>
where
    B: PatternBackend,
    P: PlaceholderValueProvider<B::PlaceholderKey<'a>>,
{
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // Discard the TryWriteable error (lossy mode)
        self.try_write_to(f).map(|_| ())
    }
}

/// A structure containing the extracted placeholder values.
///
/// Query it using [`Self::get()`].
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. If you find this useful, please comment on the
/// [tracking issue #8331](https://github.com/unicode-org/icu4x/issues/8331).
/// </div>
///
/// ✨ *Enabled with the `unstable` Cargo feature.*
#[cfg(feature = "unstable")]
pub struct PlaceholderMatches<'p, 'a, B: ExtractionBackend> {
    pub(crate) store: B::DecodedMatchesUnstable<'p, 'a>,
}

#[cfg(feature = "unstable")]
impl<'p, 'a, B: ExtractionBackend> PlaceholderMatches<'p, 'a, B> {
    /// Gets the matched substring for the given placeholder key.
    ///
    /// Returns `None` if the placeholder was not present in the pattern.
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. If you find this useful, please comment on the
    /// [tracking issue #8331](https://github.com/unicode-org/icu4x/issues/8331).
    /// </div>
    ///
    /// ✨ *Enabled with the `unstable` Cargo feature.*
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_pattern::{DoublePlaceholder, DoublePlaceholderKey, Pattern};
    ///
    /// let pattern = Pattern::<DoublePlaceholder>::try_from_str(
    ///     "Hello, {0} and {1}!",
    ///     Default::default(),
    /// )
    /// .unwrap();
    /// let matches = pattern.extract_values("Hello, Alice and Bob!").unwrap();
    /// assert_eq!(matches.get(DoublePlaceholderKey::Place0), Some("Alice"));
    /// assert_eq!(matches.get(DoublePlaceholderKey::Place1), Some("Bob"));
    /// ```
    pub fn get(&self, key: B::PlaceholderKey<'_>) -> Option<&'a str> {
        B::get_match_unstable(&self.store, key)
    }
}

#[cfg(feature = "unstable")]
impl<'p, B: ExtractionBackend> fmt::Debug for PlaceholderMatches<'p, '_, B>
where
    for<'a> B::DecodedMatchesUnstable<'p, 'a>: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("PlaceholderMatches")
            .field("store", &self.store)
            .finish()
    }
}

#[cfg(feature = "unstable")]
impl<'p, B: ExtractionBackend> PartialEq for PlaceholderMatches<'p, '_, B>
where
    for<'a> B::DecodedMatchesUnstable<'p, 'a>: PartialEq,
{
    fn eq(&self, other: &Self) -> bool {
        self.store == other.store
    }
}

#[cfg(feature = "unstable")]
impl<'p, B: ExtractionBackend> Eq for PlaceholderMatches<'p, '_, B> where
    for<'a> B::DecodedMatchesUnstable<'p, 'a>: Eq
{
}

#[cfg(feature = "unstable")]
impl<B: ExtractionBackend> Pattern<B> {
    /// Extracts placeholder values from the formatted string.
    ///
    /// Returns `None` if the input string does not match the pattern.
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. If you find this useful, please comment on the
    /// [tracking issue #8331](https://github.com/unicode-org/icu4x/issues/8331).
    /// </div>
    ///
    /// ✨ *Enabled with the `unstable` Cargo feature.*
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_pattern::{Pattern, SinglePlaceholder, SinglePlaceholderKey};
    ///
    /// let pattern = Pattern::<SinglePlaceholder>::try_from_str(
    ///     "Hello, {0}!",
    ///     Default::default(),
    /// )
    /// .unwrap();
    /// let matches = pattern.extract_values("Hello, Alice!").unwrap();
    /// assert_eq!(matches.get(SinglePlaceholderKey::Singleton), Some("Alice"));
    /// ```
    pub fn extract_values<'p, 'a>(
        &'p self,
        input: &'a str,
    ) -> Option<PlaceholderMatches<'p, 'a, B>> {
        B::extract_unstable(&self.store, input).map(|store| PlaceholderMatches { store })
    }
}

#[test]
fn test_try_from_str_inference() {
    use crate::SinglePlaceholder;
    let _: Box<Pattern<SinglePlaceholder>> =
        Pattern::try_from_str("{0} days", Default::default()).unwrap();
    let _ = Pattern::<SinglePlaceholder>::try_from_str("{0} days", Default::default()).unwrap();
}

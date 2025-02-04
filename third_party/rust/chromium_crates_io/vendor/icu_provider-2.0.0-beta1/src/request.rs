// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use alloc::borrow::Cow;
use alloc::borrow::ToOwned;
use alloc::boxed::Box;
use alloc::string::String;
use core::cmp::Ordering;
use core::default::Default;
use core::fmt;
use core::fmt::Debug;
use core::hash::Hash;
use core::ops::Deref;
use core::str::FromStr;
use icu_locale_core::extensions::unicode as unicode_ext;
use icu_locale_core::subtags::{Language, Region, Script, Subtag, Variant};
use icu_locale_core::{LanguageIdentifier, Locale, ParseError};
use zerovec::ule::VarULE;

use crate::fallback::LocaleFallbackPriority;
use crate::DataMarker;
use crate::DataMarkerInfo;

/// The request type passed into all data provider implementations.
#[derive(Default, Debug, Clone, Copy, PartialEq, Eq)]
#[allow(clippy::exhaustive_structs)] // this type is stable
pub struct DataRequest<'a> {
    /// The data identifier for which to load data.
    ///
    /// If locale fallback is enabled, the resulting data may be from a different identifier
    /// than the one requested here.
    pub id: DataIdentifierBorrowed<'a>,
    /// Metadata that may affect the behavior of the data provider.
    pub metadata: DataRequestMetadata,
}

/// Metadata for data requests. This is currently empty, but it may be extended with options
/// for tuning locale fallback, buffer layout, and so forth.
#[derive(Default, Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
#[non_exhaustive]
pub struct DataRequestMetadata {
    /// Silent requests do not log errors. This can be used for exploratory querying, such as fallbacks.
    pub silent: bool,
    /// Whether to allow prefix matches for the data marker attributes.
    pub attributes_prefix_match: bool,
}

/// The borrowed version of a [`DataIdentifierCow`].
#[derive(Default, Debug, Clone, Copy, PartialEq, Eq)]
#[non_exhaustive]
pub struct DataIdentifierBorrowed<'a> {
    /// Marker-specific request attributes
    pub marker_attributes: &'a DataMarkerAttributes,
    /// The CLDR locale
    pub locale: &'a DataLocale,
}

impl fmt::Display for DataIdentifierBorrowed<'_> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(self.locale, f)?;
        if !self.marker_attributes.is_empty() {
            write!(f, "/{}", self.marker_attributes.as_str())?;
        }
        Ok(())
    }
}

impl<'a> DataIdentifierBorrowed<'a> {
    /// Creates a [`DataIdentifierBorrowed`] for a borrowed [`DataLocale`].
    pub fn for_locale(locale: &'a DataLocale) -> Self {
        Self {
            locale,
            ..Default::default()
        }
    }

    /// Creates a [`DataIdentifierBorrowed`] for a borrowed [`DataMarkerAttributes`].
    pub fn for_marker_attributes(marker_attributes: &'a DataMarkerAttributes) -> Self {
        Self {
            marker_attributes,
            ..Default::default()
        }
    }

    /// Creates a [`DataIdentifierBorrowed`] for a borrowed [`DataMarkerAttributes`] and [`DataLocale`].
    pub fn for_marker_attributes_and_locale(
        marker_attributes: &'a DataMarkerAttributes,
        locale: &'a DataLocale,
    ) -> Self {
        Self {
            marker_attributes,
            locale,
        }
    }

    /// Converts this [`DataIdentifierBorrowed`] into a [`DataIdentifierCow<'static>`].
    pub fn into_owned(&self) -> DataIdentifierCow<'static> {
        DataIdentifierCow {
            marker_attributes: Cow::Owned(self.marker_attributes.to_owned()),
            locale: Cow::Owned(self.locale.clone()),
        }
    }

    /// Borrows this [`DataIdentifierBorrowed`] as a [`DataIdentifierCow<'a>`].
    pub fn as_cow(&self) -> DataIdentifierCow<'a> {
        DataIdentifierCow {
            marker_attributes: Cow::Borrowed(self.marker_attributes),
            locale: Cow::Borrowed(self.locale),
        }
    }
}

/// A data identifier identifies a particular version of data, such as "English".
///
/// It is a wrapper around a [`DataLocale`] and a [`DataMarkerAttributes`].
#[derive(Debug, PartialEq, Eq, Hash, Clone)]
#[non_exhaustive]
pub struct DataIdentifierCow<'a> {
    /// Marker-specific request attributes
    pub marker_attributes: Cow<'a, DataMarkerAttributes>,
    /// The CLDR locale
    pub locale: Cow<'a, DataLocale>,
}

impl PartialOrd for DataIdentifierCow<'_> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for DataIdentifierCow<'_> {
    fn cmp(&self, other: &Self) -> Ordering {
        (&self.marker_attributes, self.locale.as_tuple())
            .cmp(&(&other.marker_attributes, other.locale.as_tuple()))
    }
}

impl fmt::Display for DataIdentifierCow<'_> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(&*self.locale, f)?;
        if !self.marker_attributes.is_empty() {
            write!(f, "/{}", self.marker_attributes.as_str())?;
        }
        Ok(())
    }
}

impl<'a> DataIdentifierCow<'a> {
    /// Borrows this [`DataIdentifierCow`] as a [`DataIdentifierBorrowed<'a>`].
    pub fn as_borrowed(&'a self) -> DataIdentifierBorrowed<'a> {
        DataIdentifierBorrowed {
            marker_attributes: &self.marker_attributes,
            locale: &self.locale,
        }
    }

    /// Creates a [`DataIdentifierCow`] from an owned [`DataLocale`].
    pub fn from_locale(locale: DataLocale) -> Self {
        Self {
            marker_attributes: Cow::Borrowed(DataMarkerAttributes::empty()),
            locale: Cow::Owned(locale),
        }
    }

    /// Creates a [`DataIdentifierCow`] from a borrowed [`DataMarkerAttributes`].
    pub fn from_marker_attributes(marker_attributes: &'a DataMarkerAttributes) -> Self {
        Self {
            marker_attributes: Cow::Borrowed(marker_attributes),
            locale: Cow::Borrowed(Default::default()),
        }
    }

    /// Creates a [`DataIdentifierCow`] from an owned [`DataMarkerAttributes`].
    pub fn from_marker_attributes_owned(marker_attributes: Box<DataMarkerAttributes>) -> Self {
        Self {
            marker_attributes: Cow::Owned(marker_attributes),
            locale: Cow::Borrowed(Default::default()),
        }
    }

    /// Creates a [`DataIdentifierCow`] from an owned [`DataMarkerAttributes`] and an owned [`DataLocale`].
    pub fn from_owned(marker_attributes: Box<DataMarkerAttributes>, locale: DataLocale) -> Self {
        Self {
            marker_attributes: Cow::Owned(marker_attributes),
            locale: Cow::Owned(locale),
        }
    }

    /// Creates a [`DataIdentifierCow`] from a borrowed [`DataMarkerAttributes`] and an owned [`DataLocale`].
    pub fn from_borrowed_and_owned(
        marker_attributes: &'a DataMarkerAttributes,
        locale: DataLocale,
    ) -> Self {
        Self {
            marker_attributes: Cow::Borrowed(marker_attributes),
            locale: Cow::Owned(locale),
        }
    }

    /// Returns whether this id is equal to the default.
    pub fn is_default(&self) -> bool {
        self.marker_attributes.is_empty() && self.locale.is_default()
    }
}

impl Default for DataIdentifierCow<'_> {
    fn default() -> Self {
        Self {
            marker_attributes: Cow::Borrowed(Default::default()),
            locale: Cow::Borrowed(Default::default()),
        }
    }
}

/// A locale type optimized for use in fallbacking and the ICU4X data pipeline.
///
/// [`DataLocale`] contains less functionality than [`Locale`] but more than
/// [`LanguageIdentifier`] for better size and performance while still meeting
/// the needs of the ICU4X data pipeline.
///
/// You can create a [`DataLocale`] from a borrowed [`Locale`], which is more
/// efficient than cloning the [`Locale`], but less efficient than converting an owned
/// [`Locale`]:
///
/// ```
/// use icu_locale_core::locale;
/// use icu_provider::DataLocale;
///
/// let locale1 = locale!("en-u-ca-buddhist");
/// let data_locale = DataLocale::from(&locale1);
/// ```
///
/// [`DataLocale`] only supports `-u-sd` keywords, to reflect the current state of CLDR data
/// lookup and fallback. This may change in the future.
///
/// ```
/// use icu_locale_core::{locale, Locale};
/// use icu_provider::DataLocale;
///
/// let locale = "hi-IN-t-en-h0-hybrid-u-attr-ca-buddhist-sd-inas"
///     .parse::<Locale>()
///     .unwrap();
///
/// assert_eq!(
///     DataLocale::from(locale),
///     DataLocale::from(locale!("hi-IN-u-sd-inas"))
/// );
/// ```
#[derive(Clone, Default, Eq)]
pub struct DataLocale {
    /// Language subtag
    pub language: Language,
    /// Script subtag
    pub script: Option<Script>,
    /// Region subtag
    pub region: Option<Region>,
    /// Variant subtag
    pub variant: Option<Variant>,
    /// Subivision (-u-sd-) subtag
    pub subdivision: Option<Subtag>,

    // These are ignored by all methods/impls except for get_unicode_ext and get_single_unicode_ext
    // TODO(#3632): Remove after migrating all inputs to preferences
    keywords: unicode_ext::Keywords,
}

impl DataLocale {
    fn as_tuple(
        &self,
    ) -> (
        Language,
        Option<Script>,
        Option<Region>,
        Option<Variant>,
        Option<Subtag>,
    ) {
        (
            self.language,
            self.script,
            self.region,
            self.variant,
            self.subdivision,
        )
    }

    /// Returns a [`DataLocale`] usable for the marker `M`.
    pub const fn from_preferences_locale<M: DataMarker>(
        locale: icu_locale_core::preferences::LocalePreferences,
    ) -> Self {
        Self::from_preferences_with_info(locale, M::INFO)
    }

    pub(crate) const fn from_preferences_with_info(
        locale: icu_locale_core::preferences::LocalePreferences,
        info: DataMarkerInfo,
    ) -> Self {
        Self {
            language: locale.language,
            script: locale.script,
            region: match (locale.region, locale.ue_region) {
                (Some(_), Some(r))
                    if matches!(
                        info.fallback_config.priority,
                        LocaleFallbackPriority::Region
                    ) =>
                {
                    Some(r)
                }
                (r, _) => r,
            },
            variant: locale.variant,
            subdivision: locale.subdivision,

            keywords: unicode_ext::Keywords::new(),
        }
    }
}

impl PartialEq for DataLocale {
    fn eq(&self, other: &Self) -> bool {
        self.as_tuple() == other.as_tuple()
    }
}

impl Hash for DataLocale {
    fn hash<H: core::hash::Hasher>(&self, state: &mut H) {
        self.as_tuple().hash(state)
    }
}

impl DataLocale {
    /// `const` version of `Default::default`
    pub const fn default() -> Self {
        DataLocale {
            language: Language::UND,
            script: None,
            region: None,
            variant: None,
            subdivision: None,
            keywords: unicode_ext::Keywords::new(),
        }
    }
}

impl Default for &DataLocale {
    fn default() -> Self {
        static DEFAULT: DataLocale = DataLocale::default();
        &DEFAULT
    }
}

impl fmt::Debug for DataLocale {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "DataLocale{{{self}}}")
    }
}

icu_locale_core::impl_writeable_for_each_subtag_str_no_test!(DataLocale, selff, selff.script.is_none() && selff.region.is_none() && selff.variant.is_none() && selff.subdivision.is_none() => selff.language.write_to_string());

impl From<LanguageIdentifier> for DataLocale {
    fn from(langid: LanguageIdentifier) -> Self {
        Self {
            language: langid.language,
            script: langid.script,
            region: langid.region,
            variant: langid.variants.iter().copied().next(),
            subdivision: None,
            keywords: unicode_ext::Keywords::new(),
        }
    }
}

impl From<Locale> for DataLocale {
    fn from(mut locale: Locale) -> Self {
        let mut r = Self::from(locale.id);

        r.subdivision = locale
            .extensions
            .unicode
            .keywords
            .remove(unicode_ext::key!("sd"))
            .and_then(|v| v.as_single_subtag().copied());
        r.keywords = locale.extensions.unicode.keywords;
        r
    }
}

impl From<&LanguageIdentifier> for DataLocale {
    fn from(langid: &LanguageIdentifier) -> Self {
        Self {
            language: langid.language,
            script: langid.script,
            region: langid.region,
            variant: langid.variants.iter().copied().next(),
            subdivision: None,
            keywords: unicode_ext::Keywords::new(),
        }
    }
}

impl From<&Locale> for DataLocale {
    fn from(locale: &Locale) -> Self {
        let mut r = Self::from(&locale.id);

        let mut keywords = locale.extensions.unicode.keywords.clone();
        r.subdivision = keywords
            .remove(unicode_ext::key!("sd"))
            .and_then(|v| v.as_single_subtag().copied());
        r.keywords = keywords;
        r
    }
}

impl FromStr for DataLocale {
    type Err = ParseError;
    #[inline]
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Self::try_from_str(s)
    }
}

impl DataLocale {
    #[inline]
    /// Parses a [`DataLocale`].
    pub fn try_from_str(s: &str) -> Result<Self, ParseError> {
        Self::try_from_utf8(s.as_bytes())
    }

    /// Parses a [`DataLocale`] from a UTF-8 byte slice.
    pub fn try_from_utf8(code_units: &[u8]) -> Result<Self, ParseError> {
        let locale = Locale::try_from_utf8(code_units)?;
        if locale.id.variants.len() > 1
            || !locale.extensions.transform.is_empty()
            || !locale.extensions.private.is_empty()
            || !locale.extensions.other.is_empty()
            || !locale.extensions.unicode.attributes.is_empty()
        {
            return Err(ParseError::InvalidExtension);
        }

        let unicode_extensions_count = locale.extensions.unicode.keywords.iter().count();

        if unicode_extensions_count != 0
            && (unicode_extensions_count != 1
                || !locale
                    .extensions
                    .unicode
                    .keywords
                    .contains_key(&unicode_ext::key!("sd")))
        {
            return Err(ParseError::InvalidExtension);
        }

        Ok(locale.into())
    }

    pub(crate) fn for_each_subtag_str<E, F>(&self, f: &mut F) -> Result<(), E>
    where
        F: FnMut(&str) -> Result<(), E>,
    {
        f(self.language.as_str())?;
        if let Some(ref script) = self.script {
            f(script.as_str())?;
        }
        if let Some(ref region) = self.region {
            f(region.as_str())?;
        }
        if let Some(ref single_variant) = self.variant {
            f(single_variant.as_str())?;
        }
        if let Some(ref subdivision) = self.subdivision {
            f("u")?;
            f("sd")?;
            f(subdivision.as_str())?;
        }
        Ok(())
    }

    /// Compare this [`DataLocale`] with BCP-47 bytes.
    ///
    /// The return value is equivalent to what would happen if you first converted this
    /// [`DataLocale`] to a BCP-47 string and then performed a byte comparison.
    ///
    /// This function is case-sensitive and results in a *total order*, so it is appropriate for
    /// binary search. The only argument producing [`Ordering::Equal`] is `self.to_string()`.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_provider::DataLocale;
    /// use std::cmp::Ordering;
    ///
    /// let bcp47_strings: &[&str] = &[
    ///     "ca",
    ///     "ca-ES",
    ///     "ca-ES-u-sd-esct",
    ///     "ca-ES-valencia",
    ///     "cat",
    ///     "pl-Latn-PL",
    ///     "und",
    ///     "und-fonipa",
    ///     "zh",
    /// ];
    ///
    /// for ab in bcp47_strings.windows(2) {
    ///     let a = ab[0];
    ///     let b = ab[1];
    ///     assert_eq!(a.cmp(b), Ordering::Less, "strings: {} < {}", a, b);
    ///     let a_loc: DataLocale = a.parse().unwrap();
    ///     assert_eq!(
    ///         a_loc.strict_cmp(a.as_bytes()),
    ///         Ordering::Equal,
    ///         "strict_cmp: {} == {}",
    ///         a_loc,
    ///         a
    ///     );
    ///     assert_eq!(
    ///         a_loc.strict_cmp(b.as_bytes()),
    ///         Ordering::Less,
    ///         "strict_cmp: {} < {}",
    ///         a_loc,
    ///         b
    ///     );
    ///     let b_loc: DataLocale = b.parse().unwrap();
    ///     assert_eq!(
    ///         b_loc.strict_cmp(b.as_bytes()),
    ///         Ordering::Equal,
    ///         "strict_cmp: {} == {}",
    ///         b_loc,
    ///         b
    ///     );
    ///     assert_eq!(
    ///         b_loc.strict_cmp(a.as_bytes()),
    ///         Ordering::Greater,
    ///         "strict_cmp: {} > {}",
    ///         b_loc,
    ///         a
    ///     );
    /// }
    /// ```
    ///
    /// Comparison against invalid strings:
    ///
    /// ```
    /// use icu_provider::DataLocale;
    ///
    /// let invalid_strings: &[&str] = &[
    ///     // Less than "ca-ES"
    ///     "CA",
    ///     "ar-x-gbp-FOO",
    ///     // Greater than "ca-AR"
    ///     "ca_ES",
    ///     "ca-ES-x-gbp-FOO",
    /// ];
    ///
    /// let data_locale = "ca-ES".parse::<DataLocale>().unwrap();
    ///
    /// for s in invalid_strings.iter() {
    ///     let expected_ordering = "ca-AR".cmp(s);
    ///     let actual_ordering = data_locale.strict_cmp(s.as_bytes());
    ///     assert_eq!(expected_ordering, actual_ordering, "{}", s);
    /// }
    /// ```
    pub fn strict_cmp(&self, other: &[u8]) -> Ordering {
        writeable::cmp_utf8(self, other)
    }

    /// Returns whether this [`DataLocale`] is `und` in the locale and extensions portion.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_provider::DataLocale;
    ///
    /// assert!("und".parse::<DataLocale>().unwrap().is_default());
    /// assert!(!"de-u-sd-denw".parse::<DataLocale>().unwrap().is_default());
    /// assert!(!"und-ES".parse::<DataLocale>().unwrap().is_default());
    /// ```
    pub fn is_default(&self) -> bool {
        self.language.is_default()
            && self.script.is_none()
            && self.region.is_none()
            && self.variant.is_none()
            && self.subdivision.is_none()
    }

    /// Converts this `DataLocale` into a [`Locale`].
    pub fn into_locale(self) -> Locale {
        Locale {
            id: LanguageIdentifier {
                language: self.language,
                script: self.script,
                region: self.region,
                variants: self
                    .variant
                    .map(icu_locale_core::subtags::Variants::from_variant)
                    .unwrap_or_default(),
            },
            extensions: {
                let mut extensions = icu_locale_core::extensions::Extensions::default();
                if let Some(sd) = self.subdivision {
                    extensions.unicode = unicode_ext::Unicode {
                        keywords: unicode_ext::Keywords::new_single(
                            unicode_ext::key!("sd"),
                            unicode_ext::Value::from_subtag(Some(sd)),
                        ),
                        ..Default::default()
                    }
                }
                extensions
            },
        }
    }

    /// Gets the value of the specified Unicode extension keyword for this [`DataLocale`].
    #[inline]
    pub fn get_unicode_ext(&self, key: &unicode_ext::Key) -> Option<unicode_ext::Value> {
        self.keywords.get(key).cloned()
    }

    /// Like `get_unicode_ext` but untyped, easier to use during attributes migration.
    #[inline]
    pub fn get_single_unicode_ext(&self, key: &str) -> Option<&str> {
        Some(
            self.keywords
                .get(&key.parse().ok()?)?
                .as_single_subtag()?
                .as_str(),
        )
    }
}

/// An additional key to identify data beyond a [`DataLocale`].
///
/// The is a loose wrapper around a string, with semantics defined by each [`DataMarker`](crate::DataMarker).
#[derive(PartialEq, Eq, Ord, PartialOrd, Hash)]
#[repr(transparent)]
pub struct DataMarkerAttributes {
    // Validated to be non-empty ASCII alphanumeric + hyphen + underscore
    value: str,
}

impl Default for &DataMarkerAttributes {
    fn default() -> Self {
        DataMarkerAttributes::empty()
    }
}

impl Deref for DataMarkerAttributes {
    type Target = str;
    #[inline]
    fn deref(&self) -> &Self::Target {
        &self.value
    }
}

impl Debug for DataMarkerAttributes {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.value.fmt(f)
    }
}

/// Invalid character
#[derive(Debug)]
#[non_exhaustive]
pub struct AttributeParseError;

impl DataMarkerAttributes {
    const fn validate(s: &[u8]) -> Result<(), AttributeParseError> {
        let mut i = 0;
        while i < s.len() {
            #[allow(clippy::indexing_slicing)] // duh
            if !matches!(s[i], b'a'..=b'z' | b'A'..=b'Z' | b'0'..=b'9' | b'-' | b'_') {
                return Err(AttributeParseError);
            }
            i += 1;
        }
        Ok(())
    }

    /// Creates a borrowed [`DataMarkerAttributes`] from a borrowed string.
    ///
    /// Returns an error if the string contains characters other than `[a-zA-Z0-9_\-]`.
    pub const fn try_from_str(s: &str) -> Result<&Self, AttributeParseError> {
        Self::try_from_utf8(s.as_bytes())
    }

    /// Attempts to create a borrowed [`DataMarkerAttributes`] from a borrowed UTF-8 encoded byte slice.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_provider::prelude::*;
    ///
    /// let bytes = b"long-meter";
    /// let marker = DataMarkerAttributes::try_from_utf8(bytes).unwrap();
    /// assert_eq!(marker.to_string(), "long-meter");
    /// ```
    ///
    /// # Errors
    ///
    /// Returns an error if the byte slice contains code units other than `[a-zA-Z0-9_\-]`.
    pub const fn try_from_utf8(code_units: &[u8]) -> Result<&Self, AttributeParseError> {
        let Ok(()) = Self::validate(code_units) else {
            return Err(AttributeParseError);
        };

        // SAFETY: `validate` requires a UTF-8 subset
        let s = unsafe { core::str::from_utf8_unchecked(code_units) };

        // SAFETY: `Self` has the same layout as `str`
        Ok(unsafe { &*(s as *const str as *const Self) })
    }

    /// Creates an owned [`DataMarkerAttributes`] from an owned string.
    ///
    /// Returns an error if the string contains characters other than `[a-zA-Z0-9_\-]`.
    pub fn try_from_string(s: String) -> Result<Box<Self>, AttributeParseError> {
        let Ok(()) = Self::validate(s.as_bytes()) else {
            return Err(AttributeParseError);
        };

        // SAFETY: `Self` has the same layout as `str`
        Ok(unsafe { core::mem::transmute::<Box<str>, Box<Self>>(s.into_boxed_str()) })
    }

    /// Creates a borrowed [`DataMarkerAttributes`] from a borrowed string.
    ///
    /// Panics if the string contains characters other than `[a-zA-Z0-9_\-]`.
    pub const fn from_str_or_panic(s: &str) -> &Self {
        let Ok(r) = Self::try_from_str(s) else {
            panic!("Invalid marker attribute syntax")
        };
        r
    }

    /// Creates an empty [`DataMarkerAttributes`].
    pub const fn empty() -> &'static Self {
        // SAFETY: `Self` has the same layout as `str`
        unsafe { &*("" as *const str as *const Self) }
    }

    /// Returns this [`DataMarkerAttributes`] as a `&str`.
    pub const fn as_str(&self) -> &str {
        &self.value
    }
}

impl ToOwned for DataMarkerAttributes {
    type Owned = Box<Self>;
    fn to_owned(&self) -> Self::Owned {
        // SAFETY: `Self` has the same layout as `str`
        unsafe { core::mem::transmute::<Box<str>, Box<Self>>(self.as_str().to_boxed()) }
    }
}

#[test]
fn test_data_locale_to_string() {
    struct TestCase {
        pub locale: &'static str,
        pub expected: &'static str,
    }

    for cas in [
        TestCase {
            locale: "und",
            expected: "und",
        },
        TestCase {
            locale: "und-u-sd-sdd",
            expected: "und-u-sd-sdd",
        },
        TestCase {
            locale: "en-ZA-u-sd-zaa",
            expected: "en-ZA-u-sd-zaa",
        },
    ] {
        let locale = cas.locale.parse::<DataLocale>().unwrap();
        writeable::assert_writeable_eq!(locale, cas.expected);
    }
}

#[test]
fn test_data_locale_from_string() {
    #[derive(Debug)]
    struct TestCase {
        pub input: &'static str,
        pub success: bool,
    }

    for cas in [
        TestCase {
            input: "und",
            success: true,
        },
        TestCase {
            input: "und-u-cu-gbp",
            success: false,
        },
        TestCase {
            input: "en-ZA-u-sd-zaa",
            success: true,
        },
        TestCase {
            input: "en...",
            success: false,
        },
    ] {
        let data_locale = match (DataLocale::from_str(cas.input), cas.success) {
            (Ok(l), true) => l,
            (Err(_), false) => {
                continue;
            }
            (Ok(_), false) => {
                panic!("DataLocale parsed but it was supposed to fail: {cas:?}");
            }
            (Err(_), true) => {
                panic!("DataLocale was supposed to parse but it failed: {cas:?}");
            }
        };
        writeable::assert_writeable_eq!(data_locale, cas.input);
    }
}

#[test]
fn test_data_marker_attributes_from_utf8() {
    let bytes_vec: Vec<&[u8]> = vec![
        b"long-meter",
        b"long",
        b"meter",
        b"short-meter-second",
        b"usd",
    ];

    for bytes in bytes_vec {
        let marker = DataMarkerAttributes::try_from_utf8(bytes).unwrap();
        assert_eq!(marker.to_string().as_bytes(), bytes);
    }
}

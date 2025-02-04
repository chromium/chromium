// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::props::*;
use crate::provider::names::*;
use core::marker::PhantomData;
use icu_collections::codepointtrie::TrieValue;
use icu_provider::marker::ErasedMarker;
use icu_provider::prelude::*;
use yoke::Yokeable;
use zerotrie::cursor::ZeroTrieSimpleAsciiCursor;

/// A struct capable of looking up a property value from a string name.
/// Access its data by calling [`Self::as_borrowed()`] and using the methods on
/// [`PropertyParserBorrowed`].
///
/// The name can be a short name (`Lu`), a long name(`Uppercase_Letter`),
/// or an alias.
///
/// Property names can be looked up using "strict" matching (looking for a name
/// that matches exactly), or "loose matching", where the name is allowed to deviate
/// in terms of ASCII casing, whitespace, underscores, and hyphens.
///
/// # Example
///
/// ```
/// use icu::properties::props::GeneralCategory;
/// use icu::properties::PropertyParser;
///
/// let lookup = PropertyParser::<GeneralCategory>::new();
/// // short name for value
/// assert_eq!(
///     lookup.get_strict("Lu"),
///     Some(GeneralCategory::UppercaseLetter)
/// );
/// assert_eq!(
///     lookup.get_strict("Pd"),
///     Some(GeneralCategory::DashPunctuation)
/// );
/// // long name for value
/// assert_eq!(
///     lookup.get_strict("Uppercase_Letter"),
///     Some(GeneralCategory::UppercaseLetter)
/// );
/// assert_eq!(
///     lookup.get_strict("Dash_Punctuation"),
///     Some(GeneralCategory::DashPunctuation)
/// );
/// // name has incorrect casing
/// assert_eq!(lookup.get_strict("dashpunctuation"), None);
/// // loose matching of name
/// assert_eq!(
///     lookup.get_loose("dash-punctuation"),
///     Some(GeneralCategory::DashPunctuation)
/// );
/// // fake property
/// assert_eq!(lookup.get_strict("Animated_Gif"), None);
/// ```
#[derive(Debug)]
pub struct PropertyParser<T> {
    map: DataPayload<ErasedMarker<PropertyValueNameToEnumMapV1<'static>>>,
    markers: PhantomData<fn() -> T>,
}

/// A borrowed wrapper around property value name-to-enum data, returned by
/// [`PropertyParser::as_borrowed()`]. More efficient to query.
#[derive(Debug)]
pub struct PropertyParserBorrowed<'a, T> {
    map: &'a PropertyValueNameToEnumMapV1<'a>,
    markers: PhantomData<fn() -> T>,
}

impl<T> Clone for PropertyParserBorrowed<'_, T> {
    fn clone(&self) -> Self {
        *self
    }
}
impl<T> Copy for PropertyParserBorrowed<'_, T> {}

impl<T> PropertyParser<T> {
    /// Creates a new instance of `PropertyParser<T>` using compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    #[allow(clippy::new_ret_no_self)]
    pub fn new() -> PropertyParserBorrowed<'static, T>
    where
        T: ParseableEnumeratedProperty,
    {
        PropertyParserBorrowed::new()
    }

    #[doc = icu_provider::gen_any_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_unstable(
        provider: &(impl DataProvider<T::DataMarker> + ?Sized),
    ) -> Result<Self, DataError>
    where
        T: ParseableEnumeratedProperty,
    {
        Ok(Self {
            map: provider.load(Default::default())?.payload.cast(),
            markers: PhantomData,
        })
    }

    /// Construct a borrowed version of this type that can be queried.
    ///
    /// This avoids a potential small underlying cost per API call (like `get_strict()`) by consolidating it
    /// up front.
    #[inline]
    pub fn as_borrowed(&self) -> PropertyParserBorrowed<'_, T> {
        PropertyParserBorrowed {
            map: self.map.get(),
            markers: PhantomData,
        }
    }

    #[doc(hidden)] // used by FFI code
    pub fn erase(self) -> PropertyParser<u16> {
        PropertyParser {
            map: self.map.cast(),
            markers: PhantomData,
        }
    }
}

impl<T: TrieValue> PropertyParserBorrowed<'_, T> {
    /// Get the property value as a u16, doing a strict search looking for
    /// names that match exactly
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::props::GeneralCategory;
    /// use icu::properties::PropertyParser;
    ///
    /// let lookup = PropertyParser::<GeneralCategory>::new();
    /// assert_eq!(
    ///     lookup.get_strict_u16("Lu"),
    ///     Some(GeneralCategory::UppercaseLetter as u16)
    /// );
    /// assert_eq!(
    ///     lookup.get_strict_u16("Uppercase_Letter"),
    ///     Some(GeneralCategory::UppercaseLetter as u16)
    /// );
    /// // does not do loose matching
    /// assert_eq!(lookup.get_strict_u16("UppercaseLetter"), None);
    /// ```
    #[inline]
    pub fn get_strict_u16(&self, name: &str) -> Option<u16> {
        get_strict_u16(self.map, name)
    }

    /// Get the property value as a `T`, doing a strict search looking for
    /// names that match exactly
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::props::GeneralCategory;
    /// use icu::properties::PropertyParser;
    ///
    /// let lookup = PropertyParser::<GeneralCategory>::new();
    /// assert_eq!(
    ///     lookup.get_strict("Lu"),
    ///     Some(GeneralCategory::UppercaseLetter)
    /// );
    /// assert_eq!(
    ///     lookup.get_strict("Uppercase_Letter"),
    ///     Some(GeneralCategory::UppercaseLetter)
    /// );
    /// // does not do loose matching
    /// assert_eq!(lookup.get_strict("UppercaseLetter"), None);
    /// ```
    #[inline]
    pub fn get_strict(&self, name: &str) -> Option<T> {
        T::try_from_u32(self.get_strict_u16(name)? as u32).ok()
    }

    /// Get the property value as a u16, doing a loose search looking for
    /// names that match case-insensitively, ignoring ASCII hyphens, underscores, and
    /// whitespaces.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::props::GeneralCategory;
    /// use icu::properties::PropertyParser;
    ///
    /// let lookup = PropertyParser::<GeneralCategory>::new();
    /// assert_eq!(
    ///     lookup.get_loose_u16("Lu"),
    ///     Some(GeneralCategory::UppercaseLetter as u16)
    /// );
    /// assert_eq!(
    ///     lookup.get_loose_u16("Uppercase_Letter"),
    ///     Some(GeneralCategory::UppercaseLetter as u16)
    /// );
    /// // does do loose matching
    /// assert_eq!(
    ///     lookup.get_loose_u16("UppercaseLetter"),
    ///     Some(GeneralCategory::UppercaseLetter as u16)
    /// );
    /// ```
    #[inline]
    pub fn get_loose_u16(&self, name: &str) -> Option<u16> {
        get_loose_u16(self.map, name)
    }

    /// Get the property value as a `T`, doing a loose search looking for
    /// names that match case-insensitively, ignoring ASCII hyphens, underscores, and
    /// whitespaces.
    ///
    /// # Example
    ///
    /// ```
    /// use icu::properties::props::GeneralCategory;
    /// use icu::properties::PropertyParser;
    ///
    /// let lookup = PropertyParser::<GeneralCategory>::new();
    /// assert_eq!(
    ///     lookup.get_loose("Lu"),
    ///     Some(GeneralCategory::UppercaseLetter)
    /// );
    /// assert_eq!(
    ///     lookup.get_loose("Uppercase_Letter"),
    ///     Some(GeneralCategory::UppercaseLetter)
    /// );
    /// // does do loose matching
    /// assert_eq!(
    ///     lookup.get_loose("UppercaseLetter"),
    ///     Some(GeneralCategory::UppercaseLetter)
    /// );
    /// ```
    #[inline]
    pub fn get_loose(&self, name: &str) -> Option<T> {
        T::try_from_u32(self.get_loose_u16(name)? as u32).ok()
    }
}

#[cfg(feature = "compiled_data")]
impl<T: ParseableEnumeratedProperty> Default for PropertyParserBorrowed<'static, T> {
    fn default() -> Self {
        Self::new()
    }
}

impl<T: TrieValue> PropertyParserBorrowed<'static, T> {
    /// Creates a new instance of `PropertyParserBorrowed<T>` using compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn new() -> Self
    where
        T: ParseableEnumeratedProperty,
    {
        Self {
            map: T::SINGLETON,
            markers: PhantomData,
        }
    }

    /// Cheaply converts a [`PropertyParserBorrowed<'static>`] into a [`PropertyParser`].
    ///
    /// Note: Due to branching and indirection, using [`PropertyParser`] might inhibit some
    /// compile-time optimizations that are possible with [`PropertyParserBorrowed`].
    pub const fn static_to_owned(self) -> PropertyParser<T> {
        PropertyParser {
            map: DataPayload::from_static_ref(self.map),
            markers: PhantomData,
        }
    }
}

/// Avoid monomorphizing multiple copies of this function
fn get_strict_u16(payload: &PropertyValueNameToEnumMapV1<'_>, name: &str) -> Option<u16> {
    payload.map.get(name).and_then(|i| i.try_into().ok())
}

/// Avoid monomorphizing multiple copies of this function
fn get_loose_u16(payload: &PropertyValueNameToEnumMapV1<'_>, name: &str) -> Option<u16> {
    fn recurse(mut cursor: ZeroTrieSimpleAsciiCursor, mut rest: &[u8]) -> Option<usize> {
        if cursor.is_empty() {
            return None;
        }

        // Skip whitespace, underscore, hyphen in trie.
        for skip in [b'\t', b'\n', b'\x0C', b'\r', b' ', 0x0B, b'_', b'-'] {
            let mut skip_cursor = cursor.clone();
            skip_cursor.step(skip);
            if let Some(r) = recurse(skip_cursor, rest) {
                return Some(r);
            }
        }

        let ascii = loop {
            let Some((&a, r)) = rest.split_first() else {
                return cursor.take_value();
            };
            rest = r;

            // Skip whitespace, underscore, hyphen in input
            if !matches!(
                a,
                b'\t' | b'\n' | b'\x0C' | b'\r' | b' ' | 0x0B | b'_' | b'-'
            ) {
                break a;
            }
        };

        let mut other_case_cursor = cursor.clone();
        cursor.step(ascii);
        other_case_cursor.step(if ascii.is_ascii_lowercase() {
            ascii.to_ascii_uppercase()
        } else {
            ascii.to_ascii_lowercase()
        });
        // This uses the call stack as the DFS stack. The recursion will terminate as
        // rest's length is strictly shrinking. The call stack's depth is limited by
        // name.len().
        recurse(cursor, rest).or_else(|| recurse(other_case_cursor, rest))
    }

    recurse(payload.map.cursor(), name.as_bytes()).and_then(|i| i.try_into().ok())
}

/// A struct capable of looking up a property name from a value
/// Access its data by calling [`Self::as_borrowed()`] and using the methods on
/// [`PropertyNamesLongBorrowed`].
///
/// # Example
///
/// ```
/// use icu::properties::props::CanonicalCombiningClass;
/// use icu::properties::PropertyNamesLong;
///
/// let names = PropertyNamesLong::<CanonicalCombiningClass>::new();
/// assert_eq!(
///     names.get(CanonicalCombiningClass::KanaVoicing),
///     Some("Kana_Voicing")
/// );
/// assert_eq!(
///     names.get(CanonicalCombiningClass::AboveLeft),
///     Some("Above_Left")
/// );
/// ```
pub struct PropertyNamesLong<T: NamedEnumeratedProperty> {
    map: DataPayload<ErasedMarker<T::DataStructLong>>,
}

impl<T: NamedEnumeratedProperty> core::fmt::Debug for PropertyNamesLong<T> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("PropertyNamesLong")
            // .field("map", &self.map)
            .finish()
    }
}

/// A borrowed wrapper around property value name-to-enum data, returned by
/// [`PropertyNamesLong::as_borrowed()`]. More efficient to query.
#[derive(Debug)]
pub struct PropertyNamesLongBorrowed<'a, T: NamedEnumeratedProperty> {
    map: &'a T::DataStructLong,
}

impl<T: NamedEnumeratedProperty> Clone for PropertyNamesLongBorrowed<'_, T> {
    fn clone(&self) -> Self {
        *self
    }
}
impl<T: NamedEnumeratedProperty> Copy for PropertyNamesLongBorrowed<'_, T> {}

impl<T: NamedEnumeratedProperty> PropertyNamesLong<T> {
    /// Creates a new instance of `PropertyNamesLongBorrowed<T>`.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    #[allow(clippy::new_ret_no_self)]
    pub fn new() -> PropertyNamesLongBorrowed<'static, T> {
        PropertyNamesLongBorrowed::new()
    }

    #[doc = icu_provider::gen_any_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_unstable(
        provider: &(impl DataProvider<T::DataMarkerLong> + ?Sized),
    ) -> Result<Self, DataError> {
        Ok(Self {
            map: provider.load(Default::default())?.payload.cast(),
        })
    }

    /// Construct a borrowed version of this type that can be queried.
    ///
    /// This avoids a potential small underlying cost per API call (like `get_static()`) by consolidating it
    /// up front.
    #[inline]
    pub fn as_borrowed(&self) -> PropertyNamesLongBorrowed<'_, T> {
        PropertyNamesLongBorrowed {
            map: unsafe {
                &*(self.map.get() as *const <T::DataStructLong as Yokeable>::Output
                    as *const T::DataStructLong)
            },
        }
    }
}

impl<T: NamedEnumeratedProperty> PropertyNamesLongBorrowed<'_, T> {
    /// Get the property name given a value
    ///
    /// # Example
    ///
    /// ```rust
    /// use icu::properties::props::CanonicalCombiningClass;
    /// use icu::properties::PropertyNamesLong;
    ///
    /// let lookup = PropertyNamesLong::<CanonicalCombiningClass>::new();
    /// assert_eq!(
    ///     lookup.get(CanonicalCombiningClass::KanaVoicing),
    ///     Some("Kana_Voicing")
    /// );
    /// assert_eq!(
    ///     lookup.get(CanonicalCombiningClass::AboveLeft),
    ///     Some("Above_Left")
    /// );
    /// ```
    #[inline]
    pub fn get(&self, property: T) -> Option<&str> {
        self.map.get(property.to_u32())
    }
}

#[cfg(feature = "compiled_data")]
impl<T: NamedEnumeratedProperty> Default for PropertyNamesLongBorrowed<'static, T> {
    fn default() -> Self {
        Self::new()
    }
}

impl<T: NamedEnumeratedProperty> PropertyNamesLongBorrowed<'static, T> {
    /// Creates a new instance of `PropertyNamesLongBorrowed<T>`.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn new() -> Self {
        Self {
            map: T::SINGLETON_LONG,
        }
    }

    /// Cheaply converts a [`PropertyNamesLongBorrowed<'static>`] into a [`PropertyNamesLong`].
    ///
    /// Note: Due to branching and indirection, using [`PropertyNamesLong`] might inhibit some
    /// compile-time optimizations that are possible with [`PropertyNamesLongBorrowed`].
    pub const fn static_to_owned(self) -> PropertyNamesLong<T> {
        PropertyNamesLong {
            map: DataPayload::from_static_ref(self.map),
        }
    }
}

/// A struct capable of looking up a property name from a value
/// Access its data by calling [`Self::as_borrowed()`] and using the methods on
/// [`PropertyNamesShortBorrowed`].
///
/// # Example
///
/// ```
/// use icu::properties::props::CanonicalCombiningClass;
/// use icu::properties::PropertyNamesShort;
///
/// let names = PropertyNamesShort::<CanonicalCombiningClass>::new();
/// assert_eq!(names.get(CanonicalCombiningClass::KanaVoicing), Some("KV"));
/// assert_eq!(names.get(CanonicalCombiningClass::AboveLeft), Some("AL"));
/// ```
pub struct PropertyNamesShort<T: NamedEnumeratedProperty> {
    map: DataPayload<ErasedMarker<T::DataStructShort>>,
}

impl<T: NamedEnumeratedProperty> core::fmt::Debug for PropertyNamesShort<T> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("PropertyNamesShort")
            // .field("map", &self.map)
            .finish()
    }
}

/// A borrowed wrapper around property value name-to-enum data, returned by
/// [`PropertyNamesShort::as_borrowed()`]. More efficient to query.
#[derive(Debug)]
pub struct PropertyNamesShortBorrowed<'a, T: NamedEnumeratedProperty> {
    map: &'a T::DataStructShort,
}

impl<T: NamedEnumeratedProperty> Clone for PropertyNamesShortBorrowed<'_, T> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<T: NamedEnumeratedProperty> Copy for PropertyNamesShortBorrowed<'_, T> {}

impl<T: NamedEnumeratedProperty> PropertyNamesShort<T> {
    /// Creates a new instance of `PropertyNamesShortBorrowed<T>`.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    #[allow(clippy::new_ret_no_self)]
    pub fn new() -> PropertyNamesShortBorrowed<'static, T> {
        PropertyNamesShortBorrowed::new()
    }

    #[doc = icu_provider::gen_any_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_unstable(
        provider: &(impl DataProvider<T::DataMarkerShort> + ?Sized),
    ) -> Result<Self, DataError> {
        Ok(Self {
            map: provider.load(Default::default())?.payload.cast(),
        })
    }

    /// Construct a borrowed version of this type that can be queried.
    ///
    /// This avoids a potential small underlying cost per API call (like `get_static()`) by consolidating it
    /// up front.
    #[inline]
    pub fn as_borrowed(&self) -> PropertyNamesShortBorrowed<'_, T> {
        PropertyNamesShortBorrowed {
            map: unsafe {
                &*(self.map.get() as *const <T::DataStructShort as Yokeable>::Output
                    as *const T::DataStructShort)
            },
        }
    }
}

impl<T: NamedEnumeratedProperty> PropertyNamesShortBorrowed<'_, T> {
    /// Get the property name given a value
    ///
    /// # Example
    ///
    /// ```rust
    /// use icu::properties::props::CanonicalCombiningClass;
    /// use icu::properties::PropertyNamesShort;
    ///
    /// let lookup = PropertyNamesShort::<CanonicalCombiningClass>::new();
    /// assert_eq!(lookup.get(CanonicalCombiningClass::KanaVoicing), Some("KV"));
    /// assert_eq!(lookup.get(CanonicalCombiningClass::AboveLeft), Some("AL"));
    /// ```
    #[inline]
    pub fn get(&self, property: T) -> Option<&str> {
        self.map.get(property.to_u32())
    }
}

impl PropertyNamesShortBorrowed<'_, Script> {
    /// Gets the "name" of a script property as a `icu::locale::subtags::Script`.
    ///
    /// This method is available only on `PropertyNamesShortBorrowed<Script>`.
    ///
    /// # Example
    ///
    /// ```rust
    /// use icu::locale::subtags::script;
    /// use icu::properties::props::Script;
    /// use icu::properties::PropertyNamesShort;
    ///
    /// let lookup = PropertyNamesShort::<Script>::new();
    /// assert_eq!(
    ///     lookup.get_locale_script(Script::Brahmi),
    ///     Some(script!("Brah"))
    /// );
    /// assert_eq!(
    ///     lookup.get_locale_script(Script::Hangul),
    ///     Some(script!("Hang"))
    /// );
    /// ```
    ///
    /// For the reverse direction, use property parsing as normal:
    /// ```
    /// use icu::locale::subtags::script;
    /// use icu::properties::props::Script;
    /// use icu::properties::PropertyParser;
    ///
    /// let parser = PropertyParser::<Script>::new();
    /// assert_eq!(
    ///     parser.get_strict(script!("Brah").as_str()),
    ///     Some(Script::Brahmi)
    /// );
    /// assert_eq!(
    ///     parser.get_strict(script!("Hang").as_str()),
    ///     Some(Script::Hangul)
    /// );
    /// ```
    #[inline]
    pub fn get_locale_script(&self, property: Script) -> Option<icu_locale_core::subtags::Script> {
        let prop = usize::try_from(property.to_u32()).ok()?;
        self.map.map.get(prop).and_then(|o| o.0)
    }
}

#[cfg(feature = "compiled_data")]
impl<T: NamedEnumeratedProperty> Default for PropertyNamesShortBorrowed<'static, T> {
    fn default() -> Self {
        Self::new()
    }
}

impl<T: NamedEnumeratedProperty> PropertyNamesShortBorrowed<'static, T> {
    /// Creates a new instance of `PropertyNamesShortBorrowed<T>`.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn new() -> Self {
        Self {
            map: T::SINGLETON_SHORT,
        }
    }

    /// Cheaply converts a [`PropertyNamesShortBorrowed<'static>`] into a [`PropertyNamesShort`].
    ///
    /// Note: Due to branching and indirection, using [`PropertyNamesShort`] might inhibit some
    /// compile-time optimizations that are possible with [`PropertyNamesShortBorrowed`].
    pub const fn static_to_owned(self) -> PropertyNamesShort<T> {
        PropertyNamesShort {
            map: DataPayload::from_static_ref(self.map),
        }
    }
}

/// A property whose value names can be parsed from strings.
pub trait ParseableEnumeratedProperty: crate::private::Sealed + TrieValue {
    #[doc(hidden)]
    type DataMarker: DataMarker<DataStruct = PropertyValueNameToEnumMapV1<'static>>;
    #[doc(hidden)]
    #[cfg(feature = "compiled_data")]
    const SINGLETON: &'static PropertyValueNameToEnumMapV1<'static>;
}

// Abstract over Linear/Sparse/Script representation
pub trait PropertyEnumToValueNameLookup {
    fn get(&self, prop: u32) -> Option<&str>;
}

impl PropertyEnumToValueNameLookup for PropertyEnumToValueNameLinearMapV1<'_> {
    fn get(&self, prop: u32) -> Option<&str> {
        self.map.get(usize::try_from(prop).ok()?)
    }
}

impl PropertyEnumToValueNameLookup for PropertyEnumToValueNameSparseMapV1<'_> {
    fn get(&self, prop: u32) -> Option<&str> {
        self.map.get(&u16::try_from(prop).ok()?)
    }
}

impl PropertyEnumToValueNameLookup for PropertyScriptToIcuScriptMapV1<'_> {
    fn get(&self, prop: u32) -> Option<&str> {
        self.map
            .get_ule_ref(usize::try_from(prop).ok()?)
            .and_then(|no| no.as_ref())
            .map(|s| s.as_str())
    }
}

/// A property whose value names can be represented as strings.
pub trait NamedEnumeratedProperty: ParseableEnumeratedProperty {
    #[doc(hidden)]
    type DataStructLong: 'static + for<'a> Yokeable<'a> + PropertyEnumToValueNameLookup;
    #[doc(hidden)]
    type DataStructShort: 'static + for<'a> Yokeable<'a> + PropertyEnumToValueNameLookup;
    #[doc(hidden)]
    type DataMarkerLong: DataMarker<DataStruct = Self::DataStructLong>;
    #[doc(hidden)]
    type DataMarkerShort: DataMarker<DataStruct = Self::DataStructShort>;
    #[doc(hidden)]
    #[cfg(feature = "compiled_data")]
    const SINGLETON_LONG: &'static Self::DataStructLong;
    #[doc(hidden)]
    #[cfg(feature = "compiled_data")]
    const SINGLETON_SHORT: &'static Self::DataStructShort;
}

macro_rules! impl_value_getter {
    (
        impl $ty:ident {
            $marker_n2e:ident / $singleton_n2e:ident;
            $(
                $data_struct_s:ident / $marker_e2sn:ident / $singleton_e2sn:ident;
                $data_struct_l:ident / $marker_e2ln:ident / $singleton_e2ln:ident;
            )?
        }
    ) => {
        impl ParseableEnumeratedProperty for $ty {
            type DataMarker = $marker_n2e;
            #[cfg(feature = "compiled_data")]
            const SINGLETON: &'static PropertyValueNameToEnumMapV1<'static> = crate::provider::Baked::$singleton_n2e;
        }

        $(
            impl NamedEnumeratedProperty for $ty {
                type DataStructLong = $data_struct_l<'static>;
                type DataStructShort = $data_struct_s<'static>;
                type DataMarkerLong = crate::provider::$marker_e2ln;
                type DataMarkerShort = crate::provider::$marker_e2sn;
                #[cfg(feature = "compiled_data")]
                const SINGLETON_LONG: &'static Self::DataStructLong = crate::provider::Baked::$singleton_e2ln;
                #[cfg(feature = "compiled_data")]
                const SINGLETON_SHORT: &'static Self::DataStructShort = crate::provider::Baked::$singleton_e2sn;
            }
        )?
    };
}

impl_value_getter! {
    impl BidiClass {
        BidiClassNameToValueV2Marker / SINGLETON_BIDI_CLASS_NAME_TO_VALUE_V2_MARKER;
        PropertyEnumToValueNameLinearMapV1 / BidiClassValueToShortNameV1Marker / SINGLETON_BIDI_CLASS_VALUE_TO_SHORT_NAME_V1_MARKER;
        PropertyEnumToValueNameLinearMapV1 / BidiClassValueToLongNameV1Marker / SINGLETON_BIDI_CLASS_VALUE_TO_LONG_NAME_V1_MARKER;
    }
}

impl_value_getter! {
    impl GeneralCategory {
        GeneralCategoryNameToValueV2Marker / SINGLETON_GENERAL_CATEGORY_NAME_TO_VALUE_V2_MARKER;
        PropertyEnumToValueNameLinearMapV1 / GeneralCategoryValueToShortNameV1Marker / SINGLETON_GENERAL_CATEGORY_VALUE_TO_SHORT_NAME_V1_MARKER;
        PropertyEnumToValueNameLinearMapV1 / GeneralCategoryValueToLongNameV1Marker / SINGLETON_GENERAL_CATEGORY_VALUE_TO_LONG_NAME_V1_MARKER;
    }
}

impl_value_getter! {
    impl GeneralCategoryGroup {
        GeneralCategoryMaskNameToValueV2Marker / SINGLETON_GENERAL_CATEGORY_MASK_NAME_TO_VALUE_V2_MARKER;
    }
}

impl_value_getter! {
    impl Script {
        ScriptNameToValueV2Marker / SINGLETON_SCRIPT_NAME_TO_VALUE_V2_MARKER;
        PropertyScriptToIcuScriptMapV1 / ScriptValueToShortNameV1Marker / SINGLETON_SCRIPT_VALUE_TO_SHORT_NAME_V1_MARKER;
        PropertyEnumToValueNameLinearMapV1 / ScriptValueToLongNameV1Marker / SINGLETON_SCRIPT_VALUE_TO_LONG_NAME_V1_MARKER;
    }
}

impl_value_getter! {
   impl HangulSyllableType {
        HangulSyllableTypeNameToValueV2Marker / SINGLETON_HANGUL_SYLLABLE_TYPE_NAME_TO_VALUE_V2_MARKER;
        PropertyEnumToValueNameLinearMapV1 / HangulSyllableTypeValueToShortNameV1Marker / SINGLETON_HANGUL_SYLLABLE_TYPE_VALUE_TO_SHORT_NAME_V1_MARKER;
        PropertyEnumToValueNameLinearMapV1 / HangulSyllableTypeValueToLongNameV1Marker / SINGLETON_HANGUL_SYLLABLE_TYPE_VALUE_TO_LONG_NAME_V1_MARKER;
    }
}

impl_value_getter! {
    impl EastAsianWidth {
        EastAsianWidthNameToValueV2Marker / SINGLETON_EAST_ASIAN_WIDTH_NAME_TO_VALUE_V2_MARKER;
        PropertyEnumToValueNameLinearMapV1 / EastAsianWidthValueToShortNameV1Marker / SINGLETON_EAST_ASIAN_WIDTH_VALUE_TO_SHORT_NAME_V1_MARKER;
        PropertyEnumToValueNameLinearMapV1 / EastAsianWidthValueToLongNameV1Marker / SINGLETON_EAST_ASIAN_WIDTH_VALUE_TO_LONG_NAME_V1_MARKER;
    }
}

impl_value_getter! {
    impl LineBreak {
        LineBreakNameToValueV2Marker / SINGLETON_LINE_BREAK_NAME_TO_VALUE_V2_MARKER;
        PropertyEnumToValueNameLinearMapV1 / LineBreakValueToShortNameV1Marker / SINGLETON_LINE_BREAK_VALUE_TO_SHORT_NAME_V1_MARKER;
        PropertyEnumToValueNameLinearMapV1 / LineBreakValueToLongNameV1Marker / SINGLETON_LINE_BREAK_VALUE_TO_LONG_NAME_V1_MARKER;
    }
}

impl_value_getter! {
    impl GraphemeClusterBreak {
        GraphemeClusterBreakNameToValueV2Marker / SINGLETON_GRAPHEME_CLUSTER_BREAK_NAME_TO_VALUE_V2_MARKER;
        PropertyEnumToValueNameLinearMapV1 / GraphemeClusterBreakValueToShortNameV1Marker / SINGLETON_GRAPHEME_CLUSTER_BREAK_VALUE_TO_SHORT_NAME_V1_MARKER;
        PropertyEnumToValueNameLinearMapV1 / GraphemeClusterBreakValueToLongNameV1Marker / SINGLETON_GRAPHEME_CLUSTER_BREAK_VALUE_TO_LONG_NAME_V1_MARKER;
    }
}

impl_value_getter! {
    impl WordBreak {
        WordBreakNameToValueV2Marker / SINGLETON_WORD_BREAK_NAME_TO_VALUE_V2_MARKER;
        PropertyEnumToValueNameLinearMapV1 / WordBreakValueToShortNameV1Marker / SINGLETON_WORD_BREAK_VALUE_TO_SHORT_NAME_V1_MARKER;
        PropertyEnumToValueNameLinearMapV1 / WordBreakValueToLongNameV1Marker / SINGLETON_WORD_BREAK_VALUE_TO_LONG_NAME_V1_MARKER;
    }
}

impl_value_getter! {
    impl SentenceBreak {
        SentenceBreakNameToValueV2Marker / SINGLETON_SENTENCE_BREAK_NAME_TO_VALUE_V2_MARKER;
        PropertyEnumToValueNameLinearMapV1 / SentenceBreakValueToShortNameV1Marker / SINGLETON_SENTENCE_BREAK_VALUE_TO_SHORT_NAME_V1_MARKER;
        PropertyEnumToValueNameLinearMapV1 / SentenceBreakValueToLongNameV1Marker / SINGLETON_SENTENCE_BREAK_VALUE_TO_LONG_NAME_V1_MARKER;
    }
}

impl_value_getter! {
    impl CanonicalCombiningClass {
        CanonicalCombiningClassNameToValueV2Marker / SINGLETON_CANONICAL_COMBINING_CLASS_NAME_TO_VALUE_V2_MARKER;
        PropertyEnumToValueNameSparseMapV1 / CanonicalCombiningClassValueToShortNameV1Marker / SINGLETON_CANONICAL_COMBINING_CLASS_VALUE_TO_SHORT_NAME_V1_MARKER;
        PropertyEnumToValueNameSparseMapV1 / CanonicalCombiningClassValueToLongNameV1Marker / SINGLETON_CANONICAL_COMBINING_CLASS_VALUE_TO_LONG_NAME_V1_MARKER;
    }
}

impl_value_getter! {
    impl IndicSyllabicCategory {
        IndicSyllabicCategoryNameToValueV2Marker / SINGLETON_INDIC_SYLLABIC_CATEGORY_NAME_TO_VALUE_V2_MARKER;
        PropertyEnumToValueNameLinearMapV1 / IndicSyllabicCategoryValueToShortNameV1Marker / SINGLETON_INDIC_SYLLABIC_CATEGORY_VALUE_TO_SHORT_NAME_V1_MARKER;
        PropertyEnumToValueNameLinearMapV1 / IndicSyllabicCategoryValueToLongNameV1Marker / SINGLETON_INDIC_SYLLABIC_CATEGORY_VALUE_TO_LONG_NAME_V1_MARKER;
    }
}

impl_value_getter! {
    impl JoiningType {
        JoiningTypeNameToValueV2Marker / SINGLETON_JOINING_TYPE_NAME_TO_VALUE_V2_MARKER;
        PropertyEnumToValueNameLinearMapV1 / JoiningTypeValueToShortNameV1Marker / SINGLETON_JOINING_TYPE_VALUE_TO_SHORT_NAME_V1_MARKER;
        PropertyEnumToValueNameLinearMapV1 / JoiningTypeValueToLongNameV1Marker / SINGLETON_JOINING_TYPE_VALUE_TO_LONG_NAME_V1_MARKER;
    }
}

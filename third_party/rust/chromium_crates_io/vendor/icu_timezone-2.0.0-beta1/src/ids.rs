// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use alloc::borrow::Cow;
use alloc::string::String;
use alloc::vec::Vec;
use icu_provider::prelude::*;
use zerotrie::cursor::ZeroAsciiIgnoreCaseTrieCursor;

use crate::{
    provider::names::{
        Bcp47ToIanaMapV1, Bcp47ToIanaMapV1Marker, IanaToBcp47MapV3, IanaToBcp47MapV3Marker,
        NON_REGION_CITY_PREFIX,
    },
    TimeZoneBcp47Id,
};

/// A mapper between IANA time zone identifiers and BCP-47 time zone identifiers.
///
/// This mapper supports two-way mapping, but it is optimized for the case of IANA to BCP-47.
/// It also supports normalizing and canonicalizing the IANA strings.
///
/// There are approximately 600 IANA identifiers and 450 BCP-47 identifiers.
///
/// BCP-47 time zone identifiers are 8 ASCII characters or less and currently
/// average 5.1 characters long. Current IANA time zone identifiers are less than
/// 40 ASCII characters and average 14.2 characters long.
///
/// These lists grow very slowly; in a typical year, 2-3 new identifiers are added.
///
/// # Normalization vs Canonicalization
///
/// Multiple IANA time zone identifiers can refer to the same BCP-47 time zone. For example, the
/// following three IANA identifiers all map to `"usind"`:
///
/// - "America/Fort_Wayne"
/// - "America/Indiana/Indianapolis"
/// - "America/Indianapolis"
/// - "US/East-Indiana"
///
/// There is only one canonical identifier, which is "America/Indiana/Indianapolis". The
/// *canonicalization* operation returns the canonical identifier. You should canonicalize if
/// you need to compare time zones for equality. Note that the canonical identifier can change
/// over time. For example, the identifier "Europe/Kiev" was renamed to the newly-added
/// identifier "Europe/Kyiv" in 2022.
///
/// The *normalization* operation, on the other hand, keeps the input identifier but normalizes
/// the casing. For example, "AMERICA/FORT_WAYNE" normalizes to "America/Fort_Wayne".
/// Normalization is a data-driven operation because there are no algorithmic casing rules that
/// work for all IANA time zone identifiers.
///
/// Normalization is a cheap operation, but canonicalization might be expensive, since it might
/// require searching over all IANA IDs to find the canonicalization. If you need
/// canonicalization that is reliably fast, use [`TimeZoneIdMapperWithFastCanonicalization`].
///
/// # Examples
///
/// ```
/// use icu::timezone::TimeZoneBcp47Id;
/// use icu::timezone::TimeZoneIdMapper;
/// use tinystr::tinystr;
///
/// let mapper = TimeZoneIdMapper::new();
///
/// // The IANA zone "Australia/Melbourne" is the BCP-47 zone "aumel":
/// assert_eq!(
///     mapper.iana_to_bcp47("Australia/Melbourne"),
///     TimeZoneBcp47Id(tinystr!(8, "aumel"))
/// );
///
/// // Lookup is ASCII-case-insensitive:
/// assert_eq!(
///     mapper.iana_to_bcp47("australia/melbourne"),
///     TimeZoneBcp47Id(tinystr!(8, "aumel"))
/// );
///
/// // The IANA zone "Australia/Victoria" is an alias:
/// assert_eq!(
///     mapper.iana_to_bcp47("Australia/Victoria"),
///     TimeZoneBcp47Id(tinystr!(8, "aumel"))
/// );
///
/// // The IANA zone "Australia/Boing_Boing" does not exist
/// // (maybe not *yet*), so it produces the special unknown
/// // timezone in order for this operation to be infallible:
/// assert_eq!(
///     mapper.iana_to_bcp47("Australia/Boing_Boing"),
///     TimeZoneBcp47Id::unknown()
/// );
///
/// // We can recover the canonical identifier from the mapper:
/// assert_eq!(
///     mapper.canonicalize_iana("Australia/Victoria").unwrap().0,
///     "Australia/Melbourne"
/// );
/// ```
#[derive(Debug, Clone)]
pub struct TimeZoneIdMapper {
    data: DataPayload<IanaToBcp47MapV3Marker>,
}

impl TimeZoneIdMapper {
    /// Creates a new [`TimeZoneIdMapper`] using compiled data.
    ///
    /// See [`TimeZoneIdMapper`] for an example.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    #[allow(clippy::new_ret_no_self)]
    pub fn new() -> TimeZoneIdMapperBorrowed<'static> {
        TimeZoneIdMapperBorrowed::new()
    }

    icu_provider::gen_any_buffer_data_constructors!(() -> error: DataError,
        functions: [
            new: skip,
            try_new_with_any_provider,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self,
        ]
    );

    #[doc = icu_provider::gen_any_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_unstable<P>(provider: &P) -> Result<Self, DataError>
    where
        P: DataProvider<IanaToBcp47MapV3Marker> + ?Sized,
    {
        let data = provider.load(Default::default())?.payload;
        Ok(Self { data })
    }

    /// Returns a borrowed version of the mapper that can be queried.
    ///
    /// This avoids a small potential indirection cost when querying the mapper.
    pub fn as_borrowed(&self) -> TimeZoneIdMapperBorrowed {
        TimeZoneIdMapperBorrowed {
            data: self.data.get(),
        }
    }
}

impl AsRef<TimeZoneIdMapper> for TimeZoneIdMapper {
    #[inline]
    fn as_ref(&self) -> &TimeZoneIdMapper {
        self
    }
}

/// A borrowed wrapper around the time zone ID mapper, returned by
/// [`TimeZoneIdMapper::as_borrowed()`]. More efficient to query.
#[derive(Debug, Copy, Clone)]
pub struct TimeZoneIdMapperBorrowed<'a> {
    data: &'a IanaToBcp47MapV3<'a>,
}

#[cfg(feature = "compiled_data")]
impl Default for TimeZoneIdMapperBorrowed<'static> {
    fn default() -> Self {
        Self::new()
    }
}

impl TimeZoneIdMapperBorrowed<'static> {
    /// Creates a new [`TimeZoneIdMapperBorrowed`] using compiled data.
    ///
    /// See [`TimeZoneIdMapperBorrowed`] for an example.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn new() -> Self {
        Self {
            data: crate::provider::Baked::SINGLETON_IANA_TO_BCP47_MAP_V3_MARKER,
        }
    }

    /// Cheaply converts a [`TimeZoneIdMapperBorrowed<'static>`] into a [`TimeZoneIdMapper`].
    ///
    /// Note: Due to branching and indirection, using [`TimeZoneIdMapper`] might inhibit some
    /// compile-time optimizations that are possible with [`TimeZoneIdMapperBorrowed`].
    pub fn static_to_owned(&self) -> TimeZoneIdMapper {
        TimeZoneIdMapper {
            data: DataPayload::from_static_ref(self.data),
        }
    }
}

impl TimeZoneIdMapperBorrowed<'_> {
    /// Gets the BCP-47 time zone ID from an IANA time zone ID
    /// with a case-insensitive lookup.
    ///
    /// Returns `None` if the IANA ID is not found.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_timezone::TimeZoneBcp47Id;
    /// use icu_timezone::TimeZoneIdMapper;
    ///
    /// let mapper = TimeZoneIdMapper::new();
    ///
    /// let result = mapper.iana_to_bcp47("Asia/CALCUTTA");
    ///
    /// assert_eq!(*result, "inccu");
    ///
    /// // Unknown IANA time zone ID:
    /// assert_eq!(
    ///     mapper.iana_to_bcp47("America/San_Francisco"),
    ///     TimeZoneBcp47Id::unknown()
    /// );
    /// ```
    pub fn iana_to_bcp47(&self, iana_id: &str) -> TimeZoneBcp47Id {
        self.iana_lookup_quick(iana_id)
            .and_then(|trie_value| self.data.bcp47_ids.get(trie_value.index()))
            .unwrap_or(TimeZoneBcp47Id::unknown())
    }

    /// Same as [`Self::iana_to_bcp47()`] but works with potentially ill-formed UTF-8.
    pub fn iana_bytes_to_bcp47(&self, iana_id: &[u8]) -> TimeZoneBcp47Id {
        self.iana_lookup_quick(iana_id)
            .and_then(|trie_value| self.data.bcp47_ids.get(trie_value.index()))
            .unwrap_or(TimeZoneBcp47Id::unknown())
    }

    /// Normalizes the syntax of an IANA time zone ID.
    ///
    /// Also returns the BCP-47 time zone ID.
    ///
    /// Returns `None` if the IANA ID is not found.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_timezone::TimeZoneBcp47Id;
    /// use icu_timezone::TimeZoneIdMapper;
    /// use std::borrow::Cow;
    ///
    /// let mapper = TimeZoneIdMapper::new();
    ///
    /// let result = mapper.normalize_iana("Asia/CALCUTTA").unwrap();
    ///
    /// assert_eq!(result.0, "Asia/Calcutta");
    /// assert!(matches!(result.0, Cow::Owned(_)));
    /// assert_eq!(*result.1, "inccu");
    ///
    /// // Borrows when able:
    /// let result = mapper.normalize_iana("America/Chicago").unwrap();
    /// assert_eq!(result.0, "America/Chicago");
    /// assert!(matches!(result.0, Cow::Borrowed(_)));
    ///
    /// // Unknown IANA time zone ID:
    /// assert_eq!(mapper.normalize_iana("America/San_Francisco"), None);
    /// ```
    pub fn normalize_iana<'s>(&self, iana_id: &'s str) -> Option<(Cow<'s, str>, TimeZoneBcp47Id)> {
        let (trie_value, string) = self.iana_lookup_with_normalization(iana_id, |_| {})?;
        let Some(bcp47_id) = self.data.bcp47_ids.get(trie_value.index()) else {
            debug_assert!(false, "index should be in range");
            return None;
        };
        Some((string, bcp47_id))
    }

    /// Returns the canonical, normalized identifier of the given IANA time zone.
    ///
    /// Also returns the BCP-47 time zone ID.
    ///
    /// Returns `None` if the IANA ID is not found.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_timezone::TimeZoneBcp47Id;
    /// use icu_timezone::TimeZoneIdMapper;
    /// use std::borrow::Cow;
    ///
    /// let mapper = TimeZoneIdMapper::new();
    ///
    /// let result = mapper.canonicalize_iana("Asia/CALCUTTA").unwrap();
    ///
    /// assert_eq!(result.0, "Asia/Kolkata");
    /// assert!(matches!(result.0, Cow::Owned(_)));
    /// assert_eq!(*result.1, "inccu");
    ///
    /// // Borrows when able:
    /// let result = mapper.canonicalize_iana("America/Chicago").unwrap();
    /// assert_eq!(result.0, "America/Chicago");
    /// assert!(matches!(result.0, Cow::Borrowed(_)));
    ///
    /// // Unknown IANA time zone ID:
    /// assert_eq!(mapper.canonicalize_iana("America/San_Francisco"), None);
    /// ```
    pub fn canonicalize_iana<'s>(
        &self,
        iana_id: &'s str,
    ) -> Option<(Cow<'s, str>, TimeZoneBcp47Id)> {
        // Note: We collect the cursors into a stack so that we start probing
        // nearby the input IANA identifier. This should improve lookup time since
        // most renames share the same prefix like "Asia" or "Europe".
        let mut stack = Vec::with_capacity(iana_id.len());
        let (trie_value, mut string) = self.iana_lookup_with_normalization(iana_id, |cursor| {
            stack.push((cursor.clone(), 0, 1));
        })?;
        let Some(bcp47_id) = self.data.bcp47_ids.get(trie_value.index()) else {
            debug_assert!(false, "index should be in range");
            return None;
        };
        if trie_value.is_canonical() {
            return Some((string, bcp47_id));
        }
        // If we get here, we need to walk the trie to find the canonical IANA ID.
        let needle = trie_value.to_canonical();
        if !string.contains('/') {
            string.to_mut().insert(0, '_');
        }
        let Some(string) = self.iana_search(needle, string.into_owned(), stack) else {
            debug_assert!(false, "every time zone should have a canonical IANA ID");
            return None;
        };
        Some((Cow::Owned(string), bcp47_id))
    }

    /// Returns the canonical, normalized IANA ID of the given BCP-47 ID.
    ///
    /// This function performs a linear search over all IANA IDs. If this is problematic, consider one of the
    /// following functions instead:
    ///
    /// 1. [`TimeZoneIdMapperBorrowed::canonicalize_iana()`]
    ///    is faster if you have an IANA ID.
    /// 2. [`TimeZoneIdMapperWithFastCanonicalizationBorrowed::canonical_iana_from_bcp47()`]
    ///    is faster, but it requires loading additional data
    ///    (see [`TimeZoneIdMapperWithFastCanonicalization`]).
    ///
    /// Returns `None` if the BCP-47 ID is not found.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_timezone::TimeZoneBcp47Id;
    /// use icu_timezone::TimeZoneIdMapper;
    /// use std::borrow::Cow;
    /// use tinystr::tinystr;
    ///
    /// let mapper = TimeZoneIdMapper::new();
    ///
    /// let bcp47_id = TimeZoneBcp47Id(tinystr!(8, "inccu"));
    /// let result = mapper.find_canonical_iana_from_bcp47(bcp47_id).unwrap();
    ///
    /// assert_eq!(result, "Asia/Kolkata");
    ///
    /// // Unknown BCP-47 time zone ID:
    /// let bcp47_id = TimeZoneBcp47Id(tinystr!(8, "ussfo"));
    /// assert_eq!(mapper.find_canonical_iana_from_bcp47(bcp47_id), None);
    /// ```
    pub fn find_canonical_iana_from_bcp47(&self, bcp47_id: TimeZoneBcp47Id) -> Option<String> {
        let index = self.data.bcp47_ids.binary_search(&bcp47_id).ok()?;
        let stack = alloc::vec![(self.data.map.cursor(), 0, 0)];
        let needle = IanaTrieValue::canonical_for_index(index);
        let string = self.iana_search(needle, String::new(), stack)?;
        Some(string)
    }

    /// Queries the data for `iana_id` without recording the normalized string.
    /// This is a fast, no-alloc lookup.
    fn iana_lookup_quick(&self, iana_id: impl AsRef<[u8]>) -> Option<IanaTrieValue> {
        let mut cursor = self.data.map.cursor();
        let iana_id = iana_id.as_ref();
        if !iana_id.contains(&b'/') {
            cursor.step(NON_REGION_CITY_PREFIX);
        }
        for &b in iana_id {
            cursor.step(b);
        }
        cursor.take_value().map(IanaTrieValue)
    }

    /// Queries the data for `iana_id` while keeping track of the normalized string.
    /// This is a fast lookup, but it may require allocating memory.
    fn iana_lookup_with_normalization<'l, 's>(
        &'l self,
        iana_id: &'s str,
        mut cursor_fn: impl FnMut(&ZeroAsciiIgnoreCaseTrieCursor<'l>),
    ) -> Option<(IanaTrieValue, Cow<'s, str>)> {
        let mut cursor = self.data.map.cursor();
        if !iana_id.contains('/') {
            cursor_fn(&cursor);
            cursor.step(NON_REGION_CITY_PREFIX);
        }
        let mut string = Cow::Borrowed(iana_id);
        let mut i = 0;
        let trie_value = loop {
            cursor_fn(&cursor);
            let Some(&input_byte) = string.as_bytes().get(i) else {
                break cursor.take_value().map(IanaTrieValue);
            };
            let Some(matched_byte) = cursor.step(input_byte) else {
                break None;
            };
            if matched_byte != input_byte {
                // Safety: we write to input_byte farther down after performing safety checks.
                let Some(input_byte) = unsafe { string.to_mut().as_bytes_mut() }.get_mut(i) else {
                    debug_assert!(false, "the same index was just accessed earlier");
                    break None;
                };
                if !input_byte.is_ascii() {
                    debug_assert!(false, "non-ASCII input byte: {input_byte}");
                    break None;
                }
                if !matched_byte.is_ascii() {
                    debug_assert!(false, "non-ASCII matched byte: {matched_byte}");
                    break None;
                }
                // Safety: we just checked that both input_byte and matched_byte are ASCII,
                // so the buffer remains UTF-8 when we replace one with the other.
                *input_byte = matched_byte;
            }
            i += 1;
        }?;
        Some((trie_value, string))
    }

    /// Performs a reverse lookup by walking the trie with an optional start position.
    /// This is not a fast operation since it requires a linear search.
    fn iana_search(
        &self,
        needle: IanaTrieValue,
        mut string: String,
        mut stack: Vec<(ZeroAsciiIgnoreCaseTrieCursor, usize, usize)>,
    ) -> Option<String> {
        loop {
            let Some((mut cursor, index, suffix_len)) = stack.pop() else {
                // Nothing left in the trie.
                return None;
            };
            // Check to see if there is a value at the current node.
            if let Some(candidate) = cursor.take_value().map(IanaTrieValue) {
                if candidate == needle {
                    // Success! Found what we were looking for.
                    return Some(string);
                }
            }
            // Now check for children of the current node.
            let mut sub_cursor = cursor.clone();
            if let Some(probe_result) = sub_cursor.probe(index) {
                // Found a child. Add the current byte edge to the string.
                if !probe_result.byte.is_ascii() {
                    debug_assert!(false, "non-ASCII probe byte: {}", probe_result.byte);
                    return None;
                }
                // Safety: the byte being added is ASCII as guarded above
                unsafe { string.as_mut_vec().push(probe_result.byte) };
                // Add the child to the stack, and also add back the current
                // node if there are more siblings to visit.
                if index + 1 < probe_result.total_siblings as usize {
                    stack.push((cursor, index + 1, suffix_len));
                    stack.push((sub_cursor, 0, 1));
                } else {
                    stack.push((sub_cursor, 0, suffix_len + 1));
                }
            } else {
                // No more children. Pop this node's bytes from the string.
                for _ in 0..suffix_len {
                    // Safety: we check that the bytes being removed are ASCII
                    let removed_byte = unsafe { string.as_mut_vec().pop() };
                    if let Some(removed_byte) = removed_byte {
                        if !removed_byte.is_ascii() {
                            debug_assert!(false, "non-ASCII removed byte: {removed_byte}");
                            // If we get here for some reason, `string` is not in a valid state,
                            // so to be extra safe, we can clear it.
                            string.clear();
                            return None;
                        }
                    } else {
                        debug_assert!(false, "could not remove another byte");
                        return None;
                    }
                }
            }
        }
    }
}

/// A mapper that supplements [`TimeZoneIdMapper`] with about 8 KB of additional data to
/// improve the performance of canonical IANA ID lookup.
///
/// The data in [`TimeZoneIdMapper`] is optimized for IANA to BCP-47 lookup; the reverse
/// requires a linear walk over all ~600 IANA identifiers. The data added here allows for
/// constant-time mapping from BCP-47 to IANA.
#[derive(Debug, Clone)]
pub struct TimeZoneIdMapperWithFastCanonicalization<I> {
    inner: I,
    data: DataPayload<Bcp47ToIanaMapV1Marker>,
}

impl TimeZoneIdMapperWithFastCanonicalization<TimeZoneIdMapper> {
    /// Creates a new [`TimeZoneIdMapperWithFastCanonicalization`] using compiled data.
    ///
    /// See [`TimeZoneIdMapperWithFastCanonicalization`] for an example.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    #[allow(clippy::new_ret_no_self)]
    pub fn new() -> TimeZoneIdMapperWithFastCanonicalizationBorrowed<'static> {
        TimeZoneIdMapperWithFastCanonicalizationBorrowed::new()
    }

    icu_provider::gen_any_buffer_data_constructors!(() -> error: DataError,
        functions: [
            new: skip,
            try_new_with_any_provider,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self,
        ]
    );

    #[doc = icu_provider::gen_any_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_unstable<P>(provider: &P) -> Result<Self, DataError>
    where
        P: DataProvider<IanaToBcp47MapV3Marker> + DataProvider<Bcp47ToIanaMapV1Marker> + ?Sized,
    {
        let mapper = TimeZoneIdMapper::try_new_unstable(provider)?;
        Self::try_new_with_mapper_unstable(provider, mapper)
    }
}

impl<I> TimeZoneIdMapperWithFastCanonicalization<I>
where
    I: AsRef<TimeZoneIdMapper>,
{
    /// Creates a new [`TimeZoneIdMapperWithFastCanonicalization`] using compiled data
    /// and a pre-existing [`TimeZoneIdMapper`], which can be borrowed.
    ///
    /// See [`TimeZoneIdMapperWithFastCanonicalization`] for an example.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn try_new_with_mapper(mapper: I) -> Result<Self, DataError> {
        Self {
            inner: mapper,
            data: DataPayload::from_static_ref(
                crate::provider::Baked::SINGLETON_BCP47_TO_IANA_MAP_V1_MARKER,
            ),
        }
        .validated()
    }

    icu_provider::gen_any_buffer_data_constructors!((mapper: I) -> error: DataError,
        functions: [
            try_new_with_mapper: skip,
            try_new_with_mapper_with_any_provider,
            try_new_with_mapper_with_buffer_provider,
            try_new_with_mapper_unstable,
            Self,
        ]
    );

    #[doc = icu_provider::gen_any_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_with_mapper_unstable<P>(provider: &P, mapper: I) -> Result<Self, DataError>
    where
        P: DataProvider<IanaToBcp47MapV3Marker> + DataProvider<Bcp47ToIanaMapV1Marker> + ?Sized,
    {
        let data = provider.load(Default::default())?.payload;
        Self {
            inner: mapper,
            data,
        }
        .validated()
    }

    fn validated(self) -> Result<Self, DataError> {
        if self.inner.as_ref().data.get().bcp47_ids_checksum != self.data.get().bcp47_ids_checksum {
            return Err(
                DataErrorKind::InconsistentData(IanaToBcp47MapV3Marker::INFO)
                    .with_marker(Bcp47ToIanaMapV1Marker::INFO),
            );
        }
        Ok(self)
    }

    /// Gets the inner [`TimeZoneIdMapper`] for performing queries.
    pub fn inner(&self) -> &TimeZoneIdMapper {
        self.inner.as_ref()
    }

    /// Returns a borrowed version of the mapper that can be queried.
    ///
    /// This avoids a small potential indirection cost when querying the mapper.
    pub fn as_borrowed(&self) -> TimeZoneIdMapperWithFastCanonicalizationBorrowed {
        TimeZoneIdMapperWithFastCanonicalizationBorrowed {
            inner: self.inner.as_ref().as_borrowed(),
            data: self.data.get(),
        }
    }
}

/// A borrowed wrapper around the time zone ID mapper, returned by
/// [`TimeZoneIdMapperWithFastCanonicalization::as_borrowed()`]. More efficient to query.
#[derive(Debug, Copy, Clone)]
pub struct TimeZoneIdMapperWithFastCanonicalizationBorrowed<'a> {
    inner: TimeZoneIdMapperBorrowed<'a>,
    data: &'a Bcp47ToIanaMapV1<'a>,
}

#[cfg(feature = "compiled_data")]
impl Default for TimeZoneIdMapperWithFastCanonicalizationBorrowed<'static> {
    fn default() -> Self {
        Self::new()
    }
}

impl TimeZoneIdMapperWithFastCanonicalizationBorrowed<'static> {
    /// Creates a new [`TimeZoneIdMapperWithFastCanonicalizationBorrowed`] using compiled data.
    ///
    /// See [`TimeZoneIdMapperWithFastCanonicalizationBorrowed`] for an example.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub fn new() -> Self {
        const _: () = assert!(
            crate::provider::Baked::SINGLETON_IANA_TO_BCP47_MAP_V3_MARKER.bcp47_ids_checksum
                == crate::provider::Baked::SINGLETON_BCP47_TO_IANA_MAP_V1_MARKER.bcp47_ids_checksum,
        );
        Self {
            inner: TimeZoneIdMapperBorrowed::new(),
            data: crate::provider::Baked::SINGLETON_BCP47_TO_IANA_MAP_V1_MARKER,
        }
    }

    /// Cheaply converts a [`TimeZoneIdMapperWithFastCanonicalizationBorrowed<'static>`] into a [`TimeZoneIdMapperWithFastCanonicalization`].
    ///
    /// Note: Due to branching and indirection, using [`TimeZoneIdMapperWithFastCanonicalization`] might inhibit some
    /// compile-time optimizations that are possible with [`TimeZoneIdMapperWithFastCanonicalizationBorrowed`].
    pub fn static_to_owned(&self) -> TimeZoneIdMapperWithFastCanonicalization<TimeZoneIdMapper> {
        TimeZoneIdMapperWithFastCanonicalization {
            inner: self.inner.static_to_owned(),
            data: DataPayload::from_static_ref(self.data),
        }
    }
}

impl<'a> TimeZoneIdMapperWithFastCanonicalizationBorrowed<'a> {
    /// Gets the inner [`TimeZoneIdMapperBorrowed`] for performing queries.
    pub fn inner(&self) -> TimeZoneIdMapperBorrowed<'a> {
        self.inner
    }

    /// Returns the canonical, normalized identifier of the given IANA time zone.
    ///
    /// Also returns the BCP-47 time zone ID.
    ///
    /// This is a faster version of [`TimeZoneIdMapperBorrowed::canonicalize_iana()`]
    /// and it always returns borrowed IANA strings, but it requires loading additional data
    /// (see [`TimeZoneIdMapperWithFastCanonicalization`]).
    ///
    /// Returns `None` if the IANA ID is not found.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_timezone::TimeZoneBcp47Id;
    /// use icu_timezone::TimeZoneIdMapperWithFastCanonicalization;
    /// use std::borrow::Cow;
    ///
    /// let mapper = TimeZoneIdMapperWithFastCanonicalization::new();
    ///
    /// let result = mapper.canonicalize_iana("Asia/CALCUTTA").unwrap();
    ///
    /// // The Cow is always returned borrowed:
    /// assert_eq!(result.0, "Asia/Kolkata");
    /// assert_eq!(*result.1, "inccu");
    ///
    /// // Unknown IANA time zone ID:
    /// assert_eq!(mapper.canonicalize_iana("America/San_Francisco"), None);
    /// ```
    pub fn canonicalize_iana(&self, iana_id: &str) -> Option<(&str, TimeZoneBcp47Id)> {
        let trie_value = self.inner.iana_lookup_quick(iana_id)?;
        let Some(bcp47_id) = self.inner.data.bcp47_ids.get(trie_value.index()) else {
            debug_assert!(false, "index should be in range");
            return None;
        };
        let Some(string) = self.data.canonical_iana_ids.get(trie_value.index()) else {
            debug_assert!(false, "index should be in range");
            return None;
        };
        Some((string, bcp47_id))
    }

    /// Returns the canonical, normalized IANA ID of the given BCP-47 ID.
    ///
    /// This is a faster version of [`TimeZoneIdMapperBorrowed::find_canonical_iana_from_bcp47()`]
    /// and it always returns borrowed IANA strings, but it requires loading additional data
    /// (see [`TimeZoneIdMapperWithFastCanonicalization`]).
    ///
    /// Returns `None` if the BCP-47 ID is not found.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_timezone::TimeZoneBcp47Id;
    /// use icu_timezone::TimeZoneIdMapperWithFastCanonicalization;
    /// use std::borrow::Cow;
    /// use tinystr::tinystr;
    ///
    /// let mapper = TimeZoneIdMapperWithFastCanonicalization::new();
    ///
    /// let bcp47_id = TimeZoneBcp47Id(tinystr!(8, "inccu"));
    /// let result = mapper.canonical_iana_from_bcp47(bcp47_id).unwrap();
    ///
    /// // The Cow is always returned borrowed:
    /// assert_eq!(result, "Asia/Kolkata");
    ///
    /// // Unknown BCP-47 time zone ID:
    /// let bcp47_id = TimeZoneBcp47Id(tinystr!(8, "ussfo"));
    /// assert_eq!(mapper.canonical_iana_from_bcp47(bcp47_id), None);
    /// ```
    pub fn canonical_iana_from_bcp47(&self, bcp47_id: TimeZoneBcp47Id) -> Option<&str> {
        let index = self.inner.data.bcp47_ids.binary_search(&bcp47_id).ok()?;
        let Some(string) = self.data.canonical_iana_ids.get(index) else {
            debug_assert!(false, "index should be in range");
            return None;
        };
        Some(string)
    }
}

#[derive(Copy, Clone, PartialEq, Eq)]
#[repr(transparent)]
struct IanaTrieValue(usize);

impl IanaTrieValue {
    #[inline]
    pub(crate) fn to_canonical(self) -> Self {
        Self(self.0 | 1)
    }
    #[inline]
    pub(crate) fn canonical_for_index(index: usize) -> Self {
        Self(index << 1).to_canonical()
    }
    #[inline]
    pub(crate) fn index(self) -> usize {
        self.0 >> 1
    }
    #[inline]
    pub(crate) fn is_canonical(self) -> bool {
        (self.0 & 0x1) != 0
    }
}

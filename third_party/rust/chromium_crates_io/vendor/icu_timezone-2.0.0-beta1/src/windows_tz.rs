// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::fmt::Write;

use icu_provider::{
    prelude::icu_locale_core::subtags::{region, Region},
    DataError, DataPayload, DataProvider,
};

use crate::{
    provider::windows::{WindowsZonesToBcp47MapV1, WindowsZonesToBcp47MapV1Marker},
    TimeZoneBcp47Id,
};

/// A mapper between Windows time zone identifier and a BCP-47 ID.
///
/// This mapper currently only supports mapping from windows time zone identifiers
/// to BCP-47 identifiers.
///
/// A windows time zone may vary depending on an associated territory/region. This is represented
/// by the internal data mapping by delimiting the windows time zone and territory/region
/// code with a "/".
///
/// For instance, Central Standard Time can vary depending on the provided regions listed below:
///
/// - Central Standard Time/001
/// - Central Standard Time/US
/// - Central Standard Time/CA
/// - Central Standard Time/MX
/// - Central Standard Time/ZZ
///
/// As such, a [`Region`] may be provided to further specify a desired territory/region when
/// querying a BCP-47 identifier. If no region is provided or the specificity is not required,
/// then the territory will default to the M.49 World Code, `001`.
#[derive(Debug)]
pub struct WindowsTimeZoneMapper {
    data: DataPayload<WindowsZonesToBcp47MapV1Marker>,
}

impl WindowsTimeZoneMapper {
    /// Creates a new static [`WindowsTimeZoneMapperBorrowed`].
    #[allow(clippy::new_ret_no_self)]
    #[cfg(feature = "compiled_data")]
    pub fn new() -> WindowsTimeZoneMapperBorrowed<'static> {
        WindowsTimeZoneMapperBorrowed::new()
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
        P: DataProvider<WindowsZonesToBcp47MapV1Marker> + ?Sized,
    {
        let data = provider.load(Default::default())?.payload;
        Ok(Self { data })
    }

    /// Returns the borrowed version of the mapper that can be queried from
    /// the owned mapper.
    ///
    /// Using the borrowed version allows one to avoid a small potential
    /// indirection cost when querying the mapper from the owned version.
    pub fn as_borrowed(&self) -> WindowsTimeZoneMapperBorrowed {
        WindowsTimeZoneMapperBorrowed {
            data: self.data.get(),
        }
    }
}

/// A borrowed wrapper around the windows time zone mapper data.
#[derive(Debug)]
pub struct WindowsTimeZoneMapperBorrowed<'a> {
    data: &'a WindowsZonesToBcp47MapV1<'a>,
}

#[cfg(feature = "compiled_data")]
impl Default for WindowsTimeZoneMapperBorrowed<'_> {
    fn default() -> Self {
        Self::new()
    }
}

impl WindowsTimeZoneMapperBorrowed<'_> {
    /// Creates a new static [`WindowsTimeZoneMapperBorrowed`].
    #[cfg(feature = "compiled_data")]
    pub fn new() -> Self {
        WindowsTimeZoneMapperBorrowed {
            data: crate::provider::Baked::SINGLETON_WINDOWS_ZONES_TO_BCP47_MAP_V1_MARKER,
        }
    }

    /// Returns the BCP-47 ID for a provided Windows time zone and [`Region`] with a case sensitive query.
    ///
    /// If no region is provided or the specificity is not required,
    /// then the territory will default to the M.49 World Code, `001`.
    ///
    /// ```rust
    /// use icu::locale::subtags::region;
    /// use icu::timezone::{TimeZoneBcp47Id, WindowsTimeZoneMapper};
    /// use tinystr::tinystr;
    ///
    /// let win_tz_mapper = WindowsTimeZoneMapper::new();
    ///
    /// let bcp47_id =
    ///     win_tz_mapper.windows_tz_to_bcp47_id("Central Standard Time", None);
    /// assert_eq!(bcp47_id, Some(TimeZoneBcp47Id(tinystr!(8, "uschi"))));
    ///
    /// let bcp47_id = win_tz_mapper
    ///     .windows_tz_to_bcp47_id("Central Standard Time", Some(region!("US")));
    /// assert_eq!(bcp47_id, Some(TimeZoneBcp47Id(tinystr!(8, "uschi"))));
    ///
    /// let bcp47_id = win_tz_mapper
    ///     .windows_tz_to_bcp47_id("Central Standard Time", Some(region!("CA")));
    /// assert_eq!(bcp47_id, Some(TimeZoneBcp47Id(tinystr!(8, "cawnp"))));
    /// ```
    pub fn windows_tz_to_bcp47_id(
        &self,
        windows_tz: &str,
        region: Option<Region>,
    ) -> Option<TimeZoneBcp47Id> {
        let mut cursor = self.data.map.cursor();
        // Returns None if input is non-ASCII
        cursor.write_str(windows_tz).ok()?;
        cursor.step(b'/');
        cursor
            .write_str(region.unwrap_or(region!("001")).as_str())
            // region is valid ASCII, but we can do this instead of unwrap
            .ok()?;
        self.data.bcp47_ids.get(cursor.take_value()?)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tinystr::tinystr;

    #[test]
    fn basic_windows_tz_lookup() {
        let win_map = WindowsTimeZoneMapper::new();

        let result = win_map.windows_tz_to_bcp47_id("Central Standard Time", None);
        assert_eq!(result, Some(TimeZoneBcp47Id(tinystr!(8, "uschi"))));

        let result = win_map.windows_tz_to_bcp47_id("Eastern Standard Time", None);
        assert_eq!(result, Some(TimeZoneBcp47Id(tinystr!(8, "usnyc"))));

        let result = win_map.windows_tz_to_bcp47_id("Eastern Standard Time", Some(region!("CA")));
        assert_eq!(result, Some(TimeZoneBcp47Id(tinystr!(8, "cator"))));

        let result = win_map.windows_tz_to_bcp47_id("GMT Standard Time", None);
        assert_eq!(result, Some(TimeZoneBcp47Id(tinystr!(8, "gblon"))));
    }
}

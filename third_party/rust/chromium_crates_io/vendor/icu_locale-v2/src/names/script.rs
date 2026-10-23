// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::DisplayNamesPreferences;
use super::{
    impl_writeable_for_single_display_name_borrowed, impl_writeable_for_single_display_name_owned,
    load_one,
};
use crate::provider::names::{
    LocaleNamesScriptMediumHeavyV1, LocaleNamesScriptMediumLightV1, LocaleNamesScriptMediumTinyV1,
    LocaleNamesScriptShortHeavyV1,
};
use icu_locale_core::subtags::Script;
use icu_provider::{DataPayloadOr, prelude::*};

#[inline]
fn make_attributes(subtag: &Script) -> &DataMarkerAttributes {
    // All script markers use the same attributes.
    // Valid Script subtags conform to DataMarkerAttributes syntax.
    DataMarkerAttributes::from_str_or_panic(subtag.as_str())
}

#[inline]
fn make_locale(prefs: DisplayNamesPreferences) -> DataLocale {
    // All script markers use the same locale
    LocaleNamesScriptMediumTinyV1::make_locale(prefs.locale_preferences)
}

macro_rules! table_row {
    (try_new_tiny) => {
        "| [`try_new_tiny`](Self::try_new_tiny) | \"Latin\" | ❌ | ❌ |"
    };
    (try_new_light) => {
        "| [`try_new_light`](Self::try_new_light) | \"Latin\" | \"Unknown Script\" | ❌ |"
    };
    (try_new_heavy) => {
        "| [`try_new_heavy`](Self::try_new_heavy) | \"Latin\" | \"Unknown Script\" | \"Sumero-Akkadian Cuneiform\" |"
    };
    (try_new_short_heavy) => {
        "| [`try_new_short_heavy`](Self::try_new_short_heavy) | \"Latin\" | \"Unknown Script\" | \"S-A Cuneiform\" |"
    };
}

/// A localized display name for a single script, owned version.
///
/// # Constructor Behavior
///
/// There are several constructors, each of which links different data and serve
/// different use cases. The behavior is illustrated in the table below.
///
/// | Constructor | `Latn` | `Zzzz` | `Xsux` |
/// | :--- | :--- | :--- | :--- |
#[doc = concat!(table_row!(try_new_tiny), "\n")]
#[doc = concat!(table_row!(try_new_light), "\n")]
#[doc = concat!(table_row!(try_new_heavy), "\n")]
#[doc = concat!(table_row!(try_new_short_heavy), "\n")]
///
/// > Note: :x: means that the constructor returns an error.
///
/// There are fallible (`try_new_*`) and infallible (`new_*_with_fallback`) versions of
/// all constructors.
///
/// # Example
///
/// ```
/// use icu::locale::names::ScriptDisplayName;
/// use icu::locale::{locale, subtags::script};
/// use writeable::assert_writeable_eq;
///
/// let display_name =
///     ScriptDisplayName::try_new_light(locale!("en").into(), script!("Latn"))
///         .expect("Data should load successfully");
///
/// assert_writeable_eq!(display_name, "Latin");
/// ```
#[derive(Debug)]
pub struct ScriptDisplayName {
    pub(crate) payload: DataPayloadOr<LocaleNamesScriptMediumLightV1, Script>,
}

impl ScriptDisplayName {
    /// Loads a script display name in a formatting locale using compiled data.
    ///
    /// The `light` constructor links data for all common scripts.
    /// See the [class docs](Self) for information on which constructor to use.
    ///
    /// If the display name is not found in data, the BCP-47 code is returned. To detect this case
    /// and return an error instead, use [`ScriptDisplayName::try_new_light()`].
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::locale::names::ScriptDisplayName;
    /// use icu::locale::{locale, subtags::script};
    /// use writeable::assert_writeable_eq;
    ///
    /// assert_writeable_eq!(
    ///     ScriptDisplayName::new_light_with_fallback(
    ///         locale!("bs").into(),
    ///         script!("Cyrl")
    ///     ),
    ///     "ćirilica"
    /// );
    ///
    /// assert_writeable_eq!(
    ///     ScriptDisplayName::new_light_with_fallback(
    ///         locale!("zh").into(),
    ///         script!("Cyrl")
    ///     ),
    ///     "西里尔文"
    /// );
    /// ```
    #[cfg(feature = "compiled_data")]
    pub fn new_light_with_fallback(prefs: DisplayNamesPreferences, script: Script) -> Self {
        Self::try_new_light(prefs, script).unwrap_or(Self {
            payload: DataPayloadOr::from_other(script),
        })
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, script: Script) -> result: Result<Self, DataError>,
        /// Loads a script display name in a formatting locale using compiled data.
        ///
        /// The `light` constructor links data for all common scripts.
        /// See the [class docs](Self) for information on which constructor to use.
        ///
        /// Returns an error if the display name is not found in data. To return the BCP-47 code
        /// instead, use [`ScriptDisplayName::new_light_with_fallback()`].
        ///
        /// ✨ *Enabled with the `compiled_data` Cargo feature.*
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::names::ScriptDisplayName;
        /// use icu::locale::{locale, subtags::script};
        /// use writeable::assert_writeable_eq;
        ///
        /// let name = ScriptDisplayName::try_new_light(locale!("bs").into(), script!("Cyrl")).unwrap();
        /// assert_writeable_eq!(
        ///     name,
        ///     "ćirilica"
        /// );
        ///
        /// let name = ScriptDisplayName::try_new_light(locale!("zh").into(), script!("Cyrl")).unwrap();
        /// assert_writeable_eq!(
        ///     name,
        ///     "西里尔文"
        /// );
        /// ```
        functions: [
            try_new_light,
            try_new_light_with_buffer_provider,
            try_new_light_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_light)]
    pub fn try_new_light_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        script: Script,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<LocaleNamesScriptMediumLightV1>
            + DataProvider<LocaleNamesScriptMediumTinyV1>,
    {
        let attrs = make_attributes(&script);
        let locale = make_locale(prefs);
        let payload = load_one::<LocaleNamesScriptMediumLightV1, _, _>(provider, &locale, attrs)?
            .map_or_else(
                || load_one::<LocaleNamesScriptMediumTinyV1, _, _>(provider, &locale, attrs),
                |p| Ok(Some(p)),
            )?
            .map(DataPayloadOr::from_payload)
            .ok_or_else(|| DataErrorKind::IdentifierNotFound.into_error())?;
        Ok(Self { payload })
    }

    /// Loads a script display name in a formatting locale using compiled data.
    ///
    /// The `tiny` constructor links an extremely limited amount of data, with a focus on
    /// scripts associated with the formatting locale. For example, the Cyrillic script
    /// is included in `bs` (Bosnian) but not `zh` (Chinese).
    /// See the [class docs](Self) for more information on which constructor to use.
    ///
    /// If the display name is not found in data, the BCP-47 code is returned. To detect this case
    /// and return an error instead, use [`ScriptDisplayName::try_new_tiny()`].
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::locale::names::ScriptDisplayName;
    /// use icu::locale::{locale, subtags::script};
    /// use writeable::assert_writeable_eq;
    ///
    /// assert_writeable_eq!(
    ///     ScriptDisplayName::new_tiny_with_fallback(
    ///         locale!("bs").into(),
    ///         script!("Cyrl")
    ///     ),
    ///     "ćirilica"
    /// );
    ///
    /// // Name for Cyrillic script is NOT included in the Chinese locale
    /// assert_writeable_eq!(
    ///     ScriptDisplayName::new_tiny_with_fallback(
    ///         locale!("zh").into(),
    ///         script!("Cyrl")
    ///     ),
    ///     "Cyrl"
    /// );
    /// ```
    #[cfg(feature = "compiled_data")]
    pub fn new_tiny_with_fallback(prefs: DisplayNamesPreferences, script: Script) -> Self {
        Self::try_new_tiny(prefs, script).unwrap_or(Self {
            payload: DataPayloadOr::from_other(script),
        })
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, script: Script) -> result: Result<Self, DataError>,
        /// Loads a script display name in a formatting locale using compiled data.
        ///
        /// The `tiny` constructor links an extremely limited amount of data, with a focus on
        /// scripts associated with the formatting locale. For example, the Cyrillic script
        /// is included in `bs` (Bosnian) but not `zh` (Chinese).
        /// See the [class docs](Self) for more information on which constructor to use.
        ///
        /// Returns an error if the display name is not found in data. To return the BCP-47 code
        /// instead, use [`ScriptDisplayName::new_tiny_with_fallback()`].
        ///
        /// ✨ *Enabled with the `compiled_data` Cargo feature.*
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::names::ScriptDisplayName;
        /// use icu::locale::{locale, subtags::script};
        /// use writeable::assert_writeable_eq;
        ///
        /// let name = ScriptDisplayName::try_new_tiny(locale!("bs").into(), script!("Cyrl")).unwrap();
        /// assert_writeable_eq!(
        ///     name,
        ///     "ćirilica"
        /// );
        ///
        /// // Name for Cyrillic script is NOT included in the Chinese locale
        /// ScriptDisplayName::try_new_tiny(locale!("zh").into(), script!("Cyrl")).unwrap_err();
        /// ```
        functions: [
            try_new_tiny,
            try_new_tiny_with_buffer_provider,
            try_new_tiny_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_tiny)]
    pub fn try_new_tiny_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        script: Script,
    ) -> Result<Self, DataError>
    where
        D: ?Sized + DataProvider<LocaleNamesScriptMediumTinyV1>,
    {
        let attrs = make_attributes(&script);
        let locale = make_locale(prefs);
        let payload = load_one::<LocaleNamesScriptMediumTinyV1, _, _>(provider, &locale, attrs)?
            .map(DataPayloadOr::from_payload)
            .ok_or_else(|| DataErrorKind::IdentifierNotFound.into_error())?;
        Ok(Self { payload })
    }

    /// Loads a script display name in a formatting locale using compiled data.
    ///
    /// The `heavy` constructor includes additional data coverage for subtags that are
    /// less commonly formatted in the target locale.
    /// See the [class docs](Self) for information on which constructor to use.
    ///
    /// If the display name is not found in data, the BCP-47 code is returned. To detect this case
    /// and return an error instead, use [`ScriptDisplayName::try_new_heavy()`].
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::locale::names::ScriptDisplayName;
    /// use icu::locale::{locale, subtags::script};
    /// use writeable::assert_writeable_eq;
    ///
    /// assert_writeable_eq!(
    ///     ScriptDisplayName::new_heavy_with_fallback(
    ///         locale!("de").into(),
    ///         script!("Latn")
    ///     ),
    ///     "Lateinisch"
    /// );
    ///
    /// assert_writeable_eq!(
    ///     ScriptDisplayName::new_heavy_with_fallback(
    ///         locale!("de").into(),
    ///         script!("Xsux")
    ///     ),
    ///     "Sumerisch-akkadische Keilschrift"
    /// );
    /// ```
    #[cfg(feature = "compiled_data")]
    pub fn new_heavy_with_fallback(prefs: DisplayNamesPreferences, script: Script) -> Self {
        Self::try_new_heavy(prefs, script).unwrap_or(Self {
            payload: DataPayloadOr::from_other(script),
        })
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, script: Script) -> result: Result<Self, DataError>,
        /// Loads a script display name in a formatting locale using compiled data.
        ///
        /// The `heavy` constructor includes additional data coverage for subtags that are
        /// less commonly formatted in the target locale.
        /// See the [class docs](Self) for information on which constructor to use.
        ///
        /// Returns an error if the display name is not found in data. To return the BCP-47 code
        /// instead, use [`ScriptDisplayName::new_heavy_with_fallback()`].
        ///
        /// ✨ *Enabled with the `compiled_data` Cargo feature.*
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::names::ScriptDisplayName;
        /// use icu::locale::{locale, subtags::script};
        /// use writeable::assert_writeable_eq;
        ///
        /// let name = ScriptDisplayName::try_new_heavy(locale!("de").into(), script!("Latn")).unwrap();
        /// assert_writeable_eq!(
        ///     name,
        ///     "Lateinisch"
        /// );
        ///
        /// let name = ScriptDisplayName::try_new_heavy(locale!("de").into(), script!("Xsux")).unwrap();
        /// assert_writeable_eq!(
        ///     name,
        ///     "Sumerisch-akkadische Keilschrift"
        /// );
        /// ```
        functions: [
            try_new_heavy,
            try_new_heavy_with_buffer_provider,
            try_new_heavy_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_heavy)]
    pub fn try_new_heavy_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        script: Script,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<LocaleNamesScriptMediumHeavyV1>
            + DataProvider<LocaleNamesScriptMediumLightV1>
            + DataProvider<LocaleNamesScriptMediumTinyV1>,
    {
        let attrs = make_attributes(&script);
        let locale = make_locale(prefs);
        let payload = load_one::<LocaleNamesScriptMediumHeavyV1, _, _>(provider, &locale, attrs)?
            .map_or_else(
                || load_one::<LocaleNamesScriptMediumLightV1, _, _>(provider, &locale, attrs),
                |p| Ok(Some(p)),
            )?
            .map_or_else(
                || load_one::<LocaleNamesScriptMediumTinyV1, _, _>(provider, &locale, attrs),
                |p| Ok(Some(p)),
            )?
            .map(DataPayloadOr::from_payload)
            .ok_or_else(|| DataErrorKind::IdentifierNotFound.into_error())?;
        Ok(Self { payload })
    }

    /// Loads a short script display name in a formatting locale using compiled data.
    ///
    /// Falls back to default (medium) length if a short name is not available.
    ///
    /// The `heavy` constructor includes additional data coverage for subtags that are
    /// less commonly formatted in the target locale.
    /// See the [class docs](Self) for information on which constructor to use.
    ///
    /// If the display name is not found in data, the BCP-47 code is returned. To detect this case
    /// and return an error instead, use [`ScriptDisplayName::try_new_short_heavy()`].
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::locale::names::ScriptDisplayName;
    /// use icu::locale::{locale, subtags::script};
    /// use writeable::assert_writeable_eq;
    ///
    /// assert_writeable_eq!(
    ///     ScriptDisplayName::new_short_heavy_with_fallback(
    ///         locale!("de").into(),
    ///         script!("Latn")
    ///     ),
    ///     "Lateinisch"
    /// );
    ///
    /// // Example short name: script Xsux -> "S-A Cuneiform" in en
    /// assert_writeable_eq!(
    ///     ScriptDisplayName::new_short_heavy_with_fallback(
    ///         locale!("en").into(),
    ///         script!("Xsux")
    ///     ),
    ///     "S-A Cuneiform"
    /// );
    /// ```
    #[cfg(feature = "compiled_data")]
    pub fn new_short_heavy_with_fallback(prefs: DisplayNamesPreferences, script: Script) -> Self {
        Self::try_new_short_heavy(prefs, script).unwrap_or(Self {
            payload: DataPayloadOr::from_other(script),
        })
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, script: Script) -> result: Result<Self, DataError>,
        /// Loads a short script display name in a formatting locale using compiled data.
        ///
        /// Falls back to default (medium) length if a short name is not available.
        ///
        /// The `heavy` constructor includes additional data coverage for subtags that are
        /// less commonly formatted in the target locale.
        /// See the [class docs](Self) for information on which constructor to use.
        ///
        /// Returns an error if the display name is not found in data. To return the BCP-47 code
        /// instead, use [`ScriptDisplayName::new_short_heavy_with_fallback()`].
        ///
        /// ✨ *Enabled with the `compiled_data` Cargo feature.*
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::names::ScriptDisplayName;
        /// use icu::locale::{locale, subtags::script};
        /// use writeable::assert_writeable_eq;
        ///
        /// let name = ScriptDisplayName::try_new_short_heavy(locale!("de").into(), script!("Latn")).unwrap();
        /// assert_writeable_eq!(
        ///     name,
        ///     "Lateinisch"
        /// );
        ///
        /// // Example short name: script Xsux -> "S-A Cuneiform" in en
        /// let name = ScriptDisplayName::try_new_short_heavy(locale!("en").into(), script!("Xsux")).unwrap();
        /// assert_writeable_eq!(
        ///     name,
        ///     "S-A Cuneiform"
        /// );
        /// ```
        functions: [
            try_new_short_heavy,
            try_new_short_heavy_with_buffer_provider,
            try_new_short_heavy_unstable,
            Self
        ]
    );

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_short_heavy)]
    pub fn try_new_short_heavy_unstable<D>(
        provider: &D,
        prefs: DisplayNamesPreferences,
        script: Script,
    ) -> Result<Self, DataError>
    where
        D: ?Sized
            + DataProvider<LocaleNamesScriptShortHeavyV1>
            + DataProvider<LocaleNamesScriptMediumHeavyV1>
            + DataProvider<LocaleNamesScriptMediumLightV1>
            + DataProvider<LocaleNamesScriptMediumTinyV1>,
    {
        let attrs = make_attributes(&script);
        let locale = make_locale(prefs);
        let payload = load_one::<LocaleNamesScriptShortHeavyV1, _, _>(provider, &locale, attrs)?
            .map_or_else(
                || load_one::<LocaleNamesScriptMediumHeavyV1, _, _>(provider, &locale, attrs),
                |p| Ok(Some(p)),
            )?
            .map_or_else(
                || load_one::<LocaleNamesScriptMediumLightV1, _, _>(provider, &locale, attrs),
                |p| Ok(Some(p)),
            )?
            .map_or_else(
                || load_one::<LocaleNamesScriptMediumTinyV1, _, _>(provider, &locale, attrs),
                |p| Ok(Some(p)),
            )?
            .map(DataPayloadOr::from_payload)
            .ok_or_else(|| DataErrorKind::IdentifierNotFound.into_error())?;
        Ok(Self { payload })
    }

    #[inline]
    fn borrow_str(&self) -> &str {
        match self.payload.get() {
            Ok(s) => s,
            Err(subtag) => subtag.as_str(),
        }
    }

    /// Returns a borrowed version of this display name.
    pub fn as_borrowed(&self) -> ScriptDisplayNameBorrowed<'_> {
        ScriptDisplayNameBorrowed {
            value: self.borrow_str(),
        }
    }
}

impl_writeable_for_single_display_name_owned!(ScriptDisplayName);

/// A localized display name for a single script.
#[derive(Debug, Clone, Copy)]
pub struct ScriptDisplayNameBorrowed<'a> {
    value: &'a str,
}

impl_writeable_for_single_display_name_borrowed!(ScriptDisplayNameBorrowed);

#[cfg(test)]
mod tests {
    use super::*;
    use icu_locale_core::{locale, subtags::script};

    #[test]
    fn test_script_display_name_owned_table() {
        let prefs_en = DisplayNamesPreferences::from(locale!("en"));
        let inputs = [script!("Latn"), script!("Zzzz"), script!("Xsux")];

        macro_rules! check_row {
            ($constructor:ident) => {
                let items = inputs.iter().map(|id| {
                    ScriptDisplayName::$constructor(prefs_en, *id)
                        .map(|name| Ok::<_, ()>(name.to_string()))
                });
                assert_eq!(
                    super::super::format_table_row(stringify!($constructor), items),
                    table_row!($constructor)
                );
            };
        }

        check_row!(try_new_tiny);
        check_row!(try_new_light);
        check_row!(try_new_heavy);
        check_row!(try_new_short_heavy);
    }
}

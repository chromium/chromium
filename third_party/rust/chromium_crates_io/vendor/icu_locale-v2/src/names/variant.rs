// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::DisplayNamesPreferences;
use super::{
    impl_writeable_for_single_display_name_borrowed, impl_writeable_for_single_display_name_owned,
    load_one,
};
use crate::provider::names::LocaleNamesVariantMediumHeavyV1;
use icu_locale_core::subtags::Variant;
use icu_provider::{DataPayloadOr, prelude::*};

#[inline]
fn make_attributes(subtag: &Variant) -> &DataMarkerAttributes {
    // All variant markers use the same attributes.
    // Valid Variant subtags conform to DataMarkerAttributes syntax.
    DataMarkerAttributes::from_str_or_panic(subtag.as_str())
}

#[inline]
fn make_locale(prefs: DisplayNamesPreferences) -> DataLocale {
    // All variant markers use the same locale
    LocaleNamesVariantMediumHeavyV1::make_locale(prefs.locale_preferences)
}

macro_rules! table_row {
    (try_new_heavy) => {
        "| [`try_new_heavy`](Self::try_new_heavy) | \"IPA Phonetics\" | \"Computer\" |"
    };
}

/// A localized display name for a single variant, owned version.
///
/// # Constructor Behavior
///
/// There is currently just one constructor, which is named "extended"
/// since there are no variants with guaranteed display names.
///
/// | Constructor | `fonipa` | `posix` |
/// | :--- | :--- | :--- |
#[doc = concat!(table_row!(try_new_heavy), "\n")]
///
/// There are fallible (`try_new_*`) and infallible (`new_*_with_fallback`) versions of
/// all constructors.
///
/// # Example
///
/// ```
/// use icu::locale::names::VariantDisplayName;
/// use icu::locale::{locale, subtags::variant};
/// use writeable::assert_writeable_eq;
///
/// let display_name = VariantDisplayName::try_new_heavy(
///     locale!("en").into(),
///     variant!("fonipa"),
/// )
/// .expect("Data should load successfully");
///
/// assert_writeable_eq!(display_name, "IPA Phonetics");
/// ```
#[derive(Debug)]
pub struct VariantDisplayName {
    pub(crate) payload: DataPayloadOr<LocaleNamesVariantMediumHeavyV1, Variant>,
}

impl VariantDisplayName {
    /// Loads a variant display name in a formatting locale using compiled data.
    ///
    /// The `heavy` constructor includes additional data coverage for subtags that are
    /// less commonly formatted in the target locale.
    /// See the [class docs](Self) for information on which constructor to use.
    ///
    /// If the display name is not found in data, the BCP-47 code is returned. To detect this case
    /// and return an error instead, use [`VariantDisplayName::try_new_heavy()`].
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::locale::names::VariantDisplayName;
    /// use icu::locale::{locale, subtags::variant};
    /// use writeable::assert_writeable_eq;
    ///
    /// assert_writeable_eq!(
    ///     VariantDisplayName::new_heavy_with_fallback(
    ///         locale!("de").into(),
    ///         variant!("fonipa")
    ///     ),
    ///     "IPA Phonetisch"
    /// );
    ///
    /// assert_writeable_eq!(
    ///     VariantDisplayName::new_heavy_with_fallback(
    ///         locale!("fr").into(),
    ///         variant!("fonipa")
    ///     ),
    ///     "alphabet phonétique international"
    /// );
    /// ```
    #[cfg(feature = "compiled_data")]
    pub fn new_heavy_with_fallback(prefs: DisplayNamesPreferences, variant: Variant) -> Self {
        Self::try_new_heavy(prefs, variant).unwrap_or(Self {
            payload: DataPayloadOr::from_other(variant),
        })
    }

    icu_provider::gen_buffer_data_constructors!(
        (prefs: DisplayNamesPreferences, variant: Variant) -> result: Result<Self, DataError>,
        /// Loads a variant display name in a formatting locale using compiled data.
        ///
        /// The `heavy` constructor includes additional data coverage for subtags that are
        /// less commonly formatted in the target locale.
        /// See the [class docs](Self) for information on which constructor to use.
        ///
        /// Returns an error if the display name is not found in data. To return the BCP-47 code
        /// instead, use [`VariantDisplayName::new_heavy_with_fallback()`].
        ///
        /// ✨ *Enabled with the `compiled_data` Cargo feature.*
        ///
        /// # Examples
        ///
        /// ```
        /// use icu::locale::names::VariantDisplayName;
        /// use icu::locale::{locale, subtags::variant};
        /// use writeable::assert_writeable_eq;
        ///
        /// let name = VariantDisplayName::try_new_heavy(locale!("de").into(), variant!("fonipa")).unwrap();
        /// assert_writeable_eq!(
        ///     name,
        ///     "IPA Phonetisch"
        /// );
        ///
        /// let name = VariantDisplayName::try_new_heavy(locale!("fr").into(), variant!("fonipa")).unwrap();
        /// assert_writeable_eq!(
        ///     name,
        ///     "alphabet phonétique international"
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
        variant: Variant,
    ) -> Result<Self, DataError>
    where
        D: ?Sized + DataProvider<LocaleNamesVariantMediumHeavyV1>,
    {
        let attrs = make_attributes(&variant);
        let locale = make_locale(prefs);
        let payload = load_one::<LocaleNamesVariantMediumHeavyV1, _, _>(provider, &locale, attrs)?
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
    pub fn as_borrowed(&self) -> VariantDisplayNameBorrowed<'_> {
        VariantDisplayNameBorrowed {
            value: self.borrow_str(),
        }
    }
}

impl_writeable_for_single_display_name_owned!(VariantDisplayName);

/// A localized display name for a single variant.
#[derive(Debug, Clone, Copy)]
pub struct VariantDisplayNameBorrowed<'a> {
    value: &'a str,
}

impl_writeable_for_single_display_name_borrowed!(VariantDisplayNameBorrowed);

#[cfg(test)]
mod tests {
    use super::*;
    use icu_locale_core::{locale, subtags::variant};

    #[test]
    fn test_variant_display_name_owned_table() {
        let prefs_en = DisplayNamesPreferences::from(locale!("en"));
        let inputs = [variant!("fonipa"), variant!("posix")];

        macro_rules! check_row {
            ($constructor:ident) => {
                let items = inputs.iter().map(|id| {
                    VariantDisplayName::$constructor(prefs_en, *id)
                        .map(|name| Ok::<_, ()>(name.to_string()))
                });
                assert_eq!(
                    super::super::format_table_row(stringify!($constructor), items),
                    table_row!($constructor)
                );
            };
        }

        check_row!(try_new_heavy);
    }
}

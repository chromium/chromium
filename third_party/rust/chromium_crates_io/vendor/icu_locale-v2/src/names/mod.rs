// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Types representing the name of a language, script, region, or variant.
//!
//! See `icu_experimental::displaynames` for another display names API designed for loading
//! multiple names at once, such as when populating a dropdown menu. Please send feedback on
//! your use cases.
//!
//! Display names for full locale identifiers are not currently supported.
//! If you have any feedback, please let us know at
//! <https://github.com/unicode-org/icu4x/issues/7825>.
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. Do not use this type unless you are prepared for things to occasionally break.
//!
//! Graduation tracking issue: [issue #3913](https://github.com/unicode-org/icu4x/issues/3913).
//! </div>

mod language;
mod region;
mod script;
mod variant;

// Re-export from submodules
pub use language::{
    LanguageIdentifierDisplayName, LanguageIdentifierDisplayNameBorrowed,
    LanguageIdentifierNameFallbackError,
};
pub use region::{RegionDisplayName, RegionDisplayNameBorrowed};
pub use script::{ScriptDisplayName, ScriptDisplayNameBorrowed};
pub use variant::{VariantDisplayName, VariantDisplayNameBorrowed};

use icu_locale_core::preferences::define_preferences;
use icu_provider::prelude::*;

define_preferences!(
    /// The preferences for display names.
    [Copy]
    DisplayNamesPreferences,
    {}
);

/// A bag of options defining how a language identifier display name will be formatted.
#[derive(Copy, Debug, Eq, PartialEq, Clone, Default)]
#[non_exhaustive]
pub struct LanguageIdentifierDisplayNameOptions {
    /// The language display kind, defaults to "dialect".
    pub language_display: Option<LanguageDisplay>,
}

impl LanguageIdentifierDisplayNameOptions {
    pub(crate) fn should_load_dialect(self) -> bool {
        self.language_display.unwrap_or_default() == LanguageDisplay::Dialect
    }
}

/// An enum for language display style.
#[non_exhaustive]
#[derive(Debug, Eq, PartialEq, Clone, Copy, Default)]
pub enum LanguageDisplay {
    /// Dialect display mode (default).
    #[default]
    Dialect,
    /// Standard display mode.
    Standard,
}

fn load_one<M0, M1, P>(
    provider: &P,
    locale: &DataLocale,
    attrs: &DataMarkerAttributes,
) -> Result<Option<DataPayload<M1>>, DataError>
where
    M0: DataMarker,
    M1: DynamicDataMarker<DataStruct = M0::DataStruct>,
    P: DataProvider<M0> + ?Sized,
{
    let id = DataIdentifierBorrowed::for_marker_attributes_and_locale(attrs, locale);
    let mut metadata = DataRequestMetadata::default();
    metadata.silent = true;
    let response = provider
        .load(DataRequest { id, metadata })
        .allow_identifier_not_found()?;
    Ok(response.map(|r| r.payload.cast()))
}

macro_rules! impl_writeable_for_single_display_name_borrowed {
    ($borrowed:ident) => {
        impl<'a> writeable::Writeable for $borrowed<'a> {
            #[inline]
            fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
                sink.write_str(self.value)
            }

            #[inline]
            fn writeable_length_hint(&self) -> writeable::LengthHint {
                writeable::LengthHint::exact(self.value.len())
            }

            #[inline]
            fn writeable_borrow(&self) -> Option<&str> {
                Some(self.value)
            }
        }

        writeable::impl_display_with_writeable!($borrowed<'_>);
    };
}

macro_rules! impl_writeable_for_single_display_name_owned {
    ($owned:ident) => {
        impl writeable::Writeable for $owned {
            #[inline]
            fn write_to<W: core::fmt::Write + ?Sized>(&self, sink: &mut W) -> core::fmt::Result {
                self.as_borrowed().write_to(sink)
            }

            #[inline]
            fn write_to_parts<S: writeable::PartsWrite + ?Sized>(
                &self,
                sink: &mut S,
            ) -> core::fmt::Result {
                self.as_borrowed().write_to_parts(sink)
            }

            #[inline]
            fn writeable_length_hint(&self) -> writeable::LengthHint {
                self.as_borrowed().writeable_length_hint()
            }

            #[inline]
            fn writeable_borrow(&self) -> Option<&str> {
                Some(self.borrow_str())
            }
        }

        writeable::impl_display_with_writeable!($owned);
    };
}

pub(crate) use impl_writeable_for_single_display_name_borrowed;
pub(crate) use impl_writeable_for_single_display_name_owned;

#[cfg(test)]
pub(crate) fn format_table_row<S: core::fmt::Display, E1, E2>(
    name: &str,
    items: impl IntoIterator<Item = Result<Result<S, E1>, E2>>,
) -> String {
    let row = items
        .into_iter()
        .map(|item| match item {
            Ok(Ok(s)) => format!("\"{s}\""),
            _ => "❌".to_string(),
        })
        .collect::<Vec<_>>()
        .join(" | ");
    format!("| [`{name}`](Self::{name}) | {row} |")
}

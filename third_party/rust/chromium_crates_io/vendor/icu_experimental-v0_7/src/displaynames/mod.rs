// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Display names for languages and regions.
//!
//! There are currently two designs for how to use this component:
//!
//! 1. [`multi`]: Load multiple display names at once.
//! 2. [`single`]: Load a single display name at a time.
//!
//! There are multiple use cases for this component, so we are not yet committed
//! to either of these designs being the "primary" design. Please share feedback at
//! <https://github.com/unicode-org/icu4x/issues/7824>.
//!
//! Note: Currently, the data between the two modules is NOT being shared.
//!
//! ## Examples
//!
//! The [`multi`] module lets you load multiple names at once, whereas `icu_locale::names`
//! loads one name at a time.
//!
//! ```
//! use icu::experimental::displaynames::DisplayNamesOptions;
//! use icu::experimental::displaynames::multi::RegionDisplayNames;
//! use icu::locale::names::RegionDisplayName;
//! use icu::locale::{locale, subtags::region};
//! use writeable::assert_writeable_eq;
//!
//! // Multi: Load a formatter that can display many regions.
//! let locale = locale!("en").into();
//! let multi =
//!     RegionDisplayNames::try_new(locale, DisplayNamesOptions::default())
//!         .unwrap();
//! assert_writeable_eq!(multi.of(region!("US")).unwrap(), "United States");
//! assert_writeable_eq!(multi.of(region!("GB")).unwrap(), "United Kingdom");
//!
//! // Single: Load only the region(s) we need.
//! let locale = locale!("en").into();
//! let us = RegionDisplayName::try_new_light(locale, region!("US")).unwrap();
//! let gb = RegionDisplayName::try_new_light(locale, region!("GB")).unwrap();
//! assert_writeable_eq!(us, "United States");
//! assert_writeable_eq!(gb, "United Kingdom");
//! ```

mod displaynames;
mod options;
pub mod provider;

pub mod multi {
    //! Types for loading multiple display names at once.
    //!
    //! This submodule is useful for applications that need to display multiple names
    //! of the same type, such as a list of regions or scripts.
    //!
    //! See [the parent module](mod@super) for a comparison of single and multi.
    use super::displaynames;
    pub use displaynames::LanguageDisplayNames;
    pub use displaynames::LocaleDisplayNamesFormatter;
    pub use displaynames::RegionDisplayNames;
    pub use displaynames::ScriptDisplayNames;
    pub use displaynames::VariantDisplayNames;
}

/// The single displaynames APIs have been moved to `icu_locale::names`.
pub mod single {}

pub use displaynames::DisplayNamesPreferences;
pub use options::DisplayNamesOptions;
pub use options::Fallback;
pub use options::LanguageDisplay;
pub use options::Style;

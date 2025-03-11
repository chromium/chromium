// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// https://github.com/unicode-org/icu4x/blob/main/documents/process/boilerplate.md#library-annotations
#![cfg_attr(not(any(test, feature = "std")), no_std)]
#![cfg_attr(
    not(test),
    deny(
        clippy::indexing_slicing,
        clippy::unwrap_used,
        clippy::expect_used,
        clippy::panic,
        // Exhaustiveness and Debug is not required for Diplomat types
    )
)]
// Diplomat limitations
#![allow(
    clippy::needless_lifetimes,
    clippy::result_unit_err,
    clippy::should_implement_trait
)]

//! This crate contains the source of truth for the [Diplomat](https://github.com/rust-diplomat/diplomat)-generated
//! FFI bindings. This generates the C, C++, JavaScript, and TypeScript bindings. This crate also contains the `extern "C"`
//! FFI for ICU4X.
//!
//! While the types in this crate are public, APIs from this crate are *not intended to be used from Rust*
//! and as such this crate may unpredictably change its Rust API across compatible semver versions. The `extern "C"` APIs exposed
//! by this crate, while not directly documented, are stable within the same major semver version, as are the bindings exposed under
//! the `cpp/` and `js/` folders.
//!
//! This crate may still be explored for documentation on docs.rs, and there are language-specific docs available as well.
//! C++, Dart, and TypeScript headers contain inline documentation, which is available pre-rendered: [C++], [TypeScript].
//!
//! This crate is `no_std`-compatible. If you wish to use it in `no_std` mode, you must write a wrapper crate that defines an allocator
//! and a panic hook in order to compile as a C library.
//!
//! More information on using ICU4X from C++ can be found in [our tutorial].
//!
//! [our tutorial]: https://github.com/unicode-org/icu4x/blob/main/tutorials/cpp.md
//! [TypeScript]: https://unicode-org.github.io/icu4x/tsdoc
//! [C++]: https://unicode-org.github.io/icu4x/cppdoc

// Renamed so you can't accidentally use it
#[cfg(target_arch = "wasm32")]
extern crate std as rust_std;

#[cfg(all(not(feature = "std"), feature = "looping_panic_handler"))]
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}

extern crate alloc;
#[cfg(all(not(feature = "std"), feature = "libc_alloc"))]
extern crate libc_alloc;

// Common modules
pub mod errors;
pub mod locale_core;
#[cfg(feature = "logging")]
pub mod logging;
#[macro_use]
pub mod provider;

// Components

#[cfg(feature = "properties")]
pub mod bidi;
#[cfg(any(feature = "datetime", feature = "timezone", feature = "calendar"))]
pub mod calendar;
#[cfg(feature = "casemap")]
pub mod casemap;
#[cfg(feature = "collator")]
pub mod collator;
#[cfg(feature = "properties")]
pub mod collections_sets;
#[cfg(any(feature = "datetime", feature = "timezone", feature = "calendar"))]
pub mod date;
#[cfg(any(feature = "datetime", feature = "timezone", feature = "calendar"))]
pub mod datetime;
#[cfg(feature = "datetime")]
pub mod datetime_formatter;
#[cfg(feature = "decimal")]
pub mod decimal;
#[cfg(feature = "experimental")]
pub mod displaynames;
#[cfg(feature = "locale")]
pub mod exemplar_chars;
#[cfg(feature = "locale")]
pub mod fallbacker;
#[cfg(feature = "decimal")]
pub mod fixed_decimal;
#[cfg(any(feature = "datetime", feature = "timezone"))]
pub mod iana_parser;
#[cfg(feature = "list")]
pub mod list;
#[cfg(feature = "locale")]
pub mod locale;
#[cfg(feature = "locale")]
pub mod locale_directionality;
#[cfg(feature = "datetime")]
pub mod neo_datetime;
#[cfg(feature = "normalizer")]
pub mod normalizer;
#[cfg(feature = "normalizer")]
pub mod normalizer_properties;
#[cfg(feature = "plurals")]
pub mod pluralrules;
#[cfg(feature = "properties")]
pub mod properties_bidi;
#[cfg(feature = "properties")]
pub mod properties_enums;
#[cfg(feature = "properties")]
pub mod properties_iter;
#[cfg(feature = "properties")]
pub mod properties_maps;
#[cfg(feature = "properties")]
pub mod properties_names;
#[cfg(feature = "properties")]
pub mod properties_sets;
#[cfg(feature = "properties")]
pub mod properties_unisets;
#[cfg(feature = "properties")]
pub mod script;
#[cfg(feature = "segmenter")]
pub mod segmenter_grapheme;
#[cfg(feature = "segmenter")]
pub mod segmenter_line;
#[cfg(feature = "segmenter")]
pub mod segmenter_sentence;
#[cfg(feature = "segmenter")]
pub mod segmenter_word;
#[cfg(any(feature = "datetime", feature = "timezone", feature = "calendar"))]
pub mod time;
#[cfg(any(feature = "datetime", feature = "timezone"))]
pub mod timezone;
#[cfg(feature = "experimental")]
pub mod units_converter;
#[cfg(any(feature = "datetime", feature = "timezone"))]
pub mod utc_offset;
#[cfg(feature = "calendar")]
pub mod week;
#[cfg(any(feature = "datetime", feature = "timezone"))]
pub mod windows_parser;
#[cfg(feature = "datetime")]
pub mod zoned_datetime;
#[cfg(feature = "datetime")]
pub mod zoned_formatter;

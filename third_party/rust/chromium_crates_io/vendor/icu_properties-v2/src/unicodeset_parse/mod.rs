// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! <div class="stab unstable">
//! 🚧 This code is unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. It can be enabled with the `unstable` Cargo feature
//! of the icu meta-crate. Use with caution.
//! <a href="https://github.com/unicode-org/icu4x/issues/3959">#3959</a>
//! </div>
//!
//! This module provides parsing functionality for [UTS #35 - Unicode Sets](https://unicode.org/reports/tr35/#Unicode_Sets).
//!
//! Parses into [`CodePointInversionListAndStringList`](icu_collections::codepointinvliststringlist::CodePointInversionListAndStringList).
//!
//! See [`parse`](parse()) for more information.
//!
//! [`ICU4X`]: ../icu/index.html

mod parse;

pub use parse::*;

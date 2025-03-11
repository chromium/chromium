// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! UTS 35 classical datetime skeletons.
//!
//! Semantic skeletons (field sets) use classical skeletons for pattern matching in datagen.
//!
//! See [`Skeleton`](reference::Skeleton) and [`Bag`](components::Bag) for more information.
//!
//! ✨ *Enabled with the `datagen` Cargo feature.*
//!
//! # Implementation status
//!
//! This is currently only a partial implementation of the UTS-35 skeleton matching algorithm.
//!
//! | Algorithm step | Status |
//! |----------------|--------|
//! | Match skeleton fields according to a ranking             | Implemented |
//! | Adjust the matched pattern to have certain widths        | Implemented |
//! | Match date and times separately, and them combine them   | Implemented |
//! | Use appendItems to fill in a pattern with missing fields | Not yet, and may not be fully implemented. See [issue #586](https://github.com/unicode-org/icu4x/issues/586) |
//!
//! # Description
//!
//! A [`components::Bag`] is a model of encoding information on how to format date
//! and time by specifying a list of components the user wants to be visible in the formatted string
//! and how each field should be displayed.
//!
//! This model closely corresponds to `ECMA402` API and allows for high level of customization
//! compared to `Length` model.
//!
//! Additionally, the bag contains an optional set of `Preferences` which represent user
//! preferred adjustments that can be applied onto the pattern right before formatting.
//!
//! ## Pattern Selection
//!
//! The [`components::Bag`] is a way for the developer to describe which components
//! should be included in in a datetime, and how they should be displayed. There is not a strict
//! guarantee in how the final date will be displayed to the end user. The user's preferences and
//! locale information can override the developer preferences.
//!
//! The fields in the [`components::Bag`] are matched against available patterns in
//! the `CLDR` locale data. A best fit is found, and presented to the user. This means that in
//! certain situations, and component combinations, fields will not have a match, or the match will
//! have a different type of presentation for a given locale.
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. While the serde representation of data structs is guaranteed
//! to be stable, their Rust representation might not be. Use with caution.
//! </div>

#[cfg(doc)]
use crate::provider::fields::components;

mod error;
mod helpers;
mod plural;
pub mod reference;
pub mod runtime;
#[cfg(feature = "serde")]
mod serde;
pub use error::*;
pub use helpers::*;
pub use plural::*;

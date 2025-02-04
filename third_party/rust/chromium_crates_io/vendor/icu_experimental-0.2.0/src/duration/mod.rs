// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Duration formatting

#![warn(missing_docs)]

mod duration;
mod format;
mod formatter;
pub mod provider;

pub mod options;
mod validated_options;

pub use duration::{Duration, DurationSign};
pub use format::FormattedDuration;
pub use formatter::DurationFormatter;

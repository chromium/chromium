// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::preferences::extensions::unicode::enum_keyword;

enum_keyword!(
    /// A Unicode Hour Cycle Identifier defines the preferred time cycle. Specifying "hc" in a locale identifier overrides the default value specified by supplemental time data for the region.
    ///
    /// The valid values are listed in [LDML](https://unicode.org/reports/tr35/#UnicodeHourCycleIdentifier).
    HourCycle {
        /// Hour system using 1–12; corresponds to 'h' in patterns
        ("h11" => H11),
        /// Hour system using 0–23; corresponds to 'H' in patterns
        ("h12" => H12),
        /// Hour system using 0–11; corresponds to 'K' in patterns
        ("h23" => H23),
        /// Hour system using 1–24; corresponds to 'k' in pattern
        ("h24" => H24),
}, "hc");

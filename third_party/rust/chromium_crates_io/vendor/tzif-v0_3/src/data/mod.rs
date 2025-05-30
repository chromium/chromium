// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

/// Structs for holding data encoded by POSIX time-zone strings, as specified by
/// <https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html>
pub mod posix;

/// Simple structs for keeping track of seconds, hours, and minutes with the type system.
pub mod time;

/// Structs for holding data parsed from `TZif` binary files, as specified by
/// <https://datatracker.ietf.org/doc/html/rfc8536>
pub mod tzif;

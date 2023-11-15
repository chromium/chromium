//! Implementation of the Line Breaking Algorithm described in [Unicode Standard Annex #14][UAX14].
//!
//! Given an input text, locates "line break opportunities", or positions appropriate for wrapping
//! lines when displaying text.
//!
//! # Example
//!
//! ```
//! use unicode_linebreak::{linebreaks, BreakOpportunity::{Mandatory, Allowed}};
//!
//! let text = "a b \nc";
//! assert!(linebreaks(text).eq(vec![
//!     (2, Allowed),   // May break after first space
//!     (5, Mandatory), // Must break after line feed
//!     (6, Mandatory)  // Must break at end of text, so that there always is at least one LB
//! ]));
//! ```
//!
//! [UAX14]: https://www.unicode.org/reports/tr14/

#![no_std]
#![deny(missing_docs, missing_debug_implementations)]

use core::iter::once;
use core::mem;

/// The [Unicode version](https://www.unicode.org/versions/) conformed to.
pub const UNICODE_VERSION: (u8, u8, u8) = (13, 0, 0);

include!("shared.rs");

include!(concat!(env!("OUT_DIR"), "/tables.rs"));

/// Returns the line break property of the specified code point.
///
/// # Examples
///
/// ```
/// use unicode_linebreak::{BreakClass, break_property};
/// assert_eq!(break_property(0x2CF3), BreakClass::Alphabetic);
/// ```
#[inline]
pub fn break_property(codepoint: u32) -> BreakClass {
    let codepoint = codepoint as usize;
    match PAGE_INDICES.get(codepoint >> 8) {
        Some(&page_idx) if page_idx & UNIFORM_PAGE != 0 => unsafe {
            mem::transmute((page_idx & !UNIFORM_PAGE) as u8)
        },
        Some(&page_idx) => BREAK_PROP_DATA[page_idx][codepoint & 0xFF],
        None => BreakClass::Unknown,
    }
}

/// Break opportunity type.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum BreakOpportunity {
    /// A line must break at this spot.
    Mandatory,
    /// A line is allowed to end at this spot.
    Allowed,
}

/// Returns an iterator over line break opportunities in the specified string.
///
/// Break opportunities are given as tuples of the byte index of the character succeeding the break
/// and the type.
///
/// Uses the default Line Breaking Algorithm with the tailoring that Complex-Context Dependent
/// (SA) characters get resolved to Ordinary Alphabetic and Symbol Characters (AL) regardless of
/// General_Category.
///
/// # Examples
///
/// ```
/// use unicode_linebreak::{linebreaks, BreakOpportunity::{Mandatory, Allowed}};
/// assert!(linebreaks("Hello world!").eq(vec![(6, Allowed), (12, Mandatory)]));
/// ```
pub fn linebreaks<'a>(s: &'a str) -> impl Iterator<Item = (usize, BreakOpportunity)> + Clone + 'a {
    use BreakOpportunity::{Allowed, Mandatory};

    s.char_indices()
        .map(|(i, c)| (i, break_property(c as u32) as u8))
        .chain(once((s.len(), eot)))
        .scan((sot, false), |state, (i, cls)| {
            // ZWJ is handled outside the table to reduce its size
            let val = PAIR_TABLE[state.0 as usize][cls as usize];
            let is_mandatory = val & MANDATORY_BREAK_BIT != 0;
            let is_break = val & ALLOWED_BREAK_BIT != 0 && (!state.1 || is_mandatory);
            *state = (
                val & !(ALLOWED_BREAK_BIT | MANDATORY_BREAK_BIT),
                cls == BreakClass::ZeroWidthJoiner as u8,
            );

            Some((i, is_break, is_mandatory))
        })
        .filter_map(|(i, is_break, is_mandatory)| {
            if is_break {
                Some((i, if is_mandatory { Mandatory } else { Allowed }))
            } else {
                None
            }
        })
}

/// Divides the string at the last index where further breaks do not depend on prior context.
///
/// The trivial index at `eot` is excluded.
///
/// A common optimization is to determine only the nearest line break opportunity before the first
/// character that would cause the line to become overfull, requiring backward traversal, of which
/// there are two approaches:
///
/// * Cache breaks from forward traversals
/// * Step backward and with `split_at_safe` find a pos to safely search forward from, repeatedly
///
/// # Examples
///
/// ```
/// use unicode_linebreak::{linebreaks, split_at_safe};
/// let s = "Not allowed to break within em dashes: — —";
/// let (prev, safe) = split_at_safe(s);
/// let n = prev.len();
/// assert!(linebreaks(safe).eq(linebreaks(s).filter_map(|(i, x)| i.checked_sub(n).map(|i| (i, x)))));
/// ```
pub fn split_at_safe(s: &str) -> (&str, &str) {
    let mut chars = s.char_indices().rev().scan(None, |state, (i, c)| {
        let cls = break_property(c as u32);
        let is_safe_pair = state
            .replace(cls)
            .map_or(false, |prev| is_safe_pair(cls, prev)); // Reversed since iterating backwards
        Some((i, is_safe_pair))
    });
    chars.find(|&(_, is_safe_pair)| is_safe_pair);
    // Include preceding char for `linebreaks` to pick up break before match (disallowed after sot)
    s.split_at(chars.next().map_or(0, |(i, _)| i))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn it_works() {
        assert_eq!(break_property(0xA), BreakClass::LineFeed);
        assert_eq!(break_property(0xDB80), BreakClass::Surrogate);
    }
}

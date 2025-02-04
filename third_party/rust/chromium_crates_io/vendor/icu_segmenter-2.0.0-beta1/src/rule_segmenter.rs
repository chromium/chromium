// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::complex::ComplexPayloads;
use crate::indices::{Latin1Indices, Utf16Indices};
use crate::provider::*;
use crate::WordType;
use core::str::CharIndices;
use utf8_iter::Utf8CharIndices;

/// A trait allowing for RuleBreakIterator to be generalized to multiple string
/// encoding methods and granularity such as grapheme cluster, word, etc.
pub trait RuleBreakType<'l, 's> {
    /// The iterator over characters.
    type IterAttr: Iterator<Item = (usize, Self::CharType)> + Clone + core::fmt::Debug;

    /// The character type.
    type CharType: Copy + Into<u32> + core::fmt::Debug;

    fn get_current_position_character_len(iter: &RuleBreakIterator<'l, 's, Self>) -> usize;

    fn handle_complex_language(
        iter: &mut RuleBreakIterator<'l, 's, Self>,
        left_codepoint: Self::CharType,
    ) -> Option<usize>;
}

/// Implements the [`Iterator`] trait over the segmenter boundaries of the given string.
///
/// Lifetimes:
///
/// - `'l` = lifetime of the segmenter object from which this iterator was created
/// - `'s` = lifetime of the string being segmented
///
/// The [`Iterator::Item`] is an [`usize`] representing index of a code unit
/// _after_ the boundary (for a boundary at the end of text, this index is the length
/// of the [`str`] or array of code units).
#[derive(Debug)]
pub struct RuleBreakIterator<'l, 's, Y: RuleBreakType<'l, 's> + ?Sized> {
    pub(crate) iter: Y::IterAttr,
    pub(crate) len: usize,
    pub(crate) current_pos_data: Option<(usize, Y::CharType)>,
    pub(crate) result_cache: alloc::vec::Vec<usize>,
    pub(crate) data: &'l RuleBreakDataV2<'l>,
    pub(crate) complex: Option<&'l ComplexPayloads>,
    pub(crate) boundary_property: u8,
    pub(crate) locale_override: Option<&'l RuleBreakDataOverrideV1<'l>>,
}

impl<'l, 's, Y: RuleBreakType<'l, 's> + ?Sized> Iterator for RuleBreakIterator<'l, 's, Y> {
    type Item = usize;

    fn next(&mut self) -> Option<Self::Item> {
        // If we have break point cache by previous run, return this result
        if let Some(&first_result) = self.result_cache.first() {
            let mut i = 0;
            loop {
                if i == first_result {
                    self.result_cache = self.result_cache.iter().skip(1).map(|r| r - i).collect();
                    return self.get_current_position();
                }
                i += Y::get_current_position_character_len(self);
                self.advance_iter();
                if self.is_eof() {
                    self.result_cache.clear();
                    self.boundary_property = self.data.complex_property;
                    return Some(self.len);
                }
            }
        }

        if self.is_eof() {
            self.advance_iter();
            if self.is_eof() && self.len == 0 {
                // Empty string. Since `self.current_pos_data` is always going to be empty,
                // we never read `self.len` except for here, so we can use it to mark that
                // we have already returned the single empty-string breakpoint.
                self.len = 1;
                return Some(0);
            }
            let Some(right_prop) = self.get_current_break_property() else {
                // iterator already reaches to EOT. Reset boundary property for word-like.
                self.boundary_property = 0;
                return None;
            };
            // SOT x anything
            if matches!(
                self.get_break_state_from_table(self.data.sot_property, right_prop),
                BreakState::Break | BreakState::NoMatch
            ) {
                self.boundary_property = 0; // SOT is special type
                return self.get_current_position();
            }
        }

        'a: loop {
            debug_assert!(!self.is_eof());
            let left_codepoint = self.get_current_codepoint()?;
            let left_prop = self.get_break_property(left_codepoint);
            self.advance_iter();

            let Some(right_prop) = self.get_current_break_property() else {
                self.boundary_property = left_prop;
                return Some(self.len);
            };

            // Some segmenter rules doesn't have language-specific rules, we have to use LSTM (or dictionary) segmenter.
            // If property is marked as SA, use it
            if right_prop == self.data.complex_property {
                if left_prop != self.data.complex_property {
                    // break before SA
                    self.boundary_property = left_prop;
                    return self.get_current_position();
                }
                let break_offset = Y::handle_complex_language(self, left_codepoint);
                if break_offset.is_some() {
                    return break_offset;
                }
            }

            match self.get_break_state_from_table(left_prop, right_prop) {
                BreakState::Keep => continue,
                BreakState::Break | BreakState::NoMatch => {
                    self.boundary_property = left_prop;
                    return self.get_current_position();
                }
                BreakState::Index(mut index) | BreakState::Intermediate(mut index) => {
                    // This isn't simple rule set. We need marker to restore iterator to previous position.
                    let mut previous_iter = self.iter.clone();
                    let mut previous_pos_data = self.current_pos_data;
                    let mut previous_left_prop = left_prop;

                    loop {
                        self.advance_iter();

                        let Some(prop) = self.get_current_break_property() else {
                            // Reached EOF. But we are analyzing multiple characters now, so next break may be previous point.
                            self.boundary_property = index;
                            if self.get_break_state_from_table(index, self.data.eot_property)
                                == BreakState::NoMatch
                            {
                                self.boundary_property = previous_left_prop;
                                self.iter = previous_iter;
                                self.current_pos_data = previous_pos_data;
                                return self.get_current_position();
                            }
                            // EOF
                            return Some(self.len);
                        };

                        let previous_break_state_is_cp_prop =
                            index <= self.data.last_codepoint_property;

                        match self.get_break_state_from_table(index, prop) {
                            BreakState::Keep => continue 'a,
                            BreakState::NoMatch => {
                                self.boundary_property = previous_left_prop;
                                self.iter = previous_iter;
                                self.current_pos_data = previous_pos_data;
                                return self.get_current_position();
                            }
                            BreakState::Break => return self.get_current_position(),
                            BreakState::Intermediate(i) => {
                                index = i;
                                if previous_break_state_is_cp_prop {
                                    // Move marker
                                    previous_left_prop = index;
                                }
                                previous_iter = self.iter.clone();
                                previous_pos_data = self.current_pos_data;
                            }
                            BreakState::Index(i) => {
                                index = i;
                                if previous_break_state_is_cp_prop {
                                    // Move marker
                                    previous_iter = self.iter.clone();
                                    previous_pos_data = self.current_pos_data;
                                    previous_left_prop = index;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

impl<'l, 's, Y: RuleBreakType<'l, 's> + ?Sized> RuleBreakIterator<'l, 's, Y> {
    pub(crate) fn advance_iter(&mut self) {
        self.current_pos_data = self.iter.next();
    }

    pub(crate) fn is_eof(&self) -> bool {
        self.current_pos_data.is_none()
    }

    pub(crate) fn get_current_break_property(&self) -> Option<u8> {
        self.get_current_codepoint()
            .map(|c| self.get_break_property(c))
    }

    pub(crate) fn get_current_position(&self) -> Option<usize> {
        self.current_pos_data.map(|(pos, _)| pos)
    }

    pub(crate) fn get_current_codepoint(&self) -> Option<Y::CharType> {
        self.current_pos_data.map(|(_, codepoint)| codepoint)
    }

    fn get_break_property(&self, codepoint: Y::CharType) -> u8 {
        // Note: Default value is 0 == UNKNOWN
        if let Some(locale_override) = &self.locale_override {
            let property = locale_override
                .property_table_override
                .get32(codepoint.into());
            if property != 0 {
                return property;
            }
        }
        self.data.property_table.get32(codepoint.into())
    }

    fn get_break_state_from_table(&self, left: u8, right: u8) -> BreakState {
        let idx = left as usize * self.data.property_count as usize + right as usize;
        // We use unwrap_or to fall back to the base case and prevent panics on bad data.
        self.data
            .break_state_table
            .get(idx)
            .unwrap_or(BreakState::Keep)
    }

    /// Return the status value of break boundary.
    /// If segmenter isn't word, always return WordType::None
    pub fn word_type(&self) -> WordType {
        if !self.result_cache.is_empty() {
            // Dictionary type (CJ and East Asian) is letter.
            return WordType::Letter;
        }
        if self.boundary_property == 0 {
            // break position is SOT / Any
            return WordType::None;
        }
        self.data
            .word_type_table
            .get((self.boundary_property - 1) as usize)
            .unwrap_or(WordType::None)
    }

    /// Return true when break boundary is word-like such as letter/number/CJK
    /// If segmenter isn't word, return false
    pub fn is_word_like(&self) -> bool {
        self.word_type().is_word_like()
    }
}

#[derive(Debug)]
pub struct RuleBreakTypeUtf8;

impl<'s> RuleBreakType<'_, 's> for RuleBreakTypeUtf8 {
    type IterAttr = CharIndices<'s>;
    type CharType = char;

    fn get_current_position_character_len(iter: &RuleBreakIterator<Self>) -> usize {
        iter.get_current_codepoint().map_or(0, |c| c.len_utf8())
    }

    fn handle_complex_language(
        _: &mut RuleBreakIterator<Self>,
        _: Self::CharType,
    ) -> Option<usize> {
        unreachable!()
    }
}

#[derive(Debug)]
pub struct RuleBreakTypePotentiallyIllFormedUtf8;

impl<'s> RuleBreakType<'_, 's> for RuleBreakTypePotentiallyIllFormedUtf8 {
    type IterAttr = Utf8CharIndices<'s>;
    type CharType = char;

    fn get_current_position_character_len(iter: &RuleBreakIterator<Self>) -> usize {
        iter.get_current_codepoint().map_or(0, |c| c.len_utf8())
    }

    fn handle_complex_language(
        _: &mut RuleBreakIterator<Self>,
        _: Self::CharType,
    ) -> Option<usize> {
        unreachable!()
    }
}

#[derive(Debug)]
pub struct RuleBreakTypeLatin1;

impl<'s> RuleBreakType<'_, 's> for RuleBreakTypeLatin1 {
    type IterAttr = Latin1Indices<'s>;
    type CharType = u8;

    fn get_current_position_character_len(_: &RuleBreakIterator<Self>) -> usize {
        unreachable!()
    }

    fn handle_complex_language(
        _: &mut RuleBreakIterator<Self>,
        _: Self::CharType,
    ) -> Option<usize> {
        unreachable!()
    }
}

#[derive(Debug)]
pub struct RuleBreakTypeUtf16;

impl<'s> RuleBreakType<'_, 's> for RuleBreakTypeUtf16 {
    type IterAttr = Utf16Indices<'s>;
    type CharType = u32;

    fn get_current_position_character_len(iter: &RuleBreakIterator<Self>) -> usize {
        match iter.get_current_codepoint() {
            None => 0,
            Some(ch) if ch >= 0x10000 => 2,
            _ => 1,
        }
    }

    fn handle_complex_language(
        _: &mut RuleBreakIterator<Self>,
        _: Self::CharType,
    ) -> Option<usize> {
        unreachable!()
    }
}

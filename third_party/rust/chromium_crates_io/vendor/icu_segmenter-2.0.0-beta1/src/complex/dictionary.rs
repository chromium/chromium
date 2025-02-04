// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::grapheme::*;
use crate::indices::Utf16Indices;
use crate::provider::*;
use core::str::CharIndices;
use icu_collections::char16trie::{Char16Trie, TrieResult};

/// A trait for dictionary based iterator
trait DictionaryType<'l, 's> {
    /// The iterator over characters.
    type IterAttr: Iterator<Item = (usize, Self::CharType)> + Clone;

    /// The character type.
    type CharType: Copy + Into<u32>;

    fn to_char(c: Self::CharType) -> char;
    fn char_len(c: Self::CharType) -> usize;
}

struct DictionaryBreakIterator<
    'l,
    's,
    Y: DictionaryType<'l, 's> + ?Sized,
    X: Iterator<Item = usize> + ?Sized,
> {
    trie: Char16Trie<'l>,
    iter: Y::IterAttr,
    len: usize,
    grapheme_iter: X,
    // TODO transform value for byte trie
}

/// Implement the [`Iterator`] trait over the segmenter break opportunities of the given string.
/// Please see the [module-level documentation](crate) for its usages.
///
/// Lifetimes:
/// - `'l` = lifetime of the segmenter object from which this iterator was created
/// - `'s` = lifetime of the string being segmented
///
/// [`Iterator`]: core::iter::Iterator
impl<'l, 's, Y: DictionaryType<'l, 's> + ?Sized, X: Iterator<Item = usize> + ?Sized> Iterator
    for DictionaryBreakIterator<'l, 's, Y, X>
{
    type Item = usize;

    fn next(&mut self) -> Option<Self::Item> {
        let mut trie_iter = self.trie.iter();
        let mut intermediate_length = 0;
        let mut not_match = false;
        let mut previous_match = None;
        let mut last_grapheme_offset = 0;

        while let Some(next) = self.iter.next() {
            let ch = Y::to_char(next.1);
            match trie_iter.next(ch) {
                TrieResult::FinalValue(_) => {
                    return Some(next.0 + Y::char_len(next.1));
                }
                TrieResult::Intermediate(_) => {
                    // Dictionary has to match with grapheme cluster segment.
                    // If not, we ignore it.
                    while last_grapheme_offset < next.0 + Y::char_len(next.1) {
                        if let Some(offset) = self.grapheme_iter.next() {
                            last_grapheme_offset = offset;
                            continue;
                        }
                        last_grapheme_offset = self.len;
                        break;
                    }
                    if last_grapheme_offset != next.0 + Y::char_len(next.1) {
                        continue;
                    }

                    intermediate_length = next.0 + Y::char_len(next.1);
                    previous_match = Some(self.iter.clone());
                }
                TrieResult::NoMatch => {
                    if intermediate_length > 0 {
                        if let Some(previous_match) = previous_match {
                            // Rewind previous match point
                            self.iter = previous_match;
                        }
                        return Some(intermediate_length);
                    }
                    // Not found
                    return Some(next.0 + Y::char_len(next.1));
                }
                TrieResult::NoValue => {
                    // Prefix string is matched
                    not_match = true;
                }
            }
        }

        if intermediate_length > 0 {
            Some(intermediate_length)
        } else if not_match {
            // no match by scanning text
            Some(self.len)
        } else {
            None
        }
    }
}

impl<'s> DictionaryType<'_, 's> for u32 {
    type IterAttr = Utf16Indices<'s>;
    type CharType = u32;

    fn to_char(c: u32) -> char {
        char::from_u32(c).unwrap_or(char::REPLACEMENT_CHARACTER)
    }

    fn char_len(c: u32) -> usize {
        if c >= 0x10000 {
            2
        } else {
            1
        }
    }
}

impl<'s> DictionaryType<'_, 's> for char {
    type IterAttr = CharIndices<'s>;
    type CharType = char;

    fn to_char(c: char) -> char {
        c
    }

    fn char_len(c: char) -> usize {
        c.len_utf8()
    }
}

pub(super) struct DictionarySegmenter<'l> {
    dict: &'l UCharDictionaryBreakDataV1<'l>,
    grapheme: &'l RuleBreakDataV2<'l>,
}

impl<'l> DictionarySegmenter<'l> {
    pub(super) fn new(
        dict: &'l UCharDictionaryBreakDataV1<'l>,
        grapheme: &'l RuleBreakDataV2<'l>,
    ) -> Self {
        // TODO: no way to verify trie data
        Self { dict, grapheme }
    }

    /// Create a dictionary based break iterator for an `str` (a UTF-8 string).
    pub(super) fn segment_str(&'l self, input: &'l str) -> impl Iterator<Item = usize> + 'l {
        let grapheme_iter = GraphemeClusterSegmenter::new_and_segment_str(input, self.grapheme);
        DictionaryBreakIterator::<char, GraphemeClusterBreakIteratorUtf8> {
            trie: Char16Trie::new(self.dict.trie_data.clone()),
            iter: input.char_indices(),
            len: input.len(),
            grapheme_iter,
        }
    }

    /// Create a dictionary based break iterator for a UTF-16 string.
    pub(super) fn segment_utf16(&'l self, input: &'l [u16]) -> impl Iterator<Item = usize> + 'l {
        let grapheme_iter = GraphemeClusterSegmenter::new_and_segment_utf16(input, self.grapheme);
        DictionaryBreakIterator::<u32, GraphemeClusterBreakIteratorUtf16> {
            trie: Char16Trie::new(self.dict.trie_data.clone()),
            iter: Utf16Indices::new(input),
            len: input.len(),
            grapheme_iter,
        }
    }
}

#[cfg(test)]
#[cfg(feature = "serde")]
mod tests {
    use super::*;
    use crate::{LineSegmenter, WordSegmenter};
    use icu_provider::prelude::*;

    #[test]
    fn burmese_dictionary_test() {
        let segmenter = LineSegmenter::new_dictionary();
        // From css/css-text/word-break/word-break-normal-my-000.html
        let s = "မြန်မာစာမြန်မာစာမြန်မာစာ";
        let result: Vec<usize> = segmenter.segment_str(s).collect();
        assert_eq!(result, vec![0, 18, 24, 42, 48, 66, 72]);

        let s_utf16: Vec<u16> = s.encode_utf16().collect();
        let result: Vec<usize> = segmenter.segment_utf16(&s_utf16).collect();
        assert_eq!(result, vec![0, 6, 8, 14, 16, 22, 24]);
    }

    #[test]
    fn cj_dictionary_test() {
        let response: DataResponse<DictionaryForWordOnlyAutoV1Marker> = crate::provider::Baked
            .load(DataRequest {
                id: DataIdentifierBorrowed::for_marker_attributes(
                    DataMarkerAttributes::from_str_or_panic("cjdict"),
                ),
                ..Default::default()
            })
            .unwrap();
        let word_segmenter = WordSegmenter::new_dictionary();
        let dict_segmenter = DictionarySegmenter::new(
            response.payload.get(),
            crate::provider::Baked::SINGLETON_GRAPHEME_CLUSTER_BREAK_DATA_V2_MARKER,
        );

        // Match case
        let s = "龟山岛龟山岛";
        let result: Vec<usize> = dict_segmenter.segment_str(s).collect();
        assert_eq!(result, vec![9, 18]);

        let result: Vec<usize> = word_segmenter.segment_str(s).collect();
        assert_eq!(result, vec![0, 9, 18]);

        let s_utf16: Vec<u16> = s.encode_utf16().collect();
        let result: Vec<usize> = dict_segmenter.segment_utf16(&s_utf16).collect();
        assert_eq!(result, vec![3, 6]);

        let result: Vec<usize> = word_segmenter.segment_utf16(&s_utf16).collect();
        assert_eq!(result, vec![0, 3, 6]);

        // Match case, then no match case
        let s = "エディターエディ";
        let result: Vec<usize> = dict_segmenter.segment_str(s).collect();
        assert_eq!(result, vec![15, 24]);

        // TODO(#3236): Why is WordSegmenter not returning the middle segment?
        let result: Vec<usize> = word_segmenter.segment_str(s).collect();
        assert_eq!(result, vec![0, 24]);

        let s_utf16: Vec<u16> = s.encode_utf16().collect();
        let result: Vec<usize> = dict_segmenter.segment_utf16(&s_utf16).collect();
        assert_eq!(result, vec![5, 8]);

        // TODO(#3236): Why is WordSegmenter not returning the middle segment?
        let result: Vec<usize> = word_segmenter.segment_utf16(&s_utf16).collect();
        assert_eq!(result, vec![0, 8]);
    }

    #[test]
    fn khmer_dictionary_test() {
        let segmenter = LineSegmenter::new_dictionary();
        let s = "ភាសាខ្មែរភាសាខ្មែរភាសាខ្មែរ";
        let result: Vec<usize> = segmenter.segment_str(s).collect();
        assert_eq!(result, vec![0, 27, 54, 81]);

        let s_utf16: Vec<u16> = s.encode_utf16().collect();
        let result: Vec<usize> = segmenter.segment_utf16(&s_utf16).collect();
        assert_eq!(result, vec![0, 9, 18, 27]);
    }

    #[test]
    fn lao_dictionary_test() {
        let segmenter = LineSegmenter::new_dictionary();
        let s = "ພາສາລາວພາສາລາວພາສາລາວ";
        let r: Vec<usize> = segmenter.segment_str(s).collect();
        assert_eq!(r, vec![0, 12, 21, 33, 42, 54, 63]);

        let s_utf16: Vec<u16> = s.encode_utf16().collect();
        let r: Vec<usize> = segmenter.segment_utf16(&s_utf16).collect();
        assert_eq!(r, vec![0, 4, 7, 11, 14, 18, 21]);
    }
}

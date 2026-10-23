// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[cfg(feature = "alloc")]
use alloc::string::String;
use yoke::Yokeable;
use zerofrom::ZeroFrom;
use zerovec::{ZeroSlice, ZeroVec};

// Match-node lead unit values, after masking off intermediate-value bits:

// 00..0f: Branch node. If node!=0 then the length is node+1, otherwise
// the length is one more than the next byte.

// For a branch sub-node with at most this many entries, we drop down
// to a linear search.
const MAX_BRANCH_LINEAR_SUB_NODE_LENGTH: usize = 5;

// 0030..003f: Linear-match node, match 1..16 units and continue reading the next node.
const MIN_LINEAR_MATCH: u16 = 0x30;
const MAX_LINEAR_MATCH_LENGTH: u16 = 0x10;

// Match-node lead unit bits 14..6 for the optional intermediate value.
// If these bits are 0, then there is no intermediate value.
// Otherwise, see the *NodeValue* constants below.
const MIN_VALUE_LEAD: u16 = MIN_LINEAR_MATCH + MAX_LINEAR_MATCH_LENGTH; // 0x40
const NODE_TYPE_MASK: u16 = MIN_VALUE_LEAD - 1; // 0x003f

// A final-value node has bit 15 set.
const VALUE_IS_FINAL: u16 = 0x8000;

// Compact value: After testing bit 0, shift right by 15 and then use the following thresholds.
const MAX_ONE_UNIT_VALUE: u16 = 0x3fff;

const MIN_TWO_UNIT_VALUE_LEAD: u16 = MAX_ONE_UNIT_VALUE + 1; // 0x4000

const MAX_ONE_UNIT_NODE_VALUE: u16 = 0xff;

const MIN_TWO_UNIT_NODE_VALUE_LEAD: u16 = MIN_VALUE_LEAD + ((MAX_ONE_UNIT_NODE_VALUE + 1) << 6); // 0x4040

const THREE_UNIT_NODE_VALUE_LEAD: u16 = 0x7fc0;

const THREE_UNIT_VALUE_LEAD: u16 = 0x7fff;

// Compact delta integers.
const MAX_ONE_UNIT_DELTA: u16 = 0xfbff;
const MIN_TWO_UNIT_DELTA_LEAD: u16 = MAX_ONE_UNIT_DELTA + 1; // 0xfc00
const THREE_UNIT_DELTA_LEAD: u16 = 0xffff;

fn skip_value(pos: usize, lead: u16) -> usize {
    if lead < MIN_TWO_UNIT_VALUE_LEAD {
        pos
    } else if lead < THREE_UNIT_VALUE_LEAD {
        pos + 1
    } else {
        pos + 2
    }
}

fn skip_node_value(pos: usize, lead: u16) -> usize {
    if lead < MIN_TWO_UNIT_NODE_VALUE_LEAD {
        pos
    } else if lead < THREE_UNIT_NODE_VALUE_LEAD {
        pos + 1
    } else {
        pos + 2
    }
}

/// This struct represents a de-serialized `Char16Trie` that was exported from
/// ICU binary data.
///
/// Light-weight, non-const reader class for a `CharsTrie`. Traverses a
/// char-serialized data structure with minimal state, for mapping 16-bit-unit
/// sequences to non-negative integer values.
///
/// For more information:
/// - [ICU4C UCharsTrie](https://unicode-org.github.io/icu-docs/apidoc/released/icu4c/classicu_1_1UCharsTrie.html)
/// - [ICU4J CharsTrie](https://unicode-org.github.io/icu-docs/apidoc/released/icu4j/com/ibm/icu/util/CharsTrie.html) API.
#[cfg_attr(feature = "serde", derive(serde::Deserialize, serde::Serialize))]
#[cfg_attr(feature = "databake", derive(databake::Bake))]
#[cfg_attr(feature = "databake", databake(path = icu_collections::char16trie))]
#[derive(Clone, Debug, PartialEq, Eq, ZeroFrom, Yokeable)]
#[allow(clippy::exhaustive_structs)] // effectively exhaustive, struct-constructible for baking
pub struct Char16Trie<'data> {
    /// An array of u16 containing the trie data.
    #[cfg_attr(feature = "serde", serde(borrow, alias = "trie_data"))]
    #[doc(hidden)] // #2417
    pub data: ZeroVec<'data, u16>,
}

impl<'data> Char16Trie<'data> {
    /// Returns a new [`Char16Trie`] with ownership of the provided data.
    #[inline]
    pub fn new(data: ZeroVec<'data, u16>) -> Self {
        Self { data }
    }

    /// Returns a new [`Char16TrieCursor`] backed by borrowed data from the `trie` data.
    #[inline]
    pub fn cursor(&self) -> Char16TrieCursor<'_> {
        Char16TrieCursor::new(&self.data)
    }

    /// Returns a new [`Char16TrieIterator`] backed by borrowed data from the `trie` data
    #[deprecated(since = "2.4.0", note = "use `Char16Trie::cursor`")]
    #[allow(deprecated)]
    #[inline]
    pub fn iter(&self) -> Char16TrieIterator<'_> {
        Char16TrieIterator::new(&self.data)
    }

    /// Returns an iterator over the key/value pairs in this trie.
    ///
    /// This is called `iter2` because of the deprecated `iter` API, which
    /// is conceptually a lookup cursor, not an iterator.
    ///
    /// ✨ *Enabled with the `alloc` Cargo feature.*
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::collections::char16trie::Char16Trie;
    /// use zerovec::ZeroVec;
    ///
    /// // A Char16Trie containing the ASCII characters 'a' and 'ab'.
    /// let trie_data = [48, 97, 176, 98, 32868];
    /// let trie = Char16Trie::new(ZeroVec::from_slice_or_alloc(&trie_data));
    ///
    /// let mut it = trie.iter2();
    /// assert_eq!(it.next(), Some(("a".into(), 1)));
    /// assert_eq!(it.next(), Some(("ab".into(), 100)));
    /// assert_eq!(it.next(), None);
    /// ```
    #[cfg(feature = "alloc")]
    pub fn iter2(&self) -> impl Iterator<Item = (String, i32)> + '_ {
        let cursor = self.cursor();
        let mut root_value = cursor.value();
        let mut stack = alloc::vec![(cursor, 0)];
        let mut prefix = String::new();
        let mut lead_surrogate = None;
        core::iter::from_fn(move || {
            if let Some(v) = root_value.take() {
                return Some((String::new(), v));
            }
            while let Some((cursor, i)) = stack.last_mut() {
                let mut next_cursor = cursor.clone();
                if let Some(c) = next_cursor.probe(*i) {
                    *i += 1;

                    if let Some(lead) = lead_surrogate.take() {
                        prefix.push(
                            char::decode_utf16([lead, c])
                                .next()
                                .and_then(Result::ok)
                                .unwrap_or(char::REPLACEMENT_CHARACTER),
                        );
                    } else if matches!(c, 0xD800..0xDC00) {
                        lead_surrogate = Some(c);
                    } else {
                        prefix
                            .push(char::from_u32(c as u32).unwrap_or(char::REPLACEMENT_CHARACTER));
                    }

                    let v = next_cursor.value();
                    stack.push((next_cursor, 0));
                    if let Some(v) = v {
                        return Some((prefix.clone(), v));
                    }
                } else {
                    stack.pop();
                    if lead_surrogate.take().is_none()
                        && let Some(ch @ '\u{10000}'..) = prefix.pop()
                    {
                        lead_surrogate = Some(u16_lead(ch as u32));
                    }
                }
            }
            None
        })
    }
}

/// A cursor into a [`Char16Trie`], useful for stepwise lookup.
#[derive(Clone, Debug)]
pub struct Char16TrieCursor<'a> {
    /// A reference to the [`Char16Trie`] data to iterate over.
    trie: &'a ZeroSlice<u16>,
    /// Index of next trie unit to read, or `None` if there are no more matches.
    pos: Option<usize>,
    /// Remaining length of a linear-match node, minus 1, or `None` if not in
    /// such a node.
    remaining_match_length: Option<u8>,
}

/// This struct represents an iterator over a [`Char16Trie`].
#[deprecated(since = "2.4.0", note = "use `Char16TrieCursor`")]
#[derive(Clone, Debug)]
pub struct Char16TrieIterator<'a>(Char16TrieCursor<'a>);

/// An enum representing the return value from a lookup in [`Char16Trie`].
#[deprecated(since = "2.4.0", note = "use `Char16TrieCursor`")]
#[derive(Clone, Copy, Debug, PartialEq)]
#[allow(clippy::exhaustive_enums)]
pub enum TrieResult {
    /// The input unit(s) did not continue a matching string.
    /// Once `next()` returns `TrieResult::NoMatch`, all further calls to `next()`
    /// will also return `TrieResult::NoMatch`.
    NoMatch,
    /// The input unit(s) matched a string but there is no value for the string
    /// so far.  (It is a prefix of a longer string.)
    NoValue,
    /// The input unit(s) continued a matching string and there is a value for
    /// the string so far. No further input byte/unit can continue a matching
    /// string.
    FinalValue(i32),
    /// The input unit(s) continued a matching string and there is a value for
    /// the string so far.  Another input byte/unit can continue a matching
    /// string.
    Intermediate(i32),
}

// Get the lead surrogate (0xd800..0xdbff) for a
// supplementary code point (0x10000..0x10ffff).
// @param supplementary 32-bit code point (U+10000..U+10ffff)
// @return lead surrogate (U+d800..U+dbff) for supplementary
fn u16_lead(supplementary: u32) -> u16 {
    (((supplementary) >> 10) + 0xd7c0) as u16
}

// Get the trail surrogate (0xdc00..0xdfff) for a
// supplementary code point (0x10000..0x10ffff).
// @param supplementary 32-bit code point (U+10000..U+10ffff)
// @return trail surrogate (U+dc00..U+dfff) for supplementary
fn u16_tail(supplementary: u32) -> u16 {
    (((supplementary) & 0x3ff) | 0xdc00) as u16
}

impl<'a> Char16TrieCursor<'a> {
    /// Returns a new [`Char16TrieCursor`] backed by borrowed data for the `trie` array.
    #[inline]
    pub fn new(trie: &'a ZeroSlice<u16>) -> Self {
        Self {
            trie,
            pos: Some(0),
            remaining_match_length: None,
        }
    }

    /// Steps the cursor one `char` into the trie.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::collections::char16trie::Char16Trie;
    /// use zerovec::ZeroVec;
    ///
    /// // A Char16Trie containing the ASCII characters 'a' and 'ab'.
    /// let trie_data = [48, 97, 176, 98, 32868];
    /// let trie = Char16Trie::new(ZeroVec::from_slice_or_alloc(&trie_data));
    ///
    /// let mut cursor = trie.cursor();
    /// cursor.step('a');
    /// assert_eq!(cursor.value(), Some(1));
    /// assert!(!cursor.is_empty());
    /// cursor.step('b');
    /// assert_eq!(cursor.value(), Some(100));
    /// assert!(cursor.is_empty());
    /// cursor.step('c');
    /// assert_eq!(cursor.value(), None);
    /// assert!(cursor.is_empty());
    /// ```
    #[inline]
    pub fn step(&mut self, c: char) {
        self.step32(c as u32);
    }

    /// Steps the cursor one code point (up to `0x10ffff`) into the trie.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::collections::char16trie::Char16Trie;
    /// use zerovec::ZeroVec;
    ///
    /// // A Char16Trie containing the ASCII characters 'a' and 'ab'.
    /// let trie_data = [48, 97, 176, 98, 32868];
    /// let trie = Char16Trie::new(ZeroVec::from_slice_or_alloc(&trie_data));
    ///
    /// let mut cursor = trie.cursor();
    /// cursor.step32('a' as u32);
    /// assert_eq!(cursor.value(), Some(1));
    /// assert!(!cursor.is_empty());
    /// cursor.step32('b' as u32);
    /// assert_eq!(cursor.value(), Some(100));
    /// assert!(cursor.is_empty());
    /// cursor.step32('c' as u32);
    /// assert_eq!(cursor.value(), None);
    /// assert!(cursor.is_empty());
    /// ```
    #[inline]
    pub fn step32(&mut self, c: u32) {
        let c = if let Ok(c) = u16::try_from(c) {
            c
        } else {
            self.step16(u16_lead(c));
            u16_tail(c)
        };
        self.step16(c);
    }

    /// Steps the cursor one 16-bit code unit into the trie.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::collections::char16trie::Char16Trie;
    /// use zerovec::ZeroVec;
    ///
    /// // A Char16Trie containing the ASCII characters 'a' and 'ab'.
    /// let trie_data = [48, 97, 176, 98, 32868];
    /// let trie = Char16Trie::new(ZeroVec::from_slice_or_alloc(&trie_data));
    ///
    /// let mut cursor = trie.cursor();
    /// cursor.step16('a' as u16);
    /// assert_eq!(cursor.value(), Some(1));
    /// assert!(!cursor.is_empty());
    /// cursor.step16('b' as u16);
    /// assert_eq!(cursor.value(), Some(100));
    /// assert!(cursor.is_empty());
    /// cursor.step16('c' as u16);
    /// assert_eq!(cursor.value(), None);
    /// assert!(cursor.is_empty());
    /// ```
    #[inline]
    pub fn step16(&mut self, c: u16) {
        let Some(pos) = self.pos else {
            return;
        };
        self.pos = if let Some(length) = self.remaining_match_length {
            // Remaining part of a linear-match node
            if Some(c) == self.trie.get(pos) {
                self.remaining_match_length = length.checked_sub(1);
                Some(pos + 1)
            } else {
                None
            }
        } else {
            self.next_impl(pos, c)
        };
    }

    #[cfg(feature = "alloc")]
    fn probe(&mut self, mut index: usize) -> Option<u16> {
        let mut pos = self.pos?;
        if let Some(length) = self.remaining_match_length {
            if index > 0 {
                return None;
            }
            let unit = self.trie.get(pos)?;
            self.remaining_match_length = length.checked_sub(1);
            self.pos = Some(pos + 1);
            return Some(unit);
        }
        let mut node = self.trie.get(pos)?;
        pos += 1;
        if node >= MIN_VALUE_LEAD {
            if (node & VALUE_IS_FINAL) != 0 {
                return None;
            }
            // Skip intermediate value.
            pos = skip_node_value(pos, node);
            node &= NODE_TYPE_MASK;
        }
        if node >= MIN_LINEAR_MATCH {
            if index > 0 {
                return None;
            }
            let length = (node - MIN_LINEAR_MATCH) as u8;
            let unit = self.trie.get(pos)?;
            self.remaining_match_length = length.checked_sub(1);
            self.pos = Some(pos + 1);
            return Some(unit);
        }
        let mut length = node as usize;
        if length == 0 {
            length = self.trie.get(pos)? as usize;
            pos += 1;
        }
        length += 1;
        if index >= length {
            return None;
        }

        // The length of the branch is the number of units to select from.
        // The data structure encodes a binary search.
        while length > MAX_BRANCH_LINEAR_SUB_NODE_LENGTH {
            let half = length >> 1;
            if index < half {
                length = half;
                pos = self.jump_by_delta(pos + 1)?;
            } else {
                index -= half;
                length -= half;
                pos = self.skip_delta(pos + 1)?;
            }
        }
        // Drop down to linear search for the last few units.
        while index > 0 {
            index -= 1;
            length -= 1;
            pos = self.skip_value(pos + 1)?;
        }
        let unit = self.trie.get(pos)?;
        pos += 1;
        if length > 1 {
            let node = self.trie.get(pos)?;
            if node & VALUE_IS_FINAL == 0 {
                // Use the non-final value as the jump delta.
                pos += 1;

                if node < MIN_TWO_UNIT_VALUE_LEAD {
                    pos += node as usize;
                } else if node < THREE_UNIT_VALUE_LEAD {
                    pos += (((node - MIN_TWO_UNIT_VALUE_LEAD) as u32) << 16) as usize
                        | self.trie.get(pos)? as usize;
                    pos += 1;
                } else {
                    pos +=
                        ((self.trie.get(pos)? as usize) << 16) | self.trie.get(pos + 1)? as usize;
                    pos += 2;
                }
            }
        }
        self.pos = Some(pos);
        Some(unit)
    }

    /// Returns the value at the current position.
    #[inline]
    pub fn value(&self) -> Option<i32> {
        if self.remaining_match_length.is_some() {
            return None;
        }
        let pos = self.pos?;
        let lead_unit = self.trie.get(pos)?;
        if lead_unit < MIN_VALUE_LEAD {
            None
        } else if lead_unit & VALUE_IS_FINAL != 0 {
            let v = self.read_value(pos + 1, lead_unit & 0x7fff);
            debug_assert!(v.is_some());
            Some(v.unwrap_or(0))
        } else {
            let v = self.read_node_value(pos + 1, lead_unit);
            debug_assert!(v.is_some());
            Some(v.unwrap_or(0))
        }
    }

    /// Checks whether the cursor points to an empty trie (i.e., no further
    /// units can be matched from the current position).
    ///
    /// Use this to determine when to stop iterating.
    #[inline]
    pub fn is_empty(&self) -> bool {
        let Some(pos) = self.pos else {
            return true;
        };
        self.remaining_match_length.is_none()
            && self.trie.get(pos).is_none_or(|u| u & VALUE_IS_FINAL != 0)
    }

    fn branch_next(&self, mut pos: usize, mut length: usize, in_unit: u16) -> Option<usize> {
        if length == 0 {
            length = self.trie.get(pos)? as usize;
            pos += 1;
        }
        length += 1;

        // The length of the branch is the number of units to select from.
        // The data structure encodes a binary search.
        while length > MAX_BRANCH_LINEAR_SUB_NODE_LENGTH {
            if in_unit < self.trie.get(pos)? {
                length >>= 1;
                pos = self.jump_by_delta(pos + 1)?;
            } else {
                length = length - (length >> 1);
                pos = self.skip_delta(pos + 1)?;
            }
        }
        // Drop down to linear search for the last few bytes.
        // length>=2 because the loop body above sees length>kMaxBranchLinearSubNodeLength>=3
        // and divides length by 2.
        loop {
            if in_unit == self.trie.get(pos)? {
                pos += 1;
                let node = self.trie.get(pos)?;
                if node & VALUE_IS_FINAL != 0 {
                    return Some(pos);
                }
                // Use the non-final value as the jump delta.
                pos += 1;

                if node < MIN_TWO_UNIT_VALUE_LEAD {
                    pos += node as usize;
                } else if node < THREE_UNIT_VALUE_LEAD {
                    pos += (((node - MIN_TWO_UNIT_VALUE_LEAD) as u32) << 16) as usize
                        | self.trie.get(pos)? as usize;
                    pos += 1;
                } else {
                    pos +=
                        ((self.trie.get(pos)? as usize) << 16) | self.trie.get(pos + 1)? as usize;
                    pos += 2;
                }
                return Some(pos);
            }
            length -= 1;
            pos = self.skip_value(pos + 1)?;
            if length <= 1 {
                break;
            }
        }

        (in_unit == self.trie.get(pos)?).then_some(pos + 1)
    }

    fn next_impl(&mut self, pos: usize, in_unit: u16) -> Option<usize> {
        let mut node = self.trie.get(pos)?;
        let mut pos = pos + 1;
        loop {
            if node < MIN_LINEAR_MATCH {
                return self.branch_next(pos, node as usize, in_unit);
            } else if node < MIN_VALUE_LEAD {
                // Match the first of length+1 units.
                let length = (node - MIN_LINEAR_MATCH) as u8;
                if in_unit == self.trie.get(pos)? {
                    self.remaining_match_length = length.checked_sub(1);
                    return Some(pos + 1);
                }
                // No match
                break;
            } else if (node & VALUE_IS_FINAL) != 0 {
                // No further matching units.
                break;
            } else {
                // Skip intermediate value.
                pos = skip_node_value(pos, node);
                node &= NODE_TYPE_MASK;
            }
        }
        None
    }

    #[inline(always)] // 1 call site and we want the Option to go away
    fn jump_by_delta(&self, pos: usize) -> Option<usize> {
        let delta = self.trie.get(pos)?;
        let v = if delta < MIN_TWO_UNIT_DELTA_LEAD {
            // nothing to do
            pos + 1 + delta as usize
        } else if delta == THREE_UNIT_DELTA_LEAD {
            let delta =
                ((self.trie.get(pos + 1)? as usize) << 16) | (self.trie.get(pos + 2)? as usize);
            pos + delta + 3
        } else {
            let delta = (((delta - MIN_TWO_UNIT_DELTA_LEAD) as usize) << 16)
                | (self.trie.get(pos + 1)? as usize);
            pos + delta + 2
        };
        Some(v)
    }

    #[inline(always)] // 1 call site and we want the Option to go away
    fn skip_value(&self, pos: usize) -> Option<usize> {
        let lead_unit = self.trie.get(pos)?;
        Some(skip_value(pos + 1, lead_unit & 0x7fff))
    }

    #[inline(always)] // 1 call site and we want the Option to go away
    fn skip_delta(&self, pos: usize) -> Option<usize> {
        let delta = self.trie.get(pos)?;
        let v = if delta < MIN_TWO_UNIT_DELTA_LEAD {
            pos + 1
        } else if delta == THREE_UNIT_DELTA_LEAD {
            pos + 3
        } else {
            pos + 2
        };
        Some(v)
    }

    #[inline(always)] // 1 call site and we want the Option to go away
    fn read_value(&self, pos: usize, lead_unit: u16) -> Option<i32> {
        let v = if lead_unit < MIN_TWO_UNIT_VALUE_LEAD {
            lead_unit.into()
        } else if lead_unit < THREE_UNIT_VALUE_LEAD {
            (((lead_unit - MIN_TWO_UNIT_VALUE_LEAD) as i32) << 16) | self.trie.get(pos)? as i32
        } else {
            ((self.trie.get(pos)? as i32) << 16) | self.trie.get(pos + 1)? as i32
        };
        Some(v)
    }

    #[inline(always)] // 1 call site and we want the Option to go away
    fn read_node_value(&self, pos: usize, lead_unit: u16) -> Option<i32> {
        let v = if lead_unit < (MIN_TWO_UNIT_NODE_VALUE_LEAD) {
            ((lead_unit >> 6) - 1).into()
        } else if lead_unit < THREE_UNIT_NODE_VALUE_LEAD {
            ((((lead_unit & 0x7fc0) - MIN_TWO_UNIT_NODE_VALUE_LEAD) as i32) << 10)
                | self.trie.get(pos)? as i32
        } else {
            ((self.trie.get(pos)? as i32) << 16) | self.trie.get(pos + 1)? as i32
        };
        Some(v)
    }
}

impl core::fmt::Write for Char16TrieCursor<'_> {
    fn write_str(&mut self, s: &str) -> core::fmt::Result {
        for c in s.encode_utf16() {
            self.step16(c);
        }
        Ok(())
    }

    fn write_char(&mut self, c: char) -> core::fmt::Result {
        self.step(c);
        Ok(())
    }
}

#[allow(deprecated)]
impl<'a> Char16TrieIterator<'a> {
    /// Returns a new [`Char16TrieIterator`] backed by borrowed data for the `trie` array
    #[inline]
    pub fn new(trie: &'a ZeroSlice<u16>) -> Self {
        Self(Char16TrieCursor::new(trie))
    }

    /// Traverses the trie from the current state for this input char.
    #[inline]
    pub fn next(&mut self, c: char) -> TrieResult {
        self.next32(c as u32)
    }

    /// Traverses the trie from the current state for this input char.
    #[inline]
    pub fn next32(&mut self, c: u32) -> TrieResult {
        if c <= 0xffff {
            self.next16(c as u16)
        } else {
            self.0.step16(u16_lead(c));
            self.next16(u16_tail(c))
        }
    }

    /// Traverses the trie from the current state for this input char.
    #[inline]
    pub fn next16(&mut self, c: u16) -> TrieResult {
        self.0.step16(c);
        match (self.0.value(), self.0.is_empty()) {
            (Some(v), true) => TrieResult::FinalValue(v),
            (Some(v), false) => TrieResult::Intermediate(v),
            (None, true) => TrieResult::NoMatch,
            (None, false) => TrieResult::NoValue,
        }
    }
}

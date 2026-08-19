//! The Adobe glyph list.
//!
//! Handles mapping between glyph names and characters.

use core::ops::Range;

include!("../../data/generated/generated_agl.rs");

/// Splits the name into its base and variant components.
///
/// For example, returns `("A", Some("swash"))` for the name "A.swash".
pub fn split_variant(name: &str) -> (&str, Option<&str>) {
    name.split_once('.')
        .and_then(|(a, b)| (!a.is_empty()).then_some((a, Some(b))))
        .unwrap_or((name, None))
}

/// Returns the character for the given name, handling both AGL names and
/// uXXXX/uniXXXXXX sequences.
///
/// If the name maps to multiple characters, this returns `None`.
///
/// Variants such as the "alt" in "A.alt" will be ignored for lookup purposes.
pub fn name_to_char(name: &str) -> Option<char> {
    let name = trim_variant(name);
    lookup_char(name)
        .or_else(|| u_to_unicode(name))
        .or_else(|| uni_to_unicode(name, 1).and_then(|mut iter| iter.next()))
}

/// Returns the characters for the given name, handling both AGL names and
/// uXXXX/uniXXXXXX sequences along with a series of components separated by
/// underscores.
///
/// Variants such as the "alt" in "A.alt" will be ignored for lookup purposes.
pub fn name_to_chars(name: &str) -> impl Iterator<Item = char> + '_ {
    let name = trim_variant(name);
    name.split('_').flat_map(|name| {
        if let Some(multi) = lookup_multi_char(name) {
            CharIter::Multi(multi, 0)
        } else if let Some(single) = lookup_char(name).or_else(|| u_to_unicode(name)) {
            CharIter::Single(single)
        } else if let Some(iter) = uni_to_unicode(name, usize::MAX) {
            iter
        } else {
            CharIter::None
        }
    })
}

/// Copies the name of the requested character into the provided buffer and
/// then returns the subslice of that buffer that contains the name.
///
/// A buffer of length [`MAX_NAME_LEN`] is sufficient to hold any name.
///
/// Some codepoints may be mapped to multiple names. See
/// [`char_to_nth_name`] to access all of them.
pub fn char_to_name(ch: impl Into<u32>, dst: &mut [u8]) -> Option<&'_ str> {
    char_to_nth_name(ch, 0, dst)
}

/// Copies the nth name of the requested character into the provided buffer and
/// then returns the subslice of that buffer that contains the name.
///
/// A buffer of length [`MAX_NAME_LEN`] is sufficient to hold any name.
///
/// Some codepoints may be mapped to multiple names. To collect them all, call
/// this with increasing `n` values until the function returns `None`.
pub fn char_to_nth_name(ch: impl Into<u32>, n: usize, dst: &mut [u8]) -> Option<&'_ str> {
    fn handle_node(
        node: &Node,
        ch: u32,
        n: usize,
        n_found: &mut usize,
        name_buf: &mut [u8],
        mut dst_idx: usize,
    ) -> Option<usize> {
        // push chars to name
        for ch in node.name_chars() {
            *name_buf.get_mut(dst_idx)? = ch;
            dst_idx += 1;
        }
        // see if we found the char
        if node.value.map(|cp| cp as u32) == Some(ch) {
            // ... and if it matches our requested index
            if *n_found == n {
                return Some(dst_idx);
            }
            *n_found += 1;
        }
        // recurse into children
        for child in node.children() {
            if let Some(len) = handle_node(&child, ch, n, n_found, name_buf, dst_idx) {
                return Some(len);
            }
        }
        None
    }
    let ch = ch.into();
    let mut n_found = 0;
    for node in Node::new_root(&AGL_TRIE)?.children() {
        if let Some(len) = handle_node(&node, ch, n, &mut n_found, dst, 0) {
            // null terminate for good measure. This is helpful for
            // clients that may access this API from C
            *dst.get_mut(len)? = 0;
            return core::str::from_utf8(dst.get(..len)?).ok();
        }
    }
    None
}

fn trim_variant(name: &str) -> &str {
    name.split_once('.').map(|(a, _)| a).unwrap_or(name)
}

fn lookup_char(name: &str) -> Option<char> {
    let mut cur = Node::new_root(&AGL_TRIE)?;
    let name = name.as_bytes();
    let mut name_idx = 0;
    while let Some(child) = cur.child_for_char(*name.get(name_idx)?) {
        if child.name_chars().eq(name
            .get(name_idx..name_idx + child.num_name_chars())?
            .iter()
            .copied())
        {
            name_idx += child.num_name_chars();
        }
        if name_idx >= name.len() {
            return child.value();
        }
        cur = child;
    }
    None
}

fn lookup_multi_char(name: &str) -> Option<&'static [u16]> {
    AGL_MULTIS
        .binary_search_by_key(&name, |e| e.0)
        .ok()
        .and_then(|idx| AGL_MULTIS.get(idx))
        .map(|e| e.1)
}

// https://github.com/fonttools/fonttools/blob/8697f91cdc/Lib/fontTools/agl.py#L5200
fn uni_to_unicode(component: &str, max_values: usize) -> Option<CharIter<'_>> {
    let digits = component.strip_prefix("uni")?;
    if !digits.bytes().all(is_uppercase_hex) || digits.len() % 4 != 0 {
        return None;
    }
    let n_values = digits.len() / 4;
    (n_values <= max_values).then_some(CharIter::Uni(digits, 0, n_values))
}

fn is_uppercase_hex(b: u8) -> bool {
    b.is_ascii_digit() || (b'A'..=b'F').contains(&b)
}

// https://github.com/fonttools/fonttools/blob/8697f91cdc/Lib/fontTools/agl.py#L5219
fn u_to_unicode(component: &str) -> Option<char> {
    let value = component.strip_prefix('u')?;
    if !value.bytes().all(is_uppercase_hex) || !(4..=6).contains(&value.len()) {
        return None;
    }

    let uv = u32::from_str_radix(value, 16).ok()?;
    if (0..=0xD7FF).contains(&uv) || (0xE000..=0x10FFFF).contains(&uv) {
        char::from_u32(uv)
    } else {
        None
    }
}

enum CharIter<'a> {
    Multi(&'a [u16], usize),
    Single(char),
    // (source, cur, count)
    Uni(&'a str, usize, usize),
    None,
}

impl Iterator for CharIter<'_> {
    type Item = char;

    fn next(&mut self) -> Option<Self::Item> {
        match self {
            Self::Multi(multi, idx) => {
                let cp = *multi.get(*idx)? as u32;
                *idx += 1;
                char::from_u32(cp)
            }
            Self::Single(cp) => {
                let cp = *cp;
                *self = Self::None;
                Some(cp)
            }
            Self::Uni(src, cur, count) => {
                let i = *cur;
                if i >= *count {
                    return None;
                }
                *cur += 1;
                char::from_u32(u32::from_str_radix(src.get(i * 4..i * 4 + 4)?, 16).ok()?)
            }
            Self::None => None,
        }
    }
}

/// Node for processing the AGL trie.
///
/// Meant to offer a structured approach to the fun pointer heavy code
/// in FreeType: <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psnames/pstables.h#L4143>
#[derive(Clone)]
struct Node<'a> {
    data: &'a [u8],
    name_chars_range: Range<usize>,
    value: Option<char>,
    num_children: u8,
    children_start: usize,
}

impl<'a> Node<'a> {
    fn new_root(data: &'a [u8]) -> Option<Self> {
        let num_children = *data.get(1)?;
        Some(Self {
            data,
            name_chars_range: 0..0,
            value: None,
            num_children,
            children_start: 2,
        })
    }

    fn new_child(data: &'a [u8], offset: usize) -> Option<Self> {
        let chars_start = offset;
        let mut chars_end = offset;
        for &ch in data.get(offset..)? {
            chars_end += 1;
            // high bit marks a continuation of char sequence
            if ch & 0x80 == 0 {
                break;
            }
        }
        let mut offset = chars_end;
        // control byte contains child count in the low bits and flag
        // indicating presence of a character value in the high bit
        let control = *data.get(offset)?;
        let num_children = control & 0x7F;
        let value = if control & 0x80 != 0 {
            let ch_start = offset + 1;
            offset += 3;
            char::from_u32(*data.get(ch_start)? as u32 * 256 + *data.get(ch_start + 1)? as u32)
        } else {
            offset += 1;
            None
        };
        // `num_children` 16-bit child offsets follow
        Some(Self {
            data,
            name_chars_range: chars_start..chars_end,
            value,
            num_children,
            children_start: offset,
        })
    }

    fn num_name_chars(&self) -> usize {
        self.name_chars_range.len()
    }

    fn name_chars(&self) -> impl Iterator<Item = u8> + 'a {
        self.data
            .get(self.name_chars_range.clone())
            .unwrap_or_default()
            .iter()
            .map(|ch| *ch & 0x7F)
    }

    fn value(&self) -> Option<char> {
        self.value
    }

    fn children(&self) -> impl Iterator<Item = Self> {
        let this = self.clone();
        (0..self.num_children)
            .filter_map(move |i| Node::new_child(this.data, this.child_offset(i as usize)?))
    }

    /// Returns the offset of the child at the given index.
    fn child_offset(&self, index: usize) -> Option<usize> {
        let child_start = self.children_start + index * 2;
        Some(
            *self.data.get(child_start)? as usize * 256 + *self.data.get(child_start + 1)? as usize,
        )
    }

    /// Binary searches for the child node that starts with `ch`.
    fn child_for_char(&self, ch: u8) -> Option<Self> {
        let mut min = 0;
        let mut max = self.num_children as usize;
        while min < max {
            let idx = (min + max) / 2;
            let child_offset = self.child_offset(idx)?;
            let child_ch = *self.data.get(child_offset)? & 0x7F;
            use core::cmp::Ordering;
            match child_ch.cmp(&ch) {
                Ordering::Equal => return Self::new_child(self.data, child_offset),
                Ordering::Less => min = idx + 1,
                Ordering::Greater => max = idx,
            }
        }
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn variants() {
        let pairs = [
            ("A.swash", Some("swash")),
            (".notdef", None),
            ("A", None),
            ("uni20AC0308.alt", Some("alt")),
        ];
        for (name, var) in pairs {
            assert_eq!(split_variant(name).1, var);
        }
    }

    // https://github.com/fonttools/fonttools/blob/8697f91cdc/Tests/agl_test.py#L6
    #[test]
    fn test_spec_examples() {
        for (input, expected) in [
            ("Lcommaaccent", "Ļ"),
            ("uni20AC0308", "\u{20AC}\u{0308}"),
            ("u1040C", "\u{1040C}"),
            //out of bounds
            ("uniD801DC0C", ""),
            // lowercase is not okay!
            ("uni20ac", ""),
            (
                "Lcommaaccent_uni20AC0308_u1040C.alternate",
                "\u{013B}\u{20AC}\u{0308}\u{1040C}",
            ),
            ("Lcommaaccent_uni013B_u013B", "ĻĻĻ"),
            ("foo", ""),
            (".notdef", ""),
        ] {
            let result = name_to_chars(input).collect::<String>();
            assert_eq!(result, expected, "{result:?} != {expected:?} for '{input}'")
        }
    }

    // https://github.com/fonttools/fonttools/blob/8697f91cdc/Tests/agl_test.py#L20
    #[test]
    fn test_aglfn() {
        for (input, expected) in [("longs_t", "ſt"), ("f_f_i.alt123", "ffi")] {
            let result = name_to_chars(input).collect::<String>();
            assert_eq!(result, expected, "{result:?} != {expected:?} for '{input}'")
        }
    }

    // https://github.com/fonttools/fonttools/blob/8697f91cdc/Tests/agl_test.py#L24
    #[test]
    fn test_uni_abcd() {
        for (input, expected) in [
            ("uni0041", "A"),
            ("uni0041_uni0042_uni0043", "ABC"),
            ("uni004100420043", "ABC"),
            ("uni", ""),
            ("uni41", ""),
            ("uni004101", ""),
            ("uniDC00", ""),
        ] {
            let result = name_to_chars(input).collect::<String>();
            assert_eq!(result, expected, "{result:?} != {expected:?} for '{input}'")
        }
    }

    // https://github.com/fonttools/fonttools/blob/8697f91cdc/Tests/agl_test.py#L33
    #[test]
    fn test_u_abcd() {
        for (input, expected) in [
            ("u0041", "A"),
            ("u00041", "A"),
            ("u000041", "A"),
            ("u0000041", ""),
            ("u0041_uni0041_A.alt", "AAA"),
        ] {
            let result = name_to_chars(input).collect::<String>();
            assert_eq!(result, expected, "{result:?} != {expected:?} for '{input}'")
        }
    }

    // https://github.com/fonttools/fonttools/blob/8697f91cdc/Tests/agl_test.py#L33
    #[test]
    fn union() {
        // Interesting test case because "uni" is a prefix of "union".
        for (input, expected) in [
            ("union", "∪"),
            // U+222A U+FE00 is a Standardized Variant for UNION WITH SERIFS.
            ("union_uniFE00", "\u{222A}\u{FE00}"),
        ] {
            let result = name_to_chars(input).collect::<String>();
            assert_eq!(result, expected, "{result:?} != {expected:?} for '{input}'")
        }
    }

    #[test]
    fn one_name() {
        let expected = [
            ("Cdot", 0x10A),
            ("Cdotaccent", 0x10A),
            ("A", 0x41),
            ("union", 0x222A),
        ];
        for (name, expected_codepoint) in expected {
            assert_eq!(name_to_char(name).unwrap() as u32, expected_codepoint);
            assert_eq!(name_to_chars(name).count(), 1);
            assert_eq!(
                name_to_chars(name).next().unwrap() as u32,
                expected_codepoint
            );
        }
    }

    #[test]
    fn multi_names() {
        for (name, expected_cps) in AGL_MULTIS.iter() {
            let cps: Vec<u16> = name_to_chars(name)
                .map(|cp| cp.try_into().unwrap())
                .collect();
            assert_eq!(cps, *expected_cps);
        }
    }

    #[test]
    fn reverse_map() {
        let pairs = [
            ('∪', "union"),
            ('A', "A"),
            ('á', "aacute"),
            ('Á', "Aacute"),
            (';', "semicolon"),
            ('Ļ', "Lcedilla"),
            ('Ċ', "Cdot"),
        ];
        let mut name_buf = [0u8; MAX_NAME_LEN];
        for (ch, name) in pairs {
            assert_eq!(char_to_name(ch, &mut name_buf).unwrap(), name);
        }
    }

    #[test]
    fn reverse_map_duplicate_names() {
        // chars with multiple names
        let pairs = [
            ('Ċ', &["Cdot", "Cdotaccent"]),
            ('Ļ', &["Lcedilla", "Lcommaaccent"]),
        ];
        let mut name_buf = [0u8; MAX_NAME_LEN];
        for (ch, names) in pairs {
            for (n, name) in names.iter().enumerate() {
                assert_eq!(char_to_nth_name(ch, n, &mut name_buf).unwrap(), *name);
            }
            assert!(char_to_nth_name(ch, names.len(), &mut name_buf).is_none());
        }
    }
}

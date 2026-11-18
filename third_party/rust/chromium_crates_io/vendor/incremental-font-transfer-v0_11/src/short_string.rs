//! A buffer for UTF-8 strings that is optimized for small sizes.

use std::sync::Arc;

const SHORT_BUF_LEN: usize = 22;

/// A buffer for strings that is optimized for small sizes.
#[derive(Clone)]
pub struct ShortString(Storage);

#[derive(Clone)]
enum Storage {
    Short(ShortStorage),
    Long(Arc<str>),
}

#[derive(Clone)]
struct ShortStorage {
    // TODO: We can increase SHORT_BUF_LEN by one if we drop support for len=0 and use
    // `NonZeroU8`. We can also wait for NonMaxU8 to make it into the standard library, see
    // https://github.com/rust-lang/rust/issues/151435.
    len: u8,
    data: [u8; SHORT_BUF_LEN],
}

impl ShortString {
    /// Creates a new `ShortString` from a string slice.
    pub fn new(s: &str) -> ShortString {
        if let Some(s) = ShortStorage::try_from_str(s) {
            return ShortString(Storage::Short(s));
        }
        ShortString(Storage::Long(s.into()))
    }

    /// Returns the buffer contents as a string slice.
    ///
    /// Warning: Short strings incur a utf8 validation check.
    pub fn as_str(&self) -> &str {
        match &self.0 {
            // UNWRAP OK: We only construct ShortStorage from valid &str.
            //
            // TODO: Consider doing std::str::from_utf_unchecked. Its unsafe but bypasses UTF8
            // validation.
            Storage::Short(s) => std::str::from_utf8(s.as_bytes()).unwrap(),
            Storage::Long(data) => data,
        }
    }

    /// Returns the buffer contents as a byte slice.
    pub fn as_bytes(&self) -> &[u8] {
        match &self.0 {
            Storage::Short(s) => s.as_bytes(),
            Storage::Long(data) => data.as_bytes(),
        }
    }

    /// Returns `true` if the buffer is empty.
    pub fn is_empty(&self) -> bool {
        matches!(&self.0, Storage::Short(ShortStorage { len: 0, .. }))
    }

    #[cfg(test)]
    fn is_short(&self) -> bool {
        matches!(self.0, Storage::Short(_))
    }
}

impl std::fmt::Debug for ShortString {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "{:?}", self.as_str())
    }
}

impl std::fmt::Display for ShortString {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(self.as_str())
    }
}

impl Default for ShortString {
    fn default() -> Self {
        ShortString::new("")
    }
}

impl std::hash::Hash for ShortString {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.as_bytes().hash(state);
    }
}

impl From<&str> for ShortString {
    fn from(s: &str) -> Self {
        ShortString::new(s)
    }
}

impl Eq for ShortString {}

impl PartialEq for ShortString {
    fn eq(&self, other: &Self) -> bool {
        self.as_bytes() == other.as_bytes()
    }
}

impl Ord for ShortString {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        // UTF-8 preserves lexicographical order:
        // "The byte-value lexicographic sorting order of UTF-8 strings is the same as if ordered by character numbers."
        // See: https://datatracker.ietf.org/doc/html/rfc3629#section-1
        self.as_bytes().cmp(other.as_bytes())
    }
}

impl PartialOrd for ShortString {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl PartialEq<str> for ShortString {
    fn eq(&self, other: &str) -> bool {
        self.as_bytes() == other.as_bytes()
    }
}

impl PartialOrd<str> for ShortString {
    fn partial_cmp(&self, other: &str) -> Option<std::cmp::Ordering> {
        self.as_bytes().partial_cmp(other.as_bytes())
    }
}

impl PartialEq<&str> for ShortString {
    fn eq(&self, other: &&str) -> bool {
        self.as_bytes() == other.as_bytes()
    }
}

impl PartialOrd<&str> for ShortString {
    fn partial_cmp(&self, other: &&str) -> Option<std::cmp::Ordering> {
        self.as_bytes().partial_cmp(other.as_bytes())
    }
}

impl PartialEq<ShortString> for str {
    fn eq(&self, other: &ShortString) -> bool {
        self.as_bytes() == other.as_bytes()
    }
}

impl PartialOrd<ShortString> for str {
    fn partial_cmp(&self, other: &ShortString) -> Option<std::cmp::Ordering> {
        self.as_bytes().partial_cmp(other.as_bytes())
    }
}

impl PartialEq<ShortString> for &str {
    fn eq(&self, other: &ShortString) -> bool {
        self.as_bytes() == other.as_bytes()
    }
}

impl PartialOrd<ShortString> for &str {
    fn partial_cmp(&self, other: &ShortString) -> Option<std::cmp::Ordering> {
        self.as_bytes().partial_cmp(other.as_bytes())
    }
}

/// A builder for creating a `ShortString`.
#[derive(Debug, Default)]
pub struct ShortStringBuilder {
    len: usize,
    data: [u8; SHORT_BUF_LEN],
    long_data: Option<String>,
}

impl ShortStringBuilder {
    /// Creates a new `ShortStringBuilder` with at least the specified capacity.
    pub fn with_capacity(capacity: usize) -> Self {
        if capacity > SHORT_BUF_LEN {
            Self {
                len: 0,
                data: [0; SHORT_BUF_LEN],
                long_data: Some(String::with_capacity(capacity)),
            }
        } else {
            Self::default()
        }
    }

    /// Pushes a single char to the end of the builder.
    pub fn push(&mut self, ch: char) {
        let ch_len = ch.len_utf8();
        if let Some(long_data) = &mut self.long_data {
            long_data.push(ch);
        } else if self.len + ch_len <= SHORT_BUF_LEN {
            for b in ch.encode_utf8(&mut [0; 4]).as_bytes() {
                self.data[self.len] = *b;
                self.len += 1;
            }
        } else {
            let mut long_data = String::with_capacity(SHORT_BUF_LEN * 2);
            // UNWRAP OK: self.data is populated with `push` and `push_str` which only produce valid
            // UTF8.
            long_data.push_str(std::str::from_utf8(&self.data[..self.len]).unwrap());
            long_data.push(ch);
            self.long_data = Some(long_data);
        }
    }

    /// Extends the builder with the contents of a string slice.
    pub fn push_str(&mut self, s: &str) {
        if let Some(long_data) = &mut self.long_data {
            long_data.push_str(s);
        } else if self.len + s.len() <= SHORT_BUF_LEN {
            self.data[self.len..self.len + s.len()].copy_from_slice(s.as_bytes());
            self.len += s.len();
        } else {
            let required_cap = self.len.checked_add(s.len()).unwrap();
            let capacity = 2 * std::cmp::max(SHORT_BUF_LEN, required_cap);
            let mut long_data = String::with_capacity(capacity);
            // UNWRAP OK: self.data is populated with `push` and `push_str` which only produce valid
            // UTF8.
            long_data.push_str(std::str::from_utf8(&self.data[..self.len]).unwrap());
            long_data.push_str(s);
            self.long_data = Some(long_data);
        }
    }

    /// Consumes the builder and returns a `ShortString`.
    pub fn build(self) -> ShortString {
        if let Some(long_data) = self.long_data {
            ShortString::new(long_data.as_str())
        } else {
            ShortString(Storage::Short(ShortStorage {
                len: self.len as u8,
                data: self.data,
            }))
        }
    }
}

impl ShortStorage {
    fn try_from_str(s: &str) -> Option<ShortStorage> {
        let (bytes, len) = (s.as_bytes(), s.len());
        if len > SHORT_BUF_LEN {
            return None;
        }
        let mut data = [0u8; SHORT_BUF_LEN];
        data[0..len].copy_from_slice(bytes);
        Some(ShortStorage {
            len: len as u8,
            data,
        })
    }

    fn as_bytes(&self) -> &[u8] {
        &self.data[0..self.len as usize]
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::hash_map::DefaultHasher;
    use std::collections::HashMap;
    use std::hash::{Hash, Hasher};

    #[test]
    fn short_string_is_small() {
        assert!(std::mem::size_of::<ShortString>() <= 24);
    }

    #[test]
    fn empty_string_creates_empty_short_variant() {
        let short_str = ShortString::new("");
        assert_eq!(short_str.as_bytes(), &[]);
        assert!(short_str.is_empty());
        assert!(short_str.is_short());
    }

    #[test]
    fn max_short_slice_creates_short_variant() {
        let short_str = ShortString::new(&"*".repeat(SHORT_BUF_LEN));
        assert_eq!(short_str.as_bytes(), &[42u8; SHORT_BUF_LEN]);
        assert!(short_str.is_short());
    }

    #[test]
    fn overflow_short_slice_creates_long_variant() {
        let long_str = ShortString::new(&"*".repeat(SHORT_BUF_LEN + 1));
        assert_eq!(long_str.as_bytes(), &[42u8; SHORT_BUF_LEN + 1]);
        assert!(matches!(long_str.0, Storage::Long(_)));
    }

    #[test]
    fn as_bytes_on_short_variant_returns_correct_string() {
        let short_str = ShortString::new("abc");
        assert_eq!(short_str.as_str(), "abc");
    }

    #[test]
    fn as_bytes_on_long_variant_returns_correct_bytes() {
        let s = "a".repeat(2 * SHORT_BUF_LEN);
        let long_str = ShortString::new(&s);
        assert_eq!(long_str.as_str(), &s);
    }

    #[test]
    fn is_empty_on_empty_buffer_returns_true() {
        assert!(ShortString::new("").is_empty());
    }

    #[test]
    fn is_empty_on_non_empty_buffer_returns_false() {
        assert!(!ShortString::new("a").is_empty());
    }

    #[test]
    fn debug_format_on_utf8_returns_representation() {
        assert_eq!(
            format!("{:?}", ShortString::new("abcABC123")),
            "\"abcABC123\""
        );
    }

    #[test]
    fn display_format() {
        assert_eq!(format!("{}", ShortString::new("abcABC123")), "abcABC123");
    }

    #[test]
    fn partial_eq_with_str_and_str_ref() {
        let short_str = ShortString::new("abc");
        assert_eq!(short_str, "abc");
        assert_eq!("abc", short_str);

        let s: &str = "abc";
        assert_eq!(short_str, s);
        assert_eq!(s, short_str);
    }

    #[test]
    fn ord_and_partial_ord() {
        let s1 = ShortString::new("abc");
        let s2 = ShortString::new("def");
        let s3 = ShortString::new(&"z".repeat(SHORT_BUF_LEN + 1));

        assert!(s1 < s2);
        assert!(s2 < s3);
        assert!(s1 < s3);

        assert!(s1 < "and");
        assert!("abb" < s1);

        assert_eq!(s1.cmp(&s2), std::cmp::Ordering::Less);
        assert_eq!(s2.cmp(&s1), std::cmp::Ordering::Greater);
        assert_eq!(s1.cmp(&s1), std::cmp::Ordering::Equal);
    }

    #[test]
    fn clone_on_short_variant_returns_identical_buffer() {
        let short_str1 = ShortString::new("short");
        let short_str2 = short_str1.clone();
        assert_eq!(short_str1, short_str2);
        assert!(short_str2.is_short());
    }

    #[test]
    fn clone_on_long_variant_returns_identical_buffer() {
        let s = "0".repeat(100);
        let short_str1 = ShortString::new(&s);
        let short_str2 = short_str1.clone();
        assert_eq!(short_str1, short_str2);
        assert!(!short_str2.is_short());
    }

    #[test]
    fn hash_on_equal_buffers_yields_same_hash_value() {
        let short_str1 = ShortString::new("hash_me");
        let short_str2 = ShortString::new("hash_me");

        let mut hasher1 = DefaultHasher::new();
        let mut hasher2 = DefaultHasher::new();

        short_str1.hash(&mut hasher1);
        short_str2.hash(&mut hasher2);

        assert_eq!(hasher1.finish(), hasher2.finish());
    }

    #[test]
    fn hashmap_key_works_with_short_string_key() {
        let map: HashMap<ShortString, usize> = HashMap::from_iter([
            (ShortString::new("short"), 1),
            (ShortString::new("this uses long string variant"), 2),
        ]);

        assert_eq!(map.get(&ShortString::new("short")), Some(&1));
        assert_eq!(
            map.get(&ShortString::new("this uses long string variant")),
            Some(&2)
        );
        assert_eq!(map.get(&ShortString::new("nonexistent")), None);
    }

    #[test]
    fn small_capacity_uses_short_storage() {
        let builder = ShortStringBuilder::with_capacity(SHORT_BUF_LEN);
        assert!(builder.long_data.is_none());
    }

    #[test]
    fn large_capacity_uses_long_storage() {
        let builder = ShortStringBuilder::with_capacity(SHORT_BUF_LEN + 1);
        assert!(builder.long_data.is_some());
    }

    #[test]
    fn push_within_limit_stays_short() {
        let mut builder = ShortStringBuilder::default();
        builder.push('a');
        builder.push_str("cde");
        assert!(builder.long_data.is_none());
    }

    #[test]
    fn push_exceeding_limit_becomes_long() {
        let mut builder = ShortStringBuilder::default();
        builder.push_str(&"a".repeat(SHORT_BUF_LEN));
        builder.push('b');
        assert!(builder.long_data.is_some());
    }

    #[test]
    fn push_str_exceeding_limit_becomes_long() {
        let mut builder = ShortStringBuilder::default();
        builder.push_str(&"a".repeat(SHORT_BUF_LEN));
        builder.push_str("b");
        assert!(builder.long_data.is_some());
    }

    #[test]
    fn multibyte_push_overflow_becomes_long() {
        let mut builder = ShortStringBuilder::default();
        builder.push_str(&"a".repeat(SHORT_BUF_LEN - 1));
        builder.push('🦀'); // 4 bytes
        assert!(builder.long_data.is_some());
    }

    #[test]
    fn build_small_string_from_long_storage_becomes_short() {
        let mut builder = ShortStringBuilder::with_capacity(SHORT_BUF_LEN + 1);
        builder.push('a');
        let short_str = builder.build();
        assert!(short_str.is_short());
        assert_eq!(short_str, "a");
    }

    #[test]
    fn build_large_string_from_long_storage_stays_long() {
        let mut builder = ShortStringBuilder::default();
        let s = "a".repeat(SHORT_BUF_LEN + 1);
        builder.push_str(&s);

        let large_str = builder.build();
        assert!(!large_str.is_short());
        assert_eq!(large_str, s.as_str());
    }

    #[test]
    fn build_default_returns_short() {
        let s = ShortStringBuilder::default().build();
        assert!(s.is_short());
        assert!(s.is_empty());
    }
}

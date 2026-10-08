use core::str::FromStr;
use smallvec::SmallVec;

type SmallVecLanguage = SmallVec<[u8; 8]>;

/// A language tag used to select shaping rules.
#[derive(Clone, PartialEq, Eq, Hash, Debug)]
pub struct Language(SmallVecLanguage);

impl Language {
    /// Creates a language from nonempty bytes.
    ///
    /// ASCII letters are lowercased and underscores become hyphens. Returns
    /// `None` for empty input.
    #[inline]
    pub fn new(bytes: impl AsRef<[u8]>) -> Option<Self> {
        let bytes = bytes.as_ref();
        (!bytes.is_empty()).then(|| Language::from_bytes(bytes))
    }

    /// Returns the language as bytes.
    #[inline]
    pub fn as_bytes(&self) -> &[u8] {
        &self.0
    }

    /// Returns the language as UTF-8, or an empty string for invalid UTF-8.
    #[inline]
    pub fn as_str(&self) -> &str {
        core::str::from_utf8(&self.0).unwrap_or_default()
    }

    fn from_bytes(bytes: &[u8]) -> Self {
        if bytes.is_empty() {
            Language(SmallVec::new())
        } else {
            let mut bytes = SmallVecLanguage::from_slice(bytes);

            // Convert uppercase to lowercase and replace '_' with '-'.
            for b in &mut bytes.iter_mut() {
                if b.is_ascii_uppercase() {
                    *b = b.to_ascii_lowercase();
                } else if *b == b'_' {
                    *b = b'-';
                }
            }

            Language(bytes)
        }
    }
}

impl FromStr for Language {
    type Err = &'static str;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        if !s.is_empty() {
            Ok(Language::from_bytes(s.as_bytes()))
        } else {
            Err("invalid language")
        }
    }
}

#[cfg(test)]
mod tests_language {
    use super::*;
    use alloc::string::String;
    use alloc::vec::Vec;

    #[test]
    fn new_empty() {
        assert_eq!(Language::new(""), None);
    }

    #[test]
    fn new_basic() {
        let lang = Language::new("en").unwrap();
        assert_eq!(lang.as_str(), "en");
        assert_eq!(lang.as_bytes(), b"en");
    }

    #[test]
    fn new_lowercases() {
        let lang = Language::new("EN-US").unwrap();
        assert_eq!(lang.as_str(), "en-us");
        assert_eq!(lang.as_bytes(), b"en-us");
    }

    #[test]
    fn new_replaces_underscore() {
        let lang = Language::new("en_US").unwrap();
        assert_eq!(lang.as_str(), "en-us");
        assert_eq!(lang.as_bytes(), b"en-us");
    }

    #[test]
    fn new_accepts_str() {
        let lang = Language::new("zh-Hant").unwrap();
        assert_eq!(lang.as_str(), "zh-hant");
    }

    #[test]
    fn new_accepts_byte_slice() {
        let lang = Language::new(b"zh-Hant" as &[u8]).unwrap();
        assert_eq!(lang.as_str(), "zh-hant");
    }

    #[test]
    fn new_accepts_byte_array() {
        let lang = Language::new(*b"zh-Hant").unwrap();
        assert_eq!(lang.as_str(), "zh-hant");
    }

    #[test]
    fn new_accepts_string() {
        let lang = Language::new(String::from("zh-Hant")).unwrap();
        assert_eq!(lang.as_str(), "zh-hant");
    }

    #[test]
    fn new_accepts_vec() {
        let lang = Language::new(Vec::from(b"zh-Hant" as &[u8])).unwrap();
        assert_eq!(lang.as_str(), "zh-hant");
    }

    #[test]
    fn new_matches_from_str() {
        assert_eq!(
            Language::new("en-US"),
            Some(Language::from_str("en-US").unwrap())
        );
        assert_eq!(
            Language::new("zh_Hant_TW"),
            Some(Language::from_str("zh_Hant_TW").unwrap())
        );
    }

    #[test]
    fn new_empty_matches_from_str() {
        assert_eq!(Language::new(""), None);
        assert!(Language::from_str("").is_err());
    }
}

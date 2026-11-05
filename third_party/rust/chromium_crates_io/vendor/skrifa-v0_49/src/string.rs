//! Localized strings describing font names and other metadata.
//!
//! This provides higher level interfaces for accessing the data in the
//! OpenType [name](https://learn.microsoft.com/en-us/typography/opentype/spec/name)
//! table.
//!
//! # Example
//! The following function will print all localized strings from the set
//! of predefined identifiers in a font:
//! ```
//! use skrifa::{string::StringId, MetadataProvider};
//!
//! fn print_well_known_strings<'a>(font: &impl MetadataProvider<'a>) {
//!     for id in StringId::predefined() {
//!         let strings = font.localized_strings(id);
//!         if strings.clone().next().is_some() {
//!             println!("[{:?}]", id);
//!             for string in font.localized_strings(id) {
//!                 println!("{:?} {}", string.language(), string.to_string());
//!             }
//!         }
//!     }
//! }
//! ```

use read_fonts::{
    tables::name::{language_id_to_bcp47, CharIter, Name, NameRecord, NameString},
    FontRef, TableProvider,
};

use core::fmt;

#[doc(inline)]
pub use read_fonts::types::NameId as StringId;

/// Iterator over the characters of a string.
#[derive(Clone)]
pub struct Chars<'a> {
    inner: Option<CharIter<'a>>,
}

impl Iterator for Chars<'_> {
    type Item = char;

    fn next(&mut self) -> Option<Self::Item> {
        self.inner.as_mut()?.next()
    }
}

/// Iterator over a collection of localized strings for a specific identifier.
#[derive(Clone)]
pub struct LocalizedStrings<'a> {
    name: Option<Name<'a>>,
    records: core::slice::Iter<'a, NameRecord>,
    id: StringId,
}

impl<'a> LocalizedStrings<'a> {
    /// Creates a new localized string iterator from the given font and string identifier.
    pub fn new(font: &FontRef<'a>, id: StringId) -> Self {
        let name = font.name().ok();
        let records = name
            .as_ref()
            .map(|name| name.name_record().iter())
            .unwrap_or([].iter());
        Self { name, records, id }
    }

    /// Creates a new localized string iterator from the given `name` table and string identifier.
    pub fn from_name_table(name: Name<'a>, id: StringId) -> Self {
        let records = name.name_record().iter();
        Self {
            name: Some(name),
            records,
            id,
        }
    }

    /// Returns the informational string identifier for this iterator.
    pub fn id(&self) -> StringId {
        self.id
    }

    /// Returns the best available English string or the first string in the sequence.
    ///
    /// This prefers the following languages, in order: "en-US", "en",
    /// "" (empty, for bare Unicode platform strings which don't have an associated
    /// language).
    ///
    /// If none of these are found, returns the first string, or `None` if the sequence
    /// is empty.
    pub fn english_or_first(self) -> Option<LocalizedString<'a>> {
        let mut best_rank = -1;
        let mut best_string = None;
        for (i, string) in self.enumerate() {
            let rank = match (i, string.language()) {
                (_, Some("en-US")) => return Some(string),
                (_, Some("en")) => 2,
                (_, None) => 1,
                (0, _) => 0,
                _ => continue,
            };
            if rank > best_rank {
                best_rank = rank;
                best_string = Some(string);
            }
        }
        best_string
    }
}

impl<'a> Iterator for LocalizedStrings<'a> {
    type Item = LocalizedString<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        let name = self.name.as_ref()?;
        loop {
            let record = self.records.next()?;
            if record.name_id() == self.id {
                return Some(LocalizedString::new(name, record));
            }
        }
    }
}

impl Default for LocalizedStrings<'_> {
    fn default() -> Self {
        Self {
            name: None,
            records: [].iter(),
            id: StringId::default(),
        }
    }
}

/// String containing a name or other font metadata in a specific language.
#[derive(Clone, Debug)]
pub struct LocalizedString<'a> {
    language: Option<Language>,
    value: Option<NameString<'a>>,
}

impl<'a> LocalizedString<'a> {
    pub fn new(name: &Name<'a>, record: &NameRecord) -> Self {
        let language = Language::new(name, record);
        let value = record.string(name.string_data()).ok();
        Self { language, value }
    }

    /// Returns the BCP-47 language identifier for the localized string.
    pub fn language(&self) -> Option<&str> {
        self.language.as_ref().map(|language| language.as_str())
    }

    /// Returns an iterator over the characters of the localized string.
    pub fn chars(&self) -> Chars<'a> {
        Chars {
            inner: self.value.map(|value| value.chars()),
        }
    }
}

impl fmt::Display for LocalizedString<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        for ch in self.chars() {
            ch.fmt(f)?;
        }
        Ok(())
    }
}

/// This value is chosen arbitrarily to accommodate common language tags that
/// are almost always <= 11 bytes (LLL-SSSS-RR where L is primary language, S
/// is script and R is region) and to keep the Language enum at a reasonable
/// 32 bytes in size.
const MAX_INLINE_LANGUAGE_LEN: usize = 30;

#[derive(Copy, Clone, Debug)]
#[repr(u8)]
enum Language {
    Inline {
        buf: [u8; MAX_INLINE_LANGUAGE_LEN],
        len: u8,
    },
    Static(&'static str),
}

impl Language {
    fn new(name: &Name, record: &NameRecord) -> Option<Self> {
        let language_id = record.language_id();
        // For version 1 name tables, prefer language tags:
        // https://learn.microsoft.com/en-us/typography/opentype/spec/name#naming-table-version-1
        const BASE_LANGUAGE_TAG_ID: u16 = 0x8000;
        if name.version() == 1 && language_id >= BASE_LANGUAGE_TAG_ID {
            let index = (language_id - BASE_LANGUAGE_TAG_ID) as usize;
            let language_string = name
                .lang_tag_record()?
                .get(index)?
                .lang_tag(name.string_data())
                .ok()?;
            Self::from_name_string(&language_string)
        } else {
            Self::from_language_id(record.platform_id(), language_id)
        }
    }

    /// Decodes a language tag string into an inline ASCII byte sequence.
    fn from_name_string(s: &NameString) -> Option<Self> {
        let mut buf = [0u8; MAX_INLINE_LANGUAGE_LEN];
        let mut len = 0;
        for ch in s.chars() {
            // From "Tags for Identifying Languages" <https://www.rfc-editor.org/rfc/rfc5646.html#page-6>:
            // "Although [RFC5234] refers to octets, the language tags described in
            // this document are sequences of characters from the US-ASCII [ISO646]
            // repertoire"
            // Therefore we assume that non-ASCII characters signal an invalid language tag.
            if !ch.is_ascii() || len == MAX_INLINE_LANGUAGE_LEN {
                return None;
            }
            buf[len] = ch as u8;
            len += 1;
        }
        Some(Self::Inline {
            buf,
            len: len as u8,
        })
    }

    fn from_language_id(platform_id: u16, language_id: u16) -> Option<Self> {
        Some(Self::Static(language_id_to_bcp47(
            platform_id,
            language_id,
        )?))
    }

    fn as_str(&self) -> &str {
        match self {
            Self::Inline { buf: data, len } => {
                let data = &data[..*len as usize];
                core::str::from_utf8(data).unwrap_or_default()
            }
            Self::Static(str) => str,
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::MetadataProvider;

    use super::*;
    use read_fonts::FontRef;

    #[test]
    fn localized() {
        let font = FontRef::new(font_test_data::NAMES_ONLY).unwrap();
        let mut subfamily_names = font
            .localized_strings(StringId::SUBFAMILY_NAME)
            .map(|s| (s.language().unwrap().to_string(), s.to_string()))
            .collect::<Vec<_>>();
        subfamily_names.sort_by(|a, b| a.0.cmp(&b.0));
        let expected = [
            (String::from("ar-SA"), String::from("عادي")),
            (String::from("el-GR"), String::from("Κανονικά")),
            (String::from("en"), String::from("Regular")),
            (String::from("eu-ES"), String::from("Arrunta")),
            (String::from("pl-PL"), String::from("Normalny")),
            (String::from("zh-Hans"), String::from("正常")),
        ];
        assert_eq!(subfamily_names.as_slice(), expected);
    }

    #[test]
    fn find_by_language() {
        let font = FontRef::new(font_test_data::NAMES_ONLY).unwrap();
        assert_eq!(
            font.localized_strings(StringId::SUBFAMILY_NAME)
                .find(|s| s.language() == Some("pl-PL"))
                .unwrap()
                .to_string(),
            "Normalny"
        );
    }

    #[test]
    fn english_or_first() {
        let font = FontRef::new(font_test_data::NAMES_ONLY).unwrap();
        assert_eq!(
            font.localized_strings(StringId::SUBFAMILY_NAME)
                .english_or_first()
                .unwrap()
                .to_string(),
            "Regular"
        );
    }
}

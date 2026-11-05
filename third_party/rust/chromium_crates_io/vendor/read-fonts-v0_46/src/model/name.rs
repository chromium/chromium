//! Names of fonts and glyphs.

mod glyph;

pub(crate) use glyph::{glyph_name, glyph_names, GlyphNameIndex};
#[doc(inline)]
pub use glyph::{GlyphName, GlyphNameSource};

use crate::{
    ps::{
        cff::{CffFontRef, Metadata as CffMetadata},
        type1::Type1Font,
    },
    tables::name::{language_id_to_bcp47, Encoding, Name as SfntName, NameRecord, NameString},
    FontData, FontRead, TableProvider,
};
use core::cmp::Ordering;
use core::fmt;

#[doc(inline)]
pub use crate::types::NameId;

/// Access to names in a font.
///
/// SFNT names come from the `name` table. For Type 1 fonts, the available
/// notice, family, weight, full, version, and PostScript names are exposed as
/// name IDs 0, 1, 2, 4, 5, and 6, respectively. A Type 1 `Notice` may contain
/// either copyright or trademark text; it is exposed as ID 0. The axes of a
/// multiple master font are named by IDs from 256, one per axis, which
/// [`Axis::name`](super::variation::Axis::name) returns. Standalone CFF
/// fonts expose family, weight, full, and PostScript names as IDs 1, 2, 4,
/// and 6, and copyright, version, and notice as IDs 0, 5, and 7.
#[derive(Clone, Copy)]
pub struct Names<'a> {
    source: Source<'a>,
}

#[derive(Clone, Copy)]
enum Source<'a> {
    /// The data of the `name` table, if the font has one.
    Sfnt(Option<FontData<'a>>),
    Type1(&'a Type1Font),
    StandaloneCff(Option<CffMetadata<'a>>),
}

impl<'a> Names<'a> {
    /// Creates a name facade from an SFNT table provider.
    pub fn from_sfnt(tables: &impl TableProvider<'a>) -> Self {
        Self {
            source: Source::Sfnt(tables.name().ok().map(|name| name.offset_data())),
        }
    }

    /// Creates a name facade from a Type 1 font.
    pub fn from_type1(font: &'a Type1Font) -> Self {
        Self {
            source: Source::Type1(font),
        }
    }

    /// Creates a name facade from a standalone CFF font.
    pub fn from_cff(font: &CffFontRef<'a>) -> Self {
        Self {
            source: Source::StandaloneCff(font.metadata()),
        }
    }

    /// Returns the name entry for the given identifier.
    ///
    /// Returns `None` if the font has no name with this identifier.
    pub fn get(&self, id: NameId) -> Option<Name<'a>> {
        // Records are sorted by platform, encoding and language before
        // identifier, so those for one identifier are spread out. The first
        // is kept so the strings are found without scanning the records
        // before it again.
        let start = match &self.source {
            Source::Sfnt(data) => sfnt_name(*data)?
                .name_record()
                .iter()
                .position(|record| record.name_id() == id)?,
            Source::Type1(font) => type1_name(font, id).map(|_| 0)?,
            Source::StandaloneCff(meta) => standalone_cff_name(meta.as_ref()?, id).map(|_| 0)?,
        };
        Some(Name {
            source: self.source,
            id,
            start,
        })
    }
}

/// Reads the `name` table from its data.
fn sfnt_name(data: Option<FontData<'_>>) -> Option<SfntName<'_>> {
    SfntName::read(data?).ok()
}

/// Returns the Type 1 string for a name identifier.
fn type1_name(font: &Type1Font, id: NameId) -> Option<&str> {
    match id {
        NameId::COPYRIGHT_NOTICE => font.notice(),
        NameId::FAMILY_NAME => font.family_name(),
        NameId::SUBFAMILY_NAME => font.weight(),
        NameId::FULL_NAME => font.full_name(),
        NameId::VERSION_STRING => font.version(),
        NameId::POSTSCRIPT_NAME => font.name(),
        id => {
            let index = id.to_u16().checked_sub(TYPE1_AXIS_NAMES)?;
            font.multiple_master()?.axes().get(index as usize)?.name()
        }
    }
}

/// The name ID of the first axis of a Type 1 multiple master font. Each
/// later axis takes the next.
pub(crate) const TYPE1_AXIS_NAMES: u16 = 256;

/// Returns the standalone CFF string for a name identifier.
fn standalone_cff_name<'a>(meta: &CffMetadata<'a>, id: NameId) -> Option<&'a str> {
    match id {
        NameId::COPYRIGHT_NOTICE => meta.copyright(),
        NameId::FAMILY_NAME => meta.family_name(),
        NameId::SUBFAMILY_NAME => meta.weight(),
        NameId::FULL_NAME => meta.full_name(),
        NameId::VERSION_STRING => meta.version(),
        NameId::POSTSCRIPT_NAME => meta.name(),
        NameId::TRADEMARK => meta.notice(),
        _ => None,
    }
}

/// The localized strings for one name identifier.
#[derive(Clone, Copy)]
pub struct Name<'a> {
    source: Source<'a>,
    id: NameId,
    /// The index of the first `name` record with this identifier.
    start: usize,
}

impl<'a> Name<'a> {
    /// Returns the identifier for this name.
    pub fn id(&self) -> NameId {
        self.id
    }

    /// Returns `en-US` or `en` if present, falling back to the first entry.
    ///
    /// This prefers `en-US`, then `en`, then a name without a language tag,
    /// and finally the first name in table order. Records with unsupported
    /// encodings are skipped. Returns `None` if no decodable name with this
    /// identifier is available.
    pub fn english_or_first(&self) -> Option<Localized<'a>> {
        let mut best_rank = -1;
        let mut best = None;
        for (index, name) in self.iter().enumerate() {
            let rank = match (index, name.language().as_ref()) {
                (_, Some(language)) if *language == "en-US" => return Some(name),
                (_, Some(language)) if *language == "en" => 2,
                (_, None) => 1,
                (0, _) => 0,
                _ => continue,
            };
            if rank > best_rank {
                best_rank = rank;
                best = Some(name);
            }
        }
        best
    }

    /// Iterates over the localized strings for this identifier, in table order.
    ///
    /// Records with unsupported encodings are skipped. Malformed text in a
    /// supported encoding may contain replacement characters.
    pub fn iter(&self) -> impl Iterator<Item = Localized<'a>> + 'a {
        let (id, start) = (self.id, self.start);
        let sfnt = match self.source {
            Source::Sfnt(data) => sfnt_name(data),
            _ => None,
        };
        let sfnt = sfnt.into_iter().flat_map(move |table| {
            table
                .name_record()
                .get(start..)
                .unwrap_or_default()
                .iter()
                .filter(move |record| record.name_id() == id)
                .filter_map(move |record| Localized::from_record(&table, record))
        });
        let type1 = match &self.source {
            Source::Type1(font) => type1_name(font, id),
            _ => None,
        }
        .map(|name| Localized {
            name: Encoded::from_str(name),
            language: None,
            sfnt_record: None,
        });
        let standalone_cff = match &self.source {
            Source::StandaloneCff(Some(meta)) => standalone_cff_name(meta, id),
            _ => None,
        }
        .map(|name| Localized {
            name: Encoded::from_str(name),
            language: None,
            sfnt_record: None,
        });
        sfnt.chain(type1).chain(standalone_cff)
    }
}

/// A font name and its optional BCP 47 language tag.
///
/// The language is `None` when the source supplies no recognized language or
/// when the name comes from a Type 1 or standalone CFF font.
#[derive(Clone, Copy, Debug)]
pub struct Localized<'a> {
    name: Encoded<'a>,
    language: Option<Encoded<'a>>,
    sfnt_record: Option<NameRecord>,
}

impl<'a> Localized<'a> {
    /// Returns the encoded name text.
    pub fn name(self) -> Encoded<'a> {
        self.name
    }

    /// Returns the encoded language tag, if available.
    pub fn language(self) -> Option<Encoded<'a>> {
        self.language
    }

    /// Returns the source `name` record for an SFNT name, or `None` for Type 1 or CFF.
    pub fn sfnt_record(self) -> Option<NameRecord> {
        self.sfnt_record
    }

    fn from_record(table: &SfntName<'a>, record: &NameRecord) -> Option<Self> {
        if Encoding::new(record.platform_id(), record.encoding_id()) == Encoding::Unknown {
            return None;
        }
        let name = Encoded::from_name_string(record.string(table.string_data()).ok()?);
        let language = if table.version() == 1 && record.language_id() >= 0x8000 {
            let index = (record.language_id() - 0x8000) as usize;
            table
                .lang_tag_record()
                .and_then(|tags| tags.get(index))
                .and_then(|tag| tag.lang_tag(table.string_data()).ok())
                .filter(|tag| tag.chars().all(|ch| ch.is_ascii()))
                .map(Encoded::from_name_string)
        } else {
            language_id_to_bcp47(record.platform_id(), record.language_id()).map(Encoded::from_str)
        };
        Some(Self {
            name,
            language,
            sfnt_record: Some(*record),
        })
    }
}

/// A name or language tag in its source encoding.
///
/// SFNT UTF-16BE and Mac Roman strings are decoded when displayed or compared,
/// without allocating. Type 1 and standalone CFF names and built-in language
/// tags borrow UTF-8 text directly.
#[derive(Clone, Copy)]
pub struct Encoded<'a>(EncodedRepr<'a>);

#[derive(Clone, Copy)]
enum EncodedRepr<'a> {
    Sfnt(NameString<'a>),
    Utf8(&'a str),
}

impl<'a> Encoded<'a> {
    fn from_name_string(name: NameString<'a>) -> Self {
        Self(EncodedRepr::Sfnt(name))
    }

    fn from_str(name: &'a str) -> Self {
        Self(EncodedRepr::Utf8(name))
    }

    /// Iterates over decoded Unicode characters.
    pub fn chars(&self) -> impl Iterator<Item = char> + 'a {
        let sfnt = match self.0 {
            EncodedRepr::Sfnt(name) => Some(name),
            _ => None,
        };
        let utf8 = match self.0 {
            EncodedRepr::Utf8(name) => Some(name),
            _ => None,
        };
        sfnt.into_iter()
            .flat_map(|name| name.chars())
            .chain(utf8.into_iter().flat_map(str::chars))
    }

    /// Returns whether the decoded string begins with `prefix`.
    pub fn starts_with(&self, prefix: &str) -> bool {
        let mut chars = self.chars();
        prefix.chars().all(|ch| chars.next() == Some(ch))
    }
}

impl fmt::Display for Encoded<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        for ch in self.chars() {
            ch.fmt(f)?;
        }
        Ok(())
    }
}

impl fmt::Debug for Encoded<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "\"{self}\"")
    }
}

impl<'a, 'b> PartialEq<Encoded<'b>> for Encoded<'a> {
    fn eq(&self, other: &Encoded<'b>) -> bool {
        self.chars().eq(other.chars())
    }
}

impl Eq for Encoded<'_> {}

impl<'a, 'b> PartialOrd<Encoded<'b>> for Encoded<'a> {
    fn partial_cmp(&self, other: &Encoded<'b>) -> Option<Ordering> {
        Some(self.chars().cmp(other.chars()))
    }
}

impl PartialEq<&str> for Encoded<'_> {
    fn eq(&self, other: &&str) -> bool {
        self.chars().eq(other.chars())
    }
}

impl PartialOrd<&str> for Encoded<'_> {
    fn partial_cmp(&self, other: &&str) -> Option<Ordering> {
        Some(self.chars().cmp(other.chars()))
    }
}

impl PartialEq<Encoded<'_>> for &str {
    fn eq(&self, other: &Encoded<'_>) -> bool {
        other == self
    }
}

impl PartialOrd<Encoded<'_>> for &str {
    fn partial_cmp(&self, other: &Encoded<'_>) -> Option<Ordering> {
        Some(self.chars().cmp(other.chars()))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{model::Font, types::Tag, FontData};
    use alloc::vec::Vec;

    // The iterators borrow the font, not the name or string they came from,
    // so they outlive both.
    #[test]
    fn iterators_outlive_their_names() {
        let font = Font::new(font_test_data::NAMES_ONLY, 0).unwrap();
        let localized = font.names().get(NameId::FAMILY_NAME).unwrap().iter();
        let chars = font
            .names()
            .get(NameId::FAMILY_NAME)
            .unwrap()
            .english_or_first()
            .unwrap()
            .name()
            .chars();
        assert!(localized.count() > 0);
        assert!(chars.count() > 0);
    }

    struct NameProvider<'a>(&'a [u8]);

    impl<'a> TableProvider<'a> for NameProvider<'a> {
        fn data_for_tag(&self, tag: Tag) -> Option<FontData<'a>> {
            (tag == Tag::new(b"name")).then(|| FontData::new(self.0))
        }
    }

    fn name_table(records: &[(u16, u16, u16, NameId, &str)], tags: &[&str]) -> Vec<u8> {
        let mut bytes = Vec::new();
        let version = if tags.is_empty() { 0_u16 } else { 1 };
        let storage_offset = 6
            + records.len() * 12
            + if tags.is_empty() {
                0
            } else {
                2 + tags.len() * 4
            };
        bytes.extend_from_slice(&version.to_be_bytes());
        bytes.extend_from_slice(&(records.len() as u16).to_be_bytes());
        bytes.extend_from_slice(&(storage_offset as u16).to_be_bytes());
        let mut storage = Vec::new();
        for &(platform, encoding, language, id, text) in records {
            let encoded = if platform == 1 {
                text.as_bytes().to_vec()
            } else {
                text.encode_utf16().flat_map(u16::to_be_bytes).collect()
            };
            for value in [
                platform,
                encoding,
                language,
                id.to_u16(),
                encoded.len() as u16,
                storage.len() as u16,
            ] {
                bytes.extend_from_slice(&value.to_be_bytes());
            }
            storage.extend_from_slice(&encoded);
        }
        if !tags.is_empty() {
            bytes.extend_from_slice(&(tags.len() as u16).to_be_bytes());
            for tag in tags {
                let encoded: Vec<u8> = tag.encode_utf16().flat_map(u16::to_be_bytes).collect();
                bytes.extend_from_slice(&(encoded.len() as u16).to_be_bytes());
                bytes.extend_from_slice(&(storage.len() as u16).to_be_bytes());
                storage.extend_from_slice(&encoded);
            }
        }
        bytes.extend_from_slice(&storage);
        bytes
    }

    #[test]
    fn selects_english_by_skrifa_priority() {
        let data = name_table(
            &[
                (3, 1, 0x040c, NameId::FAMILY_NAME, "Français"),
                (0, 4, 0, NameId::FAMILY_NAME, "Bare"),
                (1, 0, 0, NameId::FAMILY_NAME, "English"),
                (3, 1, 0x0409, NameId::FAMILY_NAME, "American English"),
            ],
            &[],
        );
        let name = Names::from_sfnt(&NameProvider(&data))
            .get(NameId::FAMILY_NAME)
            .unwrap();
        let entries: Vec<_> = name.iter().collect();
        assert_eq!(entries.len(), 4);
        assert_eq!(entries[0].name(), "Français");
        assert_eq!(entries[0].language().unwrap(), "fr-FR");
        let record = entries[0].sfnt_record().unwrap();
        assert_eq!(record.name_id(), NameId::FAMILY_NAME);
        assert_eq!(record.platform_id(), 3);
        assert_eq!(record.encoding_id(), 1);
        assert_eq!(record.language_id(), 0x040c);
        assert!(entries[1].language().is_none());
        assert_eq!(entries[2].language().unwrap(), "en");
        let best = name.english_or_first().unwrap();
        assert_eq!(best.name(), "American English");
        assert_eq!(best.language().unwrap(), "en-US");
        assert_eq!(best.name().to_string(), "American English");
    }

    // A missing identifier has no entry, and an entry starting partway
    // through the records still finds every string for its identifier.
    #[test]
    fn get_finds_an_identifier_wherever_its_records_start() {
        let data = name_table(
            &[
                (1, 0, 0, NameId::FAMILY_NAME, "Mac Family"),
                (1, 0, 0, NameId::FULL_NAME, "Mac Full"),
                (3, 1, 0x0409, NameId::FAMILY_NAME, "Windows Family"),
                (3, 1, 0x0409, NameId::FULL_NAME, "Windows Full"),
            ],
            &[],
        );
        let names = Names::from_sfnt(&NameProvider(&data));
        assert!(names.get(NameId::DESCRIPTION).is_none());
        let full: Vec<_> = names
            .get(NameId::FULL_NAME)
            .unwrap()
            .iter()
            .map(|name| name.name().to_string())
            .collect();
        assert_eq!(full, ["Mac Full", "Windows Full"]);
        assert_eq!(names.get(NameId::FAMILY_NAME).unwrap().iter().count(), 2);
    }

    #[test]
    fn skips_names_with_unsupported_encodings() {
        let data = name_table(
            &[
                (3, 2, 0x0409, NameId::FAMILY_NAME, "Unsupported English"),
                (3, 1, 0x040c, NameId::FAMILY_NAME, "Français"),
            ],
            &[],
        );
        let name = Names::from_sfnt(&NameProvider(&data))
            .get(NameId::FAMILY_NAME)
            .unwrap();
        let entries: Vec<_> = name.iter().collect();
        assert_eq!(entries.len(), 1);
        assert_eq!(entries[0].name(), "Français");
        assert_eq!(name.english_or_first().unwrap().name(), "Français");
    }

    #[test]
    fn reads_version_one_language_tags() {
        let data = name_table(
            &[(3, 1, 0x8000, NameId::FULL_NAME, "Tagged Name")],
            &["en-GB"],
        );
        let name = Names::from_sfnt(&NameProvider(&data))
            .get(NameId::FULL_NAME)
            .unwrap();
        let value = name.english_or_first().unwrap();
        assert_eq!(value.name(), "Tagged Name");
        assert_eq!(value.language().unwrap(), "en-GB");

        let invalid = name_table(&[(3, 1, 0x8000, NameId::FULL_NAME, "Tagged Name")], &["é"]);
        let value = Names::from_sfnt(&NameProvider(&invalid))
            .get(NameId::FULL_NAME)
            .unwrap()
            .english_or_first()
            .unwrap();
        assert_eq!(value.name(), "Tagged Name");
        assert!(value.language().is_none());

        let long_tag = "en-Latn-US-x-private-custom-segment";
        assert!(long_tag.len() > 30);
        let data = name_table(
            &[(3, 1, 0x8000, NameId::FULL_NAME, "Tagged Name")],
            &[long_tag],
        );
        let value = Names::from_sfnt(&NameProvider(&data))
            .get(NameId::FULL_NAME)
            .unwrap()
            .english_or_first()
            .unwrap();
        assert_eq!(value.language().unwrap(), long_tag);
    }

    #[test]
    fn exposes_type1_names_by_sfnt_id() {
        let data = font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFB;
        let font = Font::new(data, 0).unwrap();
        for (id, expected) in [
            (
                NameId::COPYRIGHT_NOTICE,
                "Copyright 2015-2021 Google LLC. All Rights Reserved.",
            ),
            (NameId::FAMILY_NAME, "Noto Serif"),
            (NameId::SUBFAMILY_NAME, "Book"),
            (NameId::FULL_NAME, "Noto Serif Regular"),
            (
                NameId::VERSION_STRING,
                "2.007; ttfautohint (v1.8) -l 8 -r 50 -G 200 -x 14 -D latn -f none -a qsq -X \"\"",
            ),
            (NameId::POSTSCRIPT_NAME, "NotoSerif-Regular"),
        ] {
            let name = font.names().get(id).unwrap();
            let value = name.english_or_first().unwrap();
            assert_eq!(value.name(), expected);
            assert!(value.language().is_none());
            assert!(value.sfnt_record().is_none());
            assert_eq!(name.iter().count(), 1);
        }
        assert!(font.names().get(NameId::DESCRIPTION).is_none());

        let type1 = Type1Font::new(data).unwrap();
        assert_eq!(
            Names::from_type1(&type1)
                .get(NameId::POSTSCRIPT_NAME)
                .unwrap()
                .english_or_first()
                .unwrap()
                .name(),
            "NotoSerif-Regular"
        );
    }

    #[test]
    fn encoded_strings_compare_and_match_prefixes_without_decoding_to_string() {
        let data = name_table(
            &[
                (1, 0, 0, NameId::FAMILY_NAME, "English"),
                (3, 1, 0x0409, NameId::FAMILY_NAME, "English"),
                (3, 1, 0x040c, NameId::FAMILY_NAME, "Français"),
            ],
            &[],
        );
        let names = Names::from_sfnt(&NameProvider(&data));
        let entries: Vec<_> = names.get(NameId::FAMILY_NAME).unwrap().iter().collect();
        let mac = entries[0].name();
        let utf16 = entries[1].name();
        let accented = entries[2].name();
        assert_eq!(mac, utf16);
        assert_eq!(mac, "English");
        assert!(mac <= "English");
        assert!("English" >= mac);
        assert!(mac < accented);
        assert!(accented > "English");
        assert!(accented.starts_with("Fran"));
        assert!(accented.starts_with("Français"));
        assert!(accented.starts_with(""));
        assert!(!accented.starts_with("France"));
        assert!(!mac.starts_with("English!"));

        let type1 = Encoded::from_str("Français");
        assert_eq!(accented, type1);
        assert!(type1.starts_with("Fran"));
    }
}

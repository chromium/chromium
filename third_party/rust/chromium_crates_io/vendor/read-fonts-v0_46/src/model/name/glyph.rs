//! Names of individual glyphs.

use super::super::{Font, Kind};
use crate::{
    ps::{
        cff::{charset, CffFontRef},
        type1::Type1Font,
    },
    tables::{cff::Cff, post},
    types::GlyphId16,
    TableProvider,
};
use alloc::{boxed::Box, vec::Vec};
use core::{fmt, iter::Peekable, ops::Deref};
use types::GlyphId;

// The `post` table recommends names of at most 63 bytes. Keep names inline
// so a lookup needs no allocation, including when a fallback name is
// synthesized.
const MAX_GLYPH_NAME_LEN: usize = 63;

/// The source of a glyph name.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GlyphNameSource {
    /// The `post` table.
    Post,
    /// The `CFF` charset and string index.
    Cff,
    /// A Type 1 charstring name.
    Type1,
    /// A generated `gidNNN` name.
    Synthesized,
}

/// The name of one glyph and the source that supplied it.
///
/// A name longer than 63 bytes is truncated at a character boundary.
/// [`Font::glyph_by_name`](crate::model::Font::glyph_by_name) still finds a
/// glyph by its full name.
#[derive(Clone)]
pub struct GlyphName {
    name: [u8; MAX_GLYPH_NAME_LEN],
    len: u8,
    source: GlyphNameSource,
}

impl GlyphName {
    /// Returns the name as a string.
    pub fn as_str(&self) -> &str {
        core::str::from_utf8(&self.name[..self.len as usize]).unwrap_or_default()
    }

    /// Returns the source of this glyph's name.
    pub fn source(&self) -> GlyphNameSource {
        self.source
    }

    /// Returns whether this name was generated because the font supplied none.
    pub fn is_synthesized(&self) -> bool {
        self.source == GlyphNameSource::Synthesized
    }

    fn from_bytes(bytes: &[u8], source: GlyphNameSource) -> Option<Self> {
        let value = core::str::from_utf8(bytes).ok()?;
        if value.is_empty() {
            return None;
        }
        let mut name = Self {
            name: [0; MAX_GLYPH_NAME_LEN],
            len: 0,
            source,
        };
        name.append(value);
        Some(name)
    }

    fn synthesized(glyph: GlyphId) -> Self {
        use fmt::Write;
        let mut name = Self {
            name: [0; MAX_GLYPH_NAME_LEN],
            len: 0,
            source: GlyphNameSource::Synthesized,
        };
        let _ = write!(GlyphNameWrite(&mut name), "gid{}", glyph.to_u32());
        name
    }

    /// Appends as much of `value` as fits, truncated at a character boundary.
    fn append(&mut self, value: &str) {
        let start = self.len as usize;
        let mut count = (MAX_GLYPH_NAME_LEN - start).min(value.len());
        while !value.is_char_boundary(count) {
            count -= 1;
        }
        self.name[start..start + count].copy_from_slice(&value.as_bytes()[..count]);
        self.len = (start + count) as u8;
    }
}

impl fmt::Debug for GlyphName {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("GlyphName")
            .field("name", &self.as_str())
            .field("source", &self.source)
            .finish()
    }
}

impl fmt::Display for GlyphName {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.as_str())
    }
}

impl Deref for GlyphName {
    type Target = str;

    fn deref(&self) -> &Self::Target {
        self.as_str()
    }
}

impl PartialEq<&str> for GlyphName {
    fn eq(&self, other: &&str) -> bool {
        self.as_str() == *other
    }
}

struct GlyphNameWrite<'a>(&'a mut GlyphName);

impl fmt::Write for GlyphNameWrite<'_> {
    fn write_str(&mut self, value: &str) -> fmt::Result {
        self.0.append(value);
        Ok(())
    }
}

enum NameSource<'a> {
    Post(post::Post<'a>),
    SfntCff(Cff<'a>, charset::Charset<'a>),
    StandaloneCff(CffFontRef<'a>, charset::Charset<'a>),
    Type1(&'a Type1Font),
    Synthesized,
}

fn source(font: &Font) -> NameSource<'_> {
    if let Kind::Type1(type1) = font.kind() {
        return NameSource::Type1(type1);
    }
    if let Kind::Cff(cff, _) = font.kind() {
        if cff.is_cid() {
            return NameSource::Synthesized;
        }
        return cff.charset().map_or(NameSource::Synthesized, |charset| {
            NameSource::StandaloneCff(cff, charset)
        });
    }
    if let Ok(post) = font.tables().post() {
        if post.num_names() != 0 {
            return NameSource::Post(post);
        }
    }
    if let Ok(cff) = font.tables().cff() {
        if font.cff().is_some_and(|parsed| parsed.is_cid()) {
            return NameSource::Synthesized;
        }
        if let Some(charset) = cff.charset(0) {
            return NameSource::SfntCff(cff, charset);
        }
    }
    NameSource::Synthesized
}

pub(crate) fn glyph_name(font: &Font, glyph: GlyphId) -> Option<GlyphName> {
    if glyph.to_u32() >= font.num_glyphs() {
        return None;
    }
    let name = match source(font) {
        NameSource::Post(post) => GlyphId16::try_from(glyph)
            .ok()
            .and_then(|id| post.glyph_name(id))
            .and_then(|value| GlyphName::from_bytes(value.as_bytes(), GlyphNameSource::Post)),
        NameSource::SfntCff(cff, charset) => charset
            .string_id(glyph)
            .and_then(|id| cff.string(id))
            .and_then(|value| GlyphName::from_bytes(value, GlyphNameSource::Cff)),
        NameSource::StandaloneCff(cff, charset) => charset
            .string_id(glyph)
            .and_then(|id| cff.string(id))
            .and_then(|value| GlyphName::from_bytes(value, GlyphNameSource::Cff)),
        NameSource::Type1(type1) => type1
            .glyph_name(glyph)
            .and_then(|value| GlyphName::from_bytes(value.as_bytes(), GlyphNameSource::Type1)),
        NameSource::Synthesized => None,
    };
    Some(name.unwrap_or_else(|| GlyphName::synthesized(glyph)))
}

/// A font's glyph names, sorted so a name can be found by binary search.
pub(crate) struct GlyphNameIndex {
    // Every name, one after another, in glyph order.
    names: Box<[u8]>,
    // The start and end of a name in `names`, and its glyph, sorted by name
    // and then glyph.
    entries: Box<[(u32, u32, u32)]>,
}

impl GlyphNameIndex {
    pub(crate) fn new(font: &Font) -> Self {
        let mut names = Vec::new();
        let mut entries = Vec::new();
        // Index the names as the font stores them, so a name longer than a
        // `GlyphName` holds is found by its full spelling.
        let mut iter = name_iter(font);
        while let Some((glyph, name)) = iter.next_stored() {
            let Some((name, _)) = name else {
                continue;
            };
            let start = names.len() as u32;
            names.extend_from_slice(name.as_bytes());
            entries.push((start, names.len() as u32, glyph.to_u32()));
        }
        let name = |entry: &(u32, u32, u32)| &names[entry.0 as usize..entry.1 as usize];
        entries.sort_unstable_by(|a, b| name(a).cmp(name(b)).then(a.2.cmp(&b.2)));
        Self {
            names: names.into_boxed_slice(),
            entries: entries.into_boxed_slice(),
        }
    }

    pub(crate) fn get(&self, name: &str) -> Option<GlyphId> {
        let key = name.as_bytes();
        let index = self.entries.partition_point(|entry| self.name(entry) < key);
        let entry = self.entries.get(index)?;
        (self.name(entry) == key).then(|| GlyphId::new(entry.2))
    }

    fn name(&self, entry: &(u32, u32, u32)) -> &[u8] {
        &self.names[entry.0 as usize..entry.1 as usize]
    }
}

enum NameIterSource<'a> {
    /// Names read from the `post` table, skipping any that cannot be read.
    Post(Peekable<post::GlyphNames<'a>>),
    SfntCff(Cff<'a>, charset::Iter<'a>),
    StandaloneCff(CffFontRef<'a>, charset::Iter<'a>),
    Type1(&'a Type1Font),
    Synthesized,
}

struct NameIter<'a> {
    source: NameIterSource<'a>,
    next: u32,
    count: u32,
}

pub(crate) fn glyph_names(font: &Font) -> impl ExactSizeIterator<Item = (GlyphId, GlyphName)> + '_ {
    name_iter(font)
}

fn name_iter(font: &Font) -> NameIter<'_> {
    let source = match source(font) {
        NameSource::Post(post) => NameIterSource::Post(post.glyph_names().peekable()),
        NameSource::SfntCff(cff, charset) => NameIterSource::SfntCff(cff, charset.iter()),
        NameSource::StandaloneCff(cff, charset) => {
            NameIterSource::StandaloneCff(cff, charset.iter())
        }
        NameSource::Type1(type1) => NameIterSource::Type1(type1),
        NameSource::Synthesized => NameIterSource::Synthesized,
    };
    NameIter {
        source,
        next: 0,
        count: font.num_glyphs(),
    }
}

impl<'a> NameIter<'a> {
    /// Returns the next glyph and its name as the font stores it, or `None`
    /// for the name if the font has no usable one.
    fn next_stored(&mut self) -> Option<(GlyphId, Option<(&'a str, GlyphNameSource)>)> {
        if self.next >= self.count {
            return None;
        }
        let glyph = GlyphId::new(self.next);
        self.next += 1;
        let cff_string = |cff_string: Option<&'a [u8]>| {
            cff_string
                .and_then(|value| core::str::from_utf8(value).ok())
                .map(|value| (value, GlyphNameSource::Cff))
        };
        let name = match &mut self.source {
            NameIterSource::Post(names) => names
                .next_if(|(id, _)| *id == glyph)
                .map(|(_, value)| (value, GlyphNameSource::Post)),
            NameIterSource::SfntCff(cff, names) => names
                .next()
                .and_then(|(id, sid)| cff_string((id == glyph).then(|| cff.string(sid)).flatten())),
            NameIterSource::StandaloneCff(cff, names) => names
                .next()
                .and_then(|(id, sid)| cff_string((id == glyph).then(|| cff.string(sid)).flatten())),
            NameIterSource::Type1(type1) => type1
                .glyph_name(glyph)
                .map(|value| (value, GlyphNameSource::Type1)),
            NameIterSource::Synthesized => None,
        };
        Some((glyph, name.filter(|(value, _)| !value.is_empty())))
    }
}

impl Iterator for NameIter<'_> {
    type Item = (GlyphId, GlyphName);

    fn size_hint(&self) -> (usize, Option<usize>) {
        let remaining = (self.count - self.next) as usize;
        (remaining, Some(remaining))
    }

    fn next(&mut self) -> Option<Self::Item> {
        let (glyph, name) = self.next_stored()?;
        let name = name.and_then(|(value, source)| GlyphName::from_bytes(value.as_bytes(), source));
        Some((glyph, name.unwrap_or_else(|| GlyphName::synthesized(glyph))))
    }
}

impl ExactSizeIterator for NameIter<'_> {}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{model::Blob, FontRef};
    use alloc::sync::Arc;
    use types::Tag;

    #[test]
    fn post_names_agree_between_lookup_and_iteration() {
        let font = Font::new(font_test_data::HVAR_WITH_TRUNCATED_ADVANCE_INDEX_MAP, 0).unwrap();
        let mut iter = font.glyph_names();
        let count = font.num_glyphs() as usize;
        assert_eq!(iter.size_hint(), (count, Some(count)));
        assert!(iter.next().is_some());
        assert_eq!(iter.by_ref().count(), count - 1);

        let names: Vec<_> = font.glyph_names().collect();
        assert_eq!(names.len(), font.num_glyphs() as usize);
        assert_eq!(names[0].1, ".notdef");
        assert_eq!(names[1].1, "space");
        assert_eq!(names[2].1, "A");
        for (glyph, name) in names {
            let looked_up = font.glyph_name(glyph).unwrap();
            assert_eq!(name.as_str(), looked_up.as_str());
            assert_eq!(name.source(), looked_up.source());
        }
        assert!(font.glyph_name(GlyphId::new(font.num_glyphs())).is_none());
    }

    #[test]
    fn a_partial_post_table_synthesizes_only_missing_names() {
        let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(|tag| {
            if tag == Tag::new(b"post") {
                return Some(Blob::from(font_test_data::post::SIMPLE.to_vec()));
            }
            let font = FontRef::new(font_test_data::HVAR_WITH_TRUNCATED_ADVANCE_INDEX_MAP).ok()?;
            font.table_data(tag)
                .map(|data| Blob::from(data.as_bytes().to_vec()))
        });
        let font = Font::new(source, 0).unwrap();
        let names: Vec<_> = font.glyph_names().collect();
        assert!(font.num_glyphs() > 10);
        assert_eq!(names[9].1, "hola");
        assert_eq!(names[9].1.source(), GlyphNameSource::Post);
        assert_eq!(names[10].1, "gid10");
        assert_eq!(names[10].1.source(), GlyphNameSource::Synthesized);
        assert_eq!(
            font.glyph_name(GlyphId::new(9)).unwrap().source(),
            GlyphNameSource::Post
        );
        assert_eq!(
            font.glyph_name(GlyphId::new(10)).unwrap().source(),
            GlyphNameSource::Synthesized
        );
    }

    // A post entry that cannot be read costs that glyph its name, not the
    // glyphs after it.
    #[test]
    fn post_names_survive_a_bad_entry() {
        let long = "a".repeat(70);
        let mut post = alloc::vec![0, 2, 0, 0];
        post.extend_from_slice(&[0; 28]);
        // Glyph 2 names string 5, which the table does not have.
        post.extend_from_slice(&4u16.to_be_bytes());
        for index in [0u16, 258, 258 + 5, 258 + 1] {
            post.extend_from_slice(&index.to_be_bytes());
        }
        for name in ["ok", long.as_str()] {
            post.push(name.len() as u8);
            post.extend_from_slice(name.as_bytes());
        }
        let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(move |tag| {
            if tag == Tag::new(b"post") {
                return Some(Blob::from(post.clone()));
            }
            let font = FontRef::new(font_test_data::HVAR_WITH_TRUNCATED_ADVANCE_INDEX_MAP).ok()?;
            font.table_data(tag)
                .map(|data| Blob::from(data.as_bytes().to_vec()))
        });
        let font = Font::new(source, 0).unwrap();
        let names: Vec<_> = font.glyph_names().collect();
        assert_eq!(names[1].1, "ok");
        assert_eq!(names[2].1, "gid2");
        assert_eq!(names[3].1.as_str(), "a".repeat(63));
        assert_eq!(names[3].1.source(), GlyphNameSource::Post);
        for (glyph, name) in names {
            let looked_up = font.glyph_name(glyph).unwrap();
            assert_eq!(name.as_str(), looked_up.as_str(), "glyph {glyph}");
            assert_eq!(name.source(), looked_up.source());
        }
    }

    // CFF and Type 1 names may hold any UTF-8, so the cut at the length
    // limit falls where a character ends.
    #[test]
    fn a_long_name_is_cut_between_characters() {
        let name = GlyphName::from_bytes("é".repeat(35).as_bytes(), GlyphNameSource::Cff).unwrap();
        assert_eq!(name.as_str(), "é".repeat(31));
    }

    // Every stated name finds the first glyph that has it, and a synthesized
    // name finds nothing.
    #[test]
    fn glyph_by_name_finds_the_first_glyph_with_each_name() {
        let sfnt = FontRef::new(font_test_data::NOTO_SANS_JP_CFF).unwrap();
        let cid_cff = sfnt.cff().unwrap().offset_data().as_bytes();
        let fonts = [
            Font::new(font_test_data::HVAR_WITH_TRUNCATED_ADVANCE_INDEX_MAP, 0).unwrap(),
            Font::new(font_test_data::NOTO_SERIF_DISPLAY_TRIMMED, 0).unwrap(),
            Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA, 0).unwrap(),
            Font::new(cid_cff, 0).unwrap(),
        ];
        for font in &fonts {
            let names: Vec<_> = font.glyph_names().collect();
            for (glyph, name) in &names {
                let first = names
                    .iter()
                    .find(|(_, other)| !other.is_synthesized() && other.as_str() == name.as_str())
                    .map(|(glyph, _)| *glyph);
                assert_eq!(font.glyph_by_name(name.as_str()), first, "glyph {glyph}");
            }
            assert_eq!(font.glyph_by_name("no.such.glyph"), None);
            assert_eq!(font.glyph_by_name(""), None);
        }
        assert_eq!(fonts[0].glyph_by_name("A"), Some(GlyphId::new(2)));
        assert_eq!(fonts[3].glyph_by_name("gid1"), None);
    }

    #[test]
    fn cff_names_agree_between_lookup_and_iteration() {
        let font = Font::new(font_test_data::NOTO_SERIF_DISPLAY_TRIMMED, 0).unwrap();
        let names: Vec<_> = font.glyph_names().collect();
        for (gid, expected) in [".notdef", "i", "j", "k", "l"].iter().enumerate() {
            let (glyph, name) = &names[gid];
            assert_eq!(*glyph, GlyphId::new(gid as u32));
            assert_eq!(name, expected);
            assert_eq!(name.source(), GlyphNameSource::Cff);
            assert_eq!(font.glyph_name(*glyph).unwrap().as_str(), *expected);
        }
    }

    #[test]
    fn standalone_cid_cff_synthesizes_gid_names() {
        let sfnt = FontRef::new(font_test_data::NOTO_SANS_JP_CFF).unwrap();
        let cff = sfnt.cff().unwrap().offset_data().as_bytes();
        let font = Font::new(cff, 0).unwrap();
        let Kind::Cff(raw, _) = font.kind() else {
            panic!("expected standalone CFF font");
        };
        assert!(raw.is_cid());
        let names: Vec<_> = font.glyph_names().collect();
        assert_eq!(names.len(), font.num_glyphs() as usize);
        for (glyph, name) in names {
            assert_eq!(name.as_str(), format!("gid{}", glyph.to_u32()));
            assert!(name.is_synthesized());
            assert_eq!(font.glyph_name(glyph).unwrap().as_str(), name.as_str());
        }
        assert!(font.glyph_name(GlyphId::new(font.num_glyphs())).is_none());
    }

    #[test]
    fn sfnt_cid_cff_without_post_names_synthesizes_gid_names() {
        let font = Font::new(font_test_data::NOTO_SANS_JP_CFF, 0).unwrap();
        assert!(font.cff().unwrap().is_cid());
        let names: Vec<_> = font.glyph_names().take(3).collect();
        for (glyph, name) in names {
            assert_eq!(name.as_str(), format!("gid{}", glyph.to_u32()));
            assert!(name.is_synthesized());
        }
    }

    #[test]
    fn type1_names_agree_between_lookup_and_iteration() {
        let font = Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA, 0).unwrap();
        let Kind::Type1(type1) = font.kind() else {
            panic!("expected Type 1 font");
        };
        let names: Vec<_> = font.glyph_names().collect();
        assert_eq!(names.len(), font.num_glyphs() as usize);
        for (glyph, name) in names {
            assert_eq!(name.as_str(), type1.glyph_name(glyph).unwrap());
            assert_eq!(name.source(), GlyphNameSource::Type1);
            assert_eq!(font.glyph_name(glyph).unwrap().as_str(), name.as_str());
        }
    }

    // A name past the inline limit is found by its full spelling, and two
    // names that share the truncated prefix stay distinct.
    #[test]
    fn long_names_are_found_in_full() {
        let prefix = "a".repeat(63);
        let (x, y) = (prefix.clone() + "xxxxxxx", prefix.clone() + "yyyyyyy");
        let mut post = alloc::vec![0, 2, 0, 0];
        post.extend_from_slice(&[0; 28]);
        post.extend_from_slice(&3u16.to_be_bytes());
        for index in [0u16, 258, 259] {
            post.extend_from_slice(&index.to_be_bytes());
        }
        for name in [&x, &y] {
            post.push(name.len() as u8);
            post.extend_from_slice(name.as_bytes());
        }
        let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(move |tag| {
            if tag == Tag::new(b"post") {
                return Some(Blob::from(post.clone()));
            }
            let font = FontRef::new(font_test_data::HVAR_WITH_TRUNCATED_ADVANCE_INDEX_MAP).ok()?;
            font.table_data(tag)
                .map(|data| Blob::from(data.as_bytes().to_vec()))
        });
        let font = Font::new(source, 0).unwrap();
        assert_eq!(font.glyph_by_name(&x), Some(GlyphId::new(1)));
        assert_eq!(font.glyph_by_name(&y), Some(GlyphId::new(2)));
        assert_eq!(font.glyph_by_name(&prefix), None);
        assert_eq!(font.glyph_name(GlyphId::new(1)).unwrap(), prefix.as_str());
    }

    #[test]
    fn missing_name_tables_synthesize_each_name() {
        let source: Arc<dyn Fn(Tag) -> Option<Blob> + Send + Sync> = Arc::new(|tag| {
            if tag == Tag::new(b"post") || tag == Tag::new(b"CFF ") {
                return None;
            }
            let font = FontRef::new(font_test_data::TINOS_SUBSET).ok()?;
            font.table_data(tag)
                .map(|data| Blob::from(data.as_bytes().to_vec()))
        });
        let font = Font::new(source, 0).unwrap();
        let names: Vec<_> = font.glyph_names().collect();
        assert_eq!(names.len(), font.num_glyphs() as usize);
        for (glyph, name) in names {
            assert_eq!(name.as_str(), format!("gid{}", glyph.to_u32()));
            assert!(name.is_synthesized());
            assert_eq!(font.glyph_name(glyph).unwrap().source(), name.source());
        }
    }
}

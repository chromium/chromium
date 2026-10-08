//! Names of individual glyphs.

use super::super::{Font, Kind};
use crate::{
    ps::{cff::charset, type1::Type1Font},
    tables::{cff::Cff, post},
    types::GlyphId16,
    TableProvider,
};
use core::{fmt, ops::Deref};
use types::GlyphId;

// `post` version 2 limits names to 63 bytes. Keep names inline so a lookup
// needs no allocation, including when a fallback name is synthesized.
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

    /// Returns where this glyph's name came from.
    pub fn source(&self) -> GlyphNameSource {
        self.source
    }

    /// Returns whether this name was generated because the font supplied none.
    pub fn is_synthesized(&self) -> bool {
        self.source == GlyphNameSource::Synthesized
    }

    fn from_bytes(bytes: &[u8], source: GlyphNameSource) -> Option<Self> {
        if bytes.is_empty() || core::str::from_utf8(bytes).is_err() {
            return None;
        }
        let mut name = Self {
            name: [0; MAX_GLYPH_NAME_LEN],
            len: 0,
            source,
        };
        name.append(bytes);
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

    fn append(&mut self, bytes: &[u8]) {
        let start = self.len as usize;
        let count = (MAX_GLYPH_NAME_LEN - start).min(bytes.len());
        self.name[start..start + count].copy_from_slice(&bytes[..count]);
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
        self.0.append(value.as_bytes());
        Ok(())
    }
}

enum NameSource<'a> {
    Post(post::Post<'a>),
    Cff(Cff<'a>, charset::Charset<'a>),
    Type1(&'a Type1Font),
    Synthesized,
}

fn source(font: &Font) -> NameSource<'_> {
    if let Kind::Type1(type1) = font.kind() {
        return NameSource::Type1(type1);
    }
    if let Ok(post) = font.tables().post() {
        if post.num_names() != 0 {
            return NameSource::Post(post);
        }
    }
    if let Ok(cff) = font.tables().cff() {
        if let Some(charset) = cff.charset(0) {
            return NameSource::Cff(cff, charset);
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
        NameSource::Cff(cff, charset) => charset
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

enum NameIterSource<'a> {
    Post(post::GlyphNames<'a>),
    Cff(Cff<'a>, charset::Iter<'a>),
    Type1(&'a Type1Font),
    Synthesized,
}

struct NameIter<'a> {
    source: NameIterSource<'a>,
    next: u32,
    count: u32,
}

pub(crate) fn glyph_names(font: &Font) -> impl ExactSizeIterator<Item = (GlyphId, GlyphName)> + '_ {
    let source = match source(font) {
        NameSource::Post(post) => NameIterSource::Post(post.glyph_names()),
        NameSource::Cff(cff, charset) => NameIterSource::Cff(cff, charset.iter()),
        NameSource::Type1(type1) => NameIterSource::Type1(type1),
        NameSource::Synthesized => NameIterSource::Synthesized,
    };
    NameIter {
        source,
        next: 0,
        count: font.num_glyphs(),
    }
}

impl Iterator for NameIter<'_> {
    type Item = (GlyphId, GlyphName);

    fn size_hint(&self) -> (usize, Option<usize>) {
        let remaining = (self.count - self.next) as usize;
        (remaining, Some(remaining))
    }

    fn next(&mut self) -> Option<Self::Item> {
        if self.next >= self.count {
            return None;
        }
        let glyph = GlyphId::new(self.next);
        self.next += 1;
        let name = match &mut self.source {
            NameIterSource::Post(names) => names.next().and_then(|(_, value)| {
                GlyphName::from_bytes(value.as_bytes(), GlyphNameSource::Post)
            }),
            NameIterSource::Cff(cff, names) => names.next().and_then(|(id, sid)| {
                (id == glyph)
                    .then(|| cff.string(sid))
                    .flatten()
                    .and_then(|value| GlyphName::from_bytes(value, GlyphNameSource::Cff))
            }),
            NameIterSource::Type1(type1) => type1
                .glyph_name(glyph)
                .and_then(|value| GlyphName::from_bytes(value.as_bytes(), GlyphNameSource::Type1)),
            NameIterSource::Synthesized => None,
        };
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

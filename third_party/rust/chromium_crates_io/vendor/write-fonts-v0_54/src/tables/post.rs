//! The post table

use std::collections::HashMap;

include!("../../generated/generated_post.rs");

//TODO: I imagine we're going to need a builder for this

/// A string in the post table.
#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct PString(String);

impl Post {
    /// Construct a new version 2.0 table from a glyph order.
    pub fn new_v2<'a>(order: impl IntoIterator<Item = &'a str>) -> Self {
        let standard_glyphs = read_fonts::tables::post::DEFAULT_GLYPH_NAMES
            .iter()
            .enumerate()
            .map(|(i, name)| (*name, i as u16))
            .collect::<HashMap<_, _>>();
        const NUM_STANDARD: usize = 258;
        let mut name_index = Vec::new();
        let mut storage = Vec::new();
        let mut visited_names = HashMap::new();

        for name in order {
            match standard_glyphs.get(name) {
                Some(i) => name_index.push(*i),
                None => {
                    let idx = match visited_names.get(name) {
                        Some(i) => *i,
                        None => {
                            let idx = (NUM_STANDARD + storage.len()).try_into().unwrap();
                            visited_names.insert(name, idx);
                            storage.push(PString(name.into()));
                            idx
                        }
                    };
                    name_index.push(idx);
                }
            }
        }

        Post {
            version: Version16Dot16::VERSION_2_0,
            num_glyphs: Some(name_index.len() as u16),
            glyph_name_index: Some(name_index),
            string_data: Some(storage),
            ..Default::default()
        }
    }
}

impl std::ops::Deref for PString {
    type Target = str;
    fn deref(&self) -> &Self::Target {
        self.0.as_ref()
    }
}

impl AsRef<str> for PString {
    fn as_ref(&self) -> &str {
        self.0.as_ref()
    }
}

impl<'a> FromObjRef<read_fonts::tables::post::PString<'a>> for PString {
    fn from_obj_ref(from: &read_fonts::tables::post::PString<'a>, _: FontData) -> Self {
        PString(from.as_str().to_owned())
    }
}

impl FontWrite for PString {
    fn write_into(&self, writer: &mut TableWriter) {
        let len = self.0.len() as u8;
        len.write_into(writer);
        self.0.as_bytes().write_into(writer);
    }
}

impl PartialEq<&str> for PString {
    fn eq(&self, other: &&str) -> bool {
        self.0 == *other
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn roundtrip() {
        use font_test_data::post as test_data;

        let table = Post::read(test_data::SIMPLE.into()).unwrap();
        let dumped = crate::dump_table(&table).unwrap();
        assert_eq!(test_data::SIMPLE, &dumped);
    }

    #[test]
    fn compilev2() {
        let post = Post::new_v2([".dotdef", "A", "B", "one", "flarb", "C"]);
        let dumped = crate::dump_table(&post).unwrap();
        let loaded = read_fonts::tables::post::Post::read(FontData::new(&dumped)).unwrap();
        assert_eq!(loaded.version(), Version16Dot16::VERSION_2_0);
        assert_eq!(loaded.glyph_name(GlyphId16::new(1)), Some("A"));
        assert_eq!(loaded.glyph_name(GlyphId16::new(4)), Some("flarb"));
        assert_eq!(loaded.glyph_name(GlyphId16::new(5)), Some("C"));
    }

    #[test]
    fn compilev2_with_duplicates() {
        let post = Post::new_v2([".dotdef", "A", "flarb", "C", "A", "flarb"]);
        let dumped = crate::dump_table(&post).unwrap();
        let loaded = read_fonts::tables::post::Post::read(FontData::new(&dumped)).unwrap();

        assert_eq!(post.num_glyphs, Some(6));
        assert_eq!(post.glyph_name_index.as_ref().unwrap().len(), 6);
        assert_eq!(post.glyph_name_index.as_ref().unwrap().first(), Some(&258));
        assert_eq!(post.glyph_name_index.as_ref().unwrap().get(1), Some(&36));
        assert_eq!(post.glyph_name_index.as_ref().unwrap().get(2), Some(&259));
        assert_eq!(post.glyph_name_index.as_ref().unwrap().get(3), Some(&38));
        assert_eq!(post.glyph_name_index.as_ref().unwrap().get(4), Some(&36));
        assert_eq!(post.glyph_name_index.as_ref().unwrap().get(5), Some(&259));
        assert_eq!(post.string_data.unwrap().len(), 2);

        assert_eq!(loaded.version(), Version16Dot16::VERSION_2_0);
        assert_eq!(loaded.num_glyphs(), Some(6));
        assert_eq!(loaded.glyph_name(GlyphId16::new(1)), Some("A"));
        assert_eq!(loaded.glyph_name(GlyphId16::new(2)), Some("flarb"));
        assert_eq!(loaded.glyph_name(GlyphId16::new(3)), Some("C"));
        assert_eq!(loaded.glyph_name(GlyphId16::new(4)), Some("A"));
        assert_eq!(loaded.glyph_name(GlyphId16::new(5)), Some("flarb"));
    }
}

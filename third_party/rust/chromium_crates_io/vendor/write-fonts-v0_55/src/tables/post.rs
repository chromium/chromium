//! The post table

use std::collections::HashMap;

include!("../../generated/generated_post.rs");

//TODO: I imagine we're going to need a builder for this

/// The number of standard Macintosh glyph names.
const NUM_STANDARD: usize = 258;

/// The maximum number of custom glyph names in a version 2.0 table.
///
/// Custom names are referenced by `u16` indices starting after the standard
/// names.
const MAX_CUSTOM_NAMES: usize = u16::MAX as usize + 1 - NUM_STANDARD;

/// The maximum length in bytes of a custom glyph name.
///
/// Each name is a Pascal string, prefixed by a `u8` length.
const MAX_NAME_LEN: usize = u8::MAX as usize;

/// A string in the post table.
#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct PString(String);

impl Post {
    /// Construct a new version 2.0 table from a glyph order.
    ///
    /// A glyph order with more than 65278 distinct custom (non-standard) names
    /// cannot be encoded; the resulting table will fail validation.
    pub fn new_v2<'a>(order: impl IntoIterator<Item = &'a str>) -> Self {
        let standard_glyphs = read_fonts::tables::post::DEFAULT_GLYPH_NAMES
            .iter()
            .enumerate()
            .map(|(i, name)| (*name, i as u16))
            .collect::<HashMap<_, _>>();
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
                            // Indices past u16::MAX can't be encoded. Saturate rather
                            // than panic: the name is still stored, so validation sees
                            // the real count in `string_data` and rejects the table.
                            let idx = (NUM_STANDARD + storage.len())
                                .try_into()
                                .unwrap_or(u16::MAX);
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
            // too many glyphs is reported when validating `glyph_name_index`
            num_glyphs: Some(name_index.len().try_into().unwrap_or(u16::MAX)),
            glyph_name_index: Some(name_index),
            string_data: Some(storage),
            ..Default::default()
        }
    }

    fn validate_string_data(&self, ctx: &mut ValidationCtx) {
        let Some(names) = &self.string_data else {
            return;
        };
        if !self.version.compatible(Version16Dot16::VERSION_2_0) {
            return;
        }
        if names.len() > MAX_CUSTOM_NAMES {
            ctx.report(format!(
                "post table version 2.0 can hold at most {MAX_CUSTOM_NAMES} custom glyph names, found {}",
                names.len()
            ));
        }
        ctx.with_array_items(names.iter(), |ctx, name| {
            if name.len() > MAX_NAME_LEN {
                ctx.report(format!(
                    "glyph name '{}' is {} bytes long, but the maximum is {MAX_NAME_LEN}",
                    name.0,
                    name.len()
                ));
            }
        });
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

    #[test]
    fn compilev2_too_many_custom_names() {
        // one more than fits in the u16 indices that follow the 258 standard names
        let names = (0..65_279)
            .map(|i| format!("custom{i}"))
            .collect::<Vec<_>>();
        let post = Post::new_v2(names.iter().map(String::as_str));
        let err = crate::dump_table(&post).unwrap_err().to_string();
        assert!(
            err.contains("can hold at most 65278 custom glyph names, found 65279"),
            "{err}"
        );
    }

    #[test]
    fn compilev2_long_glyph_name() {
        let too_long = "a".repeat(256);
        let post = Post::new_v2([".notdef", too_long.as_str()]);
        let err = crate::dump_table(&post).unwrap_err().to_string();
        assert!(err.contains("is 256 bytes long"), "{err}");

        let max_len = "a".repeat(255);
        let post = Post::new_v2([".notdef", max_len.as_str()]);
        let dumped = crate::dump_table(&post).unwrap();
        let loaded = read_fonts::tables::post::Post::read(FontData::new(&dumped)).unwrap();
        assert_eq!(loaded.glyph_name(GlyphId16::new(1)), Some(max_len.as_str()));
    }

    #[test]
    fn compilev2_max_custom_names() {
        let names = (0..65_278)
            .map(|i| format!("custom{i}"))
            .collect::<Vec<_>>();
        let order = [".notdef", "space", "A"]
            .into_iter()
            .chain(names.iter().map(String::as_str));
        let post = Post::new_v2(order);
        assert_eq!(
            post.glyph_name_index.as_ref().unwrap().last(),
            Some(&u16::MAX)
        );
        let dumped = crate::dump_table(&post).unwrap();
        let loaded = read_fonts::tables::post::Post::read(FontData::new(&dumped)).unwrap();

        assert_eq!(loaded.num_glyphs(), Some(65_281));
        assert_eq!(loaded.glyph_name(GlyphId16::new(2)), Some("A"));
        assert_eq!(loaded.glyph_name(GlyphId16::new(3)), Some("custom0"));
        assert_eq!(
            loaded.glyph_name(GlyphId16::new(65_280)),
            Some("custom65277")
        );
    }
}

//! Incremental Font Transfer [Patch Map](https://w3c.github.io/IFT/Overview.html#font-format-extensions)

include!("../../generated/generated_ift.rs");

use std::str;

pub const IFT_TAG: types::Tag = Tag::new(b"IFT ");
pub const IFTX_TAG: types::Tag = Tag::new(b"IFTX");

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct U8Or16(u16);

impl ReadArgs for U8Or16 {
    type Args = u16;
}

impl ComputeSize for U8Or16 {
    fn compute_size(max_entry_index: &u16) -> Result<usize, ReadError> {
        Ok(if *max_entry_index < 256 { 1 } else { 2 })
    }
}

impl FontReadWithArgs<'_> for U8Or16 {
    fn read_with_args(data: FontData<'_>, max_entry_index: &Self::Args) -> Result<Self, ReadError> {
        if *max_entry_index < 256 {
            data.read_at::<u8>(0).map(|v| Self(v as u16))
        } else {
            data.read_at::<u16>(0).map(Self)
        }
    }
}

impl U8Or16 {
    #[inline]
    pub fn get(self) -> u16 {
        self.0
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct IdDeltaOrLength(i32);

impl ReadArgs for IdDeltaOrLength {
    type Args = Offset32;
}

impl ComputeSize for IdDeltaOrLength {
    fn compute_size(entry_id_string_data_offset: &Offset32) -> Result<usize, ReadError> {
        // This field is either a u16 or an int24 depending on whether or not string data
        // is present. See: <https://w3c.github.io/IFT/Overview.html#mapping-entry-entryiddelta>
        Ok(if entry_id_string_data_offset.is_null() {
            3
        } else {
            2
        })
    }
}

impl FontReadWithArgs<'_> for IdDeltaOrLength {
    fn read_with_args(
        data: FontData<'_>,
        entry_id_string_data_offset: &Self::Args,
    ) -> Result<Self, ReadError> {
        if entry_id_string_data_offset.is_null() {
            data.read_at::<Int24>(0).map(|v| Self(i32::from(v)))
        } else {
            data.read_at::<u16>(0).map(|v| Self(v as i32))
        }
    }
}

impl IdDeltaOrLength {
    #[inline]
    pub fn into_inner(self) -> i32 {
        self.0
    }
}

impl<'a> PatchMapFormat1<'a> {
    pub fn get_compatibility_id(&self) -> [u32; 4] {
        let fixed_array: &[BigEndian<u32>; 4] = self.compatibility_id().try_into().unwrap();
        fixed_array.map(|x| x.get())
    }

    pub fn gid_to_entry_iter(&'a self) -> impl Iterator<Item = (GlyphId, u16)> + 'a {
        GidToEntryIter {
            glyph_map: self.glyph_map().ok(),
            glyph_count: self.glyph_count().to_u32(),
            gid: self
                .glyph_map()
                .map(|glyph_map| glyph_map.first_mapped_glyph() as u32)
                .unwrap_or(0),
        }
        .filter(|(_, entry_index)| *entry_index > 0)
    }

    pub fn entry_count(&self) -> u32 {
        self.max_entry_index() as u32 + 1
    }

    pub fn uri_template_as_string(&self) -> Result<&str, ReadError> {
        str::from_utf8(self.uri_template())
            .map_err(|_| ReadError::MalformedData("Invalid UTF8 encoding for uri template."))
    }

    pub fn is_entry_applied(&self, entry_index: u16) -> bool {
        let byte_index = entry_index / 8;
        let bit_mask = 1 << (entry_index % 8);
        self.applied_entries_bitmap()
            .get(byte_index as usize)
            .map(|byte| byte & bit_mask != 0)
            .unwrap_or(false)
    }
}

impl<'a> PatchMapFormat2<'a> {
    pub fn get_compatibility_id(&self) -> [u32; 4] {
        let fixed_array: &[BigEndian<u32>; 4] = self.compatibility_id().try_into().unwrap();
        fixed_array.map(|x| x.get())
    }

    pub fn uri_template_as_string(&self) -> Result<&str, ReadError> {
        str::from_utf8(self.uri_template())
            .map_err(|_| ReadError::MalformedData("Invalid UTF8 encoding for uri template."))
    }
}

impl<'a> FeatureMap<'a> {
    pub fn entry_records_size(&self, max_entry_index: u16) -> Result<usize, ReadError> {
        let field_width = if max_entry_index < 256 { 1 } else { 2 };
        let mut num_bytes = 0usize;
        for record in self.feature_records().iter() {
            num_bytes += record?.entry_map_count().get() as usize * field_width * 2;
        }
        Ok(num_bytes)
    }
}

struct GidToEntryIter<'a> {
    glyph_map: Option<GlyphMap<'a>>,
    glyph_count: u32,
    gid: u32,
}

impl<'a> Iterator for GidToEntryIter<'a> {
    type Item = (GlyphId, u16);

    fn next(&mut self) -> Option<Self::Item> {
        let glyph_map = self.glyph_map.as_ref()?;

        let cur_gid = self.gid;
        self.gid += 1;

        if cur_gid >= self.glyph_count {
            return None;
        }

        let index = cur_gid as usize - glyph_map.first_mapped_glyph() as usize;
        glyph_map
            .entry_index()
            .get(index)
            .ok()
            .map(|entry_index| (cur_gid.into(), entry_index.0))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use font_test_data::ift as test_data;

    // TODO(garretrieger) - more tests (as functionality is implemented):
    // - Test where entryIndex array has len 0 (eg. all glyphs map to 0)
    // - Test which appliedEntriesBitmap > 1 byte
    // - Test w/ feature map populated.
    // - Test enforced minimum entry count of > 0.
    // - Test where entryIndex is a u16.
    // - Invalid table (too short).
    // - Invalid UTF8 sequence in uri template.
    // - Compat ID is to short.
    // - invalid entry map array (too short)
    // - feature map with short entry indices.

    #[test]
    fn format_1_gid_to_u8_entry_iter() {
        let data = test_data::simple_format1();
        let table = Ift::read(FontData::new(&data)).unwrap();
        let Ift::Format1(map) = table else {
            panic!("Not format 1.");
        };
        let entries: Vec<(GlyphId, u16)> = map.gid_to_entry_iter().collect();

        assert_eq!(
            entries,
            vec![(1u32.into(), 2), (2u32.into(), 1), (4u32.into(), 1)]
        );
    }

    #[test]
    fn format_1_gid_to_u16_entry_iter() {
        let data = test_data::u16_entries_format1();
        let table = Ift::read(FontData::new(&data)).unwrap();
        let Ift::Format1(map) = table else {
            panic!("Not format 1.");
        };
        let entries: Vec<(GlyphId, u16)> = map.gid_to_entry_iter().collect();

        assert_eq!(
            entries,
            vec![
                (2u32.into(), 0x50),
                (3u32.into(), 0x51),
                (4u32.into(), 0x12c),
                (5u32.into(), 0x12c),
                (6u32.into(), 0x50)
            ]
        );
    }

    #[test]
    fn format_1_feature_map() {
        let data = test_data::feature_map_format1();
        let table = Ift::read(FontData::new(&data)).unwrap();
        let Ift::Format1(map) = table else {
            panic!("Not format 1.");
        };

        let Some(feature_map_result) = map.feature_map() else {
            panic!("should have a non null feature map.");
        };

        let Ok(feature_map) = feature_map_result else {
            panic!("should have a valid feature map.");
        };

        assert_eq!(feature_map.feature_records().len(), 3);

        let fr0 = feature_map.feature_records().get(0).unwrap();
        assert_eq!(fr0.feature_tag(), Tag::new(b"dlig"));
        assert_eq!(*fr0.first_new_entry_index(), U8Or16(0x190));
        assert_eq!(*fr0.entry_map_count(), U8Or16(0x01));

        let fr1 = feature_map.feature_records().get(1).unwrap();
        assert_eq!(fr1.feature_tag(), Tag::new(b"liga"));
        assert_eq!(*fr1.first_new_entry_index(), U8Or16(0x180));
        assert_eq!(*fr1.entry_map_count(), U8Or16(0x02));
    }

    #[test]
    fn compatibility_id() {
        let data = test_data::simple_format1();
        let table = Ift::read(FontData::new(&data)).unwrap();
        let Ift::Format1(map) = table else {
            panic!("Not format 1.");
        };

        assert_eq!(map.get_compatibility_id(), [1, 2, 3, 4]);
    }

    #[test]
    fn is_entry_applied() {
        let data = test_data::simple_format1();
        let table = Ift::read(FontData::new(&data)).unwrap();
        let Ift::Format1(map) = table else {
            panic!("Not format 1.");
        };
        assert!(!map.is_entry_applied(0));
        assert!(map.is_entry_applied(1));
        assert!(!map.is_entry_applied(2));
    }

    #[test]
    fn uri_template_as_string() {
        let data = test_data::simple_format1();
        let table = Ift::read(FontData::new(&data)).unwrap();
        let Ift::Format1(map) = table else {
            panic!("Not format 1.");
        };

        assert_eq!(Ok("ABCDEFɤ"), map.uri_template_as_string());
    }
}

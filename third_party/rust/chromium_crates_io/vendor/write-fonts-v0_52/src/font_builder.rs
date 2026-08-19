//!  A builder for top-level font objects

use std::borrow::Cow;
use std::collections::BTreeMap;

use read_fonts::{FontRef, TableProvider};
use types::{Tag, TT_SFNT_VERSION};

#[cfg(feature = "tables")]
use crate::error::BuilderError;
use crate::search_range::SearchRange;

include!("../generated/generated_font.rs");

const TABLE_RECORD_LEN: usize = 16;
const CFF: Tag = Tag::new(b"CFF ");
const CFF2: Tag = Tag::new(b"CFF2");

/// Build a font from some set of tables.
#[derive(Debug, Clone, Default)]
pub struct FontBuilder<'a> {
    tables: BTreeMap<Tag, Cow<'a, [u8]>>,
}

impl TableDirectory {
    pub fn from_table_records(table_records: Vec<TableRecord>) -> TableDirectory {
        assert!(table_records.len() <= u16::MAX as usize);
        // See https://learn.microsoft.com/en-us/typography/opentype/spec/otff#table-directory
        let computed = SearchRange::compute(table_records.len(), TABLE_RECORD_LEN);

        let is_cff = table_records
            .iter()
            .any(|rec| [CFF, CFF2].contains(&rec.tag));
        let sfnt = if is_cff {
            CFF_SFNT_VERSION
        } else {
            TT_SFNT_VERSION
        };

        TableDirectory::new(
            sfnt,
            computed.search_range,
            computed.entry_selector,
            computed.range_shift,
            table_records,
        )
    }
}

// https://learn.microsoft.com/en-us/typography/opentype/spec/recom#optimized-table-ordering
const RECOMMENDED_TABLE_ORDER_TTF: [Tag; 19] = [
    Tag::new(b"head"),
    Tag::new(b"hhea"),
    Tag::new(b"maxp"),
    Tag::new(b"OS/2"),
    Tag::new(b"hmtx"),
    Tag::new(b"LTSH"),
    Tag::new(b"VDMX"),
    Tag::new(b"hdmx"),
    Tag::new(b"cmap"),
    Tag::new(b"fpgm"),
    Tag::new(b"prep"),
    Tag::new(b"cvt "),
    Tag::new(b"loca"),
    Tag::new(b"glyf"),
    Tag::new(b"kern"),
    Tag::new(b"name"),
    Tag::new(b"post"),
    Tag::new(b"gasp"),
    Tag::new(b"PCLT"),
];

const RECOMMENDED_TABLE_ORDER_CFF: [Tag; 8] = [
    Tag::new(b"head"),
    Tag::new(b"hhea"),
    Tag::new(b"maxp"),
    Tag::new(b"OS/2"),
    Tag::new(b"name"),
    Tag::new(b"cmap"),
    Tag::new(b"post"),
    Tag::new(b"CFF "),
];

impl<'a> FontBuilder<'a> {
    /// Create a new builder to compile a binary font
    pub fn new() -> Self {
        Self::default()
    }

    /// Add a table to the builder.
    ///
    /// The table can be any top-level table defined in this crate. This function
    /// will attempt to compile the table and then add it to the builder if
    /// successful, returning an error otherwise.
    #[cfg(feature = "tables")]
    pub fn add_table<T>(&mut self, table: &T) -> Result<&mut Self, BuilderError>
    where
        T: FontWrite + Validate + TopLevelTable,
    {
        let tag = T::TAG;
        let bytes = crate::dump_table(table).map_err(|inner| BuilderError { inner, tag })?;
        Ok(self.add_raw(tag, bytes))
    }

    /// A builder method to add raw data for the provided tag
    pub fn add_raw(&mut self, tag: Tag, data: impl Into<Cow<'a, [u8]>>) -> &mut Self {
        self.tables.insert(tag, data.into());
        self
    }

    /// Copy each table from the source font if it does not already exist
    pub fn copy_missing_tables(&mut self, font: FontRef<'a>) -> &mut Self {
        for record in font.table_directory().table_records() {
            let tag = record.tag();
            if !self.tables.contains_key(&tag) {
                if let Some(data) = font.data_for_tag(tag) {
                    self.add_raw(tag, data);
                } else {
                    log::warn!("data for '{tag}' is malformed");
                }
            }
        }
        self
    }

    /// Returns `true` if the builder contains a table with this tag.
    pub fn contains(&self, tag: Tag) -> bool {
        self.tables.contains_key(&tag)
    }

    /// Returns the builder's table tags in the order recommended by the OpenType spec.
    ///
    /// Table tags not in the recommended order are sorted lexicographically, and 'DSIG'
    /// is always sorted last.
    /// The presence of the 'CFF ' table determines which of the two recommended orders is used.
    /// This matches fontTools' `sortedTagList` function.
    ///
    /// See:
    /// <https://learn.microsoft.com/en-us/typography/opentype/spec/recom#optimized-table-ordering>
    /// <https://github.com/fonttools/fonttools/blob/8d6b2f8f87637fcad8dae498d32eae738cd951bf/Lib/fontTools/ttLib/ttFont.py#L1096-L1117>
    pub fn ordered_tags(&self) -> Vec<Tag> {
        let recommended_order: &[Tag] = if self.contains(Tag::new(b"CFF ")) {
            &RECOMMENDED_TABLE_ORDER_CFF
        } else {
            &RECOMMENDED_TABLE_ORDER_TTF
        };
        // Sort tags into three groups:
        //   Group 0: tags that are in the recommended order, sorted accordingly.
        //   Group 1: tags not in the recommended order, sorted alphabetically.
        //   Group 2: 'DSIG' is always sorted last, matching fontTools' behavior.
        let mut ordered_tags: Vec<Tag> = self.tables.keys().copied().collect();
        let dsig = Tag::new(b"DSIG");
        ordered_tags.sort_unstable_by_key(|rtag| {
            let tag = *rtag;
            if tag == dsig {
                (2, 0, tag)
            } else if let Some(idx) = recommended_order.iter().position(|t| t == rtag) {
                (0, idx, tag)
            } else {
                (1, 0, tag)
            }
        });

        ordered_tags
    }

    /// Assemble all the tables into a binary font file with a [Table Directory].
    ///
    /// [Table Directory]: https://learn.microsoft.com/en-us/typography/opentype/spec/otff#table-directory
    /// [Calculating Checksums]: https://learn.microsoft.com/en-us/typography/opentype/spec/otff#calculating-checksums
    pub fn build(&mut self) -> Vec<u8> {
        // See: https://learn.microsoft.com/en-us/typography/opentype/spec/head
        const HEAD_CHECKSUM_START: usize = 8;
        const HEAD_CHECKSUM_END: usize = 12;

        let header_len = std::mem::size_of::<u32>() // sfnt
            + std::mem::size_of::<u16>() * 4 // num_tables to range_shift
            + self.tables.len() * TABLE_RECORD_LEN;

        // note this is the order of the tables themselves, not the records in the table directory
        // which are sorted by tag so they can be binary searched
        let table_order = self.ordered_tags();

        let mut position = header_len as u32;
        let mut checksums = Vec::new();
        let head_tag = Tag::new(b"head");

        let mut table_records = Vec::new();
        for tag in table_order.iter() {
            // safe to unwrap as ordered_tags() guarantees that all keys exist
            let data = self.tables.get_mut(tag).unwrap();
            let offset = position;
            let length = data.len() as u32;
            position += length;
            if *tag == head_tag && data.len() >= HEAD_CHECKSUM_END {
                // The head table checksum is computed with the checksum field set to 0.
                // Equivalent to Python's `data[:HEAD_CHECKSUM_START] + b"\0\0\0\0" + data[HEAD_CHECKSUM_END:]`
                //
                // Only do this if there is enough data in the head table to write the bytes.
                let head = data.to_mut();
                head[HEAD_CHECKSUM_START..HEAD_CHECKSUM_END].copy_from_slice(&[0, 0, 0, 0]);
            }
            let (checksum, padding) = checksum_and_padding(data);
            checksums.push(checksum);
            position += padding;
            table_records.push(TableRecord::new(*tag, checksum, offset, length));
        }
        table_records.sort_unstable_by_key(|record| record.tag);

        let directory = TableDirectory::from_table_records(table_records);

        let mut writer = TableWriter::default();
        directory.write_into(&mut writer);
        let mut data = writer.into_data().bytes;
        checksums.push(read_fonts::tables::compute_checksum(&data));

        // Summing all the individual table checksums, including the table directory's,
        // gives the checksum for the entire font.
        // The checksum_adjustment is computed as 0xB1B0AFBA - checksum, modulo 2^32.
        // https://learn.microsoft.com/en-us/typography/opentype/spec/otff#calculating-checksums
        let checksum = checksums.into_iter().fold(0u32, u32::wrapping_add);
        let checksum_adjustment = 0xB1B0_AFBAu32.wrapping_sub(checksum);

        for tag in table_order {
            let table = self.tables.remove(&tag).unwrap();
            if tag == head_tag && table.len() >= HEAD_CHECKSUM_END {
                // store the checksum_adjustment in the head table
                data.extend_from_slice(&table[..HEAD_CHECKSUM_START]);
                data.extend_from_slice(&checksum_adjustment.to_be_bytes());
                data.extend_from_slice(&table[HEAD_CHECKSUM_END..]);
            } else {
                data.extend_from_slice(&table);
            }
            let rem = round4(table.len()) - table.len();
            let padding = [0u8; 4];
            data.extend_from_slice(&padding[..rem]);
        }
        data
    }
}

/// <https://github.com/google/woff2/blob/a0d0ed7da27b708c0a4e96ad7a998bddc933c06e/src/round.h#L19>
fn round4(sz: usize) -> usize {
    (sz + 3) & !3
}

fn checksum_and_padding(table: &[u8]) -> (u32, u32) {
    let checksum = read_fonts::tables::compute_checksum(table);
    let padding = round4(table.len()) - table.len();
    (checksum, padding as u32)
}

impl TTCHeader {
    #[allow(dead_code)] // compute_version is required by codegen even though sometimes unused
    fn compute_version(&self) -> MajorMinor {
        panic!("TTCHeader writing not supported (yet)")
    }
}

#[cfg(test)]
mod tests {
    use super::{RECOMMENDED_TABLE_ORDER_CFF, RECOMMENDED_TABLE_ORDER_TTF};
    use font_types::Tag;
    use read_fonts::FontRef;

    use crate::{font_builder::checksum_and_padding, FontBuilder};
    use rand::seq::SliceRandom;
    use rstest::rstest;

    #[test]
    fn sets_binary_search_assists() {
        // Based on Roboto's num tables
        let data = b"doesn't matter".to_vec();
        let mut builder = FontBuilder::default();
        (0..0x16u32).for_each(|i| {
            builder.add_raw(Tag::from_be_bytes(i.to_ne_bytes()), &data);
        });
        let bytes = builder.build();
        let font = FontRef::new(&bytes).unwrap();
        let td = font.table_directory();
        assert_eq!(
            (256, 4, 96),
            (td.search_range(), td.entry_selector(), td.range_shift())
        );
    }

    #[test]
    fn survives_no_tables() {
        FontBuilder::default().build();
    }

    #[test]
    fn pad4() {
        for i in 0..10 {
            let pad = checksum_and_padding(&vec![0; i]).1;
            assert!(pad < 4);
            assert!((i + pad as usize) % 4 == 0, "pad {i} +{pad} bytes");
        }
    }

    #[test]
    fn validate_font_checksum() {
        use rand::RngExt;

        // Add a dummy 'head' plus a couple of made-up tables containing random bytes
        // and verify that the total font checksum is always equal to the special
        // constant 0xB1B0AFBA, which should be the case if the FontBuilder computed
        // the head.checksum_adjustment correctly.
        let head_size = 54;
        let mut rng = rand::rng();
        let mut builder = FontBuilder::default();
        for tag in [Tag::new(b"head"), Tag::new(b"FOO "), Tag::new(b"BAR ")] {
            let data: Vec<u8> = (0..=head_size).map(|_| rng.r#random()).collect();
            builder.add_raw(tag, data);
        }
        let font_data = builder.build();
        assert_eq!(read_fonts::tables::compute_checksum(&font_data), 0xB1B0AFBA);
    }

    #[test]
    fn minimum_head_size_for_checksum_rewrite() {
        let mut builder = FontBuilder::default();
        builder.add_raw(
            Tag::new(b"head"),
            vec![0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11],
        );

        let font_data = builder.build();
        let font = FontRef::new(&font_data).unwrap();
        let head = font.table_data(Tag::new(b"head")).unwrap();

        assert_eq!(
            head.as_bytes(),
            &vec![0, 1, 2, 3, 4, 5, 6, 7, 65, 61, 62, 10]
        );
    }

    #[test]
    fn doesnt_overflow_head() {
        let mut builder = FontBuilder::default();
        builder.add_raw(Tag::new(b"head"), vec![0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);

        let font_data = builder.build();
        let font = FontRef::new(&font_data).unwrap();
        let head = font.table_data(Tag::new(b"head")).unwrap();

        assert_eq!(head.as_bytes(), &vec![0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    }

    #[rstest]
    #[case::ttf(&RECOMMENDED_TABLE_ORDER_TTF)]
    #[case::cff(&RECOMMENDED_TABLE_ORDER_CFF)]
    fn recommended_table_order(#[case] recommended_order: &[Tag]) {
        let dsig = Tag::new(b"DSIG");
        let mut builder = FontBuilder::default();
        builder.add_raw(dsig, vec![0]);
        let mut tags = recommended_order.to_vec();
        tags.shuffle(&mut rand::rng());
        for tag in tags {
            builder.add_raw(tag, vec![0]);
        }
        builder.add_raw(Tag::new(b"ZZZZ"), vec![0]);
        builder.add_raw(Tag::new(b"AAAA"), vec![0]);

        // recommended order first, then sorted additional tags, and last DSIG
        let mut expected = recommended_order.to_vec();
        expected.push(Tag::new(b"AAAA"));
        expected.push(Tag::new(b"ZZZZ"));
        expected.push(dsig);

        assert_eq!(builder.ordered_tags(), expected);
    }
}

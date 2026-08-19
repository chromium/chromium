//! Implementation of Table Keyed patch application.
//!
//! Table Keyed patches are a type of incremental font patch which stores opaque binary diffs
//! keyed by table tag.
//!
//! Table Keyed patches are specified here:
//! <https://w3c.github.io/IFT/Overview.html#table-keyed>
//!
use std::collections::BTreeSet;

use crate::font_patch::PatchingError;
use read_fonts::{
    tables::ift::{TableKeyedPatch, TablePatch, TablePatchFlags},
    types::Tag,
    FontRef, ReadError,
};
use shared_brotli_patch_decoder::SharedBrotliDecoder;
use write_fonts::FontBuilder;

pub(crate) fn apply_table_keyed_patch<D: SharedBrotliDecoder>(
    patch: &TableKeyedPatch<'_>,
    font: &FontRef,
    brotli_decoder: &D,
) -> Result<Vec<u8>, PatchingError> {
    if patch.format() != Tag::new(b"iftk") {
        return Err(PatchingError::InvalidPatch("Patch file tag is not 'iftk'"));
    }

    // brotli stream starts at the (u32 tag + u8 flags + u32 length) = 9th byte
    const STREAM_START: u32 = 9;
    let mut font_builder = FontBuilder::new();
    let mut processed_tables = BTreeSet::<Tag>::new();
    // TODO(garretrieger): enforce a max combined size of all decoded tables? say something in the spec about this?
    for (i, table_patch) in patch
        .patches()
        .iter()
        .take(patch.patches_count() as usize)
        .enumerate()
    {
        let next = i + 1;

        let table_patch = table_patch.map_err(PatchingError::PatchParsingFailed)?;
        let (Some(offset), Some(next_offset)) = (
            patch.patch_offsets().get(i),
            patch.patch_offsets().get(next),
        ) else {
            return Err(PatchingError::InvalidPatch("Missing patch offset."));
        };

        let offset = offset.get().to_u32();
        let next_offset = next_offset.get().to_u32();
        let Some(stream_length) = next_offset
            .checked_sub(offset)
            .and_then(|v| v.checked_sub(STREAM_START))
        else {
            return Err(PatchingError::InvalidPatch(
                "Patch offsets are not in sorted order.",
            ));
        };

        if stream_length as usize > table_patch.brotli_stream().len() {
            return Err(PatchingError::PatchParsingFailed(ReadError::OutOfBounds));
        }

        let tag = table_patch.tag();
        if !processed_tables.insert(tag) {
            // Table has already been processed.
            continue;
        }

        if table_patch.flags().contains(TablePatchFlags::DROP_TABLE) {
            // Table will not be copied, skip any further processing.
            continue;
        }

        let replacement = table_patch.flags().contains(TablePatchFlags::REPLACE_TABLE);
        let new_table = apply_table_patch(
            font,
            table_patch,
            stream_length,
            replacement,
            brotli_decoder,
        )?;
        font_builder.add_raw(tag, new_table);
    }

    copy_unprocessed_tables(font, processed_tables, &mut font_builder);

    Ok(font_builder.build())
}

fn apply_table_patch<D: SharedBrotliDecoder>(
    font: &FontRef,
    table_patch: TablePatch,
    stream_length: u32,
    replacement: bool,
    brotli_decoder: &D,
) -> Result<Vec<u8>, PatchingError> {
    let stream_length = stream_length as usize;
    let base_data = font.table_data(table_patch.tag());
    let stream = if table_patch.brotli_stream().len() >= stream_length {
        &table_patch.brotli_stream()[..stream_length]
    } else {
        return Err(PatchingError::InvalidPatch(
            "Brotli stream is larger then the maxUncompressedLength field.",
        ));
    };
    let r = match (base_data, replacement) {
        (Some(base_data), false) => brotli_decoder.decode(
            stream,
            Some(base_data.as_bytes()),
            table_patch.max_uncompressed_length() as usize,
        ),
        (None, false) => {
            return Err(PatchingError::InvalidPatch(
                "Trying to patch a base table that doesn't exist.",
            ))
        }
        _ => brotli_decoder.decode(stream, None, table_patch.max_uncompressed_length() as usize),
    };

    r.map_err(PatchingError::from)
}

pub(crate) fn copy_unprocessed_tables<'a>(
    font: &FontRef<'a>,
    processed_tables: BTreeSet<Tag>,
    font_builder: &mut FontBuilder<'a>,
) {
    font.table_directory()
        .table_records()
        .iter()
        .map(|r| r.tag())
        .filter(|tag| !processed_tables.contains(tag))
        .filter_map(|tag| Some((tag, font.table_data(tag)?)))
        .for_each(|(tag, data)| {
            font_builder.add_raw(tag, data);
        });
}

#[cfg(test)]
mod tests {
    use super::*;
    use font_test_data::ift::{noop_table_keyed_patch, table_keyed_patch};
    use read_fonts::tables::ift::IFT_TAG;
    use read_fonts::FontData;
    use read_fonts::FontRead;
    use read_fonts::FontRef;
    use read_fonts::ReadError;
    use shared_brotli_patch_decoder::BuiltInBrotliDecoder;
    use write_fonts::FontBuilder;

    const IFT_TABLE: &[u8] = b"IFT PATCH MAP";
    const TABLE_1_FINAL_STATE: &[u8] = "hijkabcdeflmnohijkabcdeflmno\n".as_bytes();
    const TABLE_2_FINAL_STATE: &[u8] = "foobarbaz foobarbaz foobarbaz\n".as_bytes();
    const TABLE_3_FINAL_STATE: &[u8] = "foobaz\n".as_bytes();
    const TABLE_4_FINAL_STATE: &[u8] = "unchanged\n".as_bytes();

    fn test_font() -> Vec<u8> {
        let mut font_builder = FontBuilder::new();
        font_builder.add_raw(IFT_TAG, IFT_TABLE);
        font_builder.add_raw(Tag::new(b"tab1"), "abcdef\n".as_bytes());
        font_builder.add_raw(Tag::new(b"tab2"), "foobar\n".as_bytes());
        font_builder.add_raw(Tag::new(b"tab3"), "foobaz\n".as_bytes());
        font_builder.add_raw(Tag::new(b"tab4"), "unchanged\n".as_bytes());
        font_builder.build()
    }

    #[test]
    fn table_keyed_patch_test() {
        let patch_data = table_keyed_patch();
        let patch = TableKeyedPatch::read(FontData::new(&patch_data)).unwrap();
        let font = test_font();
        let font = FontRef::new(font.as_slice()).unwrap();
        let r = apply_table_keyed_patch(&patch, &font, &BuiltInBrotliDecoder);

        let font = r.unwrap();
        let font = FontRef::new(&font).unwrap();

        assert_eq!(font.table_directory().num_tables(), 4);

        assert_eq!(font.table_data(IFT_TAG).unwrap().as_bytes(), IFT_TABLE);
        assert_eq!(
            font.table_data(Tag::new(b"tab1")).unwrap().as_bytes(),
            TABLE_1_FINAL_STATE
        );
        assert_eq!(
            font.table_data(Tag::new(b"tab2")).unwrap().as_bytes(),
            TABLE_2_FINAL_STATE
        );
        assert_eq!(
            font.table_data(Tag::new(b"tab4")).unwrap().as_bytes(),
            TABLE_4_FINAL_STATE
        );
    }

    #[test]
    fn noop_table_keyed_patch_test() {
        let patch_data = noop_table_keyed_patch();
        let patch = TableKeyedPatch::read(FontData::new(&patch_data)).unwrap();
        let font = test_font();
        let font = FontRef::new(font.as_slice()).unwrap();
        let r = apply_table_keyed_patch(&patch, &font, &BuiltInBrotliDecoder);

        let font = r.unwrap();
        let font = FontRef::new(&font).unwrap();
        let expected_font = test_font();
        let expected_font = FontRef::new(&expected_font).unwrap();

        assert_eq!(
            font.table_directory().num_tables(),
            expected_font.table_directory().num_tables()
        );

        for t in expected_font.table_directory().table_records() {
            let data = font.table_data(t.tag()).unwrap();
            let expected_data = expected_font.table_data(t.tag()).unwrap();
            assert_eq!(data.as_bytes(), expected_data.as_bytes());
        }
    }

    #[test]
    fn table_keyed_patch_bad_top_level_tag() {
        let mut patch_data = table_keyed_patch();
        patch_data.write_at("tag", Tag::new(b"ifgk"));
        let patch = TableKeyedPatch::read(FontData::new(&patch_data)).unwrap();
        let font = test_font();
        let font = FontRef::new(font.as_slice()).unwrap();

        assert_eq!(
            Err(PatchingError::InvalidPatch("Patch file tag is not 'iftk'")),
            apply_table_keyed_patch(&patch, &font, &BuiltInBrotliDecoder),
        );
    }

    #[test]
    fn table_keyed_ignore_duplicates() {
        // add a duplicate entry requesting dropping of tab2,
        // should be ignored.
        let mut patch_data = table_keyed_patch();
        patch_data.write_at("patch[2]", Tag::new(b"tab2"));
        let patch = TableKeyedPatch::read(FontData::new(&patch_data)).unwrap();
        let font = test_font();
        let font = FontRef::new(font.as_slice()).unwrap();
        let r = apply_table_keyed_patch(&patch, &font, &BuiltInBrotliDecoder);

        let font = r.unwrap();
        let font = FontRef::new(&font).unwrap();

        assert_eq!(font.table_directory().num_tables(), 5);

        assert_eq!(font.table_data(IFT_TAG).unwrap().as_bytes(), IFT_TABLE);
        assert_eq!(
            font.table_data(Tag::new(b"tab1")).unwrap().as_bytes(),
            TABLE_1_FINAL_STATE
        );
        assert_eq!(
            font.table_data(Tag::new(b"tab2")).unwrap().as_bytes(),
            TABLE_2_FINAL_STATE
        );
        assert_eq!(
            font.table_data(Tag::new(b"tab3")).unwrap().as_bytes(),
            TABLE_3_FINAL_STATE
        );
        assert_eq!(
            font.table_data(Tag::new(b"tab4")).unwrap().as_bytes(),
            TABLE_4_FINAL_STATE
        );
    }

    #[test]
    fn table_keyed_patch_unsorted_offsets() {
        // reorder the offsets
        let mut patch_data = table_keyed_patch();

        let offset = patch_data.offset_for("patch[1]") as u32;
        patch_data.write_at("patch_off[0]", offset);

        let offset = patch_data.offset_for("patch[0]") as u32;
        patch_data.write_at("patch_off[1]", offset);

        let patch = TableKeyedPatch::read(FontData::new(&patch_data)).unwrap();
        let font = test_font();
        let font = FontRef::new(font.as_slice()).unwrap();

        assert_eq!(
            Err(PatchingError::InvalidPatch(
                "Patch offsets are not in sorted order."
            )),
            apply_table_keyed_patch(&patch, &font, &BuiltInBrotliDecoder)
        );
    }

    #[test]
    fn table_keyed_patch_out_of_bounds_offsets() {
        // set last offset out of bounds
        let mut patch_data = table_keyed_patch();
        let offset = (patch_data.offset_for("end") + 5) as u32;
        patch_data.write_at("patch_off[3]", offset);

        let patch = TableKeyedPatch::read(FontData::new(&patch_data)).unwrap();
        let font = test_font();
        let font = FontRef::new(font.as_slice()).unwrap();

        assert_eq!(
            Err(PatchingError::PatchParsingFailed(ReadError::OutOfBounds)),
            apply_table_keyed_patch(&patch, &font, &BuiltInBrotliDecoder)
        );
    }

    #[test]
    fn table_keyed_patch_drop_and_replace() {
        let mut patch_data = table_keyed_patch();
        patch_data.write_at("flags[2]", 3u8);

        let patch = TableKeyedPatch::read(FontData::new(&patch_data)).unwrap();
        let font = test_font();
        let font = FontRef::new(font.as_slice()).unwrap();

        // When DROP and REPLACE are both set DROP takes priority.
        let r = apply_table_keyed_patch(&patch, &font, &BuiltInBrotliDecoder);

        let font = r.unwrap();
        let font = FontRef::new(&font).unwrap();

        assert_eq!(font.table_directory().num_tables(), 4);

        assert_eq!(font.table_data(IFT_TAG).unwrap().as_bytes(), IFT_TABLE);
        assert_eq!(
            font.table_data(Tag::new(b"tab1")).unwrap().as_bytes(),
            TABLE_1_FINAL_STATE
        );
        assert_eq!(
            font.table_data(Tag::new(b"tab2")).unwrap().as_bytes(),
            TABLE_2_FINAL_STATE
        );
        assert_eq!(
            font.table_data(Tag::new(b"tab4")).unwrap().as_bytes(),
            TABLE_4_FINAL_STATE
        );
    }

    #[test]
    fn table_keyed_patch_missing_table() {
        let mut patch_data = table_keyed_patch();
        patch_data.write_at("flags[1]", 0u8);
        patch_data.write_at("patch[1]", Tag::new(b"tab5"));

        let patch = TableKeyedPatch::read(FontData::new(&patch_data)).unwrap();
        let font = test_font();
        let font = FontRef::new(font.as_slice()).unwrap();

        assert_eq!(
            Err(PatchingError::InvalidPatch(
                "Trying to patch a base table that doesn't exist."
            )),
            apply_table_keyed_patch(&patch, &font, &BuiltInBrotliDecoder)
        );
    }

    #[test]
    fn table_keyed_replace_missing_table() {
        let mut patch_data = table_keyed_patch();
        patch_data.write_at("patch[1]", Tag::new(b"tab5"));

        let patch = TableKeyedPatch::read(FontData::new(&patch_data)).unwrap();
        let font = test_font();
        let font = FontRef::new(font.as_slice()).unwrap();

        let r = apply_table_keyed_patch(&patch, &font, &BuiltInBrotliDecoder);

        let font = r.unwrap();
        let font = FontRef::new(&font).unwrap();

        assert_eq!(font.table_directory().num_tables(), 5);

        assert_eq!(font.table_data(IFT_TAG).unwrap().as_bytes(), IFT_TABLE);
        assert_eq!(
            font.table_data(Tag::new(b"tab1")).unwrap().as_bytes(),
            TABLE_1_FINAL_STATE
        );
        assert_eq!(
            font.table_data(Tag::new(b"tab2")).unwrap().as_bytes(),
            "foobar\n".as_bytes()
        );
        assert_eq!(
            font.table_data(Tag::new(b"tab4")).unwrap().as_bytes(),
            TABLE_4_FINAL_STATE
        );
        assert_eq!(
            font.table_data(Tag::new(b"tab5")).unwrap().as_bytes(),
            TABLE_2_FINAL_STATE
        );
    }

    #[test]
    fn table_keyed_drop_missing_table() {
        let mut patch_data = table_keyed_patch();
        patch_data.write_at("patch[2]", Tag::new(b"tab5"));

        let patch = TableKeyedPatch::read(FontData::new(&patch_data)).unwrap();
        let font = test_font();
        let font = FontRef::new(font.as_slice()).unwrap();

        let r = apply_table_keyed_patch(&patch, &font, &BuiltInBrotliDecoder);

        let font = r.unwrap();
        let font = FontRef::new(&font).unwrap();

        assert_eq!(font.table_directory().num_tables(), 5);

        assert_eq!(font.table_data(IFT_TAG).unwrap().as_bytes(), IFT_TABLE);
        assert_eq!(
            font.table_data(Tag::new(b"tab1")).unwrap().as_bytes(),
            TABLE_1_FINAL_STATE
        );
        assert_eq!(
            font.table_data(Tag::new(b"tab2")).unwrap().as_bytes(),
            TABLE_2_FINAL_STATE,
        );
        assert_eq!(
            font.table_data(Tag::new(b"tab3")).unwrap().as_bytes(),
            TABLE_3_FINAL_STATE
        );
        assert_eq!(
            font.table_data(Tag::new(b"tab4")).unwrap().as_bytes(),
            TABLE_4_FINAL_STATE
        );
    }

    #[test]
    fn table_keyed_patch_uncompressed_len_too_small() {
        let mut patch_data = table_keyed_patch();
        patch_data.write_at("decompressed_len[0]", 28u32);

        let patch = TableKeyedPatch::read(FontData::new(&patch_data)).unwrap();
        let font = test_font();
        let font = FontRef::new(font.as_slice()).unwrap();

        assert_eq!(
            Err(PatchingError::InvalidPatch("Max size exceeded.")),
            apply_table_keyed_patch(&patch, &font, &BuiltInBrotliDecoder)
        );
    }
}

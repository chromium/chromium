//! impl subset() for gvar table
use std::mem::size_of;

use crate::{
    serialize::{SerializeErrorFlags, Serializer},
    Plan, Subset, SubsetError, SubsetFlags,
};

use read_fonts::{
    tables::gvar::{Gvar, GvarExtended, SelectedGvar},
    types::GlyphId,
    types::{Scalar, Tag, Uint24},
    FontRef, TopLevelTable,
};
use write_fonts::FontBuilder;

const FIXED_HEADER_SIZE: u32 = 20;
// reference: subset() for gvar table in harfbuzz
// https://github.com/harfbuzz/harfbuzz/blob/63d09dbefcf7ad9f794ca96445d37b6d8c3c9124/src/hb-ot-var-gvar-table.hh#L411
macro_rules! subset_gvar_table {
    ($table:ident) => {
        impl Subset for $table<'_> {
            fn subset(
                &self,
                plan: &Plan,
                _font: &FontRef,
                s: &mut Serializer,
                _builder: &mut FontBuilder,
            ) -> Result<(), SubsetError> {
                let table: SelectedGvar = self.clone().into();
                let result = subset_gvar(&table, plan, s);
                if result.is_err() {
                    s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
                }
                result
            }
        }
    };
}
subset_gvar_table!(Gvar);
subset_gvar_table!(GvarExtended);

fn subset_gvar(
    gvar: &SelectedGvar<'_>,
    plan: &Plan,
    s: &mut Serializer,
) -> Result<(), SubsetError> {
    let extended = matches!(gvar, SelectedGvar::Extended(_));
    let tag = if extended {
        GvarExtended::TAG
    } else {
        Gvar::TAG
    };
    let header_size = FIXED_HEADER_SIZE + u32::from(extended);
    // table header: from version to sharedTuplesOffset
    s.embed_bytes(gvar.offset_data().as_bytes().get(0..12).unwrap())
        .map_err(|_| SubsetError::SubsetTableError(tag))?;

    // glyphCount
    if extended {
        let count = Uint24::try_from(plan.num_output_glyphs)
            .map_err(|_| SubsetError::SubsetTableError(tag))?;
        s.embed(count)
            .map_err(|_| SubsetError::SubsetTableError(tag))?;
    } else {
        let count = u16::try_from(plan.num_output_glyphs)
            .map_err(|_| SubsetError::SubsetTableError(tag))?;
        s.embed(count)
            .map_err(|_| SubsetError::SubsetTableError(tag))?;
    }
    let num_glyphs = plan.num_output_glyphs as u32;

    let subset_data_size: u64 = plan
        .new_to_old_gid_list
        .iter()
        .filter_map(|x| {
            if x.0 == GlyphId::NOTDEF
                && !plan
                    .subset_flags
                    .contains(SubsetFlags::SUBSET_FLAGS_NOTDEF_OUTLINE)
            {
                return None;
            }
            gvar.data_for_gid(x.1)
                .ok()
                .flatten()
                .map(|data| data.len() as u64 + (data.len() % 2) as u64)
        })
        .sum();

    // According to the spec: If the short format (Offset16) is used for offsets, the value stored is the offset divided by 2.
    // So the maximum subset data size that could use short format should be 2 * 0xFFFFu, which is 0x1FFFE
    let long_offset = if subset_data_size > 0x1FFFE_u64 {
        1_u16
    } else {
        0_u16
    };
    // flags
    s.embed(long_offset)
        .map_err(|_| SubsetError::SubsetTableError(tag))?;

    if long_offset > 0 {
        subset_with_offset_type::<u32>(gvar, plan, num_glyphs, header_size, tag, s)?;
    } else {
        subset_with_offset_type::<u16>(gvar, plan, num_glyphs, header_size, tag, s)?;
    }

    Ok(())
}

fn subset_with_offset_type<OffsetType: GvarOffset>(
    gvar: &SelectedGvar<'_>,
    plan: &Plan,
    num_glyphs: u32,
    header_size: u32,
    tag: Tag,
    s: &mut Serializer,
) -> Result<(), SubsetError> {
    // calculate sharedTuplesOffset
    // shared tuples array follow the GlyphVariationData offsets array at the end of the 'gvar' header.
    let off_size = size_of::<OffsetType>();

    let glyph_var_data_offset_array_size = (num_glyphs + 1) * off_size as u32;
    if gvar.shared_tuple_count() != 0 && gvar.shared_tuples_offset().is_null() {
        return Err(SubsetError::SubsetTableError(tag));
    }
    let shared_tuples_offset =
        if gvar.shared_tuple_count() == 0 || gvar.shared_tuples_offset().is_null() {
            0_u32
        } else {
            header_size + glyph_var_data_offset_array_size
        };

    //update sharedTuplesOffset, which is of Offset32 type and byte position in gvar is 8..12
    s.copy_assign(8, shared_tuples_offset);

    // calculate glyphVariationDataArrayOffset: put the glyphVariationData at last in the table
    let shared_tuples_size =
        2u64 * u64::from(gvar.axis_count()) * u64::from(gvar.shared_tuple_count());
    let glyph_var_data_offset = u32::try_from(
        u64::from(header_size) + u64::from(glyph_var_data_offset_array_size) + shared_tuples_size,
    )
    .map_err(|_| SubsetError::SubsetTableError(tag))?;
    s.embed(glyph_var_data_offset)
        .map_err(|_| SubsetError::SubsetTableError(tag))?;

    //pre-allocate glyphVariationDataOffsets array
    let mut start_idx = s
        .allocate_size(glyph_var_data_offset_array_size as usize, true)
        .map_err(|_| SubsetError::SubsetTableError(tag))?;

    // shared tuples array
    if shared_tuples_offset > 0 {
        let offset = gvar.shared_tuples_offset().to_u32() as usize;
        let end = offset
            .checked_add(shared_tuples_size as usize)
            .ok_or(SubsetError::SubsetTableError(tag))?;
        let shared_tuples_data = gvar
            .offset_data()
            .as_bytes()
            .get(offset..end)
            .ok_or(SubsetError::SubsetTableError(tag))?;
        s.embed_bytes(shared_tuples_data)
            .map_err(|_| SubsetError::SubsetTableError(tag))?;
    }

    // GlyphVariationData table array, also update glyphVariationDataOffsets
    start_idx += off_size;

    let mut glyph_offset = 0_u32;
    let mut last = 0;
    for (new_gid, old_gid) in plan.new_to_old_gid_list.iter().filter(|x| {
        x.0 != GlyphId::NOTDEF
            || plan
                .subset_flags
                .contains(SubsetFlags::SUBSET_FLAGS_NOTDEF_OUTLINE)
    }) {
        let last_gid = last;
        for _ in last_gid..new_gid.to_u32() {
            s.copy_assign(start_idx, OffsetType::stored_value(glyph_offset));
            start_idx += off_size;
            last += 1;
        }

        if let Some(glyph_var_data) = gvar
            .data_for_gid(*old_gid)
            .map_err(|_| SubsetError::SubsetTableError(tag))?
        {
            s.embed_bytes(glyph_var_data.as_bytes())
                .map_err(|_| SubsetError::SubsetTableError(tag))?;

            let len = glyph_var_data.len();
            glyph_offset = glyph_offset
                .checked_add(u32::try_from(len).map_err(|_| SubsetError::SubsetTableError(tag))?)
                .ok_or(SubsetError::SubsetTableError(tag))?;
            // padding when short offset format is used
            if off_size == 2 && len % 2 != 0 {
                s.embed(1_u8)
                    .map_err(|_| SubsetError::SubsetTableError(tag))?;
                glyph_offset = glyph_offset
                    .checked_add(1)
                    .ok_or(SubsetError::SubsetTableError(tag))?;
            }
        };

        s.copy_assign(start_idx, OffsetType::stored_value(glyph_offset));
        start_idx += off_size;

        last += 1;
    }

    for _ in last..plan.num_output_glyphs as u32 {
        s.copy_assign(start_idx, OffsetType::stored_value(glyph_offset));
        start_idx += off_size;
    }

    Ok(())
}

trait GvarOffset: Scalar {
    fn stored_value(val: u32) -> Self;
}

impl GvarOffset for u16 {
    fn stored_value(val: u32) -> u16 {
        (val / 2) as u16
    }
}

impl GvarOffset for u32 {
    fn stored_value(val: u32) -> u32 {
        val
    }
}

#[cfg(test)]
mod extended_tests;

#[cfg(test)]
mod test {
    use super::*;
    use read_fonts::{tables::gvar::GvarFlags, FontData, FontRead};

    /// Builds a synthetic gvar table with one entry per element of `glyph_data`.
    ///
    /// The source table always uses long offsets, and has no shared tuples, so
    /// that `glyph_data[i]` is returned verbatim by `data_for_gid(i)`. The
    /// contents are opaque as far as this module is concerned: subsetting gvar
    /// copies GlyphVariationData byte-for-byte without interpreting it.
    fn make_gvar(glyph_data: &[Vec<u8>]) -> Vec<u8> {
        let glyph_count = glyph_data.len();
        let data_array_offset = FIXED_HEADER_SIZE as usize + 4 * (glyph_count + 1);

        let mut out = Vec::new();
        out.extend_from_slice(&1_u16.to_be_bytes()); // majorVersion
        out.extend_from_slice(&0_u16.to_be_bytes()); // minorVersion
        out.extend_from_slice(&1_u16.to_be_bytes()); // axisCount
        out.extend_from_slice(&0_u16.to_be_bytes()); // sharedTupleCount
        out.extend_from_slice(&0_u32.to_be_bytes()); // sharedTuplesOffset
        out.extend_from_slice(&(glyph_count as u16).to_be_bytes()); // glyphCount
        out.extend_from_slice(&1_u16.to_be_bytes()); // flags: LONG_OFFSETS
        out.extend_from_slice(&(data_array_offset as u32).to_be_bytes());

        // glyphVariationDataOffsets, glyphCount + 1 entries
        let mut offset = 0_u32;
        out.extend_from_slice(&offset.to_be_bytes());
        for data in glyph_data {
            offset += data.len() as u32;
            out.extend_from_slice(&offset.to_be_bytes());
        }

        for data in glyph_data {
            out.extend_from_slice(data);
        }
        out
    }

    /// Builds a plan retaining `old_gids`, in order, as new gids 0..n.
    fn make_plan(old_gids: &[u32]) -> Plan {
        let mut plan = Plan::default();
        // Keep new gid 0 (notdef) so every entry of old_gids is exercised.
        plan.subset_flags |= SubsetFlags::SUBSET_FLAGS_NOTDEF_OUTLINE;
        for (new_gid, old_gid) in old_gids.iter().enumerate() {
            plan.new_to_old_gid_list
                .push((GlyphId::from(new_gid as u32), GlyphId::from(*old_gid)));
        }
        plan.num_output_glyphs = old_gids.len();
        plan
    }

    fn subset_gvar(raw_bytes: &[u8], plan: &Plan) -> Vec<u8> {
        let gvar = Gvar::read(FontData::new(raw_bytes)).unwrap();
        let mut builder = FontBuilder::new();
        // Dummy font: subsetting gvar does not look at it.
        let font = FontRef::new(raw_bytes).unwrap();

        let mut s = Serializer::new(1024 * 1024);
        assert_eq!(s.start_serialize(), Ok(()));
        let ret = gvar.subset(plan, &font, &mut s, &mut builder);
        assert!(ret.is_ok());
        assert!(!s.in_error());
        s.end_serialize();
        s.copy_bytes()
    }

    /// The offset format must be chosen from the size of the *retained* glyphs'
    /// variation data, which means looking each glyph up by its old gid.
    ///
    /// Regression test: this used to size the table by indexing the source gvar
    /// with the *new* gids. Here that reads the empty low glyphs, yielding an
    /// estimate of 0 bytes, so the short format was selected for 140_000 bytes
    /// of data. Short offsets store offset/2 in a u16 and top out at 0x1FFFE
    /// bytes, so every offset past that point silently wrapped around and the
    /// resulting table was corrupt.
    #[test]
    fn test_subset_gvar_long_offsets_when_gids_are_remapped() {
        const BIG_LEN: usize = 14_000;
        const NUM_BIG: usize = 10;
        // 10 empty glyphs followed by 10 glyphs of 14_000 bytes each. Retaining
        // only the latter keeps 140_000 bytes, which overflows short offsets.
        const { assert!(BIG_LEN * NUM_BIG > 0x1FFFE) };

        let mut glyph_data: Vec<Vec<u8>> = vec![Vec::new(); NUM_BIG];
        glyph_data.extend((0..NUM_BIG).map(|i| vec![(i + 1) as u8; BIG_LEN]));
        let raw_bytes = make_gvar(&glyph_data);

        let old_gids: Vec<u32> = (NUM_BIG as u32..2 * NUM_BIG as u32).collect();
        let plan = make_plan(&old_gids);
        let subsetted_data = subset_gvar(&raw_bytes, &plan);

        let gvar = Gvar::read(FontData::new(&raw_bytes)).unwrap();
        let new_gvar = Gvar::read(FontData::new(&subsetted_data)).unwrap();

        assert!(new_gvar.flags().contains(GvarFlags::LONG_OFFSETS));
        assert_eq!(new_gvar.glyph_count(), NUM_BIG as u16);

        for (new_gid, old_gid) in old_gids.iter().enumerate() {
            let got = new_gvar
                .data_for_gid(GlyphId::from(new_gid as u32))
                .unwrap()
                .unwrap();
            let want = gvar.data_for_gid(GlyphId::from(*old_gid)).unwrap().unwrap();
            assert_eq!(got.as_bytes(), want.as_bytes(), "new gid {new_gid}");
        }
    }

    /// The short format is still used when the retained data fits, and odd
    /// length data is padded to keep every offset even.
    #[test]
    fn test_subset_gvar_short_offsets_when_gids_are_remapped() {
        // Glyph i (i > 0) carries i bytes of data, so lengths are both odd and
        // even and each glyph's data is distinguishable from its neighbours'.
        let glyph_data: Vec<Vec<u8>> = (0..6).map(|i| vec![0xA0 + i as u8; i]).collect();
        let raw_bytes = make_gvar(&glyph_data);

        let old_gids = [5_u32, 3, 4];
        let plan = make_plan(&old_gids);
        let subsetted_data = subset_gvar(&raw_bytes, &plan);

        let gvar = Gvar::read(FontData::new(&raw_bytes)).unwrap();
        let new_gvar = Gvar::read(FontData::new(&subsetted_data)).unwrap();

        assert!(!new_gvar.flags().contains(GvarFlags::LONG_OFFSETS));
        assert_eq!(new_gvar.glyph_count(), old_gids.len() as u16);

        for (new_gid, old_gid) in old_gids.iter().enumerate() {
            let got = new_gvar
                .data_for_gid(GlyphId::from(new_gid as u32))
                .unwrap()
                .unwrap();
            let want = gvar.data_for_gid(GlyphId::from(*old_gid)).unwrap().unwrap();
            let want = want.as_bytes();
            // Data is padded to an even length, but is otherwise unchanged.
            assert_eq!(got.len(), want.len() + want.len() % 2, "new gid {new_gid}");
            assert_eq!(&got.as_bytes()[..want.len()], want, "new gid {new_gid}");
        }
    }
}

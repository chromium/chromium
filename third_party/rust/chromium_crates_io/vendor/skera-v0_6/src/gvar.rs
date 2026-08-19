//! impl subset() for gvar table
use std::mem::size_of;

use crate::{serialize::Serializer, Plan, Subset, SubsetError, SubsetFlags};

use write_fonts::{
    read::{tables::gvar::Gvar, types::GlyphId, FontRef, TopLevelTable},
    types::Scalar,
    FontBuilder,
};

const FIXED_HEADER_SIZE: u32 = 20;
// reference: subset() for gvar table in harfbuzz
// https://github.com/harfbuzz/harfbuzz/blob/63d09dbefcf7ad9f794ca96445d37b6d8c3c9124/src/hb-ot-var-gvar-table.hh#L411
impl Subset for Gvar<'_> {
    fn subset(
        &self,
        plan: &Plan,
        _font: &FontRef,
        s: &mut Serializer,
        _builder: &mut FontBuilder,
    ) -> Result<(), SubsetError> {
        //table header: from version to sharedTuplesOffset
        s.embed_bytes(self.offset_data().as_bytes().get(0..12).unwrap())
            .map_err(|_| SubsetError::SubsetTableError(Gvar::TAG))?;

        // glyphCount
        let num_glyphs = plan.num_output_glyphs.min(0xFFFF) as u16;
        s.embed(num_glyphs)
            .map_err(|_| SubsetError::SubsetTableError(Gvar::TAG))?;

        let subset_data_size: usize = plan
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
                self.data_for_gid(x.0)
                    .ok()
                    .flatten()
                    .map(|data| data.len() + data.len() % 2)
            })
            .sum();

        // According to the spec: If the short format (Offset16) is used for offsets, the value stored is the offset divided by 2.
        // So the maximum subset data size that could use short format should be 2 * 0xFFFFu, which is 0x1FFFE
        let long_offset = if subset_data_size > 0x1FFFE_usize {
            1_u16
        } else {
            0_u16
        };
        // flags
        s.embed(long_offset)
            .map_err(|_| SubsetError::SubsetTableError(Gvar::TAG))?;

        if long_offset > 0 {
            subset_with_offset_type::<u32>(self, plan, num_glyphs, s)?;
        } else {
            subset_with_offset_type::<u16>(self, plan, num_glyphs, s)?;
        }

        Ok(())
    }
}

fn subset_with_offset_type<OffsetType: GvarOffset>(
    gvar: &Gvar<'_>,
    plan: &Plan,
    num_glyphs: u16,
    s: &mut Serializer,
) -> Result<(), SubsetError> {
    // calculate sharedTuplesOffset
    // shared tuples array follow the GlyphVariationData offsets array at the end of the 'gvar' header.
    let off_size = size_of::<OffsetType>();

    let glyph_var_data_offset_array_size = (num_glyphs as u32 + 1) * off_size as u32;
    let shared_tuples_offset =
        if gvar.shared_tuple_count() == 0 || gvar.shared_tuples_offset().is_null() {
            0_u32
        } else {
            FIXED_HEADER_SIZE + glyph_var_data_offset_array_size
        };

    //update sharedTuplesOffset, which is of Offset32 type and byte position in gvar is 8..12
    s.copy_assign(
        gvar.shared_tuples_offset_byte_range().start,
        shared_tuples_offset,
    );

    // calculate glyphVariationDataArrayOffset: put the glyphVariationData at last in the table
    let shared_tuples_size = 2 * gvar.axis_count() * gvar.shared_tuple_count();
    let glyph_var_data_offset =
        FIXED_HEADER_SIZE + glyph_var_data_offset_array_size + shared_tuples_size as u32;
    s.embed(glyph_var_data_offset)
        .map_err(|_| SubsetError::SubsetTableError(Gvar::TAG))?;

    //pre-allocate glyphVariationDataOffsets array
    let mut start_idx = s
        .allocate_size(glyph_var_data_offset_array_size as usize, false)
        .map_err(|_| SubsetError::SubsetTableError(Gvar::TAG))?;

    // shared tuples array
    if shared_tuples_offset > 0 {
        let offset = gvar.shared_tuples_offset().to_u32() as usize;
        let shared_tuples_data = gvar
            .offset_data()
            .as_bytes()
            .get(offset..offset + shared_tuples_size as usize)
            .ok_or(SubsetError::SubsetTableError(Gvar::TAG))?;
        s.embed_bytes(shared_tuples_data)
            .map_err(|_| SubsetError::SubsetTableError(Gvar::TAG))?;
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
            .map_err(|_| SubsetError::SubsetTableError(Gvar::TAG))?
        {
            s.embed_bytes(glyph_var_data.as_bytes())
                .map_err(|_| SubsetError::SubsetTableError(Gvar::TAG))?;

            let len = glyph_var_data.len();
            glyph_offset += len as u32;
            // padding when short offset format is used
            if off_size == 2 && len % 2 != 0 {
                s.embed(1_u8)
                    .map_err(|_| SubsetError::SubsetTableError(Gvar::TAG))?;
                glyph_offset += 1;
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

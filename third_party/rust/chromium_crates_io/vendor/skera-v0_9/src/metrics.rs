//! Shared horizontal and vertical metrics subsetting.

use crate::{
    serialize::{SerializeErrorFlags, Serializer},
    Plan, SubsetError,
};
use read_fonts::{
    tables::hmtx::GlyphMetricRecords,
    types::{GlyphId, Tag},
};
use write_fonts::FontBuilder;

pub(crate) fn error(s: &mut Serializer, tag: Tag, flag: SerializeErrorFlags) -> SubsetError {
    s.set_err(flag);
    SubsetError::SubsetTableError(tag)
}

pub(crate) fn subset_metrics(
    records: &GlyphMetricRecords<'_>,
    header: &[u8],
    tags: (Tag, Tag),
    wide: bool,
    plan: &Plan,
    s: &mut Serializer,
    builder: &mut FontBuilder,
) -> Result<(), SubsetError> {
    let (metric_tag, header_tag) = tags;
    if plan.num_output_glyphs == 0 {
        return Err(error(
            s,
            metric_tag,
            SerializeErrorFlags::SERIALIZE_ERROR_OTHER,
        ));
    }
    // Validate source glyphs, not the compact output IDs. In particular a
    // retained high glyph must have a readable side bearing in the source.
    for (_, old_gid) in &plan.new_to_old_gid_list {
        if records.advance(*old_gid).is_none() || records.side_bearing(*old_gid).is_none() {
            return Err(error(
                s,
                metric_tag,
                SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR,
            ));
        }
    }
    let advance = |new_gid: usize| {
        plan.reverse_glyph_map
            .get(&GlyphId::new(new_gid as u32))
            .and_then(|old_gid| records.advance(*old_gid))
            .unwrap_or(0)
    };
    let mut num_long = plan.num_output_glyphs;
    let last_advance = advance(num_long - 1);
    while num_long > 1 && advance(num_long - 2) == last_advance {
        num_long -= 1;
    }
    let count = if wide {
        u32::try_from(num_long).map(|n| n.to_be_bytes().to_vec())
    } else {
        u16::try_from(num_long).map(|n| n.to_be_bytes().to_vec())
    }
    .map_err(|_| {
        error(
            s,
            metric_tag,
            SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW,
        )
    })?;
    let mut header = header.to_vec();
    header
        .get_mut(34..34 + count.len())
        .ok_or_else(|| {
            error(
                s,
                header_tag,
                SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR,
            )
        })?
        .copy_from_slice(&count);

    let size = plan
        .num_output_glyphs
        .checked_add(num_long)
        .and_then(|n| n.checked_mul(2))
        .ok_or_else(|| {
            error(
                s,
                metric_tag,
                SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW,
            )
        })?;
    let start = s
        .allocate_size(size, true)
        .map_err(|_| SubsetError::SubsetTableError(metric_tag))?;
    for (new_gid, old_gid) in &plan.new_to_old_gid_list {
        let new_gid = new_gid.to_u32() as usize;
        if new_gid >= plan.num_output_glyphs {
            return Err(error(
                s,
                metric_tag,
                SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW,
            ));
        }
        let side_bearing = records.side_bearing(*old_gid).unwrap();
        if new_gid < num_long {
            let pos = start + new_gid * 4;
            s.copy_assign(pos, records.advance(*old_gid).unwrap());
            s.copy_assign(pos + 2, side_bearing);
        } else {
            let pos = start + num_long * 4 + (new_gid - num_long) * 2;
            s.copy_assign(pos, side_bearing);
        }
    }
    builder.add_raw(header_tag, header);
    Ok(())
}

macro_rules! subset_metrics_table {
    ($table:ident, $header:ident, $read_header:ident, $wide:expr) => {
        impl crate::Subset for $table<'_> {
            fn subset(
                &self,
                plan: &crate::Plan,
                font: &read_fonts::FontRef,
                s: &mut crate::serialize::Serializer,
                builder: &mut write_fonts::FontBuilder,
            ) -> Result<(), crate::SubsetError> {
                // The records of this table, not the one the font would
                // select, which may have been dropped.
                let records = read_fonts::tables::hmtx::GlyphMetricRecords::from(self.clone());
                let header = font.$read_header().map_err(|_| {
                    crate::metrics::error(
                        s,
                        $header::TAG,
                        crate::serialize::SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR,
                    )
                })?;
                crate::metrics::subset_metrics(
                    &records,
                    header.offset_data().as_bytes(),
                    ($table::TAG, $header::TAG),
                    $wide,
                    plan,
                    s,
                    builder,
                )
            }
        }
    };
}
pub(crate) use subset_metrics_table;

#[cfg(test)]
mod tests;

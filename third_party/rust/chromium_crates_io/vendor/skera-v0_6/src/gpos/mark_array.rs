//! impl subset() for MarkRecord subtable
use crate::fnv::FnvHashMap;
use crate::{
    offset::SerializeSubset,
    serialize::{SerializeErrorFlags, Serializer},
    CollectVariationIndices, Plan, SubsetTable,
};
use write_fonts::{
    read::{
        collections::IntSet,
        tables::{
            gpos::{MarkArray, MarkRecord},
            layout::CoverageTable,
        },
        types::GlyphId,
        FontData,
    },
    types::Offset16,
};

pub(crate) fn collect_mark_record_varidx(
    mark_record: &MarkRecord,
    plan: &Plan,
    varidx_set: &mut IntSet<u32>,
    font_data: FontData,
) {
    if let Ok(mark_anchor) = mark_record.mark_anchor(font_data) {
        mark_anchor.collect_variation_indices(plan, varidx_set);
    };
}

pub(crate) fn get_mark_class_map(
    coverage: &CoverageTable,
    mark_array: &MarkArray,
    glyph_set: &IntSet<GlyphId>,
) -> FnvHashMap<u16, u16> {
    let mark_records = mark_array.mark_records();

    let count = match coverage {
        CoverageTable::Format1(t) => t.glyph_count(),
        CoverageTable::Format2(t) => t.range_count(),
    };
    let num_bits = 32 - count.leading_zeros();
    let coverage_population = coverage.population();

    let retained_classes: IntSet<u16> =
        if coverage_population as u32 > (glyph_set.len() as u32) * num_bits {
            glyph_set
                .iter()
                .filter_map(|g| {
                    coverage.get(g).and_then(|idx| {
                        mark_records
                            .get(idx as usize)
                            .map(|mark_record| mark_record.mark_class())
                    })
                })
                .collect()
        } else {
            coverage
                .iter()
                .enumerate()
                .filter(|&(_, g)| glyph_set.contains(GlyphId::from(g)))
                .filter_map(|(idx, _)| {
                    mark_records
                        .get(idx)
                        .map(|mark_record| mark_record.mark_class())
                })
                .collect()
        };
    retained_classes
        .iter()
        .enumerate()
        .map(|(new_class, class)| (class, new_class as u16))
        .collect()
}

impl<'a> SubsetTable<'a> for MarkArray<'_> {
    type ArgsForSubset = (&'a IntSet<u16>, &'a FnvHashMap<u16, u16>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let (mark_record_idxes, mark_class_map) = args;
        if mark_record_idxes.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        // mark count
        s.embed(mark_record_idxes.len() as u16)?;

        let font_data = self.offset_data();
        let mark_records = self.mark_records();
        for i in mark_record_idxes.iter() {
            let Some(mark_record) = mark_records.get(i as usize) else {
                return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER));
            };
            mark_record.subset(plan, s, (mark_class_map, font_data))?;
        }
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for MarkRecord {
    type ArgsForSubset = (&'a FnvHashMap<u16, u16>, FontData<'a>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let (class_map, font_data) = args;
        let Some(new_mark_class) = class_map.get(&self.mark_class()) else {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER));
        };

        s.embed(*new_mark_class)?;

        let anchor_offset_pos = s.embed(0_u16)?;
        if self.mark_anchor_offset().is_null() {
            return Ok(());
        }
        let mark_anchor = self
            .mark_anchor(font_data)
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        Offset16::serialize_subset(&mark_anchor, s, plan, (), anchor_offset_pos)
    }
}

//! impl subset() for MarkRecord subtable
use crate::FastHashMap;
use crate::{
    layout::for_each_intersected_coverage_index,
    offset::SerializeSubset,
    serialize::{SerializeErrorFlags, SerializeResultEmpty, Serializer},
    CollectVariationIndices, Plan, SubsetTable,
};
use read_fonts::{
    collections::IntSet,
    tables::{
        gpos::{AnchorTable, MarkArray, MarkArray2, MarkRecord, MarkRecord2},
        layout::CoverageTable,
    },
    types::GlyphId,
    types::{FixedSize, Offset16, Offset24, Uint24},
    FontData, MinByteRange, ReadError,
};

pub(crate) trait MarkRecordData {
    fn mark_class(&self) -> u16;
    fn mark_anchor<'a>(&self, data: FontData<'a>) -> Result<AnchorTable<'a>, ReadError>;
}

macro_rules! mark_record_data {
    ($record:ident) => {
        impl MarkRecordData for $record {
            fn mark_class(&self) -> u16 {
                $record::mark_class(self)
            }
            fn mark_anchor<'a>(&self, data: FontData<'a>) -> Result<AnchorTable<'a>, ReadError> {
                $record::mark_anchor(self, data)
            }
        }
    };
}
mark_record_data!(MarkRecord);
mark_record_data!(MarkRecord2);

#[inline]
pub(crate) fn collect_mark_record_varidx(
    mark_record: &impl MarkRecordData,
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
    mark_records: &[impl MarkRecordData],
    glyph_set: &IntSet<GlyphId>,
) -> FastHashMap<u16, u16> {
    let mut retained_classes = IntSet::<u16>::empty();
    let _ = for_each_intersected_coverage_index::<()>(
        coverage,
        glyph_set,
        mark_records.len() as u32,
        |index| {
            if let Some(record) = mark_records.get(index as usize) {
                retained_classes.insert(record.mark_class());
            }
            Ok(())
        },
    );
    retained_classes
        .iter()
        .enumerate()
        .map(|(new_class, class)| (class, new_class as u16))
        .collect()
}

pub(crate) fn collect_retained_mark_varidx_and_classes(
    coverage: &CoverageTable,
    mark_records: &[impl MarkRecordData],
    mark_array_data: FontData,
    plan: &Plan,
    varidx_set: &mut IntSet<u32>,
) -> IntSet<u16> {
    let mut retained_classes = IntSet::empty();
    let _ = for_each_intersected_coverage_index::<()>(
        coverage,
        &plan.glyphset_gsub,
        mark_records.len() as u32,
        |index| {
            let record = mark_records.get(index as usize).ok_or(())?;
            collect_mark_record_varidx(record, plan, varidx_set, mark_array_data);
            retained_classes.insert(record.mark_class());
            Ok(())
        },
    );
    retained_classes
}

macro_rules! subset_mark_array {
    ($table:ident, $count:ident) => {
        impl<'a> SubsetTable<'a> for $table<'_> {
            /// `mark_record_idxes` are the coverage indices of the retained
            /// marks, in the same order as the glyphs written to the mark
            /// Coverage table.
            type ArgsForSubset = (&'a [u32], &'a FastHashMap<u16, u16>);
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
                if self.min_table_bytes().is_empty() {
                    return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
                }
                let mark_count = $count::try_from(mark_record_idxes.len())
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
                s.embed(mark_count)?;

                let font_data = self.offset_data();
                let mark_records = self.mark_records();
                // Preserve records with null anchors while dropping an array
                // if none of its retained records has an anchor.
                let mut has_anchor = false;
                for &i in mark_record_idxes {
                    let Some(mark_record) = mark_records.get(i as usize) else {
                        return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER));
                    };
                    has_anchor |= !mark_record
                        .subset(plan, s, (mark_class_map, font_data))
                        .is_empty()?;
                }
                if !has_anchor {
                    return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
                }
                Ok(())
            }
        }
    };
}
subset_mark_array!(MarkArray, u16);
subset_mark_array!(MarkArray2, Uint24);

macro_rules! subset_mark_record {
    ($record:ident, $offset:ident) => {
        impl<'a> SubsetTable<'a> for $record {
            type ArgsForSubset = (&'a FastHashMap<u16, u16>, FontData<'a>);
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

                let anchor_offset_pos = s.allocate_size($offset::RAW_BYTE_LEN, true)?;
                if self.mark_anchor_offset().is_null() {
                    return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
                }
                let mark_anchor = self
                    .mark_anchor(font_data)
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

                $offset::serialize_subset(&mark_anchor, s, plan, (), anchor_offset_pos)
            }
        }
    };
}
subset_mark_record!(MarkRecord, Offset16);
subset_mark_record!(MarkRecord2, Offset24);

#[cfg(test)]
mod test {
    use super::*;
    use read_fonts::FontRead;

    // MarkArray with two MarkRecords (class 0 and class 1). Record 0 has a
    // null anchor; record 1 points at `rec1_anchor_offset` (0 = null) where an
    // AnchorFormat1 (x=100, y=200) is stored.
    fn mark_array_bytes(rec1_anchor_offset: u8) -> [u8; 16] {
        #[rustfmt::skip]
        let raw: [u8; 16] = [
            // markCount=2
            0x00, 0x02,
            // record 0: class 0, null anchor
            0x00, 0x00, 0x00, 0x00,
            // record 1: class 1, anchor offset
            0x00, 0x01, 0x00, rec1_anchor_offset,
            // @10 AnchorFormat1 x=100 y=200
            0x00, 0x01, 0x00, 0x64, 0x00, 0xc8,
        ];
        raw
    }

    fn subset_mark_array(raw: &[u8]) -> (Result<(), SerializeErrorFlags>, Serializer) {
        let mark_array = MarkArray::read(FontData::new(raw)).unwrap();
        let plan = Plan::default();
        let mark_record_idxes = [0_u32, 1];
        let mark_class_map: FastHashMap<u16, u16> = [(0, 0), (1, 1)].into_iter().collect();

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        let ret = mark_array.subset(&plan, &mut s, (&mark_record_idxes[..], &mark_class_map));
        (ret, s)
    }

    /// As in harfbuzz, a MarkArray whose retained records all have null
    /// anchors is EMPTY, so the Mark*Pos subtable that owns it is dropped.
    #[test]
    fn test_subset_mark_array_all_null_anchors_is_empty() {
        let (ret, s) = subset_mark_array(&mark_array_bytes(0));
        assert_eq!(ret, Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY));
        assert!(!s.in_error());
    }

    /// One retained anchor is enough to keep the MarkArray. The record with a
    /// null anchor is still written, with its offset left as 0.
    #[test]
    fn test_subset_mark_array_keeps_null_anchor_records() {
        let (ret, mut s) = subset_mark_array(&mark_array_bytes(10));
        assert_eq!(ret, Ok(()));
        assert!(!s.in_error());
        s.end_serialize();

        #[rustfmt::skip]
        let expected: [u8; 16] = [
            0x00, 0x02,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x01, 0x00, 0x0a,
            0x00, 0x01, 0x00, 0x64, 0x00, 0xc8,
        ];
        assert_eq!(s.copy_bytes(), expected);
    }
}

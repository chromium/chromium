//! The [MVAR](https://learn.microsoft.com/en-us/typography/opentype/spec/mvar) table

include!("../../generated/generated_mvar.rs");

use super::variations::ItemVariationStore;
use std::mem::size_of;

impl Mvar {
    /// Construct a new `MVAR` table.
    pub fn new(
        version: MajorMinor,
        item_variation_store: Option<ItemVariationStore>,
        value_records: Vec<ValueRecord>,
    ) -> Self {
        Self {
            version,
            value_record_size: size_of::<ValueRecord>() as u16,
            value_record_count: value_records.len() as u16,
            item_variation_store: item_variation_store.into(),
            value_records,
        }
    }
}

#[cfg(test)]
mod tests {
    use font_types::{F2Dot14, Tag};
    use read_fonts::tables::mvar as read_mvar;

    use crate::dump_table;
    use crate::tables::variations::{
        ivs_builder::VariationStoreBuilder, RegionAxisCoordinates, VariationRegion,
    };

    use super::*;

    #[test]
    fn empty_smoke_test() {
        let table = Mvar::new(MajorMinor::new(1, 0), None, vec![]);

        let bytes = dump_table(&table).unwrap();
        let read = read_mvar::Mvar::read(FontData::new(&bytes)).unwrap();

        assert_eq!(read.version(), table.version);
        assert_eq!(read.value_record_count(), 0);
        assert_eq!(read.value_record_size(), 8);
        assert!(read.item_variation_store().is_none());
        assert_eq!(read.value_records().len(), 0);
    }

    fn reg_coords(min: f32, default: f32, max: f32) -> RegionAxisCoordinates {
        RegionAxisCoordinates {
            start_coord: F2Dot14::from_f32(min),
            peak_coord: F2Dot14::from_f32(default),
            end_coord: F2Dot14::from_f32(max),
        }
    }

    fn test_regions() -> [VariationRegion; 3] {
        [
            VariationRegion::new(vec![reg_coords(0.0, 1.0, 1.0)]),
            VariationRegion::new(vec![reg_coords(0.0, 0.5, 1.0)]),
            VariationRegion::new(vec![reg_coords(0.5, 1.0, 1.0)]),
        ]
    }

    fn read_metric_delta(mvar: &read_mvar::Mvar, tag: &[u8; 4], coords: &[f32]) -> f64 {
        let coords = coords
            .iter()
            .map(|c| F2Dot14::from_f32(*c))
            .collect::<Vec<_>>();
        mvar.metric_delta(Tag::new(tag), &coords).unwrap().to_f64()
    }

    fn assert_value_record(actual: &read_mvar::ValueRecord, expected: ValueRecord) {
        assert_eq!(actual.value_tag(), expected.value_tag);
        assert_eq!(
            actual.delta_set_outer_index(),
            expected.delta_set_outer_index
        );
        assert_eq!(
            actual.delta_set_inner_index(),
            expected.delta_set_inner_index
        );
    }

    #[test]
    fn simple_smoke_test() {
        let [r1, r2, r3] = test_regions();
        let mut builder = VariationStoreBuilder::new(1);
        let delta_ids = vec![
            // deltas for horizontal ascender 'hasc' only defined for 1 region
            builder.add_deltas(vec![(r1, 10)]),
            // deltas for horizontal descender 'hdsc' defined for 2 regions
            builder.add_deltas(vec![(r2, -20), (r3, -30)]),
        ];
        let (varstore, index_map) = builder.build();

        let mut value_records = Vec::new();
        for (tag, temp_id) in [b"hasc", b"hdsc"].into_iter().zip(delta_ids.into_iter()) {
            let varidx = index_map.get(temp_id).unwrap();
            let value_record = ValueRecord::new(
                Tag::new(tag),
                varidx.delta_set_outer_index,
                varidx.delta_set_inner_index,
            );
            value_records.push(value_record);
        }

        let table = Mvar::new(MajorMinor::new(1, 0), Some(varstore), value_records);

        let bytes = dump_table(&table).unwrap();
        let read = read_mvar::Mvar::read(FontData::new(&bytes)).unwrap();

        assert_eq!(read.version(), table.version);
        assert_eq!(read.value_record_count(), 2);
        assert_eq!(read.value_record_size(), 8);
        assert!(read.item_variation_store().is_some());

        assert_value_record(
            &read.value_records()[0],
            ValueRecord::new(Tag::new(b"hasc"), 0, 1),
        );
        assert_eq!(read_metric_delta(&read, b"hasc", &[0.0]), 0.0);
        // at axis coord 0.5, the interpolated delta will be half of r1's delta
        assert_eq!(read_metric_delta(&read, b"hasc", &[0.5]), 5.0);
        assert_eq!(read_metric_delta(&read, b"hasc", &[1.0]), 10.0);

        assert_value_record(
            &read.value_records()[1],
            ValueRecord::new(Tag::new(b"hdsc"), 0, 0),
        );
        assert_eq!(read_metric_delta(&read, b"hdsc", &[0.0]), 0.0);
        // this coincides with the peak of intermediate region r2, hence != 30.0/2
        assert_eq!(read_metric_delta(&read, b"hdsc", &[0.5]), -20.0);
        assert_eq!(read_metric_delta(&read, b"hdsc", &[1.0]), -30.0);
    }
}

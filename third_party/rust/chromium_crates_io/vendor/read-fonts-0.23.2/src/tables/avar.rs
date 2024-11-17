//! The [Axis Variations](https://docs.microsoft.com/en-us/typography/opentype/spec/avar) table

use super::variations::{DeltaSetIndexMap, ItemVariationStore};

include!("../../generated/generated_avar.rs");

impl<'a> SegmentMaps<'a> {
    /// Applies the piecewise linear mapping to the specified coordinate.
    pub fn apply(&self, coord: Fixed) -> Fixed {
        let mut prev = AxisValueMap {
            from_coordinate: Default::default(),
            to_coordinate: Default::default(),
        };
        for (i, axis_value_map) in self.axis_value_maps().iter().enumerate() {
            use core::cmp::Ordering::*;
            let from = axis_value_map.from_coordinate().to_fixed();
            match from.cmp(&coord) {
                Equal => return axis_value_map.to_coordinate().to_fixed(),
                Greater => {
                    if i == 0 {
                        return coord;
                    }
                    let to = axis_value_map.to_coordinate().to_fixed();
                    let prev_from = prev.from_coordinate().to_fixed();
                    let prev_to = prev.to_coordinate().to_fixed();
                    return prev_to + (to - prev_to).mul_div(coord - prev_from, from - prev_from);
                }
                _ => {}
            }
            prev = *axis_value_map;
        }
        coord
    }
}

impl<'a> VarSize for SegmentMaps<'a> {
    type Size = u16;

    fn read_len_at(data: FontData, pos: usize) -> Option<usize> {
        Some(
            data.read_at::<u16>(pos).ok()? as usize * AxisValueMap::RAW_BYTE_LEN
                + u16::RAW_BYTE_LEN,
        )
    }
}

impl<'a> FontRead<'a> for SegmentMaps<'a> {
    fn read(data: FontData<'a>) -> Result<Self, ReadError> {
        let mut cursor = data.cursor();
        let position_map_count: BigEndian<u16> = cursor.read_be()?;
        let axis_value_maps = cursor.read_array(position_map_count.get() as _)?;
        Ok(SegmentMaps {
            position_map_count,
            axis_value_maps,
        })
    }
}

#[cfg(test)]
mod tests {

    use super::*;
    use crate::{test_helpers, FontRef, TableProvider};

    fn value_map(from: f32, to: f32) -> [F2Dot14; 2] {
        [F2Dot14::from_f32(from), F2Dot14::from_f32(to)]
    }

    // for the purpose of testing it is easier for us to use an array
    // instead of a concrete type, since we can write that into BeBuffer
    impl PartialEq<[F2Dot14; 2]> for AxisValueMap {
        fn eq(&self, other: &[F2Dot14; 2]) -> bool {
            self.from_coordinate == other[0] && self.to_coordinate == other[1]
        }
    }

    #[test]
    fn segment_maps() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let avar = font.avar().unwrap();
        assert_eq!(avar.axis_count(), 1);
        let expected_segment_maps = &[vec![
            value_map(-1.0, -1.0),
            value_map(-0.6667, -0.5),
            value_map(-0.3333, -0.25),
            value_map(0.0, 0.0),
            value_map(0.2, 0.3674),
            value_map(0.4, 0.52246),
            value_map(0.6, 0.67755),
            value_map(0.8, 0.83875),
            value_map(1.0, 1.0),
        ]];
        let segment_maps = avar
            .axis_segment_maps()
            .iter()
            .map(|segment_map| segment_map.unwrap().axis_value_maps().to_owned())
            .collect::<Vec<_>>();
        assert_eq!(segment_maps, expected_segment_maps);
    }

    #[test]
    fn segment_maps_multi_axis() {
        use test_helpers::BeBuffer;

        let segment_one_maps = [
            value_map(-1.0, -1.0),
            value_map(-0.6667, -0.5),
            value_map(-0.3333, -0.25),
        ];
        let segment_two_maps = [value_map(0.8, 0.83875), value_map(1.0, 1.0)];

        let data = BeBuffer::new()
            .push(MajorMinor::VERSION_1_0)
            .push(0u16) // reserved
            .push(2u16) // axis count
            // segment map one
            .push(3u16) // position count
            .extend(segment_one_maps[0])
            .extend(segment_one_maps[1])
            .extend(segment_one_maps[2])
            // segment map two
            .push(2u16) // position count
            .extend(segment_two_maps[0])
            .extend(segment_two_maps[1]);

        let avar = super::Avar::read(data.font_data()).unwrap();
        assert_eq!(avar.axis_segment_maps().iter().count(), 2);
        assert_eq!(
            avar.axis_segment_maps()
                .get(0)
                .unwrap()
                .unwrap()
                .axis_value_maps,
            segment_one_maps,
        );
        assert_eq!(
            avar.axis_segment_maps()
                .get(1)
                .unwrap()
                .unwrap()
                .axis_value_maps,
            segment_two_maps,
        );
    }

    #[test]
    fn piecewise_linear() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let avar = font.avar().unwrap();
        let segment_map = avar.axis_segment_maps().get(0).unwrap().unwrap();
        let coords = [-1.0, -0.5, 0.0, 0.5, 1.0];
        let expected_result = [-1.0, -0.375, 0.0, 0.600006103515625, 1.0];
        assert_eq!(
            &expected_result[..],
            &coords
                .iter()
                .map(|coord| segment_map.apply(Fixed::from_f64(*coord)).to_f64())
                .collect::<Vec<_>>()
        );
    }

    #[test]
    fn avar2() {
        let font = FontRef::new(font_test_data::AVAR2_CHECKER).unwrap();
        let avar = font.avar().unwrap();
        assert_eq!(avar.version(), MajorMinor::VERSION_2_0);
        assert!(avar.axis_index_map_offset().is_some());
        assert!(avar.var_store_offset().is_some());
        assert!(avar.var_store().is_some());
    }
}

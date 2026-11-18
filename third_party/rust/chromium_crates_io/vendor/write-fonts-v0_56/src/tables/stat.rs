//! The STAT table

include!("../../generated/generated_stat.rs");

impl Stat {
    /// Create a new STAT 1.2 table
    pub fn new(
        design_axes: Vec<AxisRecord>,
        axis_values: Vec<AxisValue>,
        elided_fallback_name_id: NameId,
    ) -> Self {
        Stat {
            design_axes: design_axes.into(),
            offset_to_axis_values: NullableOffsetMarker::new(
                (!axis_values.is_empty())
                    .then(|| axis_values.into_iter().map(Into::into).collect()),
            ),
            elided_fallback_name_id: Some(elided_fallback_name_id),
        }
    }
}

// we use a custom conversion here because we use a shim table in read-fonts
// (required because it is an offset to an array of offsets, which is too recursive for us)
// but in write-fonts we want to skip the shim table and just use a vec.
#[allow(clippy::unwrap_or_default)] // we need to be explicit to provide type info
fn convert_axis_value_offsets(
    from: Option<Result<read_fonts::tables::stat::AxisValueArray, ReadError>>,
) -> NullableOffsetMarker<Vec<OffsetMarker<AxisValue>>, WIDTH_32> {
    from.map(|inner| {
        inner
            .ok()
            .map(|array| array.axis_values().to_owned_obj(array.offset_data()))
            .unwrap_or_else(Vec::new)
    })
    .into()
}

#[cfg(test)]
mod tests {
    use crate::dump_table;
    use read_fonts::tables::stat as read_stat;

    use super::*;

    #[test]
    fn smoke_test() {
        let table = Stat::new(
            vec![AxisRecord::new(Tag::new(b"wght"), NameId::new(257), 1)],
            vec![
                AxisValue::format_1(
                    0,
                    AxisValueTableFlags::empty(),
                    NameId::new(258),
                    Fixed::from_f64(100.),
                ),
                AxisValue::format_1(
                    0,
                    AxisValueTableFlags::empty(),
                    NameId::new(261),
                    Fixed::from_f64(400.),
                ),
            ],
            NameId::new(0),
        );

        let bytes = dump_table(&table).unwrap();
        let read = read_stat::Stat::read(FontData::new(&bytes)).unwrap();

        assert_eq!(read.design_axes().unwrap().len(), 1);
        assert_eq!(read.axis_value_count(), 2);
        let axis_values = read.offset_to_axis_values().unwrap().unwrap();
        assert_eq!(axis_values.axis_value_offsets().len(), 2);
        let value2 = axis_values.axis_values().get(1).unwrap();
        let read_stat::AxisValue::Format1(value2) = value2 else {
            panic!("wrong format");
        };
        assert_eq!(value2.value_name_id(), NameId::new(261));
    }
}

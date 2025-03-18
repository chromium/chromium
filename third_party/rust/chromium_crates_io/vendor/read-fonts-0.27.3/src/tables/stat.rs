//! The [STAT](https://learn.microsoft.com/en-us/typography/opentype/spec/stat) table

include!("../../generated/generated_stat.rs");

#[cfg(test)]
mod tests {
    use types::{Fixed, NameId};

    use crate::{table_provider::TableProvider, FontRef};

    use super::*;

    #[test]
    fn smoke_test() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let table = font.stat().unwrap();
        assert_eq!(table.design_axis_count(), 1);
        let axis_record = &table.design_axes().unwrap()[0];
        assert_eq!(axis_record.axis_tag(), Tag::new(b"wght"));
        assert_eq!(axis_record.axis_name_id(), NameId::new(257));
        assert_eq!(axis_record.axis_ordering(), 0);
        let axis_values = table.offset_to_axis_values().unwrap().unwrap();
        let axis_values = axis_values
            .axis_values()
            .iter()
            .map(|x| x.unwrap())
            .collect::<Vec<_>>();

        assert_eq!(axis_values.len(), 3);
        let last = &axis_values[2];
        if let AxisValue::Format1(table) = last {
            assert_eq!(table.axis_index(), 0);
            assert_eq!(table.value_name_id(), NameId::new(264));
            assert_eq!(table.value(), Fixed::from_f64(700.0));
        }
    }
}

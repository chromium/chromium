//! The [fvar](https://learn.microsoft.com/en-us/typography/opentype/spec/fvar) table

#[path = "./instance_record.rs"]
mod instance_record;

pub use instance_record::InstanceRecord;

include!("../../generated/generated_fvar.rs");

impl Fvar {
    /// We need everyone to have, or not have, post_script name so we can have a single record size.
    fn check_instances(&self, ctx: &mut ValidationCtx) {
        let sum: i32 = self
            .axis_instance_arrays
            .instances
            .iter()
            .map(|ir| ir.post_script_name_id.map(|_| 1).unwrap_or(-1))
            .sum();
        if sum.unsigned_abs() as usize != self.axis_instance_arrays.instances.len() {
            ctx.report("All or none of the instances must have post_script_name_id set. Use Some(0xFFFF) if you need to set it where you have no value.");
        }

        let axis_count = self.axis_count();
        let uncoordinated_instances = self
            .axis_instance_arrays
            .instances
            .iter()
            .filter(|ir| ir.coordinates.len() != axis_count as usize)
            .count();
        if uncoordinated_instances > 0 {
            ctx.report(format!(
                "{uncoordinated_instances} instances do not have axis_count ({axis_count}) coordinates",
            ));
        }
    }

    fn axis_count(&self) -> u16 {
        self.axis_instance_arrays.axes.len().try_into().unwrap()
    }

    fn instance_count(&self) -> u16 {
        self.axis_instance_arrays
            .instances
            .len()
            .try_into()
            .unwrap()
    }

    fn instance_size(&self) -> u16 {
        // https://learn.microsoft.com/en-us/typography/opentype/spec/fvar#fvar-header
        let mut instance_size = self.axis_count() * Fixed::RAW_BYTE_LEN as u16 + 4;
        if self
            .axis_instance_arrays
            .instances
            .iter()
            .any(|i| i.post_script_name_id.is_some())
        {
            instance_size += 2;
        }
        instance_size
    }
}

#[cfg(test)]
mod tests {
    use read_fonts::{FontRef, TableProvider};

    use super::*;

    fn wdth_wght_fvar() -> Fvar {
        let mut fvar = Fvar::default();

        fvar.axis_instance_arrays.axes.push(VariationAxisRecord {
            axis_tag: Tag::new(b"wght"),
            min_value: Fixed::from_i32(300),
            default_value: Fixed::from_i32(400),
            max_value: Fixed::from_i32(700),
            ..Default::default()
        });
        fvar.axis_instance_arrays.axes.push(VariationAxisRecord {
            axis_tag: Tag::new(b"wdth"),
            min_value: Fixed::from_f64(75.0),
            default_value: Fixed::from_f64(100.0),
            max_value: Fixed::from_f64(125.0),
            ..Default::default()
        });
        fvar
    }

    fn assert_wdth_wght_test_values(fvar: &read_fonts::tables::fvar::Fvar) {
        assert_eq!(fvar.version(), MajorMinor::VERSION_1_0);
        assert_eq!(fvar.axis_count(), 2);
        assert_eq!(
            vec![
                (Tag::new(b"wght"), 300.0, 400.0, 700.0),
                (Tag::new(b"wdth"), 75.0, 100.0, 125.0),
            ],
            fvar.axis_instance_arrays()
                .unwrap()
                .axes()
                .iter()
                .map(|var| (
                    var.axis_tag.get(),
                    var.min_value().to_f64(),
                    var.default_value().to_f64(),
                    var.max_value().to_f64()
                ))
                .collect::<Vec<_>>()
        );
    }

    fn get_only_instance(
        fvar: read_fonts::tables::fvar::Fvar,
    ) -> read_fonts::tables::fvar::InstanceRecord {
        let instances = fvar.axis_instance_arrays().unwrap().instances();
        assert_eq!(1, instances.len());
        instances.get(0).unwrap()
    }

    fn nameless_instance_record(coordinates: Vec<Fixed>) -> InstanceRecord {
        InstanceRecord {
            subfamily_name_id: NameId::TYPOGRAPHIC_SUBFAMILY_NAME,
            coordinates,
            ..Default::default()
        }
    }

    fn named_instance_record(coordinates: Vec<Fixed>, name_id: u16) -> InstanceRecord {
        let mut rec = nameless_instance_record(coordinates);
        rec.post_script_name_id = Some(NameId::new(name_id));
        rec
    }

    #[test]
    fn write_read_no_instances() {
        let fvar = wdth_wght_fvar();
        let bytes = crate::write::dump_table(&fvar).unwrap();
        let loaded = read_fonts::tables::fvar::Fvar::read(FontData::new(&bytes)).unwrap();
        assert_wdth_wght_test_values(&loaded);
    }

    #[test]
    fn write_read_short_instance() {
        let mut fvar = wdth_wght_fvar();
        let coordinates = vec![Fixed::from_i32(420), Fixed::from_f64(101.5)];
        fvar.axis_instance_arrays
            .instances
            .push(nameless_instance_record(coordinates.clone()));
        assert_eq!(2 * 4 + 4, fvar.instance_size());

        let bytes = crate::write::dump_table(&fvar).unwrap();
        let loaded = read_fonts::tables::fvar::Fvar::read(FontData::new(&bytes)).unwrap();
        assert_wdth_wght_test_values(&loaded);
        assert_eq!(fvar.instance_size(), loaded.instance_size());

        let instance = get_only_instance(loaded);
        assert_eq!(None, instance.post_script_name_id);
        assert_eq!(
            coordinates,
            instance
                .coordinates
                .iter()
                .map(|v| v.get())
                .collect::<Vec<_>>()
        );
    }

    #[test]
    fn write_read_long_instance() {
        let mut fvar = wdth_wght_fvar();
        let coordinates = vec![Fixed::from_i32(650), Fixed::from_i32(420)];
        fvar.axis_instance_arrays
            .instances
            .push(named_instance_record(coordinates.clone(), 256));
        assert_eq!(2 * 4 + 6, fvar.instance_size());

        let bytes = crate::write::dump_table(&fvar).unwrap();
        let loaded = read_fonts::tables::fvar::Fvar::read(FontData::new(&bytes)).unwrap();
        assert_wdth_wght_test_values(&loaded);
        assert_eq!(fvar.instance_size(), loaded.instance_size());

        let instance = get_only_instance(loaded);
        assert_eq!(Some(NameId::new(256)), instance.post_script_name_id);
        assert_eq!(
            coordinates,
            instance
                .coordinates
                .iter()
                .map(|v| v.get())
                .collect::<Vec<_>>()
        );
    }

    #[test]
    fn round_trip() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let read_testdata = font.fvar().unwrap();

        let fvar = Fvar::from_table_ref(&read_testdata);
        let bytes = crate::write::dump_table(&fvar).unwrap();
        let loaded = read_fonts::tables::fvar::Fvar::read(FontData::new(&bytes)).unwrap();

        assert_eq!(read_testdata.version(), loaded.version());
        assert_eq!(read_testdata.axis_count(), loaded.axis_count());
    }

    #[test]
    fn inconsistent_instance_size_fails() {
        let mut fvar = wdth_wght_fvar();
        let coordinates = vec![Fixed::from_i32(650), Fixed::from_i32(420)];
        // OMG no, inconsistent sizing!
        fvar.axis_instance_arrays
            .instances
            .push(nameless_instance_record(coordinates.clone()));
        fvar.axis_instance_arrays
            .instances
            .push(named_instance_record(coordinates, 256));
        assert!(fvar.validate().is_err());
    }

    #[test]
    fn wrong_number_of_coordinates_fails() {
        let mut fvar = wdth_wght_fvar();
        let coordinates = vec![Fixed::from_i32(650)];
        fvar.axis_instance_arrays
            .instances
            .push(nameless_instance_record(coordinates));
        assert!(fvar.validate().is_err());
    }
}

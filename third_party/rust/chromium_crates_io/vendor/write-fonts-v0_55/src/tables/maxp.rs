//! The maxp table

include!("../../generated/generated_maxp.rs");

macro_rules! impl_maxp_version {
    ($maxp:ident) => {
        impl $maxp {
            fn compute_version(&self) -> Version16Dot16 {
                if self.max_points.is_some()
                    || self.max_contours.is_some()
                    || self.max_composite_points.is_some()
                    || self.max_composite_contours.is_some()
                    || self.max_zones.is_some()
                    || self.max_twilight_points.is_some()
                    || self.max_storage.is_some()
                    || self.max_function_defs.is_some()
                    || self.max_instruction_defs.is_some()
                    || self.max_stack_elements.is_some()
                    || self.max_size_of_instructions.is_some()
                    || self.max_component_elements.is_some()
                    || self.max_component_depth.is_some()
                {
                    Version16Dot16::VERSION_1_0
                } else {
                    Version16Dot16::VERSION_0_5
                }
            }
        }
    };
}

impl_maxp_version!(Maxp);
impl_maxp_version!(MaxpExtended);

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn extended_maxp_round_trip_and_field_widths() {
        use crate::from_obj::ToOwnedTable;
        for count in [65535, 65536, 70002, 0xffffff] {
            let table = MaxpExtended {
                num_glyphs: Uint24::new(count),
                ..Default::default()
            };
            let bytes = crate::dump_table(&table).unwrap();
            assert_eq!(bytes.len(), 7);
            assert_eq!(&bytes[4..], &count.to_be_bytes()[1..]);
            let read =
                read_fonts::tables::maxp::MaxpExtended::read(bytes.as_slice().into()).unwrap();
            assert_eq!(read.num_glyphs().to_u32(), count);
            assert_eq!(read.version(), Version16Dot16::VERSION_0_5);
            let owned: MaxpExtended = read.to_owned_table();
            assert_eq!(crate::dump_table(&owned).unwrap(), bytes);
        }
        let table = MaxpExtended {
            num_glyphs: Uint24::new(70002),
            max_points: Some(11),
            max_contours: Some(12),
            max_composite_points: Some(13),
            max_composite_contours: Some(14),
            max_zones: Some(2),
            max_twilight_points: Some(15),
            max_storage: Some(16),
            max_function_defs: Some(17),
            max_instruction_defs: Some(18),
            max_stack_elements: Some(19),
            max_size_of_instructions: Some(20),
            max_component_elements: Some(21),
            max_component_depth: Some(22),
        };
        let bytes = crate::dump_table(&table).unwrap();
        assert_eq!(bytes.len(), 33);
        let read = read_fonts::tables::maxp::MaxpExtended::read(bytes.as_slice().into()).unwrap();
        assert_eq!(read.version(), Version16Dot16::VERSION_1_0);
        assert_eq!(read.max_points(), Some(11));
        assert_eq!(read.max_component_depth(), Some(22));
        let owned: MaxpExtended = read.to_owned_table();
        assert_eq!(crate::dump_table(&owned).unwrap(), bytes);
    }

    #[test]
    fn maxp_05() {
        let maxp_05 = Maxp {
            num_glyphs: 5,
            ..Default::default()
        };

        let dumped = crate::write::dump_table(&maxp_05).unwrap();
        assert_eq!(dumped.len(), 6);
        let data = FontData::new(&dumped);
        let loaded = read_fonts::tables::maxp::Maxp::read(data).unwrap();
        assert_eq!(loaded.version(), Version16Dot16::VERSION_0_5);
        assert_eq!(loaded.num_glyphs(), 5);
    }

    #[test]
    fn maxp_10() {
        let maxp_05 = Maxp {
            num_glyphs: 5,
            max_points: Some(6),
            max_contours: Some(7),
            max_composite_points: Some(8),
            max_composite_contours: Some(9),
            max_zones: Some(10),
            max_twilight_points: Some(11),
            max_storage: Some(12),
            max_function_defs: Some(13),
            max_instruction_defs: Some(14),
            max_stack_elements: Some(15),
            max_size_of_instructions: Some(16),
            max_component_elements: Some(17),
            max_component_depth: Some(18),
        };

        let _dumped = crate::write::dump_table(&maxp_05).unwrap();

        let data = FontData::new(&_dumped);
        let loaded = read_fonts::tables::maxp::Maxp::read(data).unwrap();
        assert_eq!(loaded.version(), Version16Dot16::VERSION_1_0);
        assert_eq!(loaded.max_composite_contours(), Some(9));
        assert_eq!(loaded.max_zones(), Some(10));
        assert_eq!(loaded.max_component_depth(), Some(18));
    }
}

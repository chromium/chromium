//! The [Font Variations](https://docs.microsoft.com/en-us/typography/opentype/spec/fvar) table

include!("../../generated/generated_fvar.rs");

#[path = "./instance_record.rs"]
mod instance_record;

pub use instance_record::InstanceRecord;

impl<'a> Fvar<'a> {
    /// Returns the array of variation axis records.
    pub fn axes(&self) -> Result<&'a [VariationAxisRecord], ReadError> {
        Ok(self.axis_instance_arrays()?.axes())
    }

    /// Returns the array of instance records.
    pub fn instances(&self) -> Result<ComputedArray<'a, InstanceRecord<'a>>, ReadError> {
        Ok(self.axis_instance_arrays()?.instances())
    }
}

impl VariationAxisRecord {
    /// Returns a normalized coordinate for the given value.
    pub fn normalize(&self, mut value: Fixed) -> Fixed {
        use core::cmp::Ordering::*;
        let min_value = self.min_value();
        let default_value = self.default_value();
        // Make sure max is >= min to avoid potential panic in clamp.
        let max_value = self.max_value().max(min_value);
        value = value.clamp(min_value, max_value);
        value = match value.cmp(&default_value) {
            Less => {
                -((default_value.saturating_sub(value)) / (default_value.saturating_sub(min_value)))
            }
            Greater => {
                (value.saturating_sub(default_value)) / (max_value.saturating_sub(default_value))
            }
            Equal => Fixed::ZERO,
        };
        value.clamp(-Fixed::ONE, Fixed::ONE)
    }
}

#[cfg(test)]
mod tests {
    use crate::{FontRef, TableProvider};
    use types::{Fixed, NameId, Tag};

    #[test]
    fn axes() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let fvar = font.fvar().unwrap();
        assert_eq!(fvar.axis_count(), 1);
        let wght = &fvar.axes().unwrap().first().unwrap();
        assert_eq!(wght.axis_tag(), Tag::new(b"wght"));
        assert_eq!(wght.min_value(), Fixed::from_f64(100.0));
        assert_eq!(wght.default_value(), Fixed::from_f64(400.0));
        assert_eq!(wght.max_value(), Fixed::from_f64(900.0));
        assert_eq!(wght.flags(), 0);
        assert_eq!(wght.axis_name_id(), NameId::new(257));
    }

    #[test]
    fn instances() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let fvar = font.fvar().unwrap();
        assert_eq!(fvar.instance_count(), 9);
        // There are 9 instances equally spaced from 100.0 to 900.0
        // with name id monotonically increasing starting at 258.
        let instances = fvar.instances().unwrap();
        for i in 0..9 {
            let value = 100.0 * (i + 1) as f64;
            let name_id = NameId::new(258 + i as u16);
            let instance = instances.get(i).unwrap();
            assert_eq!(instance.coordinates.len(), 1);
            assert_eq!(
                instance.coordinates.first().unwrap().get(),
                Fixed::from_f64(value)
            );
            assert_eq!(instance.subfamily_name_id, name_id);
            assert_eq!(instance.post_script_name_id, None);
        }
    }

    #[test]
    fn normalize() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let fvar = font.fvar().unwrap();
        let axis = fvar.axes().unwrap().first().unwrap();
        let values = [100.0, 220.0, 250.0, 400.0, 650.0, 900.0];
        let expected = [-1.0, -0.60001, -0.5, 0.0, 0.5, 1.0];
        for (value, expected) in values.into_iter().zip(expected) {
            assert_eq!(
                axis.normalize(Fixed::from_f64(value)),
                Fixed::from_f64(expected)
            );
        }
    }

    #[test]
    fn normalize_overflow() {
        // From: https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=69787
        // & https://oss-fuzz.com/testcase?key=6159008335986688
        // fvar entry triggering overflow:
        // min: -26335.87451171875 def 8224.12548828125 max 8224.12548828125
        let test_case = &[
            79, 84, 84, 79, 0, 1, 32, 32, 255, 32, 32, 32, 102, 118, 97, 114, 32, 32, 32, 32, 0, 0,
            0, 28, 0, 0, 0, 41, 32, 0, 0, 0, 0, 1, 32, 32, 0, 2, 32, 32, 32, 32, 0, 0, 32, 32, 32,
            32, 32, 0, 0, 0, 0, 153, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
        ];
        let font = FontRef::new(test_case).unwrap();
        let fvar = font.fvar().unwrap();
        let axis = fvar.axes().unwrap()[1];
        // Should not panic with "attempt to subtract with overflow".
        assert_eq!(
            axis.normalize(Fixed::from_f64(0.0)),
            Fixed::from_f64(-0.2509765625)
        );
    }
}

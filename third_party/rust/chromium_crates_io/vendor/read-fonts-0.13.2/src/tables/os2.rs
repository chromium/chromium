//! The [os2](https://docs.microsoft.com/en-us/typography/opentype/spec/os2) table

include!("../../generated/generated_os2.rs");

#[cfg(test)]
mod tests {
    use crate::{table_provider::TableProvider, FontRef};

    #[test]
    fn read_sample() {
        let font = FontRef::new(font_test_data::SIMPLE_GLYF).unwrap();
        let table = font.os2().unwrap();
        assert_eq!(table.version(), 4);
    }
}

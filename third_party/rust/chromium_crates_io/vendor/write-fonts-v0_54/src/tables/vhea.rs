//! the [vhea (Vertical Header)](https://learn.microsoft.com/en-us/typography/opentype/spec/vhea) table

include!("../../generated/generated_vhea.rs");

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn version_serialization() {
        let table = Vhea::default();
        let dumped = crate::dump_table(&table).unwrap();
        let raw_version = u32::from_be_bytes(dumped[..4].try_into().unwrap());
        // this is not a (u16, u16) pair, but a Version16Dot16, which has its own
        // weird representation
        assert_eq!(raw_version, 0x00011000);
    }
}

//! The gasp table

include!("../../generated/generated_gasp.rs");

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn gasping() {
        let gasp = Gasp {
            version: 1,
            num_ranges: 2,
            gasp_ranges: vec![
                GaspRange {
                    range_max_ppem: 42,
                    range_gasp_behavior: GaspRangeBehavior::GASP_GRIDFIT,
                },
                GaspRange {
                    range_max_ppem: 4242,
                    range_gasp_behavior: GaspRangeBehavior::GASP_SYMMETRIC_SMOOTHING,
                },
            ],
        };

        let _dumped = crate::write::dump_table(&gasp).unwrap();
        let data = FontData::new(&_dumped);
        let loaded = read_fonts::tables::gasp::Gasp::read(data).unwrap();

        assert_eq!(1, loaded.version());
        assert_eq!(
            vec![
                (42, GaspRangeBehavior::GASP_GRIDFIT),
                (4242, GaspRangeBehavior::GASP_SYMMETRIC_SMOOTHING)
            ],
            loaded
                .gasp_ranges()
                .iter()
                .map(|r| (r.range_max_ppem(), r.range_gasp_behavior()))
                .collect::<Vec<_>>()
        );
    }
}

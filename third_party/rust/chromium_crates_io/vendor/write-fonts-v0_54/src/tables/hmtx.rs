//! The hmtx table

include!("../../generated/generated_hmtx.rs");

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn smoke_test() {
        let hmtx = Hmtx {
            h_metrics: vec![LongMetric {
                advance: 602,
                side_bearing: -214,
            }],
            left_side_bearings: vec![-20, -32, -44, -6],
        };

        let _dumped = crate::write::dump_table(&hmtx).unwrap();

        let data = FontData::new(&_dumped);
        let loaded = read_fonts::tables::hmtx::Hmtx::read(data, 1).unwrap();
        assert_eq!(loaded.h_metrics()[0].advance(), 602);
        assert_eq!(loaded.h_metrics()[0].side_bearing(), -214);
        assert_eq!(loaded.left_side_bearings(), &hmtx.left_side_bearings);
    }
}

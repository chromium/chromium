//! The hmtx table

include!("../../generated/generated_hmtx.rs");

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn extended_metric_tables_round_trip() {
        use crate::{
            from_obj::ToOwnedTable,
            tables::{hhea::HheaExtended, vhea::VheaExtended, vmtx::VmtxExtended},
        };
        for count in [65535, 65536, 70000] {
            let hhea = HheaExtended {
                number_of_h_metrics: count,
                ..Default::default()
            };
            let vhea = VheaExtended {
                number_of_long_ver_metrics: count,
                ..Default::default()
            };
            let header = crate::dump_table(&hhea).unwrap();
            assert_eq!(header.len(), 38);
            assert_eq!(&header[34..], &count.to_be_bytes());
            let read =
                read_fonts::tables::hhea::HheaExtended::read(header.as_slice().into()).unwrap();
            assert_eq!(read.number_of_h_metrics(), count);
            let owned: HheaExtended = read.to_owned_table();
            assert_eq!(crate::dump_table(&owned).unwrap(), header);
            let header = crate::dump_table(&vhea).unwrap();
            assert_eq!(header.len(), 38);
            assert_eq!(&header[..4], &0x00011000u32.to_be_bytes());
            assert_eq!(&header[34..], &count.to_be_bytes());
            let read =
                read_fonts::tables::vhea::VheaExtended::read(header.as_slice().into()).unwrap();
            assert_eq!(read.number_of_long_ver_metrics(), count);
            let owned: VheaExtended = read.to_owned_table();
            assert_eq!(crate::dump_table(&owned).unwrap(), header);
            let hmtx = HmtxExtended {
                h_metrics: vec![LongMetric::new(1200, -12); count as usize],
                left_side_bearings: vec![-13, -14],
            };
            let bytes = crate::dump_table(&hmtx).unwrap();
            assert_eq!(bytes.len(), count as usize * 4 + 4);
            let read = read_fonts::tables::hmtx::HmtxExtended::read(bytes.as_slice().into(), count)
                .unwrap();
            assert_eq!(read.h_metrics().len() as u32, count);
            assert_eq!(read.side_bearing(GlyphId::new(count + 1)), Some(-14));
            let owned: HmtxExtended = read.to_owned_table();
            assert_eq!(crate::dump_table(&owned).unwrap(), bytes);
            let vmtx = VmtxExtended::new(hmtx.h_metrics.clone(), hmtx.left_side_bearings.clone());
            let bytes = crate::dump_table(&vmtx).unwrap();
            let read = read_fonts::tables::vmtx::VmtxExtended::read(bytes.as_slice().into(), count)
                .unwrap();
            assert_eq!(read.v_metrics().len() as u32, count);
            assert_eq!(read.side_bearing(GlyphId::new(count + 1)), Some(-14));
            let owned: VmtxExtended = read.to_owned_table();
            assert_eq!(crate::dump_table(&owned).unwrap(), bytes);
        }
    }

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

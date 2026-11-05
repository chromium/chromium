//! impl subset() for SingleSubst subtable
mod extended;

use crate::FastHashMap;
use crate::{
    layout::{for_each_intersected_glyph_and_index, map_gsub_glyph},
    offset::SerializeSerialize,
    serialize::{SerializeErrorFlags, Serializer},
    Plan, Serialize, SubsetState, SubsetTable,
};
use read_fonts::{
    tables::{
        gsub::{
            SingleSubst, SingleSubstFormat1, SingleSubstFormat2, SingleSubstFormat3,
            SingleSubstFormat4,
        },
        layout::CoverageTable,
    },
    types::GlyphId,
    types::{Int24, Offset16},
    FontRef,
};

impl<'a> SubsetTable<'a> for SingleSubst<'_> {
    type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FastHashMap<u16, u16>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        match self {
            Self::Format1(item) => item.subset(plan, s, ()),
            Self::Format2(item) => item.subset(plan, s, ()),
            Self::Format3(item) => item.subset(plan, s, ()),
            Self::Format4(item) => item.subset(plan, s, ()),
        }
    }
}

impl SubsetTable<'_> for SingleSubstFormat1<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        if self.coverage_offset().is_null() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let coverage = self
            .coverage()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        let cov_glyphs = coverage.intersect_set(&plan.glyphset_gsub);
        let delta = self.delta_glyph_id();
        let glyph_map = &plan.glyph_map_gsub;

        if cov_glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let cap = cov_glyphs.len() as usize;
        let mut retained_glyphs = Vec::with_capacity(cap);
        let mut sub_glyphs = Vec::with_capacity(cap);
        for (new_g, new_sub_g) in cov_glyphs
            .iter()
            .map(|g| {
                (
                    g,
                    GlyphId::from(g.to_u32().wrapping_add_signed(delta as i32) & 0xffff),
                )
            })
            .filter_map(|(g, sub_g)| {
                let new_g = map_gsub_glyph(glyph_map, g)?;
                let new_sub_g = map_gsub_glyph(glyph_map, sub_g)?;
                Some((new_g, new_sub_g))
            })
        {
            retained_glyphs.push(new_g);
            sub_glyphs.push(new_sub_g);
        }

        if retained_glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        SingleSubst::serialize(s, (&retained_glyphs, &sub_glyphs))
    }
}

impl SubsetTable<'_> for SingleSubstFormat2<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        if self.coverage_offset().is_null() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let coverage = self
            .coverage()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        let glyph_map = &plan.glyph_map_gsub;
        let glyph_set = &plan.glyphset_gsub;
        let glyph_count = self.glyph_count();
        let cap = coverage
            .population()
            .min(glyph_set.len() as usize)
            .min(glyph_count as usize);
        let mut retained_glyphs = Vec::with_capacity(cap);
        let mut sub_glyphs = Vec::with_capacity(cap);

        let sub_glyph_ids = self.substitute_glyph_ids();
        let _ = for_each_intersected_glyph_and_index::<()>(
            &coverage,
            glyph_set,
            glyph_map,
            glyph_count,
            |idx, new_g| {
                let Some(new_sub_g) = sub_glyph_ids
                    .get(idx as usize)
                    .and_then(|sub_g| map_gsub_glyph(glyph_map, GlyphId::from(sub_g.get())))
                else {
                    return Ok(());
                };
                retained_glyphs.push(new_g);
                sub_glyphs.push(new_sub_g);
                Ok(())
            },
        );

        if retained_glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        SingleSubst::serialize(s, (&retained_glyphs, &sub_glyphs))
    }
}

impl<'a> Serialize<'a> for SingleSubst<'_> {
    type Args = (&'a [GlyphId], &'a [GlyphId]);
    fn serialize(s: &mut Serializer, args: Self::Args) -> Result<(), SerializeErrorFlags> {
        let (glyphs, sub_glyphs) = args;
        if glyphs.len() != sub_glyphs.len() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
        }
        if glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let max_gid = glyphs
            .iter()
            .chain(sub_glyphs)
            .map(|gid| gid.to_u32())
            .max()
            .unwrap();
        if max_gid > 0xffffff {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW));
        }
        let wide = max_gid > 0xffff;
        let mask = if wide { 0xffffff } else { 0xffff };

        let delta = sub_glyphs[0].to_u32().wrapping_sub(glyphs[0].to_u32()) & mask;
        if glyphs
            .iter()
            .zip(sub_glyphs)
            .skip(1)
            .all(|(g, sub_g)| sub_g.to_u32().wrapping_sub(g.to_u32()) & mask == delta)
        {
            if wide {
                // Sign-extend the modular delta into the signed 24-bit field.
                let delta = Int24::new((delta as i32) << 8 >> 8);
                SingleSubstFormat3::serialize(s, (glyphs, delta))
            } else {
                SingleSubstFormat1::serialize(s, (glyphs, delta as i16))
            }
        } else if wide || glyphs.len() > u16::MAX as usize {
            SingleSubstFormat4::serialize(s, (glyphs, sub_glyphs))
        } else {
            SingleSubstFormat2::serialize(s, (glyphs, sub_glyphs))
        }
    }
}

impl<'a> Serialize<'a> for SingleSubstFormat1<'_> {
    type Args = (&'a [GlyphId], i16);
    fn serialize(s: &mut Serializer, args: Self::Args) -> Result<(), SerializeErrorFlags> {
        // format
        s.embed(1_u16)?;

        // cov offset
        let cov_offset_pos = s.embed(0_u16)?;
        let (cov_glyphs, delta) = args;
        Offset16::serialize_serialize::<CoverageTable>(s, cov_glyphs, cov_offset_pos)?;
        // delta
        s.embed(delta).map(|_| ())
    }
}

impl<'a> Serialize<'a> for SingleSubstFormat2<'_> {
    type Args = (&'a [GlyphId], &'a [GlyphId]);
    fn serialize(s: &mut Serializer, args: Self::Args) -> Result<(), SerializeErrorFlags> {
        // format
        s.embed(2_u16)?;

        // cov offset
        let cov_offset_pos = s.embed(0_u16)?;
        let (cov_glyphs, sub_glyphs) = args;

        // glyph count
        let count = cov_glyphs.len();
        if sub_glyphs.len() != count {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
        }
        s.embed(count as u16)?;

        // substitute glyph ids
        let mut pos = s.allocate_size(2 * count, false)?;
        for g in sub_glyphs {
            let g = g.to_u32() as u16;
            s.copy_assign(pos, g);
            pos += 2;
        }

        Offset16::serialize_serialize::<CoverageTable>(s, cov_glyphs, cov_offset_pos)
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use read_fonts::{types::GlyphId, FontRef, TableProvider};

    #[test]
    fn test_subset_single_subst_format1() {
        use read_fonts::tables::gsub::SubstitutionSubtables;

        let font =
            FontRef::new(include_bytes!("../../test-data/fonts/Roboto-Regular.ttf")).unwrap();
        let gsub_lookups = font.gsub().unwrap().lookup_list().unwrap();
        let lookup = gsub_lookups.lookups().get(5).unwrap();

        let SubstitutionSubtables::Single(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let singlesubst_table = sub_tables.get(0).unwrap();
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 780],
            ..Default::default()
        };

        plan.glyph_map_gsub[777] = GlyphId::from(5_u32);
        plan.glyph_map_gsub[779] = GlyphId::from(6_u32);

        plan.glyphset_gsub.insert(GlyphId::from(777_u32));
        plan.glyphset_gsub.insert(GlyphId::from(779_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        let subset_state = SubsetState::default();
        singlesubst_table
            .subset(&plan, &mut s, (&subset_state, &font, &plan.gsub_lookups))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 12] = [
            0x00, 0x01, 0x00, 0x06, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x05,
        ];

        assert_eq!(subsetted_data, expected_data);
    }

    #[test]
    fn test_subset_single_subst_format2() {
        use read_fonts::tables::gsub::SubstitutionSubtables;

        let font =
            FontRef::new(include_bytes!("../../test-data/fonts/Roboto-Regular.ttf")).unwrap();
        let gsub_lookups = font.gsub().unwrap().lookup_list().unwrap();
        let lookup = gsub_lookups.lookups().get(1).unwrap();

        let SubstitutionSubtables::Single(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };

        let singlesubst_table = sub_tables.get(0).unwrap();
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 1283],
            ..Default::default()
        };

        // test that output table is format 1
        plan.glyph_map_gsub[777] = GlyphId::from(5_u32);
        plan.glyph_map_gsub[779] = GlyphId::from(6_u32);
        plan.glyph_map_gsub[885] = GlyphId::from(7_u32);
        plan.glyph_map_gsub[1282] = GlyphId::from(8_u32);

        plan.glyphset_gsub.insert(GlyphId::from(777_u32));
        plan.glyphset_gsub.insert(GlyphId::from(779_u32));
        plan.glyphset_gsub.insert(GlyphId::from(885_u32));
        plan.glyphset_gsub.insert(GlyphId::from(1282_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        let subset_state = SubsetState::default();
        singlesubst_table
            .subset(&plan, &mut s, (&subset_state, &font, &plan.gsub_lookups))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 14] = [
            0x00, 0x01, 0x00, 0x06, 0x00, 0x02, 0x00, 0x01, 0x00, 0x02, 0x00, 0x05, 0x00, 0x06,
        ];

        assert_eq!(subsetted_data, expected_data);

        // test that output table is format 2
        plan.glyph_map_gsub.resize(553, crate::INVALID_GID);
        plan.glyph_map_gsub[73] = GlyphId::from(1_u32);
        plan.glyph_map_gsub[75] = GlyphId::from(2_u32);
        plan.glyph_map_gsub[485] = GlyphId::from(3_u32);
        plan.glyph_map_gsub[552] = GlyphId::from(4_u32);

        plan.glyphset_gsub.clear();
        plan.glyphset_gsub.insert(GlyphId::from(73_u32));
        plan.glyphset_gsub.insert(GlyphId::from(75_u32));
        plan.glyphset_gsub.insert(GlyphId::from(485_u32));
        plan.glyphset_gsub.insert(GlyphId::from(552_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        let subset_state = SubsetState::default();
        singlesubst_table
            .subset(&plan, &mut s, (&subset_state, &font, &plan.gsub_lookups))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 18] = [
            0x00, 0x02, 0x00, 0x0a, 0x00, 0x02, 0x00, 0x04, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02,
            0x00, 0x01, 0x00, 0x02,
        ];

        assert_eq!(subsetted_data, expected_data);
    }

    /// deltaGlyphID is applied modulo 65536: 504 + (-7504) maps to 58536.
    #[test]
    fn test_subset_single_subst_format1_wrapping_delta() {
        use read_fonts::{FontData, FontRead};

        #[rustfmt::skip]
        let raw_table: [u8; 12] = [
            // format=1, coverageOffset=6, deltaGlyphID=-7504
            0x00, 0x01, 0x00, 0x06, 0xE2, 0xB0,
            // coverage: format 1, glyph 504
            0x00, 0x01, 0x00, 0x01, 0x01, 0xF8,
        ];
        let table = SingleSubstFormat1::read(FontData::new(&raw_table)).unwrap();
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 58537],
            ..Default::default()
        };
        plan.glyph_map_gsub[504] = GlyphId::from(1_u32);
        plan.glyph_map_gsub[58536] = GlyphId::from(2_u32);
        plan.glyphset_gsub.insert(GlyphId::from(504_u32));
        plan.glyphset_gsub.insert(GlyphId::from(58536_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        assert_eq!(table.subset(&plan, &mut s, ()), Ok(()));
        assert!(!s.in_error());
        s.end_serialize();

        let expected_data: [u8; 12] = [
            0x00, 0x01, 0x00, 0x06, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
        ];
        assert_eq!(s.copy_bytes(), expected_data);
    }

    /// Deltas that are equal modulo 65536 still serialize as format 1.
    #[test]
    fn test_serialize_single_subst_wrapping_delta() {
        let glyphs = [GlyphId::from(1_u32), GlyphId::from(3_u32)];
        let sub_glyphs = [GlyphId::from(0xFFFF_u32), GlyphId::from(1_u32)];

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        assert_eq!(
            SingleSubst::serialize(&mut s, (&glyphs, &sub_glyphs)),
            Ok(())
        );
        assert!(!s.in_error());
        s.end_serialize();

        let expected_data: [u8; 14] = [
            0x00, 0x01, 0x00, 0x06, 0xFF, 0xFE, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x03,
        ];
        assert_eq!(s.copy_bytes(), expected_data);
    }
}

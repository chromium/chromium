//! impl subset() for SingleSubst subtable
use crate::fnv::FnvHashMap;
use crate::{
    layout::intersected_glyphs_and_indices,
    offset::SerializeSerialize,
    serialize::{SerializeErrorFlags, Serializer},
    Plan, Serialize, SubsetState, SubsetTable,
};
use write_fonts::{
    read::{
        tables::{
            gsub::{SingleSubst, SingleSubstFormat1, SingleSubstFormat2},
            layout::CoverageTable,
        },
        types::GlyphId,
        FontRef,
    },
    types::Offset16,
};

impl<'a> SubsetTable<'a> for SingleSubst<'_> {
    type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FnvHashMap<u16, u16>);
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
        let delta = self.delta_glyph_id() as i32;
        let glyph_map = &plan.glyph_map_gsub;

        if cov_glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let cap = cov_glyphs.len() as usize;
        let mut retained_glyphs = Vec::with_capacity(cap);
        let mut sub_glyphs = Vec::with_capacity(cap);
        for (new_g, new_sub_g) in cov_glyphs
            .iter()
            .map(|g| (g, GlyphId::from(g.to_u32().wrapping_add_signed(delta))))
            .filter_map(|(g, sub_g)| {
                let new_g = glyph_map.get(&g)?;
                let new_sub_g = glyph_map.get(&sub_g)?;
                Some((*new_g, *new_sub_g))
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
        let (cov_glyphs, glyph_idxes) =
            intersected_glyphs_and_indices(&coverage, &plan.glyphset_gsub, glyph_map);

        if cov_glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let cap = cov_glyphs.len();
        let mut retained_glyphs = Vec::with_capacity(cap);
        let mut sub_glyphs = Vec::with_capacity(cap);

        let sub_glyph_ids = self.substitute_glyph_ids();
        for (new_g, new_sub_g) in
            cov_glyphs
                .iter()
                .zip(glyph_idxes.iter())
                .filter_map(|(new_g, idx)| {
                    let sub_g = sub_glyph_ids.get(idx as usize)?;
                    let new_sub_g = glyph_map.get(&GlyphId::from(sub_g.get()))?;
                    Some((*new_g, *new_sub_g))
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

impl<'a> Serialize<'a> for SingleSubst<'_> {
    type Args = (&'a [GlyphId], &'a [GlyphId]);
    fn serialize(s: &mut Serializer, args: Self::Args) -> Result<(), SerializeErrorFlags> {
        let (glyphs, sub_glyphs) = args;
        if glyphs.len() != sub_glyphs.len() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
        }

        let delta = sub_glyphs[0].to_u32() as i32 - glyphs[0].to_u32() as i32;
        if glyphs
            .iter()
            .zip(sub_glyphs)
            .skip(1)
            .all(|(g, sub_g)| sub_g.to_u32() as i32 - g.to_u32() as i32 == delta)
        {
            SingleSubstFormat1::serialize(s, (glyphs, delta as i16))
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
    use write_fonts::read::{types::GlyphId, FontRef, TableProvider};

    #[test]
    fn test_subset_single_subst_format1() {
        use write_fonts::read::tables::gsub::SubstitutionSubtables;

        let font =
            FontRef::new(include_bytes!("../../test-data/fonts/Roboto-Regular.ttf")).unwrap();
        let gsub_lookups = font.gsub().unwrap().lookup_list().unwrap();
        let lookup = gsub_lookups.lookups().get(5).unwrap();

        let SubstitutionSubtables::Single(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let singlesubst_table = sub_tables.get(0).unwrap();
        let mut plan = Plan::default();

        plan.glyph_map_gsub
            .insert(GlyphId::from(777_u32), GlyphId::from(5_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(779_u32), GlyphId::from(6_u32));

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
        use write_fonts::read::tables::gsub::SubstitutionSubtables;

        let font =
            FontRef::new(include_bytes!("../../test-data/fonts/Roboto-Regular.ttf")).unwrap();
        let gsub_lookups = font.gsub().unwrap().lookup_list().unwrap();
        let lookup = gsub_lookups.lookups().get(1).unwrap();

        let SubstitutionSubtables::Single(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };

        let singlesubst_table = sub_tables.get(0).unwrap();
        let mut plan = Plan::default();

        // test that output table is format 1
        plan.glyph_map_gsub
            .insert(GlyphId::from(777_u32), GlyphId::from(5_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(779_u32), GlyphId::from(6_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(885_u32), GlyphId::from(7_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(1282_u32), GlyphId::from(8_u32));

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
        plan.glyph_map_gsub.clear();
        plan.glyph_map_gsub
            .insert(GlyphId::from(73_u32), GlyphId::from(1_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(75_u32), GlyphId::from(2_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(485_u32), GlyphId::from(3_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(552_u32), GlyphId::from(4_u32));

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
}

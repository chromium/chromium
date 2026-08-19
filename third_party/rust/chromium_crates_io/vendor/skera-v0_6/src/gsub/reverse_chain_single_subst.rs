//! impl subset() for ReverseChainSingleSubst subtable
use crate::fnv::FnvHashMap;
use crate::{
    offset::SerializeSerialize,
    serialize::{SerializeErrorFlags, Serializer},
    Plan, SubsetState, SubsetTable,
};
use write_fonts::{
    read::{
        tables::{gsub::ReverseChainSingleSubstFormat1, layout::CoverageTable},
        types::GlyphId,
        FontRef,
    },
    types::Offset16,
};

impl<'a> SubsetTable<'a> for ReverseChainSingleSubstFormat1<'_> {
    type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FnvHashMap<u16, u16>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        s.embed(self.subst_format())?;
        // coverage offset
        let cov_offset_pos = s.embed(0_u16)?;

        s.embed(self.backtrack_glyph_count())?;
        self.backtrack_coverages().subset(plan, s, ())?;

        s.embed(self.lookahead_glyph_count())?;
        self.lookahead_coverages().subset(plan, s, ())?;

        let coverage = self
            .coverage()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        let glyph_map = &plan.glyph_map_gsub;
        let mut retained_cov_glyphs = Vec::with_capacity(
            (self.glyph_count() as usize).min(plan.glyphset_gsub.len() as usize),
        );
        let sub_glyphs = self.substitute_glyph_ids();
        // glyph count
        let glyph_count_pos = s.embed(0_u16)?;
        for (cov_g, sub_g) in coverage
            .iter()
            .zip(sub_glyphs)
            .filter_map(|(cov_g, sub_g)| {
                let new_cov_g = glyph_map.get(&GlyphId::from(cov_g))?;
                let new_sub_g = glyph_map.get(&GlyphId::from(sub_g.get()))?;
                Some((*new_cov_g, *new_sub_g))
            })
        {
            retained_cov_glyphs.push(cov_g);
            s.embed(sub_g.to_u32() as u16)?;
        }

        let glyph_count = retained_cov_glyphs.len() as u16;
        if glyph_count == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        s.copy_assign(glyph_count_pos, glyph_count);
        Offset16::serialize_serialize::<CoverageTable>(s, &retained_cov_glyphs, cov_offset_pos)
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use write_fonts::read::{types::GlyphId, FontRef, TableProvider};

    #[test]
    fn test_subset_reverse_chain_single_subst() {
        use write_fonts::read::tables::gsub::SubstitutionSubtables;

        let font = FontRef::new(include_bytes!(
            "../../test-data/fonts/gsub8_manually_created.otf"
        ))
        .unwrap();
        let gsub_lookups = font.gsub().unwrap().lookup_list().unwrap();
        let lookup = gsub_lookups.lookups().get(4).unwrap();

        let SubstitutionSubtables::Reverse(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let subst_table = sub_tables.get(1).unwrap();
        let mut plan = Plan {
            font_num_glyphs: 100,
            ..Default::default()
        };

        plan.glyph_map_gsub
            .insert(GlyphId::from(49_u32), GlyphId::from(1_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(50_u32), GlyphId::from(2_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(51_u32), GlyphId::from(3_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(65_u32), GlyphId::from(4_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(67_u32), GlyphId::from(5_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(75_u32), GlyphId::from(6_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(83_u32), GlyphId::from(7_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(98_u32), GlyphId::from(8_u32));

        plan.glyphset_gsub.insert(GlyphId::from(49_u32));
        plan.glyphset_gsub.insert(GlyphId::from(50_u32));
        plan.glyphset_gsub.insert(GlyphId::from(51_u32));
        plan.glyphset_gsub.insert(GlyphId::from(65_u32));
        plan.glyphset_gsub.insert(GlyphId::from(67_u32));
        plan.glyphset_gsub.insert(GlyphId::from(75_u32));
        plan.glyphset_gsub.insert(GlyphId::from(83_u32));
        plan.glyphset_gsub.insert(GlyphId::from(98_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        let subset_state = SubsetState::default();
        subst_table
            .subset(&plan, &mut s, (&subset_state, &font, &plan.gsub_lookups))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 66] = [
            0x00, 0x01, 0x00, 0x18, 0x00, 0x03, 0x00, 0x3c, 0x00, 0x36, 0x00, 0x30, 0x00, 0x03,
            0x00, 0x2a, 0x00, 0x24, 0x00, 0x1e, 0x00, 0x01, 0x00, 0x07, 0x00, 0x01, 0x00, 0x01,
            0x00, 0x06, 0x00, 0x01, 0x00, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x01, 0x00, 0x02,
            0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x00, 0x01,
            0x00, 0x01, 0x00, 0x08, 0x00, 0x01, 0x00, 0x01, 0x00, 0x05,
        ];

        assert_eq!(subsetted_data, expected_data);
    }
}

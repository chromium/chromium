//! impl subset() for AlternateSubst subtable
use crate::FastHashMap;
use crate::{
    layout::{for_each_intersected_glyph_and_index, map_gsub_glyph},
    offset::SerializeSerialize,
    offset_array::SubsetOffsetArray,
    serialize::{SerializeErrorFlags, SerializeResultEmpty, Serializer},
    Plan, SubsetState, SubsetTable,
};
use read_fonts::{
    tables::{
        gsub::{AlternateSet, AlternateSubstFormat1},
        layout::CoverageTable,
    },
    types::GlyphId,
    types::Offset16,
    FontRef,
};

impl<'a> SubsetTable<'a> for AlternateSubstFormat1<'_> {
    type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FastHashMap<u16, u16>);
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

        s.embed(self.subst_format())?;

        // cov offset
        let cov_offset_pos = s.embed(0_u16)?;

        // alternate set count
        let alt_set_count_pos = s.embed(0_u16)?;
        let alt_sets = self.alternate_sets();

        let glyph_set = &plan.glyphset_gsub;
        let alt_set_count = self.alternate_set_count();
        let mut glyphs = Vec::with_capacity(
            coverage
                .population()
                .min(glyph_set.len() as usize)
                .min(alt_set_count as usize),
        );
        for_each_intersected_glyph_and_index(
            &coverage,
            glyph_set,
            &plan.glyph_map_gsub,
            alt_set_count,
            |idx, g| {
                if !alt_sets
                    .subset_offset(idx as usize, s, plan, ())
                    .is_empty()?
                {
                    glyphs.push(g);
                }
                Ok(())
            },
        )?;

        if glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        s.copy_assign(alt_set_count_pos, glyphs.len() as u16);
        Offset16::serialize_serialize::<CoverageTable>(s, &glyphs, cov_offset_pos)
    }
}

impl SubsetTable<'_> for AlternateSet<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        let glyph_count_pos = s.embed(0_u16)?;
        let mut glyph_count = 0_u16;

        let glyph_map = &plan.glyph_map_gsub;
        let alt_glyphs = self.alternate_glyph_ids();
        for g in alt_glyphs {
            let Some(new_g) = map_gsub_glyph(glyph_map, GlyphId::from(g.get())) else {
                continue;
            };
            s.embed(new_g.to_u32() as u16)?;
            glyph_count += 1;
        }

        if glyph_count == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        s.copy_assign(glyph_count_pos, glyph_count);
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for read_fonts::tables::gsub::AlternateSubst<'_> {
    type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FastHashMap<u16, u16>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        match self {
            Self::Format1(table) => table.subset(plan, s, args),
            Self::Format2(table) => table.subset(plan, s, ()),
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use read_fonts::{types::GlyphId, FontRef, TableProvider};

    #[test]
    fn test_subset_alternate_subst_format1() {
        use read_fonts::tables::gsub::SubstitutionSubtables;

        let font = FontRef::new(include_bytes!(
            "../../test-data/fonts/gsub_alternate_substitution.otf"
        ))
        .unwrap();
        let gsub_lookups = font.gsub().unwrap().lookup_list().unwrap();
        let lookup = gsub_lookups.lookups().get(0).unwrap();

        let SubstitutionSubtables::Alternate(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let alt_subst_table = sub_tables.get(0).unwrap();
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 19],
            ..Default::default()
        };

        plan.glyph_map_gsub[3] = GlyphId::from(1_u32);
        plan.glyph_map_gsub[10] = GlyphId::from(2_u32);
        plan.glyph_map_gsub[18] = GlyphId::from(3_u32);

        plan.glyphset_gsub.insert(GlyphId::from(3_u32));
        plan.glyphset_gsub.insert(GlyphId::from(10_u32));
        plan.glyphset_gsub.insert(GlyphId::from(18_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        let subset_state = SubsetState::default();
        alt_subst_table
            .subset(&plan, &mut s, (&subset_state, &font, &plan.gsub_lookups))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 20] = [
            0x00, 0x01, 0x00, 0x08, 0x00, 0x01, 0x00, 0x0e, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
            0x00, 0x02, 0x00, 0x03, 0x00, 0x02,
        ];

        assert_eq!(subsetted_data, expected_data);
    }
}

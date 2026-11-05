//! impl subset() for MultipleSubst subtable
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
        gsub::{MultipleSubstFormat1, Sequence},
        layout::CoverageTable,
    },
    types::GlyphId,
    types::Offset16,
    FontRef,
};

impl<'a> SubsetTable<'a> for MultipleSubstFormat1<'_> {
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

        // sequence count
        let seq_count_pos = s.embed(0_u16)?;
        let sequences = self.sequences();

        let glyph_set = &plan.glyphset_gsub;
        let sequence_count = self.sequence_count();
        let mut glyphs = Vec::with_capacity(
            coverage
                .population()
                .min(glyph_set.len() as usize)
                .min(sequence_count as usize),
        );
        for_each_intersected_glyph_and_index(
            &coverage,
            glyph_set,
            &plan.glyph_map_gsub,
            sequence_count,
            |idx, g| {
                if !sequences
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
        s.copy_assign(seq_count_pos, glyphs.len() as u16);
        Offset16::serialize_serialize::<CoverageTable>(s, &glyphs, cov_offset_pos)
    }
}

impl SubsetTable<'_> for Sequence<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        let glyph_count = self.glyph_count();
        s.embed(glyph_count)?;

        let mut pos = s.allocate_size(glyph_count as usize * 2, false)?;

        let glyph_map = &plan.glyph_map_gsub;
        let sub_glyphs = self.substitute_glyph_ids();
        for g in sub_glyphs {
            let new_g = map_gsub_glyph(glyph_map, GlyphId::from(g.get()))
                .ok_or(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)?;

            s.copy_assign(pos, new_g.to_u32() as u16);
            pos += 2;
        }

        Ok(())
    }
}

impl<'a> SubsetTable<'a> for read_fonts::tables::gsub::MultipleSubst<'_> {
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
    fn test_subset_multiple_subst() {
        use read_fonts::tables::gsub::SubstitutionSubtables;

        let font = FontRef::new(include_bytes!(
            "../../test-data/fonts/NotoNastaliqUrdu-Regular.ttf"
        ))
        .unwrap();
        let gsub_lookups = font.gsub().unwrap().lookup_list().unwrap();
        let lookup = gsub_lookups.lookups().get(152).unwrap();

        let SubstitutionSubtables::Multiple(sub_tables) = lookup.subtables().unwrap() else {
            panic!("Wrong type of lookup table!");
        };
        let multiplesubst_table = sub_tables.get(0).unwrap();
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 967],
            ..Default::default()
        };

        plan.glyph_map_gsub[362] = GlyphId::from(5_u32);
        plan.glyph_map_gsub[512] = GlyphId::from(6_u32);
        plan.glyph_map_gsub[966] = GlyphId::from(11_u32);

        plan.glyphset_gsub.insert(GlyphId::from(362_u32));
        plan.glyphset_gsub.insert(GlyphId::from(512_u32));
        plan.glyphset_gsub.insert(GlyphId::from(966_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));

        let subset_state = SubsetState::default();
        multiplesubst_table
            .subset(&plan, &mut s, (&subset_state, &font, &plan.gsub_lookups))
            .unwrap();
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 34] = [
            0x00, 0x01, 0x00, 0x0a, 0x00, 0x02, 0x00, 0x1a, 0x00, 0x12, 0x00, 0x01, 0x00, 0x02,
            0x00, 0x05, 0x00, 0x06, 0x00, 0x03, 0x00, 0x06, 0x00, 0x0b, 0x00, 0x0b, 0x00, 0x03,
            0x00, 0x05, 0x00, 0x0b, 0x00, 0x0b,
        ];

        assert_eq!(subsetted_data, expected_data);
    }
}

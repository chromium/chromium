//! impl subset() for MultipleSubst subtable
use crate::fnv::FnvHashMap;
use crate::{
    layout::intersected_glyphs_and_indices,
    offset::SerializeSerialize,
    offset_array::SubsetOffsetArray,
    serialize::{SerializeErrorFlags, Serializer},
    Plan, SubsetState, SubsetTable,
};
use write_fonts::{
    read::{
        tables::{
            gsub::{MultipleSubstFormat1, Sequence},
            layout::CoverageTable,
        },
        types::GlyphId,
        FontRef,
    },
    types::Offset16,
};

impl<'a> SubsetTable<'a> for MultipleSubstFormat1<'_> {
    type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FnvHashMap<u16, u16>);
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
        let (cov_glyphs, seq_idxes) =
            intersected_glyphs_and_indices(&coverage, &plan.glyphset_gsub, glyph_map);

        if cov_glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        s.embed(self.subst_format())?;

        // cov offset
        let cov_offset_pos = s.embed(0_u16)?;

        // sequence count
        let seq_count_pos = s.embed(0_u16)?;
        let mut seq_count = 0_u16;
        let sequences = self.sequences();

        let mut glyphs = Vec::with_capacity(cov_glyphs.len());
        for (g, idx) in cov_glyphs.iter().zip(seq_idxes.iter()) {
            match sequences.subset_offset(idx as usize, s, plan, ()) {
                Ok(()) => {
                    glyphs.push(*g);
                    seq_count += 1;
                }
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => (),
                Err(e) => return Err(e),
            }
        }

        if glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        s.copy_assign(seq_count_pos, seq_count);
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
            let new_g = glyph_map
                .get(&GlyphId::from(g.get()))
                .ok_or(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)?;

            s.copy_assign(pos, new_g.to_u32() as u16);
            pos += 2;
        }

        Ok(())
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use write_fonts::read::{types::GlyphId, FontRef, TableProvider};

    #[test]
    fn test_subset_multiple_subst() {
        use write_fonts::read::tables::gsub::SubstitutionSubtables;

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
        let mut plan = Plan::default();

        plan.glyph_map_gsub
            .insert(GlyphId::from(362_u32), GlyphId::from(5_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(512_u32), GlyphId::from(6_u32));
        plan.glyph_map_gsub
            .insert(GlyphId::from(966_u32), GlyphId::from(11_u32));

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

//! Subsetting the extended reverse-chain substitution format.

use super::*;
use crate::layout::for_each_intersected_glyph_and_index;
use read_fonts::{
    tables::gsub::ReverseChainSingleSubstFormat2,
    types::{GlyphId24, Offset32, Uint24},
};

impl SubsetTable<'_> for ReverseChainSingleSubstFormat2<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: (),
    ) -> Result<(), SerializeErrorFlags> {
        if self.coverage_offset().is_null() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let coverage = self
            .coverage()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        let glyph_set = &plan.glyphset_gsub;
        let substitutes = self.substitute_glyph_ids();
        let substitute_count = substitutes.len() as u32;
        s.embed(2u16)?;
        let coverage_pos = s.embed(0u32)?;
        s.embed(self.backtrack_glyph_count())?;
        self.backtrack_coverages().subset(plan, s, ())?;
        s.embed(self.lookahead_glyph_count())?;
        self.lookahead_coverages().subset(plan, s, ())?;
        let count_pos = s.embed(Uint24::new(0))?;
        let mut retained_glyphs = Vec::with_capacity(
            coverage
                .population()
                .min(glyph_set.len() as usize)
                .min(substitute_count as usize),
        );
        for_each_intersected_glyph_and_index(
            &coverage,
            glyph_set,
            &plan.glyph_map_gsub,
            substitute_count,
            |index, gid| {
                let Some(substitute) = substitutes.get(index as usize) else {
                    return Ok(());
                };
                let Some(substitute) =
                    map_gsub_glyph(&plan.glyph_map_gsub, substitute.get().into())
                else {
                    return Ok(());
                };
                let substitute = GlyphId24::checked_new(substitute.to_u32())
                    .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
                s.embed(substitute)?;
                retained_glyphs.push(gid);
                Ok(())
            },
        )?;
        if retained_glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let count = u32::try_from(retained_glyphs.len())
            .ok()
            .and_then(Uint24::checked_new)
            .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
        s.copy_assign(count_pos, count);
        Offset32::serialize_serialize::<CoverageTable>(s, &retained_glyphs, coverage_pos)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use font_test_data::bebuffer::BeBuffer;
    use read_fonts::{tables::gsub::ReverseChainSingleSubst, FontData, FontRead};

    fn input(backtrack: u32, lookahead: u32) -> Vec<u8> {
        BeBuffer::new()
            .push(2u16)
            .push(22u32)
            .push(1u16)
            .push(Uint24::new(backtrack))
            .push(1u16)
            .push(Uint24::new(lookahead))
            .push(Uint24::new(1))
            .push(GlyphId24::new(70000))
            .push(3u16)
            .push(Uint24::new(1))
            .push(GlyphId24::new(65536))
            .push(3u16)
            .push(Uint24::new(1))
            .push(GlyphId24::new(70001))
            .push(3u16)
            .push(Uint24::new(1))
            .push(GlyphId24::new(70002))
            .to_vec()
    }

    fn plan(mapped: [u32; 4]) -> Plan {
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 70003],
            ..Default::default()
        };
        for (old, new) in [65536, 70000, 70001, 70002].into_iter().zip(mapped) {
            plan.glyphset_gsub.insert(GlyphId::new(old));
            plan.glyph_map_gsub[old as usize] = GlyphId::new(new);
        }
        plan
    }

    fn subset(input: &[u8], plan: &Plan) -> Result<Vec<u8>, SerializeErrorFlags> {
        let table = ReverseChainSingleSubst::read(FontData::new(input)).unwrap();
        let font = FontRef::new(font_test_data::NOTOSERIFHEBREW_AUTOHINT_METRICS).unwrap();
        let mut s = Serializer::new(1024 * 1024);
        s.start_serialize().unwrap();
        table.subset(
            plan,
            &mut s,
            (&SubsetState::default(), &font, &plan.gsub_lookups),
        )?;
        s.end_serialize();
        assert!(!s.in_error());
        Ok(s.copy_bytes())
    }

    #[test]
    fn reverse_subset_preserves_format_and_remaps_context() {
        for mapped in [[1, 2, 3, 4], [65536, 70000, 70001, 0xffffff]] {
            let bytes = subset(&input(30, 38), &plan(mapped)).unwrap();
            let table = ReverseChainSingleSubstFormat2::read(FontData::new(&bytes)).unwrap();
            assert_eq!(
                table.coverage().unwrap().get(GlyphId::new(mapped[0])),
                Some(0)
            );
            assert_eq!(
                table.substitute_glyph_ids()[0].get(),
                GlyphId24::new(mapped[1])
            );
            assert_eq!(
                table
                    .backtrack_coverages()
                    .get(0)
                    .unwrap()
                    .get(GlyphId::new(mapped[2])),
                Some(0)
            );
            assert_eq!(
                table
                    .lookahead_coverages()
                    .get(0)
                    .unwrap()
                    .get(GlyphId::new(mapped[3])),
                Some(0)
            );
            assert_eq!(table.backtrack_glyph_count(), 1);
            assert_eq!(table.lookahead_glyph_count(), 1);
        }
    }

    #[test]
    fn reverse_subset_remaps_large_indices_and_serializes_large_counts() {
        let count = 65537u32;
        let input = BeBuffer::new()
            .push(2u16)
            .push(13 + count * 3)
            .push(0u16)
            .push(0u16)
            .push(Uint24::new(count))
            .extend((0..count).map(|index| GlyphId24::new(if index == 65536 { 70000 } else { 0 })))
            .push(4u16)
            .push(Uint24::new(1))
            .extend([0, 65536, 0].map(Uint24::new));
        let mut plan = plan([1, 2, 3, 4]);
        let bytes = subset(&input, &plan).unwrap();
        let table = ReverseChainSingleSubstFormat2::read(FontData::new(&bytes)).unwrap();
        assert_eq!(table.glyph_count().to_u32(), 1);
        assert_eq!(table.substitute_glyph_ids()[0].get(), GlyphId24::new(2));
        for gid in 0..count {
            plan.glyphset_gsub.insert(GlyphId::new(gid));
            plan.glyph_map_gsub[gid as usize] = GlyphId::new(gid);
        }
        let bytes = subset(&input, &plan).unwrap();
        let table = ReverseChainSingleSubstFormat2::read(FontData::new(&bytes)).unwrap();
        assert_eq!(table.glyph_count().to_u32(), count);
        assert_eq!(
            table.coverage().unwrap().get(GlyphId::new(65536)),
            Some(65536)
        );
        assert_eq!(table.substitute_glyph_ids()[65536].get(), GlyphId24::new(2));
    }

    #[test]
    fn reverse_subset_rejects_missing_context_nulls_and_overflow() {
        let mut missing = plan([1, 2, 3, 4]);
        missing.glyph_map_gsub[70001] = crate::INVALID_GID;
        assert_eq!(
            subset(&input(30, 38), &missing),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
        );
        for (backtrack, lookahead) in [(0, 38), (30, 0)] {
            assert_eq!(
                subset(&input(backtrack, lookahead), &plan([1, 2, 3, 4])),
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
            );
        }
        assert_eq!(
            subset(&input(0xffffff, 38), &plan([1, 2, 3, 4])),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
        );
        assert_eq!(
            subset(&input(30, 38), &plan([1, 0x1000000, 3, 4])),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW)
        );
    }
}

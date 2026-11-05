//! Subsetting the extended ligature substitution format.

use super::*;
use crate::layout::for_each_intersected_glyph_and_index;
use read_fonts::{
    tables::gsub::{Ligature2, LigatureSet2, LigatureSubstFormat2},
    types::{GlyphId24, Offset24, Offset32, Uint24},
};

fn retains_ligature(ligature: &Ligature2, plan: &Plan) -> bool {
    map_gsub_glyph(&plan.glyph_map_gsub, ligature.ligature_glyph().into()).is_some()
        && ligature
            .component_glyph_ids()
            .iter()
            .all(|gid| map_gsub_glyph(&plan.glyph_map_gsub, gid.get().into()).is_some())
}

impl SubsetTable<'_> for LigatureSubstFormat2<'_> {
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
        let sets = self.ligature_sets();
        let set_count = sets.len() as u32;
        let cap = coverage
            .population()
            .min(glyph_set.len() as usize)
            .min(set_count as usize);
        let mut retained_glyphs = Vec::with_capacity(cap);
        let mut retained_indices = Vec::with_capacity(cap);
        for_each_intersected_glyph_and_index(
            &coverage,
            glyph_set,
            &plan.glyph_map_gsub,
            set_count,
            |index, gid| {
                let set = match sets.get(index as usize) {
                    Err(ReadError::NullOffset) => return Ok(()),
                    Err(_) => {
                        return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))
                    }
                    Ok(set) => set,
                };
                let mut retained = false;
                for ligature in set.ligatures().iter_as_nullable() {
                    let Some(ligature) = ligature
                        .transpose()
                        .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
                    else {
                        continue;
                    };
                    if retains_ligature(&ligature, plan) {
                        retained = true;
                        break;
                    }
                }
                if retained {
                    retained_glyphs.push(gid);
                    retained_indices.push(index as usize);
                }
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
        s.embed(2u16)?;
        let coverage_pos = s.embed(0u32)?;
        // Keep the same Coverage-last graph ordering as the legacy format.
        Offset32::serialize_serialize::<CoverageTable>(s, &retained_glyphs, coverage_pos)?;
        let coverage_idx = s
            .last_added_child_index()
            .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER))?;
        s.embed(count)?;
        for index in retained_indices {
            sets.subset_offset(index, s, plan, coverage_idx)?;
        }
        Ok(())
    }
}

impl SubsetTable<'_> for LigatureSet2<'_> {
    type ArgsForSubset = ObjIdx;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        coverage_idx: ObjIdx,
    ) -> Result<(), SerializeErrorFlags> {
        let count_pos = s.embed(0u16)?;
        let mut count = 0usize;
        for ligature in self.ligatures().iter_as_nullable() {
            let Some(ligature) = ligature
                .transpose()
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
            else {
                continue;
            };
            let snap = s.snapshot();
            let offset_pos = s.allocate_size(Offset24::RAW_BYTE_LEN, true)?;
            if Offset24::serialize_subset(&ligature, s, plan, coverage_idx, offset_pos)
                .is_empty()?
            {
                s.revert_snapshot(snap);
            } else {
                count += 1;
            }
        }
        if count == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        let count = u16::try_from(count)
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
        s.copy_assign(count_pos, count);
        s.add_virtual_link(coverage_idx)
    }
}

impl SubsetTable<'_> for Ligature2<'_> {
    type ArgsForSubset = ObjIdx;
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        coverage_idx: ObjIdx,
    ) -> Result<(), SerializeErrorFlags> {
        let mapped = map_gsub_glyph(&plan.glyph_map_gsub, self.ligature_glyph().into())
            .ok_or(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)?;
        let mapped = GlyphId24::checked_new(mapped.to_u32())
            .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
        s.embed(mapped)?;
        s.embed(self.component_count())?;
        for gid in self.component_glyph_ids() {
            let mapped = map_gsub_glyph(&plan.glyph_map_gsub, gid.get().into())
                .ok_or(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)?;
            let mapped = GlyphId24::checked_new(mapped.to_u32())
                .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
            s.embed(mapped)?;
        }
        s.add_virtual_link(coverage_idx)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use font_test_data::bebuffer::BeBuffer;
    use read_fonts::{tables::gsub::LigatureSubst, FontData, FontRead};

    fn input() -> Vec<u8> {
        BeBuffer::new()
            .push(2u16)
            .push(12u32)
            .push(Uint24::new(1))
            .push(Uint24::new(20))
            .push(3u16)
            .push(Uint24::new(1))
            .push(GlyphId24::new(65536))
            .push(2u16)
            .extend([8, 16].map(Uint24::new))
            .push(GlyphId24::new(70001))
            .push(2u16)
            .push(GlyphId24::new(70000))
            .push(GlyphId24::new(70000))
            .push(2u16)
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
        let table = LigatureSubst::read(FontData::new(input)).unwrap();
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
    fn ligature_subset_preserves_format_priority_and_coverage_order() {
        for mapped in [[1, 2, 3, 4], [65536, 70000, 0xffffff, 70002]] {
            let bytes = subset(&input(), &plan(mapped)).unwrap();
            let table = LigatureSubstFormat2::read(FontData::new(&bytes)).unwrap();
            assert_eq!(table.ligature_set_count().to_u32(), 1);
            assert_eq!(
                table.coverage().unwrap().get(GlyphId::new(mapped[0])),
                Some(0)
            );
            let set_offset = table.ligature_set_offsets()[0].get().to_u32();
            let set = table.ligature_sets().get(0).unwrap();
            assert_eq!(set.ligature_count(), 2);
            for (index, output, component) in [(0, mapped[2], mapped[1]), (1, mapped[1], mapped[3])]
            {
                let ligature = set.ligatures().get(index).unwrap();
                assert_eq!(ligature.ligature_glyph(), GlyphId24::new(output));
                assert_eq!(ligature.component_count(), 2);
                assert_eq!(
                    ligature.component_glyph_ids()[0].get(),
                    GlyphId24::new(component)
                );
                assert!(
                    table.coverage_offset().to_u32()
                        > set_offset + set.ligature_offsets()[index].get().to_u32()
                );
            }
        }
    }

    #[test]
    fn ligature_subset_prunes_missing_components_outputs_and_null_sets() {
        let mut plan = plan([1, 2, 3, 4]);
        plan.glyph_map_gsub[70002] = crate::INVALID_GID;
        let bytes = subset(&input(), &plan).unwrap();
        let table = LigatureSubstFormat2::read(FontData::new(&bytes)).unwrap();
        assert_eq!(table.ligature_sets().get(0).unwrap().ligature_count(), 1);
        plan.glyph_map_gsub[70001] = crate::INVALID_GID;
        assert_eq!(
            subset(&input(), &plan),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
        );
        let null_set = BeBuffer::new()
            .push(2u16)
            .push(12u32)
            .push(Uint24::new(1))
            .push(Uint24::new(0))
            .push(3u16)
            .push(Uint24::new(1))
            .push(GlyphId24::new(65536));
        assert_eq!(
            subset(&null_set, &plan),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
        );
    }

    #[test]
    fn ligature_subset_keeps_large_indices_and_serializes_large_counts() {
        let count = 65537u32;
        let coverage_offset = 9 + 3 * count;
        let make_input = |all_sets: bool| {
            BeBuffer::new()
                .push(2u16)
                .push(coverage_offset)
                .push(Uint24::new(count))
                .extend((0..count).map(|index| {
                    Uint24::new(if all_sets || index == 65536 {
                        coverage_offset + 14
                    } else {
                        0
                    })
                }))
                .push(4u16)
                .push(Uint24::new(1))
                .extend([0, 65536, 0].map(Uint24::new))
                .push(1u16)
                .push(Uint24::new(5))
                .push(GlyphId24::new(70001))
                .push(2u16)
                .push(GlyphId24::new(70000))
        };
        let mut plan = plan([1, 2, 3, 4]);
        let bytes = subset(&make_input(false), &plan).unwrap();
        let table = LigatureSubstFormat2::read(FontData::new(&bytes)).unwrap();
        assert_eq!(table.ligature_set_count().to_u32(), 1);
        assert_eq!(
            table
                .ligature_sets()
                .get(0)
                .unwrap()
                .ligatures()
                .get(0)
                .unwrap()
                .ligature_glyph(),
            GlyphId24::new(3)
        );
        for gid in 0..count {
            plan.glyphset_gsub.insert(GlyphId::new(gid));
            plan.glyph_map_gsub[gid as usize] = GlyphId::new(gid);
        }
        let bytes = subset(&make_input(true), &plan).unwrap();
        let table = LigatureSubstFormat2::read(FontData::new(&bytes)).unwrap();
        assert_eq!(table.ligature_set_count().to_u32(), count);
        assert_eq!(
            table.coverage().unwrap().get(GlyphId::new(65536)),
            Some(65536)
        );
        assert_eq!(
            table
                .ligature_sets()
                .get(65536)
                .unwrap()
                .ligatures()
                .get(0)
                .unwrap()
                .ligature_glyph(),
            GlyphId24::new(3)
        );
        assert!(table.coverage_offset().to_u32() > 65535);
    }

    #[test]
    fn ligature_subset_rejects_overflow_and_invalid_sets() {
        for mapped in [[1, 2, 0x1000000, 4], [1, 0x1000000, 3, 4]] {
            assert_eq!(
                subset(&input(), &plan(mapped)),
                Err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW)
            );
        }
        let invalid = BeBuffer::new()
            .push(2u16)
            .push(12u32)
            .push(Uint24::new(1))
            .push(Uint24::new(0xffffff))
            .push(3u16)
            .push(Uint24::new(1))
            .push(GlyphId24::new(65536));
        assert_eq!(
            subset(&invalid, &plan([1, 2, 3, 4])),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
        );
    }
}

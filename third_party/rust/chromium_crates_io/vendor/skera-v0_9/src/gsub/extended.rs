//! Subsetting support for the ISO OFF extended GSUB formats.

use crate::{
    layout::{for_each_intersected_glyph_and_index, map_gsub_glyph},
    offset::SerializeSerialize,
    offset_array::SubsetOffsetArray,
    serialize::{SerializeErrorFlags, SerializeResultEmpty, Serializer},
    Plan, SubsetTable,
};
use read_fonts::{
    tables::{
        gsub::{AlternateSet2, AlternateSubstFormat2, MultipleSubstFormat2, Sequence2},
        layout::CoverageTable,
    },
    types::{BigEndian, GlyphId, GlyphId24, Offset24, Offset32, Uint24},
    ArrayOfOffsets, FontRead,
};

fn subset_array_sets<'a, T>(
    coverage: &CoverageTable,
    sets: ArrayOfOffsets<'a, T, Offset24>,
    plan: &Plan,
    s: &mut Serializer,
) -> Result<(), SerializeErrorFlags>
where
    T: FontRead<'a, Args = ()> + SubsetTable<'a, ArgsForSubset = (), Output = ()>,
{
    let glyph_set = &plan.glyphset_gsub;
    let set_count = sets.len() as u32;
    s.embed(2u16)?;
    let coverage_pos = s.embed(0u32)?;
    let count_pos = s.embed(Uint24::new(0))?;
    let mut retained_glyphs = Vec::with_capacity(
        coverage
            .population()
            .min(glyph_set.len() as usize)
            .min(set_count as usize),
    );
    for_each_intersected_glyph_and_index(
        coverage,
        glyph_set,
        &plan.glyph_map_gsub,
        set_count,
        |index, gid| {
            if !sets.subset_offset(index as usize, s, plan, ()).is_empty()? {
                retained_glyphs.push(gid);
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
    s.copy_assign(count_pos, count);
    Offset32::serialize_serialize::<CoverageTable>(s, &retained_glyphs, coverage_pos)
}

macro_rules! subset_array_substitution {
    ($table:ident, $sets:ident) => {
        impl SubsetTable<'_> for $table<'_> {
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
                subset_array_sets(&coverage, self.$sets(), plan, s)
            }
        }
    };
}

subset_array_substitution!(MultipleSubstFormat2, sequences);
subset_array_substitution!(AlternateSubstFormat2, alternate_sets);

fn subset_glyph_array(
    glyphs: &[BigEndian<GlyphId24>],
    all_required: bool,
    plan: &Plan,
    s: &mut Serializer,
) -> Result<(), SerializeErrorFlags> {
    let count_pos = s.embed(0u16)?;
    let mut count = 0usize;
    for gid in glyphs {
        let Some(mapped) = map_gsub_glyph(&plan.glyph_map_gsub, GlyphId::from(gid.get())) else {
            if all_required {
                return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
            }
            continue;
        };
        let mapped = GlyphId24::checked_new(mapped.to_u32())
            .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
        s.embed(mapped)?;
        count += 1;
    }
    if count == 0 && !all_required {
        return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
    }
    let count = u16::try_from(count)
        .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
    s.copy_assign(count_pos, count);
    Ok(())
}

impl SubsetTable<'_> for Sequence2<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: (),
    ) -> Result<(), SerializeErrorFlags> {
        subset_glyph_array(self.substitute_glyph_ids(), true, plan, s)
    }
}

impl SubsetTable<'_> for AlternateSet2<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: (),
    ) -> Result<(), SerializeErrorFlags> {
        subset_glyph_array(self.alternate_glyph_ids(), false, plan, s)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{FastHashMap, SubsetState, INVALID_GID};
    use font_test_data::bebuffer::BeBuffer;
    use read_fonts::{
        tables::gsub::{AlternateSubst, MultipleSubst},
        FontData, FontRef,
    };

    fn array_substitution() -> Vec<u8> {
        BeBuffer::new()
            .push(2u16)
            .push(15u32)
            .push(Uint24::new(2))
            .extend([26, 34].map(Uint24::new))
            .push(3u16)
            .push(Uint24::new(2))
            .extend([65536, 70000].map(GlyphId24::new))
            .push(2u16)
            .extend([70000, 70001].map(GlyphId24::new))
            .push(1u16)
            .push(GlyphId24::new(70002))
            .to_vec()
    }

    fn plan(mapped: [u32; 4]) -> Plan {
        let mut plan = Plan {
            glyph_map_gsub: vec![INVALID_GID; 70003],
            ..Default::default()
        };
        for (old, new) in [65536, 70000, 70001, 70002].into_iter().zip(mapped) {
            plan.glyphset_gsub.insert(GlyphId::new(old));
            plan.glyph_map_gsub[old as usize] = GlyphId::new(new);
        }
        plan
    }

    fn subset(kind: u16, input: &[u8], plan: &Plan) -> Result<Vec<u8>, SerializeErrorFlags> {
        let font = FontRef::new(font_test_data::NOTOSERIFHEBREW_AUTOHINT_METRICS).unwrap();
        let mut s = Serializer::new(1024 * 1024);
        s.start_serialize().unwrap();
        let state = SubsetState::default();
        let lookups = FastHashMap::default();
        if kind == 2 {
            MultipleSubst::read(FontData::new(input)).unwrap().subset(
                plan,
                &mut s,
                (&state, &font, &lookups),
            )?;
        } else {
            AlternateSubst::read(FontData::new(input)).unwrap().subset(
                plan,
                &mut s,
                (&state, &font, &lookups),
            )?;
        }
        s.end_serialize();
        assert!(!s.in_error());
        Ok(s.copy_bytes())
    }

    fn outputs(kind: u16, bytes: &[u8]) -> (Vec<u32>, Vec<Vec<u32>>) {
        if kind == 2 {
            let table = MultipleSubstFormat2::read(FontData::new(bytes)).unwrap();
            (
                table
                    .coverage()
                    .unwrap()
                    .iter()
                    .map(GlyphId::to_u32)
                    .collect(),
                table
                    .sequences()
                    .iter()
                    .map(|set| {
                        set.unwrap()
                            .substitute_glyph_ids()
                            .iter()
                            .map(|gid| gid.get().to_u32())
                            .collect()
                    })
                    .collect(),
            )
        } else {
            let table = AlternateSubstFormat2::read(FontData::new(bytes)).unwrap();
            (
                table
                    .coverage()
                    .unwrap()
                    .iter()
                    .map(GlyphId::to_u32)
                    .collect(),
                table
                    .alternate_sets()
                    .iter()
                    .map(|set| {
                        set.unwrap()
                            .alternate_glyph_ids()
                            .iter()
                            .map(|gid| gid.get().to_u32())
                            .collect()
                    })
                    .collect(),
            )
        }
    }

    #[test]
    fn multiple_and_alternate_subsets_preserve_extended_format() {
        for kind in [2, 3] {
            for mapped in [[1, 2, 3, 4], [65535, 65536, 70000, 0xffffff]] {
                let bytes = subset(kind, &array_substitution(), &plan(mapped)).unwrap();
                assert_eq!(&bytes[..2], &[0, 2]);
                assert_eq!(
                    outputs(kind, &bytes),
                    (
                        vec![mapped[0], mapped[1]],
                        vec![vec![mapped[1], mapped[2]], vec![mapped[3]]]
                    )
                );
            }
        }
    }

    #[test]
    fn multiple_requires_all_outputs_but_alternates_can_prune() {
        let mut plan = plan([1, 2, 3, 4]);
        plan.glyph_map_gsub[70001] = INVALID_GID;
        let bytes = subset(2, &array_substitution(), &plan).unwrap();
        assert_eq!(outputs(2, &bytes), (vec![2], vec![vec![4]]));
        let bytes = subset(3, &array_substitution(), &plan).unwrap();
        assert_eq!(outputs(3, &bytes), (vec![1, 2], vec![vec![2], vec![4]]));
        plan.glyph_map_gsub[70000] = INVALID_GID;
        assert_eq!(
            subset(2, &array_substitution(), &plan),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
        );
        assert_eq!(
            subset(3, &array_substitution(), &plan),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
        );
    }

    #[test]
    fn multiple_and_alternate_use_large_coverage_indices_and_offsets() {
        let count = 65537u32;
        let coverage_offset = 9 + count * 3;
        let input = BeBuffer::new()
            .push(2u16)
            .push(coverage_offset)
            .push(Uint24::new(count))
            .extend((0..count).map(|index| {
                Uint24::new(if index == 65536 {
                    coverage_offset + 14
                } else {
                    0
                })
            }))
            .push(4u16)
            .push(Uint24::new(1))
            .extend([0, 65536, 0].map(Uint24::new))
            .push(1u16)
            .push(GlyphId24::new(70000));
        let mut plan = plan([1, 2, 3, 4]);
        plan.glyph_map_gsub[65536] = GlyphId::new(65536);
        plan.glyph_map_gsub[70000] = GlyphId::new(70000);
        for kind in [2, 3] {
            let bytes = subset(kind, &input, &plan).unwrap();
            assert_eq!(outputs(kind, &bytes), (vec![65536], vec![vec![70000]]));
        }
    }

    #[test]
    fn multiple_and_alternate_serialize_large_retained_counts() {
        let count = 65537u32;
        let coverage_offset = 9 + count * 3;
        let input = BeBuffer::new()
            .push(2u16)
            .push(coverage_offset)
            .push(Uint24::new(count))
            .extend((0..count).map(|_| Uint24::new(coverage_offset + 14)))
            .push(4u16)
            .push(Uint24::new(1))
            .extend([0, 65536, 0].map(Uint24::new))
            .push(1u16)
            .push(GlyphId24::new(70000));
        let mut plan = plan([65536, 70000, 70001, 70002]);
        for gid in 0..count {
            plan.glyphset_gsub.insert(GlyphId::new(gid));
            plan.glyph_map_gsub[gid as usize] = GlyphId::new(gid);
        }
        for kind in [2, 3] {
            let bytes = subset(kind, &input, &plan).unwrap();
            assert_eq!(u32::from_be_bytes([0, bytes[6], bytes[7], bytes[8]]), count);
            if kind == 2 {
                let table = MultipleSubstFormat2::read(FontData::new(&bytes)).unwrap();
                assert_eq!(
                    table.coverage().unwrap().get(GlyphId::new(65536)),
                    Some(65536)
                );
                assert_eq!(
                    table.sequences().get(65536).unwrap().substitute_glyph_ids()[0].get(),
                    GlyphId24::new(70000)
                );
            } else {
                let table = AlternateSubstFormat2::read(FontData::new(&bytes)).unwrap();
                assert_eq!(
                    table.coverage().unwrap().get(GlyphId::new(65536)),
                    Some(65536)
                );
                assert_eq!(
                    table
                        .alternate_sets()
                        .get(65536)
                        .unwrap()
                        .alternate_glyph_ids()[0]
                        .get(),
                    GlyphId24::new(70000)
                );
            }
        }
    }

    #[test]
    fn multiple_and_alternate_reject_overflow_and_invalid_sets() {
        for kind in [2, 3] {
            assert_eq!(
                subset(kind, &array_substitution(), &plan([1, 2, 0x1000000, 4])),
                Err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW)
            );
            let null = BeBuffer::new().push(2u16).push(0u32).push(Uint24::new(0));
            assert_eq!(
                subset(kind, &null, &Plan::default()),
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
            );
            let invalid = BeBuffer::new()
                .push(2u16)
                .push(12u32)
                .push(Uint24::new(1))
                .push(Uint24::new(0xffffff))
                .push(3u16)
                .push(Uint24::new(1))
                .push(GlyphId24::new(65536));
            assert_eq!(
                subset(kind, &invalid, &plan([1, 2, 3, 4])),
                Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
            );
        }
    }
}

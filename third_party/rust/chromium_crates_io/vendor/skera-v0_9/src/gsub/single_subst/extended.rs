//! Subsetting the 24-bit single-substitution formats.

use super::*;
use read_fonts::types::{GlyphId24, Offset32, Uint24};

fn subset_pairs(
    pairs: impl Iterator<Item = (GlyphId, GlyphId)>,
    plan: &Plan,
    s: &mut Serializer,
) -> Result<(), SerializeErrorFlags> {
    let (glyphs, substitutes): (Vec<_>, Vec<_>) = pairs
        .filter_map(|(gid, substitute)| {
            Some((
                map_gsub_glyph(&plan.glyph_map_gsub, gid)?,
                map_gsub_glyph(&plan.glyph_map_gsub, substitute)?,
            ))
        })
        .unzip();
    SingleSubst::serialize(s, (&glyphs, &substitutes))
}

impl SubsetTable<'_> for SingleSubstFormat3<'_> {
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
        let glyphs = coverage.intersect_set(&plan.glyphset_gsub);
        let delta = self.delta_glyph_id().to_i32();
        subset_pairs(
            glyphs.iter().map(|gid| {
                (
                    gid,
                    GlyphId::new(gid.to_u32().wrapping_add_signed(delta) & 0xffffff),
                )
            }),
            plan,
            s,
        )
    }
}

impl SubsetTable<'_> for SingleSubstFormat4<'_> {
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
        let substitutes = self.substitute_glyph_ids();
        subset_pairs(
            plan.glyphset_gsub.iter().filter_map(|gid| {
                let index = coverage.get(gid)?;
                let substitute = substitutes.get(index as usize)?.get();
                Some((gid, substitute.into()))
            }),
            plan,
            s,
        )
    }
}

impl<'a> Serialize<'a> for SingleSubstFormat3<'_> {
    type Args = (&'a [GlyphId], Int24);
    fn serialize(
        s: &mut Serializer,
        (glyphs, delta): Self::Args,
    ) -> Result<(), SerializeErrorFlags> {
        s.embed(3u16)?;
        let coverage_pos = s.embed(0u32)?;
        s.embed(delta)?;
        Offset32::serialize_serialize::<CoverageTable>(s, glyphs, coverage_pos)
    }
}

impl<'a> Serialize<'a> for SingleSubstFormat4<'_> {
    type Args = (&'a [GlyphId], &'a [GlyphId]);
    fn serialize(
        s: &mut Serializer,
        (glyphs, substitutes): Self::Args,
    ) -> Result<(), SerializeErrorFlags> {
        if glyphs.len() != substitutes.len() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
        }
        let count = u32::try_from(substitutes.len())
            .ok()
            .and_then(Uint24::checked_new)
            .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
        s.embed(4u16)?;
        let coverage_pos = s.embed(0u32)?;
        s.embed(count)?;
        for gid in substitutes {
            let gid = GlyphId24::checked_new(gid.to_u32())
                .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
            s.embed(gid)?;
        }
        Offset32::serialize_serialize::<CoverageTable>(s, glyphs, coverage_pos)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use font_test_data::bebuffer::BeBuffer;
    use read_fonts::{FontData, FontRead};

    fn serialize(glyphs: &[u32], substitutes: &[u32]) -> Vec<u8> {
        let glyphs: Vec<_> = glyphs.iter().copied().map(GlyphId::new).collect();
        let substitutes: Vec<_> = substitutes.iter().copied().map(GlyphId::new).collect();
        let mut s = Serializer::new(glyphs.len() * 12 + 64);
        s.start_serialize().unwrap();
        SingleSubst::serialize(&mut s, (&glyphs, &substitutes)).unwrap();
        s.end_serialize();
        assert!(!s.in_error());
        s.copy_bytes()
    }

    fn substitutes(table: &SingleSubst) -> Vec<u32> {
        match table {
            SingleSubst::Format1(t) => t
                .coverage()
                .unwrap()
                .iter()
                .map(|g| g.to_u32().wrapping_add_signed(t.delta_glyph_id() as i32) & 0xffff)
                .collect(),
            SingleSubst::Format2(t) => t
                .substitute_glyph_ids()
                .iter()
                .map(|g| g.get().to_u32())
                .collect(),
            SingleSubst::Format3(t) => t
                .coverage()
                .unwrap()
                .iter()
                .map(|g| g.to_u32().wrapping_add_signed(t.delta_glyph_id().to_i32()) & 0xffffff)
                .collect(),
            SingleSubst::Format4(t) => t
                .substitute_glyph_ids()
                .iter()
                .map(|g| g.get().to_u32())
                .collect(),
        }
    }

    #[test]
    fn single_subst_serialization_selects_formats_and_wraps_deltas() {
        for (glyphs, outputs, format) in [
            (vec![1, 2], vec![2, 3], 1),
            (vec![0, 1], vec![65535, 0], 1),
            (vec![1, 2], vec![3, 5], 2),
            (vec![65535, 65536], vec![65536, 65537], 3),
            (vec![0, 1], vec![0xffffff, 0], 3),
            (vec![65535, 65536], vec![70000, 65537], 4),
            ((0..=65535).collect(), (1..=65536).collect(), 3),
        ] {
            let bytes = serialize(&glyphs, &outputs);
            let table = SingleSubst::read(FontData::new(&bytes)).unwrap();
            assert_eq!(table.subst_format(), format);
            assert_eq!(substitutes(&table), outputs);
        }
        let glyphs: Vec<_> = (0..=65535).collect();
        let mut outputs = glyphs.clone();
        outputs[0] = 1;
        let bytes = serialize(&glyphs, &outputs);
        let table = SingleSubst::read(FontData::new(&bytes)).unwrap();
        assert_eq!(table.subst_format(), 4);
        assert_eq!(substitutes(&table), outputs);
    }

    #[test]
    fn single_subst_wide_subsetting_remaps_and_selects_formats() {
        let inputs = [
            BeBuffer::new()
                .push(3u16)
                .push(9u32)
                .push(Int24::new(1))
                .push(3u16)
                .push(Uint24::new(2))
                .extend([65535, 70000].map(GlyphId24::new))
                .to_vec(),
            BeBuffer::new()
                .push(4u16)
                .push(15u32)
                .push(Uint24::new(2))
                .extend([65536, 70001].map(GlyphId24::new))
                .push(3u16)
                .push(Uint24::new(2))
                .extend([65535, 70000].map(GlyphId24::new))
                .to_vec(),
        ];
        let font = FontRef::new(font_test_data::NOTOSERIFHEBREW_AUTOHINT_METRICS).unwrap();
        for bytes in &inputs {
            let table = SingleSubst::read(FontData::new(bytes)).unwrap();
            for (mapped, format) in [([1, 2, 3, 4], 1), ([65535, 65536, 70000, 70002], 4)] {
                let mut plan = Plan {
                    glyph_map_gsub: vec![crate::INVALID_GID; 70002],
                    ..Default::default()
                };
                for (old, new) in [65535, 65536, 70000, 70001].into_iter().zip(mapped) {
                    plan.glyphset_gsub.insert(GlyphId::new(old));
                    plan.glyph_map_gsub[old as usize] = GlyphId::new(new);
                }
                let mut s = Serializer::new(1024);
                s.start_serialize().unwrap();
                table
                    .subset(
                        &plan,
                        &mut s,
                        (&SubsetState::default(), &font, &plan.gsub_lookups),
                    )
                    .unwrap();
                s.end_serialize();
                let subset_bytes = s.copy_bytes();
                let subset = SingleSubst::read(FontData::new(&subset_bytes)).unwrap();
                assert_eq!(subset.subst_format(), format);
                assert_eq!(substitutes(&subset), [mapped[1], mapped[3]]);
            }
        }
    }

    #[test]
    fn single_subst4_subsetting_uses_full_width_coverage_index() {
        let count = 65537u32;
        let input = BeBuffer::new()
            .push(4u16)
            .push(9 + 3 * count)
            .push(Uint24::new(count))
            .extend((0..count).map(|gid| GlyphId24::new(if gid == 65536 { 70000 } else { 0 })))
            .push(4u16)
            .push(Uint24::new(1))
            .extend([0, 65536, 0].map(Uint24::new));
        let table = SingleSubstFormat4::read(FontData::new(&input)).unwrap();
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 70001],
            ..Default::default()
        };
        plan.glyphset_gsub.insert(GlyphId::new(65536));
        plan.glyph_map_gsub[65536] = GlyphId::new(1);
        plan.glyph_map_gsub[70000] = GlyphId::new(2);
        let mut s = Serializer::new(1024);
        s.start_serialize().unwrap();
        table.subset(&plan, &mut s, ()).unwrap();
        s.end_serialize();
        let bytes = s.copy_bytes();
        assert_eq!(
            substitutes(&SingleSubst::read(FontData::new(&bytes)).unwrap()),
            [2]
        );
    }

    #[test]
    fn single_subst_legacy_negative_delta_wraps_before_remapping() {
        let input = BeBuffer::new()
            .push(1u16)
            .push(6u16)
            .push(-1i16)
            .push(1u16)
            .push(1u16)
            .push(0u16);
        let table = SingleSubstFormat1::read(FontData::new(&input)).unwrap();
        let mut plan = Plan {
            glyph_map_gsub: vec![crate::INVALID_GID; 65536],
            ..Default::default()
        };
        plan.glyphset_gsub.insert(GlyphId::new(0));
        plan.glyph_map_gsub[0] = GlyphId::new(0);
        plan.glyph_map_gsub[65535] = GlyphId::new(1);
        let mut s = Serializer::new(1024);
        s.start_serialize().unwrap();
        table.subset(&plan, &mut s, ()).unwrap();
        s.end_serialize();
        let bytes = s.copy_bytes();
        assert_eq!(
            substitutes(&SingleSubst::read(FontData::new(&bytes)).unwrap()),
            [1]
        );
    }

    #[test]
    fn single_subst_serialization_rejects_empty_and_overflow() {
        for (glyphs, expected) in [
            (vec![], SerializeErrorFlags::SERIALIZE_ERROR_EMPTY),
            (
                vec![GlyphId::new(0x1000000)],
                SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW,
            ),
        ] {
            let mut s = Serializer::new(1024);
            s.start_serialize().unwrap();
            assert_eq!(
                SingleSubst::serialize(&mut s, (&glyphs, &glyphs)),
                Err(expected)
            );
        }
    }
}

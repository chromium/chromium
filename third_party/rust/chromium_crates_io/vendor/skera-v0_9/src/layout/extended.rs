//! Subsetting support for the ISO OFF extended layout formats.

use super::*;

pub(super) fn subset_coverage(
    coverage: &CoverageTable,
    plan: &Plan,
    s: &mut Serializer,
) -> Result<(), SerializeErrorFlags> {
    let glyph_set = &plan.glyphset_gsub;
    let retained_glyphs: Vec<_> =
        if coverage.population() as u64 > glyph_set.len() * coverage.cost() as u64 {
            glyph_set
                .iter()
                .filter(|&gid| coverage.get(gid).is_some())
                .filter_map(|gid| map_gsub_glyph(&plan.glyph_map_gsub, gid))
                .collect()
        } else {
            coverage
                .iter()
                .filter_map(|gid| map_gsub_glyph(&plan.glyph_map_gsub, gid))
                .collect()
        };
    if retained_glyphs.is_empty() {
        return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
    }
    CoverageTable::serialize(s, &retained_glyphs)
}

impl<'a> Serialize<'a> for CoverageFormat3<'a> {
    type Args = &'a [GlyphId];
    fn serialize(s: &mut Serializer, glyphs: &[GlyphId]) -> Result<(), SerializeErrorFlags> {
        let count = u32::try_from(glyphs.len())
            .ok()
            .and_then(Uint24::checked_new)
            .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
        s.embed(3_u16)?;
        s.embed(count)?;
        for gid in glyphs {
            let gid = GlyphId24::checked_new(gid.to_u32())
                .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
            s.embed(gid)?;
        }
        Ok(())
    }
}

impl<'a> Serialize<'a> for CoverageFormat4<'a> {
    type Args = (&'a [GlyphId], usize);
    fn serialize(
        s: &mut Serializer,
        (glyphs, range_count): Self::Args,
    ) -> Result<(), SerializeErrorFlags> {
        let count = u32::try_from(range_count)
            .ok()
            .and_then(Uint24::checked_new)
            .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
        s.embed(4_u16)?;
        s.embed(count)?;
        let Some(first_glyph) = glyphs.first() else {
            return Ok(());
        };
        let mut first = first_glyph.to_u32();
        let mut last = first;
        let mut coverage_index = 0;
        for (index, gid) in glyphs.iter().enumerate().skip(1) {
            let gid = gid.to_u32();
            if last.checked_add(1) != Some(gid) {
                serialize_coverage_range24(s, first, last, coverage_index)?;
                first = gid;
                coverage_index = index;
            }
            last = gid;
        }
        serialize_coverage_range24(s, first, last, coverage_index)
    }
}

fn serialize_coverage_range24(
    s: &mut Serializer,
    first: u32,
    last: u32,
    coverage_index: usize,
) -> Result<(), SerializeErrorFlags> {
    for value in [Some(first), Some(last), u32::try_from(coverage_index).ok()] {
        let value = value
            .and_then(Uint24::checked_new)
            .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
        s.embed(value)?;
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use font_test_data::bebuffer::BeBuffer;

    fn serialize_coverage(glyphs: &[GlyphId]) -> Vec<u8> {
        let mut s = Serializer::new(glyphs.len() * 9 + 32);
        s.start_serialize().unwrap();
        CoverageTable::serialize(&mut s, glyphs).unwrap();
        assert!(!s.in_error());
        s.end_serialize();
        s.copy_bytes()
    }

    #[test]
    fn coverage_serialization_selects_narrow_and_wide_formats() {
        for (glyphs, format) in [
            (vec![], 1),
            (vec![65535], 1),
            ((65532..=65535).collect(), 2),
            (vec![65535, 65536, 0xffffff], 3),
            ((65534..=65537).collect(), 4),
            ((0..=65535).collect(), 2),
            ((1..=65536).chain([70000]).collect(), 4),
        ] {
            let glyphs: Vec<_> = glyphs.into_iter().map(GlyphId::new).collect();
            let bytes = serialize_coverage(&glyphs);
            let coverage = CoverageTable::read(FontData::new(&bytes)).unwrap();
            assert_eq!(coverage.coverage_format(), format);
            assert_eq!(coverage.iter().collect::<Vec<_>>(), glyphs);
            for (index, &gid) in glyphs.iter().enumerate() {
                assert_eq!(coverage.get(gid), Some(index as u32));
            }
        }
    }

    #[test]
    fn extended_coverage_subsetting_remaps_and_downgrades() {
        let inputs = [
            BeBuffer::new()
                .push(3u16)
                .push(Uint24::new(3))
                .extend([65535, 65536, 70000].map(GlyphId24::new))
                .to_vec(),
            BeBuffer::new()
                .push(4u16)
                .push(Uint24::new(2))
                .extend([65535, 65536, 0, 70000, 70000, 2].map(Uint24::new))
                .to_vec(),
        ];
        for bytes in &inputs {
            let coverage = CoverageTable::read(FontData::new(bytes)).unwrap();
            for new_glyphs in [[1, 2], [65536, 70000]] {
                let mut plan = Plan {
                    font_num_glyphs: 70001,
                    ..Default::default()
                };
                plan.glyph_map_gsub = vec![INVALID_GID; 70001];
                for (old, new) in [65536, 70000].into_iter().zip(new_glyphs) {
                    plan.glyphset_gsub.insert(GlyphId::new(old));
                    plan.glyph_map_gsub[old as usize] = GlyphId::new(new);
                }
                let mut s = Serializer::new(1024);
                s.start_serialize().unwrap();
                coverage.subset(&plan, &mut s, ()).unwrap();
                s.end_serialize();
                let subset_bytes = s.copy_bytes();
                let subset = CoverageTable::read(FontData::new(&subset_bytes)).unwrap();
                assert_eq!(
                    subset.iter().collect::<Vec<_>>(),
                    new_glyphs.map(GlyphId::new)
                );
                assert_eq!(
                    subset.coverage_format(),
                    if new_glyphs[1] <= 65535 { 1 } else { 3 }
                );
            }
            let mut s = Serializer::new(1024);
            s.start_serialize().unwrap();
            assert_eq!(
                coverage.subset(&Plan::default(), &mut s, ()),
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
            );
        }
    }

    #[test]
    fn coverage_serialization_rejects_24bit_overflow() {
        let mut s = Serializer::new(1024);
        s.start_serialize().unwrap();
        assert_eq!(
            CoverageTable::serialize(&mut s, &[GlyphId::new(0x1000000)]),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW)
        );
        assert!(s.in_error());
    }
}

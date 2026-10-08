//! impl subset() for sbix
use crate::serialize::{OffsetWhence, SerializeErrorFlags, Serializer};
use crate::{Plan, Subset, SubsetError, SubsetTable};
use skrifa::GlyphId;
use write_fonts::types::FixedSize;
use write_fonts::{
    read::{
        tables::sbix::{Sbix, Strike},
        types::Offset32,
        ArrayOfOffsets, FontRef, MinByteRange, ReadError, TopLevelTable,
    },
    FontBuilder,
};

// reference: subset() for sbix in harfbuzz
// <https://github.com/harfbuzz/harfbuzz/blob/4df11621cecf6cf855e9e13f6f5c9432748f9b3a/src/OT/Color/sbix/sbix.hh#L420>
impl Subset for Sbix<'_> {
    fn subset(
        &self,
        plan: &Plan,
        _font: &FontRef,
        s: &mut Serializer,
        _builder: &mut FontBuilder,
    ) -> Result<(), SubsetError> {
        serialize_header(self, s).map_err(|_| SubsetError::SubsetTableError(Sbix::TAG))?;
        self.strikes()
            .subset(plan, s, ())
            .map_err(|_| SubsetError::SubsetTableError(Sbix::TAG))
    }
}

fn serialize_header(sbix: &Sbix, s: &mut Serializer) -> Result<(), SerializeErrorFlags> {
    s.embed(sbix.version())?;
    s.embed(sbix.flags()).map(|_| ())
}

impl<'a> SubsetTable<'a> for ArrayOfOffsets<'a, Strike<'a>, Offset32> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let num_strikes_pos = s.embed(0_u32)?;
        let orig_num = self.len();

        // subset offset array in reverse order
        // ref: <https://github.com/harfbuzz/harfbuzz/blob/6d8035a99c279e32183ad063f0de201ef1b2f05c/src/OT/Color/sbix/sbix.hh#L385>
        let mut obj_idxes = Vec::with_capacity(orig_num);
        let mut offset_positions = Vec::with_capacity(orig_num);

        for idx in (0..orig_num).rev() {
            let t = match self.get(idx) {
                Err(ReadError::NullOffset) => continue,
                other => other,
            }
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
            let snap = s.snapshot();
            let offset_pos = s.allocate_size(Offset32::RAW_BYTE_LEN, true)?;

            s.push()?;
            match t.subset(plan, s, ()) {
                Ok(()) => {
                    let Some(obj_idx) = s.pop_pack(true) else {
                        return Err(s.error());
                    };
                    obj_idxes.push(obj_idx);
                    offset_positions.push(offset_pos);
                }
                Err(_) => {
                    s.pop_discard();
                    s.revert_snapshot(snap);
                }
            }
        }

        let num = offset_positions.len();
        for (i, pos) in offset_positions.iter().enumerate() {
            let obj_idx = obj_idxes[num - i - 1];
            s.add_link(
                *pos..*pos + Offset32::RAW_BYTE_LEN,
                obj_idx,
                OffsetWhence::Head,
                0,
                false,
            )?;
        }

        s.check_assign::<u32>(
            num_strikes_pos,
            num,
            SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW,
        )
    }
}

impl SubsetTable<'_> for Strike<'_> {
    type ArgsForSubset = ();
    type Output = ();

    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let snap = s.snapshot();
        s.embed(self.ppem())?;
        s.embed(self.ppi())?;

        let offsets_array_len = 4 * (plan.num_output_glyphs + 1);
        let mut pos = s.allocate_size(offsets_array_len, true)?;

        // header size = 4 (ppem + ppi)
        let mut offset = 4 + offsets_array_len;

        s.check_assign::<u32>(
            pos,
            offset,
            SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW,
        )?;
        pos += 4;

        let mut has_glyphs = false;
        for new_gid in 0..plan.num_output_glyphs as u32 {
            let old_gid = plan.reverse_glyph_map.get(&GlyphId::new(new_gid));
            if old_gid.is_none() {
                s.copy_assign(pos, offset as u32);
                pos += 4;
                continue;
            }

            let Ok(Some(glyph_data)) = self.glyph_data(*old_gid.unwrap()) else {
                s.copy_assign(pos, offset as u32);
                pos += 4;
                continue;
            };

            s.embed_bytes(glyph_data.min_table_bytes())?;
            offset += glyph_data.min_byte_range().end;
            s.check_assign::<u32>(
                pos,
                offset,
                SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW,
            )?;
            pos += 4;

            if !has_glyphs {
                has_glyphs = true;
            }
        }

        if !has_glyphs {
            s.revert_snapshot(snap);
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        Ok(())
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use skrifa::raw::TableProvider;
    #[test]
    fn test_subset_sbix_noop() {
        let ttf: &[u8] = include_bytes!("../test-data/fonts/sbix.ttf");
        let font = FontRef::new(ttf).unwrap();
        let sbix = font.sbix().unwrap();

        let mut plan = Plan {
            num_output_glyphs: 3,
            ..Default::default()
        };
        plan.reverse_glyph_map
            .insert(GlyphId::NOTDEF, GlyphId::NOTDEF);
        plan.reverse_glyph_map
            .insert(GlyphId::new(1), GlyphId::new(1));
        plan.reverse_glyph_map
            .insert(GlyphId::new(2), GlyphId::new(2));

        let mut builder = FontBuilder::default();
        let mut s = Serializer::new(200000);

        assert_eq!(s.start_serialize(), Ok(()));
        let ret = sbix.subset(&plan, &font, &mut s, &mut builder);
        assert!(ret.is_ok());
        s.end_serialize();
        assert!(!s.in_error());
        let subsetted_bytes = s.copy_bytes();
        assert_eq!(subsetted_bytes.len(), sbix.offset_data().len());
    }
}

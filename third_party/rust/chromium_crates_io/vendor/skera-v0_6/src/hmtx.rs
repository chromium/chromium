//! impl subset() for hmtx

use crate::serialize::Serializer;
use crate::{Plan, Subset, SubsetError, SubsetError::SubsetTableError};
use write_fonts::types::{FWord, GlyphId, UfWord};
use write_fonts::{
    read::{
        tables::{hhea::Hhea, hmtx::Hmtx},
        FontRef, TableProvider, TopLevelTable,
    },
    FontBuilder,
};

// reference: subset() for hmtx/hhea in harfbuzz
// https://github.com/harfbuzz/harfbuzz/blob/a070f9ebbe88dc71b248af9731dd49ec93f4e6e6/src/hb-ot-hmtx-table.hh#L214
impl Subset for Hmtx<'_> {
    fn subset(
        &self,
        plan: &Plan,
        font: &FontRef,
        s: &mut Serializer,
        builder: &mut FontBuilder,
    ) -> Result<(), SubsetError> {
        let h_metrics = self.h_metrics();
        let side_bearings = self.left_side_bearings();

        let last_gid = plan.num_output_glyphs - 1;
        if last_gid >= h_metrics.len() + side_bearings.len() {
            return Err(SubsetTableError(Hmtx::TAG));
        }

        let new_num_h_metrics = compute_new_num_h_metrics(self, plan);
        //subsetted hmtx table length
        let hmtx_cap = new_num_h_metrics * 4 + (plan.num_output_glyphs - new_num_h_metrics) * 2;
        s.allocate_size(hmtx_cap, false)
            .map_err(|_| SubsetError::SubsetTableError(Hmtx::TAG))?;

        for (new_gid, old_gid) in &plan.new_to_old_gid_list {
            let new_gid = new_gid.to_u32() as usize;
            if new_gid < new_num_h_metrics {
                let idx = 4 * new_gid;
                let advance = UfWord::from(self.advance(*old_gid).unwrap_or(0));
                s.copy_assign(idx, advance);

                let lsb = FWord::from(self.side_bearing(*old_gid).unwrap_or(0));
                s.copy_assign(idx + 2, lsb);
            } else {
                let idx = 4 * new_num_h_metrics + (new_gid - new_num_h_metrics) * 2;
                let lsb = FWord::from(self.side_bearing(*old_gid).unwrap_or(0));
                s.copy_assign(idx, lsb);
            }
        }

        let Ok(hhea) = font.hhea() else {
            return Ok(());
        };

        let mut hhea_out = hhea.offset_data().as_bytes().to_owned();
        let new_num_h_metrics = (new_num_h_metrics as u16).to_be_bytes();
        hhea_out
            .get_mut(34..36)
            .unwrap()
            .copy_from_slice(&new_num_h_metrics);

        builder.add_raw(Hhea::TAG, hhea_out);
        Ok(())
    }
}

fn compute_new_num_h_metrics(hmtx: &Hmtx, plan: &Plan) -> usize {
    let mut num_long_metrics = plan.num_output_glyphs.min(0xFFFF);
    let last_advance = get_new_gid_advance(hmtx, GlyphId::from(num_long_metrics as u32 - 1), plan);

    while num_long_metrics > 1 {
        let advance = get_new_gid_advance(hmtx, GlyphId::from(num_long_metrics as u32 - 2), plan);
        if advance != last_advance {
            break;
        }
        num_long_metrics -= 1;
    }
    num_long_metrics
}

fn get_new_gid_advance(hmtx: &Hmtx, new_gid: GlyphId, plan: &Plan) -> u16 {
    let Some(old_gid) = plan.reverse_glyph_map.get(&new_gid) else {
        return 0;
    };
    hmtx.advance(*old_gid).unwrap_or(0)
}

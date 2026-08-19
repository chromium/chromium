//! impl subset() for vmtx

use crate::serialize::Serializer;
use crate::{Plan, Subset, SubsetError, SubsetError::SubsetTableError};
use write_fonts::types::{FWord, GlyphId, UfWord};
use write_fonts::{
    read::{
        tables::{vhea::Vhea, vmtx::Vmtx},
        FontRef, TableProvider, TopLevelTable,
    },
    FontBuilder,
};

// reference: subset() for vmtx/vhea in harfbuzz
// https://github.com/harfbuzz/harfbuzz/blob/a070f9ebbe88dc71b248af9731dd49ec93f4e6e6/src/hb-ot-hmtx-table.hh#L214
impl Subset for Vmtx<'_> {
    fn subset(
        &self,
        plan: &Plan,
        font: &FontRef,
        s: &mut Serializer,
        builder: &mut FontBuilder,
    ) -> Result<(), SubsetError> {
        let v_metrics = self.v_metrics();
        let side_bearings = self.top_side_bearings();

        let last_gid = plan.num_output_glyphs - 1;
        if last_gid >= v_metrics.len() + side_bearings.len() {
            return Err(SubsetTableError(Vmtx::TAG));
        }

        let new_num_v_metrics = compute_new_num_v_metrics(self, plan);
        //subsetted vmtx table length
        let vmtx_cap = new_num_v_metrics * 4 + (plan.num_output_glyphs - new_num_v_metrics) * 2;
        s.allocate_size(vmtx_cap, false)
            .map_err(|_| SubsetError::SubsetTableError(Vmtx::TAG))?;

        for (new_gid, old_gid) in &plan.new_to_old_gid_list {
            let new_gid = new_gid.to_u32() as usize;
            if new_gid < new_num_v_metrics {
                let idx = 4 * new_gid;
                let advance = UfWord::from(self.advance(*old_gid).unwrap_or(0));
                s.copy_assign(idx, advance);

                let tsb = FWord::from(self.side_bearing(*old_gid).unwrap_or(0));
                s.copy_assign(idx + 2, tsb);
            } else {
                let idx = 4 * new_num_v_metrics + (new_gid - new_num_v_metrics) * 2;
                let tsb = FWord::from(self.side_bearing(*old_gid).unwrap_or(0));
                s.copy_assign(idx, tsb);
            }
        }

        let Ok(vhea) = font.vhea() else {
            return Ok(());
        };

        let mut vhea_out = vhea.offset_data().as_bytes().to_owned();
        let new_num_v_metrics = (new_num_v_metrics as u16).to_be_bytes();
        vhea_out
            .get_mut(34..36)
            .unwrap()
            .copy_from_slice(&new_num_v_metrics);

        builder.add_raw(Vhea::TAG, vhea_out);
        Ok(())
    }
}

fn compute_new_num_v_metrics(vmtx: &Vmtx, plan: &Plan) -> usize {
    let mut num_long_metrics = plan.num_output_glyphs.min(0xFFFF);
    let last_advance = get_new_gid_advance(vmtx, GlyphId::from(num_long_metrics as u32 - 1), plan);

    while num_long_metrics > 1 {
        let advance = get_new_gid_advance(vmtx, GlyphId::from(num_long_metrics as u32 - 2), plan);
        if advance != last_advance {
            break;
        }
        num_long_metrics -= 1;
    }
    num_long_metrics
}

fn get_new_gid_advance(vmtx: &Vmtx, new_gid: GlyphId, plan: &Plan) -> u16 {
    let Some(old_gid) = plan.reverse_glyph_map.get(&new_gid) else {
        return 0;
    };
    vmtx.advance(*old_gid).unwrap_or(0)
}

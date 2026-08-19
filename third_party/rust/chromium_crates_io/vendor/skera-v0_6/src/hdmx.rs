//! impl subset() for hdmx

use crate::serialize::{SerializeErrorFlags, Serializer};
use crate::{Plan, Subset, SubsetError};
use write_fonts::{
    read::{
        tables::hdmx::{DeviceRecord, Hdmx},
        FontRef, TopLevelTable,
    },
    FontBuilder,
};

fn ceil_to_4(v: u32) -> u32 {
    ((v - 1) | 3) + 1
}

// reference: subset() for hmtx/hhea in harfbuzz
// <https://github.com/harfbuzz/harfbuzz/blob/e451e91ec3608a2ebfec34d0c4f0b3d880e00e33/src/hb-ot-hdmx-table.hh#L116>
impl Subset for Hdmx<'_> {
    fn subset(
        &self,
        plan: &Plan,
        _font: &FontRef,
        s: &mut Serializer,
        _builder: &mut FontBuilder,
    ) -> Result<(), SubsetError> {
        s.embed(self.version())
            .map_err(|_| SubsetError::SubsetTableError(Hdmx::TAG))?;
        s.embed(self.num_records())
            .map_err(|_| SubsetError::SubsetTableError(Hdmx::TAG))?;

        let size_device_record = ceil_to_4(2 + plan.num_output_glyphs as u32);
        s.embed(size_device_record)
            .map_err(|_| SubsetError::SubsetTableError(Hdmx::TAG))?;

        for record in self.records().iter() {
            let Ok(r) = record else {
                return Err(SubsetError::SubsetTableError(Hdmx::TAG));
            };
            serialize_device_record(&r, s, plan, size_device_record as usize)
                .map_err(|_| SubsetError::SubsetTableError(Hdmx::TAG))?;
        }
        Ok(())
    }
}

fn serialize_device_record(
    record: &DeviceRecord,
    s: &mut Serializer,
    plan: &Plan,
    size_device_record: usize,
) -> Result<(), SerializeErrorFlags> {
    s.embed(record.pixel_size())?;
    let max_width_pos = s.embed(0_u8)?;
    let widths_array_pos = s.allocate_size(size_device_record - 2, false)?;
    let mut max_width = 0;
    for (new_gid, old_gid) in plan.new_to_old_gid_list.iter() {
        let old_idx = old_gid.to_u32() as usize;
        let Some(wdth) = record.widths().get(old_idx) else {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
        };

        let new_idx = new_gid.to_u32() as usize;
        s.copy_assign(widths_array_pos + new_idx, *wdth);
        max_width = max_width.max(*wdth);
    }

    s.copy_assign(max_width_pos, max_width);
    Ok(())
}

//! impl subset() for CPAL table

use crate::fnv::FnvHashMap;
use crate::{
    offset::{SerializeCopy, SerializeSubset},
    serialize::{SerializeErrorFlags, Serializer},
    NameIdClosure, Plan, Subset, SubsetError, SubsetTable,
};
use write_fonts::{
    read::{
        collections::IntSet,
        tables::cpal::{ColorRecord, Cpal},
        types::{BigEndian, NameId},
        FontRef, Offset, TopLevelTable,
    },
    types::{FixedSize, Offset32},
    FontBuilder,
};

// reference: subset() for CPAL in Harfbuzz:
// <https://github.com/harfbuzz/harfbuzz/blob/3c02fcd0e8ebd9330634058839941a672f777ac3/src/OT/Color/CPAL/CPAL.hh#L277>
impl Subset for Cpal<'_> {
    fn subset(
        &self,
        plan: &Plan,
        _font: &FontRef,
        s: &mut Serializer,
        _builder: &mut FontBuilder,
    ) -> Result<(), SubsetError> {
        subset_v0(self, plan, s).map_err(|_| SubsetError::SubsetTableError(Cpal::TAG))?;
        if self.version() == 1 {
            subset_v1(self, plan, s).map_err(|_| SubsetError::SubsetTableError(Cpal::TAG))?;
        }
        Ok(())
    }
}

fn subset_v0(cpal: &Cpal, plan: &Plan, s: &mut Serializer) -> Result<(), SerializeErrorFlags> {
    let colr_index_map = &plan.colr_palettes;
    if colr_index_map.is_empty() || cpal.num_palettes() == 0 {
        return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
    }

    let retained_entries: IntSet<u16> = colr_index_map
        .keys()
        .filter(|&n| *n != 0xFFFF)
        .copied()
        .collect();
    if retained_entries.is_empty() {
        return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
    }

    s.embed(cpal.version())?;

    let num_colors = retained_entries.len() as u16;
    s.embed(num_colors)?;

    s.embed(cpal.num_palettes())?;
    //numColorRecords, initialized to 0
    let num_color_records_pos = s.embed(0_u16)?;

    //colorRecordsArrayOffset
    let color_records_arr_offset_pos = s.embed(0_u32)?;

    let Some(Ok(color_records)) = cpal.color_records_array() else {
        return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
    };

    let color_record_indices = cpal.color_record_indices();
    let first_record_idx_map = Offset32::serialize_subset(
        &color_records,
        s,
        plan,
        (color_record_indices, &retained_entries),
        color_records_arr_offset_pos,
    )?;

    let num_color_records = (first_record_idx_map.len() as u16) * num_colors;
    s.copy_assign(num_color_records_pos, num_color_records);

    //colorRecordIndices
    for idx in color_record_indices {
        let new_idx = first_record_idx_map.get(&idx.get()).unwrap();
        s.embed(*new_idx)?;
    }
    Ok(())
}

fn subset_v1(cpal: &Cpal, plan: &Plan, s: &mut Serializer) -> Result<(), SerializeErrorFlags> {
    let palette_types_offset_pos = s.embed(0_u32)?;
    let palette_labels_offset_pos = s.embed(0_u32)?;
    let palette_entry_labels_offset_pos = s.embed(0_u32)?;

    let num_palettes = cpal.num_palettes();
    // v1 must have this field
    let Some(palette_types_offset) = cpal.palette_types_array_offset() else {
        return Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR);
    };

    let palette_types_offset = palette_types_offset.offset().to_usize();
    if palette_types_offset != 0 {
        //size of PaletteType is 4
        let bytes_len = (num_palettes as usize) * 4;
        let src_bytes = cpal
            .offset_data()
            .as_bytes()
            .get(palette_types_offset..palette_types_offset + bytes_len)
            .ok_or(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)?;

        Offset32::serialize_copy_from_bytes(src_bytes, s, palette_types_offset_pos)?;
    }

    // v1 must have this field
    let Some(palette_labels_offset) = cpal.palette_labels_array_offset() else {
        return Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR);
    };
    let palette_labels_offset = palette_labels_offset.offset().to_usize();
    if palette_labels_offset != 0 {
        let bytes_len = (num_palettes as usize) * NameId::RAW_BYTE_LEN;
        let src_bytes = cpal
            .offset_data()
            .as_bytes()
            .get(palette_labels_offset..palette_labels_offset + bytes_len)
            .ok_or(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)?;

        Offset32::serialize_copy_from_bytes(src_bytes, s, palette_labels_offset_pos)?;
    }

    if let Some(palette_entry_labels) = cpal
        .palette_entry_labels_array()
        .transpose()
        .map_err(|_| SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)?
    {
        Offset32::serialize_subset(
            &palette_entry_labels,
            s,
            plan,
            (),
            palette_entry_labels_offset_pos,
        )?;
    }

    Ok(())
}

impl<'a> SubsetTable<'a> for &'a [ColorRecord] {
    // color_record_indices, num_palette_entries
    type ArgsForSubset = (&'a [BigEndian<u16>], &'a IntSet<u16>);
    type Output = FnvHashMap<u16, u16>;
    fn subset(
        &self,
        _plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        let (color_record_indices, retained_entries) = args;
        let num_palette_entries = retained_entries.len() as u16;

        let mut new_idx = 0_u16;
        let mut first_record_idx_map = FnvHashMap::default();
        for first_record_idx in color_record_indices {
            let first_idx = first_record_idx.get();
            if first_record_idx_map.contains_key(&first_idx) {
                continue;
            }

            for entry_idx in retained_entries.iter() {
                let record_idx = first_idx + entry_idx;
                let Some(record) = self.get(record_idx as usize) else {
                    return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER));
                };
                s.embed(record.blue())?;
                s.embed(record.green())?;
                s.embed(record.red())?;
                s.embed(record.alpha())?;
            }
            first_record_idx_map.insert(first_idx, new_idx);
            new_idx += num_palette_entries;
        }
        Ok(first_record_idx_map)
    }
}

impl<'a> SubsetTable<'a> for &'a [BigEndian<NameId>] {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        for (entry, name_id) in self.iter().enumerate() {
            if !plan.colr_palettes.contains_key(&(entry as u16)) {
                continue;
            }
            s.embed(name_id.get())?;
        }
        Ok(())
    }
}

impl NameIdClosure for Cpal<'_> {
    fn collect_name_ids(&self, plan: &mut Plan) {
        if self.version() == 0 {
            return;
        }

        if let Some(Ok(palette_labels)) = self.palette_labels_array() {
            plan.name_ids
                .extend_unsorted(palette_labels.iter().map(|x| x.get()));
        }

        if let Some(Ok(palette_entry_labels)) = self.palette_entry_labels_array() {
            plan.name_ids.extend_unsorted(
                palette_entry_labels
                    .iter()
                    .enumerate()
                    .filter(|x| plan.colr_palettes.contains_key(&(x.0 as u16)))
                    .map(|x| x.1.get()),
            );
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use write_fonts::read::TableProvider;
    #[test]
    fn test_subset_cpal_retain_all_glyphs() {
        let ttf: &[u8] = include_bytes!("../test-data/fonts/TwemojiMozilla.subset.ttf");
        let font = FontRef::new(ttf).unwrap();
        let cpal = font.cpal().unwrap();

        let mut builder = FontBuilder::new();

        let mut plan = Plan::default();
        plan.colr_palettes.insert(2, 0);
        plan.colr_palettes.insert(11, 1);

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        let ret = cpal.subset(&plan, &font, &mut s, &mut builder);
        assert!(ret.is_ok());
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 22] = [
            0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00,
            0x44, 0x2e, 0xdd, 0xff, 0xff, 0xff, 0xff, 0xff,
        ];
        assert_eq!(subsetted_data, expected_data);
    }
}

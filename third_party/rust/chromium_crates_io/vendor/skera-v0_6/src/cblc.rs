//! impl subset() for CBLC
use crate::{
    serialize::{OffsetWhence, SerializeErrorFlags, Serializer},
    Plan, Subset, SubsetError, SubsetTable,
};
use write_fonts::{
    read::{
        collections::IntSet,
        tables::{
            bitmap::{
                BitmapSize, IndexSubtable, IndexSubtable1, IndexSubtable3, IndexSubtableList,
                IndexSubtableRecord,
            },
            cbdt::Cbdt,
            cblc::Cblc,
        },
        FontRef, MinByteRange, TableProvider, TopLevelTable,
    },
    types::{FixedSize, GlyphId, Offset32},
    FontBuilder,
};

// reference: subset() for CBLC in fonttools, Harfbuzz implementation is suboptimal
// <https://github.com/fonttools/fonttools/blob/7854669acd63be43e1ad41d0486297d8d6da325d/Lib/fontTools/subset/__init__.py#L1799>
impl Subset for Cblc<'_> {
    fn subset(
        &self,
        plan: &Plan,
        font: &FontRef,
        s: &mut Serializer,
        builder: &mut FontBuilder,
    ) -> Result<(), SubsetError> {
        let cbdt = font
            .cbdt()
            .or(Err(SubsetError::SubsetTableError(Cbdt::TAG)))?;

        s.embed(self.major_version())
            .map_err(|_| SubsetError::SubsetTableError(Cblc::TAG))?;
        s.embed(self.minor_version())
            .map_err(|_| SubsetError::SubsetTableError(Cblc::TAG))?;

        let mut num_sizes: u32 = 0;
        let num_sizes_pos = s
            .embed(num_sizes)
            .map_err(|_| SubsetError::SubsetTableError(Cblc::TAG))?;

        let bitmapsize_records = self.bitmap_sizes();
        let bitmapsize_bytes = self
            .offset_data()
            .as_bytes()
            .get(self.bitmap_sizes_byte_range())
            .unwrap();

        // cbdt out
        let mut cbdt_out = Vec::with_capacity(cbdt.offset_data().len());
        // cbdt header
        cbdt_out.extend_from_slice(cbdt.min_table_bytes());

        for (idx, bitmap_size_table) in bitmapsize_records.iter().enumerate() {
            let start = idx * BitmapSize::RAW_BYTE_LEN;
            let src_bytes = bitmapsize_bytes
                .get(start..start + BitmapSize::RAW_BYTE_LEN)
                .unwrap();
            match bitmap_size_table.subset(plan, s, (self, &cbdt, src_bytes, &mut cbdt_out)) {
                Ok(()) => num_sizes += 1,
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => continue,
                Err(_) => {
                    return Err(SubsetError::SubsetTableError(Cblc::TAG));
                }
            }
        }

        s.copy_assign(num_sizes_pos, num_sizes);
        builder.add_raw(Cbdt::TAG, cbdt_out);
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for BitmapSize {
    // (Cblc table, CBDT table, src_bitmapsize_bytes, cbdt_out)
    type ArgsForSubset = (&'a Cblc<'a>, &'a Cbdt<'a>, &'a [u8], &'a mut Vec<u8>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let (cblc, cbdt, src_bytes, cbdt_out) = args;

        if self.start_glyph_index() > plan.glyphset.last().unwrap()
            || self.end_glyph_index() < plan.glyphset.first().unwrap()
            || self.index_subtable_list_offset() == 0
            || self.index_subtable_list_size() == 0
        {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        let snap = s.snapshot();
        let offset_pos = s.embed_bytes(src_bytes)?;
        let Ok(index_subtable_list) = self.index_subtable_list(cblc.offset_data()) else {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        };

        s.push()?;
        match index_subtable_list.subset(plan, s, (cbdt, cbdt_out)) {
            Ok((min_gid, max_gid, table_list_size, num_subtables)) => {
                let Some(obj_idx) = s.pop_pack(true) else {
                    return Err(s.error());
                };
                let _ = s.add_link(
                    offset_pos..offset_pos + Offset32::RAW_BYTE_LEN,
                    obj_idx,
                    OffsetWhence::Head,
                    0,
                    false,
                );

                //update table list size, biye pos = 4
                s.copy_assign(offset_pos + 4, table_list_size as u32);
                // update number of index subtable, byte_pos = 4
                s.copy_assign(offset_pos + 8, num_subtables as u32);

                // startGlyphIndex, byte_pos = 40
                s.copy_assign(offset_pos + 40, min_gid.to_u32() as u16);

                // endGlyphIndex, byte_pos = 42
                s.copy_assign(offset_pos + 42, max_gid.to_u32() as u16);
            }
            Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => {
                s.pop_discard();
                s.revert_snapshot(snap);
                return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
            }
            Err(e) => {
                return Err(e);
            }
        }
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for IndexSubtableList<'a> {
    type ArgsForSubset = (&'a Cbdt<'a>, &'a mut Vec<u8>);
    // min_gid(new), max_gid(new), indexSubtableListSize, numberOfIndexSubtables
    type Output = (GlyphId, GlyphId, usize, usize);

    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        let records = self.index_subtable_records();
        let src_num_records = records.len();

        let mut obj_idxes = Vec::with_capacity(src_num_records);
        let mut table_list_size = 0;
        let init_len = s.length();
        // serialize subtables in reverse order
        for idx in (0..src_num_records).rev() {
            let record = records[idx];
            if record.index_subtable_offset().is_null() {
                continue;
            }
            let Ok(subtable) = record.index_subtable(self.offset_data()) else {
                return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
            };
            s.push()?;
            match subset_index_subtable(&subtable, plan, s, &record, args.0, args.1) {
                Ok((start_gid, end_gid, table_size)) => {
                    let Some(obj_idx) = s.pop_pack(true) else {
                        return Err(s.error());
                    };
                    obj_idxes.push((idx, obj_idx, start_gid, end_gid));
                    table_list_size += table_size;
                }
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => {
                    s.pop_discard();
                    continue;
                }
                Err(e) => {
                    return Err(e);
                }
            }
        }

        if obj_idxes.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        let mut min_start_gid = GlyphId::from(u16::MAX);
        let mut max_end_gid = GlyphId::NOTDEF;
        for (record_idx, objidx, start_gid, end_gid) in obj_idxes.iter().rev() {
            let record = records[*record_idx];
            record.subset(plan, s, (*objidx, *start_gid, *end_gid))?;

            min_start_gid = min_start_gid.min(*start_gid);
            max_end_gid = max_end_gid.max(*end_gid);
        }

        table_list_size += s.length() - init_len;
        Ok((min_start_gid, max_end_gid, table_list_size, obj_idxes.len()))
    }
}

impl SubsetTable<'_> for IndexSubtableRecord {
    // (obj_idx, first_glyph_id(new), last_glyph_id(new))
    type ArgsForSubset = (usize, GlyphId, GlyphId);
    type Output = ();
    fn subset(
        &self,
        _plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        s.embed(args.1.to_u32() as u16)?;
        s.embed(args.2.to_u32() as u16)?;

        let offset_pos = s.embed(0_u32)?;
        let _ = s.add_link(
            offset_pos..offset_pos + 4,
            args.0,
            OffsetWhence::Head,
            0,
            false,
        );
        Ok(())
    }
}

fn subset_index_subtable(
    table: &IndexSubtable,
    plan: &Plan,
    s: &mut Serializer,
    index_subtable_record: &IndexSubtableRecord,
    src_cbdt: &Cbdt,
    cbdt_out: &mut Vec<u8>,
) -> Result<(GlyphId, GlyphId, usize), SerializeErrorFlags> {
    match table {
        IndexSubtable::Format1(item) => {
            item.subset(plan, s, (index_subtable_record, src_cbdt, cbdt_out))
        }
        IndexSubtable::Format3(item) => {
            item.subset(plan, s, (index_subtable_record, src_cbdt, cbdt_out))
        }
        //TODO: support format 2/4/5?
        _ => Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY),
    }
}

impl<'a> SubsetTable<'a> for IndexSubtable1<'a> {
    // (src record, src CBDT, CBDT_out)
    type ArgsForSubset = (&'a IndexSubtableRecord, &'a Cbdt<'a>, &'a mut Vec<u8>);

    // output:(first_gid, end_gid, size of subsetted table)
    type Output = (GlyphId, GlyphId, usize);

    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(GlyphId, GlyphId, usize), SerializeErrorFlags> {
        let src_record = args.0;
        let src_gid_min = src_record.first_glyph_index();
        let src_gid_max = src_record.last_glyph_index();

        let mut retained_glyphs = IntSet::empty();
        retained_glyphs.insert_range(GlyphId::from(src_gid_min)..=GlyphId::from(src_gid_max));
        retained_glyphs.intersect(&plan.glyphset);

        if retained_glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        let src_offsets = self.sbit_offsets();
        let src_gid_min = src_gid_min.to_u32() as usize;
        // find the last glyph that has image_data
        let mut end_glyph = None;
        for gid in retained_glyphs.iter().rev() {
            let idx = gid.to_u32() as usize - src_gid_min;
            let offset_start = src_offsets[idx].get();
            let offset_end = src_offsets[idx + 1].get();

            if offset_end > offset_start {
                end_glyph = Some(gid);
                break;
            }
        }

        if end_glyph.is_none() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        let init_len = s.length();
        s.embed(self.index_format())?;
        s.embed(self.image_format())?;

        let cbdt_out = args.2;
        let image_offset = cbdt_out.len() as u32;
        s.embed(image_offset)?;

        let mut start_glyph = None;
        let src_cbdt = args.1;

        let mut cur_offset = 0_u32;
        let mut last = 0;

        for (new_gid, old_gid) in plan
            .new_to_old_gid_list
            .iter()
            .filter(|&(_, g)| retained_glyphs.contains(*g))
        {
            let idx = old_gid.to_u32() as usize - src_gid_min;
            let offset_start = src_offsets[idx].get();
            let offset_end = src_offsets[idx + 1].get();

            // for retain-gids
            if start_glyph.is_some() {
                while last < new_gid.to_u32() {
                    s.embed(cur_offset)?;
                    last += 1;
                }
            }

            if offset_end <= offset_start {
                if start_glyph.is_none() {
                    continue;
                } else {
                    // add skip glyph that has no image data
                    s.embed(cur_offset)?;
                }
            } else {
                if start_glyph.is_none() {
                    start_glyph = Some(*new_gid);
                    last = new_gid.to_u32();
                }
                //copy glyph image data into cbdt_out
                let src_glyph_offset = (self.image_data_offset() + offset_start) as usize;
                let len = offset_end - offset_start;

                let Some(glyph_data) = src_cbdt
                    .offset_data()
                    .as_bytes()
                    .get(src_glyph_offset..src_glyph_offset + len as usize)
                else {
                    return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
                };

                cbdt_out.extend_from_slice(glyph_data);
                // add offset for glyph
                s.embed(cur_offset)?;
                cur_offset += len;
            }
            // Skip over gid
            last += 1;

            if *old_gid == end_glyph.unwrap() {
                end_glyph = Some(*new_gid);
                s.embed(cur_offset)?;
                break;
            }
        }

        let len = s.length() - init_len;
        Ok((start_glyph.unwrap(), end_glyph.unwrap(), len))
    }
}

impl<'a> SubsetTable<'a> for IndexSubtable3<'a> {
    // (src record, src CBDT, cbdt_out)
    type ArgsForSubset = (&'a IndexSubtableRecord, &'a Cbdt<'a>, &'a mut Vec<u8>);

    // output:(first_gid, end_gid, size of subsetted table)
    type Output = (GlyphId, GlyphId, usize);
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(GlyphId, GlyphId, usize), SerializeErrorFlags> {
        let src_record = args.0;
        let src_gid_min = src_record.first_glyph_index();
        let src_gid_max = src_record.last_glyph_index();

        let mut retained_glyphs = IntSet::empty();
        retained_glyphs.insert_range(GlyphId::from(src_gid_min)..=GlyphId::from(src_gid_max));
        retained_glyphs.intersect(&plan.glyphset);

        if retained_glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        let src_offsets = self.sbit_offsets();
        let src_gid_min = src_gid_min.to_u32() as usize;
        // find the last glyph that has image_data
        let mut end_glyph = None;
        for gid in retained_glyphs.iter().rev() {
            let idx = gid.to_u32() as usize - src_gid_min;
            let offset_start = src_offsets[idx].get();
            let offset_end = src_offsets[idx + 1].get();

            if offset_end > offset_start {
                end_glyph = Some(gid);
                break;
            }
        }

        if end_glyph.is_none() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        let init_len = s.length();
        s.embed(self.index_format())?;
        s.embed(self.image_format())?;

        let cbdt_out = args.2;
        let image_offset = cbdt_out.len() as u32;
        s.embed(image_offset)?;

        let mut start_glyph = None;
        let src_cbdt = args.1;

        let mut cur_offset = 0_u16;
        let mut num_offsets: u32 = 0;
        let mut last = 0;

        for (new_gid, old_gid) in plan
            .new_to_old_gid_list
            .iter()
            .filter(|&(_, g)| retained_glyphs.contains(*g))
        {
            let idx = old_gid.to_u32() as usize - src_gid_min;
            let offset_start = src_offsets[idx].get();
            let offset_end = src_offsets[idx + 1].get();

            // for retain-gids
            if start_glyph.is_some() {
                while last < new_gid.to_u32() {
                    s.embed(cur_offset)?;
                    num_offsets += 1;
                    last += 1;
                }
            }

            if offset_end <= offset_start {
                if start_glyph.is_none() {
                    continue;
                } else {
                    // add skip glyph that has no image data
                    s.embed(cur_offset)?;
                    num_offsets += 1;
                }
            } else {
                if start_glyph.is_none() {
                    start_glyph = Some(*new_gid);
                    last = new_gid.to_u32();
                }
                //copy glyph image data into cbdt_out
                let src_glyph_offset = self.image_data_offset() as usize + offset_start as usize;
                let len = offset_end - offset_start;

                let Some(glyph_data) = src_cbdt
                    .offset_data()
                    .as_bytes()
                    .get(src_glyph_offset..src_glyph_offset + len as usize)
                else {
                    return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
                };

                cbdt_out.extend_from_slice(glyph_data);
                // add offset for glyph
                s.embed(cur_offset)?;
                cur_offset += len;
                num_offsets += 1;
            }

            // Skip over gid
            last += 1;

            if *old_gid == end_glyph.unwrap() {
                end_glyph = Some(*new_gid);
                s.embed(cur_offset)?;
                num_offsets += 1;
                break;
            }
        }
        //pad for 32-bit alignment if needed
        if num_offsets % 2 == 1 {
            s.embed(0_u16)?;
        }

        let len = s.length() - init_len;
        Ok((start_glyph.unwrap(), end_glyph.unwrap(), len))
    }
}

#[cfg(test)]
mod test {
    use super::*;
    #[test]
    fn test_subset_cbdt_noop() {
        let font = FontRef::new(include_bytes!(
            "../test-data/fonts/NotoColorEmoji.subset.ttf"
        ))
        .unwrap();

        let cblc = font.cblc().unwrap();
        let mut builder = FontBuilder::new();

        let mut plan = Plan::default();
        plan.glyphset
            .insert_range(GlyphId::NOTDEF..=GlyphId::from(5_u32));

        plan.glyph_map.insert(GlyphId::NOTDEF, GlyphId::NOTDEF);
        plan.glyph_map
            .insert(GlyphId::from(1_u32), GlyphId::from(1_u32));
        plan.glyph_map
            .insert(GlyphId::from(2_u32), GlyphId::from(2_u32));
        plan.glyph_map
            .insert(GlyphId::from(3_u32), GlyphId::from(3_u32));
        plan.glyph_map
            .insert(GlyphId::from(4_u32), GlyphId::from(4_u32));
        plan.glyph_map
            .insert(GlyphId::from(5_u32), GlyphId::from(5_u32));

        plan.new_to_old_gid_list
            .push((GlyphId::NOTDEF, GlyphId::NOTDEF));
        plan.new_to_old_gid_list
            .push((GlyphId::from(1_u32), GlyphId::from(1_u32)));
        plan.new_to_old_gid_list
            .push((GlyphId::from(2_u32), GlyphId::from(2_u32)));
        plan.new_to_old_gid_list
            .push((GlyphId::from(3_u32), GlyphId::from(3_u32)));
        plan.new_to_old_gid_list
            .push((GlyphId::from(4_u32), GlyphId::from(4_u32)));
        plan.new_to_old_gid_list
            .push((GlyphId::from(5_u32), GlyphId::from(5_u32)));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        let ret = cblc.subset(&plan, &font, &mut s, &mut builder);
        assert!(ret.is_ok());
        assert!(!s.in_error());
        s.end_serialize();

        let expected_bytes: [u8; 116] = [
            0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00,
            0x00, 0x3C, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x65, 0xE5, 0x88, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x65, 0xE5, 0x88, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x05, 0x6D, 0x6D, 0x20, 0x01,
            0x00, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 0x10, 0x00, 0x04, 0x00, 0x05, 0x00, 0x00,
            0x00, 0x28, 0x00, 0x01, 0x00, 0x11, 0x00, 0x00, 0x0E, 0xA5, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x03, 0xA6, 0x00, 0x00, 0x07, 0x50, 0x00, 0x00, 0x14, 0xB7, 0x00, 0x01,
            0x00, 0x11, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x52,
            0x00, 0x00, 0x0E, 0xA1,
        ];
        let subsetted_data = s.copy_bytes();
        assert_eq!(subsetted_data, expected_bytes);
    }

    #[test]
    fn test_subset_cbdt_keep_one() {
        let font = FontRef::new(include_bytes!(
            "../test-data/fonts/NotoColorEmoji.subset.ttf"
        ))
        .unwrap();

        let cblc = font.cblc().unwrap();
        let mut builder = FontBuilder::new();

        let mut plan = Plan::default();
        plan.glyphset.insert(GlyphId::NOTDEF);
        plan.glyphset.insert(GlyphId::from(2_u32));

        plan.glyph_map.insert(GlyphId::NOTDEF, GlyphId::NOTDEF);
        plan.glyph_map
            .insert(GlyphId::from(2_u32), GlyphId::from(1_u32));

        plan.new_to_old_gid_list
            .push((GlyphId::NOTDEF, GlyphId::NOTDEF));
        plan.new_to_old_gid_list
            .push((GlyphId::from(1_u32), GlyphId::from(2_u32)));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        let ret = cblc.subset(&plan, &font, &mut s, &mut builder);
        assert!(ret.is_ok());
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_bytes: [u8; 80] = [
            0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00,
            0x00, 0x18, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x65, 0xe5, 0x88, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x65, 0xe5, 0x88, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x6d, 0x6d, 0x20, 0x01,
            0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01, 0x00, 0x11, 0x00, 0x00,
            0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xaa,
        ];
        assert_eq!(subsetted_data, expected_bytes);
    }

    #[test]
    fn test_subset_cbdt_keep_one_last_subtable() {
        let font = FontRef::new(include_bytes!(
            "../test-data/fonts/NotoColorEmoji.subset.ttf"
        ))
        .unwrap();

        let cblc = font.cblc().unwrap();
        let mut builder = FontBuilder::new();

        let mut plan = Plan::default();
        plan.glyphset.insert(GlyphId::NOTDEF);
        plan.glyphset.insert(GlyphId::from(4_u32));

        plan.glyph_map.insert(GlyphId::NOTDEF, GlyphId::NOTDEF);
        plan.glyph_map
            .insert(GlyphId::from(4_u32), GlyphId::from(1_u32));

        plan.new_to_old_gid_list
            .push((GlyphId::NOTDEF, GlyphId::NOTDEF));
        plan.new_to_old_gid_list
            .push((GlyphId::from(1_u32), GlyphId::from(4_u32)));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        let ret = cblc.subset(&plan, &font, &mut s, &mut builder);
        assert!(ret.is_ok());
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_bytes: [u8; 80] = [
            00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00,
            0x18, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x65, 0xe5, 0x88, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x65, 0xe5, 0x88, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x6d, 0x6d, 0x20, 0x01, 0x00,
            0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01, 0x00, 0x11, 0x00, 0x00, 0x00,
            0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x52,
        ];
        assert_eq!(subsetted_data, expected_bytes);
    }

    #[test]
    fn test_subset_cbdt_keep_multiple_subtables() {
        let font = FontRef::new(include_bytes!(
            "../test-data/fonts/NotoColorEmoji.subset.multiple_size_tables.ttf"
        ))
        .unwrap();

        let cblc = font.cblc().unwrap();
        let mut builder = FontBuilder::new();

        let mut plan = Plan::default();
        plan.glyphset.insert(GlyphId::NOTDEF);
        plan.glyphset.insert(GlyphId::from(1_u32));
        plan.glyphset.insert(GlyphId::from(3_u32));
        plan.glyphset.insert(GlyphId::from(4_u32));

        plan.glyph_map.insert(GlyphId::NOTDEF, GlyphId::NOTDEF);
        plan.glyph_map
            .insert(GlyphId::from(1_u32), GlyphId::from(1_u32));
        plan.glyph_map
            .insert(GlyphId::from(3_u32), GlyphId::from(2_u32));
        plan.glyph_map
            .insert(GlyphId::from(4_u32), GlyphId::from(3_u32));

        plan.new_to_old_gid_list
            .push((GlyphId::NOTDEF, GlyphId::NOTDEF));
        plan.new_to_old_gid_list
            .push((GlyphId::from(1_u32), GlyphId::from(1_u32)));
        plan.new_to_old_gid_list
            .push((GlyphId::from(2_u32), GlyphId::from(3_u32)));
        plan.new_to_old_gid_list
            .push((GlyphId::from(3_u32), GlyphId::from(4_u32)));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        let ret = cblc.subset(&plan, &font, &mut s, &mut builder);
        assert!(ret.is_ok());
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_bytes: [u8; 208] = [
            0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x9C, 0x00, 0x00,
            0x00, 0x34, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x65, 0xE5, 0x88, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x65, 0xE5, 0x88, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0xD2, 0xD2, 0x20, 0x01,
            0x00, 0x00, 0x00, 0x68, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x00, 0x65, 0xE5, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x65, 0xE5, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
            0x00, 0x03, 0x6D, 0x6D, 0x20, 0x01, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x10,
            0x00, 0x03, 0x00, 0x03, 0x00, 0x00, 0x00, 0x24, 0x00, 0x01, 0x00, 0x11, 0x00, 0x00,
            0x21, 0xB5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xA6, 0x00, 0x00, 0x11, 0x0D,
            0x00, 0x01, 0x00, 0x11, 0x00, 0x00, 0x19, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x08, 0x52, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x10, 0x00, 0x03, 0x00, 0x03,
            0x00, 0x00, 0x00, 0x24, 0x00, 0x01, 0x00, 0x11, 0x00, 0x00, 0x08, 0x56, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x03, 0xA6, 0x00, 0x00, 0x11, 0x0D, 0x00, 0x01, 0x00, 0x11,
            0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x52,
        ];
        assert_eq!(subsetted_data, expected_bytes);
    }

    #[test]
    fn test_subset_cbdt_index_format3() {
        let font = FontRef::new(include_bytes!(
            "../test-data/fonts/NotoColorEmoji.subset.index_format3.ttf"
        ))
        .unwrap();

        let cblc = font.cblc().unwrap();
        let mut builder = FontBuilder::new();

        let mut plan = Plan::default();
        plan.glyphset.insert(GlyphId::NOTDEF);
        plan.glyphset.insert(GlyphId::from(1_u32));
        plan.glyphset.insert(GlyphId::from(3_u32));
        plan.glyphset.insert(GlyphId::from(4_u32));

        plan.glyph_map.insert(GlyphId::NOTDEF, GlyphId::NOTDEF);
        plan.glyph_map
            .insert(GlyphId::from(1_u32), GlyphId::from(1_u32));
        plan.glyph_map
            .insert(GlyphId::from(3_u32), GlyphId::from(2_u32));
        plan.glyph_map
            .insert(GlyphId::from(4_u32), GlyphId::from(3_u32));

        plan.new_to_old_gid_list
            .push((GlyphId::NOTDEF, GlyphId::NOTDEF));
        plan.new_to_old_gid_list
            .push((GlyphId::from(1_u32), GlyphId::from(1_u32)));
        plan.new_to_old_gid_list
            .push((GlyphId::from(2_u32), GlyphId::from(3_u32)));
        plan.new_to_old_gid_list
            .push((GlyphId::from(3_u32), GlyphId::from(4_u32)));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        let ret = cblc.subset(&plan, &font, &mut s, &mut builder);
        assert!(ret.is_ok());
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_bytes: [u8; 100] = [
            0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00,
            0x00, 0x2C, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x65, 0xE5, 0x88, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x65, 0xE5, 0x88, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0x6D, 0x6D, 0x20, 0x01,
            0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x10, 0x00, 0x03, 0x00, 0x03, 0x00, 0x00,
            0x00, 0x20, 0x00, 0x03, 0x00, 0x11, 0x00, 0x00, 0x08, 0x56, 0x00, 0x00, 0x03, 0xA6,
            0x11, 0x0D, 0x00, 0x00, 0x00, 0x03, 0x00, 0x11, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
            0x08, 0x52,
        ];
        assert_eq!(subsetted_data, expected_bytes);
    }
}

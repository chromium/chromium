//! impl subset() for name table
use crate::{
    serialize::{OffsetWhence, Serializer},
    Plan, Subset,
    SubsetError::{self, SubsetTableError},
    SubsetFlags,
};

use write_fonts::{
    read::{
        tables::name::{Name, NameRecord},
        FontRef, TopLevelTable,
    },
    types::FixedSize,
    FontBuilder,
};

// reference: subset() for name table in harfbuzz
// https://github.com/harfbuzz/harfbuzz/blob/a070f9ebbe88dc71b248af9731dd49ec93f4e6e6/src/OT/name/name.hh#L387
impl Subset for Name<'_> {
    fn subset(
        &self,
        plan: &Plan,
        _font: &FontRef,
        s: &mut Serializer,
        _builder: &mut FontBuilder,
    ) -> Result<(), SubsetError> {
        let name_records = self.name_record();
        //TODO: support name_table_override
        //TODO: support name table version 1
        let mut retained_name_record_idxes = name_records
            .iter()
            .enumerate()
            .filter_map(|(idx, record)| {
                if !plan.name_ids.contains(record.name_id())
                    || !plan.name_languages.contains(record.language_id())
                    || (!plan.subset_flags.contains(SubsetFlags::SUBSET_FLAGS_NAME_LEGACY)
                        && !record.is_unicode())
                {
                    return None;
                }
                Some(idx)
            })
            .collect::<Vec<_>>();

        retained_name_record_idxes.sort_unstable_by_key(|nr| {
            let nr = name_records[*nr];
            (
                nr.platform_id(),
                nr.encoding_id(),
                nr.language_id(),
                nr.name_id().to_u16(),
                nr.length(),
            )
        });

        // version
        // TODO: support version 1
        s.embed(0_u16).map_err(|_| SubsetError::SubsetTableError(Name::TAG))?;
        //count
        let count = retained_name_record_idxes.len() as u16;
        s.embed(count).map_err(|_| SubsetError::SubsetTableError(Name::TAG))?;
        //storage_offset
        let storage_offset = count * NameRecord::RAW_BYTE_LEN as u16 + 6;
        s.embed(storage_offset).map_err(|_| SubsetError::SubsetTableError(Name::TAG))?;

        serialize_name_records(self, s, &retained_name_record_idxes)
    }
}

fn serialize_name_records(
    name: &Name,
    s: &mut Serializer,
    retained_name_record_idxes: &[usize],
) -> Result<(), SubsetError> {
    let data = name.offset_data().as_bytes();
    let name_records = name.name_record();
    let name_records_bytes = data.get(name.name_record_byte_range()).unwrap();
    let storage_start = name.storage_offset() as usize;
    for idx in retained_name_record_idxes.iter() {
        let len = s.length();
        let record_pos = idx * NAME_RECORD_SIZE;
        let record_bytes = name_records_bytes
            .get(record_pos..record_pos + NAME_RECORD_SIZE)
            .ok_or(SubsetError::SubsetTableError(Name::TAG))?;
        s.embed_bytes(record_bytes).map_err(|_| SubsetError::SubsetTableError(Name::TAG))?;

        let record = name_records[*idx];
        let offset = record.string_offset().to_u32() as usize;

        // 10 is the position of offset field within a NameRecord
        let offset_pos = len + 10;
        let str_len = record.length();
        // empty name str
        if str_len == 0 {
            s.copy_assign(offset_pos, 0_u16);
            continue;
        }
        s.push().map_err(|_| SubsetError::SubsetTableError(Name::TAG))?;

        //copy string data
        let str_start = storage_start + offset;
        let str_bytes =
            data.get(str_start..str_start + str_len as usize).ok_or(SubsetTableError(Name::TAG))?;
        s.embed_bytes(str_bytes).map_err(|_| SubsetError::SubsetTableError(Name::TAG))?;
        let obj_idx = s.pop_pack(true).ok_or(SubsetError::SubsetTableError(Name::TAG))?;
        s.add_link(offset_pos..offset_pos + 2, obj_idx, OffsetWhence::Tail, 0, false)
            .map_err(|_| SubsetError::SubsetTableError(Name::TAG))?;
    }
    Ok(())
}

//NameRecord size in bytes
const NAME_RECORD_SIZE: usize = NameRecord::RAW_BYTE_LEN;

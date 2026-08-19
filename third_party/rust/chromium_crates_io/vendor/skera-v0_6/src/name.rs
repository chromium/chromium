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
                    || (!plan
                        .subset_flags
                        .contains(SubsetFlags::SUBSET_FLAGS_NAME_LEGACY)
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
        s.embed(0_u16)
            .map_err(|_| SubsetError::SubsetTableError(Name::TAG))?;
        //count
        let count = retained_name_record_idxes.len() as u16;
        s.embed(count)
            .map_err(|_| SubsetError::SubsetTableError(Name::TAG))?;
        //storage_offset
        let storage_offset = count * NameRecord::RAW_BYTE_LEN as u16 + 6;
        s.embed(storage_offset)
            .map_err(|_| SubsetError::SubsetTableError(Name::TAG))?;

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
    let name_records_bytes = data
        .get(name.name_record_byte_range())
        .ok_or(SubsetError::SubsetTableError(Name::TAG))?;
    let storage_start = name.storage_offset() as usize;
    for idx in retained_name_record_idxes.iter() {
        let len = s.length();
        let record_pos = idx * NAME_RECORD_SIZE;
        let record_bytes = name_records_bytes
            .get(record_pos..record_pos + NAME_RECORD_SIZE)
            .ok_or(SubsetError::SubsetTableError(Name::TAG))?;
        s.embed_bytes(record_bytes)
            .map_err(|_| SubsetError::SubsetTableError(Name::TAG))?;

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
        s.push()
            .map_err(|_| SubsetError::SubsetTableError(Name::TAG))?;

        //copy string data
        let str_start = storage_start + offset;
        let str_bytes = data
            .get(str_start..str_start + str_len as usize)
            .ok_or(SubsetTableError(Name::TAG))?;
        s.embed_bytes(str_bytes)
            .map_err(|_| SubsetError::SubsetTableError(Name::TAG))?;
        let obj_idx = s
            .pop_pack(true)
            .ok_or(SubsetError::SubsetTableError(Name::TAG))?;
        s.add_link(
            offset_pos..offset_pos + 2,
            obj_idx,
            OffsetWhence::Tail,
            0,
            false,
        )
        .map_err(|_| SubsetError::SubsetTableError(Name::TAG))?;
    }
    Ok(())
}

//NameRecord size in bytes
const NAME_RECORD_SIZE: usize = NameRecord::RAW_BYTE_LEN;

#[cfg(test)]
mod test {
    use super::*;
    use write_fonts::read::{types::NameId, TableProvider};
    use write_fonts::types::Tag;

    /// Returns a copy of `font_bytes` with the `count` field of the `name`
    /// table header overwritten, so that the header claims more name records
    /// than the table actually contains.
    fn font_with_bad_name_count(font_bytes: &[u8], count: u16) -> Vec<u8> {
        let mut bytes = font_bytes.to_vec();
        let font = FontRef::new(font_bytes).unwrap();
        let record = font
            .table_directory()
            .table_records()
            .iter()
            .find(|r| r.tag() == Name::TAG)
            .unwrap();
        // count is the second u16 of the name table header
        let count_pos = record.offset() as usize + 2;
        bytes[count_pos..count_pos + 2].copy_from_slice(&count.to_be_bytes());
        bytes
    }

    #[test]
    fn test_subset_name_record_count_out_of_bounds() {
        let ttf: &[u8] = include_bytes!("../test-data/fonts/Roboto-Regular.abc.ttf");
        let bytes = font_with_bad_name_count(ttf, 0xFFFF);
        let font = FontRef::new(&bytes).unwrap();
        let name = font.name().unwrap();

        let mut builder = FontBuilder::new();
        let mut plan = Plan::default();
        plan.name_ids.insert(NameId::new(1));
        plan.name_languages.insert(0x0409);

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        let ret = name.subset(&plan, &font, &mut s, &mut builder);
        assert!(matches!(
            ret,
            Err(SubsetError::SubsetTableError(tag)) if tag == Name::TAG
        ));
    }

    #[test]
    fn test_subset_font_name_record_count_out_of_bounds() {
        use write_fonts::read::collections::IntSet;

        let ttf: &[u8] = include_bytes!("../test-data/fonts/Roboto-Regular.abc.ttf");
        let bytes = font_with_bad_name_count(ttf, 0xFFFF);
        let font = FontRef::new(&bytes).unwrap();

        let mut unicodes = IntSet::<u32>::empty();
        unicodes.insert_range(0x61..=0x63);
        let mut name_ids = IntSet::<NameId>::empty();
        name_ids.insert_range(NameId::from(0)..=NameId::from(6));
        let mut name_languages = IntSet::<u16>::empty();
        name_languages.insert(0x0409);
        let mut layout_features = IntSet::empty();
        layout_features.extend_unsorted(crate::DEFAULT_LAYOUT_FEATURES.iter().copied());

        let plan = Plan::new(
            &IntSet::empty(),
            &unicodes,
            &font,
            SubsetFlags::SUBSET_FLAGS_DEFAULT,
            &IntSet::empty(),
            &IntSet::<Tag>::all(),
            &layout_features,
            &name_ids,
            &name_languages,
        );

        // subset_font used to panic here. The malformed name table is now
        // reported as a subset failure, which subset() treats as "table
        // subsetted to empty", so the table is dropped from the output.
        let out = crate::subset_font(&font, &plan).unwrap();
        let subset = FontRef::new(&out).unwrap();
        assert!(subset.table_data(Name::TAG).is_none());
    }
}

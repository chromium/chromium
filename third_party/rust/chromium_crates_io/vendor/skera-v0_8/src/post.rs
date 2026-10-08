//! impl subset() for post
use crate::fnv::FnvHashMap;
use crate::{
    serialize::{SerializeErrorFlags, Serializer},
    Plan, Subset, SubsetError, SubsetFlags,
};
use write_fonts::{
    read::{
        tables::post::{Post, DEFAULT_GLYPH_NAMES},
        FontRef, MinByteRange, TopLevelTable,
    },
    types::{BigEndian, Version16Dot16},
    FontBuilder,
};

// reference: subset() for post in harfbuzz
// https://github.com/harfbuzz/harfbuzz/blob/a070f9ebbe88dc71b248af9731dd49ec93f4e6e6/src/hb-ot-post-table.hh#L96
impl Subset for Post<'_> {
    fn subset(
        &self,
        plan: &Plan,
        _font: &FontRef,
        s: &mut Serializer,
        _builder: &mut FontBuilder,
    ) -> Result<(), SubsetError> {
        // copy header
        s.embed_bytes(self.min_table_bytes())
            .map_err(|_| SubsetError::SubsetTableError(Post::TAG))?;

        let glyph_names = plan
            .subset_flags
            .contains(SubsetFlags::SUBSET_FLAGS_GLYPH_NAMES);
        //version 3 does not have any glyph names
        if !glyph_names {
            s.copy_assign(self.version_byte_range().start, Version16Dot16::VERSION_3_0);
        }

        if glyph_names && self.version() == Version16Dot16::VERSION_2_0 {
            subset_post_v2tail(self, plan, s)
                .map_err(|_| SubsetError::SubsetTableError(Post::TAG))?;
        }
        Ok(())
    }
}

fn subset_post_v2tail(
    post: &Post,
    plan: &Plan,
    s: &mut Serializer,
) -> Result<(), SerializeErrorFlags> {
    // handle empty V2tail
    let Some(glyph_name_indices) = post.glyph_name_index() else {
        s.embed(0_u16)?;
        return Ok(());
    };

    //copy numGlyphs
    let num_output_glyphs = plan.num_output_glyphs;
    s.embed(plan.num_output_glyphs as u16)?;

    // init all glyphNameIndex as 0, which refers to name .notdef
    // this handles retain-gid holes, so we don't need to loop all retained glyphs
    let idx_start = s.allocate_size(num_output_glyphs * 2, false)?;

    let string_data_byte_range = post.string_data_byte_range();
    let (string_data, index_to_offset) = if string_data_byte_range.is_empty() {
        (None, Vec::new())
    } else {
        let str_data = post
            .offset_data()
            .as_bytes()
            .get(string_data_byte_range.start..)
            .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        (
            Some(str_data),
            index_to_offset(glyph_name_indices, num_output_glyphs, str_data),
        )
    };

    let default_name_idx_map: FnvHashMap<&[u8], u16> = if string_data_byte_range.is_empty() {
        FnvHashMap::default()
    } else {
        DEFAULT_GLYPH_NAMES
            .iter()
            .enumerate()
            .map(|(i, s)| (s.as_bytes(), i as u16))
            .collect()
    };

    let mut new_name_idx = 258_u16;
    let mut old_to_new_idx_map = FnvHashMap::default();
    let mut name_bytes_to_new_idx_map = FnvHashMap::default();

    for (new_gid, old_gid) in plan
        .new_to_old_gid_list
        .iter()
        .map(|(new, old)| (new.to_u32() as usize, old.to_u32() as usize))
    {
        let old_name_idx = glyph_name_indices
            .get(old_gid)
            .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER))?
            .get();

        let idx_pos = idx_start + new_gid * 2;
        if old_name_idx < 258 {
            s.copy_assign(idx_pos, old_name_idx);
        } else if let Some(idx) = old_to_new_idx_map.get(&old_name_idx) {
            s.copy_assign(idx_pos, *idx);
        } else {
            let Some(string_data) = string_data else {
                return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
            };

            let name_bytes = find_glyph_name(old_name_idx as usize, string_data, &index_to_offset)
                .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

            if let Some(idx) = name_bytes_to_new_idx_map.get(name_bytes) {
                s.copy_assign(idx_pos, *idx);
                old_to_new_idx_map.insert(old_name_idx, *idx);
            } else {
                let name_str = name_bytes
                    .get(1..)
                    .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
                // check if duplicate with DEFAULT names first
                if let Some(default_idx) = default_name_idx_map.get(name_str) {
                    s.copy_assign(idx_pos, *default_idx);
                    name_bytes_to_new_idx_map.insert(name_bytes, *default_idx);
                    old_to_new_idx_map.insert(old_name_idx, *default_idx);
                } else {
                    s.copy_assign(idx_pos, new_name_idx);
                    name_bytes_to_new_idx_map.insert(name_bytes, new_name_idx);
                    old_to_new_idx_map.insert(old_name_idx, new_name_idx);
                    new_name_idx += 1;

                    // copy name bytes
                    s.embed_bytes(name_bytes)?;
                }
            }
        }
    }
    Ok(())
}

fn find_glyph_name<'a>(
    idx: usize,
    string_data: &'a [u8],
    index_to_offset: &[usize],
) -> Option<&'a [u8]> {
    let offset = index_to_offset.get(idx - 258)?;
    let len = string_data.get(*offset)?;
    if *len == 0 {
        return None;
    }
    let start = *offset;
    let end = start + *len as usize + 1;
    string_data.get(start..end)
}

// get() in PString is slow, we need to precompute offset into string_data for each index
// why it's slow, ref: <https://github.com/googlefonts/fontations/blob/7c9d875992c42d0bda6d0c7f807c25222e863490/read-fonts/src/array.rs#L140>
fn index_to_offset(
    glyph_name_indices: &[BigEndian<u16>],
    num_output_glyphs: usize,
    string_data: &[u8],
) -> Vec<usize> {
    let mut index_to_offset = Vec::with_capacity(glyph_name_indices.len().min(num_output_glyphs));
    let total_len = string_data.len();
    let mut pos = 0;

    while pos < total_len && index_to_offset.len() < 65535 {
        let cur_len = string_data[pos] as usize;
        if pos + cur_len >= total_len {
            break;
        }

        index_to_offset.push(pos);
        pos += 1 + cur_len;
    }

    index_to_offset
}

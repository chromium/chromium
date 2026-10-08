//! impl Subset for OpenType font variations common tables.
use crate::fnv::FnvHashMap;
use crate::{
    inc_bimap::IncBiMap,
    offset::SerializeSubset,
    serialize::{SerializeErrorFlags, Serializer},
    Plan, Serialize, SubsetTable,
};
use write_fonts::{
    read::{
        collections::IntSet,
        tables::variations::{
            DeltaSetIndexMap, ItemVariationData, ItemVariationStore, VariationRegionList,
        },
    },
    types::{BigEndian, F2Dot14, FixedSize, Offset32},
};

impl<'a> SubsetTable<'a> for ItemVariationStore<'a> {
    type ArgsForSubset = (&'a [IncBiMap], bool);
    type Output = ();

    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let (inner_maps, keep_empty) = args;
        if !keep_empty && inner_maps.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        s.embed(self.format())?;

        if self.variation_region_list_offset().is_null() {
            if keep_empty {
                // in case of null var_region_list, set vardata count = 0 and return
                s.embed(Offset32::new(0))?;
                s.embed(0_u16)?;
                return Ok(());
            } else {
                return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
            }
        }

        let var_region_list = self
            .variation_region_list()
            .map_err(|_| SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)?;

        let var_data_array = self.item_variation_data();
        let mut region_indices = IntSet::empty();
        for (i, inner_map) in inner_maps.iter().enumerate() {
            if inner_map.len() == 0 {
                continue;
            }
            match var_data_array.get(i) {
                Some(Ok(var_data)) => {
                    collect_region_refs(&var_data, inner_map, &mut region_indices);
                }
                None => continue,
                Some(Err(_e)) => {
                    return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
                }
            }
        }

        if region_indices.is_empty() && !keep_empty {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        // varRegionList
        let max_region_count = var_region_list.region_count();
        region_indices.remove_range(max_region_count..=u16::MAX);

        let mut region_map: IncBiMap = IncBiMap::with_capacity(region_indices.len() as usize);
        for region in region_indices.iter() {
            region_map.add(region as u32);
        }

        let region_list_offset_pos = s.embed(Offset32::new(0))?;
        Offset32::serialize_subset(
            &var_region_list,
            s,
            plan,
            &region_map,
            region_list_offset_pos,
        )?;

        serialize_var_data_offset_array(self, s, plan, inner_maps, &region_map, keep_empty)
    }
}

impl<'a> SubsetTable<'a> for VariationRegionList<'a> {
    type ArgsForSubset = &'a IncBiMap;
    type Output = ();

    fn subset(
        &self,
        _plan: &Plan,
        s: &mut Serializer,
        region_map: &IncBiMap,
    ) -> Result<(), SerializeErrorFlags> {
        let axis_count = self.axis_count();
        s.embed(axis_count)?;

        let region_count = region_map.len() as u16;
        s.embed(region_count)?;

        if region_count == 0 {
            return Ok(());
        }
        //Fixed size of a VariationRegion
        let var_region_size = 3 * axis_count as usize * F2Dot14::RAW_BYTE_LEN;
        if var_region_size.checked_mul(region_count as usize).is_none() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
        }

        let subset_var_regions_size = var_region_size * region_count as usize;
        let var_regions_pos = s.allocate_size(subset_var_regions_size, false)?;

        let src_region_count = self.region_count() as u32;
        let Some(src_var_regions_bytes) = self
            .offset_data()
            .as_bytes()
            .get(self.variation_regions_byte_range())
        else {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        };

        for r in 0..region_count {
            let Some(backward) = region_map.get_backward(r as u32) else {
                return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
            };
            if *backward >= src_region_count {
                return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
            }

            let src_pos = (*backward as usize) * var_region_size;
            let Some(src_bytes) = src_var_regions_bytes.get(src_pos..src_pos + var_region_size)
            else {
                return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
            };
            s.copy_assign_from_bytes(var_regions_pos + r as usize * var_region_size, src_bytes);
        }
        Ok(())
    }
}

fn serialize_var_data_offset_array(
    var_store: &ItemVariationStore,
    s: &mut Serializer,
    plan: &Plan,
    inner_maps: &[IncBiMap],
    region_map: &IncBiMap,
    keep_empty: bool,
) -> Result<(), SerializeErrorFlags> {
    let mut vardata_count = 0_u16;
    let count_pos = s.embed(vardata_count)?;

    let var_data_array = var_store.item_variation_data();
    for (i, inner_map) in inner_maps.iter().enumerate() {
        if inner_map.len() == 0 {
            continue;
        }
        match var_data_array.get(i) {
            Some(Ok(var_data)) => {
                let offset_pos = s.embed(0_u32)?;
                Offset32::serialize_subset(
                    &var_data,
                    s,
                    plan,
                    (inner_map, region_map),
                    offset_pos,
                )?;
                vardata_count += 1;
            }
            _ => {
                return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
            }
        }
    }

    if vardata_count == 0 {
        if !keep_empty {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        } else {
            return Ok(());
        }
    }
    s.copy_assign(count_pos, vardata_count);
    Ok(())
}

impl<'a> SubsetTable<'a> for ItemVariationData<'_> {
    type ArgsForSubset = (&'a IncBiMap, &'a IncBiMap);
    type Output = ();

    fn subset(
        &self,
        _plan: &Plan,
        s: &mut Serializer,
        args: (&IncBiMap, &IncBiMap),
    ) -> Result<(), SerializeErrorFlags> {
        let (inner_map, region_map) = args;
        let new_item_count = inner_map.len() as u16;
        s.embed(new_item_count)?;

        // optimize word count
        let ri_count = self.region_index_count() as usize;

        #[derive(Clone, Copy, PartialEq)]
        enum DeltaSize {
            Zero,
            NonWord,
            Word,
        }

        let mut delta_sz = Vec::new();
        delta_sz.resize(ri_count, DeltaSize::Zero);
        // maps new index to old index
        let mut ri_map = vec![0; ri_count];

        let mut new_word_count: u16 = 0;

        let src_delta_bytes = self.delta_sets();
        let src_row_size = self.get_delta_row_len();

        let src_word_delta_count = self.word_delta_count();
        let src_word_count = (src_word_delta_count & 0x7FFF) as usize;
        let src_long_words = (src_word_delta_count & 0x8000) != 0;

        let mut has_long = false;
        if src_long_words {
            for r in 0..src_word_count {
                for item in inner_map.keys() {
                    let delta =
                        get_item_delta(self, *item as usize, r, src_row_size, src_delta_bytes);
                    if !(-65536..=65535).contains(&delta) {
                        has_long = true;
                        break;
                    }
                }
            }
        }

        let min_threshold = if has_long { -65536 } else { -128 };
        let max_threshold = if has_long { 65535 } else { 127 };

        for (r, delta_size) in delta_sz.iter_mut().enumerate() {
            let short_circuit = src_long_words == has_long && src_word_count <= r;
            for item in inner_map.keys() {
                let delta = get_item_delta(self, *item as usize, r, src_row_size, src_delta_bytes);
                if delta < min_threshold || delta > max_threshold {
                    *delta_size = DeltaSize::Word;
                    new_word_count += 1;
                    break;
                } else if delta != 0 {
                    *delta_size = DeltaSize::NonWord;
                    if short_circuit {
                        break;
                    }
                }
            }
        }

        let mut word_index: u16 = 0;
        let mut non_word_index = new_word_count;
        let mut new_ri_count: u16 = 0;

        for (r, delta_type) in delta_sz.iter().enumerate() {
            if *delta_type == DeltaSize::Zero {
                continue;
            }

            if *delta_type == DeltaSize::Word {
                let new_r = word_index as usize;
                word_index += 1;
                ri_map[new_r] = r;
            } else {
                let new_r = non_word_index as usize;
                non_word_index += 1;
                ri_map[new_r] = r;
            }
            new_ri_count += 1;
        }

        let new_word_delta_count = if has_long {
            new_word_count | 0x8000
        } else {
            new_word_count
        };
        s.embed(new_word_delta_count)?;
        s.embed(new_ri_count)?;

        let region_indices_pos = s.allocate_size(new_ri_count as usize * 2, false)?;
        let src_region_indices = self.region_indexes();
        for (idx, src_idx) in ri_map.iter().enumerate().take(new_ri_count as usize) {
            let Some(old_r) = src_region_indices.get(*src_idx) else {
                return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
            };

            let Some(region) = region_map.get(old_r.get() as u32) else {
                return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
            };
            s.copy_assign(region_indices_pos + idx * 2, *region as u16);
        }

        let new_row_size = ItemVariationData::delta_row_len(new_word_delta_count, new_ri_count);
        let new_delta_bytes_len = new_item_count as usize * new_row_size;

        let delta_bytes_pos = s.allocate_size(new_delta_bytes_len, false)?;
        for i in 0..new_item_count as usize {
            let Some(old_i) = inner_map.get_backward(i as u32) else {
                return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
            };

            let old_i = *old_i as usize;
            for (r, old_r) in ri_map.iter().enumerate().take(new_ri_count as usize) {
                set_item_delta(
                    s,
                    delta_bytes_pos,
                    i,
                    r,
                    get_item_delta(self, old_i, *old_r, src_row_size, src_delta_bytes),
                    new_row_size,
                    has_long,
                    new_word_count as usize,
                )?;
            }
        }

        Ok(())
    }
}

fn get_item_delta(
    var_data: &ItemVariationData,
    item: usize,
    region: usize,
    row_size: usize,
    delta_bytes: &[u8],
) -> i32 {
    if item >= var_data.item_count() as usize || region >= var_data.region_index_count() as usize {
        return 0;
    }

    let p = item * row_size;
    let word_delta_count = var_data.word_delta_count();
    let word_count = (word_delta_count & 0x7FFF) as usize;
    let is_long = word_delta_count & 0x8000 != 0;

    // direct port from Harfbuzz: <https://github.com/harfbuzz/harfbuzz/blob/22fbc7568828b9acfd116be44b2d77d56d2d448b/src/hb-ot-layout-common.hh#L3061>
    // ignore the lint here
    #[allow(clippy::collapsible_else_if)]
    if is_long {
        if region < word_count {
            let pos = p + region * 4;
            let Some(delta_bytes) = delta_bytes.get(pos..pos + 4) else {
                return 0;
            };
            BigEndian::<i32>::from_slice(delta_bytes).unwrap().get()
        } else {
            let pos = p + 4 * word_count + 2 * (region - word_count);
            let Some(delta_bytes) = delta_bytes.get(pos..pos + 2) else {
                return 0;
            };
            BigEndian::<i16>::from_slice(delta_bytes).unwrap().get() as i32
        }
    } else {
        if region < word_count {
            let pos = p + region * 2;
            let Some(delta_bytes) = delta_bytes.get(pos..pos + 2) else {
                return 0;
            };
            BigEndian::<i16>::from_slice(delta_bytes).unwrap().get() as i32
        } else {
            let pos = p + 2 * word_count + (region - word_count);
            let Some(delta_bytes) = delta_bytes.get(pos..pos + 1) else {
                return 0;
            };
            BigEndian::<i8>::from_slice(delta_bytes).unwrap().get() as i32
        }
    }
}

// direct port from Harfbuzz: <https://github.com/harfbuzz/harfbuzz/blob/22fbc7568828b9acfd116be44b2d77d56d2d448b/src/hb-ot-layout-common.hh#L3090>
// ignore the lint here
#[allow(clippy::too_many_arguments)]
fn set_item_delta(
    s: &mut Serializer,
    pos: usize,
    item: usize,
    region: usize,
    delta: i32,
    row_size: usize,
    has_long: bool,
    word_count: usize,
) -> Result<(), SerializeErrorFlags> {
    let p = pos + item * row_size;
    #[allow(clippy::collapsible_else_if)]
    if has_long {
        if region < word_count {
            let pos = p + region * 4;
            s.copy_assign(pos, delta);
        } else {
            let pos = p + 4 * word_count + 2 * (region - word_count);
            let Ok(delta) = i16::try_from(delta) else {
                return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
            };
            s.copy_assign(pos, delta);
        }
    } else {
        if region < word_count {
            let pos = p + region * 2;
            let Ok(delta) = i16::try_from(delta) else {
                return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
            };
            s.copy_assign(pos, delta);
        } else {
            let pos = p + 2 * word_count + (region - word_count);
            let Ok(delta) = i8::try_from(delta) else {
                return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
            };
            s.copy_assign(pos, delta);
        }
    }
    Ok(())
}

fn collect_region_refs(
    var_data: &ItemVariationData,
    inner_map: &IncBiMap,
    region_indices: &mut IntSet<u16>,
) {
    if inner_map.len() == 0 {
        return;
    }
    let delta_bytes = var_data.delta_sets();
    let row_size = var_data.get_delta_row_len();

    let regions = var_data.region_indexes();
    for (i, region) in regions.iter().enumerate() {
        let region = region.get();
        if region_indices.contains(region) {
            continue;
        }

        for item in inner_map.keys() {
            if get_item_delta(var_data, *item as usize, i, row_size, delta_bytes) != 0 {
                region_indices.insert(region);
                break;
            }
        }
    }
}

pub(crate) struct DeltaSetIndexMapSerializePlan<'a> {
    outer_bit_count: u8,
    inner_bit_count: u8,
    output_map: &'a FnvHashMap<u32, u32>,
    map_count: u32,
}

impl<'a> DeltaSetIndexMapSerializePlan<'a> {
    pub(crate) fn new(
        outer_bit_count: u8,
        inner_bit_count: u8,
        output_map: &'a FnvHashMap<u32, u32>,
        map_count: u32,
    ) -> Self {
        Self {
            outer_bit_count,
            inner_bit_count,
            output_map,
            map_count,
        }
    }

    pub(crate) fn width(&self) -> u8 {
        (self.outer_bit_count + self.inner_bit_count).div_ceil(8)
    }

    pub(crate) fn inner_bit_count(&self) -> u8 {
        self.inner_bit_count
    }

    pub(crate) fn output_map(&self) -> &'a FnvHashMap<u32, u32> {
        self.output_map
    }

    pub(crate) fn map_count(&self) -> u32 {
        self.map_count
    }
}

impl<'a> Serialize<'a> for DeltaSetIndexMap<'a> {
    type Args = &'a DeltaSetIndexMapSerializePlan<'a>;
    fn serialize(
        s: &mut Serializer,
        index_map_subset_plan: Self::Args,
    ) -> Result<(), SerializeErrorFlags> {
        let output_map = index_map_subset_plan.output_map();
        let width = index_map_subset_plan.width();
        let inner_bit_count = index_map_subset_plan.inner_bit_count();

        let map_count = index_map_subset_plan.map_count();
        // sanity check
        if map_count > 0 && (((inner_bit_count - 1) & (!0xF)) != 0 || (((width - 1) & (!0x3)) != 0))
        {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
        }

        let format: u8 = if map_count <= 0xFFFF { 0 } else { 1 };
        s.embed(format)?;

        let entry_format = ((width - 1) << 4) | (inner_bit_count - 1);
        s.embed(entry_format)?;

        if format == 0 {
            s.embed(map_count as u16)?;
        } else {
            s.embed(map_count)?;
        }

        let num_data_bytes = width as usize * map_count as usize;
        let mapdata_pos = s.allocate_size(num_data_bytes, true)?;

        let be_byte_index_start = 4 - width as usize;
        for i in 0..map_count {
            let Some(v) = output_map.get(&i) else {
                continue;
            };
            if *v == 0 {
                continue;
            }

            let outer = v >> 16;
            let inner = v & 0xFFFF;
            let u = (outer << inner_bit_count) | inner;
            let data_bytes = u.to_be_bytes();
            let data_pos = mapdata_pos + (i as usize) * width as usize;
            s.copy_assign_from_bytes(data_pos, data_bytes.get(be_byte_index_start..4).unwrap());
        }

        Ok(())
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use skrifa::raw::{FontData, FontRead};
    #[test]
    fn test_subset_item_varstore() {
        use crate::DEFAULT_LAYOUT_FEATURES;
        let raw_bytes: [u8; 471] = [
            0x00, 0x01, 0x00, 0x00, 0x00, 0x18, 0x00, 0x04, 0x00, 0x00, 0x00, 0x58, 0x00, 0x00,
            0x00, 0x6f, 0x00, 0x00, 0x00, 0x92, 0x00, 0x00, 0x01, 0xbc, 0x00, 0x02, 0x00, 0x05,
            0xc0, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x40, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x40, 0x00, 0xc0, 0x00, 0xc0, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x40, 0x00, 0x40, 0x00, 0x00, 0x00, 0x40, 0x00, 0x40, 0x00, 0x00, 0x00,
            0x40, 0x00, 0x40, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0xd8, 0xdd,
            0xe2, 0xec, 0xf1, 0xf6, 0xfb, 0x05, 0x0a, 0x0f, 0x14, 0x1e, 0x28, 0x32, 0x3c, 0x00,
            0x1b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x8d, 0xa1, 0xb5, 0xba, 0xbf, 0xc4, 0xce,
            0xd8, 0xdd, 0xe2, 0xe7, 0xec, 0xf1, 0xf6, 0xfb, 0x05, 0x0a, 0x0f, 0x14, 0x19, 0x1e,
            0x28, 0x2d, 0x32, 0x3c, 0x46, 0x64, 0x00, 0x90, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x01, 0xba, 0xa1, 0xba, 0x14, 0xba, 0x32, 0xc4, 0xc4, 0xc4, 0xd8, 0xc4, 0xe2,
            0xc4, 0x0a, 0xc4, 0x28, 0xce, 0xba, 0xce, 0xc4, 0xce, 0xd8, 0xce, 0xe2, 0xce, 0xec,
            0xce, 0x28, 0xce, 0x32, 0xd8, 0x92, 0xd8, 0x9c, 0xd8, 0xbf, 0xd8, 0xce, 0xd8, 0xd8,
            0xd8, 0xe2, 0xd8, 0xe7, 0xd8, 0xec, 0xd8, 0xf6, 0xd8, 0x1e, 0xe2, 0xc4, 0xe2, 0xce,
            0xe2, 0xd8, 0xe2, 0xe2, 0xe2, 0xe7, 0xe2, 0xec, 0xe2, 0xf1, 0xe2, 0xf6, 0xe2, 0x0a,
            0xe2, 0x14, 0xe2, 0x28, 0xe2, 0x32, 0xe2, 0x46, 0xec, 0xba, 0xec, 0xc4, 0xec, 0xce,
            0xec, 0xd8, 0xec, 0xdd, 0xec, 0xe2, 0xec, 0xec, 0xec, 0xf1, 0xec, 0xf6, 0xec, 0xfb,
            0xec, 0x05, 0xec, 0x0a, 0xec, 0x14, 0xec, 0x1e, 0xec, 0x28, 0xec, 0x32, 0xec, 0x50,
            0xf1, 0xd3, 0xf1, 0xf6, 0xf1, 0xfb, 0xf6, 0xc4, 0xf6, 0xce, 0xf6, 0xd8, 0xf6, 0xe2,
            0xf6, 0xe7, 0xf6, 0xec, 0xf6, 0xf1, 0xf6, 0xf6, 0xf6, 0xfb, 0xf6, 0x05, 0xf6, 0x0a,
            0xf6, 0x14, 0xf6, 0x19, 0xf6, 0x1e, 0xf6, 0x28, 0xf6, 0x32, 0xf6, 0x3c, 0xf6, 0x50,
            0xfb, 0xec, 0xfb, 0xf6, 0xfb, 0x05, 0xfb, 0x0a, 0xfb, 0x14, 0xfb, 0x19, 0xfb, 0x2d,
            0xfb, 0x37, 0x05, 0xe7, 0x05, 0xec, 0x05, 0xf1, 0x05, 0xf6, 0x05, 0xfb, 0x05, 0x05,
            0x05, 0x0a, 0x0a, 0xc9, 0x0a, 0xce, 0x0a, 0xd3, 0x0a, 0xd8, 0x0a, 0xe2, 0x0a, 0xec,
            0x0a, 0xf1, 0x0a, 0xf6, 0x0a, 0xfb, 0x0a, 0x05, 0x0a, 0x0a, 0x0a, 0x0f, 0x0a, 0x14,
            0x0a, 0x1e, 0x0a, 0x28, 0x0a, 0x32, 0x0a, 0x3c, 0x0a, 0x46, 0x0a, 0x50, 0x0f, 0xfb,
            0x0f, 0x05, 0x0f, 0x0a, 0x0f, 0x0f, 0x14, 0xc4, 0x14, 0xce, 0x14, 0xd8, 0x14, 0xe2,
            0x14, 0xec, 0x14, 0xf6, 0x14, 0x0a, 0x14, 0x0f, 0x14, 0x14, 0x14, 0x1e, 0x14, 0x28,
            0x14, 0x32, 0x14, 0x3c, 0x14, 0x46, 0x1e, 0xec, 0x1e, 0xf6, 0x1e, 0xfb, 0x1e, 0x0a,
            0x1e, 0x14, 0x1e, 0x1e, 0x1e, 0x28, 0x1e, 0x32, 0x1e, 0x3c, 0x28, 0xe2, 0x28, 0x0a,
            0x28, 0x14, 0x28, 0x1e, 0x28, 0x28, 0x28, 0x32, 0x32, 0x14, 0x00, 0x05, 0x00, 0x00,
            0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0xe2, 0xf6, 0x1e, 0xe2, 0x00, 0x1e,
            0xec, 0x00, 0x14, 0x00, 0x1e, 0x1e, 0x14, 0x1e, 0x0a,
        ];

        let item_varstore = ItemVariationStore::read(FontData::new(&raw_bytes)).unwrap();

        let mut plan = Plan::default();
        // create inner maps
        let mut bimap = IncBiMap::with_capacity(1);
        bimap.add(10);
        plan.base_varstore_inner_maps.push(bimap);

        let mut bimap = IncBiMap::with_capacity(4);
        bimap.add(13);
        bimap.add(16);
        bimap.add(17);
        bimap.add(18);
        plan.base_varstore_inner_maps.push(bimap);

        let mut bimap = IncBiMap::with_capacity(3);
        bimap.add(100);
        bimap.add(101);
        bimap.add(122);
        plan.base_varstore_inner_maps.push(bimap);

        let bimap = IncBiMap::default();
        plan.base_varstore_inner_maps.push(bimap);

        //layout_scripts
        plan.layout_scripts.invert();

        //layout_features
        plan.layout_features
            .extend(DEFAULT_LAYOUT_FEATURES.iter().copied());

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        let ret = item_varstore.subset(&plan, &mut s, (&plan.base_varstore_inner_maps, false));
        assert_eq!(ret, Ok(()));
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_bytes: [u8; 85] = [
            0x00, 0x01, 0x00, 0x00, 0x00, 0x39, 0x00, 0x03, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00,
            0x00, 0x24, 0x00, 0x00, 0x00, 0x14, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x01, 0x0a, 0x05, 0x0a, 0x0a, 0x14, 0x14, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01,
            0x00, 0x01, 0xf6, 0x0a, 0x0f, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
            0x14, 0x00, 0x02, 0x00, 0x02, 0xc0, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00,
        ];

        assert_eq!(subsetted_data, expected_bytes);
    }
}

//! impl subset() for HVAR

use crate::fnv::FnvHashMap;
use crate::offset::SerializeSerialize;
use crate::{
    offset::SerializeSubset,
    serialize::{SerializeErrorFlags, Serializer},
    variations::DeltaSetIndexMapSerializePlan,
    IncBiMap, Plan, Serialize, Subset, SubsetError, SubsetFlags,
};
use write_fonts::{
    read::{
        collections::IntSet,
        tables::{
            hvar::Hvar,
            variations::{DeltaSetIndex, DeltaSetIndexMap, EntryFormat, ItemVariationStore},
        },
        FontRef, ReadError, TopLevelTable,
    },
    types::{FixedSize, Offset32},
    FontBuilder,
};

// reference: subset() for HVAR in harfbuzz
// <https://github.com/harfbuzz/harfbuzz/blob/bcd5aa368d3fd3ef741ea29df15d3d56011811c0/src/hb-ot-var-hvar-table.hh#L330>
impl Subset for Hvar<'_> {
    fn subset(
        &self,
        plan: &Plan,
        _font: &FontRef,
        s: &mut Serializer,
        _builder: &mut FontBuilder,
    ) -> Result<(), SubsetError> {
        Self::serialize(s, (self, plan)).map_err(|_| SubsetError::SubsetTableError(Hvar::TAG))
    }
}

impl<'a> Serialize<'a> for Hvar<'_> {
    type Args = (&'a Hvar<'a>, &'a Plan);
    fn serialize(s: &mut Serializer, args: Self::Args) -> Result<(), SerializeErrorFlags> {
        let (hvar, plan) = args;
        s.embed(hvar.version())?;
        if hvar.item_variation_store_offset().is_null() {
            return s
                .allocate_size(5 * Offset32::RAW_BYTE_LEN, true)
                .map(|_| ());
        }

        let var_store = hvar
            .item_variation_store()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        let index_maps = hvar
            .listup_index_maps()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        let hvar_subset_plan = HvarVvarSubsetPlan::new(plan, &var_store, &index_maps)
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        let var_store_offset_pos = s.embed(0_u32)?;

        Offset32::serialize_subset(
            &var_store,
            s,
            plan,
            (hvar_subset_plan.inner_maps(), true),
            var_store_offset_pos,
        )?;

        serialize_index_maps(
            s,
            plan,
            &index_maps,
            hvar_subset_plan.index_map_subset_plans(),
        )
    }
}

pub(crate) trait ListupIndexMaps {
    fn listup_index_maps(&self) -> Result<Vec<Option<DeltaSetIndexMap<'_>>>, ReadError>;
}

impl ListupIndexMaps for Hvar<'_> {
    fn listup_index_maps(&self) -> Result<Vec<Option<DeltaSetIndexMap<'_>>>, ReadError> {
        let out = vec![
            self.advance_width_mapping().transpose()?,
            self.lsb_mapping().transpose()?,
            self.rsb_mapping().transpose()?,
        ];
        Ok(out)
    }
}

pub(crate) fn serialize_index_maps(
    s: &mut Serializer,
    _plan: &Plan,
    index_maps: &[Option<DeltaSetIndexMap>],
    index_map_plans: &[IndexMapSubsetPlan],
) -> Result<(), SerializeErrorFlags> {
    if index_maps.len() != index_map_plans.len() {
        return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
    }

    for index_map_subset_plan in index_map_plans {
        let offset_pos = s.embed(0_u32)?;
        if index_map_subset_plan.is_identity() {
            continue;
        }

        Offset32::serialize_serialize::<DeltaSetIndexMap>(
            s,
            &index_map_subset_plan.to_serialize_plan(),
            offset_pos,
        )?;
    }
    Ok(())
}

#[derive(Default)]
pub(crate) struct IndexMapSubsetPlan {
    map_count: u16,
    max_inners: Vec<u16>,
    outer_bit_count: u8,
    inner_bit_count: u8,
    output_map: FnvHashMap<u32, u32>,
}

impl IndexMapSubsetPlan {
    fn new(
        index_map: Option<&DeltaSetIndexMap>,
        plan: &Plan,
        bypass_empty: bool,
        outer_map: &mut IncBiMap,
        inner_sets: &mut [IntSet<u16>],
    ) -> Result<Self, ReadError> {
        let mut this = IndexMapSubsetPlan::default();
        if bypass_empty && index_map.is_none() {
            return Ok(this);
        }

        let entry_format = index_map
            .map(DeltaSetIndexMap::entry_format)
            .unwrap_or(EntryFormat::from_bits_truncate(1));

        this.outer_bit_count = (entry_format.entry_size() * 8) - entry_format.bit_count();
        this.max_inners.resize(inner_sets.len(), 0);

        let mut last_gid = None;
        let mut last_val = DeltaSetIndex::NO_VARIATION_INDEX;
        for (gid, old_gid) in plan.new_to_old_gid_list.iter().rev() {
            let old_gid = old_gid.to_u32();
            let val = match index_map {
                Some(m) => m.get(old_gid)?,
                None => DeltaSetIndex {
                    outer: (old_gid >> 16) as u16,
                    inner: (old_gid & 0xFFFF) as u16,
                },
            };
            if last_gid.is_none() {
                last_gid = Some(*gid);
                last_val = val;
                continue;
            }
            if val != last_val {
                break;
            }
            last_gid = Some(*gid);
        }

        if last_gid.is_none() {
            return Ok(this);
        }
        this.map_count = (last_gid.unwrap().to_u32() + 1) as u16;

        if index_map.is_none() {
            outer_map.add(0);
            inner_sets[0].extend(plan.glyphset.iter().map(|g| g.to_u32() as u16));
            this.max_inners[0] = plan.new_to_old_gid_list.iter().last().unwrap().1.to_u32() as u16;
        } else {
            for (new_gid, old_gid) in plan.new_to_old_gid_list.iter() {
                if new_gid.to_u32() >= this.map_count as u32 {
                    break;
                }

                let v = index_map.unwrap().get(old_gid.to_u32())?;
                let outer = v.outer as usize;
                if outer >= this.max_inners.len() {
                    break;
                }

                outer_map.add(v.outer as u32);
                if v.inner > this.max_inners[outer] {
                    this.max_inners[outer] = v.inner;
                }

                inner_sets[outer].insert(v.inner);
            }
        }

        Ok(this)
    }

    fn remap(
        &mut self,
        plan: &Plan,
        index_map: Option<&DeltaSetIndexMap>,
        outer_map: &IncBiMap,
        inner_maps: &[IncBiMap],
    ) {
        self.inner_bit_count = 1;

        for (max_inner, inner_map) in self.max_inners.iter().zip(inner_maps) {
            if inner_map.len() == 0 || *max_inner == 0 {
                continue;
            }

            let bit_count = 32
                - inner_map
                    .get(*max_inner as u32)
                    .unwrap_or(&0)
                    .leading_zeros() as u8;
            self.inner_bit_count = bit_count.max(self.inner_bit_count);
        }

        for (new_gid, old_gid) in plan.new_to_old_gid_list.iter() {
            if new_gid.to_u32() as u16 >= self.map_count {
                break;
            }

            let old_gid = old_gid.to_u32();
            let v = match index_map {
                Some(m) => m.get(old_gid).unwrap(),
                None => DeltaSetIndex {
                    outer: (old_gid >> 16) as u16,
                    inner: (old_gid & 0xFFFF) as u16,
                },
            };

            let outer = v.outer;
            if outer as usize >= inner_maps.len() {
                continue;
            }

            let new_outer = outer_map.get(outer as u32).unwrap();
            let new_inner = inner_maps[outer as usize].get(v.inner as u32).unwrap();
            self.output_map
                .insert(new_gid.to_u32(), (*new_outer << 16) | *new_inner);
        }
    }

    fn is_identity(&self) -> bool {
        self.output_map.is_empty()
    }

    fn to_serialize_plan(&self) -> DeltaSetIndexMapSerializePlan<'_> {
        DeltaSetIndexMapSerializePlan::new(
            self.outer_bit_count,
            self.inner_bit_count,
            &self.output_map,
            self.map_count as u32,
        )
    }
}

#[derive(Default)]
pub(crate) struct HvarVvarSubsetPlan {
    inner_maps: Vec<IncBiMap>,
    index_map_subset_plans: Vec<IndexMapSubsetPlan>,
}

impl HvarVvarSubsetPlan {
    pub(crate) fn new(
        plan: &Plan,
        var_store: &ItemVariationStore,
        index_maps: &[Option<DeltaSetIndexMap>],
    ) -> Result<Self, ReadError> {
        let mut this = HvarVvarSubsetPlan::default();
        let vardata_count = var_store.item_variation_data_count() as usize;

        let mut inner_sets = Vec::new();
        inner_sets.resize(vardata_count, Default::default());

        let mut outer_map = IncBiMap::default();
        // don't bypass empty adv map
        let index_map_subset_plan = IndexMapSubsetPlan::new(
            index_maps[0].as_ref(),
            plan,
            false,
            &mut outer_map,
            &mut inner_sets,
        )?;
        this.index_map_subset_plans.push(index_map_subset_plan);

        let mut adv_set = IntSet::empty();
        if index_maps[0].is_none() {
            adv_set.union(&inner_sets[0]);
        }

        for index_map in index_maps.iter().skip(1) {
            this.index_map_subset_plans.push(IndexMapSubsetPlan::new(
                index_map.as_ref(),
                plan,
                true,
                &mut outer_map,
                &mut inner_sets,
            )?);
        }

        outer_map.sort();

        let retain_adv_map = index_maps[0].is_none()
            && plan
                .subset_flags
                .contains(SubsetFlags::SUBSET_FLAGS_RETAIN_GIDS);

        if retain_adv_map {
            let inner_map = plan
                .new_to_old_gid_list
                .iter()
                .map(|(_, old_gid)| old_gid.to_u32())
                .collect();
            this.inner_maps.push(inner_map);
        } else {
            let mut inner_map = adv_set.iter().map(|g| g as u32).collect::<IncBiMap>();
            inner_sets[0].subtract(&adv_set);

            for i in inner_sets[0].iter() {
                inner_map.add(i as u32);
            }

            this.inner_maps.push(inner_map);
        }

        for set in inner_sets.iter().skip(1) {
            let inner_map = set.iter().map(|g| g as u32).collect();
            this.inner_maps.push(inner_map);
        }

        this.index_map_subset_plans
            .iter_mut()
            .zip(index_maps)
            .for_each(|(index_map_subset_plan, index_map)| {
                index_map_subset_plan.remap(plan, index_map.as_ref(), &outer_map, &this.inner_maps)
            });
        Ok(this)
    }

    pub(crate) fn inner_maps(&self) -> &[IncBiMap] {
        &self.inner_maps
    }

    pub(crate) fn index_map_subset_plans(&self) -> &[IndexMapSubsetPlan] {
        &self.index_map_subset_plans
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use write_fonts::{
        read::{FontData, FontRead},
        types::GlyphId,
    };
    #[test]
    fn test_subset_hvar_noop() {
        let raw_bytes: [u8; 98] = [
            0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04,
            0x00, 0x06, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x26, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x20, 0x00, 0x00, 0x00, 0x10, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x01, 0xfd, 0x20, 0x02, 0x25, 0x05, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x02, 0x00, 0x02, 0xc0, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        ];

        let hvar = Hvar::read(FontData::new(&raw_bytes)).unwrap();
        let mut builder = FontBuilder::new();

        //dummy font
        let font = FontRef::new(&raw_bytes).unwrap();

        let mut plan = Plan::default();
        plan.new_to_old_gid_list
            .push((GlyphId::NOTDEF, GlyphId::NOTDEF));
        plan.new_to_old_gid_list
            .push((GlyphId::from(1_u32), GlyphId::from(1_u32)));
        plan.new_to_old_gid_list
            .push((GlyphId::from(2_u32), GlyphId::from(2_u32)));
        plan.new_to_old_gid_list
            .push((GlyphId::from(3_u32), GlyphId::from(3_u32)));

        plan.glyphset
            .insert_range(GlyphId::NOTDEF..=GlyphId::from(3_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        let ret = hvar.subset(&plan, &font, &mut s, &mut builder);
        assert!(ret.is_ok());
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        assert_eq!(subsetted_data, raw_bytes);
    }

    #[test]
    fn test_subset_hvar() {
        let raw_bytes: [u8; 98] = [
            0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04,
            0x00, 0x06, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x26, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x20, 0x00, 0x00, 0x00, 0x10, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x01, 0xfd, 0x20, 0x02, 0x25, 0x05, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x02, 0x00, 0x02, 0xc0, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        ];

        let hvar = Hvar::read(FontData::new(&raw_bytes)).unwrap();
        let mut builder = FontBuilder::new();

        //dummy font
        let font = FontRef::new(&raw_bytes).unwrap();

        let mut plan = Plan::default();
        plan.new_to_old_gid_list
            .push((GlyphId::NOTDEF, GlyphId::NOTDEF));
        plan.new_to_old_gid_list
            .push((GlyphId::from(1_u32), GlyphId::from(1_u32)));
        plan.new_to_old_gid_list
            .push((GlyphId::from(2_u32), GlyphId::from(3_u32)));

        plan.glyphset.insert(GlyphId::NOTDEF);
        plan.glyphset.insert(GlyphId::from(1_u32));
        plan.glyphset.insert(GlyphId::from(3_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        let ret = hvar.subset(&plan, &font, &mut s, &mut builder);
        assert!(ret.is_ok());
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 94] = [
            0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02,
            0x00, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x24, 0x00, 0x02, 0x00, 0x00, 0x00, 0x1e,
            0x00, 0x00, 0x00, 0x10, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
            0xfd, 0x20, 0x02, 0x25, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02,
            0xc0, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x40, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        ];
        assert_eq!(subsetted_data, expected_data);
    }

    #[test]
    fn test_subset_hvar_retain_gids() {
        let raw_bytes: [u8; 98] = [
            0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04,
            0x00, 0x06, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x26, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x20, 0x00, 0x00, 0x00, 0x10, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x01, 0xfd, 0x20, 0x02, 0x25, 0x05, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x02, 0x00, 0x02, 0xc0, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        ];

        let hvar = Hvar::read(FontData::new(&raw_bytes)).unwrap();
        let mut builder = FontBuilder::new();

        //dummy font
        let font = FontRef::new(&raw_bytes).unwrap();

        let mut plan = Plan::default();
        plan.new_to_old_gid_list
            .push((GlyphId::NOTDEF, GlyphId::NOTDEF));
        plan.new_to_old_gid_list
            .push((GlyphId::from(1_u32), GlyphId::from(1_u32)));
        plan.new_to_old_gid_list
            .push((GlyphId::from(3_u32), GlyphId::from(3_u32)));

        plan.glyphset.insert(GlyphId::NOTDEF);
        plan.glyphset.insert(GlyphId::from(1_u32));
        plan.glyphset.insert(GlyphId::from(3_u32));

        plan.subset_flags |= SubsetFlags::SUBSET_FLAGS_RETAIN_GIDS;

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        let ret = hvar.subset(&plan, &font, &mut s, &mut builder);
        assert!(ret.is_ok());
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 96] = [
            0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02,
            0x00, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x24, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x1e, 0x00, 0x00, 0x00, 0x10, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x01, 0xfd, 0x20, 0x02, 0x25, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
            0x00, 0x02, 0xc0, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x40, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        ];
        assert_eq!(subsetted_data, expected_data);
    }

    #[test]
    fn test_subset_identity_hvar_noop() {
        let raw_bytes: [u8; 98] = [
            0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04,
            0x00, 0x06, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x26, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x20, 0x00, 0x00, 0x00, 0x10, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x01, 0xfd, 0x20, 0x02, 0x25, 0x05, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x02, 0x00, 0x02, 0xc0, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        ];

        let hvar = Hvar::read(FontData::new(&raw_bytes)).unwrap();
        let mut builder = FontBuilder::new();

        //dummy font
        let font = FontRef::new(&raw_bytes).unwrap();

        let mut plan = Plan::default();
        plan.new_to_old_gid_list
            .push((GlyphId::NOTDEF, GlyphId::NOTDEF));
        plan.new_to_old_gid_list
            .push((GlyphId::from(1_u32), GlyphId::from(1_u32)));
        plan.new_to_old_gid_list
            .push((GlyphId::from(2_u32), GlyphId::from(2_u32)));
        plan.new_to_old_gid_list
            .push((GlyphId::from(3_u32), GlyphId::from(3_u32)));

        plan.glyphset
            .insert_range(GlyphId::NOTDEF..=GlyphId::from(3_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        let ret = hvar.subset(&plan, &font, &mut s, &mut builder);
        assert!(ret.is_ok());
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        assert_eq!(subsetted_data, raw_bytes);
    }

    #[test]
    fn test_subset_identity_hvar() {
        let raw_bytes: [u8; 98] = [
            0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04,
            0x00, 0x06, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x26, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x20, 0x00, 0x00, 0x00, 0x10, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x01, 0xfd, 0x20, 0x02, 0x25, 0x05, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x02, 0x00, 0x02, 0xc0, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        ];

        let hvar = Hvar::read(FontData::new(&raw_bytes)).unwrap();
        let mut builder = FontBuilder::new();

        //dummy font
        let font = FontRef::new(&raw_bytes).unwrap();

        let mut plan = Plan::default();
        plan.new_to_old_gid_list
            .push((GlyphId::NOTDEF, GlyphId::NOTDEF));
        plan.new_to_old_gid_list
            .push((GlyphId::from(1_u32), GlyphId::from(1_u32)));
        plan.new_to_old_gid_list
            .push((GlyphId::from(2_u32), GlyphId::from(3_u32)));

        plan.glyphset.insert(GlyphId::NOTDEF);
        plan.glyphset.insert(GlyphId::from(1_u32));
        plan.glyphset.insert(GlyphId::from(3_u32));

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        let ret = hvar.subset(&plan, &font, &mut s, &mut builder);
        assert!(ret.is_ok());
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 94] = [
            0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02,
            0x00, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x24, 0x00, 0x02, 0x00, 0x00, 0x00, 0x1e,
            0x00, 0x00, 0x00, 0x10, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
            0xfd, 0x20, 0x02, 0x25, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02,
            0xc0, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x40, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        ];
        assert_eq!(subsetted_data, expected_data);
    }

    #[test]
    fn test_subset_identity_hvar_retain_gids() {
        let raw_bytes: [u8; 98] = [
            0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04,
            0x00, 0x06, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x26, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x20, 0x00, 0x00, 0x00, 0x10, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x01, 0xfd, 0x20, 0x02, 0x25, 0x05, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x02, 0x00, 0x02, 0xc0, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        ];

        let hvar = Hvar::read(FontData::new(&raw_bytes)).unwrap();
        let mut builder = FontBuilder::new();

        //dummy font
        let font = FontRef::new(&raw_bytes).unwrap();

        let mut plan = Plan::default();
        plan.new_to_old_gid_list
            .push((GlyphId::NOTDEF, GlyphId::NOTDEF));
        plan.new_to_old_gid_list
            .push((GlyphId::from(1_u32), GlyphId::from(1_u32)));
        plan.new_to_old_gid_list
            .push((GlyphId::from(3_u32), GlyphId::from(3_u32)));

        plan.glyphset.insert(GlyphId::NOTDEF);
        plan.glyphset.insert(GlyphId::from(1_u32));
        plan.glyphset.insert(GlyphId::from(3_u32));

        plan.subset_flags |= SubsetFlags::SUBSET_FLAGS_RETAIN_GIDS;

        let mut s = Serializer::new(1024);
        assert_eq!(s.start_serialize(), Ok(()));
        let ret = hvar.subset(&plan, &font, &mut s, &mut builder);
        assert!(ret.is_ok());
        assert!(!s.in_error());
        s.end_serialize();

        let subsetted_data = s.copy_bytes();
        let expected_data: [u8; 96] = [
            0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02,
            0x00, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x24, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x1e, 0x00, 0x00, 0x00, 0x10, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x01, 0xfd, 0x20, 0x02, 0x25, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
            0x00, 0x02, 0xc0, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x40, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        ];
        assert_eq!(subsetted_data, expected_data);
    }
}

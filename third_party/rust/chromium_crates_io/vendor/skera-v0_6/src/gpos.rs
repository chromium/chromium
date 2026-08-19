//! impl subset() for GPOS table

mod anchor;
mod cursive_pos;
mod mark_array;
mod mark_base_pos;
mod mark_lig_pos;
mod mark_mark_pos;
mod pair_pos;
mod single_pos;
mod value_record;

use crate::fnv::FnvHashMap;
use crate::{
    collect_features_with_retained_subs, find_duplicate_features,
    offset::SerializeSubset,
    prune_features, remap_feature_indices, remap_indices,
    serialize::{SerializeErrorFlags, Serializer},
    CollectVariationIndices, LayoutClosure, NameIdClosure, Plan, PruneLangSysContext, Subset,
    SubsetError, SubsetLayoutContext, SubsetState, SubsetTable,
};
use write_fonts::{
    read::{
        collections::IntSet,
        tables::{
            gpos::{
                ExtensionPosFormat1, ExtensionSubtable, Gpos, PositionLookup, PositionSubtables,
            },
            layout::{ExtensionLookup, Intersect, LookupFlag, Subtables},
        },
        types::Tag,
        FontRead, FontRef, ReadError, TopLevelTable,
    },
    types::{MajorMinor, Offset16, Offset32},
    FontBuilder,
};

impl NameIdClosure for Gpos<'_> {
    //TODO: support instancing: collect from feature substitutes if exist
    fn collect_name_ids(&self, plan: &mut Plan) {
        let Ok(feature_list) = self.feature_list() else {
            return;
        };
        for (i, feature_record) in feature_list.feature_records().iter().enumerate() {
            if !plan.gpos_features.contains_key(&(i as u16)) {
                continue;
            }
            let Ok(feature) = feature_record.feature(feature_list.offset_data()) else {
                continue;
            };
            feature.collect_name_ids(plan);
        }
    }
}

impl LayoutClosure for Gpos<'_> {
    fn prune_features(
        &self,
        lookup_indices: &IntSet<u16>,
        feature_indices: IntSet<u16>,
    ) -> IntSet<u16> {
        let alternate_features = if let Some(Ok(feature_variations)) = self.feature_variations() {
            collect_features_with_retained_subs(&feature_variations, lookup_indices)
        } else {
            IntSet::empty()
        };

        let Ok(feature_list) = self.feature_list() else {
            return IntSet::empty();
        };
        prune_features(
            &feature_list,
            &alternate_features,
            lookup_indices,
            feature_indices,
        )
    }

    fn find_duplicate_features(
        &self,
        lookup_indices: &IntSet<u16>,
        feature_indices: IntSet<u16>,
    ) -> FnvHashMap<u16, u16> {
        let Ok(feature_list) = self.feature_list() else {
            return FnvHashMap::default();
        };
        find_duplicate_features(&feature_list, lookup_indices, feature_indices)
    }

    fn prune_langsys(
        &self,
        duplicate_feature_index_map: &FnvHashMap<u16, u16>,
        layout_scripts: &IntSet<Tag>,
    ) -> (FnvHashMap<u16, IntSet<u16>>, IntSet<u16>) {
        let mut c = PruneLangSysContext::new(duplicate_feature_index_map);
        let Ok(script_list) = self.script_list() else {
            return (c.script_langsys_map(), c.feature_indices());
        };
        c.prune_langsys(&script_list, layout_scripts)
    }

    fn closure_glyphs_lookups_features(&self, plan: &mut Plan) {
        let Ok(feature_indices) =
            self.collect_features(&plan.layout_scripts, &IntSet::all(), &plan.layout_features)
        else {
            return;
        };

        let Ok(mut lookup_indices) = self.collect_lookups(&feature_indices) else {
            return;
        };
        if self
            .closure_lookups(&plan.glyphset_gsub, &mut lookup_indices)
            .is_err()
        {
            return;
        };

        let feature_indices = self.prune_features(&lookup_indices, feature_indices);
        let duplicate_feature_index_map =
            self.find_duplicate_features(&lookup_indices, feature_indices);

        let (script_langsys_map, feature_indices) =
            self.prune_langsys(&duplicate_feature_index_map, &plan.layout_scripts);

        plan.gpos_lookups = remap_indices(lookup_indices);

        (plan.gpos_features, plan.gpos_features_w_duplicates) =
            remap_feature_indices(&feature_indices, &duplicate_feature_index_map);
        plan.gpos_script_langsys = script_langsys_map;
    }
}

impl Subset for Gpos<'_> {
    fn subset_with_state(
        &self,
        plan: &Plan,
        font: &FontRef,
        state: &mut SubsetState,
        s: &mut Serializer,
        _builder: &mut FontBuilder,
    ) -> Result<(), SubsetError> {
        subset_gpos(self, plan, font, state, s)
            .map_err(|_| SubsetError::SubsetTableError(Gpos::TAG))
    }
}

fn subset_gpos(
    gpos: &Gpos,
    plan: &Plan,
    font: &FontRef,
    state: &SubsetState,
    s: &mut Serializer,
) -> Result<(), SerializeErrorFlags> {
    let version_pos = s.embed(gpos.version())?;
    let mut c = SubsetLayoutContext::new(Gpos::TAG);

    // script_list
    let script_list_offset_pos = s.embed(0_u16)?;

    if !gpos.script_list_offset().is_null() {
        let script_list = gpos
            .script_list()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        Offset16::serialize_subset(&script_list, s, plan, &mut c, script_list_offset_pos)?;
    }

    // feature list
    let feature_list_offset_pos = s.embed(0_u16)?;
    if !gpos.feature_list_offset().is_null() {
        let feature_list = gpos
            .feature_list()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        Offset16::serialize_subset(&feature_list, s, plan, &mut c, feature_list_offset_pos)?;
    }

    // lookup list
    let lookup_list_offset_pos = s.embed(0_u16)?;
    if !gpos.lookup_list_offset().is_null() {
        let lookup_list = gpos
            .lookup_list()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        Offset16::serialize_subset(
            &lookup_list,
            s,
            plan,
            (state, font, &plan.gpos_lookups),
            lookup_list_offset_pos,
        )?;
    }

    if let Some(feature_variations) = gpos
        .feature_variations()
        .transpose()
        .map_err(|_| SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)?
    {
        let snap = s.snapshot();
        let feature_vars_offset_pos = s.embed(0_u32)?;
        match Offset32::serialize_subset(
            &feature_variations,
            s,
            plan,
            &mut c,
            feature_vars_offset_pos,
        ) {
            Ok(()) => (),
            // downgrade table version if there are no FeatureVariations
            Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => {
                s.revert_snapshot(snap);
                s.copy_assign(version_pos, MajorMinor::VERSION_1_0);
            }
            Err(e) => return Err(e),
        }
    }
    Ok(())
}

impl<'a> SubsetTable<'a> for PositionLookup<'_> {
    type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FnvHashMap<u16, u16>);
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let lookup_type = self.lookup_type();
        s.embed(lookup_type)?;

        let lookup_flag = self.lookup_flag();
        let lookup_flag_pos = s.embed(lookup_flag)?;
        let lookup_count_pos = s.embed(0_u16)?;

        //subset subtables
        let lookup_count = match self {
            PositionLookup::Single(inner) => inner.subtables().subset(plan, s, args)?,
            PositionLookup::Pair(inner) => inner.subtables().subset(plan, s, args)?,
            PositionLookup::Cursive(inner) => inner.subtables().subset(plan, s, args)?,
            PositionLookup::MarkToBase(inner) => inner.subtables().subset(plan, s, args)?,
            PositionLookup::MarkToLig(inner) => inner.subtables().subset(plan, s, args)?,
            PositionLookup::MarkToMark(inner) => inner.subtables().subset(plan, s, args)?,
            PositionLookup::Contextual(inner) => inner.subtables().subset(plan, s, args)?,
            PositionLookup::ChainContextual(inner) => inner.subtables().subset(plan, s, args)?,
            PositionLookup::Extension(inner) => inner.subtables().subset(plan, s, args)?,
        };
        s.copy_assign(lookup_count_pos, lookup_count);

        // ref: <https://github.com/harfbuzz/harfbuzz/blob/a790c38b782f9d8e6f0299d2837229e5726fc669/src/hb-ot-layout-common.hh#L1385>
        if let Some(mark_filtering_set) = self.mark_filtering_set() {
            if let Some(new_idx) = plan.used_mark_sets_map.get(&mark_filtering_set) {
                s.embed(*new_idx)?;
            } else {
                let new_flag =
                    (lookup_flag - LookupFlag::USE_MARK_FILTERING_SET) | LookupFlag::IGNORE_MARKS;
                s.copy_assign(lookup_flag_pos, new_flag);
            }
        }
        Ok(())
    }
}

impl<'a> SubsetTable<'a> for ExtensionSubtable<'a> {
    type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FnvHashMap<u16, u16>);
    type Output = ();

    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        match self {
            ExtensionSubtable::Single(inner) => inner.subset(plan, s, args),
            ExtensionSubtable::Pair(inner) => inner.subset(plan, s, args),
            ExtensionSubtable::Cursive(inner) => inner.subset(plan, s, args),
            ExtensionSubtable::MarkToBase(inner) => inner.subset(plan, s, args),
            ExtensionSubtable::MarkToLig(inner) => inner.subset(plan, s, args),
            ExtensionSubtable::MarkToMark(inner) => inner.subset(plan, s, args),
            ExtensionSubtable::Contextual(inner) => inner.subset(plan, s, args),
            ExtensionSubtable::ChainContextual(inner) => inner.subset(plan, s, args),
        }
    }
}

impl<'a, T> SubsetTable<'a> for ExtensionPosFormat1<'a, T>
where
    T: SubsetTable<
            'a,
            ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FnvHashMap<u16, u16>),
            Output = (),
        > + FontRead<'a, Args = ()>,
{
    type ArgsForSubset = (&'a SubsetState, &'a FontRef<'a>, &'a FnvHashMap<u16, u16>);
    type Output = ();

    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        if self.extension_offset().is_null() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        s.embed(self.pos_format())?;
        s.embed(self.extension_lookup_type())?;

        let offset_pos = s.embed(0_u32)?;
        let subtable = self
            .extension()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

        Offset32::serialize_subset(&subtable, s, plan, args, offset_pos)
    }
}

impl CollectVariationIndices for Gpos<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        let Ok(lookups) = self.lookup_list() else {
            return;
        };

        let lookups = lookups.lookups();
        for i in plan.gpos_lookups.keys() {
            let Ok(lookup) = lookups.get(*i as usize) else {
                return;
            };

            match lookup.subtables() {
                Ok(subs) => subs.collect_variation_indices(plan, varidx_set),
                Err(ReadError::NullOffset) => continue,
                Err(_) => return,
            }
        }
    }
}

//TODO: support all lookup types
impl CollectVariationIndices for PositionSubtables<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        match self {
            PositionSubtables::Single(subtables) => {
                subtables.collect_variation_indices(plan, varidx_set)
            }
            PositionSubtables::Pair(subtables) => {
                subtables.collect_variation_indices(plan, varidx_set)
            }
            PositionSubtables::Cursive(subtables) => {
                subtables.collect_variation_indices(plan, varidx_set)
            }
            PositionSubtables::MarkToBase(subtables) => {
                subtables.collect_variation_indices(plan, varidx_set)
            }
            PositionSubtables::MarkToLig(subtables) => {
                subtables.collect_variation_indices(plan, varidx_set)
            }
            PositionSubtables::MarkToMark(subtables) => {
                subtables.collect_variation_indices(plan, varidx_set)
            }
            _ => (),
        }
    }
}

impl<'a, T, Ext> CollectVariationIndices for Subtables<'a, T, Ext>
where
    T: CollectVariationIndices + Intersect + FontRead<'a, Args = ()> + 'a,
    Ext: ExtensionLookup<'a, T> + 'a,
{
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        for t in self.iter().filter_map(|table| match table {
            Err(ReadError::NullOffset) => None,
            other => Some(other),
        }) {
            let Ok(t) = t else {
                return;
            };

            let Ok(intersect) = t.intersects(&plan.glyphset_gsub) else {
                return;
            };

            if !intersect {
                continue;
            }
            t.collect_variation_indices(plan, varidx_set);
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use write_fonts::read::{FontRef, TableProvider};

    #[test]
    fn test_prune_langsys() {
        let font = FontRef::new(include_bytes!("../test-data/fonts/Amiri-Regular.ttf")).unwrap();
        let gpos = font.gpos().unwrap();

        let mut layout_scripts = IntSet::all();
        let mut duplicate_feature_index_map = FnvHashMap::default();
        duplicate_feature_index_map.insert(0, 0);
        duplicate_feature_index_map.insert(2, 2);
        duplicate_feature_index_map.insert(4, 2);

        let (script_langsys_map, features) =
            gpos.prune_langsys(&duplicate_feature_index_map, &layout_scripts);
        // script langsys map is empty cause all langsys duplicate with default langsys
        assert!(script_langsys_map.is_empty());
        assert_eq!(features.len(), 3);
        assert!(features.contains(0));
        assert!(features.contains(2));
        assert!(features.contains(4));

        // test script filter
        layout_scripts.clear();
        layout_scripts.insert(Tag::new(b"arab"));
        let (script_langsys_map, features) =
            gpos.prune_langsys(&duplicate_feature_index_map, &layout_scripts);
        // script langsys map is still empty cause all langsys duplicate with default langsys
        assert!(script_langsys_map.is_empty());
        assert_eq!(features.len(), 1);
        assert!(features.contains(0));
    }

    #[test]
    fn test_find_duplicate_features() {
        let font = FontRef::new(include_bytes!("../test-data/fonts/Amiri-Regular.ttf")).unwrap();
        let gpos = font.gpos().unwrap();

        let mut lookups = IntSet::empty();
        lookups.insert(0_u16);

        let mut feature_indices = IntSet::empty();
        // 1 and 2 diffs: 2 has one more lookup that's indexed at 82
        feature_indices.insert(1_u16);
        feature_indices.insert(2_u16);
        // 3 and 4 diffs:
        // feature indexed at 4 has only 2 lookups: index 2 and 58
        // feature indexed at 3 has 13 more lookups
        feature_indices.insert(3_u16);
        feature_indices.insert(4_u16);

        let feature_index_map = gpos.find_duplicate_features(&lookups, feature_indices);
        // with only lookup index=0
        // feature=1 and feature=2 are duplicates
        // feature=3 and feature=4 are duplicates
        assert_eq!(feature_index_map.len(), 4);
        assert_eq!(feature_index_map.get(&1), Some(&1));
        assert_eq!(feature_index_map.get(&2), Some(&1));
        assert_eq!(feature_index_map.get(&3), Some(&3));
        assert_eq!(feature_index_map.get(&4), Some(&3));

        // lookup=82 only referenced by feature=2
        lookups.insert(82_u16);
        // lookup=81 only referenced by feature=3
        lookups.insert(81_u16);
        let mut feature_indices = IntSet::empty();
        // 1 and 2 diffs: 2 has one more lookup that's indexed at 82
        feature_indices.insert(1_u16);
        feature_indices.insert(2_u16);
        feature_indices.insert(3_u16);
        feature_indices.insert(4_u16);
        let feature_index_map = gpos.find_duplicate_features(&lookups, feature_indices);
        // with only lookup index=0
        // feature=1 and feature=2 are duplicates
        // feature=3 and feature=4 are duplicates
        assert_eq!(feature_index_map.len(), 4);
        assert_eq!(feature_index_map.get(&1), Some(&1));
        assert_eq!(feature_index_map.get(&2), Some(&2));
        assert_eq!(feature_index_map.get(&3), Some(&3));
        assert_eq!(feature_index_map.get(&4), Some(&4));
    }
}

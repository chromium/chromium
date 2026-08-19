//! impl subset() for GDEF

use crate::{
    layout::ClassDefSubsetStruct,
    offset::{SerializeSerialize, SerializeSubset},
    offset_array::{IterNullableHelper, SubsetOffsetArray},
    serialize::{SerializeErrorFlags, Serializer},
    CollectVariationIndices, Plan, Subset, SubsetError, SubsetState, SubsetTable,
};
use write_fonts::{
    read::{
        collections::IntSet,
        tables::{
            gdef::{
                AttachList, AttachPoint, CaretValue, CaretValueFormat1, CaretValueFormat2,
                CaretValueFormat3, Gdef, LigCaretList, LigGlyph, MarkGlyphSets,
            },
            layout::CoverageTable,
        },
        types::GlyphId,
        FontRef, MinByteRange, ReadError, TopLevelTable,
    },
    types::{FixedSize, Offset16, Offset32},
    FontBuilder,
};

// reference: subset() for GDEF in harfbuzz
// <https://github.com/harfbuzz/harfbuzz/blob/59001aa9527c056ad08626cfec9a079b65d8aec8/src/OT/Layout/GDEF/GDEF.hh#L660>
impl Subset for Gdef<'_> {
    fn subset_with_state(
        &self,
        plan: &Plan,
        _font: &FontRef,
        state: &mut SubsetState,
        s: &mut Serializer,
        _builder: &mut FontBuilder,
    ) -> Result<(), SubsetError> {
        subset_gdef(self, plan, s, state).map_err(|_| SubsetError::SubsetTableError(Gdef::TAG))
    }
}

fn subset_gdef(
    gdef: &Gdef,
    plan: &Plan,
    s: &mut Serializer,
    state: &mut SubsetState,
) -> Result<(), SerializeErrorFlags> {
    let version = gdef.version();
    // major version
    s.embed(version.major)?;

    // minor version, might change after subset
    let minor_version_pos = s.embed(version.minor)?;

    // glyph classdef offset
    let glyph_classdef_offset_pos = s.embed(0_u16)?;

    // attach list offset
    let attachlist_offset_pos = s.embed(0_u16)?;

    // ligcaret list offset
    let ligcaret_list_offset_pos = s.embed(0_u16)?;

    //mark attach classdef offset
    let markattach_classdef_offset_pos = s.embed(0_u16)?;

    let snapshot_version0 = s.snapshot();

    let mark_glyphsetsdef_offset_pos = if version.minor >= 2 {
        s.embed(0_u16)?
    } else {
        0
    };

    // Push var store first (if it's needed) so that it's last in the
    // serialization order. Some font consumers assume that varstore runs to
    // the end of the GDEF table.
    // See: https://github.com/harfbuzz/harfbuzz/issues/4636

    let (subset_varstore, varstore_idx) = if version.minor >= 3 {
        if let Some(var_store) = gdef
            .item_var_store()
            .transpose()
            .map_err(|_| SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)?
        {
            let snapshot_version2 = s.snapshot();
            let var_store_offset_pos = s.embed(0_u32)?;
            match Offset32::serialize_subset(
                &var_store,
                s,
                plan,
                (&plan.gdef_varstore_inner_maps, false),
                var_store_offset_pos,
            ) {
                Ok(()) => {
                    let Some(varstore_idx) = s.last_added_child_index() else {
                        return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER));
                    };
                    (true, varstore_idx)
                }
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => {
                    s.revert_snapshot(snapshot_version2);
                    (false, 0)
                }
                Err(e) => {
                    return Err(e);
                }
            }
        } else {
            (false, 0)
        }
    } else {
        (false, 0)
    };

    let subset_mark_glyphsets_def = if version.minor >= 2 {
        if let Some(mark_glyph_sets_def) = gdef
            .mark_glyph_sets_def()
            .transpose()
            .map_err(|_| SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)?
        {
            match Offset16::serialize_subset(
                &mark_glyph_sets_def,
                s,
                plan,
                (),
                mark_glyphsetsdef_offset_pos,
            ) {
                Ok(()) => true,
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => false,
                Err(e) => {
                    return Err(e);
                }
            }
        } else {
            false
        }
    } else {
        false
    };

    // Downgrade version if possible
    if !subset_varstore {
        if subset_mark_glyphsets_def {
            s.copy_assign(minor_version_pos, 2_u16);
        } else {
            s.copy_assign(minor_version_pos, 0_u16);
            s.revert_snapshot(snapshot_version0);
        }
    }

    let subset_mark_attach_classdef = if let Some(mark_acttach_classdef) = gdef
        .mark_attach_class_def()
        .transpose()
        .map_err(|_| SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)?
    {
        match Offset16::serialize_subset(
            &mark_acttach_classdef,
            s,
            plan,
            &ClassDefSubsetStruct {
                remap_class: false,
                keep_empty_table: false,
                use_class_zero: true,
                glyph_filter: None,
            },
            markattach_classdef_offset_pos,
        ) {
            Ok(_) => true,
            Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => false,
            Err(e) => {
                return Err(e);
            }
        }
    } else {
        false
    };

    let subset_ligcaret_list = if let Some(ligcaret_list) = gdef
        .lig_caret_list()
        .transpose()
        .map_err(|_| SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)?
    {
        match Offset16::serialize_subset(&ligcaret_list, s, plan, (), ligcaret_list_offset_pos) {
            Ok(_) => true,
            Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => false,
            Err(e) => {
                return Err(e);
            }
        }
    } else {
        false
    };

    let subset_attach_list = if let Some(attach_list) = gdef
        .attach_list()
        .transpose()
        .map_err(|_| SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)?
    {
        match Offset16::serialize_subset(&attach_list, s, plan, (), attachlist_offset_pos) {
            Ok(_) => true,
            Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => false,
            Err(e) => {
                return Err(e);
            }
        }
    } else {
        false
    };

    let subset_glyph_classdef = if let Some(glyph_classdef) = gdef
        .glyph_class_def()
        .transpose()
        .map_err(|_| SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)?
    {
        match Offset16::serialize_subset(
            &glyph_classdef,
            s,
            plan,
            &ClassDefSubsetStruct {
                remap_class: false,
                keep_empty_table: false,
                use_class_zero: true,
                glyph_filter: None,
            },
            glyph_classdef_offset_pos,
        ) {
            Ok(_) => true,
            Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => false,
            Err(e) => {
                return Err(e);
            }
        }
    } else {
        false
    };

    // make sure repacker will not move the target subtable before the other children
    // ref: <https://github.com/harfbuzz/harfbuzz/blob/aad5780f5305f38ef128c61854c5d5a0c4ca3f4f/src/OT/Layout/GDEF/GDEF.hh#L665>
    if subset_varstore {
        s.repack_last(varstore_idx)?;
    }

    if subset_glyph_classdef
        || subset_attach_list
        || subset_ligcaret_list
        || subset_mark_attach_classdef
        || (version.minor >= 2 && subset_mark_glyphsets_def)
        || (version.minor >= 3 && subset_varstore)
    {
        state.has_gdef_varstore = subset_varstore;
        Ok(())
    } else {
        Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY)
    }
}

impl SubsetTable<'_> for AttachList<'_> {
    type ArgsForSubset = ();
    type Output = ();

    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        if self.coverage_offset().is_null() || self.glyph_count() == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        //coverage offset
        let coverage_offset_pos = s.embed(0_u16)?;
        //glyph_count
        let glyph_count_pos = s.embed(0_u16)?;

        let Ok(coverage) = self.coverage() else {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        };

        let attach_points = self.attach_points();
        let mut count = 0_u16;
        let src_glyph_count = self.glyph_count() as usize;
        let mut retained_glyphs =
            Vec::with_capacity(plan.glyph_map_gsub.len().min(src_glyph_count));

        for (idx, glyph) in coverage
            .iter()
            .enumerate()
            .take(plan.font_num_glyphs.min(src_glyph_count))
        {
            let Some(new_gid) = plan.glyph_map_gsub.get(&GlyphId::from(glyph)) else {
                continue;
            };

            match attach_points.subset_offset(idx, s, plan, ()) {
                Ok(()) => {
                    count += 1;
                    retained_glyphs.push(*new_gid);
                }
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => (),
                Err(e) => return Err(e),
            }
        }

        if retained_glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        s.copy_assign(glyph_count_pos, count);
        Offset16::serialize_serialize::<CoverageTable>(s, &retained_glyphs, coverage_offset_pos)
    }
}

impl SubsetTable<'_> for AttachPoint<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        _plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        s.embed_bytes(self.min_table_bytes()).map(|_| ())
    }
}

impl SubsetTable<'_> for LigCaretList<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        if self.coverage_offset().is_null() || self.lig_glyph_count() == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        //coverage offset
        let coverage_offset_pos = s.embed(0_u16)?;
        //lig_glyph_count
        let lig_glyph_count_pos = s.embed(0_u16)?;

        let Ok(coverage) = self.coverage() else {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        };

        let lig_glyphs = self.lig_glyphs();
        let mut count = 0_u16;
        let src_lig_glyph_count = self.lig_glyph_count() as usize;
        let mut retained_glyphs =
            Vec::with_capacity(plan.glyph_map_gsub.len().min(src_lig_glyph_count));

        for (idx, glyph) in coverage
            .iter()
            .enumerate()
            .take(plan.font_num_glyphs.min(src_lig_glyph_count))
        {
            let Some(new_gid) = plan.glyph_map_gsub.get(&GlyphId::from(glyph)) else {
                continue;
            };

            match lig_glyphs.subset_offset(idx, s, plan, ()) {
                Ok(()) => {
                    count += 1;
                    retained_glyphs.push(*new_gid);
                }
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => continue,
                Err(e) => return Err(e),
            }
        }

        if retained_glyphs.is_empty() {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        s.copy_assign(lig_glyph_count_pos, count);
        Offset16::serialize_serialize::<CoverageTable>(s, &retained_glyphs, coverage_offset_pos)
    }
}

impl SubsetTable<'_> for LigGlyph<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        if self.caret_count() == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        // caret count
        let caret_count_pos = s.embed(0_u16)?;

        let caret_values = self.caret_values();
        let mut count = 0_u16;
        for caret_value in caret_values.iter() {
            match caret_value {
                Err(ReadError::NullOffset) => continue,
                Err(_) => return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)),
                Ok(caret_value) => {
                    let snap = s.snapshot();
                    let offset_pos = s.allocate_size(Offset16::RAW_BYTE_LEN, true)?;

                    if let Err(e) =
                        Offset16::serialize_subset(&caret_value, s, plan, (), offset_pos)
                    {
                        s.revert_snapshot(snap);
                        return Err(e);
                    }
                    count += 1;
                }
            }
        }

        if count == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        s.copy_assign(caret_count_pos, count);
        Ok(())
    }
}

impl SubsetTable<'_> for MarkGlyphSets<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        let mark_glyph_set_count = self.mark_glyph_set_count() as usize;
        if mark_glyph_set_count == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }
        s.embed(self.format())?;

        let count_pos = s.embed(0_u16)?;

        let coverages = self.coverages();
        let mut count = 0_u16;

        for i in 0..mark_glyph_set_count {
            match coverages.subset_offset(i, s, plan, ()) {
                Ok(()) => {
                    count += 1;
                }
                Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY) => (),
                Err(e) => {
                    return Err(e);
                }
            }
        }

        if count == 0 {
            return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
        }

        s.copy_assign(count_pos, count);
        Ok(())
    }
}

impl SubsetTable<'_> for CaretValue<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        match self {
            Self::Format1(item) => item.subset(plan, s, args),
            Self::Format2(item) => item.subset(plan, s, args),
            Self::Format3(item) => item.subset(plan, s, args),
        }
    }
}

impl SubsetTable<'_> for CaretValueFormat1<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        _plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        s.embed_bytes(self.min_table_bytes()).map(|_| ())
    }
}

impl SubsetTable<'_> for CaretValueFormat2<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        _plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        s.embed_bytes(self.min_table_bytes()).map(|_| ())
    }
}

impl SubsetTable<'_> for CaretValueFormat3<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<Self::Output, SerializeErrorFlags> {
        if self.device_offset().is_null() {
            s.embed(1_u16)?;
            return s.embed(self.coordinate()).map(|_| ());
        }

        s.embed(self.caret_value_format())?;
        s.embed(self.coordinate())?;

        let device_offset_pos = s.embed(0_u16)?;
        let Ok(device) = self.device() else {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        };
        Offset16::serialize_subset(
            &device,
            s,
            plan,
            &plan.layout_varidx_delta_map,
            device_offset_pos,
        )
    }
}

impl CollectVariationIndices for Gdef<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        if let Some(Ok(lig_caret_list)) = self.lig_caret_list() {
            lig_caret_list.collect_variation_indices(plan, varidx_set);
        }
    }
}

impl CollectVariationIndices for LigCaretList<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        let Ok(coverage) = self.coverage() else {
            return;
        };

        let lig_glyphs = self.lig_glyphs();
        for (gid, lig_glyph) in coverage.iter().zip(lig_glyphs.iter_as_nullable()) {
            let Some(lig_glyph) = lig_glyph else {
                continue;
            };
            let Ok(lig_glyph) = lig_glyph else {
                return;
            };
            if !plan.glyphset_gsub.contains(GlyphId::from(gid)) {
                continue;
            }
            lig_glyph.collect_variation_indices(plan, varidx_set);
        }
    }
}

impl CollectVariationIndices for LigGlyph<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        for caret in self.caret_values().iter_as_nullable() {
            let Some(caret) = caret else {
                continue;
            };
            let Ok(caret) = caret else {
                return;
            };
            caret.collect_variation_indices(plan, varidx_set);
        }
    }
}

impl CollectVariationIndices for CaretValue<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        if let Self::Format3(item) = self {
            if let Ok(device) = item.device() {
                device.collect_variation_indices(plan, varidx_set);
            }
        }
    }
}

pub(crate) trait CollectUsedMarkSets {
    fn collect_used_mark_sets(&self, plan: &Plan, used_mark_sets: &mut IntSet<u16>);
}

impl CollectUsedMarkSets for Gdef<'_> {
    fn collect_used_mark_sets(&self, plan: &Plan, used_mark_sets: &mut IntSet<u16>) {
        if let Some(Ok(mark_glyph_sets)) = self.mark_glyph_sets_def() {
            mark_glyph_sets.collect_used_mark_sets(plan, used_mark_sets);
        };
    }
}

impl CollectUsedMarkSets for MarkGlyphSets<'_> {
    fn collect_used_mark_sets(&self, plan: &Plan, used_mark_sets: &mut IntSet<u16>) {
        for (i, coverage) in self.coverages().iter_as_nullable().enumerate() {
            let Some(coverage) = coverage else {
                continue;
            };
            let Ok(coverage) = coverage else {
                return;
            };
            if coverage.intersects(&plan.glyphset_gsub) {
                used_mark_sets.insert(i as u16);
            }
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use write_fonts::{
        read::{FontRef, TableProvider},
        types::GlyphId,
    };
    #[test]
    fn test_collect_var_indices() {
        let mut plan = Plan::default();
        plan.glyphset_gsub.insert(GlyphId::from(4_u32));
        plan.glyphset_gsub.insert(GlyphId::from(7_u32));

        let font =
            FontRef::new(include_bytes!("../test-data/fonts/AnekBangla-subset.ttf")).unwrap();
        let gdef = font.gdef().unwrap();

        let mut varidx_set = IntSet::empty();
        gdef.collect_variation_indices(&plan, &mut varidx_set);
        assert_eq!(varidx_set.len(), 3);
        assert!(varidx_set.contains(3));
        assert!(varidx_set.contains(5));
        assert!(varidx_set.contains(0));
    }

    #[test]
    fn test_collect_used_mark_sets() {
        let mut plan = Plan::default();
        plan.glyphset_gsub.insert(GlyphId::from(171_u32));

        let font = FontRef::new(include_bytes!("../test-data/fonts/Roboto-Regular.ttf")).unwrap();
        let gdef = font.gdef().unwrap();

        let mut used_mark_sets = IntSet::empty();
        gdef.collect_used_mark_sets(&plan, &mut used_mark_sets);
        assert_eq!(used_mark_sets.len(), 1);
        assert!(used_mark_sets.contains(0));
    }
}

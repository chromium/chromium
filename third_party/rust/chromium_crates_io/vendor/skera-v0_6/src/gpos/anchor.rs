//! impl subset() for Anchor subtable

use crate::{
    offset::SerializeSubset,
    serialize::{SerializeErrorFlags, Serializer},
    CollectVariationIndices, Plan, SubsetFlags, SubsetTable,
};
use write_fonts::{
    read::{
        collections::IntSet,
        tables::gpos::{AnchorFormat1, AnchorFormat2, AnchorFormat3, AnchorTable},
        MinByteRange,
    },
    types::Offset16,
};

impl SubsetTable<'_> for AnchorTable<'_> {
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

impl SubsetTable<'_> for AnchorFormat1<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        _plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        s.embed_bytes(self.min_table_bytes()).map(|_| ())
    }
}

impl SubsetTable<'_> for AnchorFormat2<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        if plan
            .subset_flags
            .contains(SubsetFlags::SUBSET_FLAGS_NO_HINTING)
        {
            // AnchorFormat 2 just containins extra hinting information
            // if hints are being dropped downgrade to format 1.
            s.embed(1_u16)?;
            s.embed(self.x_coordinate())?;
            s.embed(self.y_coordinate()).map(|_| ())
        } else {
            s.embed_bytes(self.min_table_bytes()).map(|_| ())
        }
    }
}

impl SubsetTable<'_> for AnchorFormat3<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        _args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let format_pos = s.embed(self.anchor_format())?;
        // TODO: update x/y coordinate when instancing
        s.embed(self.x_coordinate())?;
        s.embed(self.y_coordinate())?;

        // if both offsets are null, then we can downgrade to format 1
        // TODO: downgrade to format1 for instancing if possible
        let mut downgrade_to_format1 = true;
        let snap = s.snapshot();

        let x_device_offset_pos = s.embed(0_u16)?;
        if let Some(x_device) = self
            .x_device()
            .transpose()
            .map_err(|_| SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)?
        {
            downgrade_to_format1 = false;
            Offset16::serialize_subset(
                &x_device,
                s,
                plan,
                &plan.layout_varidx_delta_map,
                x_device_offset_pos,
            )?;
        }

        let y_device_offset_pos = s.embed(0_u16)?;
        if let Some(y_device) = self
            .y_device()
            .transpose()
            .map_err(|_| SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)?
        {
            downgrade_to_format1 = false;
            Offset16::serialize_subset(
                &y_device,
                s,
                plan,
                &plan.layout_varidx_delta_map,
                y_device_offset_pos,
            )?;
        }

        if downgrade_to_format1 {
            s.revert_snapshot(snap);
            s.copy_assign(format_pos, 1_u16);
        }
        Ok(())
    }
}

impl CollectVariationIndices for AnchorTable<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        if let Self::Format3(item) = self {
            item.collect_variation_indices(plan, varidx_set)
        }
    }
}

impl CollectVariationIndices for AnchorFormat3<'_> {
    fn collect_variation_indices(&self, plan: &Plan, varidx_set: &mut IntSet<u32>) {
        if let Some(Ok(x_device)) = self.x_device() {
            x_device.collect_variation_indices(plan, varidx_set);
        }
        if let Some(Ok(y_device)) = self.y_device() {
            y_device.collect_variation_indices(plan, varidx_set);
        }
    }
}

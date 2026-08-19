//! impl subset() for ValueRecord

use crate::{
    offset::SerializeSubset,
    serialize::{SerializeErrorFlags, Serializer},
    CollectVariationIndices, Plan, SubsetTable,
};
use write_fonts::{
    read::{
        collections::IntSet,
        tables::gpos::{ValueFormat, ValueRecord},
        FontData,
    },
    types::Offset16,
};

pub(crate) fn compute_effective_format(
    value_record: &ValueRecord,
    strip_hints: bool,
    strip_empty: bool,
) -> ValueFormat {
    let mut value_format = ValueFormat::empty();

    if let Some(x_placement) = value_record.x_placement {
        if !strip_empty || x_placement.get() != 0 {
            value_format |= ValueFormat::X_PLACEMENT;
        }
    }

    if let Some(y_placement) = value_record.y_placement {
        if !strip_empty || y_placement.get() != 0 {
            value_format |= ValueFormat::Y_PLACEMENT;
        }
    }

    if let Some(x_advance) = value_record.x_advance {
        if !strip_empty || x_advance.get() != 0 {
            value_format |= ValueFormat::X_ADVANCE;
        }
    }

    if let Some(y_advance) = value_record.y_advance {
        if !strip_empty || y_advance.get() != 0 {
            value_format |= ValueFormat::Y_ADVANCE;
        }
    }

    if !value_record.x_placement_device.get().is_null() && !strip_hints {
        value_format |= ValueFormat::X_PLACEMENT_DEVICE;
    }

    if !value_record.y_placement_device.get().is_null() && !strip_hints {
        value_format |= ValueFormat::Y_PLACEMENT_DEVICE;
    }

    if !value_record.x_advance_device.get().is_null() && !strip_hints {
        value_format |= ValueFormat::X_ADVANCE_DEVICE;
    }

    if !value_record.y_advance_device.get().is_null() && !strip_hints {
        value_format |= ValueFormat::Y_ADVANCE_DEVICE;
    }
    value_format
}

impl<'a> SubsetTable<'a> for ValueRecord {
    type ArgsForSubset = (ValueFormat, FontData<'a>);
    type Output = ();

    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        args: Self::ArgsForSubset,
    ) -> Result<(), SerializeErrorFlags> {
        let (new_format, font_data) = args;
        if new_format.is_empty() {
            return Ok(());
        }

        if new_format.contains(ValueFormat::X_PLACEMENT) {
            s.embed(self.x_placement().unwrap_or(0))?;
        }

        if new_format.contains(ValueFormat::Y_PLACEMENT) {
            s.embed(self.y_placement().unwrap_or(0))?;
        }

        if new_format.contains(ValueFormat::X_ADVANCE) {
            s.embed(self.x_advance().unwrap_or(0))?;
        }

        if new_format.contains(ValueFormat::Y_ADVANCE) {
            s.embed(self.y_advance().unwrap_or(0))?;
        }

        if !new_format.intersects(ValueFormat::ANY_DEVICE_OR_VARIDX) {
            return Ok(());
        }

        if new_format.contains(ValueFormat::X_PLACEMENT_DEVICE) {
            let offset_pos = s.embed(0_u16)?;
            if let Some(device) = self
                .x_placement_device(font_data)
                .transpose()
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
            {
                Offset16::serialize_subset(
                    &device,
                    s,
                    plan,
                    &plan.layout_varidx_delta_map,
                    offset_pos,
                )?;
            }
        }

        if new_format.contains(ValueFormat::Y_PLACEMENT_DEVICE) {
            let offset_pos = s.embed(0_u16)?;
            if let Some(device) = self
                .y_placement_device(font_data)
                .transpose()
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
            {
                Offset16::serialize_subset(
                    &device,
                    s,
                    plan,
                    &plan.layout_varidx_delta_map,
                    offset_pos,
                )?;
            }
        }

        if new_format.contains(ValueFormat::X_ADVANCE_DEVICE) {
            let offset_pos = s.embed(0_u16)?;
            if let Some(device) = self
                .x_advance_device(font_data)
                .transpose()
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
            {
                Offset16::serialize_subset(
                    &device,
                    s,
                    plan,
                    &plan.layout_varidx_delta_map,
                    offset_pos,
                )?;
            }
        }

        if new_format.contains(ValueFormat::Y_ADVANCE_DEVICE) {
            let offset_pos = s.embed(0_u16)?;
            if let Some(device) = self
                .y_advance_device(font_data)
                .transpose()
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
            {
                Offset16::serialize_subset(
                    &device,
                    s,
                    plan,
                    &plan.layout_varidx_delta_map,
                    offset_pos,
                )?;
            }
        }
        Ok(())
    }
}

pub(crate) fn collect_variation_indices(
    value_record: &ValueRecord,
    font_data: FontData,
    plan: &Plan,
    varidx_set: &mut IntSet<u32>,
) {
    let value_format = value_record.format;
    if !value_format.intersects(ValueFormat::ANY_DEVICE_OR_VARIDX) {
        return;
    }

    if let Some(Ok(x_pla_device)) = value_record.x_placement_device(font_data) {
        x_pla_device.collect_variation_indices(plan, varidx_set);
    }

    if let Some(Ok(y_pla_device)) = value_record.y_placement_device(font_data) {
        y_pla_device.collect_variation_indices(plan, varidx_set);
    }

    if let Some(Ok(x_adv_device)) = value_record.x_advance_device(font_data) {
        x_adv_device.collect_variation_indices(plan, varidx_set);
    }

    if let Some(Ok(y_adv_device)) = value_record.y_advance_device(font_data) {
        y_adv_device.collect_variation_indices(plan, varidx_set);
    }
}

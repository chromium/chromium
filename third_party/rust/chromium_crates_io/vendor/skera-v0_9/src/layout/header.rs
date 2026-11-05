//! Serialization of the extended GSUB/GPOS header.

use crate::{
    offset::SerializeSubset,
    serialize::{SerializeErrorFlags, Serializer},
    Plan, SubsetTable,
};
use read_fonts::types::{Offset16, Offset32};

pub(crate) fn serialize_list<'a, T: SubsetTable<'a>>(
    table: &T,
    s: &mut Serializer,
    plan: &Plan,
    args: T::ArgsForSubset,
    positions: [usize; 2],
    wide: bool,
) -> Result<T::Output, SerializeErrorFlags> {
    if wide {
        Offset32::serialize_subset(table, s, plan, args, positions[1])
    } else {
        Offset16::serialize_subset(table, s, plan, args, positions[0])
    }
}

macro_rules! subset_extended_layout {
    ($table:ident, $lookups:ident) => {
        fn subset_extended_layout(
            table: &$table<'_>,
            plan: &Plan,
            font: &FontRef<'_>,
            state: &SubsetState,
            s: &mut Serializer,
        ) -> Result<(), SerializeErrorFlags> {
            use crate::serialize::SerializeResultEmpty;
            if table.version() != MajorMinor::new(1, 2) {
                return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER));
            }
            let script_offset = table
                .script_list2_offset()
                .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
            let feature_offset = table
                .feature_list2_offset()
                .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
            let lookup_offset = table
                .lookup_list2_offset()
                .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;

            s.embed(table.version())?;
            let script16_pos = s.embed(0u16)?;
            let feature16_pos = s.embed(0u16)?;
            let lookup16_pos = s.embed(0u16)?;
            let variations_pos = s.embed(0u32)?;
            let script32_pos = s.embed(0u32)?;
            let feature32_pos = s.embed(0u32)?;
            let lookup32_pos = s.embed(0u32)?;
            let mut c = SubsetLayoutContext::new($table::TAG);

            if !script_offset.is_null() || !table.script_list_offset().is_null() {
                let list = table
                    .script_list()
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
                crate::layout::header::serialize_list(
                    &list,
                    s,
                    plan,
                    &mut c,
                    [script16_pos, script32_pos],
                    !script_offset.is_null(),
                )?;
            }
            if !feature_offset.is_null() || !table.feature_list_offset().is_null() {
                let list = table
                    .feature_list()
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
                crate::layout::header::serialize_list(
                    &list,
                    s,
                    plan,
                    &mut c,
                    [feature16_pos, feature32_pos],
                    !feature_offset.is_null(),
                )?;
            }
            if !lookup_offset.is_null() || !table.lookup_list_offset().is_null() {
                let list = table
                    .lookup_list()
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
                crate::layout::header::serialize_list(
                    &list,
                    s,
                    plan,
                    (state, font, &plan.$lookups),
                    [lookup16_pos, lookup32_pos],
                    !lookup_offset.is_null(),
                )?;
            }
            if let Some(variations) = table
                .feature_variations()
                .transpose()
                .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
            {
                Offset32::serialize_subset(&variations, s, plan, &mut c, variations_pos)
                    .is_empty()?;
            }
            // The extended list fields require version 1.2 even without FeatureVariations.
            Ok(())
        }
    };
}

pub(crate) use subset_extended_layout;

#[cfg(test)]
mod tests;

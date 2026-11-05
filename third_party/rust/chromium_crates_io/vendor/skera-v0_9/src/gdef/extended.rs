//! Serialization of the GDEF 1.4 header.

use super::*;
use crate::layout::header::serialize_list;
use read_fonts::types::MajorMinor;

pub(super) fn subset_extended_gdef(
    gdef: &Gdef<'_>,
    plan: &Plan,
    s: &mut Serializer,
    state: &mut SubsetState,
) -> Result<(), SerializeErrorFlags> {
    if gdef.version() != MajorMinor::new(1, 4) {
        return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER));
    }
    let mut wide = [false; 5];
    for (i, offset) in [
        gdef.glyph_class_def2_offset(),
        gdef.attach_list2_offset(),
        gdef.lig_caret_list2_offset(),
        gdef.mark_attach_class_def2_offset(),
        gdef.mark_glyph_sets_def2_offset(),
    ]
    .into_iter()
    .enumerate()
    {
        wide[i] = !offset
            .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
            .is_null();
    }
    s.embed(gdef.version())?;
    let legacy = [
        s.embed(0u16)?,
        s.embed(0u16)?,
        s.embed(0u16)?,
        s.embed(0u16)?,
        s.embed(0u16)?,
    ];
    let varstore_pos = s.embed(0u32)?;
    let extended = [
        s.embed(0u32)?,
        s.embed(0u32)?,
        s.embed(0u32)?,
        s.embed(0u32)?,
        s.embed(0u32)?,
    ];

    // Serialize the variation store first so it can stay last in the output,
    // matching the legacy header's compatibility handling (HB issue #4636).
    let varstore_index = if let Some(store) = gdef
        .item_var_store()
        .transpose()
        .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
    {
        if Offset32::serialize_subset(
            &store,
            s,
            plan,
            (&plan.gdef_varstore_inner_maps, false),
            varstore_pos,
        )
        .is_empty()?
        {
            None
        } else {
            Some(
                s.last_added_child_index()
                    .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER))?,
            )
        }
    } else {
        None
    };
    let mut has_tables = varstore_index.is_some();
    let class_args = ClassDefSubsetStruct {
        remap_class: false,
        keep_empty_table: false,
        use_class_zero: true,
        glyph_filter: None,
    };
    for (table, i) in [
        (gdef.glyph_class_def(), 0),
        (gdef.mark_attach_class_def(), 3),
    ] {
        if let Some(table) = table
            .transpose()
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
        {
            has_tables |= !serialize_list(
                &table,
                s,
                plan,
                &class_args,
                [legacy[i], extended[i]],
                wide[i],
            )
            .map(|_| ())
            .is_empty()?;
        }
    }
    if let Some(table) = gdef
        .attach_list()
        .transpose()
        .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
    {
        has_tables |=
            !serialize_list(&table, s, plan, (), [legacy[1], extended[1]], wide[1]).is_empty()?;
    }
    if let Some(table) = gdef
        .lig_caret_list()
        .transpose()
        .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
    {
        has_tables |=
            !serialize_list(&table, s, plan, (), [legacy[2], extended[2]], wide[2]).is_empty()?;
    }
    if let Some(table) = gdef
        .mark_glyph_sets_def()
        .transpose()
        .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?
    {
        has_tables |=
            !serialize_list(&table, s, plan, (), [legacy[4], extended[4]], wide[4]).is_empty()?;
    }
    if let Some(index) = varstore_index {
        s.repack_last(index)?;
    }
    if !has_tables {
        return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
    }
    // Keep the extended fields even when no variation store remains.
    state.has_gdef_varstore = varstore_index.is_some();
    Ok(())
}

#[cfg(test)]
mod tests;

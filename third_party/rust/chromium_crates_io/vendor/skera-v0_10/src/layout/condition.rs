//! Subsetting and variation-index collection for layout conditions.

use super::*;
use read_fonts::{
    types::{Offset24, Scalar},
    Offset,
};

const CONDITION_OPERATIONS: usize = u16::MAX as usize + 1;

struct NestedCondition<'a> {
    table: Condition<'a>,
    depth: usize,
}

impl<'a> SubsetTable<'a> for NestedCondition<'_> {
    type ArgsForSubset = &'a mut usize;
    type Output = ();

    fn subset(
        &self,
        plan: &Plan,
        s: &mut Serializer,
        remaining: &mut usize,
    ) -> Result<(), SerializeErrorFlags> {
        if self.depth == 0 || *remaining == 0 {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        }
        *remaining -= 1;
        match &self.table {
            Condition::Format1AxisRange(table) => table.subset(plan, s, ()),
            Condition::Format2VariableValue(table) => {
                let (index, delta) = plan
                    .layout_varidx_delta_map
                    .get(&table.var_index())
                    .copied()
                    .unwrap_or((NO_VARIATION_INDEX, 0));
                let value = i32::from(table.default_value())
                    .checked_add(delta)
                    .and_then(|value| i16::try_from(value).ok())
                    .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
                s.embed(table.format())?;
                s.embed(value)?;
                s.embed(index).map(|_| ())
            }
            Condition::Format3And(table) => {
                s.embed(table.format())?;
                s.embed(table.condition_count())?;
                subset_condition_offsets(
                    table.conditions(),
                    usize::from(table.condition_count()),
                    plan,
                    s,
                    self.depth - 1,
                    remaining,
                    true,
                )
                .map(|_| ())
            }
            Condition::Format4Or(table) => {
                s.embed(table.format())?;
                s.embed(table.condition_count())?;
                subset_condition_offsets(
                    table.conditions(),
                    usize::from(table.condition_count()),
                    plan,
                    s,
                    self.depth - 1,
                    remaining,
                    true,
                )
                .map(|_| ())
            }
            Condition::Format5Negate(table) => {
                s.embed(table.format())?;
                let pos = s.allocate_size(Offset24::RAW_BYTE_LEN, true)?;
                if table.condition_offset().is_null() {
                    return Ok(());
                }
                let child = table
                    .condition()
                    .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
                Offset24::serialize_subset(
                    &NestedCondition {
                        table: child,
                        depth: self.depth - 1,
                    },
                    s,
                    plan,
                    remaining,
                    pos,
                )
            }
        }
    }
}

fn subset_condition_offsets<'a, O>(
    conditions: ArrayOfOffsets<'a, Condition<'a>, O>,
    count: usize,
    plan: &Plan,
    s: &mut Serializer,
    depth: usize,
    remaining: &mut usize,
    keep_nulls: bool,
) -> Result<usize, SerializeErrorFlags>
where
    O: Offset + Scalar + FixedSize + SerializeSubset,
{
    if conditions.len() != count {
        return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
    }
    let mut retained = 0;
    for i in 0..count {
        let child = match conditions.get(i) {
            Err(ReadError::NullOffset) => {
                if keep_nulls {
                    // Null means true. OR and NOT must not lose that operand.
                    s.allocate_size(O::RAW_BYTE_LEN, true)?;
                    retained += 1;
                }
                continue;
            }
            Err(_) => return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)),
            Ok(child) => child,
        };
        let pos = s.allocate_size(O::RAW_BYTE_LEN, true)?;
        O::serialize_subset(
            &NestedCondition {
                table: child,
                depth,
            },
            s,
            plan,
            &mut *remaining,
            pos,
        )?;
        retained += 1;
    }
    Ok(retained)
}

impl SubsetTable<'_> for Condition<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(&self, plan: &Plan, s: &mut Serializer, _: ()) -> Result<(), SerializeErrorFlags> {
        let mut remaining = CONDITION_OPERATIONS;
        NestedCondition {
            table: self.clone(),
            depth: crate::MAX_NESTING_LEVEL,
        }
        .subset(plan, s, &mut remaining)
    }
}

impl SubsetTable<'_> for ConditionSet<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(&self, plan: &Plan, s: &mut Serializer, _: ()) -> Result<(), SerializeErrorFlags> {
        let pos = s.embed(0u16)?;
        let mut remaining = CONDITION_OPERATIONS;
        let count = subset_condition_offsets(
            self.conditions(),
            usize::from(self.condition_count()),
            plan,
            s,
            crate::MAX_NESTING_LEVEL,
            &mut remaining,
            false,
        )?;
        let count = u16::try_from(count)
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
        s.copy_assign(pos, count);
        Ok(())
    }
}

impl SubsetTable<'_> for ConditionFormat1<'_> {
    type ArgsForSubset = ();
    type Output = ();
    fn subset(&self, _: &Plan, s: &mut Serializer, _: ()) -> Result<(), SerializeErrorFlags> {
        s.embed_bytes(self.min_table_bytes()).map(|_| ())
    }
}

fn collect_condition_indices(
    table: &Condition<'_>,
    depth: usize,
    remaining: &mut usize,
    indices: &mut IntSet<u32>,
) -> Result<(), ReadError> {
    if depth == 0 || *remaining == 0 {
        return Err(ReadError::OutOfBounds);
    }
    *remaining -= 1;
    match table {
        Condition::Format1AxisRange(_) => Ok(()),
        Condition::Format2VariableValue(table) => {
            if table.var_index() != NO_VARIATION_INDEX {
                indices.insert(table.var_index());
            }
            Ok(())
        }
        Condition::Format3And(table) => collect_condition_offsets(
            table.conditions(),
            usize::from(table.condition_count()),
            depth - 1,
            remaining,
            indices,
        ),
        Condition::Format4Or(table) => collect_condition_offsets(
            table.conditions(),
            usize::from(table.condition_count()),
            depth - 1,
            remaining,
            indices,
        ),
        Condition::Format5Negate(table) => {
            if table.condition_offset().is_null() {
                return Ok(());
            }
            collect_condition_indices(&table.condition()?, depth - 1, remaining, indices)
        }
    }
}

fn collect_condition_offsets<'a, O: Offset + Scalar>(
    conditions: ArrayOfOffsets<'a, Condition<'a>, O>,
    count: usize,
    depth: usize,
    remaining: &mut usize,
    indices: &mut IntSet<u32>,
) -> Result<(), ReadError> {
    if conditions.len() != count {
        return Err(ReadError::OutOfBounds);
    }
    for i in 0..count {
        let child = match conditions.get(i) {
            Err(ReadError::NullOffset) => continue,
            other => other?,
        };
        collect_condition_indices(&child, depth, remaining, indices)?;
    }
    Ok(())
}

impl CollectVariationIndices for Condition<'_> {
    fn collect_variation_indices(&self, _: &Plan, indices: &mut IntSet<u32>) {
        let mut remaining = CONDITION_OPERATIONS;
        let _ = collect_condition_indices(self, crate::MAX_NESTING_LEVEL, &mut remaining, indices);
    }
}

impl CollectVariationIndices for ConditionSet<'_> {
    fn collect_variation_indices(&self, _: &Plan, indices: &mut IntSet<u32>) {
        let mut remaining = CONDITION_OPERATIONS;
        let _ = collect_condition_offsets(
            self.conditions(),
            usize::from(self.condition_count()),
            crate::MAX_NESTING_LEVEL,
            &mut remaining,
            indices,
        );
    }
}

pub(crate) fn collect_feature_variation_condition_indices(
    variations: &FeatureVariations<'_>,
    plan: &Plan,
    features: &FastHashMap<u16, u16>,
    lookups: &FastHashMap<u16, u16>,
    indices: &mut IntSet<u32>,
) {
    let Ok(count) = super::num_variation_record_to_retain(variations, features) else {
        return;
    };
    // Earlier records can still block a later retained substitution.
    for record in &variations.feature_variation_records()[..count as usize] {
        if let Some(Ok(conditions)) = record.condition_set(variations.offset_data()) {
            conditions.collect_variation_indices(plan, indices);
        }
    }
    for record in variations.lookup_variation_records().unwrap_or_default() {
        if !features.contains_key(&record.feature_index())
            || record.feature_lookups_offset().is_null()
        {
            continue;
        }
        let Ok(table) = record.feature_lookups(variations.offset_data()) else {
            return;
        };
        if u64::from(table.lookup_condition_count()) * 8
            > table.offset_data().len().saturating_sub(10) as u64
        {
            return;
        }
        for record in table.lookup_condition_records() {
            if record.lookup_index_list_offset().is_null() {
                continue;
            }
            let Ok(list) = record.lookup_index_list(table.offset_data()) else {
                return;
            };
            if list.min_table_bytes().is_empty() {
                return;
            }
            if list
                .lookup_indices()
                .iter()
                .any(|i| lookups.contains_key(&i.get()))
            {
                if let Some(Ok(condition)) = record.condition(table.offset_data()) {
                    condition.collect_variation_indices(plan, indices);
                }
            }
        }
    }
}

#[cfg(test)]
mod tests;

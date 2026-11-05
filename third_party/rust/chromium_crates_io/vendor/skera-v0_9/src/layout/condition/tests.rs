use super::*;
use font_test_data::bebuffer::BeBuffer;
use read_fonts::types::{F2Dot14, Uint24};

fn variable(value: i16, index: u32) -> Vec<u8> {
    BeBuffer::new().push(2u16).push(value).push(index).to_vec()
}

fn compound(format: u16, children: &[Option<Vec<u8>>]) -> Vec<u8> {
    let mut bytes = BeBuffer::new().push(format).push(children.len() as u8);
    let mut offset = 3 + children.len() * 3;
    for child in children {
        bytes = bytes.push(Uint24::new(if child.is_some() { offset as u32 } else { 0 }));
        offset += child.as_ref().map_or(0, Vec::len);
    }
    let mut bytes = bytes.to_vec();
    for child in children.iter().flatten() {
        bytes.extend(child);
    }
    bytes
}

fn negate(child: Option<Vec<u8>>) -> Vec<u8> {
    let mut bytes = BeBuffer::new()
        .push(5u16)
        .push(Uint24::new(if child.is_some() { 5 } else { 0 }))
        .to_vec();
    if let Some(child) = child {
        bytes.extend(child);
    }
    bytes
}

fn subset<T>(table: &T, plan: &Plan) -> Result<Vec<u8>, SerializeErrorFlags>
where
    for<'a> T: SubsetTable<'a, ArgsForSubset = (), Output = ()>,
{
    let mut s = Serializer::new(1024 * 1024);
    s.start_serialize().unwrap();
    table.subset(plan, &mut s, ())?;
    s.end_serialize();
    if s.in_error() {
        return Err(s.error());
    }
    Ok(s.copy_bytes())
}

#[test]
fn conditions_preserve_all_formats_and_remap_variable_values() {
    let axis = BeBuffer::new()
        .push(1u16)
        .push(0u16)
        .push(F2Dot14::from_f32(-0.5))
        .push(F2Dot14::from_f32(0.5))
        .to_vec();
    let mut plan = Plan::default();
    plan.layout_varidx_delta_map.insert(0x10002, (0x70008, 3));
    let bytes = compound(
        3,
        &[
            Some(axis),
            Some(compound(
                4,
                &[
                    Some(variable(-5, 0x10002)),
                    Some(negate(Some(variable(1, NO_VARIATION_INDEX)))),
                ],
            )),
        ],
    );
    let table = Condition::read(FontData::new(&bytes)).unwrap();
    let output = subset(&table, &plan).unwrap();
    let output = Condition::read(FontData::new(&output)).unwrap();
    for coord in [-0.75, 0.0, 0.75] {
        for delta in [0.0, 5.0, 10.0] {
            let coords = [F2Dot14::from_f32(coord)];
            let before = table
                .evaluate(&coords, |index| {
                    assert_eq!(index, 0x10002);
                    Ok::<_, read_fonts::model::Error>(delta)
                })
                .unwrap();
            let after = output
                .evaluate(&coords, |index| {
                    assert_eq!(index, 0x70008);
                    Ok::<_, read_fonts::model::Error>(delta - 3.0)
                })
                .unwrap();
            assert_eq!(before, after);
        }
    }
    let mut indices = IntSet::empty();
    output.collect_variation_indices(&plan, &mut indices);
    assert_eq!(indices.iter().collect::<Vec<_>>(), [0x70008]);
    let bytes = variable(-5, 0x10002);
    let table = Condition::read(FontData::new(&bytes)).unwrap();
    let output = subset(&table, &plan).unwrap();
    let Condition::Format2VariableValue(output) = Condition::read(FontData::new(&output)).unwrap()
    else {
        panic!()
    };
    assert_eq!(output.default_value(), -2);
    assert_eq!(output.var_index(), 0x70008);
    let output = subset(&table, &Plan::default()).unwrap();
    let Condition::Format2VariableValue(output) = Condition::read(FontData::new(&output)).unwrap()
    else {
        panic!()
    };
    assert_eq!(output.default_value(), -5);
    assert_eq!(output.var_index(), NO_VARIATION_INDEX);
    plan.layout_varidx_delta_map.insert(0x10002, (0, i32::MAX));
    assert_eq!(
        subset(&table, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW)
    );
}

#[test]
fn conditions_keep_null_boolean_operands_and_condition_set_members() {
    for (bytes, expected) in [
        (
            compound(4, &[Some(variable(-1, NO_VARIATION_INDEX)), None]),
            true,
        ),
        (
            compound(3, &[Some(variable(-1, NO_VARIATION_INDEX)), None]),
            false,
        ),
        (negate(None), false),
        (compound(3, &[]), true),
        (compound(4, &[]), false),
    ] {
        let table = Condition::read(FontData::new(&bytes)).unwrap();
        let output = subset(&table, &Plan::default()).unwrap();
        let output = Condition::read(FontData::new(&output)).unwrap();
        assert_eq!(
            output
                .evaluate(&[], |_| Ok::<_, read_fonts::model::Error>(0.0))
                .unwrap(),
            expected
        );
        if let Condition::Format4Or(table) = output {
            if table.condition_count() != 0 {
                assert_eq!(table.condition_count(), 2);
                assert!(table.condition_offsets()[1].get().is_null());
            }
        }
    }
    let mut bytes = BeBuffer::new()
        .push(3u16)
        .push(0u32)
        .push(14u32)
        .push(22u32)
        .to_vec();
    bytes.extend(variable(-1, NO_VARIATION_INDEX));
    bytes.extend(negate(None));
    let table = ConditionSet::read(FontData::new(&bytes)).unwrap();
    let output = subset(&table, &Plan::default()).unwrap();
    let output = ConditionSet::read(FontData::new(&output)).unwrap();
    assert_eq!(output.condition_count(), 2);
    assert!(matches!(
        output.conditions().get(0).unwrap(),
        Condition::Format2VariableValue(_)
    ));
    assert!(matches!(
        output.conditions().get(1).unwrap(),
        Condition::Format5Negate(_)
    ));
    assert!(!output
        .evaluate(&[], |_| Ok::<_, read_fonts::model::Error>(0.0))
        .unwrap());
}

#[test]
fn condition_serialization_rejects_truncation_and_bounds_tree_work() {
    let plan = Plan::default();
    for bytes in [vec![0, 3, 1], vec![0, 4, 1], vec![0, 5, 255, 255, 255]] {
        let table = Condition::read(FontData::new(&bytes)).unwrap();
        assert_eq!(
            subset(&table, &plan),
            Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
        );
    }
    let mut bytes = variable(1, NO_VARIATION_INDEX);
    for _ in 1..crate::MAX_NESTING_LEVEL {
        bytes = negate(Some(bytes));
    }
    let table = Condition::read(FontData::new(&bytes)).unwrap();
    assert!(subset(&table, &plan).is_ok());
    bytes = negate(Some(bytes));
    let table = Condition::read(FontData::new(&bytes)).unwrap();
    assert_eq!(
        subset(&table, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
    );
    // A small DAG can refer to the same subtree exponentially many times.
    let mut bytes = Vec::new();
    for _ in 0..16 {
        bytes.extend(
            BeBuffer::new()
                .push(3u16)
                .push(2u8)
                .push(Uint24::new(9))
                .push(Uint24::new(9))
                .to_vec(),
        );
    }
    bytes.extend(variable(1, 0x10002));
    let table = Condition::read(FontData::new(&bytes)).unwrap();
    assert_eq!(
        subset(&table, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
    );
    let mut indices = IntSet::empty();
    table.collect_variation_indices(&plan, &mut indices);
    assert!(indices.contains(0x10002));
    let bytes = BeBuffer::new().push(1u16).to_vec();
    let table = ConditionSet::read(FontData::new(&bytes)).unwrap();
    assert_eq!(
        subset(&table, &plan),
        Err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR)
    );
}

#[test]
fn feature_variations_collect_conditional_only_and_earlier_barrier_indices() {
    let mut bytes = BeBuffer::new()
        .push(1u16)
        .push(1u16)
        .push(0u32)
        .push(1u32)
        .push(0u16)
        .push(18u32)
        .push(1u16)
        .push(0u16)
        .push(0u16)
        .push(1u32)
        .push(22u32)
        .push(18u32)
        .push(1u16)
        .push(7u16)
        .to_vec();
    bytes.extend(variable(1, 0x10002));
    let table = FeatureVariations::read(FontData::new(&bytes)).unwrap();
    let features = [(0, 0)].into_iter().collect();
    let lookups = [(7, 0)].into_iter().collect();
    let mut indices = IntSet::empty();
    collect_feature_variation_condition_indices(
        &table,
        &Plan::default(),
        &features,
        &lookups,
        &mut indices,
    );
    assert_eq!(indices.iter().collect::<Vec<_>>(), [0x10002]);
    indices.clear();
    collect_feature_variation_condition_indices(
        &table,
        &Plan::default(),
        &features,
        &FastHashMap::default(),
        &mut indices,
    );
    assert!(indices.is_empty());

    let mut bytes = BeBuffer::new()
        .push(1u16)
        .push(0u16)
        .push(2u32)
        .push(24u32)
        .push(0u32)
        .push(0u32)
        .push(38u32)
        .push(1u16)
        .push(6u32)
        .to_vec();
    bytes.extend(variable(1, 0x10002));
    bytes.extend(
        BeBuffer::new()
            .push(1u16)
            .push(0u16)
            .push(1u16)
            .push(0u16)
            .push(12u32)
            .push(0u16)
            .push(1u16)
            .push(7u16)
            .to_vec(),
    );
    let table = FeatureVariations::read(FontData::new(&bytes)).unwrap();
    collect_feature_variation_condition_indices(
        &table,
        &Plan::default(),
        &features,
        &lookups,
        &mut indices,
    );
    assert_eq!(indices.iter().collect::<Vec<_>>(), [0x10002]);
}

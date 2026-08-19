//! impl subset() for STAT table

use crate::{NameIdClosure, Plan};
use write_fonts::read::tables::stat::Stat;

impl NameIdClosure for Stat<'_> {
    //TODO: support instancing
    fn collect_name_ids(&self, plan: &mut Plan) {
        if let Ok(axis_records) = self.design_axes() {
            plan.name_ids
                .extend_unsorted(axis_records.iter().map(|x| x.axis_name_id()));
        }

        if let Some(Ok(axis_values)) = self.offset_to_axis_values() {
            plan.name_ids
                .extend_unsorted(axis_values.axis_values().iter().filter_map(|x| match x {
                    Ok(axis_value) => Some(axis_value.value_name_id()),
                    _ => None,
                }));
        }
        if let Some(name_id) = self.elided_fallback_name_id() {
            plan.name_ids.insert(name_id);
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use write_fonts::read::{types::NameId, FontRef, TableProvider};
    #[test]
    fn test_nameid_closure() {
        let mut plan = Plan::default();
        let font = FontRef::new(font_test_data::MATERIAL_SYMBOLS_SUBSET).unwrap();

        let stat = font.stat().unwrap();
        stat.collect_name_ids(&mut plan);
        assert_eq!(plan.name_ids.len(), 14);
        assert!(plan.name_ids.contains(NameId::new(2)));
        assert!(plan.name_ids.contains(NameId::new(256)));
        assert!(plan.name_ids.contains(NameId::new(257)));
        assert!(plan.name_ids.contains(NameId::new(258)));
        assert!(plan.name_ids.contains(NameId::new(259)));
        assert!(plan.name_ids.contains(NameId::new(267)));
        assert!(plan.name_ids.contains(NameId::new(268)));
        assert!(plan.name_ids.contains(NameId::new(260)));
        assert!(plan.name_ids.contains(NameId::new(261)));
        assert!(plan.name_ids.contains(NameId::new(262)));
        assert!(plan.name_ids.contains(NameId::new(263)));
        assert!(plan.name_ids.contains(NameId::new(264)));
        assert!(plan.name_ids.contains(NameId::new(265)));
        assert!(plan.name_ids.contains(NameId::new(266)));
    }
}

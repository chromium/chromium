//! impl subset() for fvar table

use crate::{NameIdClosure, Plan};
use write_fonts::read::tables::fvar::Fvar;

impl NameIdClosure for Fvar<'_> {
    //TODO: support partial-instancing
    fn collect_name_ids(&self, plan: &mut Plan) {
        let Ok(axis_instance_array) = self.axis_instance_arrays() else {
            return;
        };
        plan.name_ids
            .extend_unsorted(axis_instance_array.axes().iter().map(|x| x.axis_name_id()));

        for instance_record in axis_instance_array
            .instances()
            .iter()
            .filter_map(|x| x.ok())
        {
            plan.name_ids.insert(instance_record.subfamily_name_id);
            if let Some(name_id) = instance_record.post_script_name_id {
                plan.name_ids.insert(name_id);
            }
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

        let fvar = font.fvar().unwrap();
        fvar.collect_name_ids(&mut plan);
        assert_eq!(plan.name_ids.len(), 11);
        assert!(plan.name_ids.contains(NameId::new(256)));
        assert!(plan.name_ids.contains(NameId::new(257)));
        assert!(plan.name_ids.contains(NameId::new(258)));
        assert!(plan.name_ids.contains(NameId::new(259)));
        assert!(plan.name_ids.contains(NameId::new(260)));
        assert!(plan.name_ids.contains(NameId::new(261)));
        assert!(plan.name_ids.contains(NameId::new(262)));
        assert!(plan.name_ids.contains(NameId::new(263)));
        assert!(plan.name_ids.contains(NameId::new(264)));
        assert!(plan.name_ids.contains(NameId::new(265)));
        assert!(plan.name_ids.contains(NameId::new(266)));
    }
}

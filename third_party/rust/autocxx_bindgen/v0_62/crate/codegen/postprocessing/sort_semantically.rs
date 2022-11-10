use syn::{
    visit_mut::{visit_item_mod_mut, VisitMut},
    Item, ItemMod,
};

pub(super) fn sort_semantically(item_mod: &mut ItemMod) {
    Visitor.visit_item_mod_mut(item_mod)
}

struct Visitor;

impl VisitMut for Visitor {
    fn visit_item_mod_mut(&mut self, item_mod: &mut ItemMod) {
        if let Some((_, ref mut items)) = item_mod.content {
            items.sort_by_key(|item| match item {
                Item::Type(_) => 0,
                Item::Struct(_) => 1,
                Item::Const(_) => 2,
                Item::Fn(_) => 3,
                Item::Enum(_) => 4,
                Item::Union(_) => 5,
                Item::Static(_) => 6,
                Item::Trait(_) => 7,
                Item::TraitAlias(_) => 8,
                Item::Impl(_) => 9,
                Item::Mod(_) => 10,
                Item::Use(_) => 11,
                Item::Verbatim(_) => 12,
                Item::ExternCrate(_) => 13,
                Item::ForeignMod(_) => 14,
                Item::Macro(_) => 15,
                Item::Macro2(_) => 16,
                _ => 18,
            });
        }
        visit_item_mod_mut(self, item_mod)
    }
}

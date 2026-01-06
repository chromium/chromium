use std::fmt;
use std::sync::Arc;

use crate::value::{Enumerator, Object, Value, ValueMap};

/// Holds the locals of a module.
#[derive(Debug, Default)]
pub(crate) struct Module {
    values: ValueMap,
    captured: Value,
}

impl Module {
    pub fn new(values: ValueMap, captured: Value) -> Self {
        Self { values, captured }
    }
}

impl Object for Module {
    fn get_value(self: &Arc<Self>, key: &Value) -> Option<Value> {
        self.values.get(key).cloned()
    }

    fn enumerate(self: &Arc<Self>) -> Enumerator {
        let keys = self.values.keys().cloned();
        Enumerator::Values(keys.collect())
    }

    fn render(self: &Arc<Self>, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.captured)
    }
}

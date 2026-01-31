use std::collections::BTreeMap;
use std::sync::{Arc, Mutex};

use crate::value::{Enumerator, Object, Value};

/// Closure cycle breaker utility.
///
/// The closure tracker is a crude way to forcefully break the cycles
/// caused by closures on teardown of the state.  Whenever a closure is
/// exposed to the engine it's also passed to the [`track_closure`] function.
#[derive(Default)]
pub(crate) struct ClosureTracker {
    closures: Mutex<Vec<Arc<Closure>>>,
}

impl ClosureTracker {
    /// This accepts a closure as value and registers it in the
    /// tracker for cycle breaking.
    pub(crate) fn track_closure(&self, closure: Arc<Closure>) {
        self.closures.lock().unwrap().push(closure);
    }
}

impl Drop for ClosureTracker {
    fn drop(&mut self) {
        for closure in self.closures.lock().unwrap().iter() {
            closure.clear();
        }
    }
}

/// Utility to enclose values for macros.
///
/// See `closure` on the [`Frame`] for how it's used.
#[derive(Debug, Default)]
pub(crate) struct Closure {
    values: Mutex<BTreeMap<Arc<str>, Value>>,
}

impl Closure {
    /// Stores a value by key in the closure.
    pub fn store(&self, key: &str, value: Value) {
        self.values.lock().unwrap().insert(Arc::from(key), value);
    }

    /// Upset a value into the closure.
    #[cfg(feature = "macros")]
    pub fn store_if_missing<F: FnOnce() -> Value>(&self, key: &str, f: F) {
        let mut values = self.values.lock().unwrap();
        if !values.contains_key(key) {
            values.insert(Arc::from(key), f());
        }
    }

    /// Clears the closure.
    ///
    /// This is required to break cycles.
    pub fn clear(&self) {
        self.values.lock().unwrap().clear();
    }
}

impl Object for Closure {
    fn get_value(self: &Arc<Self>, key: &Value) -> Option<Value> {
        self.values.lock().unwrap().get(key.as_str()?).cloned()
    }

    fn enumerate(self: &Arc<Self>) -> Enumerator {
        let values = self.values.lock().unwrap();
        let keys = values.keys().cloned().map(Value::from);
        Enumerator::Values(keys.collect())
    }
}

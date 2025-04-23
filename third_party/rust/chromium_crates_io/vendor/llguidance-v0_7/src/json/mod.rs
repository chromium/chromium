pub mod compiler;
mod formats;
mod numeric;
mod schema;
mod shared_context;

#[cfg(feature = "referencing")]
mod context_ref;
#[cfg(not(feature = "referencing"))]
mod context_simple;

pub mod context {
    #[cfg(feature = "referencing")]
    pub use super::context_ref::*;
    #[cfg(not(feature = "referencing"))]
    pub use super::context_simple::*;
}

use std::{any::type_name_of_val, sync::Arc};

use serde_json::Value;
pub fn json_merge(a: &mut Value, b: &Value) {
    match (a, b) {
        (Value::Object(a), Value::Object(b)) => {
            for (k, v) in b.iter() {
                json_merge(a.entry(k.clone()).or_insert(Value::Null), v);
            }
        }
        (a, b) => *a = b.clone(),
    }
}

pub trait Retrieve: Send + Sync {
    fn retrieve(&self, uri: &str) -> Result<Value, Box<dyn std::error::Error + Send + Sync>>;
}

#[derive(Clone)]
pub struct RetrieveWrapper(pub Arc<dyn Retrieve>);
impl RetrieveWrapper {
    pub fn new(retrieve: Arc<dyn Retrieve>) -> Self {
        RetrieveWrapper(retrieve)
    }
}

impl std::fmt::Debug for RetrieveWrapper {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", type_name_of_val(&self.0))
    }
}

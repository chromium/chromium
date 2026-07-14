use std::{collections::HashMap, sync::atomic::AtomicU64};

use crate::write::TableData;

static OBJECT_COUNTER: AtomicU64 = AtomicU64::new(0);

/// An identifier for an object in the compilation graph.
#[derive(Debug, Clone, Copy, PartialOrd, Ord, Hash, PartialEq, Eq)]
pub struct ObjectId(pub(crate) u64);

impl ObjectId {
    pub fn next() -> Self {
        ObjectId(OBJECT_COUNTER.fetch_add(1, std::sync::atomic::Ordering::Relaxed))
    }
}

#[derive(Debug, Default)]
pub(crate) struct ObjectStore {
    pub(crate) objects: HashMap<TableData, ObjectId>,
}

impl ObjectStore {
    pub(crate) fn add(&mut self, data: TableData) -> ObjectId {
        *self.objects.entry(data).or_insert_with(ObjectId::next)
    }
}

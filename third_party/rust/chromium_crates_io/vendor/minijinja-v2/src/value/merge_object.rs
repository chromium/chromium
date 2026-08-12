use std::collections::BTreeSet;
use std::fmt;
use std::sync::Arc;

use crate::value::ops::LenIterWrap;
use crate::value::{Enumerator, Object, ObjectExt, ObjectRepr, Value, ValueKind};

/// Dictionary merging behavior - create custom object with lookup capability
#[derive(Debug)]
pub struct MergeDict {
    values: Box<[Value]>,
}

impl MergeDict {
    pub fn new(values: Vec<Value>) -> Self {
        Self {
            values: values.into_boxed_slice(),
        }
    }
}

impl Object for MergeDict {
    fn get_value(self: &Arc<Self>, key: &Value) -> Option<Value> {
        // Look up key in reverse order (last matching dict wins)
        for value in self.values.iter().rev() {
            if let Ok(v) = value.get_item(key) {
                if !v.is_undefined() {
                    return Some(v);
                }
            }
        }
        None
    }

    fn enumerate(self: &Arc<Self>) -> Enumerator {
        // Collect all keys from all dictionaries (only include maps)
        let keys: BTreeSet<Value> = self
            .values
            .iter()
            .filter(|x| x.kind() == ValueKind::Map)
            .filter_map(|v| v.try_iter().ok())
            .flatten()
            .collect();
        Enumerator::Iter(Box::new(keys.into_iter()))
    }
}

/// List merging behavior - calculate total length for size hint
pub struct MergeSeq {
    values: Box<[Value]>,
    total_len: Option<usize>,
    repr: ObjectRepr,
    depth: usize,
}

impl fmt::Debug for MergeSeq {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("<iterator>").finish()
    }
}

impl MergeSeq {
    pub(crate) const MAX_DEPTH: usize = 32;

    pub fn new(values: Vec<Value>) -> Self {
        Self::with_repr(values, ObjectRepr::Seq)
    }

    pub(crate) fn new_iterable(values: Vec<Value>) -> Self {
        Self::with_repr(values, ObjectRepr::Iterable)
    }

    fn with_repr(mut values: Vec<Value>, repr: ObjectRepr) -> Self {
        let mut depth = Self::depth_for_values(&values);
        if depth > Self::MAX_DEPTH {
            let mut flattened = Vec::new();
            for value in values.iter() {
                Self::push_flattened_value(value, &mut flattened);
            }
            values = flattened;
            depth = Self::depth_for_values(&values);
        }

        Self {
            total_len: values.iter().map(|v| v.len()).sum(),
            values: values.into_boxed_slice(),
            repr,
            depth,
        }
    }

    pub(crate) fn depth_for_values(values: &[Value]) -> usize {
        values
            .iter()
            .filter_map(|value| value.downcast_object_ref::<Self>())
            .map(|seq| seq.depth)
            .max()
            .unwrap_or(0)
            .saturating_add(1)
    }

    fn push_flattened_value(value: &Value, values: &mut Vec<Value>) {
        let mut pending = vec![value.clone()];
        while let Some(value) = pending.pop() {
            if let Some(seq) = value.downcast_object_ref::<Self>() {
                pending.extend(seq.values.iter().rev().cloned());
            } else {
                values.push(value);
            }
        }
    }
}

impl Object for MergeSeq {
    fn repr(self: &Arc<Self>) -> ObjectRepr {
        self.repr
    }

    fn get_value(self: &Arc<Self>, key: &Value) -> Option<Value> {
        if self.repr != ObjectRepr::Seq {
            return None;
        }

        if let Some(idx) = key.as_usize() {
            let mut current_idx = 0;
            for value in self.values.iter() {
                let len = value.len().unwrap_or(0);
                if idx < current_idx + len {
                    return value.get_item(&Value::from(idx - current_idx)).ok();
                }
                current_idx += len;
            }
        }
        None
    }

    fn enumerate(self: &Arc<Self>) -> Enumerator {
        self.mapped_enumerator(|this| {
            let iter = this.values.iter().flat_map(|v| match v.try_iter() {
                Ok(iter) => Box::new(iter) as Box<dyn Iterator<Item = Value> + Send + Sync>,
                Err(err) => Box::new(Some(Value::from(err)).into_iter())
                    as Box<dyn Iterator<Item = Value> + Send + Sync>,
            });
            if let Some(total_len) = this.total_len {
                Box::new(LenIterWrap(total_len, iter))
            } else {
                Box::new(iter)
            }
        })
    }
}

/// Utility function to merge multiple maps into a single one.
///
/// If values are passed that are not maps, they are for the most part ignored.
/// They cannot be enumerated, but attribute lookups can still work.   That's
/// because [`get_value`](crate::value::Object::get_value) is forwarded through
/// to all objects.
///
/// This is the operation the [`context!`](crate::context) macro uses behind
/// the scenes.  The merge is done lazily which means that any dynamic object
/// that behaves like a map can be used here.  Note though that the order of
/// this function is inverse to what the macro does.
///
/// ```
/// use minijinja::{context, value::merge_maps};
///
/// let ctx1 = context!{
///     name => "John",
///     age => 30
/// };
///
/// let ctx2 = context!{
///     location => "New York",
///     age => 25  // This will be overridden by ctx1's value
/// };
///
/// let merged = merge_maps([ctx1, ctx2]);
/// ```
pub fn merge_maps<I, V>(iter: I) -> Value
where
    I: IntoIterator<Item = V>,
    V: Into<Value>,
{
    let sources: Vec<Value> = iter.into_iter().map(Into::into).collect();
    // if we only have a single source, we can use it directly to avoid making
    // an unnecessary indirection.
    if sources.len() == 1 {
        sources[0].clone()
    } else {
        Value::from_object(MergeDict::new(sources))
    }
}

#[test]
fn test_merge_object() {
    use std::collections::BTreeMap;

    let o = merge_maps([Value::from("abc"), Value::from(vec![1, 2, 3])]);
    assert_eq!(o, Value::from(BTreeMap::<String, String>::new()));

    let mut map1 = BTreeMap::new();
    map1.insert("a", 1);
    map1.insert("b", 2);

    let mut map2 = BTreeMap::new();
    map2.insert("b", 3);
    map2.insert("c", 4);

    let merged = merge_maps([Value::from(map1), Value::from(map2)]);

    // Check that the merged object contains all keys with expected values
    // The value from the latter map should be used when keys overlap
    assert_eq!(merged.get_attr("a").unwrap(), Value::from(1));
    assert_eq!(merged.get_attr("b").unwrap(), Value::from(3)); // Takes value from map2
    assert_eq!(merged.get_attr("c").unwrap(), Value::from(4));
}

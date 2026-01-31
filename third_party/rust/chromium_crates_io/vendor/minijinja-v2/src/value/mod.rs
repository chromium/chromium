//! Provides a dynamic value type abstraction.
//!
//! This module gives access to a dynamically typed value which is used by
//! the template engine during execution.
//!
//! For the most part the existence of the value type can be ignored as
//! MiniJinja will perform the necessary conversions for you.  For instance
//! if you write a filter that converts a string you can directly declare the
//! filter to take a [`String`].  However for some more advanced use cases it's
//! useful to know that this type exists.
//!
//! # Basic Value Conversions
//!
//! Values are typically created via the [`From`] trait:
//!
//! ```
//! use std::collections::BTreeMap;
//! # use minijinja::value::Value;
//! let int_value = Value::from(42);
//! let none_value = Value::from(());
//! let true_value = Value::from(true);
//! let map = Value::from({
//!     let mut m = BTreeMap::new();
//!     m.insert("foo", 1);
//!     m.insert("bar", 2);
//!     m
//! });
//! ```
//!
//! Or via the [`FromIterator`] trait which can create sequences or maps.  When
//! given a tuple it creates maps, otherwise it makes a sequence.
//!
//! ```
//! # use minijinja::value::Value;
//! // collection into a sequence
//! let value: Value = (1..10).into_iter().collect();
//!
//! // collection into a map
//! let value: Value = [("key", "value")].into_iter().collect();
//! ```
//!
//! For certain types of iterators (`Send` + `Sync` + `'static`) it's also
//! possible to make the value lazily iterate over the value by using the
//! [`Value::make_iterable`] function instead.  Whenever the value requires
//! iteration, the function is called to create that iterator.
//!
//! ```
//! # use minijinja::value::Value;
//! let value: Value = Value::make_iterable(|| 1..10);
//! ```
//!
//! To to into the inverse directly the various [`TryFrom`]
//! implementations can be used:
//!
//! ```
//! # use minijinja::value::Value;
//! use std::convert::TryFrom;
//! let v = u64::try_from(Value::from(42)).unwrap();
//! ```
//!
//! The special [`Undefined`](Value::UNDEFINED) value also exists but does not
//! have a rust equivalent.  It can be created via the [`UNDEFINED`](Value::UNDEFINED)
//! constant.
//!
//! # Collections
//!
//! The standard library's collection types such as
//! [`HashMap`](std::collections::HashMap), [`Vec`] and various others from the
//! collections module are implemented are objects.  There is a cavet here which is
//! that maps can only have string or [`Value`] as key.  The values in the collections
//! are lazily converted into value when accessed or iterated over.   These types can
//! be constructed either from [`Value::from`] or [`Value::from_object`].  Because the
//! types are boxed unchanged, you can also downcast them.
//!
//! ```rust
//! # use minijinja::Value;
//! let vec = Value::from(vec![1i32, 2, 3, 4]);
//! let vec_ref = vec.downcast_object_ref::<Vec<i32>>().unwrap();
//! assert_eq!(vec_ref, &vec![1, 2, 3, 4]);
//! ```
//!
//! **Caveat:** for convenience reasons maps with `&str` keys can be stored.  The keys
//! however are converted into `Arc<str>`.
//!
//! # Serde Conversions
//!
//! MiniJinja will usually however create values via an indirection via [`serde`] when
//! a template is rendered or an expression is evaluated.  This can also be
//! triggered manually by using the [`Value::from_serialize`] method:
//!
//! ```
//! # use minijinja::value::Value;
//! let value = Value::from_serialize(&[1, 2, 3]);
//! ```
//!
//! The inverse of that operation is to pass a value directly as serializer to
//! a type that supports deserialization.  This requires the `deserialization`
//! feature.
//!
#![cfg_attr(
    feature = "deserialization",
    doc = r"
```
# use minijinja::value::Value;
use serde::Deserialize;
let value = Value::from(vec![1, 2, 3]);
let vec = Vec::<i32>::deserialize(value).unwrap();
```
"
)]
//!
//! # Value Function Arguments
//!
//! [Filters](crate::filters) and [tests](crate::tests) can take values as arguments
//! but optionally also rust types directly.  This conversion for function arguments
//! is performed by the [`FunctionArgs`] and related traits ([`ArgType`], [`FunctionResult`]).
//!
//! # Memory Management
//!
//! Values are immutable objects which are internally reference counted which
//! means they can be copied relatively cheaply.  Special care must be taken
//! so that cycles are not created to avoid causing memory leaks.
//!
//! # HTML Escaping
//!
//! MiniJinja inherits the general desire to be clever about escaping.  For this
//! purpose a value will (when auto escaping is enabled) always be escaped.  To
//! prevent this behavior the [`safe`](crate::filters::safe) filter can be used
//! in the template.  Outside of templates the [`Value::from_safe_string`] method
//! can be used to achieve the same result.
//!
//! # Dynamic Objects
//!
//! Values can also hold "dynamic" objects.  These are objects which implement the
//! [`Object`] trait.  These can be used to implement dynamic functionality such
//! as stateful values and more.  Dynamic objects are internally also used to
//! implement the special `loop` variable, macros and similar things.
//!
//! To create a [`Value`] from a dynamic object use [`Value::from_object`],
//! [`Value::from_dyn_object`]:
//!
//! ```rust
//! # use std::sync::Arc;
//! # use minijinja::value::{Value, Object, DynObject};
//! #[derive(Debug)]
//! struct Foo;
//!
//! impl Object for Foo {
//!     /* implementation */
//! }
//!
//! let value = Value::from_object(Foo);
//! let value = Value::from_dyn_object(Arc::new(Foo));
//! ```
//!
//! # Invalid Values
//!
//! MiniJinja knows the concept of an "invalid value".  These are rare in practice
//! and should not be used, but they are needed in some situations.  An invalid value
//! looks like a value but working with that value in the context of the engine will
//! fail in most situations.  In principle an invalid value is a value that holds an
//! error internally.  It's created with [`From`]:
//!
//! ```
//! use minijinja::{Value, Error, ErrorKind};
//! let error = Error::new(ErrorKind::InvalidOperation, "failed to generate an item");
//! let invalid_value = Value::from(error);
//! ```
//!
//! Invalid values are typically encountered in the following situations:
//!
//! - serialization fails with an error: this is the case when a value is crated
//!   via [`Value::from_serialize`] and the underlying [`Serialize`] implementation
//!   fails with an error.
//! - fallible iteration: there might be situations where an iterator cannot indicate
//!   failure ahead of iteration and must abort.  In that case the only option an
//!   iterator in MiniJinja has is to create an invalid value.
//!
//! It's generally recommende to ignore the existence of invalid objects and let them
//! fail naturally as they are encountered.
//!
//! # Notes on Bytes and Strings
//!
//! Usually one would pass strings to templates as Jinja is entirely based on string
//! rendering.  However there are situations where it can be useful to pass bytes instead.
//! As such MiniJinja allows a value type to carry bytes even though there is no syntax
//! within the template language to create a byte literal.
//!
//! When rendering bytes as strings, MiniJinja will attempt to interpret them as
//! lossy utf-8.  This is a bit different to Jinja2 which in Python 3 stopped
//! rendering byte strings as strings.  This is an intentional change that was
//! deemed acceptable given how infrequently bytes are used but how relatively
//! commonly bytes are often holding "almost utf-8" in templates.  Most
//! conversions to strings also will do almost the same.  The debug rendering of
//! bytes however is different and bytes are not iterable.  Like strings however
//! they can be sliced and indexed, but they will be sliced by bytes and not by
//! characters.

// this module is based on the content module in insta which in turn is based
// on the content module in serde::private::ser.

use core::str;
use std::cell::{Cell, RefCell};
use std::cmp::Ordering;
use std::collections::BTreeMap;
use std::fmt;
use std::hash::{Hash, Hasher};
use std::sync::{Arc, Mutex};

use serde::ser::{Serialize, SerializeTupleStruct, Serializer};

use crate::error::{Error, ErrorKind};
use crate::functions;
use crate::utils::OnDrop;
use crate::value::ops::as_f64;
use crate::value::serialize::transform;
use crate::vm::State;

pub use crate::value::argtypes::{from_args, ArgType, FunctionArgs, FunctionResult, Kwargs, Rest};
pub use crate::value::merge_object::merge_maps;
pub use crate::value::object::{DynObject, Enumerator, Object, ObjectExt, ObjectRepr};

#[macro_use]
mod type_erase;
mod argtypes;
#[cfg(feature = "deserialization")]
mod deserialize;
pub(crate) mod merge_object;
pub(crate) mod namespace_object;
mod object;
pub(crate) mod ops;
mod serialize;

#[cfg(feature = "deserialization")]
pub use self::deserialize::ViaDeserialize;

// We use in-band signalling to roundtrip some internal values.  This is
// not ideal but unfortunately there is no better system in serde today.
const VALUE_HANDLE_MARKER: &str = "\x01__minijinja_ValueHandle";

#[cfg(feature = "preserve_order")]
pub(crate) type ValueMap = indexmap::IndexMap<Value, Value>;

#[cfg(not(feature = "preserve_order"))]
pub(crate) type ValueMap = std::collections::BTreeMap<Value, Value>;

#[inline(always)]
pub(crate) fn value_map_with_capacity(capacity: usize) -> ValueMap {
    #[cfg(not(feature = "preserve_order"))]
    {
        let _ = capacity;
        ValueMap::new()
    }
    #[cfg(feature = "preserve_order")]
    {
        ValueMap::with_capacity(crate::utils::untrusted_size_hint(capacity))
    }
}

thread_local! {
    static INTERNAL_SERIALIZATION: Cell<bool> = const { Cell::new(false) };

    // This should be an AtomicU64 but sadly 32bit targets do not necessarily have
    // AtomicU64 available.
    static LAST_VALUE_HANDLE: Cell<u32> = const { Cell::new(0) };
    static VALUE_HANDLES: RefCell<BTreeMap<u32, Value>> = const { RefCell::new(BTreeMap::new()) };
}

/// Function that returns true when serialization for [`Value`] is taking place.
///
/// MiniJinja internally creates [`Value`] objects from all values passed to the
/// engine.  It does this by going through the regular serde serialization trait.
/// In some cases users might want to customize the serialization specifically for
/// MiniJinja because they want to tune the object for the template engine
/// independently of what is normally serialized to disk.
///
/// This function returns `true` when MiniJinja is serializing to [`Value`] and
/// `false` otherwise.  You can call this within your own [`Serialize`]
/// implementation to change the output format.
///
/// This is particularly useful as serialization for MiniJinja does not need to
/// support deserialization.  So it becomes possible to completely change what
/// gets sent there, even at the cost of serializing something that cannot be
/// deserialized.
pub fn serializing_for_value() -> bool {
    INTERNAL_SERIALIZATION.with(|flag| flag.get())
}

fn mark_internal_serialization() -> impl Drop {
    let old = INTERNAL_SERIALIZATION.with(|flag| {
        let old = flag.get();
        flag.set(true);
        old
    });
    OnDrop::new(move || {
        if !old {
            INTERNAL_SERIALIZATION.with(|flag| flag.set(false));
        }
    })
}

/// Describes the kind of value.
#[derive(Copy, Clone, Debug, Eq, PartialEq, Ord, PartialOrd)]
#[non_exhaustive]
pub enum ValueKind {
    /// The value is undefined
    Undefined,
    /// The value is the none singleton (`()`)
    None,
    /// The value is a [`bool`]
    Bool,
    /// The value is a number of a supported type.
    Number,
    /// The value is a string.
    String,
    /// The value is a byte array.
    Bytes,
    /// The value is an array of other values.
    Seq,
    /// The value is a key/value mapping.
    Map,
    /// An iterable
    Iterable,
    /// A plain object without specific behavior.
    Plain,
    /// This value is invalid (holds an error).
    ///
    /// This can happen when a serialization error occurred or the engine
    /// encountered a failure in a place where an error can otherwise not
    /// be produced.  Interacting with such values in the context of the
    /// template evaluation process will attempt to propagate the error.
    Invalid,
}

impl fmt::Display for ValueKind {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match *self {
            ValueKind::Undefined => "undefined",
            ValueKind::None => "none",
            ValueKind::Bool => "bool",
            ValueKind::Number => "number",
            ValueKind::String => "string",
            ValueKind::Bytes => "bytes",
            ValueKind::Seq => "sequence",
            ValueKind::Map => "map",
            ValueKind::Iterable => "iterator",
            ValueKind::Plain => "plain object",
            ValueKind::Invalid => "invalid value",
        })
    }
}

/// Type type of string
#[derive(Copy, Clone, Debug)]
pub(crate) enum StringType {
    Normal,
    Safe,
}

/// Type type of undefined
#[derive(Copy, Clone, Debug)]
pub(crate) enum UndefinedType {
    Default,
    Silent,
}

/// Wraps an internal copyable value but marks it as packed.
///
/// This is used for `i128`/`u128` in the value repr to avoid
/// the excessive 16 byte alignment.
#[derive(Copy, Debug)]
#[cfg_attr(feature = "unstable_machinery_serde", derive(serde::Serialize))]
#[repr(packed)]
pub(crate) struct Packed<T: Copy>(pub T);

impl<T: Copy> Clone for Packed<T> {
    fn clone(&self) -> Self {
        *self
    }
}

/// Max size of a small str.
///
/// Logic: Value is 24 bytes. 1 byte is for the discriminant. One byte is
/// needed for the small str length.
const SMALL_STR_CAP: usize = 22;

/// Helper to store string data inline.
#[derive(Clone)]
pub(crate) struct SmallStr {
    len: u8,
    buf: [u8; SMALL_STR_CAP],
}

impl SmallStr {
    pub fn try_new(s: &str) -> Option<SmallStr> {
        let len = s.len();
        if len <= SMALL_STR_CAP {
            let mut buf = [0u8; SMALL_STR_CAP];
            buf[..len].copy_from_slice(s.as_bytes());
            Some(SmallStr {
                len: len as u8,
                buf,
            })
        } else {
            None
        }
    }

    pub fn as_str(&self) -> &str {
        // SAFETY: This is safe because we only place well-formed utf-8 strings
        unsafe { std::str::from_utf8_unchecked(&self.buf[..self.len as usize]) }
    }

    pub fn is_empty(&self) -> bool {
        self.len == 0
    }
}

#[derive(Clone)]
pub(crate) enum ValueRepr {
    None,
    Undefined(UndefinedType),
    Bool(bool),
    U64(u64),
    I64(i64),
    F64(f64),
    Invalid(Arc<Error>),
    U128(Packed<u128>),
    I128(Packed<i128>),
    String(Arc<str>, StringType),
    SmallStr(SmallStr),
    Bytes(Arc<Vec<u8>>),
    Object(DynObject),
}

impl fmt::Debug for ValueRepr {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match *self {
            ValueRepr::Undefined(_) => f.write_str("undefined"),
            ValueRepr::Bool(ref val) => fmt::Debug::fmt(val, f),
            ValueRepr::U64(ref val) => fmt::Debug::fmt(val, f),
            ValueRepr::I64(ref val) => fmt::Debug::fmt(val, f),
            ValueRepr::F64(ref val) => fmt::Debug::fmt(val, f),
            ValueRepr::None => f.write_str("none"),
            ValueRepr::Invalid(ref val) => write!(f, "<invalid value: {val}>"),
            ValueRepr::U128(val) => fmt::Debug::fmt(&{ val.0 }, f),
            ValueRepr::I128(val) => fmt::Debug::fmt(&{ val.0 }, f),
            ValueRepr::String(ref val, _) => fmt::Debug::fmt(val, f),
            ValueRepr::SmallStr(ref val) => fmt::Debug::fmt(val.as_str(), f),
            ValueRepr::Bytes(ref val) => {
                write!(f, "b'")?;
                for &b in val.iter() {
                    if b == b'"' {
                        write!(f, "\"")?
                    } else {
                        write!(f, "{}", b.escape_ascii())?;
                    }
                }
                write!(f, "'")
            }
            ValueRepr::Object(ref val) => val.render(f),
        }
    }
}

impl Hash for Value {
    fn hash<H: Hasher>(&self, state: &mut H) {
        match self.0 {
            ValueRepr::None | ValueRepr::Undefined(_) => 0u8.hash(state),
            ValueRepr::String(ref s, _) => s.hash(state),
            ValueRepr::SmallStr(ref s) => s.as_str().hash(state),
            ValueRepr::Bool(b) => b.hash(state),
            ValueRepr::Invalid(ref e) => (e.kind(), e.detail()).hash(state),
            ValueRepr::Bytes(ref b) => b.hash(state),
            ValueRepr::Object(ref d) => d.hash(state),
            ValueRepr::U64(_)
            | ValueRepr::I64(_)
            | ValueRepr::F64(_)
            | ValueRepr::U128(_)
            | ValueRepr::I128(_) => {
                if let Ok(val) = i64::try_from(self.clone()) {
                    val.hash(state)
                } else {
                    as_f64(self, true).map(|x| x.to_bits()).hash(state)
                }
            }
        }
    }
}

/// Represents a dynamically typed value in the template engine.
#[derive(Clone)]
pub struct Value(pub(crate) ValueRepr);

impl PartialEq for Value {
    fn eq(&self, other: &Self) -> bool {
        match (&self.0, &other.0) {
            (&ValueRepr::None, &ValueRepr::None) => true,
            (&ValueRepr::Undefined(_), &ValueRepr::Undefined(_)) => true,
            (&ValueRepr::String(ref a, _), &ValueRepr::String(ref b, _)) => a == b,
            (&ValueRepr::SmallStr(ref a), &ValueRepr::SmallStr(ref b)) => a.as_str() == b.as_str(),
            (&ValueRepr::Bytes(ref a), &ValueRepr::Bytes(ref b)) => a == b,
            _ => match ops::coerce(self, other, false) {
                Some(ops::CoerceResult::F64(a, b)) => a == b,
                Some(ops::CoerceResult::I128(a, b)) => a == b,
                Some(ops::CoerceResult::Str(a, b)) => a == b,
                None => {
                    if let (Some(a), Some(b)) = (self.as_object(), other.as_object()) {
                        if a.is_same_object(b) {
                            return true;
                        } else if a.is_same_object_type(b) {
                            if let Some(rv) = a.custom_cmp(b) {
                                return rv == Ordering::Equal;
                            }
                        }
                        match (a.repr(), b.repr()) {
                            (ObjectRepr::Map, ObjectRepr::Map) => {
                                // only if we have known lengths can we compare the enumerators
                                // ahead of time.  This function has a fallback for when a
                                // map has an unknown length.  That's generally a bad idea, but
                                // it makes sense supporting regardless as silent failures are
                                // not a lot of fun.
                                let mut need_length_fallback = true;
                                if let (Some(a_len), Some(b_len)) =
                                    (a.enumerator_len(), b.enumerator_len())
                                {
                                    if a_len != b_len {
                                        return false;
                                    }
                                    need_length_fallback = false;
                                }
                                let mut a_count = 0;
                                if !a.try_iter_pairs().is_some_and(|mut ak| {
                                    ak.all(|(k, v1)| {
                                        a_count += 1;
                                        b.get_value(&k) == Some(v1)
                                    })
                                }) {
                                    return false;
                                }
                                if !need_length_fallback {
                                    true
                                } else {
                                    a_count == b.try_iter().map_or(0, |x| x.count())
                                }
                            }
                            (
                                ObjectRepr::Seq | ObjectRepr::Iterable,
                                ObjectRepr::Seq | ObjectRepr::Iterable,
                            ) => {
                                if let (Some(ak), Some(bk)) = (a.try_iter(), b.try_iter()) {
                                    ak.eq(bk)
                                } else {
                                    false
                                }
                            }
                            // terrible fallback for plain objects
                            (ObjectRepr::Plain, ObjectRepr::Plain) => {
                                a.to_string() == b.to_string()
                            }
                            // should not happen
                            (_, _) => false,
                        }
                    } else {
                        false
                    }
                }
            },
        }
    }
}

impl Eq for Value {}

impl PartialOrd for Value {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

fn f64_total_cmp(left: f64, right: f64) -> Ordering {
    // this is taken from f64::total_cmp on newer rust versions
    let mut left = left.to_bits() as i64;
    let mut right = right.to_bits() as i64;
    left ^= (((left >> 63) as u64) >> 1) as i64;
    right ^= (((right >> 63) as u64) >> 1) as i64;
    left.cmp(&right)
}

impl Ord for Value {
    fn cmp(&self, other: &Self) -> Ordering {
        let kind_ordering = self.kind().cmp(&other.kind());
        if matches!(kind_ordering, Ordering::Less | Ordering::Greater) {
            return kind_ordering;
        }
        match (&self.0, &other.0) {
            (&ValueRepr::None, &ValueRepr::None) => Ordering::Equal,
            (&ValueRepr::Undefined(_), &ValueRepr::Undefined(_)) => Ordering::Equal,
            (&ValueRepr::String(ref a, _), &ValueRepr::String(ref b, _)) => a.cmp(b),
            (&ValueRepr::SmallStr(ref a), &ValueRepr::SmallStr(ref b)) => {
                a.as_str().cmp(b.as_str())
            }
            (&ValueRepr::Bytes(ref a), &ValueRepr::Bytes(ref b)) => a.cmp(b),
            _ => match ops::coerce(self, other, false) {
                Some(ops::CoerceResult::F64(a, b)) => f64_total_cmp(a, b),
                Some(ops::CoerceResult::I128(a, b)) => a.cmp(&b),
                Some(ops::CoerceResult::Str(a, b)) => a.cmp(b),
                None => {
                    let a = self.as_object().unwrap();
                    let b = other.as_object().unwrap();

                    if a.is_same_object(b) {
                        Ordering::Equal
                    } else {
                        // if there is a custom comparison, run it.
                        if a.is_same_object_type(b) {
                            if let Some(rv) = a.custom_cmp(b) {
                                return rv;
                            }
                        }
                        match (a.repr(), b.repr()) {
                            (ObjectRepr::Map, ObjectRepr::Map) => {
                                // This is not really correct.  Because the keys can be in arbitrary
                                // order this could just sort really weirdly as a result.  However
                                // we don't want to pay the cost of actually sorting the keys for
                                // ordering so we just accept this for now.
                                match (a.try_iter_pairs(), b.try_iter_pairs()) {
                                    (Some(a), Some(b)) => a.cmp(b),
                                    _ => unreachable!(),
                                }
                            }
                            (
                                ObjectRepr::Seq | ObjectRepr::Iterable,
                                ObjectRepr::Seq | ObjectRepr::Iterable,
                            ) => match (a.try_iter(), b.try_iter()) {
                                (Some(a), Some(b)) => a.cmp(b),
                                _ => unreachable!(),
                            },
                            // terrible fallback for plain objects
                            (ObjectRepr::Plain, ObjectRepr::Plain) => {
                                a.to_string().cmp(&b.to_string())
                            }
                            // should not happen
                            (_, _) => unreachable!(),
                        }
                    }
                }
            },
        }
    }
}

impl fmt::Debug for Value {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> Result<(), std::fmt::Error> {
        fmt::Debug::fmt(&self.0, f)
    }
}

impl fmt::Display for Value {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self.0 {
            ValueRepr::Undefined(_) => Ok(()),
            ValueRepr::Bool(val) => val.fmt(f),
            ValueRepr::U64(val) => val.fmt(f),
            ValueRepr::I64(val) => val.fmt(f),
            ValueRepr::F64(val) => {
                if val.is_nan() {
                    f.write_str("NaN")
                } else if val.is_infinite() {
                    write!(f, "{}inf", if val.is_sign_negative() { "-" } else { "" })
                } else {
                    let mut num = val.to_string();
                    if !num.contains('.') {
                        num.push_str(".0");
                    }
                    write!(f, "{num}")
                }
            }
            ValueRepr::None => f.write_str("none"),
            ValueRepr::Invalid(ref val) => write!(f, "<invalid value: {val}>"),
            ValueRepr::I128(val) => write!(f, "{}", { val.0 }),
            ValueRepr::String(ref val, _) => write!(f, "{val}"),
            ValueRepr::SmallStr(ref val) => write!(f, "{}", val.as_str()),
            ValueRepr::Bytes(ref val) => write!(f, "{}", String::from_utf8_lossy(val)),
            ValueRepr::U128(val) => write!(f, "{}", { val.0 }),
            ValueRepr::Object(ref x) => write!(f, "{x}"),
        }
    }
}

impl Default for Value {
    fn default() -> Value {
        ValueRepr::Undefined(UndefinedType::Default).into()
    }
}

#[doc(hidden)]
#[deprecated = "This function no longer has an effect.  Use Arc::from directly."]
pub fn intern(s: &str) -> Arc<str> {
    Arc::from(s.to_string())
}

#[allow(clippy::len_without_is_empty)]
impl Value {
    /// The undefined value.
    ///
    /// This constant exists because the undefined type does not exist in Rust
    /// and this is the only way to construct it.
    pub const UNDEFINED: Value = Value(ValueRepr::Undefined(UndefinedType::Default));

    /// Creates a value from something that can be serialized.
    ///
    /// This is the method that MiniJinja will generally use whenever a serializable
    /// object is passed to one of the APIs that internally want to create a value.
    /// For instance this is what [`context!`](crate::context) and
    /// [`render`](crate::Template::render) will use.
    ///
    /// During serialization of the value, [`serializing_for_value`] will return
    /// `true` which makes it possible to customize serialization for MiniJinja.
    /// For more information see [`serializing_for_value`].
    ///
    /// ```
    /// # use minijinja::value::Value;
    /// let val = Value::from_serialize(&vec![1, 2, 3]);
    /// ```
    ///
    /// This method does not fail but it might return a value that is not valid.  Such
    /// values will when operated on fail in the template engine in most situations.
    /// This for instance can happen if the underlying implementation of [`Serialize`]
    /// fails.  There are also cases where invalid objects are silently hidden in the
    /// engine today.  This is for instance the case for when keys are used in hash maps
    /// that the engine cannot deal with.  Invalid values are considered an implementation
    /// detail.  There is currently no API to validate a value.
    ///
    /// If the `deserialization` feature is enabled then the inverse of this method
    /// is to use the [`Value`] type as serializer.  You can pass a value into the
    /// [`deserialize`](serde::Deserialize::deserialize) method of a type that supports
    /// serde deserialization.
    pub fn from_serialize<T: Serialize>(value: T) -> Value {
        let _serialization_guard = mark_internal_serialization();
        transform(value)
    }

    /// Extracts a contained error.
    ///
    /// An invalid value carres an error internally and will reveal that error
    /// at a later point when interacted with.  This is used to carry
    /// serialization errors or failures that happen when the engine otherwise
    /// assumes an infallible operation such as iteration.
    pub(crate) fn validate(self) -> Result<Value, Error> {
        if let ValueRepr::Invalid(err) = self.0 {
            // Today the API implies tghat errors are `Clone`, but we don't want to expose
            // this as a functionality (yet?).
            Err(Arc::try_unwrap(err).unwrap_or_else(|arc| (*arc).internal_clone()))
        } else {
            Ok(self)
        }
    }

    /// Creates a value from a safe string.
    ///
    /// A safe string is one that will bypass auto escaping.  For instance if you
    /// want to have the template engine render some HTML without the user having to
    /// supply the `|safe` filter, you can use a value of this type instead.
    ///
    /// ```
    /// # use minijinja::value::Value;
    /// let val = Value::from_safe_string("<em>note</em>".into());
    /// ```
    pub fn from_safe_string(value: String) -> Value {
        ValueRepr::String(Arc::from(value), StringType::Safe).into()
    }

    /// Creates a value from a byte vector.
    ///
    /// MiniJinja can hold on to bytes and has some limited built-in support for
    /// working with them.  They are non iterable and not particularly useful
    /// in the context of templates.  When they are stringified, they are assumed
    /// to contain UTF-8 and will be treated as such.  They become more useful
    /// when a filter can do something with them (eg: base64 encode them etc.).
    ///
    /// This method exists so that a value can be constructed as creating a
    /// value from a `Vec<u8>` would normally just create a sequence.
    pub fn from_bytes(value: Vec<u8>) -> Value {
        ValueRepr::Bytes(value.into()).into()
    }

    /// Creates a value from a dynamic object.
    ///
    /// For more information see [`Object`].
    ///
    /// ```rust
    /// # use minijinja::value::{Value, Object};
    /// use std::fmt;
    ///
    /// #[derive(Debug)]
    /// struct Thing {
    ///     id: usize,
    /// }
    ///
    /// impl Object for Thing {}
    ///
    /// let val = Value::from_object(Thing { id: 42 });
    /// ```
    pub fn from_object<T: Object + Send + Sync + 'static>(value: T) -> Value {
        Value::from(ValueRepr::Object(DynObject::new(Arc::new(value))))
    }

    /// Like [`from_object`](Self::from_object) but for type erased dynamic objects.
    ///
    /// This especially useful if you have an object that has an `Arc<T>` to another
    /// child object that you want to return as a `Arc<T>` turns into a [`DynObject`]
    /// automatically.
    ///
    /// ```rust
    /// # use std::sync::Arc;
    /// # use minijinja::value::{Value, Object, Enumerator};
    /// #[derive(Debug)]
    /// pub struct HttpConfig {
    ///     port: usize,
    /// }
    ///
    /// #[derive(Debug)]
    /// struct Config {
    ///     http: Arc<HttpConfig>,
    /// }
    ///
    /// impl Object for HttpConfig {
    ///     fn enumerate(self: &Arc<Self>) -> Enumerator {
    ///         Enumerator::Str(&["port"])
    ///     }
    ///
    ///     fn get_value(self: &Arc<Self>, key: &Value) -> Option<Value> {
    ///         match key.as_str()? {
    ///             "port" => Some(Value::from(self.port)),
    ///             _ => None,
    ///         }
    ///     }
    /// }
    ///
    /// impl Object for Config {
    ///     fn enumerate(self: &Arc<Self>) -> Enumerator {
    ///         Enumerator::Str(&["http"])
    ///     }
    ///
    ///     fn get_value(self: &Arc<Self>, key: &Value) -> Option<Value> {
    ///         match key.as_str()? {
    ///             "http" => Some(Value::from_dyn_object(self.http.clone())),
    ///             _ => None
    ///         }
    ///     }
    /// }
    /// ```
    pub fn from_dyn_object<T: Into<DynObject>>(value: T) -> Value {
        Value::from(ValueRepr::Object(value.into()))
    }

    /// Creates a value that is an iterable.
    ///
    /// The function is invoked to create a new iterator every time the value is
    /// iterated over.
    ///
    /// ```
    /// # use minijinja::value::Value;
    /// let val = Value::make_iterable(|| 0..10);
    /// ```
    ///
    /// Iterators that implement [`ExactSizeIterator`] or have a matching lower and upper
    /// bound on the [`Iterator::size_hint`] report a known `loop.length`.  Iterators that
    /// do not fulfill these requirements will not.  The same is true for `revindex` and
    /// similar properties.
    pub fn make_iterable<I, T, F>(maker: F) -> Value
    where
        I: Iterator<Item = T> + Send + Sync + 'static,
        T: Into<Value> + Send + Sync + 'static,
        F: Fn() -> I + Send + Sync + 'static,
    {
        Value::make_object_iterable((), move |_| Box::new(maker().map(Into::into)))
    }

    /// Creates an iterable that iterates over the given value.
    ///
    /// This is similar to [`make_iterable`](Self::make_iterable) but it takes an extra
    /// reference to a value it can borrow out from.  It's a bit less generic in that it
    /// needs to return a boxed iterator of values directly.
    ///
    /// ```rust
    /// # use minijinja::value::Value;
    /// let val = Value::make_object_iterable(vec![1, 2, 3], |vec| {
    ///     Box::new(vec.iter().copied().map(Value::from))
    /// });
    /// assert_eq!(val.to_string(), "[1, 2, 3]");
    /// ````
    pub fn make_object_iterable<T, F>(object: T, maker: F) -> Value
    where
        T: Send + Sync + 'static,
        F: for<'a> Fn(&'a T) -> Box<dyn Iterator<Item = Value> + Send + Sync + 'a>
            + Send
            + Sync
            + 'static,
    {
        struct Iterable<T, F> {
            maker: F,
            object: T,
        }

        impl<T, F> fmt::Debug for Iterable<T, F> {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                f.debug_struct("<iterator>").finish()
            }
        }

        impl<T, F> Object for Iterable<T, F>
        where
            T: Send + Sync + 'static,
            F: for<'a> Fn(&'a T) -> Box<dyn Iterator<Item = Value> + Send + Sync + 'a>
                + Send
                + Sync
                + 'static,
        {
            fn repr(self: &Arc<Self>) -> ObjectRepr {
                ObjectRepr::Iterable
            }

            fn enumerate(self: &Arc<Self>) -> Enumerator {
                mapped_enumerator(self, |this| (this.maker)(&this.object))
            }
        }

        Value::from_object(Iterable { maker, object })
    }

    /// Creates an object projection onto a map.
    ///
    /// This is similar to [`make_object_iterable`](Self::make_object_iterable) but
    /// it creates a map rather than an iterable.  To accomplish this, it also
    /// requires two callbacks.  One for enumeration, and one for looking up
    /// attributes.
    ///
    /// # Example
    ///
    /// ```
    /// use std::collections::HashMap;
    /// use std::sync::Arc;
    /// use minijinja::value::{Value, Object, ObjectExt, Enumerator};
    ///
    /// #[derive(Debug)]
    /// struct Element {
    ///     tag: String,
    ///     attrs: HashMap<String, String>,
    /// }
    ///
    /// impl Object for Element {
    ///     fn get_value(self: &Arc<Self>, key: &Value) -> Option<Value> {
    ///         match key.as_str()? {
    ///             "tag" => Some(Value::from(&self.tag)),
    ///             "attrs" => Some(Value::make_object_map(
    ///                 self.clone(),
    ///                 |this| Box::new(this.attrs.keys().map(Value::from)),
    ///                 |this, key| this.attrs.get(key.as_str()?).map(Value::from),
    ///             )),
    ///             _ => None
    ///         }
    ///     }
    ///
    ///     fn enumerate(self: &Arc<Self>) -> Enumerator {
    ///         Enumerator::Str(&["tag", "attrs"])
    ///     }
    /// }
    /// ```
    pub fn make_object_map<T, E, A>(object: T, enumerate_fn: E, attr_fn: A) -> Value
    where
        T: Send + Sync + 'static,
        E: for<'a> Fn(&'a T) -> Box<dyn Iterator<Item = Value> + Send + Sync + 'a>
            + Send
            + Sync
            + 'static,
        A: Fn(&T, &Value) -> Option<Value> + Send + Sync + 'static,
    {
        struct ProxyMapObject<T, E, A> {
            enumerate_fn: E,
            attr_fn: A,
            object: T,
        }

        impl<T, E, A> fmt::Debug for ProxyMapObject<T, E, A> {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                f.debug_struct("<map-object>").finish()
            }
        }

        impl<T, E, A> Object for ProxyMapObject<T, E, A>
        where
            T: Send + Sync + 'static,
            E: for<'a> Fn(&'a T) -> Box<dyn Iterator<Item = Value> + Send + Sync + 'a>
                + Send
                + Sync
                + 'static,
            A: Fn(&T, &Value) -> Option<Value> + Send + Sync + 'static,
        {
            #[inline]
            fn get_value(self: &Arc<Self>, key: &Value) -> Option<Value> {
                (self.attr_fn)(&self.object, key)
            }

            #[inline]
            fn enumerate(self: &Arc<Self>) -> Enumerator {
                mapped_enumerator(self, |this| (this.enumerate_fn)(&this.object))
            }
        }

        Value::from_object(ProxyMapObject {
            enumerate_fn,
            attr_fn,
            object,
        })
    }

    /// Creates a value from a one-shot iterator.
    ///
    /// This takes an iterator (yielding values that can be turned into a [`Value`])
    /// and wraps it in a way that it turns into an iterable value.  From the view of
    /// the template this can be iterated over exactly once for the most part once
    /// exhausted.
    ///
    /// Such iterators are strongly recommended against in the general sense due to
    /// their surprising behavior, but they can be useful for more advanced use
    /// cases where data should be streamed into the template as it becomes available.
    ///
    /// Such iterators never have any size hints.
    ///
    /// ```
    /// # use minijinja::value::Value;
    /// let val = Value::make_one_shot_iterator(0..10);
    /// ```
    ///
    /// Attempting to iterate over it a second time will not yield any more items.
    pub fn make_one_shot_iterator<I, T>(iter: I) -> Value
    where
        I: Iterator<Item = T> + Send + Sync + 'static,
        T: Into<Value> + Send + Sync + 'static,
    {
        let iter = Arc::new(Mutex::new(iter.fuse()));
        Value::make_iterable(move || {
            let iter = iter.clone();
            std::iter::from_fn(move || iter.lock().unwrap().next())
        })
    }

    /// Creates a callable value from a function.
    ///
    /// ```
    /// # use minijinja::value::Value;
    /// let pow = Value::from_function(|a: u32| a * a);
    /// ```
    pub fn from_function<F, Rv, Args>(f: F) -> Value
    where
        F: functions::Function<Rv, Args>,
        Rv: FunctionResult,
        Args: for<'a> FunctionArgs<'a>,
    {
        functions::BoxedFunction::new(f).to_value()
    }

    /// Returns the kind of the value.
    ///
    /// This can be used to determine what's in the value before trying to
    /// perform operations on it.
    pub fn kind(&self) -> ValueKind {
        match self.0 {
            ValueRepr::Undefined(_) => ValueKind::Undefined,
            ValueRepr::Bool(_) => ValueKind::Bool,
            ValueRepr::U64(_) | ValueRepr::I64(_) | ValueRepr::F64(_) => ValueKind::Number,
            ValueRepr::None => ValueKind::None,
            ValueRepr::I128(_) => ValueKind::Number,
            ValueRepr::String(..) | ValueRepr::SmallStr(_) => ValueKind::String,
            ValueRepr::Bytes(_) => ValueKind::Bytes,
            ValueRepr::U128(_) => ValueKind::Number,
            ValueRepr::Invalid(_) => ValueKind::Invalid,
            ValueRepr::Object(ref obj) => match obj.repr() {
                ObjectRepr::Map => ValueKind::Map,
                ObjectRepr::Seq => ValueKind::Seq,
                ObjectRepr::Iterable => ValueKind::Iterable,
                ObjectRepr::Plain => ValueKind::Plain,
            },
        }
    }

    /// Returns `true` if the value is a number.
    ///
    /// To convert a value into a primitive number, use [`TryFrom`] or [`TryInto`].
    pub fn is_number(&self) -> bool {
        matches!(
            self.0,
            ValueRepr::U64(_)
                | ValueRepr::I64(_)
                | ValueRepr::F64(_)
                | ValueRepr::I128(_)
                | ValueRepr::U128(_)
        )
    }

    /// Returns true if the number is a real integer.
    ///
    /// This can be used to distinguish `42` from `42.0`.  For the most part
    /// the engine keeps these the same.
    pub fn is_integer(&self) -> bool {
        matches!(
            self.0,
            ValueRepr::U64(_) | ValueRepr::I64(_) | ValueRepr::I128(_) | ValueRepr::U128(_)
        )
    }

    /// Returns `true` if the map represents keyword arguments.
    pub fn is_kwargs(&self) -> bool {
        Kwargs::extract(self).is_some()
    }

    /// Is this value considered true?
    ///
    /// The engine inherits the same behavior as Jinja2 when it comes to
    /// considering objects true.  Empty objects are generally not considered
    /// true.  For custom objects this is customized by [`Object::is_true`].
    pub fn is_true(&self) -> bool {
        match self.0 {
            ValueRepr::Bool(val) => val,
            ValueRepr::U64(x) => x != 0,
            ValueRepr::U128(x) => x.0 != 0,
            ValueRepr::I64(x) => x != 0,
            ValueRepr::I128(x) => x.0 != 0,
            ValueRepr::F64(x) => x != 0.0,
            ValueRepr::String(ref x, _) => !x.is_empty(),
            ValueRepr::SmallStr(ref x) => !x.is_empty(),
            ValueRepr::Bytes(ref x) => !x.is_empty(),
            ValueRepr::None | ValueRepr::Undefined(_) | ValueRepr::Invalid(_) => false,
            ValueRepr::Object(ref x) => x.is_true(),
        }
    }

    /// Returns `true` if this value is safe.
    pub fn is_safe(&self) -> bool {
        matches!(&self.0, ValueRepr::String(_, StringType::Safe))
    }

    /// Returns `true` if this value is undefined.
    pub fn is_undefined(&self) -> bool {
        matches!(&self.0, ValueRepr::Undefined(_))
    }

    /// Returns `true` if this value is none.
    pub fn is_none(&self) -> bool {
        matches!(&self.0, ValueRepr::None)
    }

    /// If the value is a string, return it.
    ///
    /// This will also perform a lossy string conversion of bytes from utf-8.
    pub fn to_str(&self) -> Option<Arc<str>> {
        match self.0 {
            ValueRepr::String(ref s, _) => Some(s.clone()),
            ValueRepr::SmallStr(ref s) => Some(Arc::from(s.as_str())),
            ValueRepr::Bytes(ref b) => Some(Arc::from(String::from_utf8_lossy(b))),
            _ => None,
        }
    }

    /// If the value is a string, return it.
    ///
    /// This will also return well formed utf-8 bytes as string.
    pub fn as_str(&self) -> Option<&str> {
        match self.0 {
            ValueRepr::String(ref s, _) => Some(s as &str),
            ValueRepr::SmallStr(ref s) => Some(s.as_str()),
            ValueRepr::Bytes(ref b) => str::from_utf8(b).ok(),
            _ => None,
        }
    }

    /// If this is an usize return it
    #[inline]
    pub fn as_usize(&self) -> Option<usize> {
        // This is manually implemented as the engine calls as_usize a few times
        // during execution on hotter paths.  This way we can avoid an unnecessary clone.
        match self.0 {
            ValueRepr::I64(val) => TryFrom::try_from(val).ok(),
            ValueRepr::U64(val) => TryFrom::try_from(val).ok(),
            _ => self.clone().try_into().ok(),
        }
    }

    /// If this is an i64 return it
    pub fn as_i64(&self) -> Option<i64> {
        i64::try_from(self.clone()).ok()
    }

    /// Returns the bytes of this value if they exist.
    pub fn as_bytes(&self) -> Option<&[u8]> {
        match self.0 {
            ValueRepr::String(ref s, _) => Some(s.as_bytes()),
            ValueRepr::SmallStr(ref s) => Some(s.as_str().as_bytes()),
            ValueRepr::Bytes(ref b) => Some(&b[..]),
            _ => None,
        }
    }

    /// If the value is an object a reference to it is returned.
    ///
    /// The returned value is a reference to a type erased [`DynObject`].
    /// For a specific type use [`downcast_object`](Self::downcast_object)
    /// instead.
    pub fn as_object(&self) -> Option<&DynObject> {
        match self.0 {
            ValueRepr::Object(ref dy) => Some(dy),
            _ => None,
        }
    }

    /// Returns the length of the contained value.
    ///
    /// Values without a length will return `None`.
    ///
    /// ```
    /// # use minijinja::value::Value;
    /// let seq = Value::from(vec![1, 2, 3, 4]);
    /// assert_eq!(seq.len(), Some(4));
    /// ```
    pub fn len(&self) -> Option<usize> {
        match self.0 {
            ValueRepr::String(ref s, _) => Some(s.chars().count()),
            ValueRepr::SmallStr(ref s) => Some(s.as_str().chars().count()),
            ValueRepr::Bytes(ref b) => Some(b.len()),
            ValueRepr::Object(ref dy) => dy.enumerator_len(),
            _ => None,
        }
    }

    /// Looks up an attribute by attribute name.
    ///
    /// This this returns [`UNDEFINED`](Self::UNDEFINED) when an invalid key is
    /// resolved.  An error is returned if the value does not contain an object
    /// that has attributes.
    ///
    /// ```
    /// # use minijinja::value::Value;
    /// # fn test() -> Result<(), minijinja::Error> {
    /// let ctx = minijinja::context! {
    ///     foo => "Foo"
    /// };
    /// let value = ctx.get_attr("foo")?;
    /// assert_eq!(value.to_string(), "Foo");
    /// # Ok(()) }
    /// ```
    pub fn get_attr(&self, key: &str) -> Result<Value, Error> {
        let value = match self.0 {
            ValueRepr::Undefined(_) => return Err(Error::from(ErrorKind::UndefinedError)),
            ValueRepr::Object(ref dy) => dy.get_value(&Value::from(key)),
            _ => None,
        };

        Ok(value.unwrap_or(Value::UNDEFINED))
    }

    /// Alternative lookup strategy without error handling exclusively for context
    /// resolution.
    ///
    /// The main difference is that the return value will be `None` if the value is
    /// unable to look up the key rather than returning `Undefined` and errors will
    /// also not be created.
    pub(crate) fn get_attr_fast(&self, key: &str) -> Option<Value> {
        match self.0 {
            ValueRepr::Object(ref dy) => dy.get_value(&Value::from(key)),
            _ => None,
        }
    }

    /// Looks up an index of the value.
    ///
    /// This is a shortcut for [`get_item`](Self::get_item).
    ///
    /// ```
    /// # use minijinja::value::Value;
    /// let seq = Value::from(vec![0u32, 1, 2]);
    /// let value = seq.get_item_by_index(1).unwrap();
    /// assert_eq!(value.try_into().ok(), Some(1));
    /// ```
    pub fn get_item_by_index(&self, idx: usize) -> Result<Value, Error> {
        self.get_item(&Value(ValueRepr::U64(idx as _)))
    }

    /// Looks up an item (or attribute) by key.
    ///
    /// This is similar to [`get_attr`](Self::get_attr) but instead of using
    /// a string key this can be any key.  For instance this can be used to
    /// index into sequences.  Like [`get_attr`](Self::get_attr) this returns
    /// [`UNDEFINED`](Self::UNDEFINED) when an invalid key is looked up.
    ///
    /// ```
    /// # use minijinja::value::Value;
    /// let ctx = minijinja::context! {
    ///     foo => "Foo",
    /// };
    /// let value = ctx.get_item(&Value::from("foo")).unwrap();
    /// assert_eq!(value.to_string(), "Foo");
    /// ```
    pub fn get_item(&self, key: &Value) -> Result<Value, Error> {
        if let ValueRepr::Undefined(_) = self.0 {
            Err(Error::from(ErrorKind::UndefinedError))
        } else {
            Ok(self.get_item_opt(key).unwrap_or(Value::UNDEFINED))
        }
    }

    /// Iterates over the value.
    ///
    /// Depending on the [`kind`](Self::kind) of the value the iterator
    /// has a different behavior.
    ///
    /// * [`ValueKind::Map`]: the iterator yields the keys of the map.
    /// * [`ValueKind::Seq`] / [`ValueKind::Iterable`]: the iterator yields the items in the sequence.
    /// * [`ValueKind::String`]: the iterator yields characters in a string.
    /// * [`ValueKind::None`] / [`ValueKind::Undefined`]: the iterator is empty.
    ///
    /// ```
    /// # use minijinja::value::Value;
    /// # fn test() -> Result<(), minijinja::Error> {
    /// let value = Value::from({
    ///     let mut m = std::collections::BTreeMap::new();
    ///     m.insert("foo", 42);
    ///     m.insert("bar", 23);
    ///     m
    /// });
    /// for key in value.try_iter()? {
    ///     let value = value.get_item(&key)?;
    ///     println!("{} = {}", key, value);
    /// }
    /// # Ok(()) }
    /// ```
    pub fn try_iter(&self) -> Result<ValueIter, Error> {
        match self.0 {
            ValueRepr::None | ValueRepr::Undefined(_) => Some(ValueIterImpl::Empty),
            ValueRepr::String(ref s, _) => {
                Some(ValueIterImpl::Chars(0, s.chars().count(), Arc::clone(s)))
            }
            ValueRepr::SmallStr(ref s) => Some(ValueIterImpl::Chars(
                0,
                s.as_str().chars().count(),
                Arc::from(s.as_str()),
            )),
            ValueRepr::Object(ref obj) => obj.try_iter().map(ValueIterImpl::Dyn),
            _ => None,
        }
        .map(|imp| ValueIter { imp })
        .ok_or_else(|| {
            Error::new(
                ErrorKind::InvalidOperation,
                format!("{} is not iterable", self.kind()),
            )
        })
    }

    /// Returns a reversed view of this value.
    ///
    /// This is implemented for the following types with the following behaviors:
    ///
    /// * undefined or none: value returned unchanged.
    /// * string and bytes: returns a reversed version of that value
    /// * iterables: returns a reversed version of the iterable.  If the iterable is not
    ///   reversible itself, it consumes it and then reverses it.
    pub fn reverse(&self) -> Result<Value, Error> {
        match self.0 {
            ValueRepr::Undefined(_) | ValueRepr::None => Some(self.clone()),
            ValueRepr::String(ref s, _) => Some(Value::from(s.chars().rev().collect::<String>())),
            ValueRepr::SmallStr(ref s) => {
                // TODO: add small str optimization here
                Some(Value::from(s.as_str().chars().rev().collect::<String>()))
            }
            ValueRepr::Bytes(ref b) => Some(Value::from_bytes(
                b.iter().rev().copied().collect::<Vec<_>>(),
            )),
            ValueRepr::Object(ref o) => match o.enumerate() {
                Enumerator::NonEnumerable => None,
                Enumerator::Empty => Some(Value::make_iterable(|| None::<Value>.into_iter())),
                Enumerator::Seq(l) => {
                    let self_clone = o.clone();
                    Some(Value::make_iterable(move || {
                        let self_clone = self_clone.clone();
                        (0..l).rev().map(move |idx| {
                            self_clone.get_value(&Value::from(idx)).unwrap_or_default()
                        })
                    }))
                }
                Enumerator::Iter(iter) => {
                    let mut v = iter.collect::<Vec<_>>();
                    v.reverse();
                    Some(Value::make_object_iterable(v, move |v| {
                        Box::new(v.iter().cloned())
                    }))
                }
                Enumerator::RevIter(rev_iter) => {
                    let for_restart = self.clone();
                    let iter = Mutex::new(Some(rev_iter));
                    Some(Value::make_iterable(move || {
                        if let Some(iter) = iter.lock().unwrap().take() {
                            Box::new(iter) as Box<dyn Iterator<Item = Value> + Send + Sync>
                        } else {
                            match for_restart.reverse().and_then(|x| x.try_iter()) {
                                Ok(iterable) => Box::new(iterable)
                                    as Box<dyn Iterator<Item = Value> + Send + Sync>,
                                Err(err) => Box::new(Some(Value::from(err)).into_iter())
                                    as Box<dyn Iterator<Item = Value> + Send + Sync>,
                            }
                        }
                    }))
                }
                Enumerator::Str(s) => Some(Value::make_iterable(move || s.iter().rev().copied())),
                Enumerator::Values(mut v) => {
                    v.reverse();
                    Some(Value::make_object_iterable(v, move |v| {
                        Box::new(v.iter().cloned())
                    }))
                }
            },
            _ => None,
        }
        .ok_or_else(|| {
            Error::new(
                ErrorKind::InvalidOperation,
                format!("cannot reverse values of type {}", self.kind()),
            )
        })
    }

    /// Returns some reference to the boxed object if it is of type `T`, or None if it isn’t.
    ///
    /// This is basically the "reverse" of [`from_object`](Self::from_object)
    /// and [`from_dyn_object`](Self::from_dyn_object). It's also a shortcut for
    /// [`downcast_ref`](DynObject::downcast_ref) on the return value of
    /// [`as_object`](Self::as_object).
    ///
    /// # Example
    ///
    /// ```rust
    /// # use minijinja::value::{Value, Object};
    /// use std::fmt;
    ///
    /// #[derive(Debug)]
    /// struct Thing {
    ///     id: usize,
    /// }
    ///
    /// impl Object for Thing {}
    ///
    /// let x_value = Value::from_object(Thing { id: 42 });
    /// let thing = x_value.downcast_object_ref::<Thing>().unwrap();
    /// assert_eq!(thing.id, 42);
    /// ```
    pub fn downcast_object_ref<T: 'static>(&self) -> Option<&T> {
        match self.0 {
            ValueRepr::Object(ref o) => o.downcast_ref(),
            _ => None,
        }
    }

    /// Like [`downcast_object_ref`](Self::downcast_object_ref) but returns
    /// the actual object.
    pub fn downcast_object<T: 'static>(&self) -> Option<Arc<T>> {
        match self.0 {
            ValueRepr::Object(ref o) => o.downcast(),
            _ => None,
        }
    }

    pub(crate) fn get_item_opt(&self, key: &Value) -> Option<Value> {
        fn index(value: &Value, len: impl Fn() -> Option<usize>) -> Option<usize> {
            match value.as_i64().and_then(|v| isize::try_from(v).ok()) {
                Some(i) if i < 0 => some!(len()).checked_sub(i.unsigned_abs()),
                Some(i) => Some(i as usize),
                None => None,
            }
        }

        match self.0 {
            ValueRepr::Object(ref dy) => match dy.repr() {
                ObjectRepr::Map | ObjectRepr::Plain => dy.get_value(key),
                ObjectRepr::Iterable => {
                    if let Some(rv) = dy.get_value(key) {
                        return Some(rv);
                    }
                    // The default behavior is to try to index into the iterable
                    // as if nth() was called.  This lets one slice an array and
                    // then index into it.
                    if let Some(idx) = index(key, || dy.enumerator_len()) {
                        if let Some(mut iter) = dy.try_iter() {
                            if let Some(rv) = iter.nth(idx) {
                                return Some(rv);
                            }
                        }
                    }
                    None
                }
                ObjectRepr::Seq => {
                    let idx = index(key, || dy.enumerator_len()).map(Value::from);
                    dy.get_value(idx.as_ref().unwrap_or(key))
                }
            },
            ValueRepr::String(ref s, _) => {
                let idx = some!(index(key, || Some(s.chars().count())));
                s.chars().nth(idx).map(Value::from)
            }
            ValueRepr::SmallStr(ref s) => {
                let idx = some!(index(key, || Some(s.as_str().chars().count())));
                s.as_str().chars().nth(idx).map(Value::from)
            }
            ValueRepr::Bytes(ref b) => {
                let idx = some!(index(key, || Some(b.len())));
                b.get(idx).copied().map(Value::from)
            }
            _ => None,
        }
    }

    /// Calls the value directly.
    ///
    /// If the value holds a function or macro, this invokes it.  Note that in
    /// MiniJinja there is a separate namespace for methods on objects and callable
    /// items.  To call methods (which should be a rather rare occurrence) you
    /// have to use [`call_method`](Self::call_method).
    ///
    /// The `args` slice is for the arguments of the function call.  To pass
    /// keyword arguments use the [`Kwargs`] type.
    ///
    /// Usually the state is already available when it's useful to call this method,
    /// but when it's not available you can get a fresh template state straight
    /// from the [`Template`](crate::Template) via [`new_state`](crate::Template::new_state).
    ///
    /// ```
    /// # use minijinja::{Environment, value::{Value, Kwargs}};
    /// # let mut env = Environment::new();
    /// # env.add_template("foo", "").unwrap();
    /// # let tmpl = env.get_template("foo").unwrap();
    /// # let state = tmpl.new_state(); let state = &state;
    /// let func = Value::from_function(|v: i64, kwargs: Kwargs| {
    ///     v * kwargs.get::<i64>("mult").unwrap_or(1)
    /// });
    /// let rv = func.call(
    ///     state,
    ///     &[
    ///         Value::from(42),
    ///         Value::from(Kwargs::from_iter([("mult", Value::from(2))])),
    ///     ],
    /// ).unwrap();
    /// assert_eq!(rv, Value::from(84));
    /// ```
    ///
    /// With the [`args!`](crate::args) macro creating an argument slice is
    /// simplified:
    ///
    /// ```
    /// # use minijinja::{Environment, args, value::{Value, Kwargs}};
    /// # let mut env = Environment::new();
    /// # env.add_template("foo", "").unwrap();
    /// # let tmpl = env.get_template("foo").unwrap();
    /// # let state = tmpl.new_state(); let state = &state;
    /// let func = Value::from_function(|v: i64, kwargs: Kwargs| {
    ///     v * kwargs.get::<i64>("mult").unwrap_or(1)
    /// });
    /// let rv = func.call(state, args!(42, mult => 2)).unwrap();
    /// assert_eq!(rv, Value::from(84));
    /// ```
    pub fn call(&self, state: &State, args: &[Value]) -> Result<Value, Error> {
        if let ValueRepr::Object(ref dy) = self.0 {
            dy.call(state, args)
        } else {
            Err(Error::new(
                ErrorKind::InvalidOperation,
                format!("value of type {} is not callable", self.kind()),
            ))
        }
    }

    /// Calls a method on the value.
    ///
    /// The name of the method is `name`, the arguments passed are in the `args`
    /// slice.
    pub fn call_method(&self, state: &State, name: &str, args: &[Value]) -> Result<Value, Error> {
        match self._call_method(state, name, args) {
            Ok(rv) => Ok(rv),
            Err(mut err) => {
                if err.kind() == ErrorKind::UnknownMethod {
                    if let Some(ref callback) = state.env().unknown_method_callback {
                        match callback(state, self, name, args) {
                            Ok(result) => return Ok(result),
                            Err(callback_err) => {
                                // if the callback fails with the same error, we
                                // want to also attach the default detail if
                                // it's missing
                                if callback_err.kind() == ErrorKind::UnknownMethod {
                                    err = callback_err;
                                } else {
                                    return Err(callback_err);
                                }
                            }
                        }
                    }
                    if err.detail().is_none() {
                        err.set_detail(format!("{} has no method named {}", self.kind(), name));
                    }
                }
                Err(err)
            }
        }
    }

    fn _call_method(&self, state: &State, name: &str, args: &[Value]) -> Result<Value, Error> {
        if let Some(object) = self.as_object() {
            object.call_method(state, name, args)
        } else {
            Err(Error::from(ErrorKind::UnknownMethod))
        }
    }

    #[cfg(feature = "builtins")]
    pub(crate) fn get_path(&self, path: &str) -> Result<Value, Error> {
        let mut rv = self.clone();
        for part in path.split('.') {
            if let Ok(num) = part.parse::<usize>() {
                rv = ok!(rv.get_item_by_index(num));
            } else {
                rv = ok!(rv.get_attr(part));
            }
        }
        Ok(rv)
    }

    #[cfg(feature = "builtins")]
    pub(crate) fn get_path_or_default(&self, path: &str, default: &Value) -> Value {
        match self.get_path(path) {
            Err(_) => default.clone(),
            Ok(val) if val.is_undefined() => default.clone(),
            Ok(val) => val,
        }
    }
}

impl Serialize for Value {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        // enable round tripping of values
        if serializing_for_value() {
            let handle = LAST_VALUE_HANDLE.with(|x| {
                // we are okay with overflowing the handle here because these values only
                // live for a very short period of time and it's not likely that you run out
                // of an entire u32 worth of handles in a single serialization operation.
                let rv = x.get().wrapping_add(1);
                x.set(rv);
                rv
            });
            VALUE_HANDLES.with(|handles| handles.borrow_mut().insert(handle, self.clone()));

            // we serialize this into a tuple struct as a form of in-band signalling
            // we can detect.  This also will fail with a somewhat acceptable error
            // for flattening operations.  See https://github.com/mitsuhiko/minijinja/issues/222
            let mut s = ok!(serializer.serialize_tuple_struct(VALUE_HANDLE_MARKER, 1));
            ok!(s.serialize_field(&handle));
            return s.end();
        }

        match self.0 {
            ValueRepr::Bool(b) => serializer.serialize_bool(b),
            ValueRepr::U64(u) => serializer.serialize_u64(u),
            ValueRepr::I64(i) => serializer.serialize_i64(i),
            ValueRepr::F64(f) => serializer.serialize_f64(f),
            ValueRepr::None | ValueRepr::Undefined(_) | ValueRepr::Invalid(_) => {
                serializer.serialize_unit()
            }
            ValueRepr::U128(u) => serializer.serialize_u128(u.0),
            ValueRepr::I128(i) => serializer.serialize_i128(i.0),
            ValueRepr::String(ref s, _) => serializer.serialize_str(s),
            ValueRepr::SmallStr(ref s) => serializer.serialize_str(s.as_str()),
            ValueRepr::Bytes(ref b) => serializer.serialize_bytes(b),
            ValueRepr::Object(ref o) => match o.repr() {
                ObjectRepr::Plain => serializer.serialize_str(&o.to_string()),
                ObjectRepr::Seq | ObjectRepr::Iterable => {
                    use serde::ser::SerializeSeq;
                    let mut seq = ok!(serializer.serialize_seq(o.enumerator_len()));
                    if let Some(iter) = o.try_iter() {
                        for item in iter {
                            ok!(seq.serialize_element(&item));
                        }
                    }

                    seq.end()
                }
                ObjectRepr::Map => {
                    use serde::ser::SerializeMap;
                    let mut map = ok!(serializer.serialize_map(None));
                    if let Some(iter) = o.try_iter_pairs() {
                        for (key, value) in iter {
                            ok!(map.serialize_entry(&key, &value));
                        }
                    }

                    map.end()
                }
            },
        }
    }
}

/// Helper to create an iterator proxy that borrows from an object.
pub(crate) fn mapped_enumerator<F, T>(obj: &Arc<T>, maker: F) -> Enumerator
where
    T: Object + 'static,
    F: for<'a> FnOnce(&'a T) -> Box<dyn Iterator<Item = Value> + Send + Sync + 'a>,
{
    struct Iter {
        iter: Box<dyn Iterator<Item = Value> + Send + Sync + 'static>,
        _object: DynObject,
    }

    impl Iterator for Iter {
        type Item = Value;

        fn next(&mut self) -> Option<Self::Item> {
            self.iter.next()
        }

        fn size_hint(&self) -> (usize, Option<usize>) {
            self.iter.size_hint()
        }
    }

    // SAFETY: this is safe because the object is kept alive by the iter
    let iter = unsafe {
        std::mem::transmute::<Box<dyn Iterator<Item = _>>, Box<dyn Iterator<Item = _> + Send + Sync>>(
            maker(obj),
        )
    };
    let _object = DynObject::new(obj.clone());
    Enumerator::Iter(Box::new(Iter { iter, _object }))
}

/// Utility to iterate over values.
pub struct ValueIter {
    imp: ValueIterImpl,
}

impl Iterator for ValueIter {
    type Item = Value;

    fn next(&mut self) -> Option<Self::Item> {
        match self.imp {
            ValueIterImpl::Empty => None,
            ValueIterImpl::Chars(ref mut offset, ref mut len, ref s) => {
                (s as &str)[*offset..].chars().next().map(|c| {
                    *offset += c.len_utf8();
                    *len -= 1;
                    Value::from(c)
                })
            }
            ValueIterImpl::Dyn(ref mut iter) => iter.next(),
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        match self.imp {
            ValueIterImpl::Empty => (0, Some(0)),
            ValueIterImpl::Chars(_, len, _) => (0, Some(len)),
            ValueIterImpl::Dyn(ref iter) => iter.size_hint(),
        }
    }
}

impl fmt::Debug for ValueIter {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("ValueIterator").finish()
    }
}

enum ValueIterImpl {
    Empty,
    Chars(usize, usize, Arc<str>),
    Dyn(Box<dyn Iterator<Item = Value> + Send + Sync>),
}

impl From<Error> for Value {
    fn from(value: Error) -> Self {
        Value(ValueRepr::Invalid(Arc::new(value)))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use similar_asserts::assert_eq;

    #[test]
    fn test_dynamic_object_roundtrip() {
        use std::sync::atomic::{self, AtomicUsize};

        #[derive(Debug, Clone)]
        struct X(Arc<AtomicUsize>);

        impl Object for X {
            fn get_value(self: &Arc<Self>, key: &Value) -> Option<Value> {
                match key.as_str()? {
                    "value" => Some(Value::from(self.0.load(atomic::Ordering::Relaxed))),
                    _ => None,
                }
            }

            fn enumerate(self: &Arc<Self>) -> Enumerator {
                Enumerator::Str(&["value"])
            }

            fn render(self: &Arc<Self>, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                write!(f, "{}", self.0.load(atomic::Ordering::Relaxed))
            }
        }

        let x = Arc::new(X(Default::default()));
        let x_value = Value::from_dyn_object(x.clone());
        x.0.fetch_add(42, atomic::Ordering::Relaxed);
        let x_clone = Value::from_serialize(&x_value);
        x.0.fetch_add(23, atomic::Ordering::Relaxed);

        assert_eq!(x_value.to_string(), "65");
        assert_eq!(x_clone.to_string(), "65");
    }

    #[test]
    fn test_string_char() {
        let val = Value::from('a');
        assert_eq!(char::try_from(val).unwrap(), 'a');
        let val = Value::from("a");
        assert_eq!(char::try_from(val).unwrap(), 'a');
        let val = Value::from("wat");
        assert!(char::try_from(val).is_err());
    }

    #[test]
    #[cfg(target_pointer_width = "64")]
    fn test_sizes() {
        assert_eq!(std::mem::size_of::<Value>(), 24);
    }
}

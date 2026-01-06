use std::any::Any;
use std::borrow::Cow;
use std::cmp::Ordering;
use std::collections::BTreeMap;
use std::fmt;
use std::hash::Hash;
use std::sync::Arc;

use crate::error::{Error, ErrorKind};
use crate::value::{mapped_enumerator, Value};
use crate::vm::State;

/// A trait that represents a dynamic object.
///
/// There is a type erased wrapper of this trait available called
/// [`DynObject`] which is what the engine actually holds internally.
///
/// # Basic Struct
///
/// The following example shows how to implement a dynamic object which
/// represents a struct.  All that's needed is to implement
/// [`get_value`](Self::get_value) to look up a field by name as well as
/// [`enumerate`](Self::enumerate) to return an enumerator over the known keys.
/// The [`repr`](Self::repr) defaults to `Map` so nothing needs to be done here.
///
/// ```
/// use std::sync::Arc;
/// use minijinja::value::{Value, Object, Enumerator};
///
/// #[derive(Debug)]
/// struct Point(f32, f32, f32);
///
/// impl Object for Point {
///     fn get_value(self: &Arc<Self>, key: &Value) -> Option<Value> {
///         match key.as_str()? {
///             "x" => Some(Value::from(self.0)),
///             "y" => Some(Value::from(self.1)),
///             "z" => Some(Value::from(self.2)),
///             _ => None,
///         }
///     }
///
///     fn enumerate(self: &Arc<Self>) -> Enumerator {
///         Enumerator::Str(&["x", "y", "z"])
///     }
/// }
///
/// let value = Value::from_object(Point(1.0, 2.5, 3.0));
/// ```
///
/// # Basic Sequence
///
/// The following example shows how to implement a dynamic object which
/// represents a sequence.  All that's needed is to implement
/// [`repr`](Self::repr) to indicate that this is a sequence,
/// [`get_value`](Self::get_value) to look up a field by index, and
/// [`enumerate`](Self::enumerate) to return a sequential enumerator.
/// This enumerator will automatically call `get_value` from `0..length`.
///
/// ```
/// use std::sync::Arc;
/// use minijinja::value::{Value, Object, ObjectRepr, Enumerator};
///
/// #[derive(Debug)]
/// struct Point(f32, f32, f32);
///
/// impl Object for Point {
///     fn repr(self: &Arc<Self>) -> ObjectRepr {
///         ObjectRepr::Seq
///     }
///
///     fn get_value(self: &Arc<Self>, key: &Value) -> Option<Value> {
///         match key.as_usize()? {
///             0 => Some(Value::from(self.0)),
///             1 => Some(Value::from(self.1)),
///             2 => Some(Value::from(self.2)),
///             _ => None,
///         }
///     }
///
///     fn enumerate(self: &Arc<Self>) -> Enumerator {
///         Enumerator::Seq(3)
///     }
/// }
///
/// let value = Value::from_object(Point(1.0, 2.5, 3.0));
/// ```
///
/// # Iterables
///
/// If you have something that is not quite a sequence but is capable of yielding
/// values over time, you can directly implement an iterable.  This is somewhat
/// uncommon as you can normally directly use [`Value::make_iterable`].  Here
/// is how this can be done though:
///
/// ```
/// use std::sync::Arc;
/// use minijinja::value::{Value, Object, ObjectRepr, Enumerator};
///
/// #[derive(Debug)]
/// struct Range10;
///
/// impl Object for Range10 {
///     fn repr(self: &Arc<Self>) -> ObjectRepr {
///         ObjectRepr::Iterable
///     }
///
///     fn enumerate(self: &Arc<Self>) -> Enumerator {
///         Enumerator::Iter(Box::new((1..10).map(Value::from)))
///     }
/// }
///
/// let value = Value::from_object(Range10);
/// ```
///
/// Iteration is encouraged to fail immediately (object is not iterable) or not at
/// all.  However this is not always possible, but the iteration interface itself
/// does not support fallible iteration.  It is however possible to accomplish the
/// same thing by creating an [invalid value](index.html#invalid-values).
///
/// # Map As Context
///
/// Map can also be used as template rendering context.  This has a lot of
/// benefits as it means that the serialization overhead can be largely to
/// completely avoided.  This means that even if templates take hundreds of
/// values, MiniJinja does not spend time eagerly converting them into values.
///
/// Here is a very basic example of how a template can be rendered with a dynamic
/// context.  Note that the implementation of [`enumerate`](Self::enumerate)
/// is optional for this to work.  It's in fact not used by the engine during
/// rendering but it is necessary for the [`debug()`](crate::functions::debug)
/// function to be able to show which values exist in the context.
///
/// ```
/// # fn main() -> Result<(), minijinja::Error> {
/// # use minijinja::Environment;
/// use std::sync::Arc;
/// use minijinja::value::{Value, Object};
///
/// #[derive(Debug)]
/// pub struct DynamicContext {
///     magic: i32,
/// }
///
/// impl Object for DynamicContext {
///     fn get_value(self: &Arc<Self>, field: &Value) -> Option<Value> {
///         match field.as_str()? {
///             #[cfg(not(target_os = "wasi"))]
///             "pid" => Some(Value::from(std::process::id())),
///             #[cfg(target_os = "wasi")]
///             "pid" => Some(Value::from(1234_u32)), // Mock PID for WASI
///             #[cfg(not(target_os = "wasi"))]
///             "env" => Some(Value::from_iter(std::env::vars())),
///             #[cfg(target_os = "wasi")]
///             "env" => Some(Value::from_iter([("HOME".to_string(), "/home/user".to_string())])), // Mock env for WASI
///             "magic" => Some(Value::from(self.magic)),
///             _ => None,
///         }
///     }
/// }
///
/// # let env = Environment::new();
/// let tmpl = env.template_from_str("HOME={{ env.HOME }}; PID={{ pid }}; MAGIC={{ magic }}")?;
/// let ctx = Value::from_object(DynamicContext { magic: 42 });
/// let rv = tmpl.render(ctx)?;
/// # Ok(()) }
/// ```
///
/// One thing of note here is that in the above example `env` would be re-created every
/// time the template needs it.  A better implementation would cache the value after it
/// was created first.
pub trait Object: fmt::Debug + Send + Sync {
    /// Indicates the natural representation of an object.
    ///
    /// The default implementation returns [`ObjectRepr::Map`].
    fn repr(self: &Arc<Self>) -> ObjectRepr {
        ObjectRepr::Map
    }

    /// Given a key, looks up the associated value.
    fn get_value(self: &Arc<Self>, key: &Value) -> Option<Value> {
        let _ = key;
        None
    }

    /// Enumerates the object.
    ///
    /// The engine uses the returned enumerator to implement iteration and
    /// the size information of an object.  For more information see
    /// [`Enumerator`].  The default implementation returns `Empty` for
    /// all object representations other than [`ObjectRepr::Plain`] which
    /// default to `NonEnumerable`.
    ///
    /// When wrapping other objects you might want to consider using
    /// [`ObjectExt::mapped_enumerator`] and [`ObjectExt::mapped_rev_enumerator`].
    fn enumerate(self: &Arc<Self>) -> Enumerator {
        match self.repr() {
            ObjectRepr::Plain => Enumerator::NonEnumerable,
            ObjectRepr::Iterable | ObjectRepr::Map | ObjectRepr::Seq => Enumerator::Empty,
        }
    }

    /// Returns the length of the enumerator.
    ///
    /// By default the length is taken by calling [`enumerate`](Self::enumerate) and
    /// inspecting the [`Enumerator`].  This means that in order to determine
    /// the length, an iteration is started.  If you think this is a problem for your
    /// uses, you can manually implement this.  This might for instance be
    /// needed if your type can only be iterated over once.
    fn enumerator_len(self: &Arc<Self>) -> Option<usize> {
        self.enumerate().query_len()
    }

    /// Returns `true` if this object is considered true for if conditions.
    ///
    /// The default implementation checks if the [`enumerator_len`](Self::enumerator_len)
    /// is not `Some(0)` which is the recommended behavior for objects.
    fn is_true(self: &Arc<Self>) -> bool {
        self.enumerator_len() != Some(0)
    }

    /// The engine calls this to invoke the object itself.
    ///
    /// The default implementation returns an
    /// [`InvalidOperation`](crate::ErrorKind::InvalidOperation) error.
    fn call(self: &Arc<Self>, state: &State<'_, '_>, args: &[Value]) -> Result<Value, Error> {
        let (_, _) = (state, args);
        Err(Error::new(
            ErrorKind::InvalidOperation,
            "object is not callable",
        ))
    }

    /// The engine calls this to invoke a method on the object.
    ///
    /// The default implementation returns an
    /// [`UnknownMethod`](crate::ErrorKind::UnknownMethod) error.  When this error
    /// is returned the engine will invoke the
    /// [`unknown_method_callback`](crate::Environment::set_unknown_method_callback) of
    /// the environment.
    fn call_method(
        self: &Arc<Self>,
        state: &State<'_, '_>,
        method: &str,
        args: &[Value],
    ) -> Result<Value, Error> {
        if let Some(value) = self.get_value(&Value::from(method)) {
            return value.call(state, args);
        }

        Err(Error::from(ErrorKind::UnknownMethod))
    }

    /// Custom comparison of this object against another object of the same type.
    ///
    /// This must return either `None` or `Some(Ordering)`.  When implemented this
    /// must guarantee a total ordering as otherwise sort functions will crash.
    /// This will only compare against other objects of the same type, not
    /// anything else.  Objects of different types are given an absolute
    /// ordering outside the scope of this method.
    ///
    /// The requirement is that an implementer downcasts the other [`DynObject`]
    /// to itself, and it that cannot be accomplished `None` must be returned.
    ///
    /// ```rust
    /// # use std::sync::Arc;
    /// # use std::cmp::Ordering;
    /// # use minijinja::value::{DynObject, Object};
    /// # #[derive(Debug)]
    /// # struct Thing { num: u32 };
    /// impl Object for Thing {
    ///     fn custom_cmp(self: &Arc<Self>, other: &DynObject) -> Option<Ordering> {
    ///         let other = other.downcast_ref::<Self>()?;
    ///         Some(self.num.cmp(&other.num))
    ///     }
    /// }
    /// ```
    fn custom_cmp(self: &Arc<Self>, other: &DynObject) -> Option<Ordering> {
        let _ = other;
        None
    }

    /// Formats the object for stringification.
    ///
    /// The default implementation is specific to the behavior of
    /// [`repr`](Self::repr) and usually does not need modification.
    fn render(self: &Arc<Self>, f: &mut fmt::Formatter<'_>) -> fmt::Result
    where
        Self: Sized + 'static,
    {
        match self.repr() {
            ObjectRepr::Map => {
                let mut dbg = f.debug_map();
                for (key, value) in self.try_iter_pairs().into_iter().flatten() {
                    dbg.entry(&key, &value);
                }
                dbg.finish()
            }
            // for either sequences or iterables, a length is needed, otherwise we
            // don't want to risk iteration during printing and fall back to the
            // debug print.
            ObjectRepr::Seq | ObjectRepr::Iterable if self.enumerator_len().is_some() => {
                let mut dbg = f.debug_list();
                for value in self.try_iter().into_iter().flatten() {
                    dbg.entry(&value);
                }
                dbg.finish()
            }
            _ => {
                write!(f, "{self:?}")
            }
        }
    }
}

macro_rules! impl_object_helpers {
    ($vis:vis $self_ty: ty) => {
        /// Iterates over this object.
        ///
        /// If this returns `None` then the default object iteration as defined by
        /// the object's `enumeration` is used.
        $vis fn try_iter(self: $self_ty) -> Option<Box<dyn Iterator<Item = Value> + Send + Sync>>
        where
            Self: 'static,
        {
            match self.enumerate() {
                Enumerator::NonEnumerable => None,
                Enumerator::Empty => Some(Box::new(None::<Value>.into_iter())),
                Enumerator::Seq(l) => {
                    let self_clone = self.clone();
                    Some(Box::new((0..l).map(move |idx| {
                        self_clone.get_value(&Value::from(idx)).unwrap_or_default()
                    })))
                }
                Enumerator::Iter(iter) => Some(iter),
                Enumerator::RevIter(iter) => Some(Box::new(iter)),
                Enumerator::Str(s) => Some(Box::new(s.iter().copied().map(Value::from))),
                Enumerator::Values(v) => Some(Box::new(v.into_iter())),
            }
        }

        /// Iterate over key and value at once.
        $vis fn try_iter_pairs(
            self: $self_ty,
        ) -> Option<Box<dyn Iterator<Item = (Value, Value)> + Send + Sync>> {
            let iter = some!(self.try_iter());
            let repr = self.repr();
            let self_clone = self.clone();
            Some(Box::new(iter.enumerate().map(move |(idx, item)| {
                match repr {
                    ObjectRepr::Map => {
                        let value = self_clone.get_value(&item);
                        (item, value.unwrap_or_default())
                    }
                    _ => (Value::from(idx), item)
                }
            })))
        }
    };
}

/// Provides utility methods for working with objects.
pub trait ObjectExt: Object + Send + Sync + 'static {
    /// Creates a new iterator enumeration that projects into the given object.
    ///
    /// It takes a method that is passed a reference to `self` and is expected
    /// to return an [`Iterator`].  This iterator is then wrapped in an
    /// [`Enumerator::Iter`].  This allows one to create an iterator that borrows
    /// out of the object.
    ///
    /// # Example
    ///
    /// ```
    /// # use std::collections::HashMap;
    /// use std::sync::Arc;
    /// use minijinja::value::{Value, Object, ObjectExt, Enumerator};
    ///
    /// #[derive(Debug)]
    /// struct CustomMap(HashMap<usize, i64>);
    ///
    /// impl Object for CustomMap {
    ///     fn get_value(self: &Arc<Self>, key: &Value) -> Option<Value> {
    ///         self.0.get(&key.as_usize()?).copied().map(Value::from)
    ///     }
    ///
    ///     fn enumerate(self: &Arc<Self>) -> Enumerator {
    ///         self.mapped_enumerator(|this| {
    ///             Box::new(this.0.keys().copied().map(Value::from))
    ///         })
    ///     }
    /// }
    /// ```
    fn mapped_enumerator<F>(self: &Arc<Self>, maker: F) -> Enumerator
    where
        F: for<'a> FnOnce(&'a Self) -> Box<dyn Iterator<Item = Value> + Send + Sync + 'a>
            + Send
            + Sync
            + 'static,
        Self: Sized,
    {
        mapped_enumerator(self, maker)
    }

    /// Creates a new reversible iterator enumeration that projects into the given object.
    ///
    /// It takes a method that is passed a reference to `self` and is expected
    /// to return a [`DoubleEndedIterator`].  This iterator is then wrapped in an
    /// [`Enumerator::RevIter`].  This allows one to create an iterator that borrows
    /// out of the object and is reversible.
    ///
    /// # Example
    ///
    /// ```
    /// # use std::collections::HashMap;
    /// use std::sync::Arc;
    /// use std::ops::Range;
    /// use minijinja::value::{Value, Object, ObjectExt, ObjectRepr, Enumerator};
    ///
    /// #[derive(Debug)]
    /// struct VecView(Vec<usize>);
    ///
    /// impl Object for VecView {
    ///     fn repr(self: &Arc<Self>) -> ObjectRepr {
    ///         ObjectRepr::Iterable
    ///     }
    ///
    ///     fn enumerate(self: &Arc<Self>) -> Enumerator {
    ///         self.mapped_enumerator(|this| {
    ///             Box::new(this.0.iter().cloned().map(Value::from))
    ///         })
    ///     }
    /// }
    /// ```
    fn mapped_rev_enumerator<F>(self: &Arc<Self>, maker: F) -> Enumerator
    where
        F: for<'a> FnOnce(
                &'a Self,
            )
                -> Box<dyn DoubleEndedIterator<Item = Value> + Send + Sync + 'a>
            + Send
            + Sync
            + 'static,
        Self: Sized,
    {
        // Taken from `mapped_enumerator`.

        struct Iter {
            iter: Box<dyn DoubleEndedIterator<Item = Value> + Send + Sync + 'static>,
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

        impl DoubleEndedIterator for Iter {
            fn next_back(&mut self) -> Option<Self::Item> {
                self.iter.next_back()
            }
        }

        // SAFETY: this is safe because the `Iter` will keep our object alive.
        let iter = unsafe {
            std::mem::transmute::<
                Box<dyn DoubleEndedIterator<Item = _>>,
                Box<dyn DoubleEndedIterator<Item = _> + Send + Sync>,
            >(maker(self))
        };
        let _object = DynObject::new(self.clone());
        Enumerator::RevIter(Box::new(Iter { iter, _object }))
    }

    impl_object_helpers!(&Arc<Self>);
}

impl<T: Object + Send + Sync + 'static> ObjectExt for T {}

/// Enumerators help define iteration behavior for [`Object`]s.
///
/// When Jinja wants to know the length of an object, if it's empty or
/// not or if it wants to iterate over it, it will ask the [`Object`] to
/// enumerate itself with the [`enumerate`](Object::enumerate) method.  The
/// returned enumerator has enough information so that the object can be
/// iterated over, but it does not necessarily mean that iteration actually
/// starts or that it has the data to yield the right values.
///
/// In fact, you should never inspect an enumerator.  You can create it or
/// forward it.  For actual iteration use [`ObjectExt::try_iter`] etc.
#[non_exhaustive]
pub enum Enumerator {
    /// Marks non enumerable objects.
    ///
    /// Such objects cannot be iterated over, the length is unknown which
    /// means they are not considered empty by the engine.  This is a good
    /// choice for plain objects.
    ///
    /// | Iterable | Length  |
    /// |----------|---------|
    /// | no       | unknown |
    NonEnumerable,

    /// The empty enumerator.  It yields no elements.
    ///
    /// | Iterable | Length      |
    /// |----------|-------------|
    /// | yes      | known (`0`) |
    Empty,

    /// A slice of static strings.
    ///
    /// This is a useful enumerator to enumerate the attributes of an
    /// object or the keys in a string hash map.
    ///
    /// | Iterable | Length       |
    /// |----------|--------------|
    /// | yes      | known        |
    Str(&'static [&'static str]),

    /// A dynamic iterator over values.
    ///
    /// The length is known if the [`Iterator::size_hint`] has matching lower
    /// and upper bounds.  The logic used by the engine is the following:
    ///
    /// ```
    /// # let iter = Some(1).into_iter();
    /// let len = match iter.size_hint() {
    ///     (lower, Some(upper)) if lower == upper => Some(lower),
    ///     _ => None
    /// };
    /// ```
    ///
    /// Because the engine prefers repeatable iteration, it will keep creating
    /// new enumerators every time the iteration should restart.  Sometimes
    /// that might not always be possible (eg: you stream data in) in which
    /// case
    ///
    /// | Iterable | Length          |
    /// |----------|-----------------|
    /// | yes      | sometimes known |
    Iter(Box<dyn Iterator<Item = Value> + Send + Sync>),

    /// Like `Iter` but supports efficient reversing.
    ///
    /// This means that the iterator has to be of type [`DoubleEndedIterator`].
    ///
    /// | Iterable | Length          |
    /// |----------|-----------------|
    /// | yes      | sometimes known |
    RevIter(Box<dyn DoubleEndedIterator<Item = Value> + Send + Sync>),

    /// Indicates sequential iteration.
    ///
    /// This instructs the engine to iterate over an object by enumerating it
    /// from `0` to `n` by calling [`Object::get_value`].  This is essentially the
    /// way sequences are supposed to be enumerated.
    ///
    /// | Iterable | Length          |
    /// |----------|-----------------|
    /// | yes      | known           |
    Seq(usize),

    /// A vector of known values to iterate over.
    ///
    /// The iterator will yield each value in the vector one after another.
    ///
    /// | Iterable | Length          |
    /// |----------|-----------------|
    /// | yes      | known           |
    Values(Vec<Value>),
}

/// Defines the natural representation of this object.
///
/// An [`ObjectRepr`] is a reduced form of
/// [`ValueKind`](crate::value::ValueKind) which only contains value which can
/// be represented by objects.  For instance an object can never be a primitive
/// and as such those kinds are unavailable.
///
/// The representation influences how values are serialized, stringified or
/// what kind they report.
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[non_exhaustive]
pub enum ObjectRepr {
    /// An object that has no reasonable representation.
    ///
    /// - **Default Render:** [`Debug`]
    /// - **Collection Behavior:** none
    /// - **Iteration Behavior:** none
    /// - **Serialize:** [`Debug`] / [`render`](Object::render) output as string
    Plain,

    /// Represents a map or object.
    ///
    /// - **Default Render:** `{key: value,...}` pairs
    /// - **Collection Behavior:** looks like a map, can be indexed by key, has a length
    /// - **Iteration Behavior:** iterates over keys
    /// - **Serialize:** Serializes as map
    Map,

    /// Represents a sequence (eg: array/list).
    ///
    /// - **Default Render:** `[value,...]`
    /// - **Collection Behavior:** looks like a list, can be indexed by index, has a length
    /// - **Iteration Behavior:** iterates over values
    /// - **Serialize:** Serializes as list
    Seq,

    /// Represents a non indexable, iterable object.
    ///
    /// - **Default Render:** `[value,...]` (if length is known), `"<iterator>"` otherwise.
    /// - **Collection Behavior:** looks like a list if length is known, cannot be indexed
    /// - **Iteration Behavior:** iterates over values
    /// - **Serialize:** Serializes as list
    Iterable,
}

type_erase! {
    pub trait Object => DynObject {
        fn repr(&self) -> ObjectRepr;

        fn get_value(&self, key: &Value) -> Option<Value>;

        fn enumerate(&self) -> Enumerator;

        fn is_true(&self) -> bool;

        fn enumerator_len(&self) -> Option<usize>;

        fn call(
            &self,
            state: &State<'_, '_>,
            args: &[Value]
        ) -> Result<Value, Error>;

        fn call_method(
            &self,
            state: &State<'_, '_>,
            method: &str,
            args: &[Value]
        ) -> Result<Value, Error>;

        fn custom_cmp(&self, other: &DynObject) -> Option<Ordering>;

        fn render(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result;

        impl fmt::Debug {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result;
        }
    }
}

unsafe impl Send for DynObject {}
unsafe impl Sync for DynObject {}

impl DynObject {
    impl_object_helpers!(pub &Self);

    /// Checks if this dyn object is the same as another.
    pub(crate) fn is_same_object(&self, other: &DynObject) -> bool {
        self.ptr == other.ptr && self.vtable == other.vtable
    }

    /// Checks if the two dyn objects are of the same type.
    pub(crate) fn is_same_object_type(&self, other: &DynObject) -> bool {
        self.type_id() == other.type_id()
    }
}

impl Hash for DynObject {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        if let Some(iter) = self.try_iter_pairs() {
            for (key, value) in iter {
                key.hash(state);
                value.hash(state);
            }
        }
    }
}

impl fmt::Display for DynObject {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.render(f)
    }
}

impl Enumerator {
    fn query_len(&self) -> Option<usize> {
        Some(match self {
            Enumerator::Empty => 0,
            Enumerator::Values(v) => v.len(),
            Enumerator::Str(v) => v.len(),
            Enumerator::Iter(i) => match i.size_hint() {
                (a, Some(b)) if a == b => a,
                _ => return None,
            },
            Enumerator::RevIter(i) => match i.size_hint() {
                (a, Some(b)) if a == b => a,
                _ => return None,
            },
            Enumerator::Seq(v) => *v,
            Enumerator::NonEnumerable => return None,
        })
    }
}

macro_rules! impl_value_vec {
    ($vec_type:ident) => {
        impl<T> Object for $vec_type<T>
        where
            T: Into<Value> + Clone + Send + Sync + fmt::Debug + 'static,
        {
            fn repr(self: &Arc<Self>) -> ObjectRepr {
                ObjectRepr::Seq
            }

            #[inline(always)]
            fn get_value(self: &Arc<Self>, key: &Value) -> Option<Value> {
                self.get(some!(key.as_usize())).cloned().map(|v| v.into())
            }

            fn enumerate(self: &Arc<Self>) -> Enumerator {
                Enumerator::Seq(self.len())
            }
        }

        impl<T> From<$vec_type<T>> for Value
        where
            T: Into<Value> + Clone + Send + Sync + fmt::Debug + 'static,
        {
            fn from(val: $vec_type<T>) -> Self {
                Value::from_object(val)
            }
        }
    };
}

#[allow(unused)]
macro_rules! impl_value_iterable {
    ($iterable_type:ident, $enumerator:ident) => {
        impl<T> Object for $iterable_type<T>
        where
            T: Into<Value> + Clone + Send + Sync + fmt::Debug + 'static,
        {
            fn repr(self: &Arc<Self>) -> ObjectRepr {
                ObjectRepr::Iterable
            }

            fn enumerate(self: &Arc<Self>) -> Enumerator {
                self.clone()
                    .$enumerator(|this| Box::new(this.iter().map(|x| x.clone().into())))
            }
        }

        impl<T> From<$iterable_type<T>> for Value
        where
            T: Into<Value> + Clone + Send + Sync + fmt::Debug + 'static,
        {
            fn from(val: $iterable_type<T>) -> Self {
                Value::from_object(val)
            }
        }
    };
}

macro_rules! impl_str_map_helper {
    ($map_type:ident, $key_type:ty, $enumerator:ident) => {
        impl<V> Object for $map_type<$key_type, V>
        where
            V: Into<Value> + Clone + Send + Sync + fmt::Debug + 'static,
        {
            #[inline(always)]
            fn get_value(self: &Arc<Self>, key: &Value) -> Option<Value> {
                self.get(some!(key.as_str())).cloned().map(|v| v.into())
            }

            fn enumerate(self: &Arc<Self>) -> Enumerator {
                self.$enumerator(|this| Box::new(this.keys().map(|x| Value::from(x as &str))))
            }

            fn enumerator_len(self: &Arc<Self>) -> Option<usize> {
                Some(self.len())
            }
        }
    };
}

macro_rules! impl_str_map {
    ($map_type:ident, $enumerator:ident) => {
        impl_str_map_helper!($map_type, String, $enumerator);
        impl_str_map_helper!($map_type, Arc<str>, $enumerator);

        impl<V> From<$map_type<String, V>> for Value
        where
            V: Into<Value> + Send + Sync + Clone + fmt::Debug + 'static,
        {
            fn from(val: $map_type<String, V>) -> Self {
                Value::from_object(val)
            }
        }

        impl<V> From<$map_type<Arc<str>, V>> for Value
        where
            V: Into<Value> + Send + Sync + Clone + fmt::Debug + 'static,
        {
            fn from(val: $map_type<Arc<str>, V>) -> Self {
                Value::from_object(val)
            }
        }

        impl<'a, V> From<$map_type<&'a str, V>> for Value
        where
            V: Into<Value> + Send + Sync + Clone + fmt::Debug + 'static,
        {
            fn from(val: $map_type<&'a str, V>) -> Self {
                Value::from(
                    val.into_iter()
                        .map(|(k, v)| (Arc::from(k), v))
                        .collect::<$map_type<Arc<str>, V>>(),
                )
            }
        }

        impl<'a, V> From<$map_type<Cow<'a, str>, V>> for Value
        where
            V: Into<Value> + Send + Sync + Clone + fmt::Debug + 'static,
        {
            fn from(val: $map_type<Cow<'a, str>, V>) -> Self {
                Value::from(
                    val.into_iter()
                        .map(|(k, v)| (Arc::from(k), v))
                        .collect::<$map_type<Arc<str>, V>>(),
                )
            }
        }
    };
}

macro_rules! impl_value_map {
    ($map_type:ident, $enumerator:ident) => {
        impl<V> Object for $map_type<Value, V>
        where
            V: Into<Value> + Clone + Send + Sync + fmt::Debug + 'static,
        {
            #[inline(always)]
            fn get_value(self: &Arc<Self>, key: &Value) -> Option<Value> {
                self.get(key).cloned().map(|v| v.into())
            }

            fn enumerate(self: &Arc<Self>) -> Enumerator {
                self.$enumerator(|this| Box::new(this.keys().cloned()))
            }

            fn enumerator_len(self: &Arc<Self>) -> Option<usize> {
                Some(self.len())
            }
        }

        impl<V> From<$map_type<Value, V>> for Value
        where
            V: Into<Value> + Send + Sync + Clone + fmt::Debug + 'static,
        {
            fn from(val: $map_type<Value, V>) -> Self {
                Value::from_object(val)
            }
        }
    };
}

impl_value_vec!(Vec);
impl_value_map!(BTreeMap, mapped_rev_enumerator);
impl_str_map!(BTreeMap, mapped_rev_enumerator);

#[cfg(feature = "std_collections")]
mod std_collections_impls {
    use super::*;
    use std::collections::{BTreeSet, HashMap, HashSet, LinkedList, VecDeque};

    impl_value_iterable!(LinkedList, mapped_rev_enumerator);
    impl_value_iterable!(HashSet, mapped_enumerator);
    impl_value_iterable!(BTreeSet, mapped_rev_enumerator);
    impl_str_map!(HashMap, mapped_enumerator);
    impl_value_map!(HashMap, mapped_enumerator);
    impl_value_vec!(VecDeque);
}

#[cfg(feature = "preserve_order")]
mod preserve_order_impls {
    use super::*;
    use indexmap::IndexMap;

    impl_value_map!(IndexMap, mapped_rev_enumerator);
}

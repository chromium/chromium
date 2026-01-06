//! Global functions and abstractions.
//!
//! This module provides the abstractions for functions that can registered as
//! global functions to the environment via
//! [`add_function`](crate::Environment::add_function).
//!
//! # Using Functions
//!
//! Functions can be called in any place where an expression is valid.  They
//! are useful to retrieve data.  Some functions are special and provided
//! by the engine (like `super`) within certain context, others are global.
//!
//! The following is a motivating example:
//!
//! ```jinja
//! <pre>{{ debug() }}</pre>
//! ```
//!
//! # Custom Functions
//!
//! A custom global function is just a simple rust function which accepts optional
//! arguments and then returns a result.  Global functions are typically used to
//! perform a data loading operation.  For instance these functions can be used
//! to expose data to the template that hasn't been provided by the individual
//! render invocation.
//!
//! ```rust
//! # use minijinja::Environment;
//! # let mut env = Environment::new();
//! use minijinja::{Error, ErrorKind};
//!
//! fn include_file(name: String) -> Result<String, Error> {
//!     std::fs::read_to_string(&name)
//!         .map_err(|e| Error::new(
//!             ErrorKind::InvalidOperation,
//!             "cannot load file"
//!         ).with_source(e))
//! }
//!
//! env.add_function("include_file", include_file);
//! ```
//!
#![cfg_attr(
    feature = "deserialization",
    doc = r#"
# Arguments in Custom Functions

All arguments in custom functions must implement the [`ArgType`] trait.
Standard types, such as `String`, `i32`, `bool`, `f64`, etc, already implement this trait.
There are also helper types that will make it easier to extract an arguments with custom types.
The [`ViaDeserialize<T>`](crate::value::ViaDeserialize) type, for instance, can accept any
type `T` that implements the `Deserialize` trait from `serde`.

```rust
# use minijinja::Environment;
# use serde::Deserialize;
# let mut env = Environment::new();
use minijinja::value::ViaDeserialize;

#[derive(Deserialize)]
struct Person {
    name: String,
    age: i32,
}

fn is_adult(person: ViaDeserialize<Person>) -> bool {
    person.age >= 18
}

env.add_function("is_adult", is_adult);
```
"#
)]
//!
//! # Note on Keyword Arguments
//!
//! MiniJinja inherits a lot of the runtime model from Jinja2.  That includes support for
//! keyword arguments.  These however are a concept not native to Rust which makes them
//! somewhat uncomfortable to work with.  In MiniJinja keyword arguments are implemented by
//! converting them into an extra parameter represented by a map.  That means if you call
//! a function as `foo(1, 2, three=3, four=4)` the function gets three arguments:
//!
//! ```json
//! [1, 2, {"three": 3, "four": 4}]
//! ```
//!
//! If a function wants to disambiguate between a value passed as keyword argument or not,
//! the [`Value::is_kwargs`] can be used which returns `true` if a value represents
//! keyword arguments as opposed to just a map.  A more convenient way to work with keyword
//! arguments is the [`Kwargs`](crate::value::Kwargs) type.
//!
//! # Built-in Functions
//!
//! When the `builtins` feature is enabled a range of built-in functions are
//! automatically added to the environment.  These are also all provided in
//! this module.  Note though that these functions are not to be
//! called from Rust code as their exact interface (arguments and return types)
//! might change from one MiniJinja version to another.
use std::fmt;
use std::sync::Arc;

use crate::error::Error;
use crate::utils::SealedMarker;
use crate::value::{ArgType, FunctionArgs, FunctionResult, Object, ObjectRepr, Value};
use crate::vm::State;

type FuncFunc = dyn Fn(&State, &[Value]) -> Result<Value, Error> + Sync + Send + 'static;

/// A boxed function.
#[derive(Clone)]
pub(crate) struct BoxedFunction(Arc<FuncFunc>, #[cfg(feature = "debug")] &'static str);

/// A utility trait that represents global functions.
///
/// This trait is used by the [`add_function`](crate::Environment::add_function)
/// method to abstract over different types of functions.
///
/// Functions which at the very least accept the [`State`] by reference as first
/// parameter and additionally up to 4 further parameters.  They share much of
/// their interface with [`filters`](crate::filters).
///
/// A function can return any of the following types:
///
/// * `Rv` where `Rv` implements `Into<Value>`
/// * `Result<Rv, Error>` where `Rv` implements `Into<Value>`
///
/// The parameters can be marked optional by using `Option<T>`.  The last
/// argument can also use [`Rest<T>`](crate::value::Rest) to capture the
/// remaining arguments.  All types are supported for which
/// [`ArgType`] is implemented.
///
/// For a list of built-in functions see [`functions`](crate::functions).
///
/// **Note:** this trait cannot be implemented and only exists drive the
/// functionality of [`add_function`](crate::Environment::add_function)
/// and [`from_function`](crate::value::Value::from_function).  If you want
/// to implement a custom callable, you can directly implement
/// [`Object::call`] which is what the engine actually uses internally.
///
/// This trait is also used for [`filters`](crate::filters) and
/// [`tests`](crate::tests).
///
/// # Basic Example
///
/// ```rust
/// # use minijinja::Environment;
/// # let mut env = Environment::new();
/// use minijinja::{Error, ErrorKind};
///
/// fn include_file(name: String) -> Result<String, Error> {
///     std::fs::read_to_string(&name)
///         .map_err(|e| Error::new(
///             ErrorKind::InvalidOperation,
///             "cannot load file"
///         ).with_source(e))
/// }
///
/// env.add_function("include_file", include_file);
/// ```
///
/// ```jinja
/// {{ include_file("filename.txt") }}
/// ```
///
/// # Variadic
///
/// ```
/// # use minijinja::Environment;
/// # let mut env = Environment::new();
/// use minijinja::value::Rest;
///
/// fn sum(values: Rest<i64>) -> i64 {
///     values.iter().sum()
/// }
///
/// env.add_function("sum", sum);
/// ```
///
/// ```jinja
/// {{ sum(1, 2, 3) }} -> 6
/// ```
///
/// # Optional Arguments
///
/// ```
/// # use minijinja::Environment;
/// # let mut env = Environment::new();
/// fn substr(value: String, start: u32, end: Option<u32>) -> String {
///     let end = end.unwrap_or(value.len() as _);
///     value.get(start as usize..end as usize).unwrap_or_default().into()
/// }
///
/// env.add_filter("substr", substr);
/// ```
///
/// ```jinja
/// {{ "Foo Bar Baz"|substr(4) }} -> Bar Baz
/// {{ "Foo Bar Baz"|substr(4, 7) }} -> Bar
/// ```
pub trait Function<Rv, Args: for<'a> FunctionArgs<'a>>: Send + Sync + 'static {
    /// Calls a function with the given arguments.
    #[doc(hidden)]
    fn invoke(&self, args: <Args as FunctionArgs<'_>>::Output, _: SealedMarker) -> Rv;
}

// This is necessary to avoid a bug in the trait solver. See
// https://github.com/mitsuhiko/minijinja/pull/787 for more details.
trait FunctionHelper<Rv, Args> {
    fn invoke_nested(&self, args: Args) -> Rv;
}

macro_rules! tuple_impls {
    ( $( $name:ident )* ) => {
        impl<Func, Rv, $($name),*> FunctionHelper<Rv, ($($name,)*)> for Func
        where
            Func: Fn($($name),*) -> Rv
        {
            fn invoke_nested(&self, args: ($($name,)*)) -> Rv {
                #[allow(non_snake_case)]
                let ($($name,)*) = args;
                (self)($($name,)*)
            }
        }

        impl<Func, Rv, $($name),*> Function<Rv, ($($name,)*)> for Func
        where
            Func: Send + Sync + 'static,
            // the crazy bounds here exist to enable borrowing in closures
            Func: Fn($($name),*) -> Rv + for<'a> FunctionHelper<Rv, ($(<$name as ArgType<'a>>::Output,)*)>,
            Rv: FunctionResult,
            $($name: for<'a> ArgType<'a>,)*
        {
            // Need to allow this lint for the one-element tuple case.
            #[allow(clippy::needless_lifetimes)]
            fn invoke<'a>(&self, args: ($(<$name as ArgType<'a>>::Output,)*), _: SealedMarker) -> Rv {
                self.invoke_nested(args)
            }
        }
    };
}

tuple_impls! {}
tuple_impls! { A }
tuple_impls! { A B }
tuple_impls! { A B C }
tuple_impls! { A B C D }
tuple_impls! { A B C D E }

impl BoxedFunction {
    /// Creates a new boxed filter.
    pub fn new<F, Rv, Args>(f: F) -> BoxedFunction
    where
        F: Function<Rv, Args>,
        Rv: FunctionResult,
        Args: for<'a> FunctionArgs<'a>,
    {
        BoxedFunction(
            Arc::new(move |state, args| -> Result<Value, Error> {
                f.invoke(ok!(Args::from_values(Some(state), args)), SealedMarker)
                    .into_result()
            }),
            #[cfg(feature = "debug")]
            std::any::type_name::<F>(),
        )
    }

    /// Invokes the function.
    pub fn invoke(&self, state: &State, args: &[Value]) -> Result<Value, Error> {
        (self.0)(state, args)
    }

    /// Creates a value from a boxed function.
    pub fn to_value(&self) -> Value {
        Value::from_object(self.clone())
    }
}

impl fmt::Debug for BoxedFunction {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        #[cfg(feature = "debug")]
        {
            if !self.1.is_empty() {
                return f.write_str(self.1);
            }
        }
        f.write_str("function")
    }
}

impl Object for BoxedFunction {
    fn repr(self: &Arc<Self>) -> ObjectRepr {
        ObjectRepr::Plain
    }

    fn call(self: &Arc<Self>, state: &State, args: &[Value]) -> Result<Value, Error> {
        self.invoke(state, args)
    }
}

#[cfg(feature = "builtins")]
mod builtins {
    use std::cmp::Ordering;

    use super::*;

    use crate::error::ErrorKind;
    use crate::value::{Rest, ValueMap, ValueRepr};

    /// Returns a range.
    ///
    /// Return a list containing an arithmetic progression of integers. `range(i,
    /// j)` returns `[i, i+1, i+2, ..., j-1]`. `lower` defaults to 0. When `step` is
    /// given, it specifies the increment (or decrement). For example, `range(4)`
    /// and `range(0, 4, 1)` return `[0, 1, 2, 3]`. The end point is omitted.
    ///
    /// ```jinja
    /// <ul>
    /// {% for num in range(1, 11) %}
    ///   <li>{{ num }}
    /// {% endfor %}
    /// </ul>
    /// ```
    ///
    /// This function will refuse to create ranges over 10.000 items.
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn range(lower: isize, upper: Option<isize>, step: Option<isize>) -> Result<Value, Error> {
        fn to_result<I: ExactSizeIterator<Item = isize> + Send + Sync + Clone + 'static>(
            i: I,
        ) -> Result<Value, Error> {
            if i.len() > 100000 {
                Err(Error::new(
                    ErrorKind::InvalidOperation,
                    "range has too many elements",
                ))
            } else {
                Ok(Value::make_iterable(move || i.clone()))
            }
        }

        let rng = match upper {
            Some(upper) => lower..upper,
            None => 0..lower,
        };

        let Some(step) = step else {
            return to_result(rng);
        };

        match step.cmp(&0) {
            Ordering::Equal => Err(Error::new(
                ErrorKind::InvalidOperation,
                "cannot create range with step of 0",
            )),
            Ordering::Greater => to_result(rng.step_by(step as usize)),
            Ordering::Less => {
                // handle negative steps
                debug_assert!(step < 0);
                let (start, end) = match upper {
                    Some(upper) => (lower, upper),
                    None => (0, lower),
                };

                let len = if start <= end {
                    0
                } else {
                    ((start - end + (-step) - 1) / (-step)) as usize
                };

                let iter = (0..len).map(move |i| start + (i as isize) * step);
                to_result(iter)
            }
        }
    }

    /// Creates a dictionary.
    ///
    /// This is a convenient alternative for a dictionary literal.
    /// `{"foo": "bar"}` is the same as `dict(foo="bar")`.
    ///
    /// ```jinja
    /// <script>const CONFIG = {{ dict(
    ///   DEBUG=true,
    ///   API_URL_PREFIX="/api"
    /// )|tojson }};</script>
    /// ```
    ///
    /// Additionally this can be used to merge objects by passing extra keyword
    /// arguments:
    ///
    /// ```jinja
    /// {% set new_dict = dict(old_dict, extra_value=2) %}
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn dict(value: Option<Value>, update_with: crate::value::Kwargs) -> Result<Value, Error> {
        let mut rv = match value {
            None => ValueMap::default(),
            Some(value) => match value.0 {
                ValueRepr::Undefined(_) => ValueMap::default(),
                ValueRepr::Object(obj) if obj.repr() == ObjectRepr::Map => {
                    obj.try_iter_pairs().into_iter().flatten().collect()
                }
                _ => return Err(Error::from(ErrorKind::InvalidOperation)),
            },
        };

        if update_with.values.is_true() {
            rv.extend(
                update_with
                    .values
                    .iter()
                    .map(|(k, v)| (k.clone(), v.clone())),
            );
        }

        Ok(Value::from_object(rv))
    }

    /// Outputs the current context or the arguments stringified.
    ///
    /// This is a useful function to quickly figure out the state of affairs
    /// in a template.  It emits a stringified debug dump of the current
    /// engine state including the layers of the context, the current block
    /// and auto escaping setting.  The exact output is not defined and might
    /// change from one version of Jinja2 to the next.
    ///
    /// ```jinja
    /// <pre>{{ debug() }}</pre>
    /// <pre>{{ debug(variable1, variable2) }}</pre>
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn debug(state: &State, args: Rest<Value>) -> String {
        if args.is_empty() {
            format!("{state:#?}")
        } else if args.len() == 1 {
            format!("{:#?}", args.0[0])
        } else {
            format!("{:#?}", &args.0[..])
        }
    }

    /// Creates a new container that allows attribute assignment using the `{% set %}` tag.
    ///
    /// ```jinja
    /// {% set ns = namespace() %}
    /// {% set ns.foo = 'bar' %}
    /// ```
    ///
    /// The main purpose of this is to allow carrying a value from within a loop body
    /// to an outer scope. Initial values can be provided as a dict, as keyword arguments,
    /// or both (same behavior as [`dict`]).
    #[cfg_attr(docsrs, doc(cfg(feature = "builtins")))]
    pub fn namespace(defaults: Option<Value>) -> Result<Value, Error> {
        let ns = crate::value::namespace_object::Namespace::default();
        if let Some(defaults) = defaults {
            if let Some(pairs) = defaults
                .as_object()
                .filter(|x| matches!(x.repr(), ObjectRepr::Map))
                .and_then(|x| x.try_iter_pairs())
            {
                for (key, value) in pairs {
                    if let Some(key) = key.as_str() {
                        ns.set_value(key, value);
                    }
                }
            } else {
                return Err(Error::new(
                    ErrorKind::InvalidOperation,
                    format!(
                        "expected object or keyword arguments, got {}",
                        defaults.kind()
                    ),
                ));
            }
        }
        Ok(Value::from_object(ns))
    }
}

#[cfg(feature = "builtins")]
pub use self::builtins::*;

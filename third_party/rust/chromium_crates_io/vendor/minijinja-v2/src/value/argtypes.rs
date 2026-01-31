use std::borrow::Cow;
use std::cell::RefCell;
use std::collections::HashSet;
use std::ops::{Deref, DerefMut};
use std::sync::Arc;

use crate::error::{Error, ErrorKind};
use crate::value::{
    DynObject, ObjectExt, ObjectRepr, Packed, SmallStr, StringType, Value, ValueKind, ValueMap,
    ValueRepr,
};
use crate::vm::State;

use super::{Enumerator, Object};

/// A utility trait that represents the return value of functions, filters and tests.
///
/// It's implemented for the following types:
///
/// * `Rv` where `Rv` implements `Into<AnyMapObject>`
/// * `Result<Rv, Error>` where `Rv` implements `Into<Value>`
pub trait FunctionResult {
    #[doc(hidden)]
    fn into_result(self) -> Result<Value, Error>;
}

impl<I: Into<Value>> FunctionResult for Result<I, Error> {
    fn into_result(self) -> Result<Value, Error> {
        self.map(Into::into)
    }
}

impl<I: Into<Value>> FunctionResult for I {
    fn into_result(self) -> Result<Value, Error> {
        Ok(self.into())
    }
}

/// Helper trait representing valid filter, test and function arguments.
///
/// Since it's more convenient to write filters and tests with concrete
/// types instead of values, this helper trait exists to automatically
/// perform this conversion.  It is implemented for functions up to an
/// arity of 5 parameters.
///
/// For each argument the conversion is performed via the [`ArgType`]
/// trait which is implemented for many common types.  For manual
/// conversions the [`from_args`] utility should be used.
pub trait FunctionArgs<'a> {
    /// The output type of the function arguments.
    type Output;

    /// Converts to function arguments from a slice of values.
    #[doc(hidden)]
    fn from_values(state: Option<&'a State>, values: &'a [Value]) -> Result<Self::Output, Error>;
}

/// Utility function to convert a slice of values into arguments.
///
/// This performs the same conversion that [`Function`](crate::functions::Function)
/// performs.  It exists so that you one can leverage the same functionality when
/// implementing [`Object::call_method`](crate::value::Object::call_method).
///
/// ```
/// # use minijinja::value::from_args;
/// # use minijinja::value::Value;
/// # fn foo() -> Result<(), minijinja::Error> {
/// # let args = vec![Value::from("foo"), Value::from(42i64)]; let args = &args[..];
/// // args is &[Value]
/// let (string, num): (&str, i64) = from_args(args)?;
/// # Ok(()) } fn main() { foo().unwrap(); }
/// ```
///
/// Note that only value conversions are supported which means that `&State` is not
/// a valid conversion type.
///
/// You can also use this function to split positional and keyword arguments ([`Kwargs`]):
///
/// ```
/// # use minijinja::value::{Value, Rest, Kwargs, from_args};
/// # use minijinja::Error;
/// # fn foo() -> Result<(), minijinja::Error> {
/// # let args = vec![Value::from("foo"), Value::from(42i64)]; let args = &args[..];
/// // args is &[Value], kwargs is Kwargs
/// let (args, kwargs): (&[Value], Kwargs) = from_args(args)?;
/// # Ok(())
/// # } fn main() { foo().unwrap(); }
/// ```
#[inline(always)]
pub fn from_args<'a, Args>(values: &'a [Value]) -> Result<Args, Error>
where
    Args: FunctionArgs<'a, Output = Args>,
{
    Args::from_values(None, values)
}

/// A trait implemented by all filter/test argument types.
///
/// This trait is used by [`FunctionArgs`].  It's implemented for many common
/// types that are typically passed to filters, tests or functions.  It's
/// implemented for the following types:
///
/// * eval state: [`&State`](crate::State) (see below for notes)
/// * unsigned integers: [`u8`], [`u16`], [`u32`], [`u64`], [`u128`], [`usize`]
/// * signed integers: [`i8`], [`i16`], [`i32`], [`i64`], [`i128`]
/// * floats: [`f32`], [`f64`]
/// * bool: [`bool`]
/// * string: [`String`], [`&str`], `Cow<'_, str>`, [`char`]
/// * bytes: [`&[u8]`][`slice`]
/// * values: [`Value`], `&Value`
/// * vectors: [`Vec<T>`]
/// * objects: [`DynObject`], [`Arc<T>`], `&T` (where `T` is an [`Object`])
/// * serde deserializable: [`ViaDeserialize<T>`](crate::value::deserialize::ViaDeserialize)
/// * keyword arguments: [`Kwargs`]
/// * leftover arguments: [`Rest<T>`]
///
/// The type is also implemented for optional values (`Option<T>`) which is used
/// to encode optional parameters to filters, functions or tests.  Additionally
/// it's implemented for [`Rest<T>`] which is used to encode the remaining arguments
/// of a function call.
///
/// ## Notes on Borrowing
///
/// Note on that there is an important difference between `String` and `&str`:
/// the former will be valid for all values and an implicit conversion to string
/// via [`ToString`] will take place, for the latter only values which are already
/// strings will be passed.  A compromise between the two is `Cow<'_, str>` which
/// will behave like `String` but borrows when possible.
///
/// Byte slices will borrow out of values carrying bytes or strings.  In the latter
/// case the utf-8 bytes are returned.
///
/// There are also further restrictions imposed on borrowing in some situations.
/// For instance you cannot implicitly borrow out of sequences which means that
/// for instance `Vec<&str>` is not a legal argument.
///
/// ## Notes on State
///
/// When `&State` is used, it does not consume a passed parameter.  This means that
/// a filter that takes `(&State, String)` actually only has one argument.  The
/// state is passed implicitly.
pub trait ArgType<'a> {
    /// The output type of this argument.
    type Output;

    #[doc(hidden)]
    fn from_value(value: Option<&'a Value>) -> Result<Self::Output, Error>;

    #[doc(hidden)]
    fn from_value_owned(_value: Value) -> Result<Self::Output, Error> {
        Err(Error::new(
            ErrorKind::InvalidOperation,
            "type conversion is not legal in this situation (implicit borrow)",
        ))
    }

    #[doc(hidden)]
    fn from_state_and_value(
        _state: Option<&'a State>,
        value: Option<&'a Value>,
    ) -> Result<(Self::Output, usize), Error> {
        Ok((ok!(Self::from_value(value)), 1))
    }

    #[doc(hidden)]
    #[inline(always)]
    fn from_state_and_values(
        state: Option<&'a State>,
        values: &'a [Value],
        offset: usize,
    ) -> Result<(Self::Output, usize), Error> {
        Self::from_state_and_value(state, values.get(offset))
    }

    #[doc(hidden)]
    #[inline(always)]
    fn is_trailing() -> bool {
        false
    }
}

macro_rules! tuple_impls {
    ( $( $name:ident )* * $rest_name:ident ) => {
        impl<'a, $($name,)* $rest_name> FunctionArgs<'a> for ($($name,)* $rest_name,)
            where $($name: ArgType<'a>,)* $rest_name: ArgType<'a>
        {
            type Output = ($($name::Output,)* $rest_name::Output ,);

            fn from_values(state: Option<&'a State>, mut values: &'a [Value]) -> Result<Self::Output, Error> {
                #![allow(non_snake_case, unused)]
                $( let $name; )*
                let mut $rest_name = None;
                let mut idx = 0;

                // special case: the last type is marked trailing (eg: for Kwargs) and we have at
                // least one value.  In that case we need to read it first before going to the rest
                // of the arguments.  This is needed to support from_args::<(&[Value], Kwargs)>
                // or similar.
                let rest_first = $rest_name::is_trailing() && !values.is_empty();
                if rest_first {
                    let (val, offset) = ok!($rest_name::from_state_and_values(state, values, values.len() - 1));
                    $rest_name = Some(val);
                    values = &values[..values.len() - offset];
                }
                $(
                    let (val, offset) = ok!($name::from_state_and_values(state, values, idx));
                    $name = val;
                    idx += offset;
                )*

                if !rest_first {
                    let (val, offset) = ok!($rest_name::from_state_and_values(state, values, idx));
                    $rest_name = Some(val);
                    idx += offset;
                }

                if values.get(idx).is_some() {
                    Err(Error::from(ErrorKind::TooManyArguments))
                } else {
                    // SAFETY: this is safe because both no matter what `rest_first` is set to
                    // the rest_name variable is set at this point.
                    Ok(($($name,)* unsafe { $rest_name.unwrap_unchecked() },))
                }
            }
        }
    };
}

impl<'a> FunctionArgs<'a> for () {
    type Output = ();

    fn from_values(_state: Option<&'a State>, values: &'a [Value]) -> Result<Self::Output, Error> {
        if values.is_empty() {
            Ok(())
        } else {
            Err(Error::from(ErrorKind::TooManyArguments))
        }
    }
}

tuple_impls! { *A }
tuple_impls! { A *B }
tuple_impls! { A B *C }
tuple_impls! { A B C *D }
tuple_impls! { A B C D *E }

impl From<ValueRepr> for Value {
    #[inline(always)]
    fn from(val: ValueRepr) -> Value {
        Value(val)
    }
}

impl<'a> From<&'a [u8]> for Value {
    #[inline(always)]
    fn from(val: &'a [u8]) -> Self {
        ValueRepr::Bytes(Arc::new(val.into())).into()
    }
}

impl<'a> From<&'a str> for Value {
    #[inline(always)]
    fn from(val: &'a str) -> Self {
        SmallStr::try_new(val)
            .map(|small_str| Value(ValueRepr::SmallStr(small_str)))
            .unwrap_or_else(|| Value(ValueRepr::String(val.into(), StringType::Normal)))
    }
}

impl<'a> From<&'a String> for Value {
    #[inline(always)]
    fn from(val: &'a String) -> Self {
        Value::from(val.as_str())
    }
}

impl From<String> for Value {
    #[inline(always)]
    fn from(val: String) -> Self {
        // There is no benefit here of "reusing" the string allocation.  The reason
        // is that From<String> for Arc<str> copies the bytes over anyways.
        Value::from(val.as_str())
    }
}

impl<'a> From<Cow<'a, str>> for Value {
    #[inline(always)]
    fn from(val: Cow<'a, str>) -> Self {
        match val {
            Cow::Borrowed(x) => x.into(),
            Cow::Owned(x) => x.into(),
        }
    }
}

impl From<Arc<str>> for Value {
    fn from(value: Arc<str>) -> Self {
        Value(ValueRepr::String(value, StringType::Normal))
    }
}

impl From<()> for Value {
    #[inline(always)]
    fn from(_: ()) -> Self {
        ValueRepr::None.into()
    }
}

impl<V: Into<Value>> FromIterator<V> for Value {
    fn from_iter<T: IntoIterator<Item = V>>(iter: T) -> Self {
        Value::from_object(iter.into_iter().map(Into::into).collect::<Vec<Value>>())
    }
}

impl<K: Into<Value>, V: Into<Value>> FromIterator<(K, V)> for Value {
    fn from_iter<T: IntoIterator<Item = (K, V)>>(iter: T) -> Self {
        Value::from_object(
            iter.into_iter()
                .map(|(k, v)| (k.into(), v.into()))
                .collect::<ValueMap>(),
        )
    }
}

macro_rules! value_from {
    ($src:ty, $dst:ident) => {
        impl From<$src> for Value {
            #[inline(always)]
            fn from(val: $src) -> Self {
                ValueRepr::$dst(val as _).into()
            }
        }
    };
}

impl From<i128> for Value {
    #[inline(always)]
    fn from(val: i128) -> Self {
        ValueRepr::I128(Packed(val)).into()
    }
}

impl From<u128> for Value {
    #[inline(always)]
    fn from(val: u128) -> Self {
        ValueRepr::U128(Packed(val)).into()
    }
}

impl From<char> for Value {
    #[inline(always)]
    fn from(val: char) -> Self {
        let mut buf = [0u8; 4];
        ValueRepr::SmallStr(SmallStr::try_new(val.encode_utf8(&mut buf)).unwrap()).into()
    }
}

value_from!(bool, Bool);
value_from!(u8, U64);
value_from!(u16, U64);
value_from!(u32, U64);
value_from!(u64, U64);
value_from!(i8, I64);
value_from!(i16, I64);
value_from!(i32, I64);
value_from!(i64, I64);
value_from!(f32, F64);
value_from!(f64, F64);
value_from!(Arc<Vec<u8>>, Bytes);
value_from!(DynObject, Object);

fn unsupported_conversion(kind: ValueKind, target: &str) -> Error {
    Error::new(
        ErrorKind::InvalidOperation,
        format!("cannot convert {kind} to {target}"),
    )
}

macro_rules! primitive_try_from {
    ($ty:ident, {
        $($pat:pat $(if $if_expr:expr)? => $expr:expr,)*
    }) => {
        impl TryFrom<Value> for $ty {
            type Error = Error;

            fn try_from(value: Value) -> Result<Self, Self::Error> {
                match value.0 {
                    $($pat $(if $if_expr)? => TryFrom::try_from($expr).ok(),)*
                    _ => None
                }.ok_or_else(|| unsupported_conversion(value.kind(), stringify!($ty)))
            }
        }

        impl<'a> ArgType<'a> for $ty {
            type Output = Self;
            fn from_value(value: Option<&Value>) -> Result<Self, Error> {
                match value {
                    Some(value) => TryFrom::try_from(value.clone()),
                    None => Err(Error::from(ErrorKind::MissingArgument))
                }
            }

            fn from_value_owned(value: Value) -> Result<Self, Error> {
                TryFrom::try_from(value)
            }
        }
    }
}

macro_rules! primitive_int_try_from {
    ($ty:ident) => {
        primitive_try_from!($ty, {
            ValueRepr::Bool(val) => val as usize,
            ValueRepr::I64(val) => val,
            ValueRepr::U64(val) => val,
            // for the intention here see Key::from_borrowed_value
            ValueRepr::F64(val) if (val as i64 as f64 == val) => val as i64,
            ValueRepr::I128(val) => val.0,
            ValueRepr::U128(val) => val.0,
        });
    }
}

primitive_int_try_from!(u8);
primitive_int_try_from!(u16);
primitive_int_try_from!(u32);
primitive_int_try_from!(u64);
primitive_int_try_from!(u128);
primitive_int_try_from!(i8);
primitive_int_try_from!(i16);
primitive_int_try_from!(i32);
primitive_int_try_from!(i64);
primitive_int_try_from!(i128);
primitive_int_try_from!(usize);
primitive_int_try_from!(isize);

primitive_try_from!(bool, {
    ValueRepr::Bool(val) => val,
});
primitive_try_from!(char, {
    ValueRepr::String(ref val, _) => {
        let mut char_iter = val.chars();
        ok!(char_iter.next().filter(|_| char_iter.next().is_none()).ok_or_else(|| {
            unsupported_conversion(ValueKind::String, "non single character string")
        }))
    },
    ValueRepr::SmallStr(ref val) => {
        let mut char_iter = val.as_str().chars();
        ok!(char_iter.next().filter(|_| char_iter.next().is_none()).ok_or_else(|| {
            unsupported_conversion(ValueKind::String, "non single character string")
        }))
    },
});
primitive_try_from!(f32, {
    ValueRepr::U64(val) => val as f32,
    ValueRepr::I64(val) => val as f32,
    ValueRepr::U128(val) => val.0 as f32,
    ValueRepr::I128(val) => val.0 as f32,
    ValueRepr::F64(val) => val as f32,
});
primitive_try_from!(f64, {
    ValueRepr::U64(val) => val as f64,
    ValueRepr::I64(val) => val as f64,
    ValueRepr::U128(val) => val.0 as f64,
    ValueRepr::I128(val) => val.0 as f64,
    ValueRepr::F64(val) => val,
});

impl<'a> ArgType<'a> for &str {
    type Output = &'a str;

    fn from_value(value: Option<&'a Value>) -> Result<Self::Output, Error> {
        match value {
            Some(value) => value
                .as_str()
                .ok_or_else(|| Error::new(ErrorKind::InvalidOperation, "value is not a string")),
            None => Err(Error::from(ErrorKind::MissingArgument)),
        }
    }
}

impl TryFrom<Value> for Arc<str> {
    type Error = Error;

    fn try_from(value: Value) -> Result<Self, Self::Error> {
        match value.0 {
            ValueRepr::String(x, _) => Ok(x),
            ValueRepr::SmallStr(x) => Ok(Arc::from(x.as_str())),
            ValueRepr::Bytes(ref x) => Ok(Arc::from(String::from_utf8_lossy(x))),
            _ => Err(Error::new(
                ErrorKind::InvalidOperation,
                "value is not a string",
            )),
        }
    }
}

impl<'a> ArgType<'a> for Arc<str> {
    type Output = Arc<str>;

    fn from_value(value: Option<&'a Value>) -> Result<Self::Output, Error> {
        match value {
            Some(value) => TryFrom::try_from(value.clone()),
            None => Err(Error::from(ErrorKind::MissingArgument)),
        }
    }
}

impl<'a> ArgType<'a> for &[u8] {
    type Output = &'a [u8];

    fn from_value(value: Option<&'a Value>) -> Result<Self::Output, Error> {
        match value {
            Some(value) => value
                .as_bytes()
                .ok_or_else(|| Error::new(ErrorKind::InvalidOperation, "value is not in bytes")),
            None => Err(Error::from(ErrorKind::MissingArgument)),
        }
    }
}

impl<'a, T: ArgType<'a>> ArgType<'a> for Option<T> {
    type Output = Option<T::Output>;

    fn from_value(value: Option<&'a Value>) -> Result<Self::Output, Error> {
        match value {
            Some(value) => {
                if value.is_undefined() || value.is_none() {
                    Ok(None)
                } else {
                    T::from_value(Some(value)).map(Some)
                }
            }
            None => Ok(None),
        }
    }

    fn from_value_owned(value: Value) -> Result<Self::Output, Error> {
        if value.is_undefined() || value.is_none() {
            Ok(None)
        } else {
            T::from_value_owned(value).map(Some)
        }
    }
}

impl<'a> ArgType<'a> for Cow<'_, str> {
    type Output = Cow<'a, str>;

    #[inline(always)]
    fn from_value(value: Option<&'a Value>) -> Result<Cow<'a, str>, Error> {
        match value {
            Some(value) => Ok(match value.0 {
                ValueRepr::String(ref s, _) => Cow::Borrowed(s as &str),
                ValueRepr::SmallStr(ref s) => Cow::Borrowed(s.as_str()),
                _ => {
                    if value.is_kwargs() {
                        return Err(Error::new(
                            ErrorKind::InvalidOperation,
                            "cannot convert kwargs to string",
                        ));
                    }
                    Cow::Owned(value.to_string())
                }
            }),
            None => Err(Error::from(ErrorKind::MissingArgument)),
        }
    }
}

impl<'a> ArgType<'a> for &Value {
    type Output = &'a Value;

    #[inline(always)]
    fn from_value(value: Option<&'a Value>) -> Result<&'a Value, Error> {
        match value {
            Some(value) => Ok(value),
            None => Err(Error::from(ErrorKind::MissingArgument)),
        }
    }
}

impl<'a> ArgType<'a> for &[Value] {
    type Output = &'a [Value];

    #[inline(always)]
    fn from_value(value: Option<&'a Value>) -> Result<&'a [Value], Error> {
        match value {
            Some(value) => Ok(std::slice::from_ref(value)),
            None => Err(Error::from(ErrorKind::MissingArgument)),
        }
    }

    fn from_state_and_values(
        _state: Option<&'a State>,
        values: &'a [Value],
        offset: usize,
    ) -> Result<(&'a [Value], usize), Error> {
        let args = values.get(offset..).unwrap_or_default();
        Ok((args, args.len()))
    }
}

impl<'a, T: Object + 'static> ArgType<'a> for &T {
    type Output = &'a T;

    #[inline(always)]
    fn from_value(value: Option<&'a Value>) -> Result<Self::Output, Error> {
        match value {
            Some(value) => value
                .downcast_object_ref()
                .ok_or_else(|| Error::new(ErrorKind::InvalidOperation, "expected object")),
            None => Err(Error::from(ErrorKind::MissingArgument)),
        }
    }
}

impl<'a, T: Object + 'static> ArgType<'a> for Arc<T> {
    type Output = Arc<T>;

    #[inline(always)]
    fn from_value(value: Option<&'a Value>) -> Result<Self::Output, Error> {
        match value {
            Some(value) => value
                .downcast_object()
                .ok_or_else(|| Error::new(ErrorKind::InvalidOperation, "expected object")),
            None => Err(Error::from(ErrorKind::MissingArgument)),
        }
    }
}

/// Utility type to capture remaining arguments.
///
/// In some cases you might want to have a variadic function.  In that case
/// you can define the last argument to a [`Function`](crate::functions::Function)
/// this way.  The `Rest<T>` type will collect all the remaining arguments
/// here.  It's implemented for all [`ArgType`]s.  The type itself deref's
/// into the inner vector.
///
/// ```
/// # use minijinja::Environment;
/// # let mut env = Environment::new();
/// use minijinja::State;
/// use minijinja::value::Rest;
///
/// fn sum(_state: &State, values: Rest<i64>) -> i64 {
///     values.iter().sum()
/// }
/// ```
#[derive(Debug)]
pub struct Rest<T>(pub Vec<T>);

impl<T> Deref for Rest<T> {
    type Target = Vec<T>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl<T> DerefMut for Rest<T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl<'a, T: ArgType<'a, Output = T>> ArgType<'a> for Rest<T> {
    type Output = Self;

    fn from_value(value: Option<&'a Value>) -> Result<Self, Error> {
        Ok(Rest(ok!(value
            .iter()
            .map(|v| T::from_value(Some(v)))
            .collect::<Result<_, _>>())))
    }

    fn from_state_and_values(
        _state: Option<&'a State>,
        values: &'a [Value],
        offset: usize,
    ) -> Result<(Self, usize), Error> {
        let args = values.get(offset..).unwrap_or_default();
        Ok((
            Rest(ok!(args
                .iter()
                .map(|v| T::from_value(Some(v)))
                .collect::<Result<_, _>>())),
            args.len(),
        ))
    }
}

/// Utility to accept keyword arguments.
///
/// Keyword arguments are represented as regular values as the last argument
/// in an argument list.  This can be quite complex to use manually so this
/// type is added as a utility.  You can use [`get`](Self::get) to fetch a
/// single keyword argument and then use [`assert_all_used`](Self::assert_all_used)
/// to make sure extra arguments create an error.
///
/// Here an example of a function modifying values in different ways.
///
/// ```
/// use minijinja::value::{Value, Kwargs};
/// use minijinja::Error;
///
/// fn modify(mut values: Vec<Value>, options: Kwargs) -> Result<Vec<Value>, Error> {
///     // get pulls a parameter of any type.  Same as from_args.  For optional
///     // boolean values the type inference is particularly convenient.
///     if let Some(true) = options.get("reverse")? {
///         values.reverse();
///     }
///     if let Some(limit) = options.get("limit")? {
///         values.truncate(limit);
///     }
///     options.assert_all_used()?;
///     Ok(values)
/// }
/// ```
///
/// If for whatever reason you need a value again you can use [`Into`] to
/// convert it back into a [`Value`].  This is particularly useful when performing
/// calls into values.  To create a [`Kwargs`] object from scratch you can use
/// [`FromIterator`]:
///
/// ```
/// use minijinja::value::{Value, Kwargs};
/// let kwargs = Kwargs::from_iter([
///     ("foo", Value::from(true)),
///     ("bar", Value::from(42)),
/// ]);
/// let value = Value::from(kwargs);
/// assert!(value.is_kwargs());
/// ```
///
/// When working with [`Rest`] you can use [`from_args`] to split all arguments into
/// positional arguments and keyword arguments:
///
/// ```
/// # use minijinja::value::{Value, Rest, Kwargs, from_args};
/// # use minijinja::Error;
/// fn my_func(args: Rest<Value>) -> Result<Value, Error> {
///     let (args, kwargs) = from_args::<(&[Value], Kwargs)>(&args)?;
///     // do something with args and kwargs
/// # todo!()
/// }
/// ```
#[derive(Debug, Clone)]
pub struct Kwargs {
    pub(crate) values: Arc<KwargsValues>,
    used: RefCell<HashSet<String>>,
}

#[repr(transparent)]
#[derive(Default, Debug)]
pub(crate) struct KwargsValues(ValueMap);

impl Deref for KwargsValues {
    type Target = ValueMap;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl Object for KwargsValues {
    fn get_value(self: &Arc<Self>, key: &Value) -> Option<Value> {
        self.0.get(key).cloned()
    }

    fn enumerate(self: &Arc<Self>) -> Enumerator {
        self.mapped_enumerator(|this| Box::new(this.0.keys().cloned()))
    }

    fn enumerator_len(self: &Arc<Self>) -> Option<usize> {
        Some(self.0.len())
    }
}

impl<'a> ArgType<'a> for Kwargs {
    type Output = Self;

    fn from_value(value: Option<&'a Value>) -> Result<Self, Error> {
        match value {
            Some(value) => {
                Kwargs::extract(value).ok_or_else(|| Error::from(ErrorKind::MissingArgument))
            }
            None => Ok(Kwargs::new(Default::default())),
        }
    }

    fn from_state_and_values(
        _state: Option<&'a State>,
        values: &'a [Value],
        offset: usize,
    ) -> Result<(Self, usize), Error> {
        let args = values
            .get(offset)
            .and_then(Kwargs::extract)
            .map(|kwargs| (kwargs, 1))
            .unwrap_or_else(|| (Kwargs::new(Default::default()), 0));

        Ok(args)
    }

    fn is_trailing() -> bool {
        true
    }
}

impl Kwargs {
    fn new(map: Arc<KwargsValues>) -> Kwargs {
        Kwargs {
            values: map,
            used: RefCell::new(HashSet::new()),
        }
    }

    /// Given a value, extracts the kwargs if there are any.
    pub(crate) fn extract(value: &Value) -> Option<Kwargs> {
        value
            .as_object()
            .and_then(|x| x.downcast::<KwargsValues>())
            .map(Kwargs::new)
    }

    /// Wraps a value map into kwargs.
    pub(crate) fn wrap(map: ValueMap) -> Value {
        Value::from_object(KwargsValues(map))
    }

    /// Get a single argument from the kwargs but don't mark it as used.
    pub fn peek<'a, T>(&'a self, key: &'a str) -> Result<T, Error>
    where
        T: ArgType<'a, Output = T>,
    {
        T::from_value(self.values.get(&Value::from(key))).map_err(|mut err| {
            if err.kind() == ErrorKind::MissingArgument && err.detail().is_none() {
                err.set_detail(format!("missing keyword argument '{key}'"));
            }
            err
        })
    }

    /// Gets a single argument from the kwargs and marks it as used.
    ///
    /// This method works pretty much like [`from_args`] and marks any parameter
    /// used internally.  For optional arguments you would typically use
    /// `Option<T>` and for non optional ones directly `T`.
    ///
    /// Examples:
    ///
    /// ```
    /// # use minijinja::Error;
    /// # use minijinja::value::Kwargs; fn f(kwargs: Kwargs) -> Result<(), Error> {
    /// // f(int=42) -> Some(42)
    /// // f() -> None
    /// let optional_int: Option<u32> = kwargs.get("int")?;
    /// // f(int=42) -> 42
    /// // f() -> Error
    /// let required_int: u32 = kwargs.get("int")?;
    /// # Ok(()) }
    /// ```
    ///
    /// If you don't want to mark it as used, us [`peek`](Self::peek) instead.
    pub fn get<'a, T>(&'a self, key: &'a str) -> Result<T, Error>
    where
        T: ArgType<'a, Output = T>,
    {
        let rv = ok!(self.peek::<T>(key));
        self.used.borrow_mut().insert(key.to_string());
        Ok(rv)
    }

    /// Checks if a keyword argument exists.
    pub fn has(&self, key: &str) -> bool {
        self.values.contains_key(&Value::from(key))
    }

    /// Iterates over all passed keyword arguments.
    pub fn args(&self) -> impl Iterator<Item = &str> {
        self.values.iter().filter_map(|x| x.0.as_str())
    }

    /// Asserts that all kwargs were used.
    pub fn assert_all_used(&self) -> Result<(), Error> {
        let used = self.used.borrow();
        for key in self.values.keys() {
            if let Some(key) = key.as_str() {
                if !used.contains(key) {
                    return Err(Error::new(
                        ErrorKind::TooManyArguments,
                        format!("unknown keyword argument '{key}'"),
                    ));
                }
            } else {
                return Err(Error::new(
                    ErrorKind::InvalidOperation,
                    "non string keys passed to kwargs",
                ));
            }
        }
        Ok(())
    }
}

impl FromIterator<(String, Value)> for Kwargs {
    fn from_iter<T>(iter: T) -> Self
    where
        T: IntoIterator<Item = (String, Value)>,
    {
        Kwargs::new(Arc::new(KwargsValues(
            iter.into_iter().map(|(k, v)| (Value::from(k), v)).collect(),
        )))
    }
}

impl<'a> FromIterator<(&'a str, Value)> for Kwargs {
    fn from_iter<T>(iter: T) -> Self
    where
        T: IntoIterator<Item = (&'a str, Value)>,
    {
        Kwargs::new(Arc::new(KwargsValues(
            iter.into_iter().map(|(k, v)| (Value::from(k), v)).collect(),
        )))
    }
}

impl From<Kwargs> for Value {
    fn from(value: Kwargs) -> Self {
        Value::from_dyn_object(value.values)
    }
}

impl TryFrom<Value> for Kwargs {
    type Error = Error;

    fn try_from(value: Value) -> Result<Self, Self::Error> {
        match value.0 {
            ValueRepr::Undefined(_) => Ok(Kwargs::new(Default::default())),
            ValueRepr::Object(_) => {
                Kwargs::extract(&value).ok_or_else(|| Error::from(ErrorKind::InvalidOperation))
            }
            _ => Err(Error::from(ErrorKind::InvalidOperation)),
        }
    }
}

impl<'a> ArgType<'a> for Value {
    type Output = Self;

    fn from_value(value: Option<&'a Value>) -> Result<Self, Error> {
        match value {
            Some(value) => Ok(value.clone()),
            None => Err(Error::from(ErrorKind::MissingArgument)),
        }
    }

    fn from_value_owned(value: Value) -> Result<Self, Error> {
        Ok(value)
    }
}

impl<'a> ArgType<'a> for String {
    type Output = Self;

    fn from_value(value: Option<&'a Value>) -> Result<Self, Error> {
        match value {
            Some(value) => {
                if value.is_kwargs() {
                    return Err(Error::new(
                        ErrorKind::InvalidOperation,
                        "cannot convert kwargs to string",
                    ));
                }
                Ok(value.to_string())
            }
            None => Err(Error::from(ErrorKind::MissingArgument)),
        }
    }

    fn from_value_owned(value: Value) -> Result<Self, Error> {
        Ok(value.to_string())
    }
}

impl<'a, T: ArgType<'a, Output = T>> ArgType<'a> for Vec<T> {
    type Output = Vec<T>;

    fn from_value(value: Option<&'a Value>) -> Result<Self, Error> {
        match value {
            None => Ok(Vec::new()),
            Some(value) => {
                let iter = ok!(value
                    .as_object()
                    .filter(|x| matches!(x.repr(), ObjectRepr::Seq | ObjectRepr::Iterable))
                    .and_then(|x| x.try_iter())
                    .ok_or_else(|| { Error::new(ErrorKind::InvalidOperation, "not iterable") }));
                let mut rv = Vec::new();
                for value in iter {
                    rv.push(ok!(T::from_value_owned(value)));
                }
                Ok(rv)
            }
        }
    }

    fn from_value_owned(value: Value) -> Result<Self, Error> {
        let iter = ok!(value
            .as_object()
            .filter(|x| matches!(x.repr(), ObjectRepr::Seq | ObjectRepr::Iterable))
            .and_then(|x| x.try_iter())
            .ok_or_else(|| { Error::new(ErrorKind::InvalidOperation, "not iterable") }));
        let mut rv = Vec::new();
        for value in iter {
            rv.push(ok!(T::from_value_owned(value)));
        }
        Ok(rv)
    }
}

impl<'a> ArgType<'a> for DynObject {
    type Output = Self;

    fn from_value(value: Option<&'a Value>) -> Result<Self, Error> {
        value
            .ok_or_else(|| Error::from(ErrorKind::MissingArgument))
            .and_then(|v| Self::from_value_owned(v.clone()))
    }

    fn from_value_owned(value: Value) -> Result<Self, Error> {
        value
            .as_object()
            .cloned()
            .ok_or_else(|| Error::new(ErrorKind::InvalidOperation, "not an object"))
    }
}

impl From<Value> for String {
    fn from(val: Value) -> Self {
        val.to_string()
    }
}

impl From<usize> for Value {
    fn from(val: usize) -> Self {
        Value::from(val as u64)
    }
}

impl From<isize> for Value {
    fn from(val: isize) -> Self {
        Value::from(val as i64)
    }
}

impl<I: Into<Value>> From<Option<I>> for Value {
    fn from(value: Option<I>) -> Self {
        match value {
            Some(value) => value.into(),
            None => Value::from(()),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_as_f64() {
        let v = Value::from(42u32);
        let f: f64 = v.try_into().unwrap();
        assert_eq!(f, 42.0);
        let v = Value::from(42.5);
        let f: f64 = v.try_into().unwrap();
        assert_eq!(f, 42.5);
    }

    #[test]
    fn test_split_kwargs() {
        let args = [
            Value::from(42),
            Value::from(true),
            Value::from(Kwargs::from_iter([
                ("foo", Value::from(1)),
                ("bar", Value::from(2)),
            ])),
        ];
        let (args, kwargs) = from_args::<(&[Value], Kwargs)>(&args).unwrap();
        assert_eq!(args, &[Value::from(42), Value::from(true)]);
        assert_eq!(kwargs.get::<Value>("foo").unwrap(), Value::from(1));
        assert_eq!(kwargs.get::<Value>("bar").unwrap(), Value::from(2));
    }

    #[test]
    fn test_kwargs_fails_string_conversion() {
        let kwargs = Kwargs::from_iter([("foo", Value::from(1)), ("bar", Value::from(2))]);
        let args = [Value::from(kwargs)];

        let result = from_args::<(String,)>(&args);
        assert!(result.is_err());
        assert_eq!(
            result.unwrap_err().to_string(),
            "invalid operation: cannot convert kwargs to string"
        );

        let result = from_args::<(Cow<str>,)>(&args);
        assert!(result.is_err());
        assert_eq!(
            result.unwrap_err().to_string(),
            "invalid operation: cannot convert kwargs to string"
        );
    }

    #[test]
    fn test_optional_none() {
        let (one,) = from_args::<(Option<i32>,)>(args!(None::<i32>)).unwrap();
        assert!(one.is_none());
        let (one,) = from_args::<(Option<i32>,)>(args!(Some(Value::UNDEFINED))).unwrap();
        assert!(one.is_none());
    }
}

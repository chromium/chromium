use std::ops::{Deref, DerefMut};

use serde::de::value::{MapDeserializer, SeqDeserializer};
use serde::de::{
    self, Deserialize, DeserializeOwned, DeserializeSeed, Deserializer, EnumAccess,
    IntoDeserializer, MapAccess, SeqAccess, Unexpected, VariantAccess, Visitor,
};
use serde::forward_to_deserialize_any;

use crate::value::{ArgType, ObjectRepr, Value, ValueKind, ValueMap, ValueRepr};
use crate::{Error, ErrorKind};

#[cfg_attr(docsrs, doc(cfg(feature = "deserialization")))]
impl<'de> Deserialize<'de> for Value {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        deserializer.deserialize_any(ValueVisitor)
    }
}

struct ValueVisitor;

macro_rules! visit_value_primitive {
    ($name:ident, $ty:ty) => {
        fn $name<E: de::Error>(self, v: $ty) -> Result<Value, E> {
            Ok(Value::from(v))
        }
    };
}

impl<'de> Visitor<'de> for ValueVisitor {
    type Value = Value;

    fn expecting(&self, fmt: &mut std::fmt::Formatter) -> std::fmt::Result {
        fmt.write_str("any MiniJinja compatible value")
    }

    visit_value_primitive!(visit_bool, bool);
    visit_value_primitive!(visit_i8, i8);
    visit_value_primitive!(visit_i16, i16);
    visit_value_primitive!(visit_i32, i32);
    visit_value_primitive!(visit_i64, i64);
    visit_value_primitive!(visit_i128, i128);
    visit_value_primitive!(visit_u16, u16);
    visit_value_primitive!(visit_u32, u32);
    visit_value_primitive!(visit_u64, u64);
    visit_value_primitive!(visit_u128, u128);
    visit_value_primitive!(visit_f32, f32);
    visit_value_primitive!(visit_f64, f64);
    visit_value_primitive!(visit_char, char);
    visit_value_primitive!(visit_str, &str);
    visit_value_primitive!(visit_string, String);
    visit_value_primitive!(visit_bytes, &[u8]);
    visit_value_primitive!(visit_byte_buf, Vec<u8>);

    fn visit_none<E: de::Error>(self) -> Result<Value, E> {
        Ok(Value::from(()))
    }

    fn visit_some<D: Deserializer<'de>>(self, deserializer: D) -> Result<Value, D::Error> {
        Deserialize::deserialize(deserializer)
    }

    fn visit_unit<E: de::Error>(self) -> Result<Value, E> {
        Ok(Value::from(()))
    }

    fn visit_newtype_struct<D: Deserializer<'de>>(
        self,
        deserializer: D,
    ) -> Result<Value, D::Error> {
        Deserialize::deserialize(deserializer)
    }

    fn visit_seq<A: SeqAccess<'de>>(self, mut visitor: A) -> Result<Value, A::Error> {
        let mut rv = Vec::<Value>::new();
        while let Some(e) = ok!(visitor.next_element()) {
            rv.push(e);
        }
        Ok(Value::from(rv))
    }

    fn visit_map<A: MapAccess<'de>>(self, mut map: A) -> Result<Value, A::Error> {
        let mut rv = ValueMap::default();
        while let Some((k, v)) = ok!(map.next_entry()) {
            rv.insert(k, v);
        }
        Ok(Value::from_object(rv))
    }
}

/// Utility type to deserialize an argument.
///
/// This allows you to directly accept a type that implements [`Deserialize`] as an
/// argument to a filter or test.  The type dereferences into the inner type and
/// it also lets you move out the inner type.
///
/// ```rust
/// # use minijinja::Environment;
/// use std::path::PathBuf;
/// use minijinja::value::ViaDeserialize;
///
/// fn dirname(path: ViaDeserialize<PathBuf>) -> String {
///     match path.parent() {
///         Some(parent) => parent.display().to_string(),
///         None => "".to_string()
///     }
/// }
///
/// # let mut env = Environment::new();
/// env.add_filter("dirname", dirname);
/// ```
#[cfg_attr(docsrs, doc(cfg(feature = "deserialization")))]
pub struct ViaDeserialize<T: DeserializeOwned>(pub T);

impl<'a, T: DeserializeOwned> ArgType<'a> for ViaDeserialize<T> {
    type Output = Self;

    fn from_value(value: Option<&'a Value>) -> Result<Self, Error> {
        match value {
            Some(value) => {
                if value.is_kwargs() {
                    return Err(Error::new(
                        ErrorKind::InvalidOperation,
                        "cannot deserialize from kwargs",
                    ));
                }
                T::deserialize(value).map(ViaDeserialize)
            }
            None => Err(Error::from(ErrorKind::MissingArgument)),
        }
    }
}

impl<T: DeserializeOwned> Deref for ViaDeserialize<T> {
    type Target = T;

    fn deref(&self) -> &T {
        &self.0
    }
}

impl<T: DeserializeOwned> DerefMut for ViaDeserialize<T> {
    fn deref_mut(&mut self) -> &mut T {
        &mut self.0
    }
}

// this is a macro so that we don't accidentally diverge between
// the Value and &Value deserializer
macro_rules! common_forward {
    () => {
        forward_to_deserialize_any! {
            bool u8 u16 u32 u64 i8 i16 i32 i64 f32 f64 char str string unit
            seq bytes byte_buf map
            tuple_struct struct tuple ignored_any identifier
        }
    };
}

#[cfg_attr(docsrs, doc(cfg(feature = "deserialization")))]
impl IntoDeserializer<'_, Error> for Value {
    type Deserializer = Value;

    fn into_deserializer(self) -> Value {
        self
    }
}

#[cfg_attr(docsrs, doc(cfg(feature = "deserialization")))]
impl<'de> Deserializer<'de> for Value {
    type Error = Error;

    fn deserialize_any<V: Visitor<'de>>(self, visitor: V) -> Result<V::Value, Error> {
        match self.0 {
            ValueRepr::Invalid(ref error) => Err(de::Error::custom(error)),
            ValueRepr::Bool(v) => visitor.visit_bool(v),
            ValueRepr::U64(v) => visitor.visit_u64(v),
            ValueRepr::I64(v) => visitor.visit_i64(v),
            ValueRepr::I128(v) => visitor.visit_i128(v.0),
            ValueRepr::U128(v) => visitor.visit_u128(v.0),
            ValueRepr::F64(v) => visitor.visit_f64(v),
            ValueRepr::String(ref v, _) => visitor.visit_str(v),
            ValueRepr::SmallStr(v) => visitor.visit_str(v.as_str()),
            ValueRepr::Undefined(_) | ValueRepr::None => visitor.visit_unit(),
            ValueRepr::Bytes(ref v) => visitor.visit_bytes(v),
            ValueRepr::Object(o) => match o.repr() {
                ObjectRepr::Plain => Err(de::Error::custom("cannot deserialize plain objects")),
                ObjectRepr::Seq | ObjectRepr::Iterable => {
                    visitor.visit_seq(SeqDeserializer::new(o.try_iter().into_iter().flatten()))
                }
                ObjectRepr::Map => visitor.visit_map(MapDeserializer::new(
                    o.try_iter_pairs().into_iter().flatten(),
                )),
            },
        }
    }

    fn deserialize_option<V: Visitor<'de>>(self, visitor: V) -> Result<V::Value, Error> {
        match self.0 {
            ValueRepr::None | ValueRepr::Undefined(_) => visitor.visit_unit(),
            _ => visitor.visit_some(self),
        }
    }

    fn deserialize_enum<V: Visitor<'de>>(
        self,
        _name: &'static str,
        _variants: &'static [&'static str],
        visitor: V,
    ) -> Result<V::Value, Error> {
        let (variant, value) = match self.kind() {
            ValueKind::Map => {
                let mut iter = ok!(self.try_iter());
                let variant = match iter.next() {
                    Some(v) => v,
                    None => {
                        return Err(de::Error::invalid_value(
                            Unexpected::Map,
                            &"map with a single key",
                        ))
                    }
                };
                if iter.next().is_some() {
                    return Err(de::Error::invalid_value(
                        Unexpected::Map,
                        &"map with a single key",
                    ));
                }
                let val = self.get_item_opt(&variant);
                (variant, val)
            }
            ValueKind::String => (self, None),
            _ => {
                return Err(de::Error::invalid_type(
                    value_to_unexpected(&self),
                    &"string or map",
                ))
            }
        };

        visitor.visit_enum(EnumDeserializer { variant, value })
    }

    #[inline]
    fn deserialize_unit_struct<V: Visitor<'de>>(
        self,
        _name: &'static str,
        visitor: V,
    ) -> Result<V::Value, Error> {
        self.deserialize_unit(visitor)
    }

    #[inline]
    fn deserialize_newtype_struct<V: Visitor<'de>>(
        self,
        _name: &'static str,
        visitor: V,
    ) -> Result<V::Value, Error> {
        visitor.visit_newtype_struct(self)
    }

    common_forward!();
}

struct EnumDeserializer {
    variant: Value,
    value: Option<Value>,
}

impl<'de> EnumAccess<'de> for EnumDeserializer {
    type Error = Error;
    type Variant = VariantDeserializer;

    fn variant_seed<V: DeserializeSeed<'de>>(
        self,
        seed: V,
    ) -> Result<(V::Value, VariantDeserializer), Error> {
        seed.deserialize(self.variant)
            .map(|v| (v, VariantDeserializer { value: self.value }))
    }
}

struct VariantDeserializer {
    value: Option<Value>,
}

impl<'de> VariantAccess<'de> for VariantDeserializer {
    type Error = Error;

    fn unit_variant(self) -> Result<(), Error> {
        match self.value {
            Some(value) => Deserialize::deserialize(value),
            None => Ok(()),
        }
    }

    fn newtype_variant_seed<T: DeserializeSeed<'de>>(self, seed: T) -> Result<T::Value, Error> {
        match self.value {
            Some(value) => seed.deserialize(value),
            None => Err(de::Error::invalid_type(
                Unexpected::UnitVariant,
                &"newtype variant",
            )),
        }
    }

    fn tuple_variant<V: Visitor<'de>>(self, _len: usize, visitor: V) -> Result<V::Value, Error> {
        match self.value.as_ref().and_then(|x| x.as_object()) {
            Some(obj) if matches!(obj.repr(), ObjectRepr::Seq) => Deserializer::deserialize_any(
                SeqDeserializer::new(obj.try_iter().into_iter().flatten()),
                visitor,
            ),
            _ => Err(de::Error::invalid_type(
                self.value
                    .as_ref()
                    .map_or(Unexpected::Unit, value_to_unexpected),
                &"tuple variant",
            )),
        }
    }

    fn struct_variant<V: Visitor<'de>>(
        self,
        _fields: &'static [&'static str],
        visitor: V,
    ) -> Result<V::Value, Error> {
        match self.value.as_ref().map(|x| (x.kind(), x)) {
            Some((ValueKind::Map, val)) => Deserializer::deserialize_any(
                MapDeserializer::new(
                    ok!(val.try_iter())
                        .map(|ref k| (k.clone(), val.get_item(k).unwrap_or_default())),
                ),
                visitor,
            ),
            _ => Err(de::Error::invalid_type(
                self.value
                    .as_ref()
                    .map_or(Unexpected::Unit, value_to_unexpected),
                &"struct variant",
            )),
        }
    }
}

#[cfg_attr(docsrs, doc(cfg(feature = "deserialization")))]
impl<'de> Deserializer<'de> for &Value {
    type Error = Error;

    #[inline]
    fn deserialize_any<V: Visitor<'de>>(self, visitor: V) -> Result<V::Value, Error> {
        self.clone().deserialize_any(visitor)
    }

    #[inline]
    fn deserialize_option<V: Visitor<'de>>(self, visitor: V) -> Result<V::Value, Error> {
        self.clone().deserialize_option(visitor)
    }

    #[inline]
    fn deserialize_enum<V: Visitor<'de>>(
        self,
        name: &'static str,
        variants: &'static [&'static str],
        visitor: V,
    ) -> Result<V::Value, Error> {
        self.clone().deserialize_enum(name, variants, visitor)
    }

    #[inline]
    fn deserialize_unit_struct<V: Visitor<'de>>(
        self,
        name: &'static str,
        visitor: V,
    ) -> Result<V::Value, Error> {
        self.clone().deserialize_unit_struct(name, visitor)
    }

    #[inline]
    fn deserialize_newtype_struct<V: Visitor<'de>>(
        self,
        name: &'static str,
        visitor: V,
    ) -> Result<V::Value, Error> {
        self.clone().deserialize_newtype_struct(name, visitor)
    }

    common_forward!();
}

#[cfg_attr(docsrs, doc(cfg(feature = "deserialization")))]
impl<'de> IntoDeserializer<'de, Error> for &'de Value {
    type Deserializer = &'de Value;

    fn into_deserializer(self) -> &'de Value {
        self
    }
}

#[cfg_attr(docsrs, doc(cfg(feature = "deserialization")))]
impl de::Error for Error {
    fn custom<T: std::fmt::Display>(msg: T) -> Self {
        Error::new(ErrorKind::CannotDeserialize, msg.to_string())
    }
}

fn value_to_unexpected(value: &Value) -> Unexpected<'_> {
    match value.0 {
        ValueRepr::Undefined(_) | ValueRepr::None => Unexpected::Unit,
        ValueRepr::Bool(val) => Unexpected::Bool(val),
        ValueRepr::U64(val) => Unexpected::Unsigned(val),
        ValueRepr::I64(val) => Unexpected::Signed(val),
        ValueRepr::F64(val) => Unexpected::Float(val),
        ValueRepr::Invalid(_) => Unexpected::Other("<invalid value>"),
        ValueRepr::U128(val) => {
            let unsigned = val.0 as u64;
            if unsigned as u128 == val.0 {
                Unexpected::Unsigned(unsigned)
            } else {
                Unexpected::Other("u128")
            }
        }
        ValueRepr::I128(val) => {
            let signed = val.0 as i64;
            if signed as i128 == val.0 {
                Unexpected::Signed(signed)
            } else {
                Unexpected::Other("u128")
            }
        }
        ValueRepr::String(ref s, _) => Unexpected::Str(s),
        ValueRepr::SmallStr(ref s) => Unexpected::Str(s.as_str()),
        ValueRepr::Bytes(ref b) => Unexpected::Bytes(b),
        ValueRepr::Object(..) => Unexpected::Other("<dynamic value>"),
    }
}

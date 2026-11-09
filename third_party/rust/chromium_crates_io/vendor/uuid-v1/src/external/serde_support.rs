// Copyright 2013-2014 The Rust Project Developers.
// Copyright 2018 The Uuid Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use core::marker::PhantomData;

use crate::{
    error::*,
    fmt::{Braced, Hyphenated, Simple, Urn},
    non_nil::NonNilUuid,
    std::fmt,
    Bytes, Uuid,
};
use serde_core::{
    de::{self, Error as _},
    Deserialize, Deserializer, Serialize, Serializer,
};

impl Serialize for Uuid {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        if serializer.is_human_readable() {
            serializer.serialize_str(self.hyphenated().encode_lower(&mut Uuid::encode_buffer()))
        } else {
            serializer.serialize_bytes(self.as_bytes())
        }
    }
}

impl Serialize for NonNilUuid {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde_core::Serializer,
    {
        Uuid::from(*self).serialize(serializer)
    }
}

impl Serialize for Hyphenated {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        serializer.serialize_str(self.encode_lower(&mut Uuid::encode_buffer()))
    }
}

impl Serialize for Simple {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        serializer.serialize_str(self.encode_lower(&mut Uuid::encode_buffer()))
    }
}

impl Serialize for Urn {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        serializer.serialize_str(self.encode_lower(&mut Uuid::encode_buffer()))
    }
}

impl Serialize for Braced {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        serializer.serialize_str(self.encode_lower(&mut Uuid::encode_buffer()))
    }
}

struct UuidReadableVisitor<T> {
    expecting: &'static str,
    _marker: PhantomData<T>,
}

impl<'vi, T: UuidDeserialize> de::Visitor<'vi> for UuidReadableVisitor<T> {
    type Value = T;

    fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter.write_str(self.expecting)
    }

    fn visit_str<E: de::Error>(self, value: &str) -> Result<T, E> {
        T::from_str(value).map_err(de_error)
    }

    fn visit_bytes<E: de::Error>(self, value: &[u8]) -> Result<T, E> {
        T::from_slice(value).map_err(de_error)
    }

    fn visit_seq<A>(self, mut seq: A) -> Result<T, A::Error>
    where
        A: de::SeqAccess<'vi>,
    {
        #[rustfmt::skip]
        let bytes = [
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(0, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(1, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(2, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(3, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(4, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(5, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(6, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(7, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(8, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(9, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(10, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(11, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(12, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(13, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(14, &self)) },
            match seq.next_element()? { Some(e) => e, None => return Err(A::Error::invalid_length(15, &self)) },
        ];

        T::from_bytes(bytes).map_err(de_error)
    }
}

struct UuidBytesVisitor<T> {
    _marker: PhantomData<T>,
}

impl<'vi, T: UuidDeserialize> de::Visitor<'vi> for UuidBytesVisitor<T> {
    type Value = T;

    fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(formatter, "a 16 byte array")
    }

    fn visit_bytes<E: de::Error>(self, value: &[u8]) -> Result<T, E> {
        T::from_slice(value).map_err(de_error)
    }
}

fn de_error<E: de::Error>(e: Error) -> E {
    E::custom(format_args!("UUID parsing failed: {}", e))
}

trait UuidDeserialize {
    fn from_str(formatted: &str) -> Result<Self, Error>
    where
        Self: Sized;
    fn from_slice(bytes: &[u8]) -> Result<Self, Error>
    where
        Self: Sized;
    fn from_bytes(bytes: Bytes) -> Result<Self, Error>
    where
        Self: Sized;
}

impl UuidDeserialize for Uuid {
    fn from_str(formatted: &str) -> Result<Self, Error> {
        formatted.parse()
    }

    fn from_slice(bytes: &[u8]) -> Result<Self, Error> {
        Uuid::from_slice(bytes)
    }

    fn from_bytes(bytes: Bytes) -> Result<Self, Error> {
        Ok(Uuid::from_bytes(bytes))
    }
}

impl<'de> Deserialize<'de> for Uuid {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        if deserializer.is_human_readable() {
            deserializer.deserialize_str(UuidReadableVisitor {
                expecting: "a formatted UUID string",
                _marker: PhantomData::<Uuid>,
            })
        } else {
            deserializer.deserialize_bytes(UuidBytesVisitor {
                _marker: PhantomData::<Uuid>,
            })
        }
    }
}

impl UuidDeserialize for Braced {
    fn from_str(formatted: &str) -> Result<Self, Error> {
        formatted.parse()
    }

    fn from_slice(bytes: &[u8]) -> Result<Self, Error> {
        Ok(Uuid::from_slice(bytes)?.into())
    }

    fn from_bytes(bytes: Bytes) -> Result<Self, Error> {
        Ok(Uuid::from_bytes(bytes).into())
    }
}

impl<'de> Deserialize<'de> for Braced {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        deserializer.deserialize_str(UuidReadableVisitor {
            expecting: "a UUID string in the braced format",
            _marker: PhantomData::<Braced>,
        })
    }
}

impl UuidDeserialize for Hyphenated {
    fn from_str(formatted: &str) -> Result<Self, Error> {
        formatted.parse()
    }

    fn from_slice(bytes: &[u8]) -> Result<Self, Error> {
        Ok(Uuid::from_slice(bytes)?.into())
    }

    fn from_bytes(bytes: Bytes) -> Result<Self, Error> {
        Ok(Uuid::from_bytes(bytes).into())
    }
}

impl<'de> Deserialize<'de> for Hyphenated {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        deserializer.deserialize_str(UuidReadableVisitor {
            expecting: "a UUID string in the hyphenated format",
            _marker: PhantomData::<Hyphenated>,
        })
    }
}

impl UuidDeserialize for Simple {
    fn from_str(formatted: &str) -> Result<Self, Error> {
        formatted.parse()
    }

    fn from_slice(bytes: &[u8]) -> Result<Self, Error> {
        Ok(Uuid::from_slice(bytes)?.into())
    }

    fn from_bytes(bytes: Bytes) -> Result<Self, Error> {
        Ok(Uuid::from_bytes(bytes).into())
    }
}

impl<'de> Deserialize<'de> for Simple {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        deserializer.deserialize_str(UuidReadableVisitor {
            expecting: "a UUID string in the simple format",
            _marker: PhantomData::<Simple>,
        })
    }
}

impl UuidDeserialize for Urn {
    fn from_str(formatted: &str) -> Result<Self, Error> {
        formatted.parse()
    }

    fn from_slice(bytes: &[u8]) -> Result<Self, Error> {
        Ok(Uuid::from_slice(bytes)?.into())
    }

    fn from_bytes(bytes: Bytes) -> Result<Self, Error> {
        Ok(Uuid::from_bytes(bytes).into())
    }
}

impl<'de> Deserialize<'de> for Urn {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        deserializer.deserialize_str(UuidReadableVisitor {
            expecting: "a UUID string in the URN format",
            _marker: PhantomData::<Urn>,
        })
    }
}

impl<'de> Deserialize<'de> for NonNilUuid {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde_core::Deserializer<'de>,
    {
        let uuid = Uuid::deserialize(deserializer)?;

        NonNilUuid::try_from(uuid).map_err(|_| {
            de::Error::invalid_value(de::Unexpected::Other("nil UUID"), &"a non-nil UUID")
        })
    }
}

pub mod compact {
    //! Serialize a [`Uuid`] as a `[u8; 16]`.
    //!
    //! [`Uuid`]: ../../struct.Uuid.html

    /// Serialize from a [`Uuid`] as a `[u8; 16]`
    ///
    /// [`Uuid`]: ../../struct.Uuid.html
    pub fn serialize<S>(u: &crate::Uuid, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde_core::Serializer,
    {
        serde_core::Serialize::serialize(u.as_bytes(), serializer)
    }

    /// Deserialize a `[u8; 16]` as a [`Uuid`]
    ///
    /// [`Uuid`]: ../../struct.Uuid.html
    pub fn deserialize<'de, D>(deserializer: D) -> Result<crate::Uuid, D::Error>
    where
        D: serde_core::Deserializer<'de>,
    {
        let bytes: [u8; 16] = serde_core::Deserialize::deserialize(deserializer)?;

        Ok(crate::Uuid::from_bytes(bytes))
    }

    /// Serialize and deserialize an optional [`Uuid`](crate::Uuid) as an optional `[u8; 16]`.
    pub mod option {
        /// Serialize an optional [`Uuid`](crate::Uuid) as an optional `[u8; 16]`.
        pub fn serialize<S>(u: &Option<crate::Uuid>, serializer: S) -> Result<S::Ok, S::Error>
        where
            S: serde_core::Serializer,
        {
            match u {
                Some(u) => serializer.serialize_some(u.as_bytes()),
                None => serializer.serialize_none(),
            }
        }

        /// Deserialize an optional `[u8; 16]` as an optional [`Uuid`](crate::Uuid).
        pub fn deserialize<'de, D>(deserializer: D) -> Result<Option<crate::Uuid>, D::Error>
        where
            D: serde_core::Deserializer<'de>,
        {
            <Option<[u8; 16]> as serde_core::Deserialize>::deserialize(deserializer)
                .map(|bytes| bytes.map(crate::Uuid::from_bytes))
        }
    }

    #[cfg(test)]
    mod tests {
        use serde_derive::*;
        use serde_test::{Configure, Token};

        use crate::Uuid;

        const BYTES: [u8; 16] = [
            0xf9, 0x16, 0x8c, 0x5e, 0xce, 0xb2, 0x4f, 0xaa, 0xb6, 0xbf, 0x32, 0x9b, 0xf3, 0x9f,
            0xa1, 0xe4,
        ];

        #[test]
        fn test_compact_option() {
            #[derive(Debug, PartialEq, Serialize, Deserialize)]
            struct Field {
                #[serde(with = "crate::serde::compact::option")]
                id: Option<Uuid>,
            }

            let present = Field {
                id: Some(Uuid::from_bytes(BYTES)),
            };
            let absent = Field { id: None };
            let tokens = [
                Token::Struct {
                    name: "Field",
                    len: 1,
                },
                Token::Str("id"),
                Token::Some,
                Token::Tuple { len: 16 },
                Token::U8(BYTES[0]),
                Token::U8(BYTES[1]),
                Token::U8(BYTES[2]),
                Token::U8(BYTES[3]),
                Token::U8(BYTES[4]),
                Token::U8(BYTES[5]),
                Token::U8(BYTES[6]),
                Token::U8(BYTES[7]),
                Token::U8(BYTES[8]),
                Token::U8(BYTES[9]),
                Token::U8(BYTES[10]),
                Token::U8(BYTES[11]),
                Token::U8(BYTES[12]),
                Token::U8(BYTES[13]),
                Token::U8(BYTES[14]),
                Token::U8(BYTES[15]),
                Token::TupleEnd,
                Token::StructEnd,
            ];
            serde_test::assert_tokens(&present, &tokens);
            let none_tokens = [
                Token::Struct {
                    name: "Field",
                    len: 1,
                },
                Token::Str("id"),
                Token::None,
                Token::StructEnd,
            ];
            serde_test::assert_tokens(&absent, &none_tokens);
        }

        #[test]
        fn test_serialize_compact() {
            #[derive(Serialize, Debug, Deserialize, PartialEq)]
            struct UuidContainer {
                #[serde(with = "crate::serde::compact")]
                u: crate::Uuid,
            }

            let uuid_bytes = b"F9168C5E-CEB2-4F";
            let container = UuidContainer {
                u: crate::Uuid::from_slice(uuid_bytes).unwrap(),
            };

            // more complex because of the struct wrapping the actual UUID
            // serialization
            serde_test::assert_tokens(
                &container.compact(),
                &[
                    serde_test::Token::Struct {
                        name: "UuidContainer",
                        len: 1,
                    },
                    serde_test::Token::Str("u"),
                    serde_test::Token::Tuple { len: 16 },
                    serde_test::Token::U8(uuid_bytes[0]),
                    serde_test::Token::U8(uuid_bytes[1]),
                    serde_test::Token::U8(uuid_bytes[2]),
                    serde_test::Token::U8(uuid_bytes[3]),
                    serde_test::Token::U8(uuid_bytes[4]),
                    serde_test::Token::U8(uuid_bytes[5]),
                    serde_test::Token::U8(uuid_bytes[6]),
                    serde_test::Token::U8(uuid_bytes[7]),
                    serde_test::Token::U8(uuid_bytes[8]),
                    serde_test::Token::U8(uuid_bytes[9]),
                    serde_test::Token::U8(uuid_bytes[10]),
                    serde_test::Token::U8(uuid_bytes[11]),
                    serde_test::Token::U8(uuid_bytes[12]),
                    serde_test::Token::U8(uuid_bytes[13]),
                    serde_test::Token::U8(uuid_bytes[14]),
                    serde_test::Token::U8(uuid_bytes[15]),
                    serde_test::Token::TupleEnd,
                    serde_test::Token::StructEnd,
                ],
            )
        }
    }
}

/// Serialize a [`Uuid`] as a byte string.
///
/// Unlike [`compact`](crate::serde::compact), which serializes a [`Uuid`] as a
/// `[u8; 16]` array, this module uses the serializer's dedicated byte string
/// support through [`serialize_bytes`]. Formats that distinguish byte strings
/// from sequences, such as CBOR or MessagePack, can then encode the [`Uuid`] as
/// a single 16 byte string instead of a sequence of 16 individually encoded
/// integers.
///
/// [`Uuid`]: ../../struct.Uuid.html
/// [`serialize_bytes`]: serde_core::Serializer::serialize_bytes
///
/// ## Examples
///
/// ```
/// # use uuid::Uuid;
/// #[derive(serde_derive::Serialize, serde_derive::Deserialize)]
/// struct Container {
///     #[serde(with = "uuid::serde::bytes")]
///     id: Uuid,
/// }
/// ```
pub mod bytes {
    use super::*;

    struct ByteUuid(crate::Uuid);

    impl serde_core::Serialize for ByteUuid {
        fn serialize<S: serde_core::Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
            serializer.serialize_bytes(self.0.as_bytes())
        }
    }

    impl<'de> serde_core::Deserialize<'de> for ByteUuid {
        fn deserialize<D: serde_core::Deserializer<'de>>(
            deserializer: D,
        ) -> Result<Self, D::Error> {
            deserializer
                .deserialize_bytes(UuidBytesVisitor {
                    _marker: PhantomData::<crate::Uuid>,
                })
                .map(Self)
        }
    }

    /// Serialize from a [`Uuid`] as a byte string.
    ///
    /// [`Uuid`]: ../../struct.Uuid.html
    pub fn serialize<S>(u: &crate::Uuid, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde_core::Serializer,
    {
        ByteUuid(*u).serialize(serializer)
    }

    /// Deserialize a byte string as a [`Uuid`].
    ///
    /// [`Uuid`]: ../../struct.Uuid.html
    pub fn deserialize<'de, D>(deserializer: D) -> Result<crate::Uuid, D::Error>
    where
        D: serde_core::Deserializer<'de>,
    {
        ByteUuid::deserialize(deserializer).map(|u| u.0)
    }

    /// Serialize and deserialize an optional [`Uuid`](crate::Uuid) as an optional byte string.
    pub mod option {
        use super::*;

        /// Serialize an optional [`Uuid`](crate::Uuid) as an optional byte string.
        pub fn serialize<S>(u: &Option<crate::Uuid>, serializer: S) -> Result<S::Ok, S::Error>
        where
            S: serde_core::Serializer,
        {
            match u {
                Some(u) => serializer.serialize_some(&ByteUuid(*u)),
                None => serializer.serialize_none(),
            }
        }

        /// Deserialize an optional byte string as an optional [`Uuid`](crate::Uuid).
        pub fn deserialize<'de, D>(deserializer: D) -> Result<Option<crate::Uuid>, D::Error>
        where
            D: serde_core::Deserializer<'de>,
        {
            Option::<ByteUuid>::deserialize(deserializer).map(|u| u.map(|u| u.0))
        }
    }

    #[cfg(test)]
    mod tests {
        use serde_derive::*;

        use crate::Uuid;
        use serde_test::Token;

        const BYTES: [u8; 16] = [
            0xf9, 0x16, 0x8c, 0x5e, 0xce, 0xb2, 0x4f, 0xaa, 0xb6, 0xbf, 0x32, 0x9b, 0xf3, 0x9f,
            0xa1, 0xe4,
        ];

        #[test]
        fn test_bytes_option() {
            #[derive(Debug, PartialEq, Serialize, Deserialize)]
            struct Field {
                #[serde(with = "crate::serde::bytes::option")]
                id: Option<Uuid>,
            }

            let present = Field {
                id: Some(Uuid::from_bytes(BYTES)),
            };
            let absent = Field { id: None };
            let present_tokens = [
                Token::Struct {
                    name: "Field",
                    len: 1,
                },
                Token::Str("id"),
                Token::Some,
                Token::Bytes(&BYTES),
                Token::StructEnd,
            ];
            let none_tokens = [
                Token::Struct {
                    name: "Field",
                    len: 1,
                },
                Token::Str("id"),
                Token::None,
                Token::StructEnd,
            ];
            serde_test::assert_tokens(&present, &present_tokens);
            serde_test::assert_tokens(&absent, &none_tokens);
        }

        #[test]
        fn test_serialize_bytes() {
            #[derive(Serialize, Debug, Deserialize, PartialEq)]
            struct UuidContainer {
                #[serde(with = "crate::serde::bytes")]
                u: crate::Uuid,
            }

            let uuid_bytes = b"F9168C5E-CEB2-4F";
            let container = UuidContainer {
                u: crate::Uuid::from_slice(uuid_bytes).unwrap(),
            };

            serde_test::assert_tokens(
                &container,
                &[
                    serde_test::Token::Struct {
                        name: "UuidContainer",
                        len: 1,
                    },
                    serde_test::Token::Str("u"),
                    serde_test::Token::Bytes(uuid_bytes),
                    serde_test::Token::StructEnd,
                ],
            )
        }
    }
}

/// Serialize a [`Uuid`] as [`uuid::fmt::Simple`](crate::fmt::Simple).
///
/// [`Uuid`]: ../../struct.Uuid.html
///
/// ## Examples
///
/// Serialize and deserialize using the simple format, failing to deserialize
/// any other format:
///
/// ```
/// #[derive(serde_derive::Serialize, serde_derive::Deserialize)]
/// struct StructA {
///     #[serde(with = "uuid::serde::simple")]
///     id: uuid::Uuid,
/// }
/// ```
///
/// Serialize using the simple format, but deserialize any format:
///
/// ```
/// #[derive(serde_derive::Serialize, serde_derive::Deserialize)]
/// struct StructB {
///     #[serde(serialize_with = "uuid::serde::simple::serialize")]
///     id: uuid::Uuid,
/// }
/// ```
pub mod simple {
    use super::*;

    /// Serialize a [`Uuid`] as a simple string.
    ///
    /// [`Uuid`]: ../../struct.Uuid.html
    ///
    /// # Examples
    ///
    /// ```
    /// #[derive(serde_derive::Serialize)]
    /// struct Struct {
    ///     #[serde(serialize_with = "uuid::serde::simple::serialize")]
    ///     id: uuid::Uuid,
    /// }
    ///
    /// ```
    pub fn serialize<S>(u: &crate::Uuid, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde_core::Serializer,
    {
        serde_core::Serialize::serialize(u.as_simple(), serializer)
    }

    /// Deserialize a simple-formatted string as a [`Uuid`].
    ///
    /// [`Uuid`]: ../../struct.Uuid.html
    pub fn deserialize<'de, D>(deserializer: D) -> Result<Uuid, D::Error>
    where
        D: serde_core::Deserializer<'de>,
    {
        Ok(Simple::deserialize(deserializer)?.into())
    }

    /// Serialize and deserialize an optional [`Uuid`](crate::Uuid) in the simple format.
    pub mod option {
        use super::*;

        /// Serialize an optional [`Uuid`](crate::Uuid) in the simple format.
        pub fn serialize<S>(u: &Option<Uuid>, serializer: S) -> Result<S::Ok, S::Error>
        where
            S: serde_core::Serializer,
        {
            match u {
                Some(u) => serializer.serialize_some(u.as_simple()),
                None => serializer.serialize_none(),
            }
        }

        /// Deserialize an optional simple string as an optional [`Uuid`](crate::Uuid).
        pub fn deserialize<'de, D>(deserializer: D) -> Result<Option<Uuid>, D::Error>
        where
            D: serde_core::Deserializer<'de>,
        {
            Option::<Simple>::deserialize(deserializer).map(|u| u.map(Into::into))
        }
    }

    #[cfg(test)]
    mod tests {
        use serde::de::{self, Error};
        use serde_test::{Readable, Token};

        use super::*;

        const HYPHENATED_UUID_STR: &str = "f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4";
        const SIMPLE_UUID_STR: &str = "f9168c5eceb24faab6bf329bf39fa1e4";

        #[test]
        fn test_simple_option() {
            #[derive(Debug, PartialEq, serde_derive::Serialize, serde_derive::Deserialize)]
            struct Field {
                #[serde(with = "crate::serde::simple::option")]
                id: Option<Uuid>,
            }

            let present = Field {
                id: Some(HYPHENATED_UUID_STR.parse().unwrap()),
            };
            let absent = Field { id: None };
            let present_tokens = [
                Token::Struct {
                    name: "Field",
                    len: 1,
                },
                Token::Str("id"),
                Token::Some,
                Token::Str(SIMPLE_UUID_STR),
                Token::StructEnd,
            ];
            let absent_tokens = [
                Token::Struct {
                    name: "Field",
                    len: 1,
                },
                Token::Str("id"),
                Token::None,
                Token::StructEnd,
            ];

            serde_test::assert_tokens(&present, &present_tokens);
            serde_test::assert_tokens(&absent, &absent_tokens);
        }

        #[test]
        fn test_serialize_as_simple() {
            #[derive(serde_derive::Serialize)]
            struct Struct(#[serde(with = "super")] crate::Uuid);

            let u = Struct(Uuid::parse_str(HYPHENATED_UUID_STR).unwrap());
            serde_test::assert_ser_tokens(
                &u,
                &[
                    Token::NewtypeStruct { name: "Struct" },
                    Token::Str(SIMPLE_UUID_STR),
                ],
            );
        }

        #[test]
        fn test_de_from_simple() {
            #[derive(PartialEq, Debug, serde_derive::Deserialize)]
            struct Struct(#[serde(with = "super")] crate::Uuid);
            let s = Struct(HYPHENATED_UUID_STR.parse().unwrap());
            serde_test::assert_de_tokens::<Struct>(
                &s,
                &[
                    Token::TupleStruct {
                        name: "Struct",
                        len: 1,
                    },
                    Token::BorrowedStr(SIMPLE_UUID_STR),
                    Token::TupleStructEnd,
                ],
            );
        }

        #[test]
        fn test_de_reject_hyphenated() {
            #[derive(PartialEq, Debug, serde_derive::Deserialize)]
            struct Struct(#[serde(with = "super")] crate::Uuid);
            serde_test::assert_de_tokens_error::<Readable<Struct>>(
                &[
                    Token::TupleStruct {
                        name: "Struct",
                        len: 1,
                    },
                    Token::BorrowedStr(HYPHENATED_UUID_STR),
                    Token::TupleStructEnd,
                ],
                &format!(
                    "{}",
                    de::value::Error::custom(
                        "UUID parsing failed: invalid character: found `-` at 8"
                    )
                ),
            );
        }
    }
}

/// Serialize a [`Uuid`] as [`uuid::fmt::Braced`](crate::fmt::Braced).
///
/// [`Uuid`]: ../../struct.Uuid.html
///
/// ## Examples
///
/// Serialize and deserialize using the braced format, failing to deserialize
/// any other format:
///
/// ```
/// #[derive(serde_derive::Serialize, serde_derive::Deserialize)]
/// struct StructA {
///     #[serde(with = "uuid::serde::braced")]
///     id: uuid::Uuid,
/// }
/// ```
///
/// Serialize using the braced format, but deserialize any format:
///
/// ```
/// #[derive(serde_derive::Serialize, serde_derive::Deserialize)]
/// struct StructB {
///     #[serde(serialize_with = "uuid::serde::braced::serialize")]
///     id: uuid::Uuid,
/// }
/// ```
pub mod braced {
    use super::*;

    /// Serialize a [`Uuid`] as a braced string.
    ///
    /// [`Uuid`]: ../../struct.Uuid.html
    ///
    /// # Examples
    ///
    /// ```
    /// #[derive(serde_derive::Serialize)]
    /// struct Struct {
    ///     #[serde(serialize_with = "uuid::serde::braced::serialize")]
    ///     id: uuid::Uuid,
    /// }
    ///
    /// ```
    pub fn serialize<S>(u: &crate::Uuid, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde_core::Serializer,
    {
        serde_core::Serialize::serialize(u.as_braced(), serializer)
    }

    /// Deserialize a braced-formatted string as a [`Uuid`].
    ///
    /// [`Uuid`]: ../../struct.Uuid.html
    pub fn deserialize<'de, D>(deserializer: D) -> Result<crate::Uuid, D::Error>
    where
        D: serde_core::Deserializer<'de>,
    {
        Ok(Braced::deserialize(deserializer)?.into())
    }

    /// Serialize and deserialize an optional [`Uuid`](crate::Uuid) in the braced format.
    pub mod option {
        use super::*;

        /// Serialize an optional [`Uuid`](crate::Uuid) in the braced format.
        pub fn serialize<S>(u: &Option<Uuid>, serializer: S) -> Result<S::Ok, S::Error>
        where
            S: serde_core::Serializer,
        {
            match u {
                Some(u) => serializer.serialize_some(u.as_braced()),
                None => serializer.serialize_none(),
            }
        }

        /// Deserialize an optional braced string as an optional [`Uuid`](crate::Uuid).
        pub fn deserialize<'de, D>(deserializer: D) -> Result<Option<Uuid>, D::Error>
        where
            D: serde_core::Deserializer<'de>,
        {
            Option::<Braced>::deserialize(deserializer).map(|u| u.map(Into::into))
        }
    }

    #[cfg(test)]
    mod tests {

        use serde::de::{self, Error};
        use serde_test::{Readable, Token};

        use super::*;

        const HYPHENATED_UUID_STR: &str = "f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4";
        const BRACED_UUID_STR: &str = "{f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4}";

        #[test]
        fn test_braced_option() {
            #[derive(Debug, PartialEq, serde_derive::Serialize, serde_derive::Deserialize)]
            struct Field {
                #[serde(with = "crate::serde::braced::option")]
                id: Option<Uuid>,
            }

            let present = Field {
                id: Some(HYPHENATED_UUID_STR.parse().unwrap()),
            };
            let absent = Field { id: None };
            let present_tokens = [
                Token::Struct {
                    name: "Field",
                    len: 1,
                },
                Token::Str("id"),
                Token::Some,
                Token::Str(BRACED_UUID_STR),
                Token::StructEnd,
            ];
            let absent_tokens = [
                Token::Struct {
                    name: "Field",
                    len: 1,
                },
                Token::Str("id"),
                Token::None,
                Token::StructEnd,
            ];

            serde_test::assert_tokens(&present, &present_tokens);
            serde_test::assert_tokens(&absent, &absent_tokens);
        }

        #[test]
        fn test_serialize_as_braced() {
            #[derive(serde_derive::Serialize)]
            struct Struct(#[serde(with = "super")] crate::Uuid);

            let u = Struct(Uuid::parse_str(HYPHENATED_UUID_STR).unwrap());
            serde_test::assert_ser_tokens(
                &u,
                &[
                    Token::NewtypeStruct { name: "Struct" },
                    Token::Str(BRACED_UUID_STR),
                ],
            );
        }

        #[test]
        fn test_de_from_braced() {
            #[derive(PartialEq, Debug, serde_derive::Deserialize)]
            struct Struct(#[serde(with = "super")] crate::Uuid);
            let s = Struct(HYPHENATED_UUID_STR.parse().unwrap());
            serde_test::assert_de_tokens::<Struct>(
                &s,
                &[
                    Token::TupleStruct {
                        name: "Struct",
                        len: 1,
                    },
                    Token::BorrowedStr(BRACED_UUID_STR),
                    Token::TupleStructEnd,
                ],
            );
        }

        #[test]
        fn test_de_reject_hyphenated() {
            #[derive(PartialEq, Debug, serde_derive::Deserialize)]
            struct Struct(#[serde(with = "super")] crate::Uuid);
            serde_test::assert_de_tokens_error::<Readable<Struct>>(
                &[
                    Token::TupleStruct {
                        name: "Struct",
                        len: 1,
                    },
                    Token::BorrowedStr(HYPHENATED_UUID_STR),
                    Token::TupleStructEnd,
                ],
                &format!(
                    "{}",
                    de::value::Error::custom(
                        "UUID parsing failed: invalid character: found `f` at 0"
                    )
                ),
            );
        }
    }
}

/// Serialize a [`Uuid`] as [`uuid::fmt::Hyphenated`](crate::fmt::Hyphenated).
///
/// [`Uuid`]: ../../struct.Uuid.html
///
/// ## Examples
///
/// Serialize and deserialize using the hyphenated format, failing to deserialize
/// any other format:
///
/// ```
/// #[derive(serde_derive::Serialize, serde_derive::Deserialize)]
/// struct StructA {
///     #[serde(with = "uuid::serde::hyphenated")]
///     id: uuid::Uuid,
/// }
/// ```
///
/// Serialize using the hyphenated format, but deserialize any format:
///
/// ```
/// #[derive(serde_derive::Serialize, serde_derive::Deserialize)]
/// struct StructB {
///     #[serde(serialize_with = "uuid::serde::hyphenated::serialize")]
///     id: uuid::Uuid,
/// }
/// ```
pub mod hyphenated {

    use super::*;

    /// Serialize a [`Uuid`] as a hyphenated string.
    ///
    /// [`Uuid`]: ../../struct.Uuid.html
    ///
    /// # Examples
    ///
    /// ```
    /// #[derive(serde_derive::Serialize)]
    /// struct Struct {
    ///     #[serde(serialize_with = "uuid::serde::hyphenated::serialize")]
    ///     id: uuid::Uuid,
    /// }
    ///
    /// ```
    pub fn serialize<S>(u: &crate::Uuid, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde_core::Serializer,
    {
        serde_core::Serialize::serialize(u.as_hyphenated(), serializer)
    }

    /// Deserialize a hyphenated-formatted string as a [`Uuid`].
    ///
    /// [`Uuid`]: ../../struct.Uuid.html
    pub fn deserialize<'de, D>(deserializer: D) -> Result<crate::Uuid, D::Error>
    where
        D: serde_core::Deserializer<'de>,
    {
        Ok(Hyphenated::deserialize(deserializer)?.into())
    }

    /// Serialize and deserialize an optional [`Uuid`](crate::Uuid) in the hyphenated format.
    pub mod option {
        use super::*;

        /// Serialize an optional [`Uuid`](crate::Uuid) in the hyphenated format.
        pub fn serialize<S>(u: &Option<Uuid>, serializer: S) -> Result<S::Ok, S::Error>
        where
            S: serde_core::Serializer,
        {
            match u {
                Some(u) => serializer.serialize_some(u.as_hyphenated()),
                None => serializer.serialize_none(),
            }
        }

        /// Deserialize an optional hyphenated string as an optional [`Uuid`](crate::Uuid).
        pub fn deserialize<'de, D>(deserializer: D) -> Result<Option<Uuid>, D::Error>
        where
            D: serde_core::Deserializer<'de>,
        {
            Option::<Hyphenated>::deserialize(deserializer).map(|u| u.map(Into::into))
        }
    }

    #[cfg(test)]
    mod tests {

        use serde::de::{self, Error};
        use serde_test::{Readable, Token};

        use super::*;

        const HYPHENATED_UUID_STR: &str = "f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4";
        const BRACED_UUID_STR: &str = "{f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4}";

        #[test]
        fn test_hyphenated_option() {
            #[derive(Debug, PartialEq, serde_derive::Serialize, serde_derive::Deserialize)]
            struct Field {
                #[serde(with = "crate::serde::hyphenated::option")]
                id: Option<Uuid>,
            }

            let present = Field {
                id: Some(HYPHENATED_UUID_STR.parse().unwrap()),
            };
            let absent = Field { id: None };
            let present_tokens = [
                Token::Struct {
                    name: "Field",
                    len: 1,
                },
                Token::Str("id"),
                Token::Some,
                Token::Str(HYPHENATED_UUID_STR),
                Token::StructEnd,
            ];
            let absent_tokens = [
                Token::Struct {
                    name: "Field",
                    len: 1,
                },
                Token::Str("id"),
                Token::None,
                Token::StructEnd,
            ];

            serde_test::assert_tokens(&present, &present_tokens);
            serde_test::assert_tokens(&absent, &absent_tokens);
        }

        #[test]
        fn test_serialize_as_hyphenated() {
            #[derive(serde_derive::Serialize)]
            struct Struct(#[serde(with = "super")] crate::Uuid);

            let u = Struct(Uuid::parse_str(HYPHENATED_UUID_STR).unwrap());
            serde_test::assert_ser_tokens(
                &u,
                &[
                    Token::NewtypeStruct { name: "Struct" },
                    Token::Str(HYPHENATED_UUID_STR),
                ],
            );
        }

        #[test]
        fn test_de_from_hyphenated() {
            #[derive(PartialEq, Debug, serde_derive::Deserialize)]
            struct Struct(#[serde(with = "super")] crate::Uuid);
            let s = Struct(HYPHENATED_UUID_STR.parse().unwrap());
            serde_test::assert_de_tokens::<Struct>(
                &s,
                &[
                    Token::TupleStruct {
                        name: "Struct",
                        len: 1,
                    },
                    Token::BorrowedStr(HYPHENATED_UUID_STR),
                    Token::TupleStructEnd,
                ],
            );
        }

        #[test]
        fn test_de_reject_braced() {
            #[derive(PartialEq, Debug, serde_derive::Deserialize)]
            struct Struct(#[serde(with = "super")] crate::Uuid);
            serde_test::assert_de_tokens_error::<Readable<Struct>>(
                &[
                    Token::TupleStruct {
                        name: "Struct",
                        len: 1,
                    },
                    Token::BorrowedStr(BRACED_UUID_STR),
                    Token::TupleStructEnd,
                ],
                &format!(
                    "{}",
                    de::value::Error::custom(
                        "UUID parsing failed: invalid character: found `{` at 0"
                    )
                ),
            );
        }
    }
}

/// Serialize a [`Uuid`] as [`uuid::fmt::Urn`](crate::fmt::Urn).
///
/// [`Uuid`]: ../../struct.Uuid.html
///
/// ## Examples
///
/// Serialize and deserialize using the URN format, failing to deserialize
/// any other format:
///
/// ```
/// #[derive(serde_derive::Serialize, serde_derive::Deserialize)]
/// struct StructA {
///     #[serde(with = "uuid::serde::urn")]
///     id: uuid::Uuid,
/// }
/// ```
///
/// Serialize using the URN format, but deserialize any format:
///
/// ```
/// #[derive(serde_derive::Serialize, serde_derive::Deserialize)]
/// struct StructB {
///     #[serde(serialize_with = "uuid::serde::urn::serialize")]
///     id: uuid::Uuid,
/// }
/// ```
pub mod urn {
    use super::*;

    /// Serialize a [`Uuid`] as a URN string.
    ///
    /// [`Uuid`]: ../../struct.Uuid.html
    ///
    /// # Examples
    ///
    /// ```
    /// #[derive(serde_derive::Serialize)]
    /// struct Struct {
    ///     #[serde(serialize_with = "uuid::serde::urn::serialize")]
    ///     id: uuid::Uuid,
    /// }
    ///
    /// ```
    pub fn serialize<S>(u: &crate::Uuid, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde_core::Serializer,
    {
        serde_core::Serialize::serialize(u.as_urn(), serializer)
    }

    /// Deserialize a URN-formatted string as a [`Uuid`].
    ///
    /// [`Uuid`]: ../../struct.Uuid.html
    pub fn deserialize<'de, D>(deserializer: D) -> Result<crate::Uuid, D::Error>
    where
        D: serde_core::Deserializer<'de>,
    {
        Ok(Urn::deserialize(deserializer)?.into())
    }

    /// Serialize and deserialize an optional [`Uuid`](crate::Uuid) in the URN format.
    pub mod option {
        use super::*;

        /// Serialize an optional [`Uuid`](crate::Uuid) in the URN format.
        pub fn serialize<S>(u: &Option<Uuid>, serializer: S) -> Result<S::Ok, S::Error>
        where
            S: serde_core::Serializer,
        {
            match u {
                Some(u) => serializer.serialize_some(u.as_urn()),
                None => serializer.serialize_none(),
            }
        }

        /// Deserialize an optional URN string as an optional [`Uuid`](crate::Uuid).
        pub fn deserialize<'de, D>(deserializer: D) -> Result<Option<Uuid>, D::Error>
        where
            D: serde_core::Deserializer<'de>,
        {
            Option::<Urn>::deserialize(deserializer).map(|u| u.map(Into::into))
        }
    }

    #[cfg(test)]
    mod tests {
        use serde::de::{self, Error};
        use serde_test::{Readable, Token};

        use super::*;

        const HYPHENATED_UUID_STR: &str = "f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4";
        const URN_UUID_STR: &str = "urn:uuid:f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4";

        #[test]
        fn test_urn_option() {
            #[derive(Debug, PartialEq, serde_derive::Serialize, serde_derive::Deserialize)]
            struct Field {
                #[serde(with = "crate::serde::urn::option")]
                id: Option<Uuid>,
            }

            let present = Field {
                id: Some(HYPHENATED_UUID_STR.parse().unwrap()),
            };
            let absent = Field { id: None };
            let present_tokens = [
                Token::Struct {
                    name: "Field",
                    len: 1,
                },
                Token::Str("id"),
                Token::Some,
                Token::Str(URN_UUID_STR),
                Token::StructEnd,
            ];
            let absent_tokens = [
                Token::Struct {
                    name: "Field",
                    len: 1,
                },
                Token::Str("id"),
                Token::None,
                Token::StructEnd,
            ];

            serde_test::assert_tokens(&present, &present_tokens);
            serde_test::assert_tokens(&absent, &absent_tokens);
        }

        #[test]
        fn test_serialize_as_urn() {
            #[derive(serde_derive::Serialize)]
            struct Struct(#[serde(with = "super")] crate::Uuid);

            let u = Struct(Uuid::parse_str(HYPHENATED_UUID_STR).unwrap());
            serde_test::assert_ser_tokens(
                &u,
                &[
                    Token::NewtypeStruct { name: "Struct" },
                    Token::Str(URN_UUID_STR),
                ],
            );
        }

        #[test]
        fn test_de_from_urn() {
            #[derive(PartialEq, Debug, serde_derive::Deserialize)]
            struct Struct(#[serde(with = "super")] crate::Uuid);
            let s = Struct(HYPHENATED_UUID_STR.parse().unwrap());
            serde_test::assert_de_tokens::<Struct>(
                &s,
                &[
                    Token::TupleStruct {
                        name: "Struct",
                        len: 1,
                    },
                    Token::BorrowedStr(URN_UUID_STR),
                    Token::TupleStructEnd,
                ],
            );
        }

        #[test]
        fn test_de_reject_hyphenated() {
            #[derive(PartialEq, Debug, serde_derive::Deserialize)]
            struct Struct(#[serde(with = "super")] crate::Uuid);
            serde_test::assert_de_tokens_error::<Readable<Struct>>(
                &[
                    Token::TupleStruct {
                        name: "Struct",
                        len: 1,
                    },
                    Token::BorrowedStr(HYPHENATED_UUID_STR),
                    Token::TupleStructEnd,
                ],
                &format!(
                    "{}",
                    de::value::Error::custom(
                        "UUID parsing failed: invalid character: found `f` at 0"
                    )
                ),
            );
        }
    }
}

#[cfg(test)]
mod serde_tests {
    use super::*;

    use serde_test::{Compact, Configure, Readable, Token};

    #[test]
    fn test_serialize_readable_string() {
        let uuid_str = "f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4";
        let u = Uuid::parse_str(uuid_str).unwrap();
        serde_test::assert_tokens(&u.readable(), &[Token::Str(uuid_str)]);
    }

    #[test]
    fn test_deserialize_readable_compact() {
        let uuid_bytes = b"F9168C5E-CEB2-4F";
        let u = Uuid::from_slice(uuid_bytes).unwrap();

        serde_test::assert_de_tokens(
            &u.readable(),
            &[
                serde_test::Token::Tuple { len: 16 },
                serde_test::Token::U8(uuid_bytes[0]),
                serde_test::Token::U8(uuid_bytes[1]),
                serde_test::Token::U8(uuid_bytes[2]),
                serde_test::Token::U8(uuid_bytes[3]),
                serde_test::Token::U8(uuid_bytes[4]),
                serde_test::Token::U8(uuid_bytes[5]),
                serde_test::Token::U8(uuid_bytes[6]),
                serde_test::Token::U8(uuid_bytes[7]),
                serde_test::Token::U8(uuid_bytes[8]),
                serde_test::Token::U8(uuid_bytes[9]),
                serde_test::Token::U8(uuid_bytes[10]),
                serde_test::Token::U8(uuid_bytes[11]),
                serde_test::Token::U8(uuid_bytes[12]),
                serde_test::Token::U8(uuid_bytes[13]),
                serde_test::Token::U8(uuid_bytes[14]),
                serde_test::Token::U8(uuid_bytes[15]),
                serde_test::Token::TupleEnd,
            ],
        );
    }

    #[test]
    fn test_deserialize_readable_bytes() {
        let uuid_bytes = b"F9168C5E-CEB2-4F";
        let u = Uuid::from_slice(uuid_bytes).unwrap();

        serde_test::assert_de_tokens(&u.readable(), &[serde_test::Token::Bytes(uuid_bytes)]);
    }

    #[test]
    fn test_serialize_hyphenated() {
        let uuid_str = "f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4";
        let u = Uuid::parse_str(uuid_str).unwrap();
        serde_test::assert_ser_tokens(&u.hyphenated(), &[Token::Str(uuid_str)]);
        serde_test::assert_de_tokens(&u.hyphenated(), &[Token::Str(uuid_str)]);
    }

    #[test]
    fn test_serialize_simple() {
        let uuid_str = "f9168c5eceb24faab6bf329bf39fa1e4";
        let u = Uuid::parse_str(uuid_str).unwrap();
        serde_test::assert_ser_tokens(&u.simple(), &[Token::Str(uuid_str)]);
        serde_test::assert_de_tokens(&u.simple(), &[Token::Str(uuid_str)]);
    }

    #[test]
    fn test_serialize_urn() {
        let uuid_str = "urn:uuid:f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4";
        let u = Uuid::parse_str(uuid_str).unwrap();
        serde_test::assert_ser_tokens(&u.urn(), &[Token::Str(uuid_str)]);
        serde_test::assert_de_tokens(&u.urn(), &[Token::Str(uuid_str)]);
    }

    #[test]
    fn test_serialize_braced() {
        let uuid_str = "{f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4}";
        let u = Uuid::parse_str(uuid_str).unwrap();
        serde_test::assert_ser_tokens(&u.braced(), &[Token::Str(uuid_str)]);
        serde_test::assert_de_tokens(&u.braced(), &[Token::Str(uuid_str)]);
    }

    #[test]
    fn test_serialize_non_human_readable() {
        let uuid_bytes = b"F9168C5E-CEB2-4F";
        let u = Uuid::from_slice(uuid_bytes).unwrap();
        serde_test::assert_tokens(
            &u.compact(),
            &[serde_test::Token::Bytes(&[
                70, 57, 49, 54, 56, 67, 53, 69, 45, 67, 69, 66, 50, 45, 52, 70,
            ])],
        );
    }

    #[test]
    fn test_de_failure() {
        serde_test::assert_de_tokens_error::<Readable<Uuid>>(
            &[Token::Str("hello_world")],
            "UUID parsing failed: invalid character: found `h` at 0",
        );

        serde_test::assert_de_tokens_error::<Compact<Uuid>>(
            &[Token::Bytes(b"hello_world")],
            "UUID parsing failed: invalid length: expected 16 bytes, found 11",
        );
    }

    #[test]
    fn test_serde_non_nil_uuid() {
        let uuid_str = "f9168c5e-ceb2-4faa-b6bf-329bf39fa1e4";
        let uuid = Uuid::parse_str(uuid_str).unwrap();
        let non_nil_uuid = NonNilUuid::try_from(uuid).unwrap();

        serde_test::assert_ser_tokens(&non_nil_uuid.readable(), &[Token::Str(uuid_str)]);
        serde_test::assert_de_tokens(&non_nil_uuid.readable(), &[Token::Str(uuid_str)]);
    }
}

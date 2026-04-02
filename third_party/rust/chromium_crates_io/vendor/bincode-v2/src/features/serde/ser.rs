use super::EncodeError as SerdeEncodeError;
use crate::{
    config::Config,
    enc::{write::Writer, Encode, Encoder},
    error::EncodeError,
};
#[cfg(feature = "alloc")]
use alloc::vec::Vec;
use serde::ser::*;

/// Encode the given value into a `Vec<u8>` with the given `Config`. See the [config] module for more information.
///
/// [config]: ../config/index.html
#[cfg(feature = "alloc")]
#[cfg_attr(docsrs, doc(cfg(feature = "alloc")))]
pub fn encode_to_vec<E, C>(val: E, config: C) -> Result<Vec<u8>, EncodeError>
where
    E: Serialize,
    C: Config,
{
    let mut encoder = crate::enc::EncoderImpl::new(crate::VecWriter::default(), config);
    let serializer = SerdeEncoder { enc: &mut encoder };
    val.serialize(serializer)?;
    Ok(encoder.into_writer().collect())
}

/// Encode the given value into the given slice. Returns the amount of bytes that have been written.
///
/// See the [config] module for more information on configurations.
///
/// [config]: ../config/index.html
pub fn encode_into_slice<E, C>(val: E, dst: &mut [u8], config: C) -> Result<usize, EncodeError>
where
    E: Serialize,
    C: Config,
{
    let mut encoder =
        crate::enc::EncoderImpl::new(crate::enc::write::SliceWriter::new(dst), config);
    let serializer = SerdeEncoder { enc: &mut encoder };
    val.serialize(serializer)?;
    Ok(encoder.into_writer().bytes_written())
}

/// Encode the given value into a custom [Writer].
///
/// See the [config] module for more information on configurations.
///
/// [config]: ../config/index.html
pub fn encode_into_writer<E: Serialize, W: Writer, C: Config>(
    val: E,
    writer: W,
    config: C,
) -> Result<(), EncodeError> {
    let mut encoder = crate::enc::EncoderImpl::<_, C>::new(writer, config);
    let serializer = SerdeEncoder { enc: &mut encoder };
    val.serialize(serializer)?;
    Ok(())
}

/// Encode the given value into any type that implements `std::io::Write`, e.g. `std::fs::File`, with the given `Config`.
/// See the [config] module for more information.
///
/// [config]: ../config/index.html
#[cfg_attr(docsrs, doc(cfg(feature = "std")))]
#[cfg(feature = "std")]
pub fn encode_into_std_write<E: Serialize, C: Config, W: std::io::Write>(
    val: E,
    dst: &mut W,
    config: C,
) -> Result<usize, EncodeError> {
    let writer = crate::IoWriter::new(dst);
    let mut encoder = crate::enc::EncoderImpl::<_, C>::new(writer, config);
    let serializer = SerdeEncoder { enc: &mut encoder };
    val.serialize(serializer)?;
    Ok(encoder.into_writer().bytes_written())
}

pub(super) struct SerdeEncoder<'a, ENC: Encoder> {
    pub(super) enc: &'a mut ENC,
}

impl<ENC> Serializer for SerdeEncoder<'_, ENC>
where
    ENC: Encoder,
{
    type Ok = ();

    type Error = EncodeError;

    type SerializeSeq = Self;
    type SerializeTuple = Self;
    type SerializeTupleStruct = Self;
    type SerializeTupleVariant = Self;
    type SerializeMap = Self;
    type SerializeStruct = Self;
    type SerializeStructVariant = Self;

    fn serialize_bool(self, v: bool) -> Result<Self::Ok, Self::Error> {
        v.encode(self.enc)
    }

    fn serialize_i8(self, v: i8) -> Result<Self::Ok, Self::Error> {
        v.encode(self.enc)
    }

    fn serialize_i16(self, v: i16) -> Result<Self::Ok, Self::Error> {
        v.encode(self.enc)
    }

    fn serialize_i32(self, v: i32) -> Result<Self::Ok, Self::Error> {
        v.encode(self.enc)
    }

    fn serialize_i64(self, v: i64) -> Result<Self::Ok, Self::Error> {
        v.encode(self.enc)
    }

    serde::serde_if_integer128! {
        fn serialize_i128(self, v: i128) -> Result<Self::Ok, Self::Error> {
            v.encode(self.enc)
        }
    }

    fn serialize_u8(self, v: u8) -> Result<Self::Ok, Self::Error> {
        v.encode(self.enc)
    }

    fn serialize_u16(self, v: u16) -> Result<Self::Ok, Self::Error> {
        v.encode(self.enc)
    }

    fn serialize_u32(self, v: u32) -> Result<Self::Ok, Self::Error> {
        v.encode(self.enc)
    }

    fn serialize_u64(self, v: u64) -> Result<Self::Ok, Self::Error> {
        v.encode(self.enc)
    }

    serde::serde_if_integer128! {
        fn serialize_u128(self, v: u128) -> Result<Self::Ok, Self::Error> {
            v.encode(self.enc)
        }
    }

    fn serialize_f32(self, v: f32) -> Result<Self::Ok, Self::Error> {
        v.encode(self.enc)
    }

    fn serialize_f64(self, v: f64) -> Result<Self::Ok, Self::Error> {
        v.encode(self.enc)
    }

    fn serialize_char(self, v: char) -> Result<Self::Ok, Self::Error> {
        v.encode(self.enc)
    }

    fn serialize_str(self, v: &str) -> Result<Self::Ok, Self::Error> {
        v.encode(self.enc)
    }

    fn serialize_bytes(self, v: &[u8]) -> Result<Self::Ok, Self::Error> {
        v.encode(self.enc)
    }

    fn serialize_none(self) -> Result<Self::Ok, Self::Error> {
        0u8.encode(self.enc)
    }

    fn serialize_some<T>(mut self, value: &T) -> Result<Self::Ok, Self::Error>
    where
        T: Serialize + ?Sized,
    {
        1u8.encode(&mut self.enc)?;
        value.serialize(self)
    }

    fn serialize_unit(self) -> Result<Self::Ok, Self::Error> {
        Ok(())
    }

    fn serialize_unit_struct(self, _name: &'static str) -> Result<Self::Ok, Self::Error> {
        Ok(())
    }

    fn serialize_unit_variant(
        self,
        _name: &'static str,
        variant_index: u32,
        _variant: &'static str,
    ) -> Result<Self::Ok, Self::Error> {
        variant_index.encode(self.enc)
    }

    fn serialize_newtype_struct<T>(
        self,
        _name: &'static str,
        value: &T,
    ) -> Result<Self::Ok, Self::Error>
    where
        T: Serialize + ?Sized,
    {
        value.serialize(self)
    }

    fn serialize_newtype_variant<T>(
        mut self,
        _name: &'static str,
        variant_index: u32,
        _variant: &'static str,
        value: &T,
    ) -> Result<Self::Ok, Self::Error>
    where
        T: Serialize + ?Sized,
    {
        variant_index.encode(&mut self.enc)?;
        value.serialize(self)
    }

    fn serialize_seq(mut self, len: Option<usize>) -> Result<Self::SerializeSeq, Self::Error> {
        let len = len.ok_or_else(|| SerdeEncodeError::SequenceMustHaveLength.into())?;
        len.encode(&mut self.enc)?;
        Ok(Compound { enc: self.enc })
    }

    fn serialize_tuple(self, _: usize) -> Result<Self::SerializeTuple, Self::Error> {
        Ok(self)
    }

    fn serialize_tuple_struct(
        self,
        _name: &'static str,
        _len: usize,
    ) -> Result<Self::SerializeTupleStruct, Self::Error> {
        Ok(Compound { enc: self.enc })
    }

    fn serialize_tuple_variant(
        mut self,
        _name: &'static str,
        variant_index: u32,
        _variant: &'static str,
        _len: usize,
    ) -> Result<Self::SerializeTupleVariant, Self::Error> {
        variant_index.encode(&mut self.enc)?;
        Ok(Compound { enc: self.enc })
    }

    fn serialize_map(mut self, len: Option<usize>) -> Result<Self::SerializeMap, Self::Error> {
        let len = len.ok_or_else(|| SerdeEncodeError::SequenceMustHaveLength.into())?;
        len.encode(&mut self.enc)?;
        Ok(Compound { enc: self.enc })
    }

    fn serialize_struct(
        self,
        _name: &'static str,
        _len: usize,
    ) -> Result<Self::SerializeStruct, Self::Error> {
        Ok(Compound { enc: self.enc })
    }

    fn serialize_struct_variant(
        mut self,
        _name: &'static str,
        variant_index: u32,
        _variant: &'static str,
        _len: usize,
    ) -> Result<Self::SerializeStructVariant, Self::Error> {
        variant_index.encode(&mut self.enc)?;
        Ok(Compound { enc: self.enc })
    }

    #[cfg(not(feature = "alloc"))]
    fn collect_str<T>(self, _: &T) -> Result<Self::Ok, Self::Error>
    where
        T: core::fmt::Display + ?Sized,
    {
        Err(SerdeEncodeError::CannotCollectStr.into())
    }

    fn is_human_readable(&self) -> bool {
        false
    }
}

type Compound<'a, ENC> = SerdeEncoder<'a, ENC>;

impl<ENC: Encoder> SerializeSeq for Compound<'_, ENC> {
    type Ok = ();
    type Error = EncodeError;

    fn serialize_element<T>(&mut self, value: &T) -> Result<(), Self::Error>
    where
        T: Serialize + ?Sized,
    {
        value.serialize(SerdeEncoder { enc: self.enc })
    }

    fn end(self) -> Result<Self::Ok, Self::Error> {
        Ok(())
    }
}

impl<ENC: Encoder> SerializeTuple for Compound<'_, ENC> {
    type Ok = ();
    type Error = EncodeError;

    fn serialize_element<T>(&mut self, value: &T) -> Result<(), Self::Error>
    where
        T: Serialize + ?Sized,
    {
        value.serialize(SerdeEncoder { enc: self.enc })
    }

    fn end(self) -> Result<Self::Ok, Self::Error> {
        Ok(())
    }
}

impl<ENC: Encoder> SerializeTupleStruct for Compound<'_, ENC> {
    type Ok = ();
    type Error = EncodeError;

    fn serialize_field<T>(&mut self, value: &T) -> Result<(), Self::Error>
    where
        T: Serialize + ?Sized,
    {
        value.serialize(SerdeEncoder { enc: self.enc })
    }

    fn end(self) -> Result<Self::Ok, Self::Error> {
        Ok(())
    }
}

impl<ENC: Encoder> SerializeTupleVariant for Compound<'_, ENC> {
    type Ok = ();
    type Error = EncodeError;

    fn serialize_field<T>(&mut self, value: &T) -> Result<(), Self::Error>
    where
        T: Serialize + ?Sized,
    {
        value.serialize(SerdeEncoder { enc: self.enc })
    }

    fn end(self) -> Result<Self::Ok, Self::Error> {
        Ok(())
    }
}

impl<ENC: Encoder> SerializeMap for Compound<'_, ENC> {
    type Ok = ();
    type Error = EncodeError;

    fn serialize_key<T>(&mut self, key: &T) -> Result<(), Self::Error>
    where
        T: Serialize + ?Sized,
    {
        key.serialize(SerdeEncoder { enc: self.enc })
    }

    fn serialize_value<T>(&mut self, value: &T) -> Result<(), Self::Error>
    where
        T: Serialize + ?Sized,
    {
        value.serialize(SerdeEncoder { enc: self.enc })
    }

    fn end(self) -> Result<Self::Ok, Self::Error> {
        Ok(())
    }
}

impl<ENC: Encoder> SerializeStruct for Compound<'_, ENC> {
    type Ok = ();
    type Error = EncodeError;

    fn serialize_field<T>(&mut self, _key: &'static str, value: &T) -> Result<(), Self::Error>
    where
        T: Serialize + ?Sized,
    {
        value.serialize(SerdeEncoder { enc: self.enc })
    }

    fn end(self) -> Result<Self::Ok, Self::Error> {
        Ok(())
    }
}

impl<ENC: Encoder> SerializeStructVariant for Compound<'_, ENC> {
    type Ok = ();
    type Error = EncodeError;

    fn serialize_field<T>(&mut self, _key: &'static str, value: &T) -> Result<(), Self::Error>
    where
        T: Serialize + ?Sized,
    {
        value.serialize(SerdeEncoder { enc: self.enc })
    }

    fn end(self) -> Result<Self::Ok, Self::Error> {
        Ok(())
    }
}

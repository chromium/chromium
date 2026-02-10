use crate::{error::FendError, result::FResult};
use std::io;

pub(crate) trait Serialize {
	fn serialize(&self, write: &mut impl io::Write) -> FResult<()>;

	fn serialize_with_tag(&self, write: &mut impl io::Write, tag: u64) -> FResult<()> {
		serialize_int(tag, 0xc0, write)?;
		self.serialize(write)?;
		Ok(())
	}
}

pub(crate) trait Deserialize
where
	Self: Sized,
{
	fn deserialize(read: &mut impl io::Read) -> FResult<Self>;
}

#[allow(clippy::cast_possible_truncation)]
fn serialize_int(abs: u64, or: u8, write: &mut impl io::Write) -> FResult<()> {
	match abs {
		0..=0x17 => write.write_all(&[abs as u8 | or])?,
		0x18..=0xff => write.write_all(&[0x18 | or, abs as u8])?,
		0x100..=0xffff => write.write_all(&[0x19 | or, (abs >> 8) as u8, abs as u8])?,
		0x10000..=0xffff_ffff => write.write_all(&[
			0x1a | or,
			(abs >> 24) as u8,
			(abs >> 16) as u8,
			(abs >> 8) as u8,
			abs as u8,
		])?,
		0x1_0000_0000..=0xffff_ffff_ffff_ffff => write.write_all(&[
			0x1b | or,
			(abs >> 56) as u8,
			(abs >> 48) as u8,
			(abs >> 40) as u8,
			(abs >> 32) as u8,
			(abs >> 24) as u8,
			(abs >> 16) as u8,
			(abs >> 8) as u8,
			abs as u8,
		])?,
	}
	Ok(())
}

#[allow(dead_code)]
pub(crate) enum CborValue {
	Uint(u64),
	Negative(u64),
	Bytes(Vec<u8>),
	String(String),
	Array(Vec<Self>),
	Map(Vec<(Self, Self)>),
	Tag(u64, Box<Self>),
	Boolean(bool),
	Null,
	Undefined,
	F32(f32),
	F64(f64),
}

impl CborValue {
	pub(crate) fn as_int<T: TryFrom<u64> + TryFrom<i64>>(&self) -> FResult<T> {
		match self {
			Self::Uint(val) => (*val).try_into().map_err(|_| {
				FendError::DeserializationError("cbor uint is out of range of target type")
			}),
			Self::Negative(val) => (-i64::try_from(*val).map_err(|_| {
				FendError::DeserializationError("cbor negative int is out of range of i64")
			})? - 1)
				.try_into()
				.map_err(|_| {
					FendError::DeserializationError(
						"cbor negative int is out of range of target type",
					)
				}),
			_ => Err(FendError::DeserializationError(
				"cbor integer must have major type 0 or 1",
			)),
		}
	}
}

impl Serialize for CborValue {
	fn serialize(&self, write: &mut impl io::Write) -> FResult<()> {
		match self {
			Self::Uint(v) => v.serialize(write)?,
			Self::Negative(v) => serialize_int(*v, 0x20, write)?,
			Self::Bytes(v) => {
				v.serialize(write)?;
			}
			Self::String(v) => {
				v.serialize(write)?;
			}
			Self::Array(v) => {
				v.serialize(write)?;
			}
			Self::Map(kv) => {
				serialize_map(kv.len(), kv.iter(), write)?;
			}
			Self::Tag(tag, v) => v.serialize_with_tag(write, *tag)?,
			Self::Boolean(v) => v.serialize(write)?,
			Self::Null => write.write_all(&[0xf6])?,
			Self::Undefined => write.write_all(&[0xf7])?,
			Self::F32(v) => {
				write.write_all(&[0xfa])?;
				write.write_all(&v.to_be_bytes())?;
			}
			Self::F64(v) => {
				write.write_all(&[0xfb])?;
				write.write_all(&v.to_be_bytes())?;
			}
		}
		Ok(())
	}
}

impl Deserialize for CborValue {
	fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		let mut r = || -> FResult<u8> {
			let mut buf = [0];
			read.read_exact(&mut buf)?;
			Ok(buf[0])
		};
		let n = r()?;
		let mut read_payload = || {
			Ok(match n & 0x1f {
				0..=0x17 => (n & 0x1f).into(),
				0x18 => r()?.into(),
				0x19 => u16::from_be_bytes([r()?, r()?]).into(),
				0x1a => u32::from_be_bytes([r()?, r()?, r()?, r()?]).into(),
				0x1b => u64::from_be_bytes([r()?, r()?, r()?, r()?, r()?, r()?, r()?, r()?]),
				0x1c..=0x1f => {
					return Err(FendError::DeserializationError(
						"payload cannot be between 0x1c and 0x1f inclusive",
					));
				}
				_ => unreachable!(),
			})
		};
		Ok(match n & 0xe0 {
			0x00 => Self::Uint(read_payload()?),
			0x20 => Self::Negative(read_payload()?),
			0x40 | 0x60 => {
				let len = read_payload()?;
				let mut buf = vec![
					0;
					usize::try_from(len).map_err(|_| {
						FendError::DeserializationError(
							"string/byte array length cannot be converted to usize",
						)
					})?
				];
				read.read_exact(&mut buf)?;
				if n & 0xe0 == 0x40 {
					Self::Bytes(buf)
				} else {
					Self::String(String::from_utf8(buf).map_err(|_| {
						FendError::DeserializationError("string contains non-utf8 characters")
					})?)
				}
			}
			0x80 => {
				let len = read_payload()?;
				let len = usize::try_from(len).map_err(|_| {
					FendError::DeserializationError("array length cannot be converted to usize")
				})?;
				let mut arr = Vec::with_capacity(len);
				for _ in 0..len {
					arr.push(Self::deserialize(read)?);
				}
				Self::Array(arr)
			}
			0xa0 => {
				let len = read_payload()?;
				let len = usize::try_from(len).map_err(|_| {
					FendError::DeserializationError("map length cannot be converted to usize")
				})?;
				let mut kv = Vec::with_capacity(len);
				for _ in 0..len {
					kv.push((Self::deserialize(read)?, Self::deserialize(read)?));
				}
				Self::Map(kv)
			}
			0xc0 => Self::Tag(read_payload()?, Box::new(Self::deserialize(read)?)),
			0xe0 => match read_payload()? {
				0x14 => Self::Boolean(false),
				0x15 => Self::Boolean(true),
				0x16 => Self::Null,
				0x17 => Self::Undefined,
				_ => {
					return Err(FendError::DeserializationError(
						"major type 7 has an out-of-range payload",
					));
				}
			},
			_ => unreachable!(),
		})
	}
}

macro_rules! impl_serde {
	($($typ: ty)+) => {
		$(
			impl Serialize for $typ {
				#[allow(unused_comparisons, clippy::cast_sign_loss, clippy::cast_lossless)]
				fn serialize(&self, write: &mut impl io::Write) -> FResult<()> {
					Ok(if *self < 0 {
						serialize_int(!*self as u64, 0x20, write)?
					} else {
						serialize_int(*self as u64, 0, write)?
					})
				}
			}
			impl Deserialize for $typ {
				#[allow(clippy::cast_possible_truncation)]
				fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
					CborValue::deserialize(read)?.as_int()
				}
			}
		) +
	};
}

impl_serde!(u8 i32 u64 usize);

impl Serialize for [u8] {
	fn serialize(&self, write: &mut impl io::Write) -> FResult<()> {
		serialize_int(self.len() as u64, 0x40, write)?;
		write.write_all(self)?;
		Ok(())
	}
}

impl Deserialize for Vec<u8> {
	fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		let CborValue::Bytes(val) = CborValue::deserialize(read)? else {
			return Err(FendError::DeserializationError(
				"cbor value is not a byte array",
			));
		};
		Ok(val)
	}
}

impl Serialize for str {
	fn serialize(&self, write: &mut impl io::Write) -> FResult<()> {
		serialize_int(self.len() as u64, 0x60, write)?;
		write.write_all(self.as_bytes())?;
		Ok(())
	}
}

impl Deserialize for String {
	fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		let CborValue::String(val) = CborValue::deserialize(read)? else {
			return Err(FendError::DeserializationError(
				"cbor value is not a string",
			));
		};
		Ok(val)
	}
}

impl Serialize for bool {
	fn serialize(&self, write: &mut impl io::Write) -> FResult<()> {
		Ok(write.write_all(&[if *self { 0xf5 } else { 0xf4 }])?)
	}
}

impl Deserialize for bool {
	fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		let CborValue::Boolean(val) = CborValue::deserialize(read)? else {
			return Err(FendError::DeserializationError(
				"cbor value is not a boolean",
			));
		};
		Ok(val)
	}
}

impl Serialize for [CborValue] {
	fn serialize(&self, write: &mut impl io::Write) -> FResult<()> {
		serialize_int(self.len() as u64, 0x80, write)?;
		for v in self {
			v.serialize(write)?;
		}
		Ok(())
	}
}

fn serialize_map<'a>(
	len: usize,
	map: impl IntoIterator<Item = &'a (CborValue, CborValue)>,
	write: &mut impl io::Write,
) -> FResult<()> {
	serialize_int(len.try_into().unwrap(), 0xa0, write)?;
	for (k, v) in map {
		k.serialize(write)?;
		v.serialize(write)?;
	}
	Ok(())
}

#[cfg(test)]
mod tests {
	use std::io;

	use crate::serialize::{Deserialize, Serialize};

	#[track_caller]
	fn test(i: i128, bytes: &[u8]) {
		let mut buf = vec![];
		i32::try_from(i).unwrap().serialize(&mut buf).unwrap();
		assert_eq!(&buf, bytes);
		let mut cursor = io::Cursor::new(bytes);
		let res = i32::deserialize(&mut cursor).unwrap();
		assert_eq!(res, i32::try_from(i).unwrap());
	}

	#[test]
	fn cbor() {
		test(0, &[0x00]);
		test(16, &[0x10]);
		test(23, &[0x17]);
		test(24, &[0x18, 0x18]);
		test(426_937, &[0x1a, 0x00, 0x06, 0x83, 0xb9]);
		test(-1, &[0x20]);
		test(-24, &[0x37]);
		test(-426_937, &[0x3a, 0x00, 0x06, 0x83, 0xb8]);
	}

	#[test]
	fn cbor_string() {
		let mut buf = vec![];
		"hello world".serialize(&mut buf).unwrap();
		assert_eq!(
			&buf,
			&[
				0x6b, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x77, 0x6f, 0x72, 0x6c, 0x64
			]
		);
		let mut cursor = io::Cursor::new(buf);
		let s = String::deserialize(&mut cursor).unwrap();
		assert_eq!(s, "hello world");
	}
}

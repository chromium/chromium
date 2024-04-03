use crate::{error::FendError, result::FResult};
use std::io;

pub(crate) trait Serialize
where
	Self: Sized,
{
	fn serialize(&self, write: &mut impl io::Write) -> FResult<()>;
}

pub(crate) trait Deserialize
where
	Self: Sized,
{
	fn deserialize(read: &mut impl io::Read) -> FResult<Self>;
}

macro_rules! impl_serde {
	($($typ: ty)+) => {
		$(
			impl Serialize for $typ {
				fn serialize(&self, write: &mut impl io::Write) -> FResult<()> {
					Ok(write.write_all(&self.to_be_bytes())?)
				}
			}
			impl Deserialize for $typ {
				fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
					let mut buf = [0; std::mem::size_of::<$typ>()];
					read.read_exact(&mut buf[..])?;
					Ok(<$typ>::from_be_bytes(buf))
				}
			}
		) +
	};
}

impl_serde!(u8 i32 u64);

impl Serialize for usize {
	fn serialize(&self, write: &mut impl io::Write) -> FResult<()> {
		(*self as u64).serialize(write)
	}
}

impl Deserialize for usize {
	fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		let val = u64::deserialize(read)?;
		Self::try_from(val).map_err(|_| FendError::DeserializationError)
	}
}

impl Serialize for &str {
	fn serialize(&self, write: &mut impl io::Write) -> FResult<()> {
		self.len().serialize(write)?;
		self.as_bytes()
			.iter()
			.try_for_each(|&bit| bit.serialize(write))?;
		Ok(())
	}
}

impl Deserialize for String {
	fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		let len = usize::deserialize(read)?;
		let mut buf = Vec::with_capacity(len);
		for _ in 0..len {
			buf.push(u8::deserialize(read)?);
		}
		match Self::from_utf8(buf) {
			Ok(string) => Ok(string),
			Err(_) => Err(FendError::DeserializationError),
		}
	}
}

impl Serialize for bool {
	fn serialize(&self, write: &mut impl io::Write) -> FResult<()> {
		Ok(write.write_all(&[u8::from(*self)])?)
	}
}

impl Deserialize for bool {
	fn deserialize(read: &mut impl io::Read) -> FResult<Self> {
		let mut buf = [0; 1];
		read.read_exact(&mut buf[..])?;
		match buf[0] {
			0 => Ok(false),
			1 => Ok(true),
			_ => Err(FendError::DeserializationError),
		}
	}
}

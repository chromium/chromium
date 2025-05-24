#[cfg(debug_assertions)]
use std::convert::TryInto;

// TODO remove this, use try_into wher as_* is used

pub trait Truncate {
    fn truncate_as_u8(self) -> u8;
}

impl Truncate for u16 {
    #[allow(clippy::cast_possible_truncation)]
    fn truncate_as_u8(self) -> u8 {
        (self & 0xff) as u8
    }
}

pub trait As {
    fn as_u16(self) -> u16;
    fn as_i16(self) -> i16;
    fn as_usize(self) -> usize;
}

macro_rules! impl_as {
    ($ty:ty) => {
        #[cfg(debug_assertions)]
        impl As for $ty {
            fn as_u16(self) -> u16 {
                self.try_into().unwrap()
            }

            fn as_i16(self) -> i16 {
                self.try_into().unwrap()
            }

            fn as_usize(self) -> usize {
                self.try_into().unwrap()
            }
        }

        #[cfg(not(debug_assertions))]
        impl As for $ty {
            fn as_u16(self) -> u16 {
                self as u16
            }
            fn as_i16(self) -> i16 {
                self as i16
            }
            fn as_usize(self) -> usize {
                self as usize
            }
        }
    };
}

impl_as!(i16);
impl_as!(u32);
impl_as!(usize);

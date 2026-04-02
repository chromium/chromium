use crate::prelude::*;
use ::quickcheck::{Arbitrary, Gen};

impl<T, const BITS: usize> Arbitrary for UInt<T, BITS>
where
    T: Arbitrary + UnsignedInteger + BuiltinInteger,
    Self: UnsignedInteger<UnderlyingType = T>,
{
    fn arbitrary(g: &mut Gen) -> Self {
        loop {
            match Self::try_new(T::arbitrary(g)) {
                Ok(value) => break value,
                Err(_) => continue,
            }
        }
    }
}

impl<T, const BITS: usize> Arbitrary for Int<T, BITS>
where
    T: Arbitrary + SignedInteger + BuiltinInteger,
    Self: SignedInteger<UnderlyingType = T>,
{
    fn arbitrary(g: &mut Gen) -> Self {
        loop {
            match Self::try_new(T::arbitrary(g)) {
                Ok(value) => break value,
                Err(_) => continue,
            }
        }
    }
}

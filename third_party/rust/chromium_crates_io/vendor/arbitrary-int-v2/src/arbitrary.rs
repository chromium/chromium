use crate::prelude::*;
use arbitrary::{unstructured, Arbitrary, Result, Unstructured};

impl<'a, T: UnsignedInteger + BuiltinInteger, const BITS: usize> Arbitrary<'a> for UInt<T, BITS>
where
    T: unstructured::Int,
    Self: UnsignedInteger<UnderlyingType = T>,
{
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self> {
        u.int_in_range(Self::MIN.value()..=Self::MAX.value())
            .map(Self::new)
    }
}

impl<'a, T: SignedInteger + BuiltinInteger, const BITS: usize> Arbitrary<'a> for Int<T, BITS>
where
    T: unstructured::Int,
    Self: SignedInteger<UnderlyingType = T>,
{
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self> {
        u.int_in_range(Self::MIN.value()..=Self::MAX.value())
            .map(Self::new)
    }
}

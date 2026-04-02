#![cfg(all(feature = "alloc", feature = "derive"))]

use bincode::{
    config, de::BorrowDecoder, decode_from_slice, decode_from_slice_with_context, encode_to_vec,
    error::DecodeError, BorrowDecode, Decode, Encode,
};
use bumpalo::{collections::Vec, vec, Bump};

#[derive(PartialEq, Eq, Debug)]
struct CodableVec<'bump, T: 'bump>(Vec<'bump, T>);

impl<'bump, T: Encode> Encode for CodableVec<'bump, T> {
    fn encode<E: bincode::enc::Encoder>(
        &self,
        encoder: &mut E,
    ) -> Result<(), bincode::error::EncodeError> {
        self.0.as_slice().encode(encoder)
    }
}

impl<'bump, T: Decode<&'bump Bump>> Decode<&'bump Bump> for CodableVec<'bump, T> {
    fn decode<D: bincode::de::Decoder<Context = &'bump Bump>>(
        decoder: &mut D,
    ) -> Result<Self, bincode::error::DecodeError> {
        let len = u64::decode(decoder)?;
        let len = usize::try_from(len).map_err(|_| DecodeError::OutsideUsizeRange(len))?;
        decoder.claim_container_read::<T>(len)?;
        let mut vec = Vec::with_capacity_in(len, decoder.context());
        for _ in 0..len {
            decoder.unclaim_bytes_read(core::mem::size_of::<T>());
            vec.push(T::decode(decoder)?);
        }
        Ok(Self(vec))
    }
}

impl<'de, 'bump, T: BorrowDecode<'de, &'bump Bump>> BorrowDecode<'de, &'bump Bump>
    for CodableVec<'bump, T>
{
    fn borrow_decode<D: BorrowDecoder<'de, Context = &'bump Bump>>(
        decoder: &mut D,
    ) -> Result<Self, DecodeError> {
        let len = u64::decode(decoder)?;
        let len = usize::try_from(len).map_err(|_| DecodeError::OutsideUsizeRange(len))?;

        decoder.claim_container_read::<T>(len)?;

        let mut vec = Vec::with_capacity_in(len, decoder.context());
        for _ in 0..len {
            // See the documentation on `unclaim_bytes_read` as to why we're doing this here
            decoder.unclaim_bytes_read(core::mem::size_of::<T>());

            vec.push(T::borrow_decode(decoder)?);
        }
        Ok(Self(vec))
    }
}

#[derive(Encode, Decode, PartialEq, Eq, Debug)]
#[bincode(decode_context = "&'bump Bump")]
struct Container<'bump> {
    vec: CodableVec<'bump, u32>,
}

#[derive(Encode, Decode, PartialEq, Eq, Debug)]
#[bincode(decode_context = "&'bump Bump")]
enum _EnumContainer<'bump> {
    Vec(CodableVec<'bump, u32>),
}

#[ouroboros::self_referencing]
struct SelfReferencing {
    bump: Bump,
    #[borrows(bump)]
    #[not_covariant]
    container: Container<'this>,
}

impl<Context> Decode<Context> for SelfReferencing {
    fn decode<D: bincode::de::Decoder<Context = Context>>(
        decoder: &mut D,
    ) -> Result<Self, DecodeError> {
        SelfReferencing::try_new(Bump::new(), |bump| {
            Container::decode(&mut decoder.with_context(bump))
        })
    }
}

#[test]
fn decode_with_context() {
    let config = config::standard();
    let bump = Bump::new();
    let container = Container {
        vec: CodableVec(vec![in &bump; 1, 2, 3]),
    };

    let bytes = encode_to_vec(&container, config).unwrap();
    let (decoded_container, _) =
        decode_from_slice_with_context::<_, Container, _>(&bytes, config, &bump).unwrap();

    assert_eq!(container, decoded_container);

    let self_referencing: SelfReferencing = decode_from_slice(&bytes, config).unwrap().0;
    self_referencing.with_container(|c| assert_eq!(&container, c))
}

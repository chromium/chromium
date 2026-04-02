use crate::{
    de::{read::Reader, BorrowDecoder, Decode, Decoder},
    enc::{
        self,
        write::{SizeWriter, Writer},
        Encode, Encoder,
    },
    error::{DecodeError, EncodeError},
    impl_borrow_decode, BorrowDecode, Config,
};
use alloc::{
    borrow::{Cow, ToOwned},
    boxed::Box,
    collections::*,
    rc::Rc,
    string::String,
    vec::Vec,
};

#[cfg(target_has_atomic = "ptr")]
use alloc::sync::Arc;

#[derive(Default)]
pub(crate) struct VecWriter {
    inner: Vec<u8>,
}

impl VecWriter {
    /// Create a new vec writer with the given capacity
    pub fn with_capacity(cap: usize) -> Self {
        Self {
            inner: Vec::with_capacity(cap),
        }
    }
    // May not be used in all feature combinations
    #[allow(dead_code)]
    pub(crate) fn collect(self) -> Vec<u8> {
        self.inner
    }
}

impl enc::write::Writer for VecWriter {
    #[inline(always)]
    fn write(&mut self, bytes: &[u8]) -> Result<(), EncodeError> {
        self.inner.extend_from_slice(bytes);
        Ok(())
    }
}

/// Encode the given value into a `Vec<u8>` with the given `Config`. See the [config] module for more information.
///
/// [config]: config/index.html
#[cfg_attr(docsrs, doc(cfg(feature = "alloc")))]
pub fn encode_to_vec<E: enc::Encode, C: Config>(val: E, config: C) -> Result<Vec<u8>, EncodeError> {
    let size = {
        let mut size_writer = enc::EncoderImpl::<_, C>::new(SizeWriter::default(), config);
        val.encode(&mut size_writer)?;
        size_writer.into_writer().bytes_written
    };
    let writer = VecWriter::with_capacity(size);
    let mut encoder = enc::EncoderImpl::<_, C>::new(writer, config);
    val.encode(&mut encoder)?;
    Ok(encoder.into_writer().inner)
}

impl<Context, T> Decode<Context> for BinaryHeap<T>
where
    T: Decode<Context> + Ord,
{
    fn decode<D: Decoder<Context = Context>>(decoder: &mut D) -> Result<Self, DecodeError> {
        Ok(Vec::<T>::decode(decoder)?.into())
    }
}
impl<'de, T, Context> BorrowDecode<'de, Context> for BinaryHeap<T>
where
    T: BorrowDecode<'de, Context> + Ord,
{
    fn borrow_decode<D: BorrowDecoder<'de, Context = Context>>(
        decoder: &mut D,
    ) -> Result<Self, DecodeError> {
        Ok(Vec::<T>::borrow_decode(decoder)?.into())
    }
}

impl<T> Encode for BinaryHeap<T>
where
    T: Encode + Ord,
{
    fn encode<E: Encoder>(&self, encoder: &mut E) -> Result<(), EncodeError> {
        // BLOCKEDTODO(https://github.com/rust-lang/rust/issues/83659): we can u8 optimize this with `.as_slice()`
        crate::enc::encode_slice_len(encoder, self.len())?;
        for val in self.iter() {
            val.encode(encoder)?;
        }
        Ok(())
    }
}

impl<Context, K, V> Decode<Context> for BTreeMap<K, V>
where
    K: Decode<Context> + Ord,
    V: Decode<Context>,
{
    fn decode<D: Decoder<Context = Context>>(decoder: &mut D) -> Result<Self, DecodeError> {
        let len = crate::de::decode_slice_len(decoder)?;
        decoder.claim_container_read::<(K, V)>(len)?;

        let mut map = BTreeMap::new();
        for _ in 0..len {
            // See the documentation on `unclaim_bytes_read` as to why we're doing this here
            decoder.unclaim_bytes_read(core::mem::size_of::<(K, V)>());

            let key = K::decode(decoder)?;
            let value = V::decode(decoder)?;
            map.insert(key, value);
        }
        Ok(map)
    }
}
impl<'de, K, V, Context> BorrowDecode<'de, Context> for BTreeMap<K, V>
where
    K: BorrowDecode<'de, Context> + Ord,
    V: BorrowDecode<'de, Context>,
{
    fn borrow_decode<D: BorrowDecoder<'de, Context = Context>>(
        decoder: &mut D,
    ) -> Result<Self, DecodeError> {
        let len = crate::de::decode_slice_len(decoder)?;
        decoder.claim_container_read::<(K, V)>(len)?;

        let mut map = BTreeMap::new();
        for _ in 0..len {
            // See the documentation on `unclaim_bytes_read` as to why we're doing this here
            decoder.unclaim_bytes_read(core::mem::size_of::<(K, V)>());

            let key = K::borrow_decode(decoder)?;
            let value = V::borrow_decode(decoder)?;
            map.insert(key, value);
        }
        Ok(map)
    }
}

impl<K, V> Encode for BTreeMap<K, V>
where
    K: Encode + Ord,
    V: Encode,
{
    fn encode<E: Encoder>(&self, encoder: &mut E) -> Result<(), EncodeError> {
        crate::enc::encode_slice_len(encoder, self.len())?;
        for (key, val) in self.iter() {
            key.encode(encoder)?;
            val.encode(encoder)?;
        }
        Ok(())
    }
}

impl<Context, T> Decode<Context> for BTreeSet<T>
where
    T: Decode<Context> + Ord,
{
    fn decode<D: Decoder<Context = Context>>(decoder: &mut D) -> Result<Self, DecodeError> {
        let len = crate::de::decode_slice_len(decoder)?;
        decoder.claim_container_read::<T>(len)?;

        let mut map = BTreeSet::new();
        for _ in 0..len {
            // See the documentation on `unclaim_bytes_read` as to why we're doing this here
            decoder.unclaim_bytes_read(core::mem::size_of::<T>());

            let key = T::decode(decoder)?;
            map.insert(key);
        }
        Ok(map)
    }
}
impl<'de, T, Context> BorrowDecode<'de, Context> for BTreeSet<T>
where
    T: BorrowDecode<'de, Context> + Ord,
{
    fn borrow_decode<D: BorrowDecoder<'de, Context = Context>>(
        decoder: &mut D,
    ) -> Result<Self, DecodeError> {
        let len = crate::de::decode_slice_len(decoder)?;
        decoder.claim_container_read::<T>(len)?;

        let mut map = BTreeSet::new();
        for _ in 0..len {
            // See the documentation on `unclaim_bytes_read` as to why we're doing this here
            decoder.unclaim_bytes_read(core::mem::size_of::<T>());

            let key = T::borrow_decode(decoder)?;
            map.insert(key);
        }
        Ok(map)
    }
}

impl<T> Encode for BTreeSet<T>
where
    T: Encode + Ord,
{
    fn encode<E: Encoder>(&self, encoder: &mut E) -> Result<(), EncodeError> {
        crate::enc::encode_slice_len(encoder, self.len())?;
        for item in self.iter() {
            item.encode(encoder)?;
        }
        Ok(())
    }
}

impl<Context, T> Decode<Context> for VecDeque<T>
where
    T: Decode<Context>,
{
    fn decode<D: Decoder<Context = Context>>(decoder: &mut D) -> Result<Self, DecodeError> {
        Ok(Vec::<T>::decode(decoder)?.into())
    }
}
impl<'de, T, Context> BorrowDecode<'de, Context> for VecDeque<T>
where
    T: BorrowDecode<'de, Context>,
{
    fn borrow_decode<D: BorrowDecoder<'de, Context = Context>>(
        decoder: &mut D,
    ) -> Result<Self, DecodeError> {
        Ok(Vec::<T>::borrow_decode(decoder)?.into())
    }
}

impl<T> Encode for VecDeque<T>
where
    T: Encode,
{
    fn encode<E: Encoder>(&self, encoder: &mut E) -> Result<(), EncodeError> {
        crate::enc::encode_slice_len(encoder, self.len())?;
        if unty::type_equal::<T, u8>() {
            let slices: (&[T], &[T]) = self.as_slices();
            // Safety: T is u8 so turning this into `&[u8]` is okay
            let slices: (&[u8], &[u8]) = unsafe {
                (
                    core::slice::from_raw_parts(slices.0.as_ptr().cast(), slices.0.len()),
                    core::slice::from_raw_parts(slices.1.as_ptr().cast(), slices.1.len()),
                )
            };

            encoder.writer().write(slices.0)?;
            encoder.writer().write(slices.1)?;
        } else {
            for item in self.iter() {
                item.encode(encoder)?;
            }
        }
        Ok(())
    }
}

impl<Context, T> Decode<Context> for Vec<T>
where
    T: Decode<Context>,
{
    fn decode<D: Decoder<Context = Context>>(decoder: &mut D) -> Result<Self, DecodeError> {
        let len = crate::de::decode_slice_len(decoder)?;

        if unty::type_equal::<T, u8>() {
            decoder.claim_container_read::<T>(len)?;
            // optimize for reading u8 vecs
            let mut vec = alloc::vec![0u8; len];
            decoder.reader().read(&mut vec)?;
            // Safety: Vec<T> is Vec<u8>
            Ok(unsafe { core::mem::transmute::<Vec<u8>, Vec<T>>(vec) })
        } else {
            decoder.claim_container_read::<T>(len)?;

            let mut vec = Vec::with_capacity(len);
            for _ in 0..len {
                // See the documentation on `unclaim_bytes_read` as to why we're doing this here
                decoder.unclaim_bytes_read(core::mem::size_of::<T>());

                vec.push(T::decode(decoder)?);
            }
            Ok(vec)
        }
    }
}

impl<'de, T, Context> BorrowDecode<'de, Context> for Vec<T>
where
    T: BorrowDecode<'de, Context>,
{
    fn borrow_decode<D: BorrowDecoder<'de, Context = Context>>(
        decoder: &mut D,
    ) -> Result<Self, DecodeError> {
        let len = crate::de::decode_slice_len(decoder)?;

        if unty::type_equal::<T, u8>() {
            decoder.claim_container_read::<T>(len)?;
            // optimize for reading u8 vecs
            let mut vec = alloc::vec![0u8; len];
            decoder.reader().read(&mut vec)?;
            // Safety: Vec<T> is Vec<u8>
            Ok(unsafe { core::mem::transmute::<Vec<u8>, Vec<T>>(vec) })
        } else {
            decoder.claim_container_read::<T>(len)?;

            let mut vec = Vec::with_capacity(len);
            for _ in 0..len {
                // See the documentation on `unclaim_bytes_read` as to why we're doing this here
                decoder.unclaim_bytes_read(core::mem::size_of::<T>());

                vec.push(T::borrow_decode(decoder)?);
            }
            Ok(vec)
        }
    }
}

impl<T> Encode for Vec<T>
where
    T: Encode,
{
    fn encode<E: Encoder>(&self, encoder: &mut E) -> Result<(), EncodeError> {
        crate::enc::encode_slice_len(encoder, self.len())?;
        if unty::type_equal::<T, u8>() {
            // Safety: T == u8
            let slice: &[u8] = unsafe { core::mem::transmute(self.as_slice()) };
            encoder.writer().write(slice)?;
            Ok(())
        } else {
            for item in self.iter() {
                item.encode(encoder)?;
            }
            Ok(())
        }
    }
}

impl<Context> Decode<Context> for String {
    fn decode<D: Decoder<Context = Context>>(decoder: &mut D) -> Result<Self, DecodeError> {
        let bytes = Vec::<u8>::decode(decoder)?;
        String::from_utf8(bytes).map_err(|e| DecodeError::Utf8 {
            inner: e.utf8_error(),
        })
    }
}
impl_borrow_decode!(String);

impl<Context> Decode<Context> for Box<str> {
    fn decode<D: Decoder>(decoder: &mut D) -> Result<Self, DecodeError> {
        String::decode(decoder).map(String::into_boxed_str)
    }
}
impl_borrow_decode!(Box<str>);

impl Encode for String {
    fn encode<E: Encoder>(&self, encoder: &mut E) -> Result<(), EncodeError> {
        self.as_bytes().encode(encoder)
    }
}

impl<Context, T> Decode<Context> for Box<T>
where
    T: Decode<Context>,
{
    fn decode<D: Decoder<Context = Context>>(decoder: &mut D) -> Result<Self, DecodeError> {
        let t = T::decode(decoder)?;
        Ok(Box::new(t))
    }
}
impl<'de, T, Context> BorrowDecode<'de, Context> for Box<T>
where
    T: BorrowDecode<'de, Context>,
{
    fn borrow_decode<D: BorrowDecoder<'de, Context = Context>>(
        decoder: &mut D,
    ) -> Result<Self, DecodeError> {
        let t = T::borrow_decode(decoder)?;
        Ok(Box::new(t))
    }
}

impl<T> Encode for Box<T>
where
    T: Encode + ?Sized,
{
    fn encode<E: Encoder>(&self, encoder: &mut E) -> Result<(), EncodeError> {
        T::encode(self, encoder)
    }
}

impl<Context, T> Decode<Context> for Box<[T]>
where
    T: Decode<Context> + 'static,
{
    fn decode<D: Decoder<Context = Context>>(decoder: &mut D) -> Result<Self, DecodeError> {
        let vec = Vec::decode(decoder)?;
        Ok(vec.into_boxed_slice())
    }
}

impl<'de, T, Context> BorrowDecode<'de, Context> for Box<[T]>
where
    T: BorrowDecode<'de, Context> + 'de,
{
    fn borrow_decode<D: BorrowDecoder<'de, Context = Context>>(
        decoder: &mut D,
    ) -> Result<Self, DecodeError> {
        let vec = Vec::borrow_decode(decoder)?;
        Ok(vec.into_boxed_slice())
    }
}

impl<Context, T> Decode<Context> for Cow<'_, T>
where
    T: ToOwned + ?Sized,
    <T as ToOwned>::Owned: Decode<Context>,
{
    fn decode<D: Decoder<Context = Context>>(decoder: &mut D) -> Result<Self, DecodeError> {
        let t = <T as ToOwned>::Owned::decode(decoder)?;
        Ok(Cow::Owned(t))
    }
}
impl<'cow, T, Context> BorrowDecode<'cow, Context> for Cow<'cow, T>
where
    T: ToOwned + ?Sized,
    &'cow T: BorrowDecode<'cow, Context>,
{
    fn borrow_decode<D: BorrowDecoder<'cow, Context = Context>>(
        decoder: &mut D,
    ) -> Result<Self, DecodeError> {
        let t = <&T>::borrow_decode(decoder)?;
        Ok(Cow::Borrowed(t))
    }
}

impl<T> Encode for Cow<'_, T>
where
    T: ToOwned + ?Sized,
    for<'a> &'a T: Encode,
{
    fn encode<E: Encoder>(&self, encoder: &mut E) -> Result<(), EncodeError> {
        self.as_ref().encode(encoder)
    }
}

#[test]
fn test_cow_round_trip() {
    let start = Cow::Borrowed("Foo");
    let encoded = crate::encode_to_vec(&start, crate::config::standard()).unwrap();
    let (end, _) =
        crate::borrow_decode_from_slice::<Cow<str>, _>(&encoded, crate::config::standard())
            .unwrap();
    assert_eq!(start, end);
    let (end, _) =
        crate::decode_from_slice::<Cow<str>, _>(&encoded, crate::config::standard()).unwrap();
    assert_eq!(start, end);
}

impl<Context, T> Decode<Context> for Rc<T>
where
    T: Decode<Context>,
{
    fn decode<D: Decoder<Context = Context>>(decoder: &mut D) -> Result<Self, DecodeError> {
        let t = T::decode(decoder)?;
        Ok(Rc::new(t))
    }
}

impl<Context> Decode<Context> for Rc<str> {
    fn decode<D: Decoder<Context = Context>>(decoder: &mut D) -> Result<Self, DecodeError> {
        let decoded = String::decode(decoder)?;
        Ok(decoded.into())
    }
}

impl<'de, T, Context> BorrowDecode<'de, Context> for Rc<T>
where
    T: BorrowDecode<'de, Context>,
{
    fn borrow_decode<D: BorrowDecoder<'de, Context = Context>>(
        decoder: &mut D,
    ) -> Result<Self, DecodeError> {
        let t = T::borrow_decode(decoder)?;
        Ok(Rc::new(t))
    }
}

impl<'de, Context> BorrowDecode<'de, Context> for Rc<str> {
    fn borrow_decode<D: BorrowDecoder<'de, Context = Context>>(
        decoder: &mut D,
    ) -> Result<Self, DecodeError> {
        let decoded = String::decode(decoder)?;
        Ok(decoded.into())
    }
}

impl<T> Encode for Rc<T>
where
    T: Encode + ?Sized,
{
    fn encode<E: Encoder>(&self, encoder: &mut E) -> Result<(), EncodeError> {
        T::encode(self, encoder)
    }
}

impl<Context, T> Decode<Context> for Rc<[T]>
where
    T: Decode<Context> + 'static,
{
    fn decode<D: Decoder<Context = Context>>(decoder: &mut D) -> Result<Self, DecodeError> {
        let vec = Vec::decode(decoder)?;
        Ok(vec.into())
    }
}

impl<'de, T, Context> BorrowDecode<'de, Context> for Rc<[T]>
where
    T: BorrowDecode<'de, Context> + 'de,
{
    fn borrow_decode<D: BorrowDecoder<'de, Context = Context>>(
        decoder: &mut D,
    ) -> Result<Self, DecodeError> {
        let vec = Vec::borrow_decode(decoder)?;
        Ok(vec.into())
    }
}

#[cfg(target_has_atomic = "ptr")]
impl<Context, T> Decode<Context> for Arc<T>
where
    T: Decode<Context>,
{
    fn decode<D: Decoder<Context = Context>>(decoder: &mut D) -> Result<Self, DecodeError> {
        let t = T::decode(decoder)?;
        Ok(Arc::new(t))
    }
}

#[cfg(target_has_atomic = "ptr")]
impl<Context> Decode<Context> for Arc<str> {
    fn decode<D: Decoder<Context = Context>>(decoder: &mut D) -> Result<Self, DecodeError> {
        let decoded = String::decode(decoder)?;
        Ok(decoded.into())
    }
}

#[cfg(target_has_atomic = "ptr")]
impl<'de, T, Context> BorrowDecode<'de, Context> for Arc<T>
where
    T: BorrowDecode<'de, Context>,
{
    fn borrow_decode<D: BorrowDecoder<'de, Context = Context>>(
        decoder: &mut D,
    ) -> Result<Self, DecodeError> {
        let t = T::borrow_decode(decoder)?;
        Ok(Arc::new(t))
    }
}

#[cfg(target_has_atomic = "ptr")]
impl<'de, Context> BorrowDecode<'de, Context> for Arc<str> {
    fn borrow_decode<D: BorrowDecoder<'de, Context = Context>>(
        decoder: &mut D,
    ) -> Result<Self, DecodeError> {
        let decoded = String::decode(decoder)?;
        Ok(decoded.into())
    }
}

#[cfg(target_has_atomic = "ptr")]
impl<T> Encode for Arc<T>
where
    T: Encode + ?Sized,
{
    fn encode<E: Encoder>(&self, encoder: &mut E) -> Result<(), EncodeError> {
        T::encode(self, encoder)
    }
}

#[cfg(target_has_atomic = "ptr")]
impl<Context, T> Decode<Context> for Arc<[T]>
where
    T: Decode<Context> + 'static,
{
    fn decode<D: Decoder<Context = Context>>(decoder: &mut D) -> Result<Self, DecodeError> {
        let vec = Vec::decode(decoder)?;
        Ok(vec.into())
    }
}

#[cfg(target_has_atomic = "ptr")]
impl<'de, T, Context> BorrowDecode<'de, Context> for Arc<[T]>
where
    T: BorrowDecode<'de, Context> + 'de,
{
    fn borrow_decode<D: BorrowDecoder<'de, Context = Context>>(
        decoder: &mut D,
    ) -> Result<Self, DecodeError> {
        let vec = Vec::borrow_decode(decoder)?;
        Ok(vec.into())
    }
}

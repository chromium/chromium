//! Decoder-based structs and traits.

mod decoder;
mod impl_core;
mod impl_tuples;
mod impls;

use self::{
    decoder::WithContext,
    read::{BorrowReader, Reader},
};
use crate::{
    config::{Config, InternalLimitConfig},
    error::DecodeError,
    utils::Sealed,
};

pub mod read;

pub use self::decoder::DecoderImpl;

/// Trait that makes a type able to be decoded, akin to serde's `DeserializeOwned` trait.
///
/// Some types may require specific contexts. For example, to decode arena-based collections, an arena allocator must be provided as a context. In these cases, the context type `Context` should be specified or bounded.
///
/// This trait should be implemented for types which do not have references to data in the reader. For types that contain e.g. `&str` and `&[u8]`, implement [BorrowDecode] instead.
///
/// Whenever you derive `Decode` for your type, the base trait `BorrowDecode` is automatically implemented.
///
/// This trait will be automatically implemented with unbounded `Context` if you enable the `derive` feature and add `#[derive(bincode::Decode)]` to your type. Note that if the type contains any lifetimes, `BorrowDecode` will be implemented instead.
///
/// # Implementing this trait manually
///
/// If you want to implement this trait for your type, the easiest way is to add a `#[derive(bincode::Decode)]`, build and check your `target/generated/bincode/` folder. This should generate a `<Struct name>_Decode.rs` file.
///
/// For this struct:
///
/// ```
/// struct Entity {
///     pub x: f32,
///     pub y: f32,
/// }
/// ```
///
/// It will look something like:
///
/// ```
/// # struct Entity {
/// #     pub x: f32,
/// #     pub y: f32,
/// # }
/// impl<Context> bincode::Decode<Context> for Entity {
///     fn decode<D: bincode::de::Decoder<Context = Context>>(
///         decoder: &mut D,
///     ) -> core::result::Result<Self, bincode::error::DecodeError> {
///         Ok(Self {
///             x: bincode::Decode::decode(decoder)?,
///             y: bincode::Decode::decode(decoder)?,
///         })
///     }
/// }
/// impl<'de, Context> bincode::BorrowDecode<'de, Context> for Entity {
///     fn borrow_decode<D: bincode::de::BorrowDecoder<'de, Context = Context>>(
///         decoder: &mut D,
///     ) -> core::result::Result<Self, bincode::error::DecodeError> {
///         Ok(Self {
///             x: bincode::BorrowDecode::borrow_decode(decoder)?,
///             y: bincode::BorrowDecode::borrow_decode(decoder)?,
///         })
///     }
/// }
/// ```
///
/// From here you can add/remove fields, or add custom logic.
///
/// To get specific integer types, you can use:
/// ```
/// # struct Foo;
/// # impl<Context> bincode::Decode<Context> for Foo {
/// #     fn decode<D: bincode::de::Decoder<Context = Context>>(
/// #         decoder: &mut D,
/// #     ) -> core::result::Result<Self, bincode::error::DecodeError> {
/// let x: u8 = bincode::Decode::<Context>::decode(decoder)?;
/// let x = <u8 as bincode::Decode::<Context>>::decode(decoder)?;
/// #         Ok(Foo)
/// #     }
/// # }
/// # bincode::impl_borrow_decode!(Foo);
/// ```
///
/// You can use `Context` to require contexts for decoding a type:
/// ```
/// # /// # use bumpalo::Bump;
/// use bincode::de::Decoder;
/// use bincode::error::DecodeError;
/// struct BytesInArena<'a>(bumpalo::collections::Vec<'a, u8>);
/// impl<'a> bincode::Decode<&'a bumpalo::Bump> for BytesInArena<'a> {
/// fn decode<D: Decoder>(decoder: &mut D) -> Result<Self, DecodeError> {
///         todo!()
///     }
/// # }
/// ```
pub trait Decode<Context>: Sized {
    /// Attempt to decode this type with the given [Decode].
    fn decode<D: Decoder<Context = Context>>(decoder: &mut D) -> Result<Self, DecodeError>;
}

/// Trait that makes a type able to be decoded, akin to serde's `Deserialize` trait.
///
/// This trait should be implemented for types that contain borrowed data, like `&str` and `&[u8]`. If your type does not have borrowed data, consider implementing [Decode] instead.
///
/// This trait will be automatically implemented if you enable the `derive` feature and add `#[derive(bincode::Decode)]` to a type with a lifetime.
pub trait BorrowDecode<'de, Context>: Sized {
    /// Attempt to decode this type with the given [BorrowDecode].
    fn borrow_decode<D: BorrowDecoder<'de, Context = Context>>(
        decoder: &mut D,
    ) -> Result<Self, DecodeError>;
}

/// Helper macro to implement `BorrowDecode` for any type that implements `Decode`.
#[macro_export]
macro_rules! impl_borrow_decode {
    ($ty:ty $(, $param:tt)*) => {
        impl<'de $(, $param)*, __Context> $crate::BorrowDecode<'de, __Context> for $ty {
            fn borrow_decode<D: $crate::de::BorrowDecoder<'de, Context = __Context>>(
                decoder: &mut D,
            ) -> core::result::Result<Self, $crate::error::DecodeError> {
                $crate::Decode::decode(decoder)
            }
        }
    };
}

/// Helper macro to implement `BorrowDecode` for any type that implements `Decode`.
#[macro_export]
macro_rules! impl_borrow_decode_with_context {
    ($ty:ty, $context:ty $(, $param:tt)*) => {
        impl<'de $(, $param)*> $crate::BorrowDecode<'de, $context> for $ty {
            fn borrow_decode<D: $crate::de::BorrowDecoder<'de, Context = $context>>(
                decoder: &mut D,
            ) -> core::result::Result<Self, $crate::error::DecodeError> {
                $crate::Decode::decode(decoder)
            }
        }
    };
}

/// Any source that can decode basic types. This type is most notably implemented for [Decoder].
pub trait Decoder: Sealed {
    /// The concrete [Reader] type
    type R: Reader;

    /// The concrete [Config] type
    type C: Config;

    /// The decoding context type
    type Context;

    /// Returns the decoding context
    fn context(&mut self) -> &mut Self::Context;

    /// Wraps decoder with a context
    fn with_context<C>(&mut self, context: C) -> WithContext<Self, C> {
        WithContext {
            decoder: self,
            context,
        }
    }

    /// Returns a mutable reference to the reader
    fn reader(&mut self) -> &mut Self::R;

    /// Returns a reference to the config
    fn config(&self) -> &Self::C;

    /// Claim that `n` bytes are going to be read from the decoder.
    /// This can be used to validate `Configuration::Limit<N>()`.
    fn claim_bytes_read(&mut self, n: usize) -> Result<(), DecodeError>;

    /// Claim that we're going to read a container which contains `len` entries of `T`.
    /// This will correctly handle overflowing if `len * size_of::<T>() > usize::max_value`
    fn claim_container_read<T>(&mut self, len: usize) -> Result<(), DecodeError> {
        if <Self::C as InternalLimitConfig>::LIMIT.is_some() {
            match len.checked_mul(core::mem::size_of::<T>()) {
                Some(val) => self.claim_bytes_read(val),
                None => Err(DecodeError::LimitExceeded),
            }
        } else {
            Ok(())
        }
    }

    /// Notify the decoder that `n` bytes are being reclaimed.
    ///
    /// When decoding container types, a typical implementation would claim to read `len * size_of::<T>()` bytes.
    /// This is to ensure that bincode won't allocate several GB of memory while constructing the container.
    ///
    /// Because the implementation claims `len * size_of::<T>()`, but then has to decode each `T`, this would be marked
    /// as double. This function allows us to un-claim each `T` that gets decoded.
    ///
    /// We cannot check if `len * size_of::<T>()` is valid without claiming it, because this would mean that if you have
    /// a nested container (e.g. `Vec<Vec<T>>`), it does not know how much memory is already claimed, and could easily
    /// allocate much more than the user intends.
    /// ```
    /// # use bincode::de::{Decode, Decoder};
    /// # use bincode::error::DecodeError;
    /// # struct Container<T>(Vec<T>);
    /// # impl<T> Container<T> {
    /// #     fn with_capacity(cap: usize) -> Self {
    /// #         Self(Vec::with_capacity(cap))
    /// #     }
    /// #     
    /// #     fn push(&mut self, t: T) {
    /// #         self.0.push(t);
    /// #     }
    /// # }
    /// impl<Context, T: Decode<Context>> Decode<Context> for Container<T> {
    ///     fn decode<D: Decoder<Context = Context>>(decoder: &mut D) -> Result<Self, DecodeError> {
    ///         let len = u64::decode(decoder)?;
    ///         let len: usize = len.try_into().map_err(|_| DecodeError::OutsideUsizeRange(len))?;
    ///         // Make sure we don't allocate too much memory
    ///         decoder.claim_bytes_read(len * core::mem::size_of::<T>());
    ///
    ///         let mut result = Container::with_capacity(len);
    ///         for _ in 0..len {
    ///             // un-claim the memory
    ///             decoder.unclaim_bytes_read(core::mem::size_of::<T>());
    ///             result.push(T::decode(decoder)?)
    ///         }
    ///         Ok(result)
    ///     }
    /// }
    /// impl<'de, Context, T: bincode::BorrowDecode<'de, Context>> bincode::BorrowDecode<'de, Context> for Container<T> {
    ///     fn borrow_decode<D: bincode::de::BorrowDecoder<'de, Context = Context>>(
    ///         decoder: &mut D,
    ///     ) -> core::result::Result<Self, bincode::error::DecodeError> {
    ///         let len = u64::borrow_decode(decoder)?;
    ///         let len: usize = len.try_into().map_err(|_| DecodeError::OutsideUsizeRange(len))?;
    ///         // Make sure we don't allocate too much memory
    ///         decoder.claim_bytes_read(len * core::mem::size_of::<T>());
    ///
    ///         let mut result = Container::with_capacity(len);
    ///         for _ in 0..len {
    ///             // un-claim the memory
    ///             decoder.unclaim_bytes_read(core::mem::size_of::<T>());
    ///             result.push(T::borrow_decode(decoder)?)
    ///         }
    ///         Ok(result)
    ///     }
    /// }
    /// ```
    fn unclaim_bytes_read(&mut self, n: usize);
}

/// Any source that can decode basic types. This type is most notably implemented for [Decoder].
///
/// This is an extension of [Decode] that can also return borrowed data.
pub trait BorrowDecoder<'de>: Decoder {
    /// The concrete [BorrowReader] type
    type BR: BorrowReader<'de>;

    /// Rerturns a mutable reference to the borrow reader
    fn borrow_reader(&mut self) -> &mut Self::BR;
}

impl<T> Decoder for &mut T
where
    T: Decoder,
{
    type R = T::R;

    type C = T::C;

    type Context = T::Context;

    fn reader(&mut self) -> &mut Self::R {
        T::reader(self)
    }

    fn config(&self) -> &Self::C {
        T::config(self)
    }

    #[inline]
    fn claim_bytes_read(&mut self, n: usize) -> Result<(), DecodeError> {
        T::claim_bytes_read(self, n)
    }

    #[inline]
    fn unclaim_bytes_read(&mut self, n: usize) {
        T::unclaim_bytes_read(self, n)
    }

    fn context(&mut self) -> &mut Self::Context {
        T::context(self)
    }
}

impl<'de, T> BorrowDecoder<'de> for &mut T
where
    T: BorrowDecoder<'de>,
{
    type BR = T::BR;

    fn borrow_reader(&mut self) -> &mut Self::BR {
        T::borrow_reader(self)
    }
}

/// Decodes only the option variant from the decoder. Will not read any more data than that.
#[inline]
pub(crate) fn decode_option_variant<D: Decoder>(
    decoder: &mut D,
    type_name: &'static str,
) -> Result<Option<()>, DecodeError> {
    let is_some = u8::decode(decoder)?;
    match is_some {
        0 => Ok(None),
        1 => Ok(Some(())),
        x => Err(DecodeError::UnexpectedVariant {
            found: x as u32,
            allowed: &crate::error::AllowedEnumVariants::Range { max: 1, min: 0 },
            type_name,
        }),
    }
}

/// Decodes the length of any slice, container, etc from the decoder
#[inline]
pub(crate) fn decode_slice_len<D: Decoder>(decoder: &mut D) -> Result<usize, DecodeError> {
    let v = u64::decode(decoder)?;

    v.try_into().map_err(|_| DecodeError::OutsideUsizeRange(v))
}

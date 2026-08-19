//! Handling offsets

use super::read::{FontRead, ReadError};
use crate::font_data::FontData;
use types::{Nullable, Offset16, Offset24, Offset32};

/// Any offset type.
pub trait Offset: Copy {
    fn to_usize(self) -> usize;

    fn non_null(self) -> Option<usize> {
        match self.to_usize() {
            0 => None,
            other => Some(other),
        }
    }
}

macro_rules! impl_offset {
    ($name:ident, $width:literal) => {
        impl Offset for $name {
            #[inline]
            fn to_usize(self) -> usize {
                self.to_u32() as _
            }
        }
    };
}

impl_offset!(Offset16, 2);
impl_offset!(Offset24, 3);
impl_offset!(Offset32, 4);

/// A helper trait providing a 'resolve' method for offset types
pub trait ResolveOffset {
    fn resolve_with_args<'a, T: FontRead<'a>>(
        &self,
        data: FontData<'a>,
        args: T::Args,
    ) -> Result<T, ReadError>;

    /// Resolve an offset to a target requiring no external state (`Args = ()`).
    fn resolve<'a, T: FontRead<'a, Args = ()>>(&self, data: FontData<'a>) -> Result<T, ReadError> {
        self.resolve_with_args(data, ())
    }
}

/// A helper trait providing a 'resolve' method for nullable offset types
pub trait ResolveNullableOffset {
    fn resolve_with_args<'a, T: FontRead<'a>>(
        &self,
        data: FontData<'a>,
        args: T::Args,
    ) -> Option<Result<T, ReadError>>;

    /// Resolve an offset to a target requiring no external state (`Args = ()`).
    fn resolve<'a, T: FontRead<'a, Args = ()>>(
        &self,
        data: FontData<'a>,
    ) -> Option<Result<T, ReadError>> {
        self.resolve_with_args(data, ())
    }
}

impl<O: Offset> ResolveNullableOffset for Nullable<O> {
    fn resolve_with_args<'a, T: FontRead<'a>>(
        &self,
        data: FontData<'a>,
        args: T::Args,
    ) -> Option<Result<T, ReadError>> {
        match self.offset().resolve_with_args(data, args) {
            Ok(thing) => Some(Ok(thing)),
            Err(ReadError::NullOffset) => None,
            Err(e) => Some(Err(e)),
        }
    }
}

impl<O: Offset> ResolveOffset for O {
    fn resolve_with_args<'a, T: FontRead<'a>>(
        &self,
        data: FontData<'a>,
        args: T::Args,
    ) -> Result<T, ReadError> {
        self.non_null()
            .ok_or(ReadError::NullOffset)
            .and_then(|off| data.split_off(off).ok_or(ReadError::OutOfBounds))
            .and_then(|data| T::read_with_args(data, args))
    }
}

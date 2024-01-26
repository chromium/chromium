//! Custom array types

use crate::codegen_prelude::FromBytes;
use crate::read::{ComputeSize, FontReadWithArgs, ReadArgs, VarSize};
use crate::{FontData, FontRead, ReadError};

/// An array whose items size is not known at compile time.
///
/// This requires the inner type to implement [`FontReadWithArgs`] as well as
/// [`ComputeSize`].
///
/// At runtime, `Args` are provided which will be used to compute the size
/// of each item; this size is then used to compute the positions of the items
/// within the underlying data, from which they will be read lazily.
#[derive(Clone)]
pub struct ComputedArray<'a, T: ReadArgs> {
    // the length of each item
    item_len: usize,
    len: usize,
    data: FontData<'a>,
    args: T::Args,
}

impl<'a, T: ComputeSize> ComputedArray<'a, T> {
    pub fn new(data: FontData<'a>, args: T::Args) -> Self {
        let item_len = T::compute_size(&args);
        let len = data.len().checked_div(item_len).unwrap_or(0);
        ComputedArray {
            item_len,
            len,
            data,
            args,
        }
    }

    /// The number of items in the array
    pub fn len(&self) -> usize {
        self.len
    }

    pub fn is_empty(&self) -> bool {
        self.len == 0
    }
}

impl<'a, T: ReadArgs> ReadArgs for ComputedArray<'a, T> {
    type Args = T::Args;
}

impl<'a, T> FontReadWithArgs<'a> for ComputedArray<'a, T>
where
    T: ComputeSize + FontReadWithArgs<'a>,
    T::Args: Copy,
{
    fn read_with_args(data: FontData<'a>, args: &Self::Args) -> Result<Self, ReadError> {
        Ok(Self::new(data, *args))
    }
}

impl<'a, T> ComputedArray<'a, T>
where
    T: FontReadWithArgs<'a>,
    T::Args: Copy + 'static,
{
    pub fn iter(&self) -> impl Iterator<Item = Result<T, ReadError>> + 'a {
        let mut i = 0;
        let data = self.data;
        let args = self.args;
        let item_len = self.item_len;
        let len = self.len;

        std::iter::from_fn(move || {
            if i == len {
                return None;
            }
            let item_start = item_len * i;
            i += 1;
            let data = data.split_off(item_start)?;
            Some(T::read_with_args(data, &args))
        })
    }

    pub fn get(&self, idx: usize) -> Result<T, ReadError> {
        let item_start = idx * self.item_len;
        self.data
            .split_off(item_start)
            .ok_or(ReadError::OutOfBounds)
            .and_then(|data| T::read_with_args(data, &self.args))
    }
}

impl<T: ReadArgs> std::fmt::Debug for ComputedArray<'_, T> {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        f.debug_struct("DynSizedArray")
            .field("bytes", &self.data)
            .finish()
    }
}

/// An array of items of non-uniform length.
///
/// Random access into this array cannot be especially efficient, since it requires
/// a linear scan.
pub struct VarLenArray<'a, T> {
    data: FontData<'a>,
    phantom: std::marker::PhantomData<*const T>,
}

impl<'a, T: FontRead<'a> + VarSize> VarLenArray<'a, T> {
    /// Return the item at the provided index.
    ///
    /// This performs a linear search.
    pub fn get(&self, idx: usize) -> Option<Result<T, ReadError>> {
        let mut pos = 0;
        for _ in 0..idx {
            pos += T::read_len_at(self.data, pos)?;
        }
        self.data.split_off(pos).map(T::read)
    }

    /// Return an iterator over this array's items.
    pub fn iter(&self) -> impl Iterator<Item = Result<T, ReadError>> + 'a {
        let mut data = self.data;
        std::iter::from_fn(move || {
            if data.is_empty() {
                return None;
            }

            let item_len = T::read_len_at(data, 0)?;
            let next = T::read(data);
            data = data.split_off(item_len)?;
            Some(next)
        })
    }
}

impl<'a, T> FontRead<'a> for VarLenArray<'a, T> {
    fn read(data: FontData<'a>) -> Result<Self, ReadError> {
        Ok(VarLenArray {
            data,
            phantom: core::marker::PhantomData,
        })
    }
}

impl<'a, T: FromBytes> ReadArgs for &'a [T] {
    type Args = u16;
}

impl<'a, T: FromBytes> FontReadWithArgs<'a> for &'a [T] {
    fn read_with_args(data: FontData<'a>, args: &u16) -> Result<Self, ReadError> {
        let len = *args as usize * T::RAW_BYTE_LEN;
        data.read_array(0..len)
    }
}

//! Table data with a fixed-size header at its start.

use core::marker::PhantomData;

/// The bytes of a table whose start holds a header of type `H`.
///
/// The length is checked once, when the reference is made, so reading the
/// header's fields needs no further bounds checks.
pub struct TableDataRef<'a, H> {
    data: &'a [u8],
    _header: PhantomData<&'a H>,
}

impl<'a, H: bytemuck::AnyBitPattern> TableDataRef<'a, H> {
    const HEADER_ALIGN_IS_ONE: () = assert!(
        core::mem::align_of::<H>() == 1,
        "a table header must be readable at any address"
    );

    /// Creates a reference to the given data, or `None` if it is too short to
    /// hold the header.
    #[inline(always)]
    pub fn new(data: &'a [u8]) -> Option<Self> {
        #[allow(clippy::let_unit_value)]
        let _ = Self::HEADER_ALIGN_IS_ONE;
        (data.len() >= core::mem::size_of::<H>()).then_some(Self {
            data,
            _header: PhantomData,
        })
    }

    /// Returns the header at the start of the data.
    #[inline(always)]
    pub fn header(&self) -> &'a H {
        // SAFETY: `new` is the only constructor, and it checked that the data
        // holds at least `size_of::<H>()` bytes. `H` has an alignment of one,
        // and any bit pattern is a valid `H`.
        unsafe { &*self.data.as_ptr().cast::<H>() }
    }

    /// Returns all of the data, including the header.
    #[inline(always)]
    pub fn data(&self) -> &'a [u8] {
        self.data
    }
}

impl<H> Clone for TableDataRef<'_, H> {
    fn clone(&self) -> Self {
        *self
    }
}

impl<H> Copy for TableDataRef<'_, H> {}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::BigEndian;

    #[derive(Clone, Copy, bytemuck::AnyBitPattern)]
    #[repr(C, packed)]
    struct Header {
        a: BigEndian<u16>,
        b: BigEndian<u32>,
    }

    #[test]
    fn reads_header() {
        let data = [0, 1, 0, 0, 0, 2, 0xff];
        let table = TableDataRef::<Header>::new(&data).unwrap();
        assert_eq!(table.header().a.get(), 1);
        assert_eq!(table.header().b.get(), 2);
        assert_eq!(table.data().len(), 7);
    }

    #[test]
    fn rejects_short_data() {
        assert!(TableDataRef::<Header>::new(&[0, 1, 0, 0, 0]).is_none());
        assert!(TableDataRef::<Header>::new(&[0, 1, 0, 0, 0, 2]).is_some());
    }

    #[test]
    fn reads_unaligned_header() {
        let data = [0xff, 0, 3, 0, 0, 0, 4];
        let table = TableDataRef::<Header>::new(&data[1..]).unwrap();
        assert_eq!(table.header().a.get(), 3);
        assert_eq!(table.header().b.get(), 4);
    }
}

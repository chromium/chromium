//! compile-time representations of offsets

use super::write::{FontWrite, TableWriter};

/// The width in bytes of an Offset16
pub const WIDTH_16: usize = 2;
/// The width in bytes of an Offset24
pub const WIDTH_24: usize = 3;
/// The width in bytes of an Offset32
#[allow(dead_code)]
pub const WIDTH_32: usize = 4;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(u8)]
pub enum OffsetLen {
    Offset16 = 2,
    Offset24 = 3,
    Offset32 = 4,
}

impl OffsetLen {
    /// The maximum value for an offset of this length.
    #[cfg(feature = "tables")]
    pub const fn max_value(self) -> u32 {
        match self {
            Self::Offset16 => u16::MAX as u32,
            Self::Offset24 => (1 << 24) - 1,
            Self::Offset32 => u32::MAX,
        }
    }
}

impl std::fmt::Display for OffsetLen {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        match self {
            Self::Offset16 => write!(f, "Offset16"),
            Self::Offset24 => write!(f, "Offset24"),
            Self::Offset32 => write!(f, "Offset32"),
        }
    }
}

/// An offset subtable.
///
/// The generic const `N` is the width of the offset, in bytes.
#[derive(Clone, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct OffsetMarker<T, const N: usize = WIDTH_16> {
    obj: Box<T>,
}

/// An offset subtable which may be null.
///
/// The generic const `N` is the width of the offset, in bytes.
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
pub struct NullableOffsetMarker<T, const N: usize = WIDTH_16> {
    obj: Option<Box<T>>,
}

impl<T, const N: usize> std::ops::Deref for OffsetMarker<T, N> {
    type Target = T;
    fn deref(&self) -> &Self::Target {
        &self.obj
    }
}

impl<T, const N: usize> std::ops::DerefMut for OffsetMarker<T, N> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.obj
    }
}

impl<T, const N: usize> std::ops::Deref for NullableOffsetMarker<T, N> {
    type Target = Option<Box<T>>;
    fn deref(&self) -> &Self::Target {
        &self.obj
    }
}

impl<T, const N: usize> std::ops::DerefMut for NullableOffsetMarker<T, N> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.obj
    }
}

impl<T, const N: usize> AsRef<T> for OffsetMarker<T, N> {
    fn as_ref(&self) -> &T {
        &self.obj
    }
}

impl<T, const N: usize> AsMut<T> for OffsetMarker<T, N> {
    fn as_mut(&mut self) -> &mut T {
        &mut self.obj
    }
}

// NOTE: we don't impl AsRef/AsMut for NullableOffsetMarker, since it is less
// useful than the Option::as_ref and Option::as_mut methods available through deref
impl<const N: usize, T> OffsetMarker<T, N> {
    /// Create a new marker.
    pub fn new(obj: T) -> Self {
        OffsetMarker { obj: Box::new(obj) }
    }

    /// Set the contents of the marker, replacing any existing contents.
    #[cfg(feature = "tables")]
    pub fn set(&mut self, obj: impl Into<T>) {
        self.obj = Box::new(obj.into());
    }

    /// Convert into the inner type
    #[cfg(feature = "tables")]
    pub fn into_inner(self) -> T {
        *self.obj
    }
}

impl<const N: usize, T> NullableOffsetMarker<T, N> {
    /// Create a new marker.
    pub fn new(obj: Option<T>) -> Self {
        NullableOffsetMarker {
            obj: obj.map(|t| Box::new(t)),
        }
    }

    /// Set the contents of the marker, replacing any existing contents.
    ///
    /// The argument must be some value; to set the offset to null, use the
    /// [`clear`] method.
    ///
    /// [`clear`]: Self::clear
    #[cfg(feature = "tables")]
    pub fn set(&mut self, obj: impl Into<T>) {
        self.obj = Some(Box::new(obj.into()))
    }

    /// Clear the contents of the marker.
    #[cfg(feature = "tables")]
    pub fn clear(&mut self) {
        self.obj = None;
    }

    /// Convert into the inner type
    #[cfg(feature = "tables")]
    pub fn into_inner(self) -> Option<T> {
        self.obj.map(|b| *b)
    }

    pub fn as_ref(&self) -> Option<&T> {
        match &self.obj {
            Some(obj) => Some(obj.as_ref()),
            None => None,
        }
    }

    #[cfg(feature = "tables")]
    pub fn as_mut(&mut self) -> Option<&mut T> {
        match &mut self.obj {
            Some(obj) => Some(&mut *obj),
            None => None,
        }
    }
}

impl<const N: usize, T: FontWrite> FontWrite for OffsetMarker<T, N> {
    fn write_into(&self, writer: &mut TableWriter) {
        writer.write_offset(self.obj.as_ref(), N);
    }

    fn table_type(&self) -> crate::table_type::TableType {
        self.obj.table_type()
    }
}

impl<const N: usize, T: FontWrite> FontWrite for NullableOffsetMarker<T, N> {
    fn write_into(&self, writer: &mut TableWriter) {
        match self.obj.as_ref() {
            Some(obj) => writer.write_offset(obj.as_ref(), N),
            None => writer.write_slice([0u8; N].as_slice()),
        }
    }

    fn table_type(&self) -> crate::table_type::TableType {
        match self.obj.as_ref() {
            Some(obj) => obj.table_type(),
            None => crate::table_type::TableType::Unknown,
        }
    }
}

impl<T, const N: usize> Default for NullableOffsetMarker<T, N> {
    fn default() -> Self {
        Self { obj: None }
    }
}

impl<T, const N: usize> From<T> for OffsetMarker<T, N> {
    fn from(src: T) -> Self {
        OffsetMarker::new(src)
    }
}

impl<T, const N: usize> From<T> for NullableOffsetMarker<T, N> {
    fn from(src: T) -> Self {
        NullableOffsetMarker::new(Some(src))
    }
}

impl<T, const N: usize> From<Option<T>> for NullableOffsetMarker<T, N> {
    fn from(src: Option<T>) -> Self {
        NullableOffsetMarker::new(src)
    }
}

// In case I still want to use these?

//impl<T: std::fmt::Debug, const N: usize> std::fmt::Debug for OffsetMarker<T, N> {
//fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
//write!(f, "OffsetMarker({}, {:?})", N * 8, self.obj.as_ref(),)
//}
//}

//impl<const N: usize, T: std::fmt::Debug> std::fmt::Debug for NullableOffsetMarker<T, N> {
//fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
//write!(
//f,
//"NullableOffsetMarker({}, {:?})",
//N * 8,
//self.obj.as_ref(),
//)
//}
//}

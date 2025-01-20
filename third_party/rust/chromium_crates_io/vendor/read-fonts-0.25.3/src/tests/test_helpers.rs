//! small utilities used in tests

use crate::{FontData, Scalar};
use std::collections::HashMap;

/// A convenience type for generating a buffer of big-endian bytes.
#[derive(Debug, Clone, Default)]
pub struct BeBuffer {
    data: Vec<u8>,
    tagged_locations: HashMap<String, usize>,
}

impl BeBuffer {
    pub fn new() -> Self {
        Default::default()
    }

    /// The current length of the buffer in bytes.
    pub fn len(&self) -> usize {
        self.data.len()
    }

    /// Returns `true` if the buffer contains zero bytes.
    pub fn is_empty(&self) -> bool {
        self.data.is_empty()
    }

    /// Return a reference to the contents of the buffer
    pub fn as_slice(&self) -> &[u8] {
        &self.data
    }

    /// Write any scalar to this buffer.
    pub fn push(mut self, item: impl Scalar) -> Self {
        self.data.extend(item.to_raw().as_ref());
        self
    }

    pub fn push_with_tag(mut self, item: impl Scalar, tag: &str) -> Self {
        self.tagged_locations
            .insert(tag.to_string(), self.data.len());
        self.data.extend(item.to_raw().as_ref());
        self
    }

    /// Write multiple scalars into the buffer
    pub fn extend<T: Scalar>(mut self, iter: impl IntoIterator<Item = T>) -> Self {
        for item in iter {
            self.data.extend(item.to_raw().as_ref());
        }
        self
    }

    pub fn offset_for(&self, tag: &str) -> usize {
        // panic on unrecognized tags
        self.tagged_locations.get(tag).copied().unwrap()
    }

    fn data_for(&mut self, tag: &str) -> &mut [u8] {
        let offset = self.offset_for(tag);
        &mut self.data[offset..]
    }

    pub fn write_at(&mut self, tag: &str, item: impl Scalar) {
        let data = self.data_for(tag);
        let raw = item.to_raw();
        let new_data: &[u8] = raw.as_ref();

        if data.len() < new_data.len() {
            panic!("not enough room left in buffer for the requested write.");
        }

        for (left, right) in data.iter_mut().zip(new_data) {
            *left = *right
        }
    }

    pub fn font_data(&self) -> FontData {
        FontData::new(&self.data)
    }
}

impl std::ops::Deref for BeBuffer {
    type Target = [u8];
    fn deref(&self) -> &Self::Target {
        &self.data
    }
}

/// be_buffer_add!(buffer, value) - Add an item to a be_buffer.
///
/// Will call the appropriate insertion method on BeBuffer depending on the values format.
///
/// Values can be one of three types:
/// 1. Single numeric value, ie: 10u16
/// 2. Array of numeric values, ie: [1u8, 2, 3]
/// 3. Tagged value: {10u16, "tag_name"}. This will insert the numeric value
///    and attach the provide string tag to the insertion location in the buffer.
#[macro_export]
macro_rules! be_buffer_add {
    ($b:ident, $v:literal) => {
        let $b = $b.push($v);
    };
    ($b:ident, {$v:tt : $tag:literal}) => {
        let $b = $b.push_with_tag($v, $tag);
    };
    ($b:ident, [$($v:literal),+]) => {
        let $b = $b.extend([$($v),*]);
    };
    ($b:ident, $v:tt) => {
        let $b = $b.push($v);
    };
}

/// be_buffer!(val1, ..., valn) - Constructs a BeBuffer from the provided list of values:
///
/// Values can be one of three types:
/// 1. Single numeric value, ie: 10u16
/// 2. Array of numeric values, ie: [1u8, 2, 3]
/// 3. Tagged value: {10u16, "tag_name"}. This will insert the numeric value
///    and attach the provide string tag to the insertion location in the buffer.
#[macro_export]
macro_rules! be_buffer {
    ( $( $x:tt ),+ ) => {
        {
            let builder = BeBuffer::new();
            $(
                be_buffer_add!(builder, $x);
            )*
            builder
        }
    };
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn be_buffer_macro() {
        let data: &[u8] = &be_buffer! {
            1u8,
            2u16,
            3u32
        };

        assert_eq!([1, 0, 2, 0, 0, 0, 3], data);
    }

    #[test]
    fn be_buffer_macro_array() {
        let data: &[u8] = &be_buffer! {
            1u8,
            [2u8, 3, 4, 5, 6]
        };

        assert_eq!([1, 2, 3, 4, 5, 6], data);
    }

    #[test]
    fn be_buffer_macro_tagged() {
        let builder = be_buffer! {
            1u8,
            {2u16: "foo"},
            {(u32::from(3u16)): "bar"}
        };
        let data: &[u8] = &builder;

        assert_eq!([1, 0, 2, 0, 0, 0, 3], data);
        assert_eq!(builder.offset_for("foo"), 1);
        assert_eq!(builder.offset_for("bar"), 3);
    }
}

use std::ops::Range;

/// Return the minimum range of the table bytes
///
/// This trait is implemented in generated code, and we use this to get the
/// minimum length/bytes of a table.
pub trait MinByteRange<'a> {
    fn min_byte_range(&self) -> Range<usize>;
    fn min_table_bytes(&self) -> &'a [u8];
}

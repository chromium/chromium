//! Input to lexer
use std::borrow::Cow;

use crate::char_stream::{CharStream, InputData};
use crate::int_stream::IntStream;
use std::ops::Deref;

/// Default rust target input stream.
///
/// Since Rust uses UTF-8 format which does not support indexing by char,
/// `InputStream<&str>` has slightly different index behavior in compare to java runtime when there are
/// non-ASCII unicode characters.
/// If you need it to generate exactly the same indexes as Java runtime, you have to use `CodePoint8/16/32BitCharStream`,
/// which does not use rusts native `str` type, so it would do additional conversions and allocations along the way.
#[derive(Debug)]
pub struct InputStream<Data: Deref> {
    name: String,
    data_raw: Data,
    index: isize,
}

// #[impl_tid]
// impl<'a, T: ?Sized + 'static> TidAble<'a> for InputStream<Box<T>> {}
// #[impl_tid]
// impl<'a, T: ?Sized + 'static> TidAble<'a> for InputStream<&'a T> {}
better_any::tid! {impl<'a, T: 'static> TidAble<'a> for InputStream<&'a T> where T: ?Sized}
better_any::tid! {impl<'a, T: 'static> TidAble<'a> for InputStream<Box<T>> where T: ?Sized}

impl<'a, T: From<&'a str>> CharStream<T> for InputStream<&'a str> {
    #[inline]
    fn get_text(&self, start: isize, stop: isize) -> T {
        self.get_text_inner(start, stop).into()
    }
}

impl<T: From<D::Owned>, D: ?Sized + InputData> CharStream<T> for InputStream<Box<D>> {
    #[inline]
    fn get_text(&self, start: isize, stop: isize) -> T {
        self.get_text_owned(start, stop).into()
    }
}
/// `InputStream` over byte slice
pub type ByteStream<'a> = InputStream<&'a [u8]>;
/// InputStream which treats the input as a series of Unicode code points that fit into `u8`
pub type CodePoint8BitCharStream<'a> = InputStream<&'a [u8]>;
/// InputStream which treats the input as a series of Unicode code points that fit into `u16`
pub type CodePoint16BitCharStream<'a> = InputStream<&'a [u16]>;
/// InputStream which treats the input as a series of Unicode code points
pub type CodePoint32BitCharStream<'a> = InputStream<&'a [u32]>;

impl<'a, T> CharStream<Cow<'a, [T]>> for InputStream<&'a [T]>
where
    [T]: InputData,
{
    #[inline]
    fn get_text(&self, a: isize, b: isize) -> Cow<'a, [T]> {
        Cow::Borrowed(self.get_text_inner(a, b))
    }
}

impl<T> CharStream<String> for InputStream<&[T]>
where
    [T]: InputData,
{
    fn get_text(&self, a: isize, b: isize) -> String {
        self.get_text_inner(a, b).to_display()
    }
}

impl<'b, T> CharStream<Cow<'b, str>> for InputStream<&[T]>
where
    [T]: InputData,
{
    #[inline]
    fn get_text(&self, a: isize, b: isize) -> Cow<'b, str> {
        self.get_text_inner(a, b).to_display().into()
    }
}

impl<'a, T> CharStream<&'a [T]> for InputStream<&'a [T]>
where
    [T]: InputData,
{
    #[inline]
    fn get_text(&self, a: isize, b: isize) -> &'a [T] {
        self.get_text_inner(a, b)
    }
}

impl<Data: ?Sized + InputData> InputStream<Box<Data>> {
    fn get_text_owned(&self, start: isize, stop: isize) -> Data::Owned {
        let start = start as usize;
        let stop = self.data_raw.offset(stop, 1).unwrap_or(stop) as usize;

        if stop < self.data_raw.len() {
            &self.data_raw[start..stop]
        } else {
            &self.data_raw[start..]
        }
        .to_owned()
    }

    /// Creates new `InputStream` over owned data   
    pub fn new_owned(data: Box<Data>) -> Self {
        Self {
            name: "<empty>".to_string(),
            data_raw: data,
            index: 0,
        }
    }
}

impl<'a, Data> InputStream<&'a Data>
where
    Data: ?Sized + InputData,
{
    fn get_text_inner(&self, start: isize, stop: isize) -> &'a Data {
        // println!("get text {}..{} of {:?}",start,stop,self.data_raw.to_display());
        let start = start as usize;
        let stop = self.data_raw.offset(stop, 1).unwrap_or(stop) as usize;
        // println!("justed range {}..{} ",start,stop);
        // let start = self.data_raw.offset(0,start).unwrap() as usize;
        // let stop = self.data_raw.offset(0,stop + 1).unwrap() as usize;

        if stop < self.data_raw.len() {
            &self.data_raw[start..stop]
        } else {
            &self.data_raw[start..]
        }
    }

    /// Creates new `InputStream` over borrowed data
    pub fn new(data_raw: &'a Data) -> Self {
        // let data_raw = data_raw.as_ref();
        // let data = data_raw.to_indexed_vec();
        Self {
            name: "<empty>".to_string(),
            data_raw,
            index: 0,
            // phantom: Default::default(),
        }
    }
}
impl<'a, Data: Deref> InputStream<Data>
where
    Data::Target: InputData,
{
    /// Resets input stream to start from the beginning of this slice
    #[inline]
    pub fn reset(&mut self) {
        self.index = 0
    }
}

impl<'a, Data: Deref> IntStream for InputStream<Data>
where
    Data::Target: InputData,
{
    #[inline]
    fn consume(&mut self) {
        if let Some(index) = self.data_raw.offset(self.index, 1) {
            self.index = index;
            // self.current = self.data_raw.deref().item(index).unwrap_or(TOKEN_EOF);
            // Ok(())
        } else {
            panic!("cannot consume EOF");
        }
    }

    #[inline]
    fn la(&mut self, mut offset: isize) -> i32 {
        if offset == 1 {
            return self
                .data_raw
                .item(self.index)
                .unwrap_or(crate::int_stream::EOF);
        }
        if offset == 0 {
            panic!("should not be called with offset 0");
        }
        if offset < 0 {
            offset += 1; // e.g., translate LA(-1) to use offset i=0; then data[p+0-1]
        }

        self.data_raw
            .offset(self.index, offset - 1)
            .and_then(|index| self.data_raw.item(index))
            .unwrap_or(crate::int_stream::EOF)
    }

    #[inline]
    fn mark(&mut self) -> isize {
        -1
    }

    #[inline]
    fn release(&mut self, _marker: isize) {}

    #[inline]
    fn index(&self) -> isize {
        self.index
    }

    #[inline]
    fn seek(&mut self, index: isize) {
        self.index = index
    }

    #[inline]
    fn size(&self) -> isize {
        self.data_raw.len() as isize
    }

    fn get_source_name(&self) -> String {
        self.name.clone()
    }
}

#[cfg(test)]
mod test {
    use std::ops::Deref;

    use crate::char_stream::CharStream;
    use crate::int_stream::{IntStream, EOF};

    use super::InputStream;

    #[test]
    fn test_str_input_stream() {
        let mut input = InputStream::new("V1は3");
        let input = &mut input as &mut dyn CharStream<String>;
        assert_eq!(input.la(1), 'V' as i32);
        assert_eq!(input.index(), 0);
        input.consume();
        assert_eq!(input.la(1), '1' as i32);
        assert_eq!(input.la(-1), 'V' as i32);
        assert_eq!(input.index(), 1);
        input.consume();
        assert_eq!(input.la(1), 0x306F);
        assert_eq!(input.index(), 2);
        input.consume();
        assert_eq!(input.index(), 5);
        assert_eq!(input.la(-2), '1' as i32);
        assert_eq!(input.la(2), EOF);
        assert_eq!(input.get_text(1, 1).deref(), "1");
        assert_eq!(input.get_text(1, 2).deref(), "1は");
        assert_eq!(input.get_text(2, 2).deref(), "は");
        assert_eq!(input.get_text(2, 5).deref(), "は3");
        assert_eq!(input.get_text(5, 5).deref(), "3");
    }

    #[test]
    fn test_byte_input_stream() {
        let mut input = InputStream::new(&b"V\xaa\xbb"[..]);
        assert_eq!(input.la(1), 'V' as i32);
        input.seek(2);
        assert_eq!(input.la(1), 0xBB);
        assert_eq!(input.index(), 2);
        let mut input = InputStream::new("は".as_bytes());
        assert_eq!(input.la(1), 227);
    }
}

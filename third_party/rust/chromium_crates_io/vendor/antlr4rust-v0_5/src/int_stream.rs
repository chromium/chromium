//! <ost generic stream of symbols

use crate::token::{TOKEN_EOF};

/// `IntStream::la` must return EOF in the end of stream
pub const EOF: i32 = -1;

/// A simple stream of symbols whose values are represented as integers. This
/// interface provides *marked ranges* with support for a minimum level
/// of buffering necessary to implement arbitrary lookahead during prediction.
pub trait IntStream {
    /// Consumes the current symbol in the stream.
    /// Advances this stream to the next element.
    ///
    ///	This method has the following
    /// effects:
    ///
    ///  - Forward movement: The value of `index`
    ///		before calling this method is less than the value of `index`
    ///		after calling this method.
    ///  - Ordered lookahead: The value of {@code LA(1)} before
    ///		calling this method becomes the value of {@code LA(-1)} after calling
    ///		this method.
    ///
    /// Note that calling this method does not guarantee that `index()` is
    /// incremented by exactly 1.
    ///
    /// Allowed to panic if trying to consume EOF
    fn consume(&mut self);

    /// Lookaheads (or loopbacks if `i` is negative)
    ///
    /// Gets the value of the symbol at offset {@code i} from the current
    /// position. When {@code i==1}, this method returns the value of the current
    /// symbol in the stream (which is the next symbol to be consumed). When
    /// {@code i==-1}, this method returns the value of the previously read
    /// symbol in the stream. It is not valid to call this method with
    /// {@code i==0}, but the specific behavior is unspecified because this
    /// method is frequently called from performance-critical code.
    ///
    /// Note that default Lexer does not call this method with anything other than `-1`
    /// so it can be used for optimizations in downstream implementations.
    ///
    /// Must return `EOF` if `i` points to position at or beyond the end of the stream
    fn la(&mut self, i: isize) -> i32;

    /// After this call subsequent calls to seek must succeed if seek index is greater than mark index
    ///
    /// Returns marker that should be used later by `release` call to release this stream from
    fn mark(&mut self) -> isize;

    /// Releases `marker`
    fn release(&mut self, marker: isize);

    /// Returns current position of the input stream
    ///
    /// If there is active marker from `mark` then calling `seek` later with result of this call
    /// should put stream in same state it is currently in.
    fn index(&self) -> isize;
    /// Put stream back in state it was when it was in `index` position
    ///
    /// Allowed to panic if `index` does not belong to marked region(via `mark`-`release` calls)
    fn seek(&mut self, index: isize);

    /// Returns the total number of symbols in the stream.
    fn size(&self) -> isize;

    /// Returns name of the source this stream operates over if any
    fn get_source_name(&self) -> String;
}

/// Iterator over `IntStream`
#[derive(Debug)]
pub struct IterWrapper<'a, T: IntStream>(pub &'a mut T, pub bool);

impl<T: IntStream> Iterator for IterWrapper<'_, T> {
    type Item = i32;

    fn next(&mut self) -> Option<Self::Item> {
        if self.1 {
            return None
        }
        let token = self.0.la(1);

        let result = if self.0.size() > self.0.index() {
            Some(token)
        } else {
            None
        };
        if token == TOKEN_EOF {
            self.1 = true;
        }
        if !self.1 {
            self.0.consume();
        }
        result
    }
}

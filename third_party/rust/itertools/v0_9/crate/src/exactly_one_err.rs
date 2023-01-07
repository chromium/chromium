use std::iter::ExactSizeIterator;

use crate::size_hint;

/// Iterator returned for the error case of `IterTools::exactly_one()`
/// This iterator yields exactly the same elements as the input iterator.
///
/// During the execution of exactly_one the iterator must be mutated.  This wrapper
/// effectively "restores" the state of the input iterator when it's handed back.
///
/// This is very similar to PutBackN except this iterator only supports 0-2 elements and does not
/// use a `Vec`.
#[derive(Debug, Clone)]
pub struct ExactlyOneError<I>
where
    I: Iterator,
{
    first_two: (Option<I::Item>, Option<I::Item>),
    inner: I,
}

impl<I> ExactlyOneError<I>
where
    I: Iterator,
{
    /// Creates a new `ExactlyOneErr` iterator.
    pub(crate) fn new(first_two: (Option<I::Item>, Option<I::Item>), inner: I) -> Self {
        Self { first_two, inner }
    }
}

impl<I> Iterator for ExactlyOneError<I>
where
    I: Iterator,
{
    type Item = I::Item;

    fn next(&mut self) -> Option<Self::Item> {
        self.first_two
            .0
            .take()
            .or_else(|| self.first_two.1.take())
            .or_else(|| self.inner.next())
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let mut additional_len = 0;
        if self.first_two.0.is_some() {
            additional_len += 1;
        }
        if self.first_two.1.is_some() {
            additional_len += 1;
        }
        size_hint::add_scalar(self.inner.size_hint(), additional_len)
    }
}

impl<I> ExactSizeIterator for ExactlyOneError<I> where I: ExactSizeIterator {}

use std::iter::Fuse;
use super::size_hint;

#[derive(Clone)]
/// An iterator adaptor to insert a particular value
/// between each element of the adapted iterator.
///
/// Iterator element type is `I::Item`
///
/// This iterator is *fused*.
///
/// See [`.intersperse()`](../trait.Itertools.html#method.intersperse) for more information.
#[must_use = "iterator adaptors are lazy and do nothing unless consumed"]
#[derive(Debug)]
pub struct Intersperse<I>
    where I: Iterator
{
    element: I::Item,
    iter: Fuse<I>,
    peek: Option<I::Item>,
}

/// Create a new Intersperse iterator
pub fn intersperse<I>(iter: I, elt: I::Item) -> Intersperse<I>
    where I: Iterator
{
    let mut iter = iter.fuse();
    Intersperse {
        peek: iter.next(),
        iter,
        element: elt,
    }
}

impl<I> Iterator for Intersperse<I>
    where I: Iterator,
          I::Item: Clone
{
    type Item = I::Item;
    #[inline]
    fn next(&mut self) -> Option<I::Item> {
        if self.peek.is_some() {
            self.peek.take()
        } else {
            self.peek = self.iter.next();
            if self.peek.is_some() {
                Some(self.element.clone())
            } else {
                None
            }
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        // 2 * SH + { 1 or 0 }
        let has_peek = self.peek.is_some() as usize;
        let sh = self.iter.size_hint();
        size_hint::add_scalar(size_hint::add(sh, sh), has_peek)
    }

    fn fold<B, F>(mut self, init: B, mut f: F) -> B where
        Self: Sized, F: FnMut(B, Self::Item) -> B,
    {
        let mut accum = init;
        
        if let Some(x) = self.peek.take() {
            accum = f(accum, x);
        }

        let element = &self.element;

        self.iter.fold(accum,
            |accum, x| {
                let accum = f(accum, element.clone());
                let accum = f(accum, x);
                accum
        })
    }
}

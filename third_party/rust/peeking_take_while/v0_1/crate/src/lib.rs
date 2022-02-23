//! # `peeking_take_while`
//!
//! Provides the `peeking_take_while` iterator adaptor method.
//!
//! The `peeking_take_while` method is very similar to `take_while`, but behaves
//! differently when used with a borrowed iterator (perhaps returned by
//! `Iterator::by_ref`).
//!
//! `peeking_take_while` peeks at the next item in the iterator and runs the
//! predicate on that peeked item. This avoids consuming the first item yielded
//! by the underlying iterator for which the predicate returns `false`. On the
//! other hand, `take_while` will consume that first item for which the
//! predicate returns `false`, and it will be lost.
//!
//! ```
//! extern crate peeking_take_while;
//!
//! // Bring the `peeking_take_while` method for peekable iterators into
//! // scope.
//! use peeking_take_while::PeekableExt;
//!
//! # fn main() {
//! // Let's say we have two collections we want to iterate through: `xs` and
//! // `ys`. We want to perform one operation on all the leading contiguous
//! // elements that match some predicate, and a different thing with the rest of
//! // the elements. With the `xs`, we will use the normal `take_while`. With the
//! // `ys`, we will use `peeking_take_while`.
//!
//! let xs: Vec<u8> = (0..100).collect();
//! let ys = xs.clone();
//!
//! let mut iter_xs = xs.into_iter();
//! let mut iter_ys = ys.into_iter().peekable();
//!
//! {
//!     // Let's do one thing with all the items that are less than 10.
//! #   fn do_things_with<T>(_: T) {}
//!
//!     let xs_less_than_ten = iter_xs.by_ref().take_while(|x| *x < 10);
//!     for x in xs_less_than_ten {
//!         do_things_with(x);
//!     }
//!
//!     let ys_less_than_ten = iter_ys.by_ref().peeking_take_while(|y| *y < 10);
//!     for y in ys_less_than_ten {
//!         do_things_with(y);
//!     }
//! }
//!
//! // And now we will do some other thing with the items that are greater than
//! // or equal to 10.
//!
//! // ...except, when using plain old `take_while` we lost 10!
//! assert_eq!(iter_xs.next(), Some(11));
//!
//! // However, when using `peeking_take_while` we did not! Great!
//! assert_eq!(iter_ys.next(), Some(10));
//! # }
//! ```

use std::iter::Peekable;

/// The iterator returned by `peeking_take_while`.
///
/// See the [module documentation](./index.html) for details.
pub struct PeekingTakeWhile<'a, I, P>
    where I: 'a + Iterator
{
    iter: &'a mut Peekable<I>,
    predicate: P,
}

impl<'a, I, P> Iterator for PeekingTakeWhile<'a, I, P>
    where I: Iterator,
          I::Item: ::std::fmt::Debug,
          P: FnMut(&<I as Iterator>::Item) -> bool
{
    type Item = <I as Iterator>::Item;

    fn next(&mut self) -> Option<Self::Item> {
        let predicate = &mut self.predicate;
        if self.iter.peek().map_or(false, |x| !(predicate)(x)) {
            None
        } else {
            self.iter.next()
        }
    }
}

/// The `Iterator` extension trait that provides the `peeking_take_while`
/// method.
///
/// See the [module documentation](./index.html) for details.
pub trait PeekableExt<'a, I>: Iterator
    where I: 'a + Iterator
{
    /// The `Iterator` extension trait that provides the `peeking_take_while`
    /// method.
    ///
    /// See the [module documentation](./index.html) for details.
    fn peeking_take_while<P>(&'a mut self, predicate: P) -> PeekingTakeWhile<'a, I, P>
        where Self: Sized,
              P: FnMut(&<Self as Iterator>::Item) -> bool;
}

impl<'a, I> PeekableExt<'a, I> for Peekable<I>
    where I: 'a + Iterator
{
    fn peeking_take_while<P>(&'a mut self, predicate: P) -> PeekingTakeWhile<I, P>
        where P: FnMut(&<Self as Iterator>::Item) -> bool
    {
        PeekingTakeWhile {
            iter: self,
            predicate: predicate,
        }
    }
}

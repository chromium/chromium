/*!
Iterate over error `.source()` chains.

NOTE: This module is taken wholesale from <https://crates.io/crates/eyre>.
*/
use std::error::Error as StdError;
use std::vec;

use ChainState::*;

/// Iterator of a chain of source errors.
///
/// This type is the iterator returned by [`Report::chain`].
///
/// # Example
///
/// ```
/// use miette::Report;
/// use std::io;
///
/// pub fn underlying_io_error_kind(error: &Report) -> Option<io::ErrorKind> {
///     for cause in error.chain() {
///         if let Some(io_error) = cause.downcast_ref::<io::Error>() {
///             return Some(io_error.kind());
///         }
///     }
///     None
/// }
/// ```
#[derive(Clone)]
#[allow(missing_debug_implementations)]
pub struct Chain<'a> {
    state: crate::chain::ChainState<'a>,
}

#[derive(Clone)]
pub(crate) enum ChainState<'a> {
    Linked {
        next: Option<&'a (dyn StdError + 'static)>,
    },
    Buffered {
        rest: vec::IntoIter<&'a (dyn StdError + 'static)>,
    },
}

impl<'a> Chain<'a> {
    pub(crate) fn new(head: &'a (dyn StdError + 'static)) -> Self {
        Chain {
            state: ChainState::Linked { next: Some(head) },
        }
    }
}

impl<'a> Iterator for Chain<'a> {
    type Item = &'a (dyn StdError + 'static);

    fn next(&mut self) -> Option<Self::Item> {
        match &mut self.state {
            Linked { next } => {
                let error = (*next)?;
                *next = error.source();
                Some(error)
            }
            Buffered { rest } => rest.next(),
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let len = self.len();
        (len, Some(len))
    }
}

impl DoubleEndedIterator for Chain<'_> {
    fn next_back(&mut self) -> Option<Self::Item> {
        match &mut self.state {
            Linked { mut next } => {
                let mut rest = Vec::new();
                while let Some(cause) = next {
                    next = cause.source();
                    rest.push(cause);
                }
                let mut rest = rest.into_iter();
                let last = rest.next_back();
                self.state = Buffered { rest };
                last
            }
            Buffered { rest } => rest.next_back(),
        }
    }
}

impl ExactSizeIterator for Chain<'_> {
    fn len(&self) -> usize {
        match &self.state {
            Linked { mut next } => {
                let mut len = 0;
                while let Some(cause) = next {
                    next = cause.source();
                    len += 1;
                }
                len
            }
            Buffered { rest } => rest.len(),
        }
    }
}

impl Default for Chain<'_> {
    fn default() -> Self {
        Chain {
            state: ChainState::Buffered {
                rest: Vec::new().into_iter(),
            },
        }
    }
}

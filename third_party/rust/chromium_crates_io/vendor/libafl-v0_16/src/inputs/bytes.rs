//! The `BytesInput` is the "normal" input, a map of bytes, that can be sent directly to the client
//! (As opposed to other, more abstract, inputs, like an Grammar-Based AST Input)

use alloc::{
    borrow::ToOwned,
    rc::Rc,
    vec::{self, Vec},
};
use core::cell::RefCell;

use libafl_bolts::{HasLen, ownedref::OwnedSlice};

use super::ValueInput;
use crate::inputs::{HasMutatorBytes, HasTargetBytes, ResizableMutator};

/// A bytes input is the basic input
pub type BytesInput = ValueInput<Vec<u8>>;

/// Rc Ref-cell from Input
impl From<BytesInput> for Rc<RefCell<BytesInput>> {
    fn from(input: BytesInput) -> Self {
        Rc::new(RefCell::new(input))
    }
}

impl HasMutatorBytes for BytesInput {
    fn mutator_bytes(&self) -> &[u8] {
        self.as_ref()
    }

    fn mutator_bytes_mut(&mut self) -> &mut [u8] {
        self.as_mut()
    }
}

impl ResizableMutator<u8> for BytesInput {
    fn resize(&mut self, new_len: usize, value: u8) {
        self.as_mut().resize(new_len, value);
    }

    fn extend<'a, I: IntoIterator<Item = &'a u8>>(&mut self, iter: I) {
        <Vec<u8> as Extend<I::Item>>::extend(self.as_mut(), iter);
    }

    fn splice<R, I>(&mut self, range: R, replace_with: I) -> vec::Splice<'_, I::IntoIter>
    where
        R: core::ops::RangeBounds<usize>,
        I: IntoIterator<Item = u8>,
    {
        self.as_mut().splice(range, replace_with)
    }

    fn drain<R>(&mut self, range: R) -> vec::Drain<'_, u8>
    where
        R: core::ops::RangeBounds<usize>,
    {
        self.as_mut().drain(range)
    }
}

impl HasTargetBytes for BytesInput {
    #[inline]
    fn target_bytes(&self) -> OwnedSlice<'_, u8> {
        OwnedSlice::from(self.as_ref())
    }
}

impl HasLen for BytesInput {
    fn len(&self) -> usize {
        self.as_ref().len()
    }
}

impl From<&[u8]> for BytesInput {
    fn from(bytes: &[u8]) -> Self {
        Self::new(bytes.to_owned())
    }
}

impl From<BytesInput> for Vec<u8> {
    fn from(value: BytesInput) -> Vec<u8> {
        value.into_inner()
    }
}

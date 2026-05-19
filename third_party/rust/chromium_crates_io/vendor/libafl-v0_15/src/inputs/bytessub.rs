//! [`BytesSubInput`] is a wrapper input that can be used to mutate parts of a byte slice

use alloc::vec::{self, Vec};
use core::{
    cmp::Ordering,
    ops::{Range, RangeBounds},
};

use libafl_bolts::{
    HasLen,
    subrange::{end_index, start_index, sub_range},
};

use crate::inputs::{HasMutatorBytes, ResizableMutator};

/// The [`BytesSubInput`] makes it possible to use [`crate::mutators::Mutator`]`s` that work on
/// inputs implementing the [`HasMutatorBytes`] for a sub-range of this input.
///
/// For example, we can do the following:
/// ```rust
/// # extern crate alloc;
/// # extern crate libafl;
/// # use libafl::inputs::{BytesInput, HasMutatorBytes};
/// # use alloc::vec::Vec;
/// #
/// # #[cfg(not(feature = "std"))]
/// # #[unsafe(no_mangle)]
/// # pub extern "C" fn external_current_millis() -> u64 { 0 }
///
/// let mut bytes_input = BytesInput::new(vec![1, 2, 3]);
/// let mut sub_input = bytes_input.sub_input(1..);
///
/// // Run any mutations on the sub input.
/// sub_input.mutator_bytes_mut()[0] = 42;
///
/// // The mutations are applied to the underlying input.
/// assert_eq!(bytes_input.mutator_bytes()[1], 42);
/// ```
///
/// If inputs implement the [`ResizableMutator`] trait, growing or shrinking the sub input
/// will grow or shrink the parent input,
/// and keep elements around the current range untouched / move them accordingly.
///
/// For example:
/// ```rust
/// # extern crate alloc;
/// # extern crate libafl;
/// # use libafl::inputs::{BytesInput, HasMutatorBytes, ResizableMutator};
/// # use alloc::vec::Vec;
/// #
/// # #[cfg(not(feature = "std"))]
/// # #[unsafe(no_mangle)]
/// # pub extern "C" fn external_current_millis() -> u64 { 0 }
///
/// let mut bytes_input = BytesInput::new(vec![1, 2, 3, 4, 5]);
///
/// // Note that the range ends on an exclusive value this time.
/// let mut sub_input = bytes_input.sub_input(1..=3);
///
/// assert_eq!(sub_input.mutator_bytes(), &[2, 3, 4]);
///
/// // We extend it with a few values.
/// sub_input.extend(&[42, 42, 42]);
///
/// // The values outside of the range are moved back and forwards, accordingly.
/// assert_eq!(bytes_input.mutator_bytes(), [1, 2, 3, 4, 42, 42, 42, 5]);
/// ```
///
/// The input supports all methods in the [`HasMutatorBytes`] trait if the parent input also implements this trait.
#[derive(Debug)]
pub struct BytesSubInput<'a, I: ?Sized> {
    /// The (complete) parent input we will work on
    pub(crate) parent_input: &'a mut I,
    /// The range inside the parent input we will work on
    pub(crate) range: Range<usize>,
}

impl<'a, I> BytesSubInput<'a, I>
where
    I: HasMutatorBytes + ?Sized,
{
    /// Creates a new [`BytesSubInput`] that's a view on an input with mutator bytes.
    /// The sub input can then be used to mutate parts of the original input.
    pub fn new<R>(parent_input: &'a mut I, range: R) -> Self
    where
        R: RangeBounds<usize>,
    {
        let parent_len = parent_input.len();

        BytesSubInput {
            parent_input,
            range: Range {
                start: start_index(&range),
                end: end_index(&range, parent_len),
            },
        }
    }
}

impl<I> HasMutatorBytes for BytesSubInput<'_, I>
where
    I: HasMutatorBytes,
{
    #[inline]
    fn mutator_bytes(&self) -> &[u8] {
        &self.parent_input.mutator_bytes()[self.range.clone()]
    }

    #[inline]
    fn mutator_bytes_mut(&mut self) -> &mut [u8] {
        &mut self.parent_input.mutator_bytes_mut()[self.range.clone()]
    }
}

impl<I> ResizableMutator<u8> for BytesSubInput<'_, I>
where
    I: ResizableMutator<u8> + HasMutatorBytes + HasLen,
{
    fn resize(&mut self, new_len: usize, value: u8) {
        let start_index = self.range.start;
        let end_index = self.range.end;
        let old_len = end_index - start_index;

        match new_len.cmp(&old_len) {
            Ordering::Equal => {
                // Nothing to do here.
            }
            Ordering::Greater => {
                // We grow. Resize the underlying buffer, then move the entries past our `end_index` back.
                let diff = new_len - old_len;

                let old_parent_len = self.parent_input.len();
                self.parent_input.resize(old_parent_len + diff, value);

                if old_parent_len > end_index {
                    // the parent has a reminder, move it back.
                    let parent_bytes = self.parent_input.mutator_bytes_mut();

                    // move right
                    let (_, rest) = parent_bytes.split_at_mut(start_index + old_len);
                    rest.copy_within(0..rest.len() - diff, diff);
                    let (new, _rest) = rest.split_at_mut(diff);

                    // fill
                    new.fill(value);
                }

                self.range.end += diff;
            }
            Ordering::Less => {
                // We shrink. Remove the values, then remove the underlying buffer.
                let diff = old_len - new_len;

                let parent_bytes = self.parent_input.mutator_bytes_mut();

                // move left
                let (_, rest) = parent_bytes.split_at_mut(start_index + new_len);
                rest.copy_within(diff.., 0);

                // cut off the rest
                self.parent_input
                    .resize(self.parent_input.len() - diff, value);

                self.range.end -= diff;
            }
        }
    }

    fn extend<'b, IT: IntoIterator<Item = &'b u8>>(&mut self, iter: IT) {
        let old_len = self.len();

        let new_values: Vec<u8> = iter.into_iter().copied().collect();
        self.resize(old_len + new_values.len(), 0);
        self.mutator_bytes_mut()[old_len..].copy_from_slice(&new_values);
    }

    /// Creates a splicing iterator that replaces the specified range in the vector
    /// with the given `replace_with` iterator and yields the removed items.
    /// `replace_with` does not need to be the same length as range.
    /// Refer to the docs of [`Vec::splice`]
    fn splice<R2, IT>(&mut self, range: R2, replace_with: IT) -> vec::Splice<'_, IT::IntoIter>
    where
        R2: RangeBounds<usize>,
        IT: IntoIterator<Item = u8>,
    {
        let range = sub_range(&self.range, range);
        self.parent_input.splice(range, replace_with)
    }

    fn drain<R2>(&mut self, range: R2) -> vec::Drain<'_, u8>
    where
        R2: RangeBounds<usize>,
    {
        let sub_range = sub_range(&self.range, range);
        let drain = self.parent_input.drain(sub_range);
        self.range.end -= drain.len();
        drain
    }
}

impl<I> HasLen for BytesSubInput<'_, I>
where
    I: HasMutatorBytes,
{
    #[inline]
    fn len(&self) -> usize {
        self.range.len()
    }
}
#[cfg(test)]
mod tests {
    use alloc::vec::Vec;

    use libafl_bolts::HasLen;

    use crate::{
        inputs::{BytesInput, HasMutatorBytes, NopInput, ResizableMutator},
        mutators::{MutatorsTuple, havoc_mutations_no_crossover},
        state::NopState,
    };

    fn init_bytes_input() -> (BytesInput, usize) {
        let bytes_input = BytesInput::new(vec![1, 2, 3, 4, 5, 6, 7]);
        let len_orig = bytes_input.len();
        (bytes_input, len_orig)
    }

    #[test]
    fn test_bytessubinput() {
        let (bytes_input, _) = init_bytes_input();

        let sub_input = bytes_input.sub_bytes(0..1);
        assert_eq!(*sub_input.as_slice(), [1]);

        let sub_input = bytes_input.sub_bytes(1..=2);
        assert_eq!(*sub_input.as_slice(), [2, 3]);

        let sub_input = bytes_input.sub_bytes(..);
        assert_eq!(*sub_input.as_slice(), [1, 2, 3, 4, 5, 6, 7]);
    }

    #[test]
    fn test_mutablebytessubinput() {
        let (mut bytes_input, len_orig) = init_bytes_input();

        let mut sub_input = bytes_input.sub_input(0..1);
        assert_eq!(sub_input.len(), 1);
        sub_input.mutator_bytes_mut()[0] = 2;
        assert_eq!(bytes_input.mutator_bytes()[0], 2);

        let mut sub_input = bytes_input.sub_input(1..=2);
        assert_eq!(sub_input.len(), 2);
        sub_input.mutator_bytes_mut()[0] = 3;
        assert_eq!(bytes_input.mutator_bytes()[1], 3);

        let mut sub_input = bytes_input.sub_input(..);
        assert_eq!(sub_input.len(), len_orig);
        sub_input.mutator_bytes_mut()[0] = 1;
        sub_input.mutator_bytes_mut()[1] = 2;
        assert_eq!(bytes_input.mutator_bytes()[0], 1);
    }

    #[test]
    fn test_bytessubinput_resize() {
        let (mut bytes_input, len_orig) = init_bytes_input();
        let bytes_input_orig = bytes_input.clone();

        let mut sub_input = bytes_input.sub_input(2..);
        assert_eq!(sub_input.len(), len_orig - 2);
        sub_input.resize(len_orig, 0);
        assert_eq!(sub_input.mutator_bytes()[sub_input.len() - 1], 0);
        assert_eq!(sub_input.len(), len_orig);
        assert_eq!(bytes_input.len(), len_orig + 2);
        assert_eq!(bytes_input.mutator_bytes()[bytes_input.len() - 1], 0);

        let (mut bytes_input, len_orig) = init_bytes_input();

        let mut sub_input = bytes_input.sub_input(..2);
        assert_eq!(sub_input.len(), 2);
        sub_input.resize(3, 0);
        assert_eq!(sub_input.len(), 3);
        assert_eq!(sub_input.mutator_bytes()[sub_input.len() - 1], 0);
        assert_eq!(bytes_input.len(), len_orig + 1);

        let mut sub_input = bytes_input.sub_input(..3);
        assert_eq!(sub_input.len(), 3);
        sub_input.resize(2, 0);
        assert_eq!(sub_input.len(), 2);
        assert_eq!(bytes_input, bytes_input_orig);

        let mut sub_input = bytes_input.sub_input(2..=2);
        sub_input.resize(2, 0);
        sub_input.resize(1, 0);
        assert_eq!(bytes_input, bytes_input_orig);

        let mut sub_input = bytes_input.sub_input(..);
        assert_eq!(sub_input.len(), bytes_input_orig.len());
        sub_input.resize(1, 0);
        assert_eq!(sub_input.len(), 1);
        sub_input.resize(10, 0);
        assert_eq!(sub_input.len(), 10);
        assert_eq!(bytes_input.len(), 10);
        assert_eq!(bytes_input.mutator_bytes()[2], 0);

        let mut sub_input = bytes_input.sub_input(..);
        sub_input.resize(1, 0);
        assert_eq!(bytes_input.len(), 1);
    }

    #[test]
    fn test_bytessubinput_drain_extend() {
        let (mut bytes_input, len_orig) = init_bytes_input();
        let bytes_input_cloned = bytes_input.clone();

        let mut sub_input = bytes_input.sub_input(..2);
        let drained: Vec<_> = sub_input.drain(..).collect();
        assert_eq!(sub_input.len(), 0);
        assert_eq!(bytes_input.len(), len_orig - 2);

        let mut sub_input = bytes_input.sub_input(..0);
        assert_eq!(sub_input.len(), 0);
        let drained_len = drained.len();
        sub_input.extend(&drained[..]);
        assert_eq!(sub_input.len(), drained_len);
        assert_eq!(bytes_input, bytes_input_cloned);
    }

    #[test]
    fn test_bytessubinput_mutator() {
        let (mut bytes_input, _len_orig) = init_bytes_input();
        let bytes_input_cloned = bytes_input.clone();

        let mut sub_input = bytes_input.sub_input(..2);

        // Note that if you want to use NopState in production like this, you should see the rng! :)
        let mut state: NopState<NopInput> = NopState::new();

        let result = havoc_mutations_no_crossover().mutate_all(&mut state, &mut sub_input);
        assert!(result.is_ok());
        assert_ne!(bytes_input, bytes_input_cloned);
    }

    #[test]
    fn test_bytessubinput_use_vec() {
        let mut test_vec = vec![0, 1, 2, 3, 4];
        let mut sub_vec = test_vec.sub_input(1..2);
        drop(sub_vec.drain(..));
        assert_eq!(test_vec.len(), 4);
    }

    #[test]
    fn test_ranges() {
        let bytes_input = BytesInput::new(vec![1, 2, 3]);

        assert_eq!(bytes_input.sub_bytes(..1).start_index(), 0);
        assert_eq!(bytes_input.sub_bytes(1..=1).start_index(), 1);
        assert_eq!(bytes_input.sub_bytes(..1).end_index(), 1);
        assert_eq!(bytes_input.sub_bytes(..=1).end_index(), 2);
        assert_eq!(bytes_input.sub_bytes(1..=1).end_index(), 2);
        assert_eq!(bytes_input.sub_bytes(1..).end_index(), 3);
        assert_eq!(bytes_input.sub_bytes(..3).end_index(), 3);
    }

    #[test]
    fn test_ranges_mut() {
        let mut bytes_input = BytesInput::new(vec![1, 2, 3]);

        assert_eq!(bytes_input.sub_bytes_mut(..1).start_index(), 0);
        assert_eq!(bytes_input.sub_bytes_mut(1..=1).start_index(), 1);
        assert_eq!(bytes_input.sub_bytes_mut(..1).end_index(), 1);
        assert_eq!(bytes_input.sub_bytes_mut(..=1).end_index(), 2);
        assert_eq!(bytes_input.sub_bytes_mut(1..=1).end_index(), 2);
        assert_eq!(bytes_input.sub_bytes_mut(1..).end_index(), 3);
        assert_eq!(bytes_input.sub_bytes_mut(..3).end_index(), 3);
    }
}

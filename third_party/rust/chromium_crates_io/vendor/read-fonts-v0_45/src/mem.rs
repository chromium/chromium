//! Short lived memory.
//!
//! For work whose size is not known until it runs, and which is finished with
//! the memory by the time it returns. Allocating for each such call can cost
//! more than the work itself.

use alloc::{vec, vec::Vec};
use bytemuck::{AnyBitPattern, NoUninit};
use core::mem::{align_of, size_of};

/// Calls `f` with a zeroed block of `size` bytes.
///
/// The block comes from the stack where it is small enough, and from the heap
/// otherwise. Sizes are bucketed so that a small request does not pay to zero
/// a block sized for the largest one.
pub fn with_scratch<R>(size: usize, mut f: impl FnMut(&mut [u8]) -> R) -> R {
    // Not inlined, so a caller that lands in another bucket neither reserves
    // this one's stack nor zeroes it.
    #[inline(never)]
    fn on_stack<const SIZE: usize, R>(size: usize, mut f: impl FnMut(&mut [u8]) -> R) -> R {
        f(&mut [0u8; SIZE][..size])
    }
    if size <= 512 {
        on_stack::<512, _>(size, f)
    } else if size <= 1024 {
        on_stack::<1024, _>(size, f)
    } else if size <= 2048 {
        on_stack::<2048, _>(size, f)
    } else if size <= 4096 {
        on_stack::<4096, _>(size, f)
    } else if size <= 8192 {
        on_stack::<8192, _>(size, f)
    } else if size <= 16384 {
        on_stack::<16384, _>(size, f)
    } else {
        let mut block: Vec<u8> = vec![0; size];
        f(&mut block)
    }
}

/// Takes a `len` element slice of `T` off the front of `bytes`, aligning
/// first, and returns it along with what is left.
///
/// For carving one block into the several typed slices a caller needs, so that
/// the whole of it is one allocation rather than one each.
///
/// Returns `None` when the block is of insufficient size.
pub fn take_slice<T>(bytes: &mut [u8], len: usize) -> Option<(&mut [T], &mut [u8])>
where
    T: AnyBitPattern + NoUninit,
{
    if len == 0 {
        return Some((Default::default(), bytes));
    }
    let padding = (bytes.as_ptr() as usize).wrapping_neg() & (align_of::<T>() - 1);
    let bytes = bytes.get_mut(padding..)?;
    // A count that cannot be expressed in bytes is one no block can hold.
    let byte_len = len.checked_mul(size_of::<T>())?;
    if byte_len > bytes.len() {
        return None;
    }
    let (mine, rest) = bytes.split_at_mut(byte_len);
    // Bytemuck checks size and alignment; both hold by construction.
    let mine = bytemuck::try_cast_slice_mut(mine).ok()?;
    Some((mine, rest))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn every_bucket_hands_back_the_size_asked_for() {
        // A block from a larger bucket is trimmed to the request, so a caller
        // carving it into typed slices reads none of the slack.
        for size in [0, 1, 512, 513, 4096, 16384, 16385, 40000] {
            assert_eq!(with_scratch(size, |block| block.len()), size);
        }
    }

    #[test]
    fn a_block_arrives_zeroed() {
        // A caller may write every field before reading it, but carving a
        // block into typed slices reads it as those types first.
        for size in [8, 700, 20000] {
            assert!(with_scratch(size, |block| block
                .iter()
                .all(|byte| *byte == 0)));
        }
    }

    #[test]
    fn a_block_is_carved_into_aligned_slices() {
        with_scratch(64, |block| {
            let (first, rest) = take_slice::<u32>(block, 3).unwrap();
            assert_eq!(first.len(), 3);
            assert_eq!(first.as_ptr() as usize % align_of::<u32>(), 0);
            let (second, rest) = take_slice::<u16>(rest, 5).unwrap();
            assert_eq!(second.len(), 5);
            assert_eq!(second.as_ptr() as usize % align_of::<u16>(), 0);
            // Three `u32` and five `u16` leave 42 of the 64 bytes, less
            // whatever aligning the second slice cost.
            assert!(rest.len() <= 42, "{} left", rest.len());
            first[0] = 7;
            second[4] = 9;
        });
    }

    #[test]
    fn an_empty_slice_takes_nothing() {
        // A caller whose slice is unused still asks for it, so asking for
        // none of something is an answer rather than a failure.
        let mut block = [0u8; 4];
        let (taken, rest) = take_slice::<u64>(&mut block, 0).unwrap();
        assert!(taken.is_empty());
        assert_eq!(rest.len(), 4);
    }

    /// A block whose first byte is deliberately not aligned for `i32`.
    fn unaligned(block: &mut [u8]) -> &mut [u8] {
        let base = block.as_ptr() as usize;
        let offset = usize::from(base % align_of::<i32>() == 0);
        let block = &mut block[offset..];
        assert_ne!(block.as_ptr() as usize % align_of::<i32>(), 0);
        block
    }

    #[test]
    fn an_unaligned_block_is_aligned_before_anything_is_taken() {
        let mut block = [0u8; 40];
        let (taken, _) = take_slice::<i32>(unaligned(&mut block), 8).unwrap();
        assert_eq!(taken.as_ptr() as usize % align_of::<i32>(), 0);
    }

    #[test]
    fn aligning_comes_out_of_the_block() {
        // 40 bytes hold ten `i32`, but not once some of them have gone to
        // aligning the first.
        let mut block = [0u8; 40];
        assert!(take_slice::<i32>(unaligned(&mut block), 10).is_none());
    }

    #[test]
    fn a_block_too_small_for_what_is_asked_refuses() {
        let mut block = [0u8; 8];
        assert!(take_slice::<u32>(&mut block, 3).is_none());
        // What one slice takes is gone for the next, padding included, so a
        // block holding two of something holds neither twice.
        let (first, rest) = take_slice::<u32>(&mut block, 1).unwrap();
        assert_eq!(first.len(), 1);
        assert!(take_slice::<u32>(rest, 2).is_none());
        // Nor can it hold a count that is not a size at all.
        assert!(take_slice::<u32>(&mut block, usize::MAX).is_none());
    }
}

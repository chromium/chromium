// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(unsafe_code)]

use core::slice;
use std::{
    fmt::Debug,
    mem::MaybeUninit,
    ops::{Deref, DerefMut},
};

pub trait SmallVecHeapStorage<T> {
    const STACK_ONLY: bool;
    fn as_slice(&self) -> &[T];
    fn as_slice_mut(&mut self) -> &mut [T];
    fn with_capacity(cap: usize) -> Self;
    fn push(&mut self, v: T);
    fn extend<Iter: Iterator<Item = T>>(&mut self, iter: Iter);
}

pub enum StackOnly {}

impl<T> SmallVecHeapStorage<T> for StackOnly {
    const STACK_ONLY: bool = true;
    #[inline(always)]
    fn as_slice(&self) -> &[T] {
        unreachable!()
    }
    #[inline(always)]
    fn as_slice_mut(&mut self) -> &mut [T] {
        unreachable!()
    }
    #[inline(always)]
    fn with_capacity(_: usize) -> Self {
        unreachable!();
    }
    #[inline(always)]
    fn push(&mut self, _: T) {
        unreachable!()
    }
    #[inline(always)]
    fn extend<Iter: Iterator<Item = T>>(&mut self, _: Iter) {
        unreachable!()
    }
}

impl<T> SmallVecHeapStorage<T> for Vec<T> {
    const STACK_ONLY: bool = false;
    #[inline(always)]
    fn as_slice(&self) -> &[T] {
        &self[..]
    }
    #[inline(always)]
    fn as_slice_mut(&mut self) -> &mut [T] {
        &mut self[..]
    }
    #[inline(always)]
    fn with_capacity(cap: usize) -> Self {
        Self::with_capacity(cap)
    }
    #[inline(always)]
    fn push(&mut self, v: T) {
        self.push(v)
    }
    #[inline(always)]
    fn extend<Iter: Iterator<Item = T>>(&mut self, iter: Iter) {
        Extend::extend(self, iter)
    }
}

/// Note: this implementation of SmallVec is not panic-safe, in the sense
/// that in presence of panics the SmallVec will be left in some valid but
/// unspecified state.
pub enum SmallVec<T, const N: usize, HeapStorage: SmallVecHeapStorage<T> = Vec<T>> {
    Stack {
        // Safety invariant: the first `len` values of `data` are initialized.
        len: usize,
        data: [MaybeUninit<T>; N],
    },
    Heap(HeapStorage),
}

/// A SmallVec optimized to store up to #channels values.
// Most images have at most 7 channels (RGBA + noise extra channels).
// 8 gives a bit extra leeway and makes the size a power of two.
pub(crate) type ChannelVec<T> = SmallVec<T, 8>;

impl<T, const N: usize, HeapStorage: SmallVecHeapStorage<T>> Deref for SmallVec<T, N, HeapStorage> {
    type Target = [T];

    #[inline]
    fn deref(&self) -> &[T] {
        match self {
            SmallVec::Stack { len, data } => {
                let data = &data[..*len];
                // SAFETY: the safety invariant on `self` guarantees that the elements are
                // initialized, and T and MaybeUninit<T> have the same size and alignment.
                unsafe { slice::from_raw_parts(data.as_ptr().cast::<T>(), data.len()) }
            }
            SmallVec::Heap(v) => v.as_slice(),
        }
    }
}

impl<T, const N: usize, HeapStorage: SmallVecHeapStorage<T>> DerefMut
    for SmallVec<T, N, HeapStorage>
{
    #[inline]
    fn deref_mut(&mut self) -> &mut [T] {
        match self {
            SmallVec::Stack { len, data } => {
                let data = &mut data[..*len];
                // SAFETY: the safety invariant on `self` guarantees that the elements are
                // initialized, and T and MaybeUninit<T> have the same size and alignment.
                unsafe { slice::from_raw_parts_mut(data.as_mut_ptr().cast::<T>(), data.len()) }
            }
            SmallVec::Heap(v) => v.as_slice_mut(),
        }
    }
}

impl<T: Debug, const N: usize, HeapStorage: SmallVecHeapStorage<T>> Debug
    for SmallVec<T, N, HeapStorage>
{
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "SmallVec<{N}>({:?})", &**self)
    }
}

impl<T, const N: usize, HeapStorage: SmallVecHeapStorage<T>> Default
    for SmallVec<T, N, HeapStorage>
{
    fn default() -> Self {
        Self::new()
    }
}

impl<T, const N: usize, HeapStorage: SmallVecHeapStorage<T>> SmallVec<T, N, HeapStorage> {
    #[inline]
    pub fn new() -> Self {
        Self::Stack {
            // Safety note: len == 0 makes the safety invariant trivially true.
            len: 0,
            data: [const { MaybeUninit::uninit() }; N],
        }
    }

    #[inline]
    pub fn is_empty(&self) -> bool {
        match self {
            Self::Stack { len, .. } => *len == 0,
            Self::Heap(v) => v.as_slice().is_empty(),
        }
    }

    #[inline]
    pub fn len(&self) -> usize {
        match self {
            Self::Stack { len, .. } => *len,
            Self::Heap(v) => v.as_slice().len(),
        }
    }

    #[inline(never)]
    fn move_to_heap_impl(&mut self) {
        let Self::Stack { len, data } = self else {
            // Nothing to do.
            return;
        };
        let mut ret = HeapStorage::with_capacity(*len);
        let old_len = *len;
        *len = 0;
        for data in data[..old_len].iter_mut() {
            let mut tmp = MaybeUninit::uninit();
            std::mem::swap(&mut tmp, data);
            // SAFETY: the safety invariant on `self` promises that `data[i]` is initialized
            // for all i < old_len. Since we set `len` to 0, we are not breaking the safety
            // invariant if this function were to panic.
            ret.push(unsafe { tmp.assume_init() });
        }
        *self = Self::Heap(ret);
    }

    #[inline(always)]
    fn move_to_heap(&mut self) {
        if HeapStorage::STACK_ONLY {
            panic!("Tried to move a stack-only SmallVec to the heap!")
        }
        self.move_to_heap_impl();
    }

    // Note: if `iter` has an incorrect implementation of `size_hint` (specifically, incorrect
    // upper bound), some elements of `iter` may be discarded.
    #[inline(always)]
    pub fn extend<I: IntoIterator<Item = T>>(&mut self, iter: I) {
        let mut iter = iter.into_iter();
        let new_size = iter.size_hint().1.and_then(|x| x.checked_add(self.len()));
        if new_size.is_none_or(|u| u > N) {
            self.move_to_heap();
        }
        let (len, data) = match self {
            Self::Heap(v) => {
                v.extend(iter);
                return;
            }
            Self::Stack { len, data } => (len, data),
        };

        // We now know `iter`'s elements fit on the stack.
        while *len < N
            && let Some(e) = iter.next()
        {
            data[*len].write(e);
            // Safety note: we just wrote a new element in the first non-initialized slot of
            // the array.
            *len += 1;
        }
    }

    #[inline]
    pub fn push(&mut self, val: T) {
        if self.len() + 1 > N {
            self.move_to_heap();
        }
        let (len, data) = match self {
            Self::Heap(v) => {
                v.push(val);
                return;
            }
            Self::Stack { len, data } => (len, data),
        };
        data[*len].write(val);
        // Safety note: we just wrote a new element in the first non-initialized slot of
        // the array.
        *len += 1;
    }
}

impl<T, const N: usize, HeapStorage: SmallVecHeapStorage<T>> FromIterator<T>
    for SmallVec<T, N, HeapStorage>
{
    #[inline]
    fn from_iter<I: IntoIterator<Item = T>>(iter: I) -> Self {
        let mut ret = Self::new();
        ret.extend(iter);
        ret
    }
}

impl<T, const N: usize, HeapStorage: SmallVecHeapStorage<T>> Drop for SmallVec<T, N, HeapStorage> {
    fn drop(&mut self) {
        if let SmallVec::Stack { len, data } = self {
            let old_len = *len;
            *len = 0;
            for el in data[..old_len].iter_mut() {
                // SAFETY: by safety invariant, the first `old_len` elements are initialized.
                // We set *len to 0 to make sure we preserve the safety invariant, although
                // that should not be strictly necessary as *self cannot be accessed outside
                // this function anymore.
                unsafe { el.assume_init_drop() };
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use arbtest::arbitrary::Arbitrary;

    #[test]
    fn test_size() {
        // StackOnly is uninhabited, so the enum should not have a discriminant
        // (and all the SmallVec::Heap paths should be erased by dead-code elimination)
        assert_eq!(
            std::mem::size_of::<SmallVec<usize, 3, StackOnly>>(),
            4 * std::mem::size_of::<usize>()
        )
    }

    #[test]
    fn test_new() {
        let sv: SmallVec<i32, 4> = SmallVec::new();
        assert!(sv.is_empty());
        assert_eq!(sv.len(), 0);
    }

    #[test]
    fn test_push_stack() {
        let mut sv: SmallVec<i32, 4> = SmallVec::new();
        sv.push(1);
        sv.push(2);
        assert_eq!(sv.len(), 2);
        assert_eq!(sv[0], 1);
        assert_eq!(sv[1], 2);
        assert!(matches!(sv, SmallVec::Stack { .. }));
    }

    #[test]
    fn test_push_heap() {
        let mut sv: SmallVec<i32, 2> = SmallVec::new();
        sv.push(1);
        sv.push(2);
        sv.push(3);
        assert_eq!(sv.len(), 3);
        assert_eq!(sv[0], 1);
        assert_eq!(sv[1], 2);
        assert_eq!(sv[2], 3);
        assert!(matches!(sv, SmallVec::Heap(_)));
    }

    #[test]
    #[should_panic(expected = "Tried to move a stack-only SmallVec to the heap!")]
    fn test_push_heap_stackonly() {
        let mut sv: SmallVec<i32, 2, StackOnly> = SmallVec::new();
        sv.push(1);
        sv.push(2);
        sv.push(3);
    }

    #[test]
    fn test_extend() {
        let mut sv: SmallVec<i32, 4> = SmallVec::new();
        sv.extend(vec![1, 2, 3]);
        assert_eq!(sv.len(), 3);
        assert_eq!(sv[0], 1);
        assert_eq!(sv[2], 3);
        assert!(matches!(sv, SmallVec::Stack { .. }));

        sv.extend(vec![4, 5]);
        assert_eq!(sv.len(), 5);
        assert_eq!(sv[4], 5);
        assert!(matches!(sv, SmallVec::Heap(_)));
    }

    #[test]
    fn test_from_iter() {
        let sv: SmallVec<i32, 4> = SmallVec::from_iter(vec![1, 2, 3]);
        assert_eq!(sv.len(), 3);
        assert_eq!(sv[0], 1);

        let sv: SmallVec<i32, 2> = SmallVec::from_iter(vec![1, 2, 3]);
        assert_eq!(sv.len(), 3);
        assert!(matches!(sv, SmallVec::Heap(_)));
    }

    #[test]
    fn test_debug() {
        let mut sv: SmallVec<i32, 4> = SmallVec::new();
        sv.push(1);
        let s = format!("{:?}", sv);
        assert!(s.contains("SmallVec"));
        assert!(s.contains("[1]"));
    }

    #[test]
    fn test_smallvec_matches_vec() {
        arbtest::arbtest(|u| {
            let mut smallvec: SmallVec<u8, 8> = SmallVec::new();
            let mut vec: Vec<u8> = Vec::new();

            let num_ops = u8::arbitrary(u)?;
            for _ in 0..num_ops {
                let op_type = *u.choose(&[0, 1])?;
                if op_type == 0 {
                    let val = u8::arbitrary(u)?;
                    smallvec.push(val);
                    vec.push(val);
                } else {
                    let num_elements = u8::arbitrary(u)? as usize;
                    let mut elements = Vec::new();
                    for _ in 0..num_elements {
                        elements.push(u8::arbitrary(u)?);
                    }
                    smallvec.extend(elements.iter().copied());
                    Extend::extend(&mut vec, elements.iter().copied());
                }

                assert_eq!(&*smallvec, &*vec);
            }

            Ok(())
        });
    }
}

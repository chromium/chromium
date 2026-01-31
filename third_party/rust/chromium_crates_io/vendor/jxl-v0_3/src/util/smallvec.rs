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

/// Note: this implementation of SmallVec is not panic-safe, in the sense
/// that in presence of panics the SmallVec will be left in some valid but
/// unspecified state.
pub enum SmallVec<T, const N: usize> {
    Stack {
        // Safety invariant: the first `len` values of `data` are initialized.
        len: usize,
        data: [MaybeUninit<T>; N],
    },
    Heap(Vec<T>),
}

impl<T, const N: usize> Deref for SmallVec<T, N> {
    type Target = [T];

    fn deref(&self) -> &[T] {
        match self {
            SmallVec::Stack { len, data } => {
                let data = &data[..*len];
                // SAFETY: the safety invariant on `self` guarantees that the elements are
                // initialized, and T and MaybeUninit<T> have the same size and alignment.
                unsafe { slice::from_raw_parts(data.as_ptr().cast::<T>(), data.len()) }
            }
            SmallVec::Heap(v) => &v[..],
        }
    }
}

impl<T, const N: usize> DerefMut for SmallVec<T, N> {
    fn deref_mut(&mut self) -> &mut [T] {
        match self {
            SmallVec::Stack { len, data } => {
                let data = &mut data[..*len];
                // SAFETY: the safety invariant on `self` guarantees that the elements are
                // initialized, and T and MaybeUninit<T> have the same size and alignment.
                unsafe { slice::from_raw_parts_mut(data.as_mut_ptr().cast::<T>(), data.len()) }
            }
            SmallVec::Heap(v) => &mut v[..],
        }
    }
}

impl<T: Debug, const N: usize> Debug for SmallVec<T, N> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "SmallVec<{N}>({:?})", &**self)
    }
}

impl<T, const N: usize> Default for SmallVec<T, N> {
    fn default() -> Self {
        Self::new()
    }
}

impl<T, const N: usize> SmallVec<T, N> {
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
            Self::Heap(v) => v.is_empty(),
        }
    }

    #[inline]
    pub fn len(&self) -> usize {
        match self {
            Self::Stack { len, .. } => *len,
            Self::Heap(v) => v.len(),
        }
    }

    #[inline(never)]
    fn move_to_heap(&mut self) {
        let Self::Stack { len, data } = self else {
            // Nothing to do.
            return;
        };
        let mut ret = Vec::<T>::with_capacity(*len);
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

    // It is easier to implement this method than to implement IntoIterator.
    #[inline]
    pub fn extend_sv<const M: usize>(&mut self, mut other: SmallVec<T, M>) {
        if self.len() + other.len() > N {
            self.move_to_heap();
        }
        if matches!(self, Self::Heap(_)) {
            other.move_to_heap();
        }
        if matches!(other, SmallVec::Heap(_)) {
            self.move_to_heap();
        }
        let (len, data) = match self {
            Self::Heap(v) => {
                let SmallVec::Heap(o) = &mut other else {
                    unreachable!()
                };
                v.extend(std::mem::take(o));
                return;
            }
            Self::Stack { len, data } => (len, data),
        };

        // We now know `other`'s elements fit on the stack.
        let SmallVec::Stack {
            len: olen,
            data: odata,
        } = &mut other
        else {
            unreachable!()
        };
        let other_len = *olen;
        *olen = 0;
        data[*len..*len + other_len].swap_with_slice(&mut odata[..other_len]);
        // Safety note: we just wrote `other_len` elements in the first non-initialized slots
        // of the array.
        *len += other_len;
    }
}

impl<T, const N: usize> FromIterator<T> for SmallVec<T, N> {
    #[inline]
    fn from_iter<I: IntoIterator<Item = T>>(iter: I) -> Self {
        let mut ret = Self::new();
        ret.extend(iter);
        ret
    }
}

impl<T, const N: usize> Drop for SmallVec<T, N> {
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
    fn test_extend_sv() {
        let mut sv1: SmallVec<i32, 4> = SmallVec::new();
        sv1.push(1);
        let mut sv2: SmallVec<i32, 4> = SmallVec::new();
        sv2.push(2);

        sv1.extend_sv(sv2);
        assert_eq!(sv1.len(), 2);
        assert_eq!(sv1[0], 1);
        assert_eq!(sv1[1], 2);
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
                    vec.extend(elements.iter().copied());
                }

                assert_eq!(&*smallvec, &*vec);
            }

            Ok(())
        });
    }
}

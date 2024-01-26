//! Runtime sized fixed array type with inlined storage optimization.
//!
//! This should be replaced with `smallvec` when that dependency is
//! available to us. In the meantime, this type should be kept small
//! with functionality only added when needed.

use core::fmt;
use std::ops::{Deref, DerefMut};

/// Internal SmallVec like implementation but for fixed sized arrays whose
/// size is only known at runtime.
///
/// Used to avoid allocations when possible for small or temporary
/// arrays.
#[derive(Clone)]
pub(crate) struct SmallArray<T, const N: usize>
where
    T: Copy,
{
    storage: SmallStorage<T, N>,
}

impl<T, const N: usize> SmallArray<T, N>
where
    T: Copy,
{
    pub fn new(initial_value: T, len: usize) -> Self {
        Self {
            storage: if len <= N {
                SmallStorage::Inline([initial_value; N], len)
            } else {
                SmallStorage::Heap(vec![initial_value; len])
            },
        }
    }

    pub fn as_slice(&self) -> &[T] {
        match &self.storage {
            SmallStorage::Inline(array, len) => &array[..*len],
            SmallStorage::Heap(vec) => vec.as_slice(),
        }
    }

    pub fn as_mut_slice(&mut self) -> &mut [T] {
        match &mut self.storage {
            SmallStorage::Inline(array, len) => &mut array[..*len],
            SmallStorage::Heap(vec) => vec.as_mut_slice(),
        }
    }
}

impl<T, const N: usize> Deref for SmallArray<T, N>
where
    T: Copy,
{
    type Target = [T];

    fn deref(&self) -> &Self::Target {
        self.as_slice()
    }
}

impl<T, const N: usize> DerefMut for SmallArray<T, N>
where
    T: Copy,
{
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.as_mut_slice()
    }
}

impl<T, const N: usize> PartialEq for SmallArray<T, N>
where
    T: Copy + PartialEq,
{
    fn eq(&self, other: &Self) -> bool {
        self.as_slice() == other.as_slice()
    }
}

impl<T, const N: usize> Eq for SmallArray<T, N> where T: Copy + Eq {}

impl<T, const N: usize> fmt::Debug for SmallArray<T, N>
where
    T: Copy + fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_list().entries(self.as_slice().iter()).finish()
    }
}

#[derive(Clone)]
enum SmallStorage<T, const N: usize> {
    Inline([T; N], usize),
    Heap(Vec<T>),
}

#[derive(Clone)]
pub(crate) struct SmallArrayIter<T, const N: usize>
where
    T: Copy,
{
    array: SmallArray<T, N>,
    pos: usize,
}

impl<T, const N: usize> Iterator for SmallArrayIter<T, N>
where
    T: Copy,
{
    type Item = T;

    fn next(&mut self) -> Option<Self::Item> {
        let pos = self.pos;
        self.pos += 1;
        self.array.as_slice().get(pos).copied()
    }
}

impl<T, const N: usize> IntoIterator for SmallArray<T, N>
where
    T: Copy,
{
    type Item = T;
    type IntoIter = SmallArrayIter<T, N>;

    fn into_iter(self) -> Self::IntoIter {
        Self::IntoIter {
            array: self,
            pos: 0,
        }
    }
}

#[cfg(test)]
mod test {
    use super::{SmallArray, SmallStorage};

    #[test]
    fn choose_inline() {
        let arr = SmallArray::<_, 4>::new(0, 4);
        assert!(matches!(arr.storage, SmallStorage::Inline(..)));
        assert_eq!(arr.len(), 4);
    }

    #[test]
    fn choose_heap() {
        let arr = SmallArray::<_, 4>::new(0, 5);
        assert!(matches!(arr.storage, SmallStorage::Heap(..)));
        assert_eq!(arr.len(), 5);
    }

    #[test]
    fn store_and_read_inline() {
        let mut arr = SmallArray::<_, 8>::new(0, 8);
        for (i, value) in arr.as_mut_slice().iter_mut().enumerate() {
            *value = i * 2;
        }
        let expected = [0, 2, 4, 6, 8, 10, 12, 14];
        assert_eq!(arr.as_slice(), &expected);
        assert_eq!(format!("{arr:?}"), format!("{expected:?}"));
    }

    #[test]
    fn store_and_read_heap() {
        let mut arr = SmallArray::<_, 4>::new(0, 8);
        for (i, value) in arr.as_mut_slice().iter_mut().enumerate() {
            *value = i * 2;
        }
        let expected = [0, 2, 4, 6, 8, 10, 12, 14];
        assert_eq!(arr.as_slice(), &expected);
        assert_eq!(format!("{arr:?}"), format!("{expected:?}"));
    }
}

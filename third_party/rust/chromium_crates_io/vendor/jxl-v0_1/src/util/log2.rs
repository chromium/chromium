// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

pub trait FloorLog2 {
    fn floor_log2(&self) -> Self;
}

pub trait CeilLog2 {
    fn ceil_log2(&self) -> Self;
}

impl FloorLog2 for u32 {
    fn floor_log2(&self) -> Self {
        debug_assert_ne!(*self, 0);
        0u32.leading_zeros() - self.leading_zeros() - 1
    }
}

impl FloorLog2 for u64 {
    fn floor_log2(&self) -> Self {
        debug_assert_ne!(*self, 0);
        (0u64.leading_zeros() - self.leading_zeros() - 1) as u64
    }
}

impl FloorLog2 for usize {
    fn floor_log2(&self) -> Self {
        debug_assert_ne!(*self, 0);
        (0usize.leading_zeros() - self.leading_zeros() - 1) as usize
    }
}

impl<T> CeilLog2 for T
where
    T: FloorLog2,
    T: std::ops::Add<Output = Self>,
    T: std::ops::Sub<Output = Self>,
    T: std::ops::BitAnd<Output = Self>,
    T: std::cmp::PartialEq,
    T: From<u8>,
    T: Copy,
{
    fn ceil_log2(&self) -> Self {
        if (*self & (*self - 1.into())) != 0.into() {
            self.floor_log2() + 1.into()
        } else {
            self.floor_log2()
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    #[test]
    fn test_floor() {
        assert_eq!(0, 1u32.floor_log2());
        assert_eq!(1, 2u32.floor_log2());
        assert_eq!(1, 3u32.floor_log2());
        assert_eq!(2, 4u32.floor_log2());
    }
    #[test]
    fn test_ceil() {
        assert_eq!(0, 1u32.ceil_log2());
        assert_eq!(1, 2u32.ceil_log2());
        assert_eq!(2, 3u32.ceil_log2());
        assert_eq!(2, 4u32.ceil_log2());
    }
    #[test]
    fn test_floor_us() {
        assert_eq!(0, 1usize.floor_log2());
        assert_eq!(1, 2usize.floor_log2());
        assert_eq!(1, 3usize.floor_log2());
        assert_eq!(2, 4usize.floor_log2());
    }
    #[test]
    fn test_ceil_us() {
        assert_eq!(0, 1usize.ceil_log2());
        assert_eq!(1, 2usize.ceil_log2());
        assert_eq!(2, 3usize.ceil_log2());
        assert_eq!(2, 4usize.ceil_log2());
    }
}

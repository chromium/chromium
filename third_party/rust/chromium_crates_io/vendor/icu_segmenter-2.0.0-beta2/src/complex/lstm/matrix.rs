// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use alloc::vec;
use alloc::vec::Vec;
use core::ops::Range;
#[allow(unused_imports)]
use core_maths::*;
use zerovec::ule::AsULE;
use zerovec::ZeroSlice;

/// A `D`-dimensional, heap-allocated matrix.
///
/// This matrix implementation supports slicing matrices into tightly-packed
/// submatrices. For example, indexing into a matrix of size 5x4x3 returns a
/// matrix of size 4x3. For more information, see [`MatrixOwned::submatrix`].
#[derive(Debug, Clone)]
pub(super) struct MatrixOwned<const D: usize> {
    data: Vec<f32>,
    dims: [usize; D],
}

impl<const D: usize> MatrixOwned<D> {
    pub(super) fn as_borrowed(&self) -> MatrixBorrowed<D> {
        MatrixBorrowed {
            data: &self.data,
            dims: self.dims,
        }
    }

    pub(super) fn new_zero(dims: [usize; D]) -> Self {
        let total_len = dims.iter().product::<usize>();
        MatrixOwned {
            data: vec![0.0; total_len],
            dims,
        }
    }

    /// Returns the tightly packed submatrix at _index_, or `None` if _index_ is out of range.
    ///
    /// For example, if the matrix is 5x4x3, this function returns a matrix sized 4x3. If the
    /// matrix is 4x3, then this function returns a linear matrix of length 3.
    ///
    /// The type parameter `M` should be `D - 1`.
    #[inline]
    pub(super) fn submatrix<const M: usize>(&self, index: usize) -> Option<MatrixBorrowed<M>> {
        // This assertion is based on const generics; it should always succeed and be elided.
        assert_eq!(M, D - 1);
        let (range, dims) = self.as_borrowed().submatrix_range(index);
        let data = &self.data.get(range)?;
        Some(MatrixBorrowed { data, dims })
    }

    pub(super) fn as_mut(&mut self) -> MatrixBorrowedMut<D> {
        MatrixBorrowedMut {
            data: &mut self.data,
            dims: self.dims,
        }
    }

    /// A mutable version of [`Self::submatrix`].
    #[inline]
    pub(super) fn submatrix_mut<const M: usize>(
        &mut self,
        index: usize,
    ) -> Option<MatrixBorrowedMut<M>> {
        // This assertion is based on const generics; it should always succeed and be elided.
        assert_eq!(M, D - 1);
        let (range, dims) = self.as_borrowed().submatrix_range(index);
        let data = self.data.get_mut(range)?;
        Some(MatrixBorrowedMut { data, dims })
    }
}

/// A `D`-dimensional, borrowed matrix.
#[derive(Debug, Clone, Copy)]
pub(super) struct MatrixBorrowed<'a, const D: usize> {
    data: &'a [f32],
    dims: [usize; D],
}

impl<'a, const D: usize> MatrixBorrowed<'a, D> {
    #[cfg(debug_assertions)]
    pub(super) fn debug_assert_dims(&self, dims: [usize; D]) {
        debug_assert_eq!(dims, self.dims);
        let expected_len = dims.iter().product::<usize>();
        debug_assert_eq!(expected_len, self.data.len());
    }

    pub(super) fn as_slice(&self) -> &'a [f32] {
        self.data
    }

    /// See [`MatrixOwned::submatrix`].
    #[inline]
    pub(super) fn submatrix<const M: usize>(&self, index: usize) -> Option<MatrixBorrowed<'a, M>> {
        // This assertion is based on const generics; it should always succeed and be elided.
        assert_eq!(M, D - 1);
        let (range, dims) = self.submatrix_range(index);
        let data = &self.data.get(range)?;
        Some(MatrixBorrowed { data, dims })
    }

    #[inline]
    fn submatrix_range<const M: usize>(&self, index: usize) -> (Range<usize>, [usize; M]) {
        // This assertion is based on const generics; it should always succeed and be elided.
        assert_eq!(M, D - 1);
        // The above assertion guarantees that the following line will succeed
        #[allow(clippy::indexing_slicing, clippy::unwrap_used)]
        let sub_dims: [usize; M] = self.dims[1..].try_into().unwrap();
        let n = sub_dims.iter().product::<usize>();
        (n * index..n * (index + 1), sub_dims)
    }
}

macro_rules! impl_basic_dim {
    ($t1:path, $t2:path, $t3:path) => {
        impl<'a> $t1 {
            #[allow(dead_code)]
            pub(super) fn dim(&self) -> usize {
                let [dim] = self.dims;
                dim
            }
        }
        impl<'a> $t2 {
            #[allow(dead_code)]
            pub(super) fn dim(&self) -> (usize, usize) {
                let [d0, d1] = self.dims;
                (d0, d1)
            }
        }
        impl<'a> $t3 {
            #[allow(dead_code)]
            pub(super) fn dim(&self) -> (usize, usize, usize) {
                let [d0, d1, d2] = self.dims;
                (d0, d1, d2)
            }
        }
    };
}

impl_basic_dim!(MatrixOwned<1>, MatrixOwned<2>, MatrixOwned<3>);
impl_basic_dim!(
    MatrixBorrowed<'a, 1>,
    MatrixBorrowed<'a, 2>,
    MatrixBorrowed<'a, 3>
);
impl_basic_dim!(
    MatrixBorrowedMut<'a, 1>,
    MatrixBorrowedMut<'a, 2>,
    MatrixBorrowedMut<'a, 3>
);
impl_basic_dim!(MatrixZero<'a, 1>, MatrixZero<'a, 2>, MatrixZero<'a, 3>);

/// A `D`-dimensional, mutably borrowed matrix.
pub(super) struct MatrixBorrowedMut<'a, const D: usize> {
    pub(super) data: &'a mut [f32],
    pub(super) dims: [usize; D],
}

impl<const D: usize> MatrixBorrowedMut<'_, D> {
    pub(super) fn as_borrowed(&self) -> MatrixBorrowed<D> {
        MatrixBorrowed {
            data: self.data,
            dims: self.dims,
        }
    }

    pub(super) fn as_mut_slice(&mut self) -> &mut [f32] {
        self.data
    }

    pub(super) fn copy_submatrix<const M: usize>(&mut self, from: usize, to: usize) {
        let (range_from, _) = self.as_borrowed().submatrix_range::<M>(from);
        let (range_to, _) = self.as_borrowed().submatrix_range::<M>(to);
        if let (Some(_), Some(_)) = (
            self.data.get(range_from.clone()),
            self.data.get(range_to.clone()),
        ) {
            // This function is panicky, but we just validated the ranges
            self.data.copy_within(range_from, range_to.start);
        }
    }

    #[must_use]
    pub(super) fn add(&mut self, other: MatrixZero<'_, D>) -> Option<()> {
        debug_assert_eq!(self.dims, other.dims);
        // TODO: Vectorize?
        for i in 0..self.data.len() {
            *self.data.get_mut(i)? += other.data.get(i)?;
        }
        Some(())
    }

    #[allow(dead_code)] // maybe needed for more complicated bies calculations
    /// Mutates this matrix by applying a softmax transformation.
    pub(super) fn softmax_transform(&mut self) {
        for v in self.data.iter_mut() {
            *v = v.exp();
        }
        let sm = 1.0 / self.data.iter().sum::<f32>();
        for v in self.data.iter_mut() {
            *v *= sm;
        }
    }

    pub(super) fn sigmoid_transform(&mut self) {
        for x in &mut self.data.iter_mut() {
            *x = 1.0 / (1.0 + (-*x).exp());
        }
    }

    pub(super) fn tanh_transform(&mut self) {
        for x in &mut self.data.iter_mut() {
            *x = x.tanh();
        }
    }

    pub(super) fn convolve(
        &mut self,
        i: MatrixBorrowed<'_, D>,
        c: MatrixBorrowed<'_, D>,
        f: MatrixBorrowed<'_, D>,
    ) {
        let i = i.as_slice();
        let c = c.as_slice();
        let f = f.as_slice();
        let len = self.data.len();
        if len != i.len() || len != c.len() || len != f.len() {
            debug_assert!(false, "LSTM matrices not the correct dimensions");
            return;
        }
        for idx in 0..len {
            // Safety: The lengths are all the same (checked above)
            unsafe {
                *self.data.get_unchecked_mut(idx) = i.get_unchecked(idx) * c.get_unchecked(idx)
                    + self.data.get_unchecked(idx) * f.get_unchecked(idx)
            }
        }
    }

    pub(super) fn mul_tanh(&mut self, o: MatrixBorrowed<'_, D>, c: MatrixBorrowed<'_, D>) {
        let o = o.as_slice();
        let c = c.as_slice();
        let len = self.data.len();
        if len != o.len() || len != c.len() {
            debug_assert!(false, "LSTM matrices not the correct dimensions");
            return;
        }
        for idx in 0..len {
            // Safety: The lengths are all the same (checked above)
            unsafe {
                *self.data.get_unchecked_mut(idx) =
                    o.get_unchecked(idx) * c.get_unchecked(idx).tanh();
            }
        }
    }
}

impl MatrixBorrowed<'_, 1> {
    #[allow(dead_code)] // could be useful
    pub(super) fn dot_1d(&self, other: MatrixZero<1>) -> f32 {
        debug_assert_eq!(self.dims, other.dims);
        unrolled_dot_1(self.data, other.data)
    }
}

impl MatrixBorrowedMut<'_, 1> {
    /// Calculate the dot product of a and b, adding the result to self.
    ///
    /// Note: For better dot product efficiency, if `b` is MxN, then `a` should be N;
    /// this is the opposite of standard practice.
    pub(super) fn add_dot_2d(&mut self, a: MatrixBorrowed<1>, b: MatrixZero<2>) {
        let m = a.dim();
        let n = self.as_borrowed().dim();
        debug_assert_eq!(
            m,
            b.dim().1,
            "dims: {:?}/{:?}/{:?}",
            self.as_borrowed().dim(),
            a.dim(),
            b.dim()
        );
        debug_assert_eq!(
            n,
            b.dim().0,
            "dims: {:?}/{:?}/{:?}",
            self.as_borrowed().dim(),
            a.dim(),
            b.dim()
        );
        for i in 0..n {
            if let (Some(dest), Some(b_sub)) = (self.as_mut_slice().get_mut(i), b.submatrix::<1>(i))
            {
                *dest += unrolled_dot_1(a.data, b_sub.data);
            } else {
                debug_assert!(false, "unreachable: dims checked above");
            }
        }
    }
}

impl MatrixBorrowedMut<'_, 2> {
    /// Calculate the dot product of a and b, adding the result to self.
    ///
    /// Self should be _MxN_; `a`, _O_; and `b`, _MxNxO_.
    pub(super) fn add_dot_3d_1(&mut self, a: MatrixBorrowed<1>, b: MatrixZero<3>) {
        let m = a.dim();
        let n = self.as_borrowed().dim().0 * self.as_borrowed().dim().1;
        debug_assert_eq!(
            m,
            b.dim().2,
            "dims: {:?}/{:?}/{:?}",
            self.as_borrowed().dim(),
            a.dim(),
            b.dim()
        );
        debug_assert_eq!(
            n,
            b.dim().0 * b.dim().1,
            "dims: {:?}/{:?}/{:?}",
            self.as_borrowed().dim(),
            a.dim(),
            b.dim()
        );
        // Note: The following two loops are equivalent, but the second has more opportunity for
        // vectorization since it allows the vectorization to span submatrices.
        // for i in 0..b.dim().0 {
        //     self.submatrix_mut::<1>(i).add_dot_2d(a, b.submatrix(i));
        // }
        let lhs = a.as_slice();
        for i in 0..n {
            if let (Some(dest), Some(rhs)) = (
                self.as_mut_slice().get_mut(i),
                b.as_slice().get_subslice(i * m..(i + 1) * m),
            ) {
                *dest += unrolled_dot_1(lhs, rhs);
            } else {
                debug_assert!(false, "unreachable: dims checked above");
            }
        }
    }

    /// Calculate the dot product of a and b, adding the result to self.
    ///
    /// Self should be _MxN_; `a`, _O_; and `b`, _MxNxO_.
    pub(super) fn add_dot_3d_2(&mut self, a: MatrixZero<1>, b: MatrixZero<3>) {
        let m = a.dim();
        let n = self.as_borrowed().dim().0 * self.as_borrowed().dim().1;
        debug_assert_eq!(
            m,
            b.dim().2,
            "dims: {:?}/{:?}/{:?}",
            self.as_borrowed().dim(),
            a.dim(),
            b.dim()
        );
        debug_assert_eq!(
            n,
            b.dim().0 * b.dim().1,
            "dims: {:?}/{:?}/{:?}",
            self.as_borrowed().dim(),
            a.dim(),
            b.dim()
        );
        // Note: The following two loops are equivalent, but the second has more opportunity for
        // vectorization since it allows the vectorization to span submatrices.
        // for i in 0..b.dim().0 {
        //     self.submatrix_mut::<1>(i).add_dot_2d(a, b.submatrix(i));
        // }
        let lhs = a.as_slice();
        for i in 0..n {
            if let (Some(dest), Some(rhs)) = (
                self.as_mut_slice().get_mut(i),
                b.as_slice().get_subslice(i * m..(i + 1) * m),
            ) {
                *dest += unrolled_dot_2(lhs, rhs);
            } else {
                debug_assert!(false, "unreachable: dims checked above");
            }
        }
    }
}

/// A `D`-dimensional matrix borrowed from a [`ZeroSlice`].
#[derive(Debug, Clone, Copy)]
pub(super) struct MatrixZero<'a, const D: usize> {
    data: &'a ZeroSlice<f32>,
    dims: [usize; D],
}

impl<'a> From<&'a crate::provider::LstmMatrix1<'a>> for MatrixZero<'a, 1> {
    fn from(other: &'a crate::provider::LstmMatrix1<'a>) -> Self {
        Self {
            data: &other.data,
            dims: other.dims.map(|x| x as usize),
        }
    }
}

impl<'a> From<&'a crate::provider::LstmMatrix2<'a>> for MatrixZero<'a, 2> {
    fn from(other: &'a crate::provider::LstmMatrix2<'a>) -> Self {
        Self {
            data: &other.data,
            dims: other.dims.map(|x| x as usize),
        }
    }
}

impl<'a> From<&'a crate::provider::LstmMatrix3<'a>> for MatrixZero<'a, 3> {
    fn from(other: &'a crate::provider::LstmMatrix3<'a>) -> Self {
        Self {
            data: &other.data,
            dims: other.dims.map(|x| x as usize),
        }
    }
}

impl<'a, const D: usize> MatrixZero<'a, D> {
    #[allow(clippy::wrong_self_convention)] // same convention as slice::to_vec
    pub(super) fn to_owned(&self) -> MatrixOwned<D> {
        MatrixOwned {
            data: self.data.iter().collect(),
            dims: self.dims,
        }
    }

    pub(super) fn as_slice(&self) -> &ZeroSlice<f32> {
        self.data
    }

    #[cfg(debug_assertions)]
    pub(super) fn debug_assert_dims(&self, dims: [usize; D]) {
        debug_assert_eq!(dims, self.dims);
        let expected_len = dims.iter().product::<usize>();
        debug_assert_eq!(expected_len, self.data.len());
    }

    /// See [`MatrixOwned::submatrix`].
    #[inline]
    pub(super) fn submatrix<const M: usize>(&self, index: usize) -> Option<MatrixZero<'a, M>> {
        // This assertion is based on const generics; it should always succeed and be elided.
        assert_eq!(M, D - 1);
        let (range, dims) = self.submatrix_range(index);
        let data = &self.data.get_subslice(range)?;
        Some(MatrixZero { data, dims })
    }

    #[inline]
    fn submatrix_range<const M: usize>(&self, index: usize) -> (Range<usize>, [usize; M]) {
        // This assertion is based on const generics; it should always succeed and be elided.
        assert_eq!(M, D - 1);
        // The above assertion guarantees that the following line will succeed
        #[allow(clippy::indexing_slicing, clippy::unwrap_used)]
        let sub_dims: [usize; M] = self.dims[1..].try_into().unwrap();
        let n = sub_dims.iter().product::<usize>();
        (n * index..n * (index + 1), sub_dims)
    }
}

macro_rules! f32c {
    ($ule:expr) => {
        f32::from_unaligned($ule)
    };
}

/// Compute the dot product of an aligned and an unaligned f32 slice.
///
/// `xs` and `ys` must be the same length
///
/// (Based on ndarray 0.15.6)
fn unrolled_dot_1(xs: &[f32], ys: &ZeroSlice<f32>) -> f32 {
    debug_assert_eq!(xs.len(), ys.len());
    // eightfold unrolled so that floating point can be vectorized
    // (even with strict floating point accuracy semantics)
    let mut p = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    let xit = xs.chunks_exact(8);
    let yit = ys.as_ule_slice().chunks_exact(8);
    let sum = xit
        .remainder()
        .iter()
        .zip(yit.remainder().iter())
        .map(|(x, y)| x * f32c!(*y))
        .sum::<f32>();
    for (xx, yy) in xit.zip(yit) {
        // TODO: Use array_chunks once stable to avoid the unwrap.
        // <https://github.com/rust-lang/rust/issues/74985>
        #[allow(clippy::unwrap_used)]
        let [x0, x1, x2, x3, x4, x5, x6, x7] = *<&[f32; 8]>::try_from(xx).unwrap();
        #[allow(clippy::unwrap_used)]
        let [y0, y1, y2, y3, y4, y5, y6, y7] = *<&[<f32 as AsULE>::ULE; 8]>::try_from(yy).unwrap();
        p.0 += x0 * f32c!(y0);
        p.1 += x1 * f32c!(y1);
        p.2 += x2 * f32c!(y2);
        p.3 += x3 * f32c!(y3);
        p.4 += x4 * f32c!(y4);
        p.5 += x5 * f32c!(y5);
        p.6 += x6 * f32c!(y6);
        p.7 += x7 * f32c!(y7);
    }
    sum + (p.0 + p.4) + (p.1 + p.5) + (p.2 + p.6) + (p.3 + p.7)
}

/// Compute the dot product of two unaligned f32 slices.
///
/// `xs` and `ys` must be the same length
///
/// (Based on ndarray 0.15.6)
fn unrolled_dot_2(xs: &ZeroSlice<f32>, ys: &ZeroSlice<f32>) -> f32 {
    debug_assert_eq!(xs.len(), ys.len());
    // eightfold unrolled so that floating point can be vectorized
    // (even with strict floating point accuracy semantics)
    let mut p = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    let xit = xs.as_ule_slice().chunks_exact(8);
    let yit = ys.as_ule_slice().chunks_exact(8);
    let sum = xit
        .remainder()
        .iter()
        .zip(yit.remainder().iter())
        .map(|(x, y)| f32c!(*x) * f32c!(*y))
        .sum::<f32>();
    for (xx, yy) in xit.zip(yit) {
        // TODO: Use array_chunks once stable to avoid the unwrap.
        // <https://github.com/rust-lang/rust/issues/74985>
        #[allow(clippy::unwrap_used)]
        let [x0, x1, x2, x3, x4, x5, x6, x7] = *<&[<f32 as AsULE>::ULE; 8]>::try_from(xx).unwrap();
        #[allow(clippy::unwrap_used)]
        let [y0, y1, y2, y3, y4, y5, y6, y7] = *<&[<f32 as AsULE>::ULE; 8]>::try_from(yy).unwrap();
        p.0 += f32c!(x0) * f32c!(y0);
        p.1 += f32c!(x1) * f32c!(y1);
        p.2 += f32c!(x2) * f32c!(y2);
        p.3 += f32c!(x3) * f32c!(y3);
        p.4 += f32c!(x4) * f32c!(y4);
        p.5 += f32c!(x5) * f32c!(y5);
        p.6 += f32c!(x6) * f32c!(y6);
        p.7 += f32c!(x7) * f32c!(y7);
    }
    sum + (p.0 + p.4) + (p.1 + p.5) + (p.2 + p.6) + (p.3 + p.7)
}

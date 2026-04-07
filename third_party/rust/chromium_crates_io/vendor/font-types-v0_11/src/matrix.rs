//! Generic affine matrix type.

use super::{Fixed, Point};
use core::ops::{Add, Mul, MulAssign};

/// A transformation matrix.
///
/// This is a column-major 3x2 affine matrix with the following
/// representation:
///
/// ```none
/// | xx xy dx |
/// | yx yy dy |
/// ```
/// with the basis vectors `(xx, yx)`, `(xy, yy)` and the translation
/// `(dx, dy)`.
///
/// Transformation of the point `(x, y)` is applied as follows:
/// ```none
/// x' = xx * x + xy * y + dx
/// y' = yx * x + yy * y + dy
/// ```
///
/// Our matrices are typically considered to be _right-handed_, meaning that
/// positive angles apply counter-clockwise rotations and the rotation matrix
/// for angle a is:
/// ```none
/// | cos(a) -sin(a) 0 |
/// | sin(a)  cos(a) 0 |
/// ```
///
/// Matrix multiplication chains transforms in _right to left_ order meaning
/// that for `C = A * B`, matrix `C` will apply `B` and then `A`.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
#[cfg_attr(feature = "serde", derive(serde::Serialize, serde::Deserialize))]
#[cfg_attr(feature = "bytemuck", derive(bytemuck::AnyBitPattern))]
#[repr(C)]
pub struct Matrix<T> {
    pub xx: T,
    pub yx: T,
    pub xy: T,
    pub yy: T,
    pub dx: T,
    pub dy: T,
}

impl<T: MatrixElement> Default for Matrix<T> {
    fn default() -> Self {
        Self::IDENTITY
    }
}

impl<T: Copy> Matrix<T> {
    /// Creates a new matrix from an array of elements in the order
    /// `[xx, yx, xy, yy, dx, dy]`.
    pub const fn from_elements(elements: [T; 6]) -> Self {
        Self {
            xx: elements[0],
            yx: elements[1],
            xy: elements[2],
            yy: elements[3],
            dx: elements[4],
            dy: elements[5],
        }
    }

    /// Returns the elements as an array in the order
    /// `[xx, yx, xy, yy, dx, dy]`.
    pub const fn elements(&self) -> [T; 6] {
        [self.xx, self.yx, self.xy, self.yy, self.dx, self.dy]
    }

    /// Maps `Matrix<T>` to `Matrix<U>` by applying a function to each element.
    #[inline(always)]
    pub fn map<U: Copy>(self, f: impl FnMut(T) -> U) -> Matrix<U> {
        Matrix::from_elements(self.elements().map(f))
    }
}

impl<T: MatrixElement> Matrix<T> {
    /// The identity matrix.
    pub const IDENTITY: Self = Self {
        xx: T::ONE,
        yx: T::ZERO,
        xy: T::ZERO,
        yy: T::ONE,
        dx: T::ZERO,
        dy: T::ZERO,
    };

    /// Applies the matrix to the given coordinates.
    pub fn transform(&self, x: T, y: T) -> (T, T) {
        (
            self.xx * x + self.xy * y + self.dx,
            self.yx * x + self.yy * y + self.dy,
        )
    }
}

impl<T: MatrixElement> Mul for Matrix<T> {
    type Output = Self;

    fn mul(self, rhs: Self) -> Self::Output {
        Self {
            xx: self.xx * rhs.xx + self.xy * rhs.yx,
            yx: self.yx * rhs.xx + self.yy * rhs.yx,
            xy: self.xx * rhs.xy + self.xy * rhs.yy,
            yy: self.yx * rhs.xy + self.yy * rhs.yy,
            dx: self.xx * rhs.dx + self.xy * rhs.dy + self.dx,
            dy: self.yx * rhs.dx + self.yy * rhs.dy + self.dy,
        }
    }
}

impl<T: MatrixElement> MulAssign for Matrix<T> {
    fn mul_assign(&mut self, rhs: Self) {
        *self = *self * rhs;
    }
}

impl<T: MatrixElement> Mul<Point<T>> for Matrix<T> {
    type Output = Point<T>;

    fn mul(self, rhs: Point<T>) -> Self::Output {
        let (x, y) = self.transform(rhs.x, rhs.y);
        Point::new(x, y)
    }
}

/// Trait for types that can be used as matrix elements.
pub trait MatrixElement: Copy + Add<Output = Self> + Mul<Output = Self> {
    const ZERO: Self;
    const ONE: Self;
}

impl MatrixElement for Fixed {
    const ONE: Self = Fixed::ONE;
    const ZERO: Self = Fixed::ZERO;
}

impl MatrixElement for f32 {
    const ONE: Self = 1.0;
    const ZERO: Self = 0.0;
}

impl MatrixElement for f64 {
    const ONE: Self = 1.0;
    const ZERO: Self = 0.0;
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn mul_matrix_identity_and_known_product() {
        let a = Matrix::from_elements([0.5f32, 1.0, -2.0, 0.25, 7.0, -3.0]);
        assert_eq!(Matrix::IDENTITY * a, a);
        assert_eq!(a * Matrix::IDENTITY, a);
        let translate = Matrix::from_elements([1.0, 0.0, 0.0, 1.0, 10.0, 20.0]);
        let scale = Matrix::from_elements([2.0, 0.0, 0.0, 3.0, 0.0, 0.0]);
        assert_eq!(
            (translate * scale).elements(),
            [2.0, 0.0, 0.0, 3.0, 10.0, 20.0]
        );
        assert_eq!(
            (scale * translate).elements(),
            [2.0, 0.0, 0.0, 3.0, 20.0, 60.0]
        );
    }

    #[test]
    fn transform_points() {
        let translate = Matrix::from_elements([1.0f32, 0.0, 0.0, 1.0, 10.0, 20.0]);
        let scale = Matrix::from_elements([2.0, 0.0, 0.0, 3.0, 0.0, 0.0]);
        let translate_scale = translate * scale;
        let scale_translate = scale * translate;
        let p = Point::new(5.0, -22.0);
        assert_eq!(translate * p, Point::new(15.0, -2.0));
        assert_eq!(scale * p, Point::new(10.0, -66.0));
        assert_eq!(translate_scale * p, Point::new(20.0, -46.0));
        assert_eq!(scale_translate * p, Point::new(30.0, -6.0));
    }
}

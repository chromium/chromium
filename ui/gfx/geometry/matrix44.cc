// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/geometry/matrix44.h"

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <utility>

#include "ui/gfx/geometry/decomposed_transform.h"

namespace gfx {

namespace {

ALWAYS_INLINE Double4 SwapHighLow(Double4 v) {
  return Double4{v[2], v[3], v[0], v[1]};
}

ALWAYS_INLINE Double4 SwapInPairs(Double4 v) {
  return Double4{v[1], v[0], v[3], v[2]};
}

// This is based on
// https://github.com/niswegmann/small-matrix-inverse/blob/master/invert4x4_llvm.h,
// which is based on Intel AP-928 "Streaming SIMD Extensions - Inverse of 4x4
// Matrix": https://drive.google.com/file/d/0B9rh9tVI0J5mX1RUam5nZm85OFE/view.
ALWAYS_INLINE bool InverseWithDouble4Cols(Double4& c0,
                                          Double4& c1,
                                          Double4& c2,
                                          Double4& c3) {
  // Note that r1 and r3 have components 2/3 and 0/1 swapped.
  Double4 r0 = {c0[0], c1[0], c2[0], c3[0]};
  Double4 r1 = {c2[1], c3[1], c0[1], c1[1]};
  Double4 r2 = {c0[2], c1[2], c2[2], c3[2]};
  Double4 r3 = {c2[3], c3[3], c0[3], c1[3]};

  Double4 t = SwapInPairs(r2 * r3);
  c0 = r1 * t;
  c1 = r0 * t;

  t = SwapHighLow(t);
  c0 = r1 * t - c0;
  c1 = SwapHighLow(r0 * t - c1);

  t = SwapInPairs(r1 * r2);
  c0 += r3 * t;
  c3 = r0 * t;

  t = SwapHighLow(t);
  c0 -= r3 * t;
  c3 = SwapHighLow(r0 * t - c3);

  t = SwapInPairs(SwapHighLow(r1) * r3);
  r2 = SwapHighLow(r2);
  c0 += r2 * t;
  c2 = r0 * t;

  t = SwapHighLow(t);
  c0 -= r2 * t;

  double det = Sum(r0 * c0);
  if (!std::isnormal(static_cast<float>(det)))
    return false;

  c2 = SwapHighLow(r0 * t - c2);

  t = SwapInPairs(r0 * r1);
  c2 = r3 * t + c2;
  c3 = r2 * t - c3;

  t = SwapHighLow(t);
  c2 = r3 * t - c2;
  c3 -= r2 * t;

  t = SwapInPairs(r0 * r3);
  c1 -= r2 * t;
  c2 = r1 * t + c2;

  t = SwapHighLow(t);
  c1 = r2 * t + c1;
  c2 -= r1 * t;

  t = SwapInPairs(r0 * r2);
  c1 = r3 * t + c1;
  c3 -= r1 * t;

  t = SwapHighLow(t);
  c1 -= r3 * t;
  c3 = r1 * t + c3;

  det = 1.0 / det;
  c0 *= det;
  c1 *= det;
  c2 *= det;
  c3 *= det;
  return true;
}

}  // anonymous namespace

void Matrix44::GetColMajor(double dst[16]) const {
  const double* src = &matrix_[0][0];
  std::copy(src, src + 16, dst);
}

void Matrix44::GetColMajorF(float dst[16]) const {
  const double* src = &matrix_[0][0];
  std::copy(src, src + 16, dst);
}

void Matrix44::PreTranslate(double dx, double dy) {
  SetCol(3, Col(0) * dx + Col(1) * dy + Col(3));
}

void Matrix44::PreTranslate3d(double dx, double dy, double dz) {
  if (AllTrue(Double4{dx, dy, dz, 0} == Double4{0, 0, 0, 0}))
    return;

  SetCol(3, Col(0) * dx + Col(1) * dy + Col(2) * dz + Col(3));
}

void Matrix44::PostTranslate(double dx, double dy) {
  if (!HasPerspective()) [[likely]] {
    matrix_[3][0] += dx;
    matrix_[3][1] += dy;
  } else {
    if (dx != 0) {
      matrix_[0][0] += matrix_[0][3] * dx;
      matrix_[1][0] += matrix_[1][3] * dx;
      matrix_[2][0] += matrix_[2][3] * dx;
      matrix_[3][0] += matrix_[3][3] * dx;
    }
    if (dy != 0) {
      matrix_[0][1] += matrix_[0][3] * dy;
      matrix_[1][1] += matrix_[1][3] * dy;
      matrix_[2][1] += matrix_[2][3] * dy;
      matrix_[3][1] += matrix_[3][3] * dy;
    }
  }
}

void Matrix44::PostTranslate3d(double dx, double dy, double dz) {
  Double4 t{dx, dy, dz, 0};
  if (AllTrue(t == Double4{0, 0, 0, 0}))
    return;

  if (!HasPerspective()) [[likely]] {
    SetCol(3, Col(3) + t);
  } else {
    for (int i = 0; i < 4; ++i)
      SetCol(i, Col(i) + t * matrix_[i][3]);
  }
}

void Matrix44::PreScale(double sx, double sy) {
  SetCol(0, Col(0) * sx);
  SetCol(1, Col(1) * sy);
}

void Matrix44::PreScale3d(double sx, double sy, double sz) {
  if (AllTrue(Double4{sx, sy, sz, 1} == Double4{1, 1, 1, 1}))
    return;

  SetCol(0, Col(0) * sx);
  SetCol(1, Col(1) * sy);
  SetCol(2, Col(2) * sz);
}

void Matrix44::PostScale(double sx, double sy) {
  if (sx != 1) {
    matrix_[0][0] *= sx;
    matrix_[1][0] *= sx;
    matrix_[2][0] *= sx;
    matrix_[3][0] *= sx;
  }
  if (sy != 1) {
    matrix_[0][1] *= sy;
    matrix_[1][1] *= sy;
    matrix_[2][1] *= sy;
    matrix_[3][1] *= sy;
  }
}

void Matrix44::PostScale3d(double sx, double sy, double sz) {
  if (AllTrue(Double4{sx, sy, sz, 1} == Double4{1, 1, 1, 1}))
    return;

  Double4 s{sx, sy, sz, 1};
  for (int i = 0; i < 4; i++)
    SetCol(i, Col(i) * s);
}

void Matrix44::RotateUnitSinCos(double x,
                                double y,
                                double z,
                                double sin_angle,
                                double cos_angle) {
  // Optimize cases where the axis is along a major axis. Since we've already
  // normalized the vector we don't need to check that the other two dimensions
  // are zero. Tiny errors of the other two dimensions are ignored.
  if (z == 1.0) {
    RotateAboutZAxisSinCos(sin_angle, cos_angle);
    return;
  }
  if (y == 1.0) {
    RotateAboutYAxisSinCos(sin_angle, cos_angle);
    return;
  }
  if (x == 1.0) {
    RotateAboutXAxisSinCos(sin_angle, cos_angle);
    return;
  }

  double c = cos_angle;
  double s = sin_angle;
  double C = 1 - c;
  double xs = x * s;
  double ys = y * s;
  double zs = z * s;
  double xC = x * C;
  double yC = y * C;
  double zC = z * C;
  double xyC = x * yC;
  double yzC = y * zC;
  double zxC = z * xC;

  PreConcat(Matrix44(x * xC + c, xyC + zs, zxC - ys, 0,  // col 0
                     xyC - zs, y * yC + c, yzC + xs, 0,  // col 1
                     zxC + ys, yzC - xs, z * zC + c, 0,  // col 2
                     0, 0, 0, 1));                       // col 3
}

void Matrix44::RotateAboutXAxisSinCos(double sin_angle, double cos_angle) {
  Double4 c1 = Col(1);
  Double4 c2 = Col(2);
  SetCol(1, c1 * cos_angle + c2 * sin_angle);
  SetCol(2, c2 * cos_angle - c1 * sin_angle);
}

void Matrix44::RotateAboutYAxisSinCos(double sin_angle, double cos_angle) {
  Double4 c0 = Col(0);
  Double4 c2 = Col(2);
  SetCol(0, c0 * cos_angle - c2 * sin_angle);
  SetCol(2, c2 * cos_angle + c0 * sin_angle);
}

void Matrix44::RotateAboutZAxisSinCos(double sin_angle, double cos_angle) {
  Double4 c0 = Col(0);
  Double4 c1 = Col(1);
  SetCol(0, c0 * cos_angle + c1 * sin_angle);
  SetCol(1, c1 * cos_angle - c0 * sin_angle);
}

void Matrix44::Skew(double tan_skew_x, double tan_skew_y) {
  Double4 c0 = Col(0);
  Double4 c1 = Col(1);
  SetCol(0, c0 + c1 * tan_skew_y);
  SetCol(1, c1 + c0 * tan_skew_x);
}

void Matrix44::ApplyDecomposedSkews(const double skews[3]) {
  Double4 c0 = Col(0);
  Double4 c1 = Col(1);
  Double4 c2 = Col(2);
  //                  / |1 0 0  0|   |1 0 s1 0|   |1 s0 0 0|   |1 s0 s1 0| \
  // |c0 c1 c2 c3| * |  |0 1 s2 0| * |0 1  0 0| * |0  1 0 0| = |0  1 s2 0|  |
  //                 |  |0 0 1  0|   |0 0  1 0|   |0  0 1 0|   |0  0  1 0|  |
  //                  \ |0 0 0  1|   |0 0  0 1|   |0  0 0 1|   |0  0  0 1| /
  SetCol(1, c1 + c0 * skews[0]);
  SetCol(2, c0 * skews[1] + c1 * skews[2] + c2);
}

void Matrix44::ApplyPerspectiveDepth(double perspective) {
  DCHECK_NE(perspective, 0.0);
  SetCol(2, Col(2) + Col(3) * (-1.0 / perspective));
}

void Matrix44::SetConcat(const Matrix44& x, const Matrix44& y) {
  if (x.Is2dTransform() && y.Is2dTransform()) {
    double a = x.matrix_[0][0];
    double b = x.matrix_[0][1];
    double c = x.matrix_[1][0];
    double d = x.matrix_[1][1];
    double e = x.matrix_[3][0];
    double f = x.matrix_[3][1];
    double ya = y.matrix_[0][0];
    double yb = y.matrix_[0][1];
    double yc = y.matrix_[1][0];
    double yd = y.matrix_[1][1];
    double ye = y.matrix_[3][0];
    double yf = y.matrix_[3][1];
    *this = Matrix44(a * ya + c * yb, b * ya + d * yb, 0, 0,           // col 0
                     a * yc + c * yd, b * yc + d * yd, 0, 0,           // col 1
                     0, 0, 1, 0,                                       // col 2
                     a * ye + c * yf + e, b * ye + d * yf + f, 0, 1);  // col 3
    return;
  }

  auto c0 = x.Col(0);
  auto c1 = x.Col(1);
  auto c2 = x.Col(2);
  auto c3 = x.Col(3);

  auto mc0 = y.Col(0);
  auto mc1 = y.Col(1);
  auto mc2 = y.Col(2);
  auto mc3 = y.Col(3);

  SetCol(0, c0 * mc0[0] + c1 * mc0[1] + c2 * mc0[2] + c3 * mc0[3]);
  SetCol(1, c0 * mc1[0] + c1 * mc1[1] + c2 * mc1[2] + c3 * mc1[3]);
  SetCol(2, c0 * mc2[0] + c1 * mc2[1] + c2 * mc2[2] + c3 * mc2[3]);
  SetCol(3, c0 * mc3[0] + c1 * mc3[1] + c2 * mc3[2] + c3 * mc3[3]);
}

bool Matrix44::GetInverse(Matrix44& result) const {
  if (Is2dTransform()) {
    double determinant = Determinant();
    if (!std::isnormal(static_cast<float>(determinant)))
      return false;

    double inv_det = 1.0 / determinant;
    double a = matrix_[0][0];
    double b = matrix_[0][1];
    double c = matrix_[1][0];
    double d = matrix_[1][1];
    double e = matrix_[3][0];
    double f = matrix_[3][1];
    result = Matrix44(d * inv_det, -b * inv_det, 0, 0,  // col 0
                      -c * inv_det, a * inv_det, 0, 0,  // col 1
                      0, 0, 1, 0,                       // col 2
                      (c * f - d * e) * inv_det, (b * e - a * f) * inv_det, 0,
                      1);  // col 3
    return true;
  }

  Double4 c0 = Col(0);
  Double4 c1 = Col(1);
  Double4 c2 = Col(2);
  Double4 c3 = Col(3);

  if (!InverseWithDouble4Cols(c0, c1, c2, c3))
    return false;

  result.SetCol(0, c0);
  result.SetCol(1, c1);
  result.SetCol(2, c2);
  result.SetCol(3, c3);
  return true;
}

bool Matrix44::IsInvertible() const {
  return std::isnormal(static_cast<float>(Determinant()));
}

// This is a simplified version of InverseWithDouble4Cols().
double Matrix44::Determinant() const {
  if (Is2dTransform())
    return matrix_[0][0] * matrix_[1][1] - matrix_[0][1] * matrix_[1][0];

  Double4 c0 = Col(0);
  Double4 c1 = Col(1);
  Double4 c2 = Col(2);
  Double4 c3 = Col(3);

  // Note that r1 and r3 have components 2/3 and 0/1 swapped.
  Double4 r0 = {c0[0], c1[0], c2[0], c3[0]};
  Double4 r1 = {c2[1], c3[1], c0[1], c1[1]};
  Double4 r2 = {c0[2], c1[2], c2[2], c3[2]};
  Double4 r3 = {c2[3], c3[3], c0[3], c1[3]};

  Double4 t = SwapInPairs(r2 * r3);
  c0 = r1 * t;
  t = SwapHighLow(t);
  c0 = r1 * t - c0;
  t = SwapInPairs(r1 * r2);
  c0 += r3 * t;
  t = SwapHighLow(t);
  c0 -= r3 * t;
  t = SwapInPairs(SwapHighLow(r1) * r3);
  r2 = SwapHighLow(r2);
  c0 += r2 * t;
  t = SwapHighLow(t);
  c0 -= r2 * t;

  return Sum(r0 * c0);
}

void Matrix44::Transpose() {
  using std::swap;
  swap(matrix_[0][1], matrix_[1][0]);
  swap(matrix_[0][2], matrix_[2][0]);
  swap(matrix_[0][3], matrix_[3][0]);
  swap(matrix_[1][2], matrix_[2][1]);
  swap(matrix_[1][3], matrix_[3][1]);
  swap(matrix_[2][3], matrix_[3][2]);
}

void Matrix44::Zoom(double zoom_factor) {
  matrix_[0][3] /= zoom_factor;
  matrix_[1][3] /= zoom_factor;
  matrix_[2][3] /= zoom_factor;
  matrix_[3][0] *= zoom_factor;
  matrix_[3][1] *= zoom_factor;
  matrix_[3][2] *= zoom_factor;
}

double Matrix44::MapVector2(double vec[2]) const {
  double v0 = vec[0];
  double v1 = vec[1];
  double x = v0 * matrix_[0][0] + v1 * matrix_[1][0] + matrix_[3][0];
  double y = v0 * matrix_[0][1] + v1 * matrix_[1][1] + matrix_[3][1];
  double w = v0 * matrix_[0][3] + v1 * matrix_[1][3] + matrix_[3][3];
  vec[0] = x;
  vec[1] = y;
  return w;
}

void Matrix44::MapVector4(double vec[4]) const {
  Double4 v = LoadDouble4(vec);
  Double4 r0{matrix_[0][0], matrix_[1][0], matrix_[2][0], matrix_[3][0]};
  Double4 r1{matrix_[0][1], matrix_[1][1], matrix_[2][1], matrix_[3][1]};
  Double4 r2{matrix_[0][2], matrix_[1][2], matrix_[2][2], matrix_[3][2]};
  Double4 r3{matrix_[0][3], matrix_[1][3], matrix_[2][3], matrix_[3][3]};
  StoreDouble4(Double4{Sum(r0 * v), Sum(r1 * v), Sum(r2 * v), Sum(r3 * v)},
               vec);
}

void Matrix44::Flatten() {
  matrix_[0][2] = 0;
  matrix_[1][2] = 0;
  matrix_[3][2] = 0;
  SetCol(2, Double4{0, 0, 1, 0});
}

// TODO(crbug.com/40237414): Consider letting this function always succeed.
std::optional<DecomposedTransform> Matrix44::Decompose2d() const {
  DCHECK(Is2dTransform());

  // https://www.w3.org/TR/css-transforms-1/#decomposing-a-2d-matrix.
  // Decompose a 2D transformation matrix of the form:
  // [m11 m21 0 m41]
  // [m12 m22 0 m42]
  // [ 0   0  1  0 ]
  // [ 0   0  0  1 ]
  //
  // The decomposition is of the form:
  // M = translate * rotate * skew * scale
  //     [1 0 0 Tx] [cos(R) -sin(R) 0 0] [1 K 0 0] [Sx 0  0 0]
  //   = [0 1 0 Ty] [sin(R)  cos(R) 0 0] [0 1 0 0] [0  Sy 0 0]
  //     [0 0 1 0 ] [  0       0    1 0] [0 0 1 0] [0  0  1 0]
  //     [0 0 0 1 ] [  0       0    0 1] [0 0 0 1] [0  0  0 1]

  double m11 = matrix_[0][0];
  double m21 = matrix_[1][0];
  double m12 = matrix_[0][1];
  double m22 = matrix_[1][1];

  double determinant = m11 * m22 - m12 * m21;
  // Test for matrix being singular.
  if (determinant == 0)
    return std::nullopt;

  DecomposedTransform decomp;

  // Translation transform.
  // [m11 m21 0 m41]    [1 0 0 Tx] [m11 m21 0 0]
  // [m12 m22 0 m42]  = [0 1 0 Ty] [m12 m22 0 0]
  // [ 0   0  1  0 ]    [0 0 1 0 ] [ 0   0  1 0]
  // [ 0   0  0  1 ]    [0 0 0 1 ] [ 0   0  0 1]
  decomp.translate[0] = matrix_[3][0];
  decomp.translate[1] = matrix_[3][1];

  // For the remainder of the decomposition process, we can focus on the upper
  // 2x2 submatrix
  // [m11 m21] = [cos(R) -sin(R)] [1 K] [Sx 0 ]
  // [m12 m22]   [sin(R)  cos(R)] [0 1] [0  Sy]
  //           = [Sx*cos(R) Sy*(K*cos(R) - sin(R))]
  //             [Sx*sin(R) Sy*(K*sin(R) + cos(R))]

  // Determine sign of the x and y scale.
  if (determinant < 0) {
    // If the determinant is negative, we need to flip either the x or y scale.
    // Flipping both is equivalent to rotating by 180 degrees.
    if (m11 < m22) {
      decomp.scale[0] *= -1;
    } else {
      decomp.scale[1] *= -1;
    }
  }

  // X Scale.
  // m11^2 + m12^2 = Sx^2*(cos^2(R) + sin^2(R)) = Sx^2.
  // Sx = +/-sqrt(m11^2 + m22^2)
  decomp.scale[0] *= sqrt(m11 * m11 + m12 * m12);
  m11 /= decomp.scale[0];
  m12 /= decomp.scale[0];

  // Post normalization, the submatrix is now of the form:
  // [m11 m21] = [cos(R)  Sy*(K*cos(R) - sin(R))]
  // [m12 m22]   [sin(R)  Sy*(K*sin(R) + cos(R))]

  // XY Shear.
  // m11 * m21 + m12 * m22 = Sy*K*cos^2(R) - Sy*sin(R)*cos(R) +
  //                         Sy*K*sin^2(R) + Sy*cos(R)*sin(R)
  //                       = Sy*K
  double scaled_shear = m11 * m21 + m12 * m22;
  m21 -= m11 * scaled_shear;
  m22 -= m12 * scaled_shear;

  // Post normalization, the submatrix is now of the form:
  // [m11 m21] = [cos(R)  -Sy*sin(R)]
  // [m12 m22]   [sin(R)   Sy*cos(R)]

  // Y Scale.
  // Similar process to determining x-scale.
  decomp.scale[1] *= sqrt(m21 * m21 + m22 * m22);
  m21 /= decomp.scale[1];
  m22 /= decomp.scale[1];
  decomp.skew[0] = scaled_shear / decomp.scale[1];

  // Rotation transform.
  // [1-2(yy+zz)  2(xy-zw)    2(xz+yw) ]   [cos(R) -sin(R)  0]
  // [2(xy+zw)   1-2(xx+zz)   2(yz-xw) ] = [sin(R)  cos(R)  0]
  // [2(xz-yw)    2*(yz+xw)  1-2(xx+yy)]   [  0       0     1]
  // Comparing terms, we can conclude that x = y = 0.
  // [1-2zz   -2zw  0]   [cos(R) -sin(R)  0]
  // [ 2zw   1-2zz  0] = [sin(R)  cos(R)  0]
  // [  0     0     1]   [  0       0     1]
  // cos(R) = 1 - 2*z^2
  // From the double angle formula: cos(2a) = 1 - 2 sin(a)^2
  // cos(R) = 1 - 2*sin(R/2)^2 = 1 - 2*z^2 ==> z = sin(R/2)
  // sin(R) = 2*z*w
  // But sin(2a) = 2 sin(a) cos(a)
  // sin(R) = 2 sin(R/2) cos(R/2) = 2*z*w ==> w = cos(R/2)
  double angle = std::atan2(m12, m11);
  decomp.quaternion.set_x(0);
  decomp.quaternion.set_y(0);
  decomp.quaternion.set_z(std::sin(0.5 * angle));
  decomp.quaternion.set_w(std::cos(0.5 * angle));

  return decomp;
}

std::optional<DecomposedTransform> Matrix44::Decompose() const {
  // See documentation of Transform::Decompose() for why we need the 2d branch.
  if (Is2dTransform())
    return Decompose2d();

  // https://www.w3.org/TR/css-transforms-2/#decomposing-a-3d-matrix.

  Double4 c0 = Col(0);
  Double4 c1 = Col(1);
  Double4 c2 = Col(2);
  Double4 c3 = Col(3);

  // Normalize the matrix.
  if (!std::isnormal(c3[3]))
    return std::nullopt;

  double inv_w = 1.0 / c3[3];
  c0 *= inv_w;
  c1 *= inv_w;
  c2 *= inv_w;
  c3 *= inv_w;

  Double4 perspective = {c0[3], c1[3], c2[3], 1.0};
  // Clear the perspective partition.
  c0[3] = c1[3] = c2[3] = 0;
  c3[3] = 1;

  Double4 inverse_c0 = c0;
  Double4 inverse_c1 = c1;
  Double4 inverse_c2 = c2;
  Double4 inverse_c3 = c3;
  if (!InverseWithDouble4Cols(inverse_c0, inverse_c1, inverse_c2, inverse_c3))
    return std::nullopt;

  DecomposedTransform decomp;

  // First, isolate perspective.
  if (!AllTrue(perspective == Double4{0, 0, 0, 1})) {
    // Solve the equation by multiplying perspective by the inverse.
    decomp.perspective[0] = gfx::Sum(perspective * inverse_c0);
    decomp.perspective[1] = gfx::Sum(perspective * inverse_c1);
    decomp.perspective[2] = gfx::Sum(perspective * inverse_c2);
    decomp.perspective[3] = gfx::Sum(perspective * inverse_c3);
  }

  // Next take care of translation (easy).
  decomp.translate[0] = c3[0];
  c3[0] = 0;
  decomp.translate[1] = c3[1];
  c3[1] = 0;
  decomp.translate[2] = c3[2];
  c3[2] = 0;

  // Note: Deviating from the spec in terms of variable naming. The matrix is
  // stored on column major order and not row major. Using the variable 'row'
  // instead of 'column' in the spec pseudocode has been the source of
  // confusion, specifically in sorting out rotations.

  // From now on, only the first 3 components of the Double4 column is used.
  auto sum3 = [](Double4 c) -> double { return c[0] + c[1] + c[2]; };
  auto extract_scale = [&sum3](Double4& c, double& scale) -> bool {
    scale = std::sqrt(sum3(c * c));
    if (!std::isnormal(scale))
      return false;
    c *= 1.0 / scale;
    return true;
  };
  auto epsilon_to_zero = [](double d) -> double {
    return std::abs(d) < std::numeric_limits<float>::epsilon() ? 0 : d;
  };

  // Compute X scale factor and normalize the first column.
  if (!extract_scale(c0, decomp.scale[0]))
    return std::nullopt;

  // Compute XY shear factor and make 2nd column orthogonal to 1st.
  decomp.skew[0] = epsilon_to_zero(sum3(c0 * c1));
  c1 -= c0 * decomp.skew[0];

  // Now, compute Y scale and normalize 2nd column.
  if (!extract_scale(c1, decomp.scale[1]))
    return std::nullopt;

  decomp.skew[0] /= decomp.scale[1];

  // Compute XZ and YZ shears, and orthogonalize the 3rd column.
  decomp.skew[1] = epsilon_to_zero(sum3(c0 * c2));
  c2 -= c0 * decomp.skew[1];
  decomp.skew[2] = epsilon_to_zero(sum3(c1 * c2));
  c2 -= c1 * decomp.skew[2];

  // Next, get Z scale and normalize the 3rd column.
  if (!extract_scale(c2, decomp.scale[2]))
    return std::nullopt;

  decomp.skew[1] /= decomp.scale[2];
  decomp.skew[2] /= decomp.scale[2];

  // At this point, the matrix is orthonormal.
  // Check for a coordinate system flip.  If the determinant is -1, then negate
  // the matrix and the scaling factors.
  auto cross3 = [](Double4 a, Double4 b) -> Double4 {
    return Double4{a[1], a[2], a[0], a[3]} * Double4{b[2], b[0], b[1], b[3]} -
           Double4{a[2], a[0], a[1], a[3]} * Double4{b[1], b[2], b[0], b[3]};
  };
  Double4 pdum3 = cross3(c1, c2);
  if (sum3(c0 * pdum3) < 0) {
    // Flip all 3 scaling factors, following the 3d decomposition spec. See
    // documentation of Transform::Decompose() about the difference between
    // the 2d spec and and 3d spec about scale flipping.
    decomp.scale[0] *= -1;
    decomp.scale[1] *= -1;
    decomp.scale[2] *= -1;
    c0 *= -1;
    c1 *= -1;
    c2 *= -1;
  }

  // Lastly, compute the quaternions.
  // See https://en.wikipedia.org/wiki/Rotation_matrix#Quaternion.
  // Note: deviating from spec (http://www.w3.org/TR/css3-transforms/)
  // which has a degenerate case when the trace (t) of the orthonormal matrix
  // (Q) approaches -1. In the Wikipedia article, Q_ij is indexing on row then
  // column. Thus, Q_ij = column[j][i].

  // The following are equivalent representations of the rotation matrix:
  //
  // Axis-angle form:
  //
  //      [ c+(1-c)x^2  (1-c)xy-sz  (1-c)xz+sy ]    c = cos theta
  // R =  [ (1-c)xy+sz  c+(1-c)y^2  (1-c)yz-sx ]    s = sin theta
  //      [ (1-c)xz-sy  (1-c)yz+sx  c+(1-c)z^2 ]    [x,y,z] = axis or rotation
  //
  // The sum of the diagonal elements (trace) is a simple function of the cosine
  // of the angle. The w component of the quaternion is cos(theta/2), and we
  // make use of the double angle formula to directly compute w from the
  // trace. Differences between pairs of skew symmetric elements in this matrix
  // isolate the remaining components. Since w can be zero (also numerically
  // unstable if near zero), we cannot rely solely on this approach to compute
  // the quaternion components.
  //
  // Quaternion form:
  //
  //       [ 1-2(y^2+z^2)    2(xy-zw)      2(xz+yw)   ]
  //  r =  [   2(xy+zw)    1-2(x^2+z^2)    2(yz-xw)   ]    q = (x,y,z,w)
  //       [   2(xz-yw)      2(yz+xw)    1-2(x^2+y^2) ]
  //
  // Different linear combinations of the diagonal elements isolates x, y or z.
  // Sums or differences between skew symmetric elements isolate the remainder.

  double r, s, t, x, y, z, w;

  t = c0[0] + c1[1] + c2[2];  // trace of Q

  // https://en.wikipedia.org/wiki/Rotation_matrix#Quaternion
  if (1 + t > 0.001) {
    // Numerically stable as long as 1+t is not close to zero. Otherwise use the
    // diagonal element with the greatest value to compute the quaternions.
    r = std::sqrt(1.0 + t);
    s = 0.5 / r;
    w = 0.5 * r;
    x = (c1[2] - c2[1]) * s;
    y = (c2[0] - c0[2]) * s;
    z = (c0[1] - c1[0]) * s;
  } else if (c0[0] > c1[1] && c0[0] > c2[2]) {
    // Q_xx is largest.
    r = std::sqrt(1.0 + c0[0] - c1[1] - c2[2]);
    s = 0.5 / r;
    x = 0.5 * r;
    y = (c1[0] + c0[1]) * s;
    z = (c2[0] + c0[2]) * s;
    w = (c1[2] - c2[1]) * s;
  } else if (c1[1] > c2[2]) {
    // Q_yy is largest.
    r = std::sqrt(1.0 - c0[0] + c1[1] - c2[2]);
    s = 0.5 / r;
    x = (c1[0] + c0[1]) * s;
    y = 0.5 * r;
    z = (c2[1] + c1[2]) * s;
    w = (c2[0] - c0[2]) * s;
  } else {
    // Q_zz is largest.
    r = std::sqrt(1.0 - c0[0] - c1[1] + c2[2]);
    s = 0.5 / r;
    x = (c2[0] + c0[2]) * s;
    y = (c2[1] + c1[2]) * s;
    z = 0.5 * r;
    w = (c0[1] - c1[0]) * s;
  }

  decomp.quaternion.set_x(x);
  decomp.quaternion.set_y(y);
  decomp.quaternion.set_z(z);
  decomp.quaternion.set_w(w);

  return decomp;
}

}  // namespace gfx

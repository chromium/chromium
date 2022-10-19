// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/matrix44.h"

#include <type_traits>
#include <utility>

namespace gfx {

namespace {

ALWAYS_INLINE Double4 SwapHighLow(Double4 v) {
  return Double4{v[2], v[3], v[0], v[1]};
}

ALWAYS_INLINE Double4 SwapInPairs(Double4 v) {
  return Double4{v[1], v[0], v[3], v[2]};
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

void Matrix44::PreTranslate(double dx, double dy, double dz) {
  if (AllTrue(Double4{dx, dy, dz, 0} == Double4{0, 0, 0, 0}))
    return;

  SetCol(3, Col(0) * dx + Col(1) * dy + Col(2) * dz + Col(3));
}

void Matrix44::PostTranslate(double dx, double dy, double dz) {
  Double4 t{dx, dy, dz, 0};
  if (AllTrue(t == Double4{0, 0, 0, 0}))
    return;

  if (HasPerspective()) {
    for (int i = 0; i < 4; ++i)
      SetCol(i, Col(i) + t * matrix_[i][3]);
  } else {
    SetCol(3, Col(3) + t);
  }
}

void Matrix44::PreScale(double sx, double sy, double sz) {
  if (AllTrue(Double4{sx, sy, sz, 1} == Double4{1, 1, 1, 1}))
    return;

  SetCol(0, Col(0) * sx);
  SetCol(1, Col(1) * sy);
  SetCol(2, Col(2) * sz);
}

void Matrix44::PostScale(double sx, double sy, double sz) {
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

void Matrix44::ApplyPerspectiveDepth(double perspective) {
  DCHECK_NE(perspective, 0.0);
  SetCol(2, Col(2) + Col(3) * (-1.0 / perspective));
}

void Matrix44::SetConcat(const Matrix44& a, const Matrix44& b) {
  auto c0 = a.Col(0);
  auto c1 = a.Col(1);
  auto c2 = a.Col(2);
  auto c3 = a.Col(3);

  auto mc0 = b.Col(0);
  auto mc1 = b.Col(1);
  auto mc2 = b.Col(2);
  auto mc3 = b.Col(3);

  SetCol(0, c0 * mc0[0] + c1 * mc0[1] + c2 * mc0[2] + c3 * mc0[3]);
  SetCol(1, c0 * mc1[0] + c1 * mc1[1] + c2 * mc1[2] + c3 * mc1[3]);
  SetCol(2, c0 * mc2[0] + c1 * mc2[1] + c2 * mc2[2] + c3 * mc2[3]);
  SetCol(3, c0 * mc3[0] + c1 * mc3[1] + c2 * mc3[2] + c3 * mc3[3]);
}

// This is based on
// https://github.com/niswegmann/small-matrix-inverse/blob/master/invert4x4_llvm.h,
// which is based on Intel AP-928 "Streaming SIMD Extensions - Inverse of 4x4
// Matrix": https://drive.google.com/file/d/0B9rh9tVI0J5mX1RUam5nZm85OFE/view.
bool Matrix44::GetInverse(Matrix44& result) const {
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

  result.SetCol(0, c0);
  result.SetCol(1, c1);
  result.SetCol(2, c2);
  result.SetCol(3, c3);
  return true;
}

bool Matrix44::IsInvertible() const {
  return std::isnormal(static_cast<float>(Determinant()));
}

// This is a simplified version of GetInverse().
double Matrix44::Determinant() const {
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

void Matrix44::MapScalars(double vec[4]) const {
  Double4 v = LoadDouble4(vec);
  Double4 r0{matrix_[0][0], matrix_[1][0], matrix_[2][0], matrix_[3][0]};
  Double4 r1{matrix_[0][1], matrix_[1][1], matrix_[2][1], matrix_[3][1]};
  Double4 r2{matrix_[0][2], matrix_[1][2], matrix_[2][2], matrix_[3][2]};
  Double4 r3{matrix_[0][3], matrix_[1][3], matrix_[2][3], matrix_[3][3]};
  StoreDouble4(Double4{Sum(r0 * v), Sum(r1 * v), Sum(r2 * v), Sum(r3 * v)},
               vec);
}

void Matrix44::FlattenTo2d() {
  matrix_[0][2] = 0;
  matrix_[1][2] = 0;
  matrix_[3][2] = 0;
  SetCol(2, Double4{0, 0, 1, 0});
}

}  // namespace gfx

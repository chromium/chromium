// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_MATRIX44_H_
#define UI_GFX_GEOMETRY_MATRIX44_H_

#include <atomic>
#include <cstring>

#include "build/build_config.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "ui/gfx/geometry/geometry_skia_export.h"

#if BUILDFLAG(IS_MAC)
struct CATransform3D;
#endif

namespace gfx {

struct Vector4 {
  SkScalar fData[4];

  Vector4() { this->set(0, 0, 0, 1); }
  Vector4(const Vector4& src) { memcpy(fData, src.fData, sizeof(fData)); }
  Vector4(SkScalar x, SkScalar y, SkScalar z, SkScalar w = SK_Scalar1) {
    fData[0] = x;
    fData[1] = y;
    fData[2] = z;
    fData[3] = w;
  }

  Vector4& operator=(const Vector4& src) {
    memcpy(fData, src.fData, sizeof(fData));
    return *this;
  }

  bool operator==(const Vector4& v) const {
    return fData[0] == v.fData[0] && fData[1] == v.fData[1] &&
           fData[2] == v.fData[2] && fData[3] == v.fData[3];
  }
  bool operator!=(const Vector4& v) const { return !(*this == v); }
  bool equals(SkScalar x, SkScalar y, SkScalar z, SkScalar w = SK_Scalar1) {
    return fData[0] == x && fData[1] == y && fData[2] == z && fData[3] == w;
  }

  void set(SkScalar x, SkScalar y, SkScalar z, SkScalar w = SK_Scalar1) {
    fData[0] = x;
    fData[1] = y;
    fData[2] = z;
    fData[3] = w;
  }
};

// This is the underlying data structure of Transform. Don't use this type
// directly. The public methods can be called through Transform::matrix().
class GEOMETRY_SKIA_EXPORT Matrix44 {
 public:
  enum Uninitialized_Constructor { kUninitialized_Constructor };

  explicit Matrix44(Uninitialized_Constructor) {}

  constexpr Matrix44()
      : fMat{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}},
        fTypeMask(kIdentity_Mask) {}

  // The parameters are in row-major order.
  Matrix44(SkScalar col1row1,
           SkScalar col2row1,
           SkScalar col3row1,
           SkScalar col4row1,
           SkScalar col1row2,
           SkScalar col2row2,
           SkScalar col3row2,
           SkScalar col4row2,
           SkScalar col1row3,
           SkScalar col2row3,
           SkScalar col3row3,
           SkScalar col4row3,
           SkScalar col1row4,
           SkScalar col2row4,
           SkScalar col3row4,
           SkScalar col4row4)
      // fMat is indexed by [col][row] (i.e. col-major).
      : fMat{{col1row1, col1row2, col1row3, col1row4},
             {col2row1, col2row2, col2row3, col2row4},
             {col3row1, col3row2, col3row3, col3row4},
             {col4row1, col4row2, col4row3, col4row4}} {
    recomputeTypeMask();
  }

  Matrix44(const Matrix44& a, const Matrix44& b) { this->setConcat(a, b); }

  bool operator==(const Matrix44& other) const;
  bool operator!=(const Matrix44& other) const { return !(other == *this); }

  /* When converting from Matrix44 to SkMatrix, the third row and
   * column is dropped.  When converting from SkMatrix to Matrix44
   * the third row and column remain as identity:
   * [ a b c ]      [ a b 0 c ]
   * [ d e f ]  ->  [ d e 0 f ]
   * [ g h i ]      [ 0 0 1 0 ]
   *                [ g h 0 i ]
   */
  explicit Matrix44(const SkMatrix&);

  // Inverse conversion of the above.
  SkMatrix asM33() const;

  using TypeMask = uint8_t;
  enum : TypeMask {
    kIdentity_Mask = 0,
    kTranslate_Mask = 1 << 0,    //!< set if the matrix has translation
    kScale_Mask = 1 << 1,        //!< set if the matrix has any scale != 1
    kAffine_Mask = 1 << 2,       //!< set if the matrix skews or rotates
    kPerspective_Mask = 1 << 3,  //!< set if the matrix is in perspective
  };

  /**
   *  Returns a bitfield describing the transformations the matrix may
   *  perform. The bitfield is computed conservatively, so it may include
   *  false positives. For example, when kPerspective_Mask is true, all
   *  other bits may be set to true even in the case of a pure perspective
   *  transform.
   */
  inline TypeMask getType() const { return fTypeMask; }

  /**
   *  Return true if the matrix is identity.
   */
  inline bool isIdentity() const { return kIdentity_Mask == this->getType(); }

  /**
   *  Return true if the matrix contains translate or is identity.
   */
  inline bool isTranslate() const {
    return !(this->getType() & ~kTranslate_Mask);
  }

  /**
   *  Return true if the matrix only contains scale or translate or is identity.
   */
  inline bool isScaleTranslate() const {
    return !(this->getType() & ~(kScale_Mask | kTranslate_Mask));
  }

  /**
   *  Returns true if the matrix only contains scale or is identity.
   */
  inline bool isScale() const { return !(this->getType() & ~kScale_Mask); }

  inline bool hasPerspective() const {
    return SkToBool(this->getType() & kPerspective_Mask);
  }

  void setIdentity();

  /**
   *  get a value from the matrix. The row,col parameters work as follows:
   *  (0, 0)  scale-x
   *  (0, 3)  translate-x
   *  (3, 0)  perspective-x
   */
  inline SkScalar rc(int row, int col) const {
    SkASSERT((unsigned)row <= 3);
    SkASSERT((unsigned)col <= 3);
    return fMat[col][row];
  }

  /**
   *  set a value in the matrix. The row,col parameters work as follows:
   *  (0, 0)  scale-x
   *  (0, 3)  translate-x
   *  (3, 0)  perspective-x
   */
  inline void setRC(int row, int col, SkScalar value) {
    SkASSERT((unsigned)row <= 3);
    SkASSERT((unsigned)col <= 3);
    fMat[col][row] = value;
    this->recomputeTypeMask();
  }

  /** These methods allow one to efficiently read matrix entries into an
   *  array. The given array must have room for exactly 16 entries. Whenever
   *  possible, they will try to use memcpy rather than an entry-by-entry
   *  copy.
   *
   *  Col major indicates that consecutive elements of columns will be stored
   *  contiguously in memory.  Row major indicates that consecutive elements
   *  of rows will be stored contiguously in memory.
   */
  void getColMajor(float[]) const;
  void getRowMajor(float[]) const;

  /** These methods allow one to efficiently set all matrix entries from an
   *  array. The given array must have room for exactly 16 entries. Whenever
   *  possible, they will try to use memcpy rather than an entry-by-entry
   *  copy.
   *
   *  Col major indicates that input memory will be treated as if consecutive
   *  elements of columns are stored contiguously in memory.  Row major
   *  indicates that input memory will be treated as if consecutive elements
   *  of rows are stored contiguously in memory.
   */
  void setColMajor(const float[]);
  void setRowMajor(const float[]);

#if BUILDFLAG(IS_MAC)
  CATransform3D ToCATransform3D() const;
#endif

  Matrix44& setTranslate(SkScalar dx, SkScalar dy, SkScalar dz);
  Matrix44& preTranslate(SkScalar dx, SkScalar dy, SkScalar dz);
  Matrix44& postTranslate(SkScalar dx, SkScalar dy, SkScalar dz);

  Matrix44& setScale(SkScalar sx, SkScalar sy, SkScalar sz);
  Matrix44& preScale(SkScalar sx, SkScalar sy, SkScalar sz);
  Matrix44& postScale(SkScalar sx, SkScalar sy, SkScalar sz);

  inline Matrix44& setScale(SkScalar scale) {
    return this->setScale(scale, scale, scale);
  }
  inline Matrix44& preScale(SkScalar scale) {
    return this->preScale(scale, scale, scale);
  }
  inline Matrix44& postScale(SkScalar scale) {
    return this->postScale(scale, scale, scale);
  }

  // Sets this matrix to rotate about the specified unit-length axis vector,
  // by an angle specified by its sin() and cos(). This does not attempt to
  // verify that axis(x, y, z).length() == 1 or that the sin, cos values are
  // correct.
  void setRotateUnitSinCos(SkScalar x,
                           SkScalar y,
                           SkScalar z,
                           SkScalar sin_angle,
                           SkScalar cos_angle);

  // Special case for x, y or z axis of the above function.
  void setRotateAboutXAxisSinCos(SkScalar sin_angle, SkScalar cos_angle);
  void setRotateAboutYAxisSinCos(SkScalar sin_angle, SkScalar cos_angle);
  void setRotateAboutZAxisSinCos(SkScalar sin_angle, SkScalar cos_angle);

  void setConcat(const Matrix44& a, const Matrix44& b);
  inline void preConcat(const Matrix44& m) { this->setConcat(*this, m); }
  inline void postConcat(const Matrix44& m) { this->setConcat(m, *this); }

  friend Matrix44 operator*(const Matrix44& a, const Matrix44& b) {
    return Matrix44(a, b);
  }

  /** If this is invertible, return that in inverse and return true. If it is
      not invertible, return false and leave the inverse parameter in an
      unspecified state.
   */
  bool invert(Matrix44* inverse) const;

  /** Transpose this matrix in place. */
  void transpose();

  /** Apply the matrix to the src vector, returning the new vector in dst.
      It is legal for src and dst to point to the same memory.
   */
  void mapScalars(const SkScalar src[4], SkScalar dst[4]) const;
  inline void mapScalars(SkScalar vec[4]) const { this->mapScalars(vec, vec); }

  friend Vector4 operator*(const Matrix44& m, const Vector4& src) {
    Vector4 dst;
    m.mapScalars(src.fData, dst.fData);
    return dst;
  }

  /**
   *  map an array of [x, y, 0, 1] through the matrix, returning an array
   *  of [x', y', z', w'].
   *
   *  @param src2     array of [x, y] pairs, with implied z=0 and w=1
   *  @param count    number of [x, y] pairs in src2
   *  @param dst4     array of [x', y', z', w'] quads as the output.
   */
  void map2(const float src2[], int count, float dst4[]) const;
  void map2(const double src2[], int count, double dst4[]) const;

  /** Returns true if transformating an axis-aligned square in 2d by this matrix
      will produce another 2d axis-aligned square; typically means the matrix
      is a scale with perhaps a 90-degree rotation. A 3d rotation through 90
      degrees into a perpendicular plane collapses a square to a line, but
      is still considered to be axis-aligned.

      By default, tolerates very slight error due to float imprecisions;
      a 90-degree rotation can still end up with 10^-17 of
      "non-axis-aligned" result.
   */
  bool preserves2dAxisAlignment(SkScalar epsilon = SK_ScalarNearlyZero) const;

  double determinant() const;

  void FlattenTo2d();

 private:
  /* This is indexed by [col][row]. */
  SkScalar fMat[4][4];
  TypeMask fTypeMask;

  static constexpr int kAllPublic_Masks = 0xF;

  SkScalar transX() const { return fMat[3][0]; }
  SkScalar transY() const { return fMat[3][1]; }
  SkScalar transZ() const { return fMat[3][2]; }

  SkScalar scaleX() const { return fMat[0][0]; }
  SkScalar scaleY() const { return fMat[1][1]; }
  SkScalar scaleZ() const { return fMat[2][2]; }

  SkScalar perspX() const { return fMat[0][3]; }
  SkScalar perspY() const { return fMat[1][3]; }
  SkScalar perspZ() const { return fMat[2][3]; }

  void recomputeTypeMask();

  inline void setTypeMask(TypeMask mask) {
    SkASSERT(0 == (~kAllPublic_Masks & mask));
    fTypeMask = mask;
  }
};

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_MATRIX44_H_

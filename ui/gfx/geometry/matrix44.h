// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_MATRIX44_H_
#define UI_GFX_GEOMETRY_MATRIX44_H_

#include <atomic>
#include <cstring>

#include "third_party/skia/include/core/SkMatrix.h"
#include "ui/gfx/geometry/geometry_skia_export.h"

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
  enum Identity_Constructor { kIdentity_Constructor };
  enum NaN_Constructor { kNaN_Constructor };

  explicit Matrix44(Uninitialized_Constructor) {}

  constexpr explicit Matrix44(Identity_Constructor)
      : fMat{{
                 1,
                 0,
                 0,
                 0,
             },
             {
                 0,
                 1,
                 0,
                 0,
             },
             {
                 0,
                 0,
                 1,
                 0,
             },
             {
                 0,
                 0,
                 0,
                 1,
             }},
        fTypeMask(kIdentity_Mask) {}

  explicit Matrix44(NaN_Constructor)
      : fMat{{SK_ScalarNaN, SK_ScalarNaN, SK_ScalarNaN, SK_ScalarNaN},
             {SK_ScalarNaN, SK_ScalarNaN, SK_ScalarNaN, SK_ScalarNaN},
             {SK_ScalarNaN, SK_ScalarNaN, SK_ScalarNaN, SK_ScalarNaN},
             {SK_ScalarNaN, SK_ScalarNaN, SK_ScalarNaN, SK_ScalarNaN}},
        fTypeMask(kTranslate_Mask | kScale_Mask | kAffine_Mask |
                  kPerspective_Mask) {}

  constexpr Matrix44() : Matrix44{kIdentity_Constructor} {}

  Matrix44(const Matrix44& src) = default;

  Matrix44& operator=(const Matrix44& src) = default;

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
  Matrix44& operator=(const SkMatrix& src);

  // TODO: make this explicit (will need to guard that change to update chrome,
  // etc.
#ifndef SK_SUPPORT_LEGACY_IMPLICIT_CONVERSION_MATRIX44
  explicit
#endif
  operator SkMatrix() const;

  /**
   *  Return a reference to a const identity matrix
   */
  static const Matrix44& I();

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
  inline void reset() { this->setIdentity(); }

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
  void asColMajorf(float[]) const;
  void asColMajord(double[]) const;
  void asRowMajorf(float[]) const;
  void asRowMajord(double[]) const;

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
  void setColMajorf(const float[]);
  void setColMajord(const double[]);
  void setRowMajorf(const float[]);
  void setRowMajord(const double[]);

  void setColMajor(const SkScalar data[]) { this->setColMajorf(data); }
  void setRowMajor(const SkScalar data[]) { this->setRowMajorf(data); }

  /* This sets the top-left of the matrix and clears the translation and
   * perspective components (with [3][3] set to 1).  m_ij is interpreted
   * as the matrix entry at row = i, col = j. */
  void set3x3(SkScalar m_00,
              SkScalar m_10,
              SkScalar m_20,
              SkScalar m_01,
              SkScalar m_11,
              SkScalar m_21,
              SkScalar m_02,
              SkScalar m_12,
              SkScalar m_22);
  void set3x3RowMajorf(const float[]);

  void set4x4(SkScalar m_00,
              SkScalar m_10,
              SkScalar m_20,
              SkScalar m_30,
              SkScalar m_01,
              SkScalar m_11,
              SkScalar m_21,
              SkScalar m_31,
              SkScalar m_02,
              SkScalar m_12,
              SkScalar m_22,
              SkScalar m_32,
              SkScalar m_03,
              SkScalar m_13,
              SkScalar m_23,
              SkScalar m_33);

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

  void setRotateDegreesAbout(SkScalar x,
                             SkScalar y,
                             SkScalar z,
                             SkScalar degrees) {
    this->setRotateAbout(x, y, z, degrees * SK_ScalarPI / 180);
  }

  /** Rotate about the vector [x,y,z]. If that vector is not unit-length,
      it will be automatically resized.
   */
  void setRotateAbout(SkScalar x, SkScalar y, SkScalar z, SkScalar radians);
  /** Rotate about the vector [x,y,z]. Does not check the length of the
      vector, assuming it is unit-length.
   */
  void setRotateAboutUnit(SkScalar x, SkScalar y, SkScalar z, SkScalar radians);

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

  void dump() const;

  double determinant() const;

 private:
  /* This is indexed by [col][row]. */
  SkScalar fMat[4][4];
  TypeMask fTypeMask;

  static constexpr int kAllPublic_Masks = 0xF;

  void as3x4RowMajorf(float[]) const;
  void set3x4RowMajorf(const float[]);

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

  inline const SkScalar* values() const { return &fMat[0][0]; }
};

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_MATRIX44_H_

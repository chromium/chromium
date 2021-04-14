// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_AXIS_TRANSFORM2D_H_
#define UI_GFX_GEOMETRY_AXIS_TRANSFORM2D_H_

#include "base/check_op.h"
#include "ui/gfx/geometry/geometry_export.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace gfx {

// This class implements the subset of 2D linear transforms that only
// translation and uniform scaling are allowed.
// Internally this is stored as a vector for pre-scale, and another vector
// for post-translation. The class constructor and member accessor follows
// the same convention, but a scalar scale factor is also accepted.
class GEOMETRY_EXPORT AxisTransform2d {
 public:
  constexpr AxisTransform2d() = default;
  constexpr AxisTransform2d(float scale, const Vector2dF& translation)
      : scale_(scale, scale), translation_(translation) {}
  constexpr AxisTransform2d(const Vector2dF& scale,
                            const Vector2dF& translation)
      : scale_(scale), translation_(translation) {}

  bool operator==(const AxisTransform2d& other) const {
    return scale_ == other.scale_ && translation_ == other.translation_;
  }
  bool operator!=(const AxisTransform2d& other) const {
    return !(*this == other);
  }

  void PreScale(const Vector2dF& scale) { scale_.Scale(scale.x(), scale.y()); }
  void PostScale(const Vector2dF& scale) {
    scale_.Scale(scale.x(), scale.y());
    translation_.Scale(scale.x(), scale.y());
  }
  void PreTranslate(const Vector2dF& translation) {
    translation_ += ScaleVector2d(translation, scale_.x(), scale_.y());
  }
  void PostTranslate(const Vector2dF& translation) {
    translation_ += translation;
  }

  void PreConcat(const AxisTransform2d& pre) {
    PreTranslate(pre.translation_);
    PreScale(pre.scale_);
  }
  void PostConcat(const AxisTransform2d& post) {
    PostScale(post.scale_);
    PostTranslate(post.translation_);
  }

  void Invert() {
    DCHECK(scale_.x());
    DCHECK(scale_.y());
    scale_ = Vector2dF(1.f / scale_.x(), 1.f / scale_.y());
    translation_.Scale(-scale_.x(), -scale_.y());
  }

  PointF MapPoint(const PointF& p) const {
    return ScalePoint(p, scale_.x(), scale_.y()) + translation_;
  }
  PointF InverseMapPoint(const PointF& p) const {
    return ScalePoint(p - translation_, 1.f / scale_.x(), 1.f / scale_.y());
  }

  RectF MapRect(const RectF& r) const {
    DCHECK_GE(scale_.x(), 0.f);
    DCHECK_GE(scale_.y(), 0.f);
    return ScaleRect(r, scale_.x(), scale_.y()) + translation_;
  }
  RectF InverseMapRect(const RectF& r) const {
    DCHECK_GT(scale_.x(), 0.f);
    DCHECK_GT(scale_.y(), 0.f);
    return ScaleRect(r - translation_, 1.f / scale_.x(), 1.f / scale_.y());
  }

  const Vector2dF& scale() const { return scale_; }
  const Vector2dF& translation() const { return translation_; }

  std::string ToString() const;

 private:
  // Scale is applied before translation, i.e.
  // this->Transform(p) == scale_ * p + translation_
  Vector2dF scale_{1.f, 1.f};
  Vector2dF translation_;
};

inline AxisTransform2d PreScaleAxisTransform2d(const AxisTransform2d& t,
                                               float scale) {
  AxisTransform2d result(t);
  result.PreScale(Vector2dF(scale, scale));
  return result;
}

inline AxisTransform2d PostScaleAxisTransform2d(const AxisTransform2d& t,
                                                float scale) {
  AxisTransform2d result(t);
  result.PostScale(Vector2dF(scale, scale));
  return result;
}

inline AxisTransform2d PreTranslateAxisTransform2d(
    const AxisTransform2d& t,
    const Vector2dF& translation) {
  AxisTransform2d result(t);
  result.PreTranslate(translation);
  return result;
}

inline AxisTransform2d PostTranslateAxisTransform2d(
    const AxisTransform2d& t,
    const Vector2dF& translation) {
  AxisTransform2d result(t);
  result.PostTranslate(translation);
  return result;
}

inline AxisTransform2d ConcatAxisTransform2d(const AxisTransform2d& post,
                                             const AxisTransform2d& pre) {
  AxisTransform2d result(post);
  result.PreConcat(pre);
  return result;
}

inline AxisTransform2d InvertAxisTransform2d(const AxisTransform2d& t) {
  AxisTransform2d result = t;
  result.Invert();
  return result;
}

// This is declared here for use in gtest-based unit tests but is defined in
// the //ui/gfx:test_support target. Depend on that to use this in your unit
// test. This should not be used in production code - call ToString() instead.
void PrintTo(const AxisTransform2d&, ::std::ostream* os);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_AXIS_TRANSFORM2D_H_

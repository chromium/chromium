// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/interpolated_transform.h"

#include <cmath>

#include "base/check.h"
#include "base/numerics/safe_conversions.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/transform_util.h"

namespace {

static const double EPSILON = 1e-6;

bool IsMultipleOfNinetyDegrees(double degrees) {
  double remainder = fabs(fmod(degrees, 90.0));
  return remainder < EPSILON || 90.0 - remainder < EPSILON;
}

// Returns false if |degrees| is not a multiple of ninety degrees or if
// |rotation| is NULL. It does not affect |rotation| in this case. Otherwise
// *rotation is set to be the appropriate sanitized rotation matrix. That is,
// the rotation matrix corresponding to |degrees| which has entries that are all
// either 0, 1 or -1.
bool MassageRotationIfMultipleOfNinetyDegrees(gfx::Transform* rotation,
                                              float degrees) {
  if (!IsMultipleOfNinetyDegrees(degrees) || !rotation)
    return false;

  float degrees_by_ninety = degrees / 90.0f;

  int n = base::ClampRound(degrees_by_ninety);

  n %= 4;
  if (n < 0)
    n += 4;

  // n should now be in the range [0, 3]
  if (n == 0) {
    rotation->MakeIdentity();
  } else if (n == 1) {
    *rotation = gfx::Transform::Make90degRotation();
  } else if (n == 2) {
    *rotation = gfx::Transform::Make180degRotation();
  } else if (n == 3) {
    *rotation = gfx::Transform::Make270degRotation();
  }
  return true;
}

} // namespace

namespace ui {

///////////////////////////////////////////////////////////////////////////////
// InterpolatedTransform
//

InterpolatedTransform::InterpolatedTransform()
    : start_time_(0.0f),
      end_time_(1.0f),
      reversed_(false) {
}

InterpolatedTransform::InterpolatedTransform(float start_time,
                                             float end_time)
    : start_time_(start_time),
      end_time_(end_time),
      reversed_(false) {
}

InterpolatedTransform::~InterpolatedTransform() {}

gfx::Transform InterpolatedTransform::Interpolate(float t) const {
  if (reversed_)
    t = 1.0f - t;
  gfx::Transform result = InterpolateButDoNotCompose(t);
  if (child_.get()) {
    result.PostConcat(child_->Interpolate(t));
  }
  return result;
}

void InterpolatedTransform::SetChild(
    std::unique_ptr<InterpolatedTransform> child) {
  child_ = std::move(child);
}

inline float InterpolatedTransform::ValueBetween(float time,
                                                 float start_value,
                                                 float end_value) const {
  // can't handle NaN
  DCHECK(time == time && start_time_ == start_time_ && end_time_ == end_time_);
  if (time != time || start_time_ != start_time_ || end_time_ != end_time_)
    return start_value;

  // Ok if equal -- we'll get a step function. Note: if end_time_ ==
  // start_time_ == x, then if none of the numbers are NaN, then it
  // must be true that time < x or time >= x, so we will return early
  // due to one of the following if statements.
  DCHECK(end_time_ >= start_time_);

  if (time < start_time_)
    return start_value;

  if (time >= end_time_)
    return end_value;

  float t = (time - start_time_) / (end_time_ - start_time_);
  return static_cast<float>(
      gfx::Tween::DoubleValueBetween(t, start_value, end_value));
}

///////////////////////////////////////////////////////////////////////////////
// InterpolatedRotation
//

InterpolatedRotation::InterpolatedRotation(float start_degrees,
                                           float end_degrees)
    : InterpolatedTransform(),
      start_degrees_(start_degrees),
      end_degrees_(end_degrees) {
}

InterpolatedRotation::InterpolatedRotation(float start_degrees,
                                           float end_degrees,
                                           float start_time,
                                           float end_time)
    : InterpolatedTransform(start_time, end_time),
      start_degrees_(start_degrees),
      end_degrees_(end_degrees) {
}

InterpolatedRotation::~InterpolatedRotation() {}

gfx::Transform InterpolatedRotation::InterpolateButDoNotCompose(float t) const {
  gfx::Transform result;
  float interpolated_degrees = ValueBetween(t, start_degrees_, end_degrees_);
  result.Rotate(interpolated_degrees);
  if (t == 0.0f || t == 1.0f)
    MassageRotationIfMultipleOfNinetyDegrees(&result, interpolated_degrees);
  return result;
}

///////////////////////////////////////////////////////////////////////////////
// InterpolatedAxisAngleRotation
//

InterpolatedAxisAngleRotation::InterpolatedAxisAngleRotation(
    const gfx::Vector3dF& axis,
    float start_degrees,
    float end_degrees)
    : InterpolatedTransform(),
      axis_(axis),
      start_degrees_(start_degrees),
      end_degrees_(end_degrees) {
}

InterpolatedAxisAngleRotation::InterpolatedAxisAngleRotation(
    const gfx::Vector3dF& axis,
    float start_degrees,
    float end_degrees,
    float start_time,
    float end_time)
    : InterpolatedTransform(start_time, end_time),
      axis_(axis),
      start_degrees_(start_degrees),
      end_degrees_(end_degrees) {
}

InterpolatedAxisAngleRotation::~InterpolatedAxisAngleRotation() {}

gfx::Transform
InterpolatedAxisAngleRotation::InterpolateButDoNotCompose(float t) const {
  gfx::Transform result;
  result.RotateAbout(axis_, ValueBetween(t, start_degrees_, end_degrees_));
  return result;
}

///////////////////////////////////////////////////////////////////////////////
// InterpolatedScale
//

InterpolatedScale::InterpolatedScale(float start_scale, float end_scale)
    : InterpolatedTransform(),
      start_scale_(gfx::Point3F(start_scale, start_scale, start_scale)),
      end_scale_(gfx::Point3F(end_scale, end_scale, end_scale)) {
}

InterpolatedScale::InterpolatedScale(float start_scale, float end_scale,
                                     float start_time, float end_time)
    : InterpolatedTransform(start_time, end_time),
      start_scale_(gfx::Point3F(start_scale, start_scale, start_scale)),
      end_scale_(gfx::Point3F(end_scale, end_scale, end_scale)) {
}

InterpolatedScale::InterpolatedScale(const gfx::Point3F& start_scale,
                                     const gfx::Point3F& end_scale)
    : InterpolatedTransform(),
      start_scale_(start_scale),
      end_scale_(end_scale) {
}

InterpolatedScale::InterpolatedScale(const gfx::Point3F& start_scale,
                                     const gfx::Point3F& end_scale,
                                     float start_time,
                                     float end_time)
    : InterpolatedTransform(start_time, end_time),
      start_scale_(start_scale),
      end_scale_(end_scale) {
}

InterpolatedScale::~InterpolatedScale() {}

gfx::Transform InterpolatedScale::InterpolateButDoNotCompose(float t) const {
  gfx::Transform result;
  float scale_x = ValueBetween(t, start_scale_.x(), end_scale_.x());
  float scale_y = ValueBetween(t, start_scale_.y(), end_scale_.y());
  float scale_z = ValueBetween(t, start_scale_.z(), end_scale_.z());
  result.Scale3d(scale_x, scale_y, scale_z);
  return result;
}

///////////////////////////////////////////////////////////////////////////////
// InterpolatedTranslation
//

InterpolatedTranslation::InterpolatedTranslation(const gfx::PointF& start_pos,
                                                 const gfx::PointF& end_pos)
    : InterpolatedTransform(), start_pos_(start_pos), end_pos_(end_pos) {}

InterpolatedTranslation::InterpolatedTranslation(const gfx::PointF& start_pos,
                                                 const gfx::PointF& end_pos,
                                                 float start_time,
                                                 float end_time)
    : InterpolatedTransform(start_time, end_time),
      start_pos_(start_pos),
      end_pos_(end_pos) {}

InterpolatedTranslation::InterpolatedTranslation(const gfx::Point3F& start_pos,
                                                 const gfx::Point3F& end_pos)
    : InterpolatedTransform(), start_pos_(start_pos), end_pos_(end_pos) {
}

InterpolatedTranslation::InterpolatedTranslation(const gfx::Point3F& start_pos,
                                                 const gfx::Point3F& end_pos,
                                                 float start_time,
                                                 float end_time)
    : InterpolatedTransform(start_time, end_time),
      start_pos_(start_pos),
      end_pos_(end_pos) {
}

InterpolatedTranslation::~InterpolatedTranslation() {}

gfx::Transform
InterpolatedTranslation::InterpolateButDoNotCompose(float t) const {
  gfx::Transform result;
  result.Translate3d(ValueBetween(t, start_pos_.x(), end_pos_.x()),
                     ValueBetween(t, start_pos_.y(), end_pos_.y()),
                     ValueBetween(t, start_pos_.z(), end_pos_.z()));
  return result;
}

///////////////////////////////////////////////////////////////////////////////
// InterpolatedConstantTransform
//

InterpolatedConstantTransform::InterpolatedConstantTransform(
  const gfx::Transform& transform)
    : InterpolatedTransform(),
  transform_(transform) {
}

gfx::Transform
InterpolatedConstantTransform::InterpolateButDoNotCompose(float t) const {
  return transform_;
}

InterpolatedConstantTransform::~InterpolatedConstantTransform() {}

///////////////////////////////////////////////////////////////////////////////
// InterpolatedTransformAboutPivot
//

InterpolatedTransformAboutPivot::InterpolatedTransformAboutPivot(
    const gfx::Point& pivot,
    std::unique_ptr<InterpolatedTransform> transform)
    : InterpolatedTransform() {
  Init(pivot, std::move(transform));
}

InterpolatedTransformAboutPivot::InterpolatedTransformAboutPivot(
    const gfx::Point& pivot,
    std::unique_ptr<InterpolatedTransform> transform,
    float start_time,
    float end_time)
    : InterpolatedTransform() {
  Init(pivot, std::move(transform));
}

InterpolatedTransformAboutPivot::~InterpolatedTransformAboutPivot() {}

gfx::Transform
InterpolatedTransformAboutPivot::InterpolateButDoNotCompose(float t) const {
  if (transform_.get()) {
    return transform_->Interpolate(t);
  }
  return gfx::Transform();
}

void InterpolatedTransformAboutPivot::Init(
    const gfx::Point& pivot,
    std::unique_ptr<InterpolatedTransform> xform) {
  gfx::Transform to_pivot;
  gfx::Transform from_pivot;
  to_pivot.Translate(SkIntToScalar(-pivot.x()), SkIntToScalar(-pivot.y()));
  from_pivot.Translate(SkIntToScalar(pivot.x()), SkIntToScalar(pivot.y()));

  std::unique_ptr<InterpolatedTransform> pre_transform =
      std::make_unique<InterpolatedConstantTransform>(to_pivot);
  std::unique_ptr<InterpolatedTransform> post_transform =
      std::make_unique<InterpolatedConstantTransform>(from_pivot);

  xform->SetChild(std::move(post_transform));
  pre_transform->SetChild(std::move(xform));
  transform_ = std::move(pre_transform);
}

InterpolatedMatrixTransform::InterpolatedMatrixTransform(
    const gfx::Transform& start_transform,
    const gfx::Transform& end_transform)
    : InterpolatedTransform() {
  Init(start_transform, end_transform);
}

InterpolatedMatrixTransform::InterpolatedMatrixTransform(
    const gfx::Transform& start_transform,
    const gfx::Transform& end_transform,
    float start_time,
    float end_time)
    : InterpolatedTransform() {
  Init(start_transform, end_transform);
}

InterpolatedMatrixTransform::~InterpolatedMatrixTransform() {}

gfx::Transform
InterpolatedMatrixTransform::InterpolateButDoNotCompose(float t) const {
  gfx::DecomposedTransform blended =
      gfx::BlendDecomposedTransforms(end_decomp_, start_decomp_, t);
  return gfx::Transform::Compose(blended);
}

void InterpolatedMatrixTransform::Init(const gfx::Transform& start_transform,
                                       const gfx::Transform& end_transform) {
  // Both transforms should be decomposible.
  start_decomp_ = *start_transform.Decompose();
  end_decomp_ = *end_transform.Decompose();
}

} // namespace ui

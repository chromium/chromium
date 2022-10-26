// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_INTERPOLATED_TRANSFORM_H_
#define UI_GFX_INTERPOLATED_TRANSFORM_H_

#include <memory>

#include "ui/gfx/geometry/decomposed_transform.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/gfx_export.h"

namespace ui {

///////////////////////////////////////////////////////////////////////////////
// class InterpolatedTransform
//
// Abstract base class for transforms that animate over time. These
// interpolated transforms can be combined to allow for more sophisticated
// animations. For example, you might combine a rotation of 90 degrees between
// times 0 and 1, with a scale from 1 to 0.3 between times 0 and 0.25 and a
// scale from 0.3 to 1 from between times 0.75 and 1.
//
///////////////////////////////////////////////////////////////////////////////
class GFX_EXPORT InterpolatedTransform {
 public:
  InterpolatedTransform();
  // The interpolated transform varies only when t in (start_time, end_time).
  // If t <= start_time, Interpolate(t) will return the initial transform, and
  // if t >= end_time, Interpolate(t) will return the final transform.
  InterpolatedTransform(float start_time, float end_time);

  InterpolatedTransform(const InterpolatedTransform&) = delete;
  InterpolatedTransform& operator=(const InterpolatedTransform&) = delete;

  virtual ~InterpolatedTransform();

  // Returns the interpolated transform at time t. Note: not virtual.
  gfx::Transform Interpolate(float t) const;

  // The Intepolate ultimately returns the product of our transform at time t
  // and our child's transform at time t (if we have one).
  //
  // This function takes ownership of the passed InterpolatedTransform.
  void SetChild(std::unique_ptr<InterpolatedTransform> child);

  // If the interpolated transform is reversed, Interpolate(t) will return
  // Interpolate(1 - t)
  void SetReversed(bool reversed) { reversed_ = reversed; }
  bool Reversed() const { return reversed_; }

 protected:
  // Calculates the interpolated transform without considering our child.
  virtual gfx::Transform InterpolateButDoNotCompose(float t) const = 0;

  // If time in (start_time_, end_time_], this function linearly interpolates
  // between start_value and end_value.  More precisely it returns
  // (1 - t) * start_value + t * end_value where
  // t = (start_time_ - time) / (end_time_ - start_time_).
  // If time < start_time_ it returns start_value, and if time >= end_time_
  // it returns end_value.
  float ValueBetween(float time, float start_value, float end_value) const;

  float start_time() const { return start_time_; }
  float end_time() const { return end_time_; }

 private:
  const float start_time_;
  const float end_time_;

  // The child transform. If you consider an interpolated transform as a
  // function of t. If, without a child, we are f(t), and our child is
  // g(t), then with a child we become f'(t) = f(t) * g(t). Using a child
  // transform, we can chain collections of transforms together.
  std::unique_ptr<InterpolatedTransform> child_;

  bool reversed_;
};

///////////////////////////////////////////////////////////////////////////////
// class InterpolatedRotation
//
// Represents an animated rotation.
//
///////////////////////////////////////////////////////////////////////////////
class GFX_EXPORT InterpolatedRotation : public InterpolatedTransform {
 public:
  InterpolatedRotation(float start_degrees, float end_degrees);
  InterpolatedRotation(float start_degrees,
                       float end_degrees,
                       float start_time,
                       float end_time);

  InterpolatedRotation(const InterpolatedRotation&) = delete;
  InterpolatedRotation& operator=(const InterpolatedRotation&) = delete;

  ~InterpolatedRotation() override;

 protected:
  gfx::Transform InterpolateButDoNotCompose(float t) const override;

 private:
  const float start_degrees_;
  const float end_degrees_;
};

///////////////////////////////////////////////////////////////////////////////
// class InterpolatedAxisAngleRotation
//
// Represents an animated rotation.
//
///////////////////////////////////////////////////////////////////////////////
class GFX_EXPORT InterpolatedAxisAngleRotation : public InterpolatedTransform {
 public:
  InterpolatedAxisAngleRotation(const gfx::Vector3dF& axis,
                                float start_degrees,
                                float end_degrees);
  InterpolatedAxisAngleRotation(const gfx::Vector3dF& axis,
                                float start_degrees,
                                float end_degrees,
                                float start_time,
                                float end_time);

  InterpolatedAxisAngleRotation(const InterpolatedAxisAngleRotation&) = delete;
  InterpolatedAxisAngleRotation& operator=(
      const InterpolatedAxisAngleRotation&) = delete;

  ~InterpolatedAxisAngleRotation() override;

 protected:
  gfx::Transform InterpolateButDoNotCompose(float t) const override;

 private:
  gfx::Vector3dF axis_;
  const float start_degrees_;
  const float end_degrees_;
};

///////////////////////////////////////////////////////////////////////////////
// class InterpolatedScale
//
// Represents an animated scale.
//
///////////////////////////////////////////////////////////////////////////////
class GFX_EXPORT InterpolatedScale : public InterpolatedTransform {
 public:
  InterpolatedScale(float start_scale, float end_scale);
  InterpolatedScale(float start_scale, float end_scale,
                    float start_time, float end_time);
  InterpolatedScale(const gfx::Point3F& start_scale,
                    const gfx::Point3F& end_scale);
  InterpolatedScale(const gfx::Point3F& start_scale,
                    const gfx::Point3F& end_scale,
                    float start_time,
                    float end_time);

  InterpolatedScale(const InterpolatedScale&) = delete;
  InterpolatedScale& operator=(const InterpolatedScale&) = delete;

  ~InterpolatedScale() override;

 protected:
  gfx::Transform InterpolateButDoNotCompose(float t) const override;

 private:
  const gfx::Point3F start_scale_;
  const gfx::Point3F end_scale_;
};

class GFX_EXPORT InterpolatedTranslation : public InterpolatedTransform {
 public:
  InterpolatedTranslation(const gfx::PointF& start_pos,
                          const gfx::PointF& end_pos);
  InterpolatedTranslation(const gfx::PointF& start_pos,
                          const gfx::PointF& end_pos,
                          float start_time,
                          float end_time);
  InterpolatedTranslation(const gfx::Point3F& start_pos,
                          const gfx::Point3F& end_pos);
  InterpolatedTranslation(const gfx::Point3F& start_pos,
                          const gfx::Point3F& end_pos,
                          float start_time,
                          float end_time);

  InterpolatedTranslation(const InterpolatedTranslation&) = delete;
  InterpolatedTranslation& operator=(const InterpolatedTranslation&) = delete;

  ~InterpolatedTranslation() override;

 protected:
  gfx::Transform InterpolateButDoNotCompose(float t) const override;

 private:
  const gfx::Point3F start_pos_;
  const gfx::Point3F end_pos_;
};

///////////////////////////////////////////////////////////////////////////////
// class InterpolatedConstantTransform
//
// Represents a transform that is constant over time. This is only useful when
// composed with other interpolated transforms.
//
// See InterpolatedTransformAboutPivot for an example of its usage.
//
///////////////////////////////////////////////////////////////////////////////
class GFX_EXPORT InterpolatedConstantTransform : public InterpolatedTransform {
 public:
  explicit InterpolatedConstantTransform(const gfx::Transform& transform);

  InterpolatedConstantTransform(const InterpolatedConstantTransform&) = delete;
  InterpolatedConstantTransform& operator=(
      const InterpolatedConstantTransform&) = delete;

  ~InterpolatedConstantTransform() override;

 protected:
  gfx::Transform InterpolateButDoNotCompose(float t) const override;

 private:
  const gfx::Transform transform_;
};

///////////////////////////////////////////////////////////////////////////////
// class InterpolatedTransformAboutPivot
//
// Represents an animated transform with a transformed origin. Essentially,
// at each time, t, the interpolated transform is created by composing
// P * T * P^-1 where P is a constant transform to the new origin.
//
///////////////////////////////////////////////////////////////////////////////
class GFX_EXPORT InterpolatedTransformAboutPivot
    : public InterpolatedTransform {
 public:
  // Takes ownership of the passed transform.
  InterpolatedTransformAboutPivot(
      const gfx::Point& pivot,
      std::unique_ptr<InterpolatedTransform> transform);

  // Takes ownership of the passed transform.
  InterpolatedTransformAboutPivot(
      const gfx::Point& pivot,
      std::unique_ptr<InterpolatedTransform> transform,
      float start_time,
      float end_time);

  InterpolatedTransformAboutPivot(const InterpolatedTransformAboutPivot&) =
      delete;
  InterpolatedTransformAboutPivot& operator=(
      const InterpolatedTransformAboutPivot&) = delete;

  ~InterpolatedTransformAboutPivot() override;

 protected:
  gfx::Transform InterpolateButDoNotCompose(float t) const override;

 private:
  void Init(const gfx::Point& pivot,
            std::unique_ptr<InterpolatedTransform> transform);

  std::unique_ptr<InterpolatedTransform> transform_;
};

class GFX_EXPORT InterpolatedMatrixTransform : public InterpolatedTransform {
 public:
  InterpolatedMatrixTransform(const gfx::Transform& start_transform,
                              const gfx::Transform& end_transform);

  InterpolatedMatrixTransform(const gfx::Transform& start_transform,
                              const gfx::Transform& end_transform,
                              float start_time,
                              float end_time);

  ~InterpolatedMatrixTransform() override;

 protected:
  gfx::Transform InterpolateButDoNotCompose(float t) const override;

 private:
  void Init(const gfx::Transform& start_transform,
            const gfx::Transform& end_transform);

  gfx::DecomposedTransform start_decomp_;
  gfx::DecomposedTransform end_decomp_;
};

} // namespace ui

#endif  // UI_GFX_INTERPOLATED_TRANSFORM_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_MASK_FILTER_INFO_H_
#define UI_GFX_GEOMETRY_MASK_FILTER_INFO_H_

#include <optional>

#include "ui/gfx/geometry/geometry_skia_export.h"
#include "ui/gfx/geometry/linear_gradient.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"

namespace gfx {

class AxisTransform2d;
class Transform;

// This class defines a mask filter to be applied to the given rect.
class GEOMETRY_SKIA_EXPORT MaskFilterInfo {
 public:
  MaskFilterInfo() = default;
  explicit MaskFilterInfo(const RRectF& rrect)
      : rounded_corner_bounds_(rrect) {}
  MaskFilterInfo(const RRectF& rrect, const gfx::LinearGradient& gradient_mask)
      : rounded_corner_bounds_(rrect), gradient_mask_(gradient_mask) {}
  MaskFilterInfo(const RectF& bounds,
                 const RoundedCornersF& radii,
                 const gfx::LinearGradient& gradient_mask)
      : rounded_corner_bounds_(bounds, radii), gradient_mask_(gradient_mask) {}
  MaskFilterInfo(const MaskFilterInfo& copy) = default;
  ~MaskFilterInfo() = default;

  // The bounds the filter will be applied to.
  RectF bounds() const { return rounded_corner_bounds_.rect(); }

  // Defines the rounded corner bounds to clip.
  const RRectF& rounded_corner_bounds() const { return rounded_corner_bounds_; }

  // True if this contains a rounded corner mask.
  bool HasRoundedCorners() const {
    return rounded_corner_bounds_.HasRoundedCorners();
  }

  const std::optional<gfx::LinearGradient>& gradient_mask() const {
    return gradient_mask_;
  }

  // True if this contains an effective gradient mask (requires filter bounds).
  bool HasGradientMask() const {
    if (rounded_corner_bounds_.IsEmpty())
      return false;

    return gradient_mask_ && !gradient_mask_->IsEmpty();
  }

  // True if this contains no effective mask information.
  bool IsEmpty() const { return rounded_corner_bounds_.IsEmpty(); }

  // Transform the mask filter information. If the transform cannot be applied
  // (e.g. it would make rounded_corner_bounds_ invalid), rounded_corner_bounds_
  // will be set to empty.
  void ApplyTransform(const Transform& transform);
  void ApplyTransform(const AxisTransform2d& transform);

  std::string ToString() const;

 private:
  // The rounded corner bounds. This also defines the bounds that the mask
  // filter will be applied to.
  RRectF rounded_corner_bounds_;

  // Shader based linear gradient mask to be applied to a layer.
  std::optional<gfx::LinearGradient> gradient_mask_;
};

inline bool operator==(const MaskFilterInfo& lhs, const MaskFilterInfo& rhs) {
  return (lhs.rounded_corner_bounds() == rhs.rounded_corner_bounds()) &&
         (lhs.gradient_mask() == rhs.gradient_mask());
}

inline bool operator!=(const MaskFilterInfo& lhs, const MaskFilterInfo& rhs) {
  return !(lhs == rhs);
}

// This is declared here for use in gtest-based unit tests but is defined in
// the //ui/gfx:test_support target. Depend on that to use this in your unit
// test. This should not be used in production code - call ToString() instead.
void PrintTo(const MaskFilterInfo&, ::std::ostream* os);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_MASK_FILTER_INFO_H_

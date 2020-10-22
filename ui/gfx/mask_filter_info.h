// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MASK_FILTER_INFO_H_
#define UI_GFX_MASK_FILTER_INFO_H_

#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry_skia_export.h"
#include "ui/gfx/rrect_f.h"

namespace gfx {

class Transform;

class GEOMETRY_SKIA_EXPORT MaskFilterInfo {
 public:
  MaskFilterInfo() = default;
  MaskFilterInfo(const RRectF& rrect, bool is_fast_rounded_corner)
      : rounded_corner_bounds_(rrect),
        is_fast_rounded_corner_(is_fast_rounded_corner) {}
  MaskFilterInfo(const RectF& bounds,
                 const RoundedCornersF& radii,
                 bool is_fast_rounded_corner);
  MaskFilterInfo(const MaskFilterInfo& copy) = default;
  ~MaskFilterInfo() = default;

  // The bounds the filter will be applied to.
  RectF bounds() const { return rounded_corner_bounds_.rect(); }

  // Defines the rounded corner bounds to clip.
  const RRectF& rounded_corner_bounds() const { return rounded_corner_bounds_; }

  // If true, it makes the filter not trigger a render surface when the rounded
  // corners is defined if possible.
  bool is_fast_rounded_corner() const { return is_fast_rounded_corner_; }

  // True if this contains a rounded corner mask.
  bool HasRoundedCorners() const {
    return !IsEmpty() &&
           rounded_corner_bounds_.GetType() != RRectF::Type::kRect;
  }

  // True if this contains no effective mask information.
  bool IsEmpty() const { return rounded_corner_bounds_.IsEmpty(); }

  // Transform the mask information. Returns false if the transform
  // cannot be applied.
  bool Transform(const Transform& transform);

  std::string ToString() const;

 private:
  // The rounded corner bounds. This also defines the bounds that the mask
  // filter will be applied to.
  RRectF rounded_corner_bounds_;

  bool is_fast_rounded_corner_ = false;
};

inline bool operator==(const MaskFilterInfo& lhs, const MaskFilterInfo& rhs) {
  return lhs.rounded_corner_bounds() == rhs.rounded_corner_bounds() &&
         lhs.is_fast_rounded_corner() == rhs.is_fast_rounded_corner();
}

inline bool operator!=(const MaskFilterInfo& lhs, const MaskFilterInfo& rhs) {
  return !(lhs == rhs);
}

}  // namespace gfx

#endif  // UI_GFX_MASK_FILTER_INFO_H_

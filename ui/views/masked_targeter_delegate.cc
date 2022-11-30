// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/masked_targeter_delegate.h"

#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/view.h"

namespace views {

bool MaskedTargeterDelegate::DoesIntersectRect(const View* target,
                                               const gfx::Rect& rect) const {
  // Early return if |rect| does not even intersect the rectangular bounds
  // of |target|.
  if (!ViewTargeterDelegate::DoesIntersectRect(target, rect))
    return false;

  // Early return if |mask| is not a valid hit test mask.
  SkPath mask;
  if (!GetHitTestMask(&mask))
    return false;

  // Return whether or not |rect| intersects the custom hit test mask
  // of |target|.
  SkRegion clip_region;
  clip_region.setRect({0, 0, target->width(), target->height()});
  SkRegion mask_region;
  return mask_region.setPath(mask, clip_region) &&
         mask_region.intersects(RectToSkIRect(rect));
}

}  // namespace views

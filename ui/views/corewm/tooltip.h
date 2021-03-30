// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TOOLTIP_H_
#define UI_VIEWS_COREWM_TOOLTIP_H_

#include <string>

#include "ui/gfx/geometry/point.h"
#include "ui/views/views_export.h"

namespace aura {
class Window;
}

namespace views {
namespace corewm {

enum class TooltipPositionBehavior {
  // A centered tooltip will have its horizontal center aligned with the anchor
  // point x value. The top of the tooltip will be aligned with the anchor point
  // y value.
  kCentered,
  // A tooltip positioned relatively to the cursor will have its top-left corner
  // aligned with the anchor point. It will have an additional offset the size
  // of the cursor, resulting in the tooltip being positioned at the
  // bottom-right of the cursor.
  kRelativeToCursor,
};

struct VIEWS_EXPORT TooltipPosition {
  gfx::Point anchor_point;
  TooltipPositionBehavior behavior = TooltipPositionBehavior::kRelativeToCursor;
};

// Tooltip is responsible for showing the tooltip in an appropriate manner.
// Tooltip is used by TooltipController.
class VIEWS_EXPORT Tooltip {
 public:
  virtual ~Tooltip() = default;

  // Returns the max width of the tooltip when shown at the specified location.
  virtual int GetMaxWidth(const gfx::Point& location) const = 0;

  // Updates the text on the tooltip and resizes to fit.
  virtual void Update(aura::Window* window,
                      const std::u16string& tooltip_text,
                      const TooltipPosition& position) = 0;

  // Shows the tooltip at the specified location (in screen coordinates).
  virtual void Show() = 0;

  // Hides the tooltip.
  virtual void Hide() = 0;

  // Is the tooltip visible?
  virtual bool IsVisible() = 0;
};

}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_TOOLTIP_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TOOLTIP_H_
#define UI_VIEWS_COREWM_TOOLTIP_H_

#include "base/strings/string16.h"
#include "ui/views/views_export.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
}

namespace views {
namespace corewm {

// Tooltip is responsible for showing the tooltip in an appropriate manner.
// Tooltip is used by TooltipController.
class VIEWS_EXPORT Tooltip {
 public:
  virtual ~Tooltip() = default;

  // Returns the max width of the tooltip when shown at the specified location.
  virtual int GetMaxWidth(const gfx::Point& location) const = 0;

  // Updates the text on the tooltip and resizes to fit.
  virtual void SetText(aura::Window* window,
                       const base::string16& tooltip_text,
                       const gfx::Point& location) = 0;

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

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TOOLTIP_H_
#define UI_VIEWS_COREWM_TOOLTIP_H_

#include <string>

#include "ui/views/views_export.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
}  // namespace gfx

namespace wm {
class TooltipObserver;
}

namespace views::corewm {

enum class TooltipTrigger {
  kCursor,
  kKeyboard,
};

// Tooltip is responsible for showing the tooltip in an appropriate manner.
// Tooltip is used by TooltipController.
class VIEWS_EXPORT Tooltip {
 public:
  virtual ~Tooltip() = default;

  virtual void AddObserver(wm::TooltipObserver* observer) = 0;
  virtual void RemoveObserver(wm::TooltipObserver* observer) = 0;

  // Returns the max width of the tooltip when shown at the specified location.
  virtual int GetMaxWidth(const gfx::Point& location) const = 0;
  virtual void SetMaxWidth(int width) {}

  // Updates the text on the tooltip and resizes to fit.
  // `position` is relative to `window` and in `window` coordinate space.
  virtual void Update(aura::Window* window,
                      const std::u16string& tooltip_text,
                      const gfx::Point& position,
                      const TooltipTrigger trigger) = 0;

  // Shows the tooltip at the specified location (in screen coordinates).
  virtual void Show() = 0;

  // Hides the tooltip.
  virtual void Hide() = 0;

  // Is the tooltip visible?
  virtual bool IsVisible() = 0;
};

}  // namespace views::corewm

#endif  // UI_VIEWS_COREWM_TOOLTIP_H_

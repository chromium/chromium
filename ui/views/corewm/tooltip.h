// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TOOLTIP_H_
#define UI_VIEWS_COREWM_TOOLTIP_H_

#include <string>

#include "build/chromeos_buildflags.h"
#include "ui/views/views_export.h"

namespace aura {
class Window;
}

namespace base {
class TimeDelta;
}

namespace gfx {
class Point;
class Rect;
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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Sets show/hide delay. Only used for Lacros.
  virtual void SetDelay(const base::TimeDelta& show_delay,
                        const base::TimeDelta& hide_delay) {}

  // Called when tooltip is shown/hidden on server.
  // Only used by Lacros.
  virtual void OnTooltipShownOnServer(const std::u16string& text,
                                      const gfx::Rect& bounds) {}
  virtual void OnTooltipHiddenOnServer() {}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Shows the tooltip at the specified location (in screen coordinates).
  virtual void Show() = 0;

  // Hides the tooltip.
  virtual void Hide() = 0;

  // Is the tooltip visible?
  virtual bool IsVisible() = 0;

 protected:
  // Max visual tooltip width. If a tooltip is greater than this width, it will
  // be wrapped.
  static constexpr int kTooltipMaxWidth = 800;
};

}  // namespace views::corewm

#endif  // UI_VIEWS_COREWM_TOOLTIP_H_

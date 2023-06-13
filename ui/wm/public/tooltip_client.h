// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_PUBLIC_TOOLTIP_CLIENT_H_
#define UI_WM_PUBLIC_TOOLTIP_CLIENT_H_

#include "ui/aura/window.h"
#include "ui/gfx/font.h"
#include "ui/wm/public/wm_public_export.h"

namespace base {
class TimeDelta;
}

namespace gfx {
class Point;
}

namespace wm {

class ScopedTooltipDisabler;

class WM_PUBLIC_EXPORT TooltipClient {
 public:
  // Returns the max width of the tooltip when shown at the specified location.
  virtual int GetMaxWidth(const gfx::Point& point) const = 0;

  // Informs the shell tooltip manager of change in tooltip for window |target|.
  virtual void UpdateTooltip(aura::Window* target) = 0;

  // Informs the shell tooltip manager of change in tooltip for window |target|,
  // triggered by a keyboard and passing the bounds of the newly focused item
  // within the target. The bounds are relative to the window in which it is
  // contained.
  virtual void UpdateTooltipFromKeyboard(const gfx::Rect& bounds,
                                         aura::Window* target) = 0;

  // Returns true if the tooltip was set from a keyboard action.
  virtual bool IsTooltipSetFromKeyboard(aura::Window* target) = 0;

  // Sets the time after which the tooltip is hidden for Window |target|. If
  // |timeout_in_ms| is <= 0, the tooltip is shown indefinitely.
  virtual void SetHideTooltipTimeout(aura::Window* target,
                                     base::TimeDelta timeout) = 0;

 protected:
  // Enables/Disables tooltips. This is treated as a reference count. Consumers
  // must use ScopedTooltipDisabler to enable/disabled tooltips.
  virtual void SetTooltipsEnabled(bool enable) = 0;

 private:
  friend class ScopedTooltipDisabler;
};

WM_PUBLIC_EXPORT void SetTooltipClient(aura::Window* root_window,
                                       TooltipClient* client);
WM_PUBLIC_EXPORT TooltipClient* GetTooltipClient(aura::Window* root_window);

// Sets the text for the tooltip. The id is used to determine uniqueness when
// the text does not change. For example, if the tooltip text does not change,
// but the id does then the position of the tooltip is updated.
WM_PUBLIC_EXPORT void SetTooltipText(aura::Window* window,
                                     std::u16string* tooltip_text);
WM_PUBLIC_EXPORT void SetTooltipId(aura::Window* window, void* id);
WM_PUBLIC_EXPORT const std::u16string GetTooltipText(aura::Window* window);
WM_PUBLIC_EXPORT const void* GetTooltipId(aura::Window* window);

WM_PUBLIC_EXPORT extern const aura::WindowProperty<TooltipClient*>* const
    kRootWindowTooltipClientKey;
WM_PUBLIC_EXPORT extern const aura::WindowProperty<void*>* const kTooltipIdKey;
WM_PUBLIC_EXPORT extern const aura::WindowProperty<std::u16string*>* const
    kTooltipTextKey;

}  // namespace wm

#endif  // UI_WM_PUBLIC_TOOLTIP_CLIENT_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_PUBLIC_TOOLTIP_OBSERVER_H_
#define UI_WM_PUBLIC_TOOLTIP_OBSERVER_H_

#include <string>

#include "base/observer_list_types.h"
#include "ui/wm/public/wm_public_export.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace wm {

class WM_PUBLIC_EXPORT TooltipObserver : public base::CheckedObserver {
 public:
  ~TooltipObserver() override = default;

  // Called when tooltip is shown.
  // `target` is a target window of show tooltip. This may be null if the target
  // window is already destroyed,
  // `bounds` is relative to the target window position.
  // TODO(crbug.com/40246673): Use tooltip's parent window for `target`.
  virtual void OnTooltipShown(aura::Window* target,
                              const std::u16string& text,
                              const gfx::Rect& bounds) = 0;

  // Called when tooltip is hidden.
  // `target` is a target window of show tooltip. This may be null if the target
  // window is already destroyed,
  // TODO(crbug.com/40246673): Use tooltip's parent window for `target`.
  virtual void OnTooltipHidden(aura::Window* target) = 0;
};

}  // namespace wm

#endif  // UI_WM_PUBLIC_TOOLTIP_OBSERVER_H_

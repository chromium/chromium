// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_PUBLIC_TOOLTIP_OBSERVER_H_
#define UI_WM_PUBLIC_TOOLTIP_OBSERVER_H_

#include <string>

#include "ui/wm/public/wm_public_export.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace wm {

class WM_PUBLIC_EXPORT TooltipObserver {
 public:
  virtual ~TooltipObserver() = default;

  // Called when tooltip whose parent window's toplevel window is `target` is
  // shown. `bounds` is relative to `target` position.
  // TODO(crbug.com/1385219): Use tooltip's parent window for `target`.
  virtual void OnTooltipShown(aura::Window* target,
                              const std::u16string& text,
                              const gfx::Rect& bounds) = 0;

  // Called when tooltip whose parent window's toplevel window is `target` is
  // hidden.
  // TODO(crbug.com/1385219): Use tooltip's parent window for `target`.
  virtual void OnTooltipHidden(aura::Window* window) = 0;
};

}  // namespace wm

#endif  // UI_WM_PUBLIC_TOOLTIP_OBSERVER_H_

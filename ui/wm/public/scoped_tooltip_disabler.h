// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_PUBLIC_SCOPED_TOOLTIP_DISABLER_H_
#define UI_WM_PUBLIC_SCOPED_TOOLTIP_DISABLER_H_

#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/public/wm_public_export.h"

namespace wm {

// Use to temporarily disable tooltips.
class WM_PUBLIC_EXPORT ScopedTooltipDisabler : aura::WindowObserver {
 public:
  // Disables tooltips on |window| (does nothing if |window| is nullptr).
  // Tooltips are re-enabled from the destructor when there are no most
  // outstanding ScopedTooltipDisablers for |window|.
  explicit ScopedTooltipDisabler(aura::Window* window);

  ScopedTooltipDisabler(const ScopedTooltipDisabler&) = delete;
  ScopedTooltipDisabler& operator=(const ScopedTooltipDisabler&) = delete;

  ~ScopedTooltipDisabler() override;

 private:
  // Reenables the tooltips on the TooltipClient.
  void EnableTooltips();

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // The RootWindow to disable Tooltips on; nullptr if the Window passed to the
  // constructor was not in a root or the root has been destroyed.
  raw_ptr<aura::Window> root_;
};

}  // namespace wm

#endif  // UI_WM_PUBLIC_SCOPED_TOOLTIP_DISABLER_H_

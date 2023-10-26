// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_TOOLTIP_MANAGER_MAC_H_
#define UI_VIEWS_COCOA_TOOLTIP_MANAGER_MAC_H_

#include "ui/views/widget/tooltip_manager.h"

#include "base/memory/raw_ptr.h"

namespace remote_cocoa::mojom {
class NativeWidgetNSWindow;
}  // namespace remote_cocoa::mojom

namespace views {

// Manages native Cocoa tooltips for the given NativeWidgetNSWindowHostImpl.
class TooltipManagerMac : public TooltipManager {
 public:
  explicit TooltipManagerMac(remote_cocoa::mojom::NativeWidgetNSWindow* bridge);

  TooltipManagerMac(const TooltipManagerMac&) = delete;
  TooltipManagerMac& operator=(const TooltipManagerMac&) = delete;

  ~TooltipManagerMac() override;

  // TooltipManager:
  int GetMaxWidth(const gfx::Point& location) const override;
  const gfx::FontList& GetFontList() const override;
  void UpdateTooltip() override;
  void UpdateTooltipForFocus(View* view) override;
  void TooltipTextChanged(View* view) override;

 private:
  raw_ptr<remote_cocoa::mojom::NativeWidgetNSWindow, DanglingUntriaged>
      bridge_;  // Weak. Owned by the owner of this.
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_TOOLTIP_MANAGER_MAC_H_

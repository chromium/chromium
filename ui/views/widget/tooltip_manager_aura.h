// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_TOOLTIP_MANAGER_AURA_H_
#define UI_VIEWS_WIDGET_TOOLTIP_MANAGER_AURA_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/tooltip_manager.h"

namespace aura {
class Window;
}

namespace gfx {
class FontList;
}

namespace views {
namespace internal {
class NativeWidgetPrivate;
}
// TooltipManager implementation for Aura.
class VIEWS_EXPORT TooltipManagerAura : public TooltipManager {
 public:
  explicit TooltipManagerAura(internal::NativeWidgetPrivate* native_widget);

  TooltipManagerAura(const TooltipManagerAura&) = delete;
  TooltipManagerAura& operator=(const TooltipManagerAura&) = delete;

  ~TooltipManagerAura() override;

  // If |source| has capture this finds the Widget under the mouse and invokes
  // UpdateTooltip() on it's TooltipManager. This is necessary as when capture
  // is held mouse events are only delivered to the Window that has capture even
  // though we may show tooltips for the Window under the mouse.
  static void UpdateTooltipManagerForCapture(
      internal::NativeWidgetPrivate* source);

  // Returns the FontList used by all TooltipManagerAuras.
  static const gfx::FontList& GetDefaultFontList();

  // TooltipManager:
  int GetMaxWidth(const gfx::Point& location) const override;
  const gfx::FontList& GetFontList() const override;
  void UpdateTooltip() override;
  void UpdateTooltipForFocus(View* view) override;
  void TooltipTextChanged(View* view) override;

 private:
  View* GetViewUnderPoint(const gfx::Point& point);
  void UpdateTooltipForTarget(View* target,
                              const gfx::Point& point,
                              aura::Window* root_window);

  // Returns the Window the tooltip text is installed on.
  aura::Window* GetWindow();

  raw_ptr<internal::NativeWidgetPrivate> native_widget_;
  std::u16string tooltip_text_;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_TOOLTIP_MANAGER_AURA_H_

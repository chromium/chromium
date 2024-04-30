// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SCROLLBAR_SCROLL_BAR_BUTTON_H_
#define UI_VIEWS_CONTROLS_SCROLLBAR_SCROLL_BAR_BUTTON_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/size.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/repeat_controller.h"
#include "ui/views/views_export.h"

namespace base {
class TickClock;
}

namespace gfx {
class Canvas;
}

namespace views {

// A button that activates on mouse pressed rather than released, and that
// continues to fire the clicked action as the mouse button remains pressed
// down on the button.
class VIEWS_EXPORT ScrollBarButton : public Button {
  METADATA_HEADER(ScrollBarButton, Button)

 public:
  enum class Type {
    kUp,
    kDown,
    kLeft,
    kRight,
  };

  ScrollBarButton(PressedCallback callback,
                  Type type,
                  const base::TickClock* tick_clock = nullptr);
  ScrollBarButton(const ScrollBarButton&) = delete;
  ScrollBarButton& operator=(const ScrollBarButton&) = delete;
  ~ScrollBarButton() override;

  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override;

 protected:
  // Button
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  void OnThemeChanged() override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  ui::NativeTheme::ExtraParams GetNativeThemeParams() const;
  ui::NativeTheme::Part GetNativeThemePart() const;
  ui::NativeTheme::State GetNativeThemeState() const;
  void RepeaterNotifyClick();

  Type type_;

  // The repeat controller that we use to repeatedly click the button when the
  // mouse button is down.
  RepeatController repeater_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SCROLLBAR_SCROLL_BAR_BUTTON_H_

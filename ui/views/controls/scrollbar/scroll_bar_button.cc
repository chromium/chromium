// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scrollbar/scroll_bar_button.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/time/tick_clock.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"

namespace views {

ScrollBarButton::ScrollBarButton(PressedCallback callback,
                                 Type type,
                                 const base::TickClock* tick_clock)
    : Button(std::move(callback)),
      type_(type),
      repeater_(base::BindRepeating(&ScrollBarButton::RepeaterNotifyClick,
                                    base::Unretained(this)),
                tick_clock) {
  SetFlipCanvasOnPaintForRTLUI(true);
  // Not focusable by default.
  SetFocusBehavior(FocusBehavior::NEVER);
}

ScrollBarButton::~ScrollBarButton() = default;

gfx::Size ScrollBarButton::CalculatePreferredSize(
    const SizeBounds& /*available_size*/) const {
  if (!GetWidget())
    return gfx::Size();
  return GetNativeTheme()->GetPartSize(
      GetNativeThemePart(), GetNativeThemeState(), GetNativeThemeParams());
}

bool ScrollBarButton::OnMousePressed(const ui::MouseEvent& event) {
  Button::NotifyClick(event);
  repeater_.Start();
  return true;
}

void ScrollBarButton::OnMouseReleased(const ui::MouseEvent& event) {
  OnMouseCaptureLost();
}

void ScrollBarButton::OnMouseCaptureLost() {
  repeater_.Stop();
}

void ScrollBarButton::OnThemeChanged() {
  Button::OnThemeChanged();
  PreferredSizeChanged();
}

void ScrollBarButton::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::Rect bounds(GetPreferredSize({}));
  GetNativeTheme()->Paint(canvas->sk_canvas(), GetColorProvider(),
                          GetNativeThemePart(), GetNativeThemeState(), bounds,
                          GetNativeThemeParams());
}

ui::NativeTheme::ExtraParams ScrollBarButton::GetNativeThemeParams() const {
  ui::NativeTheme::ScrollbarArrowExtraParams scrollbar_arrow;
  scrollbar_arrow.is_hovering = GetState() == Button::STATE_HOVERED;
  return ui::NativeTheme::ExtraParams(scrollbar_arrow);
}

ui::NativeTheme::Part ScrollBarButton::GetNativeThemePart() const {
  switch (type_) {
    case Type::kUp:
      return ui::NativeTheme::kScrollbarUpArrow;
    case Type::kDown:
      return ui::NativeTheme::kScrollbarDownArrow;
    case Type::kLeft:
      return ui::NativeTheme::kScrollbarLeftArrow;
    case Type::kRight:
      return ui::NativeTheme::kScrollbarRightArrow;
  }

  NOTREACHED();
}

ui::NativeTheme::State ScrollBarButton::GetNativeThemeState() const {
  switch (GetState()) {
    case Button::STATE_HOVERED:
      return ui::NativeTheme::kHovered;
    case Button::STATE_PRESSED:
      return ui::NativeTheme::kPressed;
    case Button::STATE_DISABLED:
      return ui::NativeTheme::kDisabled;
    case Button::STATE_NORMAL:
      return ui::NativeTheme::kNormal;
    case Button::STATE_COUNT:
      break;
  }

  NOTREACHED();
}

void ScrollBarButton::RepeaterNotifyClick() {
  gfx::Point cursor_point =
      display::Screen::GetScreen()->GetCursorScreenPoint();
  ui::MouseEvent event(ui::EventType::kMouseReleased, cursor_point,
                       cursor_point, ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  Button::NotifyClick(event);
}

BEGIN_METADATA(ScrollBarButton)
END_METADATA

}  // namespace views

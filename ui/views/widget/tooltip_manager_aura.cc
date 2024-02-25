// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/tooltip_manager_aura.h"

#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/tooltip_client.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// TooltipManagerAura public:

TooltipManagerAura::TooltipManagerAura(
    internal::NativeWidgetPrivate* native_widget)
    : native_widget_(native_widget) {
  wm::SetTooltipText(GetWindow(), &tooltip_text_);
}

TooltipManagerAura::~TooltipManagerAura() {
  wm::SetTooltipText(GetWindow(), nullptr);
}

// static
const gfx::FontList& TooltipManagerAura::GetDefaultFontList() {
  return ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::BaseFont);
}

// static
void TooltipManagerAura::UpdateTooltipManagerForCapture(
    internal::NativeWidgetPrivate* source) {
  if (!source->HasCapture())
    return;

  aura::Window* root_window = source->GetNativeView()->GetRootWindow();
  if (!root_window)
    return;

  gfx::Point screen_loc(
      root_window->GetHost()->dispatcher()->GetLastMouseLocationInRoot());
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root_window);
  if (!screen_position_client)
    return;
  screen_position_client->ConvertPointToScreen(root_window, &screen_loc);
  display::Screen* screen = display::Screen::GetScreen();
  aura::Window* target = screen->GetWindowAtScreenPoint(screen_loc);
  if (!target)
    return;
  gfx::Point target_loc(screen_loc);
  screen_position_client =
      aura::client::GetScreenPositionClient(target->GetRootWindow());
  if (!screen_position_client)
    return;
  screen_position_client->ConvertPointFromScreen(target, &target_loc);
  target = target->GetEventHandlerForPoint(target_loc);
  while (target) {
    internal::NativeWidgetPrivate* target_native_widget =
        internal::NativeWidgetPrivate::GetNativeWidgetForNativeView(target);
    if (target_native_widget == source)
      return;

    if (target_native_widget) {
      if (target_native_widget->GetTooltipManager())
        target_native_widget->GetTooltipManager()->UpdateTooltip();
      return;
    }
    target = target->parent();
  }
}

////////////////////////////////////////////////////////////////////////////////
// TooltipManagerAura, TooltipManager implementation:

const gfx::FontList& TooltipManagerAura::GetFontList() const {
  return GetDefaultFontList();
}

int TooltipManagerAura::GetMaxWidth(const gfx::Point& point) const {
  return wm::GetTooltipClient(native_widget_->GetNativeView()->GetRootWindow())
      ->GetMaxWidth(point);
}

void TooltipManagerAura::UpdateTooltip() {
  aura::Window* root_window = GetWindow()->GetRootWindow();
  if (wm::GetTooltipClient(root_window)) {
    if (!native_widget_->IsVisible()) {
      UpdateTooltipForTarget(nullptr, gfx::Point(), root_window);
      return;
    }
    gfx::Point view_point =
        root_window->GetHost()->dispatcher()->GetLastMouseLocationInRoot();
    aura::Window::ConvertPointToTarget(root_window, GetWindow(), &view_point);
    View* view = GetViewUnderPoint(view_point);
    UpdateTooltipForTarget(view, view_point, root_window);
  }
}

void TooltipManagerAura::UpdateTooltipForFocus(View* view) {
  aura::Window* root_window = GetWindow()->GetRootWindow();
  if (wm::GetTooltipClient(root_window)) {
    tooltip_text_ = view->GetTooltipText(gfx::Point());

    auto bounds = gfx::Rect(gfx::Point(), view->size());
    auto root_bounds = View::ConvertRectToTarget(
        view, view->GetWidget()->GetRootView(), bounds);

    wm::GetTooltipClient(root_window)
        ->UpdateTooltipFromKeyboard(root_bounds, GetWindow());
  }
}

void TooltipManagerAura::TooltipTextChanged(View* view) {
  aura::Window* root_window = GetWindow()->GetRootWindow();
  if (wm::GetTooltipClient(root_window)) {
    gfx::Point view_point =
        root_window->GetHost()->dispatcher()->GetLastMouseLocationInRoot();
    aura::Window::ConvertPointToTarget(root_window, GetWindow(), &view_point);
    View* target = GetViewUnderPoint(view_point);
    if (target != view)
      return;
    UpdateTooltipForTarget(view, view_point, root_window);
  }
}

View* TooltipManagerAura::GetViewUnderPoint(const gfx::Point& point) {
  View* root_view = native_widget_->GetWidget()
                        ? native_widget_->GetWidget()->GetRootView()
                        : nullptr;

  if (root_view)
    return root_view->GetTooltipHandlerForPoint(point);
  return nullptr;
}

void TooltipManagerAura::UpdateTooltipForTarget(View* target,
                                                const gfx::Point& point,
                                                aura::Window* root_window) {
  if (target) {
    gfx::Point view_point = point;
    View::ConvertPointFromWidget(target, &view_point);
    tooltip_text_ = target->GetTooltipText(view_point);
  } else {
    tooltip_text_.clear();
  }

  wm::SetTooltipId(GetWindow(), target);

  wm::GetTooltipClient(root_window)->UpdateTooltip(GetWindow());
}

aura::Window* TooltipManagerAura::GetWindow() {
  return native_widget_->GetNativeView();
}

}  // namespace views.

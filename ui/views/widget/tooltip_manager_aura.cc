// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/tooltip_manager_aura.h"

#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/tooltip_client.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// TooltipManagerAura public:

TooltipManagerAura::TooltipManagerAura(Widget* widget) : widget_(widget) {
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
void TooltipManagerAura::UpdateTooltipManagerForCapture(Widget* source) {
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
    Widget* target_widget = Widget::GetWidgetForNativeView(target);
    if (target_widget == source)
      return;

    if (target_widget) {
      if (target_widget->GetTooltipManager())
        target_widget->GetTooltipManager()->UpdateTooltip();
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
  return wm::GetTooltipClient(widget_->GetNativeView()->GetRootWindow())
      ->GetMaxWidth(point);
}

void TooltipManagerAura::UpdateTooltip() {
  aura::Window* root_window = GetWindow()->GetRootWindow();
  if (wm::GetTooltipClient(root_window)) {
    if (!widget_->IsVisible()) {
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
  View* root_view = widget_->GetRootView();
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
  return widget_->GetNativeView();
}

}  // namespace views.

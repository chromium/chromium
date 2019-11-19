// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_host_root_view.h"

#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace views {

MenuHostRootView::MenuHostRootView(Widget* widget, SubmenuView* submenu)
    : internal::RootView(widget), submenu_(submenu) {}

bool MenuHostRootView::OnMousePressed(const ui::MouseEvent& event) {
  return GetMenuControllerForInputEvents() &&
         GetMenuControllerForInputEvents()->OnMousePressed(submenu_, event);
}

bool MenuHostRootView::OnMouseDragged(const ui::MouseEvent& event) {
  return GetMenuControllerForInputEvents() &&
         GetMenuControllerForInputEvents()->OnMouseDragged(submenu_, event);
}

void MenuHostRootView::OnMouseReleased(const ui::MouseEvent& event) {
  if (GetMenuControllerForInputEvents())
    GetMenuControllerForInputEvents()->OnMouseReleased(submenu_, event);
}

void MenuHostRootView::OnMouseMoved(const ui::MouseEvent& event) {
  if (GetMenuControllerForInputEvents())
    GetMenuControllerForInputEvents()->OnMouseMoved(submenu_, event);
}

bool MenuHostRootView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  return GetMenuControllerForInputEvents() &&
         GetMenuControllerForInputEvents()->OnMouseWheel(submenu_, event);
}

View* MenuHostRootView::GetTooltipHandlerForPoint(const gfx::Point& point) {
  return GetMenuControllerForInputEvents()
             ? GetMenuControllerForInputEvents()->GetTooltipHandlerForPoint(
                   submenu_, point)
             : nullptr;
}

void MenuHostRootView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (GetMenuControllerForInputEvents())
    GetMenuControllerForInputEvents()->ViewHierarchyChanged(submenu_, details);
  RootView::ViewHierarchyChanged(details);
}

bool MenuHostRootView::ProcessMousePressed(const ui::MouseEvent& event) {
  return RootView::OnMousePressed(event);
}

bool MenuHostRootView::ProcessMouseDragged(const ui::MouseEvent& event) {
  return RootView::OnMouseDragged(event);
}

void MenuHostRootView::ProcessMouseReleased(const ui::MouseEvent& event) {
  RootView::OnMouseReleased(event);
}

void MenuHostRootView::ProcessMouseMoved(const ui::MouseEvent& event) {
  RootView::OnMouseMoved(event);
}

View* MenuHostRootView::ProcessGetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return RootView::GetTooltipHandlerForPoint(point);
}

void MenuHostRootView::OnEventProcessingFinished(ui::Event* event) {
  RootView::OnEventProcessingFinished(event);

  // Forward unhandled gesture events to our menu controller.
  // TODO(tdanderson): Investigate whether this should be moved into a
  //                   post-target handler installed on |this| instead
  //                   (invoked only if event->target() == this).
  if (event->IsGestureEvent() && !event->handled() && GetMenuController())
    GetMenuController()->OnGestureEvent(submenu_, event->AsGestureEvent());
}

MenuController* MenuHostRootView::GetMenuController() {
  return submenu_ ? submenu_->GetMenuItem()->GetMenuController() : nullptr;
}

MenuController* MenuHostRootView::GetMenuControllerForInputEvents() {
  return GetMenuController() && GetMenuController()->CanProcessInputEvents()
             ? GetMenuController()
             : nullptr;
}

}  // namespace views

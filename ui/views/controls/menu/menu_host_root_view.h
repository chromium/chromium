// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_HOST_ROOT_VIEW_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_HOST_ROOT_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/widget/root_view.h"

namespace views {

class MenuController;
class SubmenuView;

// MenuHostRootView is the RootView of the window showing the menu.
// SubmenuView's scroll view is added as a child of MenuHostRootView.
// MenuHostRootView forwards relevant events to the MenuController.
//
// As all the menu items are owned by the root menu item, care must be taken
// such that when MenuHostRootView is deleted it doesn't delete the menu items.
class MenuHostRootView : public internal::RootView {
  METADATA_HEADER(MenuHostRootView, internal::RootView)

 public:
  MenuHostRootView(Widget* widget, SubmenuView* submenu);

  MenuHostRootView(const MenuHostRootView&) = delete;
  MenuHostRootView& operator=(const MenuHostRootView&) = delete;

  void ClearSubmenu() { submenu_ = nullptr; }

  // Overridden from View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

  bool ProcessMousePressed(const ui::MouseEvent& event);
  bool ProcessMouseDragged(const ui::MouseEvent& event);
  void ProcessMouseReleased(const ui::MouseEvent& event);
  void ProcessMouseMoved(const ui::MouseEvent& event);
  View* ProcessGetTooltipHandlerForPoint(const gfx::Point& point);

 private:
  // ui::EventProcessor:
  void OnEventProcessingFinished(ui::Event* event) override;

  // Returns the MenuController for this MenuHostRootView.
  MenuController* GetMenuController();
  MenuController* GetMenuControllerForInputEvents();

  // The SubmenuView we contain.
  raw_ptr<SubmenuView> submenu_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_HOST_ROOT_VIEW_H_

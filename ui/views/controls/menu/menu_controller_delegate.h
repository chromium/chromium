// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_CONTROLLER_DELEGATE_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_CONTROLLER_DELEGATE_H_

namespace views {

class MenuItemView;

// This is internal as there should be no need for usage of this class outside
// of views.
namespace internal {

// Used by MenuController to notify of interesting events that are intended for
// the class using MenuController. This is implemented by MenuRunnerImpl.
class MenuControllerDelegate {
 public:
  enum NotifyType { NOTIFY_DELEGATE, DONT_NOTIFY_DELEGATE };

  // Invoked when MenuController closes. unless the owner deletes the
  // MenuController during MenuDelegate::ExecuteCommand. |mouse_event_flags| are
  // the flags set on the ui::MouseEvent which selected |menu|, otherwise 0.
  virtual void OnMenuClosed(NotifyType type,
                            MenuItemView* menu,
                            int mouse_event_flags) = 0;

  // Invoked when the MenuDelegate::GetSiblingMenu() returns non-NULL.
  virtual void SiblingMenuCreated(MenuItemView* menu) = 0;

 protected:
  virtual ~MenuControllerDelegate() = default;
};

}  // namespace internal

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_CONTROLLER_DELEGATE_H_

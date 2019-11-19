// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_HOST_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_HOST_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

class SubmenuView;
class View;
class Widget;

namespace internal {

// This class is internal to views.
class PreMenuEventDispatchHandler;

}  // internal

namespace test {
class MenuControllerTest;
}  // test

// SubmenuView uses a MenuHost to house the SubmenuView.
//
// SubmenuView owns the MenuHost. When SubmenuView is done with the MenuHost
// |DestroyMenuHost| is invoked. The one exception to this is if the native
// OS destroys the widget out from under us, in which case |MenuHostDestroyed|
// is invoked back on the SubmenuView and the SubmenuView then drops references
// to the MenuHost.
class MenuHost : public Widget, public WidgetObserver {
 public:
  explicit MenuHost(SubmenuView* submenu);
  ~MenuHost() override;

  // Initializes and shows the MenuHost.
  // WARNING: |parent| may be NULL.
  void InitMenuHost(Widget* parent,
                    const gfx::Rect& bounds,
                    View* contents_view,
                    bool do_capture);

  // Returns true if the menu host is visible.
  bool IsMenuHostVisible();

  // Shows the menu host. If |do_capture| is true the menu host should do a
  // mouse grab.
  void ShowMenuHost(bool do_capture);

  // Hides the menu host.
  void HideMenuHost();

  // Destroys and deletes the menu host.
  void DestroyMenuHost();

  // Sets the bounds of the menu host.
  void SetMenuHostBounds(const gfx::Rect& bounds);

  // Releases a mouse grab installed by |ShowMenuHost|.
  void ReleaseMenuHostCapture();

 private:
  friend class test::MenuControllerTest;

  // Widget:
  internal::RootView* CreateRootView() override;
  void OnMouseCaptureLost() override;
  void OnNativeWidgetDestroyed() override;
  void OnOwnerClosing() override;
  void OnDragWillStart() override;
  void OnDragComplete() override;

  // WidgetObserver:
  void OnWidgetDestroying(Widget* widget) override;

  // Parent of the MenuHost widget.
  Widget* owner_ = nullptr;

  // The view we contain.
  SubmenuView* submenu_;

  // If true, DestroyMenuHost has been invoked.
  bool destroying_;

  // If true and capture is lost we don't notify the delegate.
  bool ignore_capture_lost_;

#if !defined(OS_MACOSX)
  // Handles raw touch events at the moment.
  std::unique_ptr<internal::PreMenuEventDispatchHandler> pre_dispatch_handler_;
#endif

  DISALLOW_COPY_AND_ASSIGN(MenuHost);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_HOST_H_

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_HOST_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_HOST_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/base/owned_window_anchor.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

class SubmenuView;
class View;
class Widget;

namespace internal {
#if defined(USE_AURA)
// This class is internal to views.
class PreMenuEventDispatchHandler;
#endif  // defined(USE_AURA)
}  // namespace internal

namespace test {
class MenuControllerTest;
}  // namespace test

// SubmenuView uses a MenuHost to house the SubmenuView.
//
// SubmenuView owns the MenuHost. When SubmenuView is done with the MenuHost
// |DestroyMenuHost| is invoked. The one exception to this is if the native
// OS destroys the widget out from under us, in which case |MenuHostDestroyed|
// is invoked back on the SubmenuView and the SubmenuView then drops references
// to the MenuHost.
class MenuHost : public Widget, public WidgetObserver {
 public:
  struct InitParams {
    raw_ptr<Widget> parent = nullptr;
    gfx::Rect bounds;
    raw_ptr<View> contents_view = nullptr;
    bool do_capture = false;
    gfx::NativeView native_view_for_gestures;
    ui::MenuType menu_type = ui::MenuType::kRootContextMenu;
    // Window that is stacked below a new menu window (can be different from the
    // |parent|).
    raw_ptr<Widget> context = nullptr;

    // Additional information that helps to position anchored windows in such
    // backends as Wayland.
    ui::OwnedWindowAnchor owned_window_anchor;
  };

  explicit MenuHost(SubmenuView* submenu);

  MenuHost(const MenuHost&) = delete;
  MenuHost& operator=(const MenuHost&) = delete;

  ~MenuHost() override;

  // Initializes and shows the MenuHost.
  // WARNING: |init_params.parent| may be NULL.
  void InitMenuHost(const InitParams& init_params);

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

  // Sets the anchor of the menu host.
  void SetMenuHostOwnedWindowAnchor(const ui::OwnedWindowAnchor& anchor);

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
  Widget* GetPrimaryWindowWidget() override;

  // WidgetObserver:
  void OnWidgetDestroying(Widget* widget) override;

  // Parent of the MenuHost widget.
  raw_ptr<Widget, DanglingUntriaged> owner_ = nullptr;

  gfx::NativeView native_view_for_gestures_ = nullptr;

  // The view we contain.
  raw_ptr<SubmenuView, DanglingUntriaged> submenu_;

  // If true, DestroyMenuHost has been invoked.
  bool destroying_ = false;

  // If true and capture is lost we don't notify the delegate.
  bool ignore_capture_lost_ = false;

#if defined(USE_AURA)
  // Handles raw touch events at the moment.
  std::unique_ptr<internal::PreMenuEventDispatchHandler> pre_dispatch_handler_;
#endif  // defined(USE_AURA)
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_HOST_H_

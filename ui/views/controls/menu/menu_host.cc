// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_host.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/aura/window_observer.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_host_root_view.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/round_rect_painter.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/widget.h"

#if !defined(OS_APPLE)
#include "ui/aura/window.h"
#endif

namespace views {

namespace internal {

#if !defined(OS_APPLE)
// This class adds itself as the pre target handler for the |window|
// passed in. It currently handles touch events and forwards them to the
// controller. Reason for this approach is views does not get raw touch
// events which we need to determine if a touch happened outside the bounds
// of the menu.
class PreMenuEventDispatchHandler : public ui::EventHandler,
                                    aura::WindowObserver {
 public:
  PreMenuEventDispatchHandler(const MenuController* controller,
                              SubmenuView* submenu,
                              aura::Window* window)
      : menu_controller_(const_cast<MenuController*>(controller)),
        submenu_(submenu),
        window_(window) {
    window_->AddPreTargetHandler(this);
    window_->AddObserver(this);
  }

  ~PreMenuEventDispatchHandler() override { StopObserving(); }

  // ui::EventHandler overrides.
  void OnTouchEvent(ui::TouchEvent* event) override {
    menu_controller_->OnTouchEvent(submenu_, event);
  }

  // aura::WindowObserver overrides.
  void OnWindowDestroying(aura::Window* window) override {
    DCHECK(window_ == window);
    StopObserving();
  }

 private:
  void StopObserving() {
    if (!window_)
      return;
    window_->RemovePreTargetHandler(this);
    window_->RemoveObserver(this);
    window_ = nullptr;
  }

  MenuController* menu_controller_;
  SubmenuView* submenu_;
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(PreMenuEventDispatchHandler);
};
#endif  // OS_APPLE

void TransferGesture(Widget* source, Widget* target) {
#if defined(OS_APPLE)
  NOTIMPLEMENTED();
#else   // !defined(OS_APPLE)
  source->GetGestureRecognizer()->TransferEventsTo(
      source->GetNativeView(), target->GetNativeView(),
      ui::TransferTouchesBehavior::kDontCancel);
#endif  // defined(OS_APPLE)
}

}  // namespace internal

////////////////////////////////////////////////////////////////////////////////
// MenuHost, public:

MenuHost::MenuHost(SubmenuView* submenu)
    : submenu_(submenu), destroying_(false), ignore_capture_lost_(false) {
  set_auto_release_capture(false);
}

MenuHost::~MenuHost() {
  if (owner_)
    owner_->RemoveObserver(this);
  CHECK(!IsInObserverList());
}

void MenuHost::InitMenuHost(Widget* parent,
                            const gfx::Rect& bounds,
                            View* contents_view,
                            bool do_capture) {
  TRACE_EVENT0("views", "MenuHost::InitMenuHost");
  Widget::InitParams params(Widget::InitParams::TYPE_MENU);
  const MenuController* menu_controller =
      submenu_->GetMenuItem()->GetMenuController();
  const MenuConfig& menu_config = MenuConfig::instance();
  bool rounded_border = menu_config.CornerRadiusForMenu(menu_controller) != 0;
  bool bubble_border = submenu_->GetScrollViewContainer() &&
                       submenu_->GetScrollViewContainer()->HasBubbleBorder();
  params.shadow_type = bubble_border ? Widget::InitParams::ShadowType::kNone
                                     : Widget::InitParams::ShadowType::kDrop;
  params.opacity = (bubble_border || rounded_border)
                       ? Widget::InitParams::WindowOpacity::kTranslucent
                       : Widget::InitParams::WindowOpacity::kOpaque;
  params.parent = parent ? parent->GetNativeView() : gfx::kNullNativeView;
  params.bounds = bounds;
  // If MenuHost has no parent widget, it needs to be marked
  // Activatable, so that calling Show in ShowMenuHost will
  // get keyboard focus.
  if (parent == nullptr)
    params.activatable = Widget::InitParams::ACTIVATABLE_YES;
#if defined(OS_WIN)
  // On Windows use the software compositor to ensure that we don't block
  // the UI thread blocking issue during command buffer creation. We can
  // revert this change once http://crbug.com/125248 is fixed.
  params.force_software_compositing = true;
#endif
  Init(std::move(params));

#if !defined(OS_APPLE)
  pre_dispatch_handler_ =
      std::make_unique<internal::PreMenuEventDispatchHandler>(
          menu_controller, submenu_, GetNativeView());
#endif

  DCHECK(!owner_);
  owner_ = parent;
  if (owner_)
    owner_->AddObserver(this);

  SetContentsView(contents_view);
  ShowMenuHost(do_capture);
}

bool MenuHost::IsMenuHostVisible() {
  return IsVisible();
}

void MenuHost::ShowMenuHost(bool do_capture) {
  // Doing a capture may make us get capture lost. Ignore it while we're in the
  // process of showing.
  base::AutoReset<bool> reseter(&ignore_capture_lost_, true);
  ShowInactive();
  if (do_capture) {
    MenuController* menu_controller =
        submenu_->GetMenuItem()->GetMenuController();
    if (menu_controller && menu_controller->send_gesture_events_to_owner()) {
      // TransferGesture when owner needs gesture events so that the incoming
      // touch events after MenuHost is created are properly translated into
      // gesture events instead of being dropped.
      internal::TransferGesture(owner_, this);
    } else {
      GetGestureRecognizer()->CancelActiveTouchesExcept(nullptr);
    }
    // If MenuHost has no parent widget, it needs to call Show to get focus,
    // so that it will get keyboard events.
    if (owner_ == nullptr)
      Show();
    native_widget_private()->SetCapture();
  }
}

void MenuHost::HideMenuHost() {
  MenuController* menu_controller =
      submenu_->GetMenuItem()->GetMenuController();
  if (owner_ && menu_controller &&
      menu_controller->send_gesture_events_to_owner()) {
    internal::TransferGesture(this, owner_);
  }
  ignore_capture_lost_ = true;
  ReleaseMenuHostCapture();
  Hide();
  ignore_capture_lost_ = false;
}

void MenuHost::DestroyMenuHost() {
  HideMenuHost();
  destroying_ = true;
  static_cast<MenuHostRootView*>(GetRootView())->ClearSubmenu();
#if !defined(OS_APPLE)
  pre_dispatch_handler_.reset();
#endif
  Close();
}

void MenuHost::SetMenuHostBounds(const gfx::Rect& bounds) {
  SetBounds(bounds);
}

void MenuHost::ReleaseMenuHostCapture() {
  if (native_widget_private()->HasCapture())
    native_widget_private()->ReleaseCapture();
}

////////////////////////////////////////////////////////////////////////////////
// MenuHost, private:

internal::RootView* MenuHost::CreateRootView() {
  return new MenuHostRootView(this, submenu_);
}

void MenuHost::OnMouseCaptureLost() {
  if (destroying_ || ignore_capture_lost_)
    return;
  MenuController* menu_controller =
      submenu_->GetMenuItem()->GetMenuController();
  if (menu_controller && !menu_controller->drag_in_progress())
    menu_controller->Cancel(MenuController::ExitType::kAll);
  Widget::OnMouseCaptureLost();
}

void MenuHost::OnNativeWidgetDestroyed() {
  if (!destroying_) {
    // We weren't explicitly told to destroy ourselves, which means the menu was
    // deleted out from under us (the window we're parented to was closed). Tell
    // the SubmenuView to drop references to us.
    submenu_->MenuHostDestroyed();
  }
  Widget::OnNativeWidgetDestroyed();
}

void MenuHost::OnOwnerClosing() {
  if (destroying_)
    return;

  MenuController* menu_controller =
      submenu_->GetMenuItem()->GetMenuController();
  if (menu_controller && !menu_controller->drag_in_progress())
    menu_controller->Cancel(MenuController::ExitType::kAll);
}

void MenuHost::OnDragWillStart() {
  MenuController* menu_controller =
      submenu_->GetMenuItem()->GetMenuController();
  DCHECK(menu_controller);
  menu_controller->OnDragWillStart();
}

void MenuHost::OnDragComplete() {
  // If we are being destroyed there is no guarantee that the menu items are
  // available.
  if (destroying_)
    return;
  MenuController* menu_controller =
      submenu_->GetMenuItem()->GetMenuController();
  if (!menu_controller)
    return;

  bool should_close = true;
  // If the view came from outside menu code (i.e., not a MenuItemView), we
  // should consult the MenuDelegate to determine whether or not to close on
  // exit.
  if (!menu_controller->did_initiate_drag()) {
    MenuDelegate* menu_delegate = submenu_->GetMenuItem()->GetDelegate();
    should_close = menu_delegate ? menu_delegate->ShouldCloseOnDragComplete()
                                 : should_close;
  }
  menu_controller->OnDragComplete(should_close);

  // We may have lost capture in the drag and drop, but are remaining open.
  // Return capture so we get MouseCaptureLost events.
  if (!should_close)
    native_widget_private()->SetCapture();
}

void MenuHost::OnWidgetDestroying(Widget* widget) {
  DCHECK_EQ(owner_, widget);
  owner_->RemoveObserver(this);
  owner_ = nullptr;
}

}  // namespace views

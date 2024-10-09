// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_host.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/compositor.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_host_root_view.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/round_rect_painter.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/base/ui_base_features.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace views {

namespace internal {

#if defined(USE_AURA)
// This class adds itself as the pre target handler for the |window|
// passed in. It currently handles touch events and forwards them to the
// controller. Reason for this approach is views does not get raw touch
// events which we need to determine if a touch happened outside the bounds
// of the menu.
class PreMenuEventDispatchHandler : public ui::EventHandler,
                                    aura::WindowObserver {
 public:
  PreMenuEventDispatchHandler(MenuController* controller,
                              SubmenuView* submenu,
                              aura::Window* window)
      : menu_controller_(controller->AsWeakPtr()), submenu_(submenu) {
    window_observation_.Observe(window);
    window->AddPreTargetHandler(this);
  }

  PreMenuEventDispatchHandler(const PreMenuEventDispatchHandler&) = delete;
  PreMenuEventDispatchHandler& operator=(const PreMenuEventDispatchHandler&) =
      delete;

  ~PreMenuEventDispatchHandler() override { StopObserving(); }

  // ui::EventHandler overrides.
  void OnTouchEvent(ui::TouchEvent* event) override {
    menu_controller_->OnTouchEvent(submenu_, event);
  }

  // aura::WindowObserver overrides.
  void OnWindowDestroying(aura::Window* window) override {
    DCHECK(window_observation_.IsObserving());
    DCHECK_EQ(window_observation_.GetSource(), window);
    StopObserving();
  }

 private:
  void StopObserving() {
    if (!window_observation_.IsObserving()) {
      return;
    }
    window_observation_.GetSource()->RemovePreTargetHandler(this);
    window_observation_.Reset();
  }

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
  // Non-null unless the menu is closing.
  const base::WeakPtr<MenuController> menu_controller_;
  const raw_ptr<SubmenuView> submenu_;
};
#endif  // USE_AURA

void TransferGesture(ui::GestureRecognizer* gesture_recognizer,
                     gfx::NativeView source,
                     gfx::NativeView target) {
#if defined(USE_AURA)
  // Use kCancel for the transfer touches behavior to ensure that `source` sees
  // a valid touch stream. If kCancel is not used source's touch state may not
  // be valid after the menu is closed, potentially causing it to drop touch
  // events it encounters immediately after the menu is closed.
  gesture_recognizer->TransferEventsTo(source, target,
                                       ui::TransferTouchesBehavior::kCancel);
#else
  NOTIMPLEMENTED();
#endif  // defined(USE_AURA)
}

}  // namespace internal

////////////////////////////////////////////////////////////////////////////////
// MenuHost, public:

MenuHost::MenuHost(SubmenuView* submenu) : submenu_(submenu) {
  set_auto_release_capture(false);
}

MenuHost::~MenuHost() = default;

void MenuHost::InitMenuHost(const InitParams& init_params) {
  TRACE_EVENT0("views", "MenuHost::InitMenuHost");
  Widget::InitParams params(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                            Widget::InitParams::TYPE_MENU);
  MenuController* menu_controller =
      submenu_->GetMenuItem()->GetMenuController();
  bool bubble_border = submenu_->GetScrollViewContainer() &&
                       submenu_->GetScrollViewContainer()->HasBubbleBorder();
  params.name = "MenuHost";
  params.shadow_type = bubble_border ? Widget::InitParams::ShadowType::kNone
                                     : Widget::InitParams::ShadowType::kDrop;

  const int corner_radius =
      MenuConfig::instance().CornerRadiusForMenu(menu_controller);
  params.opacity = (bubble_border || corner_radius)
                       ? Widget::InitParams::WindowOpacity::kTranslucent
                       : Widget::InitParams::WindowOpacity::kOpaque;
  params.corner_radius = corner_radius;
  params.parent = init_params.parent ? init_params.parent->GetNativeView()
                                     : gfx::NativeView();
  params.context = init_params.context ? init_params.context->GetNativeWindow()
                                       : gfx::NativeWindow();
  params.bounds = init_params.bounds;

#if defined(USE_AURA)
  params.init_properties_container.SetProperty(aura::client::kOwnedWindowAnchor,
                                               init_params.owned_window_anchor);
#endif
  // If MenuHost has no parent widget, it needs to be marked
  // Activatable, so that calling Show in ShowMenuHost will
  // get keyboard focus.
  if (init_params.parent == nullptr)
    params.activatable = Widget::InitParams::Activatable::kYes;

#if BUILDFLAG(IS_WIN)
  // On Windows use the software compositor to ensure that we don't block
  // the UI thread blocking issue during command buffer creation. We can
  // revert this change once http://crbug.com/125248 is fixed.
  params.force_software_compositing = true;
#endif
  Init(std::move(params));
  std::optional<std::string> show_menu_host_duration_histogram =
      menu_controller->TakeShowMenuHostDurationHistogram();
  CHECK(!menu_controller->TakeShowMenuHostDurationHistogram().has_value());
  if (show_menu_host_duration_histogram.has_value()) {
    // Register callback to emit histogram to measure the time from when the
    // menu host is initialized to successful presentation of the next frame
    // handled by the compositor of menu host's widget.
    GetCompositor()->RequestSuccessfulPresentationTimeForNextFrame(
        base::BindOnce(
            [](std::string histogram, base::TimeTicks menu_host_init_time,
               const viz::FrameTimingDetails& frame_timing_details) {
              base::TimeTicks presentation_time =
                  frame_timing_details.presentation_feedback.timestamp;
              UMA_HISTOGRAM_TIMES(histogram,
                                  presentation_time - menu_host_init_time);
            },
            show_menu_host_duration_histogram.value(), base::TimeTicks::Now()));
  }

#if defined(USE_AURA)
  pre_dispatch_handler_ =
      std::make_unique<internal::PreMenuEventDispatchHandler>(
          menu_controller, submenu_, GetNativeView());
#endif

  DCHECK(!GetOwner());
  if (init_params.parent) {
    owner_observation_.Observe(init_params.parent);
  }
  native_view_for_gestures_ = init_params.native_view_for_gestures;
  SetContentsView(init_params.contents_view);
  ShowMenuHost(init_params.do_capture);
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
      gfx::NativeView source_view = native_view_for_gestures_
                                        ? native_view_for_gestures_
                                        : GetOwner()->GetNativeView();
      internal::TransferGesture(GetGestureRecognizer(), source_view,
                                GetNativeView());
    } else {
      GetGestureRecognizer()->CancelActiveTouchesExcept(nullptr);
    }

    // If MenuHost has no parent widget, it needs to call Show to get focus,
    // so that it will get keyboard events.
    if (GetOwner() == nullptr) {
      Show();
    }
    native_widget_private()->SetCapture();
  }
}

void MenuHost::HideMenuHost() {
  MenuController* menu_controller =
      submenu_->GetMenuItem()->GetMenuController();
  if (GetOwner() && menu_controller &&
      menu_controller->send_gesture_events_to_owner()) {
    gfx::NativeView target_view = native_view_for_gestures_
                                      ? native_view_for_gestures_
                                      : GetOwner()->GetNativeView();
    internal::TransferGesture(GetGestureRecognizer(), GetNativeView(),
                              target_view);
  }
  ignore_capture_lost_ = true;
  ReleaseMenuHostCapture();
  Hide();
  ignore_capture_lost_ = false;
}

void MenuHost::DestroyMenuHost() {
  HideMenuHost();
  destroying_ = true;
  submenu_ = nullptr;
#if defined(USE_AURA)
  pre_dispatch_handler_.reset();
#endif
  static_cast<MenuHostRootView*>(GetRootView())->ClearSubmenu();
  Close();
}

void MenuHost::SetMenuHostBounds(const gfx::Rect& bounds) {
  SetBounds(bounds);
}

void MenuHost::SetMenuHostOwnedWindowAnchor(
    const ui::OwnedWindowAnchor& anchor) {
#if defined(USE_AURA)
  native_widget_private()->GetNativeWindow()->SetProperty(
      aura::client::kOwnedWindowAnchor, anchor);
#endif
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

  if (!ViewsDelegate::GetInstance()->ShouldCloseMenuIfMouseCaptureLost())
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

Widget* MenuHost::GetPrimaryWindowWidget() {
  return GetOwner() ? GetOwner()->GetPrimaryWindowWidget()
                    : Widget::GetPrimaryWindowWidget();
}

gfx::Insets MenuHost::GetCustomInsetsInDIP() const {
#if BUILDFLAG(IS_OZONE)
  if (submenu_) {
    return submenu_->GetScrollViewContainer()->outside_border_insets();
  }
#endif  // BUILDFLAG(IS_OZONE)
  return gfx::Insets();
}

void MenuHost::OnWidgetDestroying(Widget* widget) {
  DCHECK_EQ(GetOwner(), widget);
  owner_observation_.Reset();
  native_view_for_gestures_ = nullptr;
}

Widget* MenuHost::GetOwner() {
  return owner_observation_.GetSource();
}

}  // namespace views

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_lacros.h"

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_target.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/extensions/desk_extension.h"
#include "ui/platform_window/extensions/pinned_mode_extension.h"
#include "ui/platform_window/extensions/system_modal_extension.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/platform_window/wm/wm_move_resize_handler.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/window_event_filter_lacros.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace {

chromeos::WindowStateType ToChromeosWindowStateType(
    ui::PlatformWindowState state) {
  switch (state) {
    case ui::PlatformWindowState::kUnknown:
      return chromeos::WindowStateType::kDefault;
    case ui::PlatformWindowState::kMaximized:
      return chromeos::WindowStateType::kMaximized;
    case ui::PlatformWindowState::kMinimized:
      return chromeos::WindowStateType::kMinimized;
    case ui::PlatformWindowState::kNormal:
      return chromeos::WindowStateType::kNormal;
    case ui::PlatformWindowState::kFullScreen:
      return chromeos::WindowStateType::kFullscreen;
    case ui::PlatformWindowState::kSnappedPrimary:
      return chromeos::WindowStateType::kPrimarySnapped;
    case ui::PlatformWindowState::kSnappedSecondary:
      return chromeos::WindowStateType::kSecondarySnapped;
    case ui::PlatformWindowState::kFloated:
      return chromeos::WindowStateType::kFloated;
    case ui::PlatformWindowState::kPip:
      return chromeos::WindowStateType::kPip;
    case ui::PlatformWindowState::kPinnedFullscreen:
      return chromeos::WindowStateType::kPinned;
    case ui::PlatformWindowState::kTrustedPinnedFullscreen:
      return chromeos::WindowStateType::kTrustedPinned;
  }
}

// Chrome do not expect the pointer (mouse/touch) events are dispatched to
// chrome during move loop. The mouse events are already consumed by
// ozone-wayland but touch events are sent to the `aura::WindowEventDispatcher`
// to update the touch location. Consume touch events at system handler level so
// that chrome will not see the touch events.
class ScopedTouchEventDisabler : public ui::EventHandler {
 public:
  ScopedTouchEventDisabler() {
    aura::Env::GetInstance()->AddPreTargetHandler(
        this, ui::EventTarget::Priority::kSystem);
  }
  ScopedTouchEventDisabler(const ScopedTouchEventDisabler&) = delete;
  ScopedTouchEventDisabler& operator=(const ScopedTouchEventDisabler&) = delete;
  ~ScopedTouchEventDisabler() override {
    aura::Env::GetInstance()->RemovePreTargetHandler(this);
  }

  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override { event->SetHandled(); }
};

bool IsImmersive(ui::PlatformFullscreenType type) {
  return type == ui::PlatformFullscreenType::kImmersive;
}

gfx::RoundedCornersF GetWindowCornerRadii(
    aura::Window* window,
    ui::WaylandToplevelExtension* wayland_extension) {
  if (!wayland_extension) {
    return gfx::RoundedCornersF();
  }

  // If window is a pip, ignore the window radii specified by the server. Window
  // radii specified by the server is for a window in normal window state.
  const auto window_state = window->GetProperty(chromeos::kWindowStateTypeKey);
  if (window_state == chromeos::WindowStateType::kPip) {
    return gfx::RoundedCornersF(chromeos::kPipRoundedCornerRadius);
  }

  return wayland_extension->GetWindowCornersRadii();
}

}  // namespace
namespace views {

DesktopWindowTreeHostLacros::DesktopWindowTreeHostLacros(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura)
    : DesktopWindowTreeHostPlatform(native_widget_delegate,
                                    desktop_native_widget_aura) {
  CHECK(GetContentWindow());
  content_window_observation_.Observe(GetContentWindow());
}

DesktopWindowTreeHostLacros::~DesktopWindowTreeHostLacros() = default;

ui::WaylandToplevelExtension*
DesktopWindowTreeHostLacros::GetWaylandToplevelExtension() {
  return platform_window()
             ? ui::GetWaylandToplevelExtension(*(platform_window()))
             : nullptr;
}

const ui::WaylandToplevelExtension*
DesktopWindowTreeHostLacros::GetWaylandToplevelExtension() const {
  return platform_window()
             ? ui::GetWaylandToplevelExtension(*(platform_window()))
             : nullptr;
}

void DesktopWindowTreeHostLacros::OnNativeWidgetCreated(
    const Widget::InitParams& params) {
  CreateNonClientEventFilter();
  DesktopWindowTreeHostPlatform::OnNativeWidgetCreated(params);
  platform_window()->SetUseNativeFrame(false);
}

void DesktopWindowTreeHostLacros::InitModalType(
    ui::mojom::ModalType modal_type) {
  if (ui::GetSystemModalExtension(*(platform_window()))) {
    ui::GetSystemModalExtension(*(platform_window()))
        ->SetSystemModal(modal_type == ui::mojom::ModalType::kSystem);
  }

  switch (modal_type) {
    case ui::mojom::ModalType::kNone:
    case ui::mojom::ModalType::kSystem:
      break;
    default:
      // TODO(erg): Figure out under what situations |modal_type| isn't
      // none. The comment in desktop_native_widget_aura.cc suggests that this
      // is rare.
      NOTIMPLEMENTED();
  }
}

void DesktopWindowTreeHostLacros::OnClosed() {
  DestroyNonClientEventFilter();
  DesktopWindowTreeHostPlatform::OnClosed();
}

void DesktopWindowTreeHostLacros::OnWindowStateChanged(
    ui::PlatformWindowState old_state,
    ui::PlatformWindowState new_state) {
  DesktopWindowTreeHostPlatform::OnWindowStateChanged(old_state, new_state);
  GetContentWindow()->SetProperty(chromeos::kWindowStateTypeKey,
                                  ToChromeosWindowStateType(new_state));

  UpdateWindowHints();
}

void DesktopWindowTreeHostLacros::OnFullscreenTypeChanged(
    ui::PlatformFullscreenType old_type,
    ui::PlatformFullscreenType new_type) {
  // Keep in sync with ImmersiveFullscreenController::Enable for widget. See
  // comment there for details.
  if (IsImmersive(old_type) != IsImmersive(new_type)) {
    GetContentWindow()->SetProperty(chromeos::kImmersiveIsActive,
                                    IsImmersive(new_type));
  }
}

void DesktopWindowTreeHostLacros::OnOverviewModeChanged(bool in_overview) {
  GetContentWindow()->SetProperty(chromeos::kIsShowingInOverviewKey,
                                  in_overview);

  // Window corner radius depends on whether the window is in overview mode or
  // not. Once the overview property has been updated, the browser window
  // corners need to be updated.
  // See `chromeos::GetFrameCornerRadius()` for more details.
  UpdateWindowHints();
}

void DesktopWindowTreeHostLacros::OnTooltipShownOnServer(
    const std::u16string& text,
    const gfx::Rect& bounds) {
  if (tooltip_controller()) {
    tooltip_controller()->OnTooltipShownOnServer(GetContentWindow(), text,
                                                 bounds);
  }
}

void DesktopWindowTreeHostLacros::OnTooltipHiddenOnServer() {
  if (tooltip_controller()) {
    tooltip_controller()->OnTooltipHiddenOnServer();
  }
}

void DesktopWindowTreeHostLacros::OnBoundsChanged(const BoundsChange& change) {
  // DesktopWindowTreeHostPlatform::OnBoundsChanged() may result in |this| being
  // deleted. As an extra safety guard, keep track of `this` with a weak
  // pointer, and only call UpdateWindowHints() if it still exists.
  auto weak_this = weak_factory_.GetWeakPtr();
  DesktopWindowTreeHostPlatform::OnBoundsChanged(change);

  if (weak_this.get()) {
    UpdateWindowHints();
  }
}

void DesktopWindowTreeHostLacros::AddAdditionalInitProperties(
    const Widget::InitParams& params,
    ui::PlatformWindowInitProperties* properties) {
  properties->icon = ViewsDelegate::GetInstance()->GetDefaultWindowIcon();
  properties->wayland_app_id = params.wayland_app_id;
}

Widget::MoveLoopResult DesktopWindowTreeHostLacros::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  ScopedTouchEventDisabler touch_event_disabler;
  return DesktopWindowTreeHostPlatform::RunMoveLoop(drag_offset, source,
                                                    escape_behavior);
}

void DesktopWindowTreeHostLacros::OnWindowPropertyChanged(aura::Window* window,
                                                          const void* key,
                                                          intptr_t old) {
  CHECK_EQ(GetContentWindow(), window);
  if (key == aura::client::kTopViewInset) {
    if (auto* wayland_extension = GetWaylandToplevelExtension()) {
      wayland_extension->SetTopInset(
          GetContentWindow()->GetProperty(aura::client::kTopViewInset));
    }
  }
}

void DesktopWindowTreeHostLacros::OnWindowDestroying(aura::Window* window) {
  CHECK_EQ(GetContentWindow(), window);
  content_window_observation_.Reset();
}

void DesktopWindowTreeHostLacros::OnWidgetInitDone() {
  DesktopWindowTreeHostPlatform::OnWidgetInitDone();

  UpdateWindowHints();
}

void DesktopWindowTreeHostLacros::CreateNonClientEventFilter() {
  DCHECK(!non_client_window_event_filter_);
  non_client_window_event_filter_ = std::make_unique<WindowEventFilterLacros>(
      this, GetWmMoveResizeHandler(*platform_window()));
}

void DesktopWindowTreeHostLacros::DestroyNonClientEventFilter() {
  non_client_window_event_filter_.reset();
}

void DesktopWindowTreeHostLacros::UpdateWindowHints() {
  const float scale = device_scale_factor();
  const gfx::Size widget_size_px =
      platform_window()->GetBoundsInPixels().size();

  // Content window can have a window_targeter that allows located events fall
  // to the window underneath it. There is no window underneath the content
  // window from aura's point of view so wayland platform needs to know about
  // it.
  gfx::Rect hit_test_rect_mouse_dp{platform_window()->GetBoundsInDIP().size()};
  if (GetContentWindow()->targeter()) {
    gfx::Rect hit_test_rect_touch_dp = gfx::Rect{hit_test_rect_mouse_dp};
    GetContentWindow()->targeter()->GetHitTestRects(
        GetContentWindow(), &hit_test_rect_mouse_dp, &hit_test_rect_touch_dp);
  }
  const gfx::Rect hit_test_rect_px =
      ConvertRectToPixels(hit_test_rect_mouse_dp);

  aura::Window* native_window = GetWidget()->GetNativeWindow();

  auto* wayland_extension = ui::GetWaylandToplevelExtension(*platform_window());
  const gfx::RoundedCornersF window_radii =
      GetWindowCornerRadii(native_window, wayland_extension);

  const bool should_have_rounded_window =
      views::ViewsDelegate::GetInstance()->ShouldWindowHaveRoundedCorners(
          native_window);

  std::vector<gfx::Rect> input_region;

  if (should_have_rounded_window) {
    GetContentWindow()->layer()->SetRoundedCornerRadius(window_radii);
    GetContentWindow()->layer()->SetIsFastRoundedCorner(true);

    auto to_rect = [](float origin_x, float origin_y, float size) {
      gfx::RectF rect(origin_x, origin_y, size, size);
      return gfx::ToEnclosingRectIgnoringError(rect);
    };

    cc::Region region(hit_test_rect_px);
    const int width = widget_size_px.width(), height = widget_size_px.height();

    const float upper_left_px = window_radii.upper_left() * scale;
    region.Subtract(to_rect(0, 0, upper_left_px));

    const float upper_right_px = window_radii.upper_right() * scale;
    region.Subtract(to_rect(width - upper_right_px * scale, 0, upper_right_px));

    const float lower_left_px = window_radii.lower_left() * scale;
    region.Subtract(to_rect(0, height - lower_left_px, lower_left_px));

    const float lower_right_px = window_radii.lower_right() * scale;
    region.Subtract(to_rect(width - lower_right_px, height - lower_right_px,
                            lower_right_px));

    // Convert the region to a list of rectangles.
    for (gfx::Rect i : region) {
      input_region.push_back(i);
    }
  } else {
    GetContentWindow()->layer()->SetRoundedCornerRadius({});
    GetContentWindow()->layer()->SetIsFastRoundedCorner(false);
    input_region.push_back(hit_test_rect_px);
  }
  // TODO(crbug.com/40218466): Instead of setting in pixels, set in dp.
  platform_window()->SetInputRegion(input_region);

  // If the window is rounded, we hint the platform to match the drop shadow's
  // radii to the window's radii. Otherwise, we allow the platform to
  // determine the drop shadow's radii.
  if (should_have_rounded_window && wayland_extension) {
    wayland_extension->SetShadowCornersRadii(window_radii);
  }
}

// static
DesktopWindowTreeHostLacros* DesktopWindowTreeHostLacros::From(
    WindowTreeHost* wth) {
  DCHECK(has_open_windows()) << "Calling this method from non-Platform based "
                                "platform.";

  for (auto widget : open_windows()) {
    DesktopWindowTreeHostPlatform* wth_platform =
        DesktopWindowTreeHostPlatform::GetHostForWidget(widget);
    if (wth_platform != wth)
      continue;

    return static_cast<views::DesktopWindowTreeHostLacros*>(wth_platform);
  }
  return nullptr;
}

ui::DeskExtension* DesktopWindowTreeHostLacros::GetDeskExtension() {
  return ui::GetDeskExtension(*(platform_window()));
}
const ui::DeskExtension* DesktopWindowTreeHostLacros::GetDeskExtension() const {
  return ui::GetDeskExtension(*(platform_window()));
}

ui::PinnedModeExtension* DesktopWindowTreeHostLacros::GetPinnedModeExtension() {
  return ui::GetPinnedModeExtension(*(platform_window()));
}

const ui::PinnedModeExtension*
DesktopWindowTreeHostLacros::GetPinnedModeExtension() const {
  return ui::GetPinnedModeExtension(*(platform_window()));
}

// static
DesktopWindowTreeHost* DesktopWindowTreeHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura) {
  return new DesktopWindowTreeHostLacros(native_widget_delegate,
                                         desktop_native_widget_aura);
}

}  // namespace views

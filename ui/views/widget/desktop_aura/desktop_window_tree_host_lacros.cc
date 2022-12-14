// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_lacros.h"

#include <memory>
#include <string>

#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/events/event.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/extensions/desk_extension.h"
#include "ui/platform_window/extensions/pinned_mode_extension.h"
#include "ui/platform_window/extensions/system_modal_extension.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/platform_window/wm/wm_move_resize_handler.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/corewm/tooltip_lacros.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/window_event_filter_lacros.h"
#include "ui/views/widget/widget.h"

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
  }
}

}  // namespace
namespace views {

DesktopWindowTreeHostLacros::DesktopWindowTreeHostLacros(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura)
    : DesktopWindowTreeHostPlatform(native_widget_delegate,
                                    desktop_native_widget_aura) {}

DesktopWindowTreeHostLacros::~DesktopWindowTreeHostLacros() = default;

ui::WaylandExtension* DesktopWindowTreeHostLacros::GetWaylandExtension() {
  return platform_window() ? ui::GetWaylandExtension(*(platform_window()))
                           : nullptr;
}

const ui::WaylandExtension* DesktopWindowTreeHostLacros::GetWaylandExtension()
    const {
  return platform_window() ? ui::GetWaylandExtension(*(platform_window()))
                           : nullptr;
}

void DesktopWindowTreeHostLacros::OnNativeWidgetCreated(
    const Widget::InitParams& params) {
  CreateNonClientEventFilter();
  DesktopWindowTreeHostPlatform::OnNativeWidgetCreated(params);
  platform_window()->SetUseNativeFrame(false);
}

void DesktopWindowTreeHostLacros::InitModalType(ui::ModalType modal_type) {
  if (ui::GetSystemModalExtension(*(platform_window()))) {
    ui::GetSystemModalExtension(*(platform_window()))
        ->SetSystemModal(modal_type == ui::MODAL_TYPE_SYSTEM);
  }

  switch (modal_type) {
    case ui::MODAL_TYPE_NONE:
    case ui::MODAL_TYPE_SYSTEM:
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
    ui::PlatformWindowState old_window_show_state,
    ui::PlatformWindowState new_window_show_state) {
  DesktopWindowTreeHostPlatform::OnWindowStateChanged(old_window_show_state,
                                                      new_window_show_state);
  GetContentWindow()->SetProperty(
      chromeos::kWindowStateTypeKey,
      ToChromeosWindowStateType(new_window_show_state));
}

void DesktopWindowTreeHostLacros::OnImmersiveModeChanged(bool enabled) {
  // Keep in sync with ImmersiveFullscreenController::Enable for widget. See
  // comment there for details.
  GetContentWindow()->SetProperty(chromeos::kImmersiveIsActive, enabled);
}

void DesktopWindowTreeHostLacros::OnTooltipShownOnServer(
    const std::u16string& text,
    const gfx::Rect& bounds) {
  if (tooltip_controller()) {
    tooltip_controller()->OnTooltipShownOnServer(text, bounds);
  }
}

void DesktopWindowTreeHostLacros::OnTooltipHiddenOnServer() {
  if (tooltip_controller()) {
    tooltip_controller()->OnTooltipHiddenOnServer();
  }
}

void DesktopWindowTreeHostLacros::AddAdditionalInitProperties(
    const Widget::InitParams& params,
    ui::PlatformWindowInitProperties* properties) {
  properties->icon = ViewsDelegate::GetInstance()->GetDefaultWindowIcon();
  properties->wayland_app_id = params.wayland_app_id;
}

std::unique_ptr<corewm::Tooltip> DesktopWindowTreeHostLacros::CreateTooltip() {
  // TODO(crbug.com/1338597): Remove TooltipAura from Lacros when Ash is new
  // enough.
  if (ui::OzonePlatform::GetInstance()
          ->GetPlatformRuntimeProperties()
          .supports_tooltip) {
    return std::make_unique<views::corewm::TooltipLacros>();
  }
  // Fallback to TooltipAura if wayland version is not new enough.
  return std::make_unique<views::corewm::TooltipAura>();
}

void DesktopWindowTreeHostLacros::CreateNonClientEventFilter() {
  DCHECK(!non_client_window_event_filter_);
  non_client_window_event_filter_ = std::make_unique<WindowEventFilterLacros>(
      this, GetWmMoveResizeHandler(*platform_window()));
}

void DesktopWindowTreeHostLacros::DestroyNonClientEventFilter() {
  non_client_window_event_filter_.reset();
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

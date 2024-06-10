// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/screen_ozone.h"

#include <memory>
#include <optional>

#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/tablet_state.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_screen.h"

namespace aura {

ScreenOzone::ScreenOzone() {
  DCHECK(!display::Screen::HasScreen());
  display::Screen::SetScreenInstance(this);

  auto* platform = ui::OzonePlatform::GetInstance();
  platform_screen_ = platform->CreateScreen();
  if (platform_screen_) {
    // Separate `CreateScreen` from `InitScreen` so that synchronous observers
    // that call into `Screen` functions below have a valid `platform_screen_`.
    platform->InitScreen(platform_screen_.get());
  } else {
    NOTREACHED_IN_MIGRATION()
        << "PlatformScreen is not implemented for this ozone platform.";
  }
}

ScreenOzone::~ScreenOzone() {
  display::Screen::SetScreenInstance(nullptr);
}

// static
bool ScreenOzone::IsOzoneInitialized() {
  return ui::OzonePlatform::IsInitialized();
}

gfx::Point ScreenOzone::GetCursorScreenPoint() {
  return platform_screen_->GetCursorScreenPoint();
}

bool ScreenOzone::IsWindowUnderCursor(gfx::NativeWindow window) {
  DCHECK(platform_screen_);
  gfx::AcceleratedWidget widget = GetAcceleratedWidgetForWindow(window);
  return platform_screen_->IsAcceleratedWidgetUnderCursor(widget);
}

gfx::NativeWindow ScreenOzone::GetWindowAtScreenPoint(const gfx::Point& point) {
  DCHECK(platform_screen_);
  return GetNativeWindowFromAcceleratedWidget(
      platform_screen_->GetAcceleratedWidgetAtScreenPoint(point));
}

gfx::NativeWindow ScreenOzone::GetLocalProcessWindowAtPoint(
    const gfx::Point& point,
    const std::set<gfx::NativeWindow>& ignore) {
  DCHECK(platform_screen_);
  std::set<gfx::AcceleratedWidget> ignore_top_level;
  for (auto* const window : ignore)
    ignore_top_level.emplace(window->GetHost()->GetAcceleratedWidget());

  return GetNativeWindowFromAcceleratedWidget(
      platform_screen_->GetLocalProcessWidgetAtPoint(point, ignore_top_level));
}

int ScreenOzone::GetNumDisplays() const {
  return GetAllDisplays().size();
}

const std::vector<display::Display>& ScreenOzone::GetAllDisplays() const {
  return platform_screen_->GetAllDisplays();
}

display::Display ScreenOzone::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  DCHECK(platform_screen_);
  gfx::AcceleratedWidget widget = GetAcceleratedWidgetForWindow(window);
  if (!widget)
    return GetPrimaryDisplay();

  return platform_screen_->GetDisplayForAcceleratedWidget(widget);
}

display::Display ScreenOzone::GetDisplayNearestView(
    gfx::NativeView view) const {
  return GetDisplayNearestWindow(view);
}

display::Display ScreenOzone::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return platform_screen_->GetDisplayNearestPoint(point);
}

display::Display ScreenOzone::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  return platform_screen_->GetDisplayMatching(match_rect);
}

display::Display ScreenOzone::GetPrimaryDisplay() const {
  return platform_screen_->GetPrimaryDisplay();
}

#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
ScreenOzone::ScreenSaverSuspenderOzone::ScreenSaverSuspenderOzone(
    std::unique_ptr<ui::PlatformScreen::PlatformScreenSaverSuspender> suspender)
    : suspender_(std::move(suspender)) {}

ScreenOzone::ScreenSaverSuspenderOzone::~ScreenSaverSuspenderOzone() = default;

std::unique_ptr<display::Screen::ScreenSaverSuspender>
ScreenOzone::SuspendScreenSaver() {
  return std::make_unique<ScreenSaverSuspenderOzone>(
      platform_screen_->SuspendScreenSaver());
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)

bool ScreenOzone::IsScreenSaverActive() const {
  return platform_screen_->IsScreenSaverActive();
}

base::TimeDelta ScreenOzone::CalculateIdleTime() const {
  return platform_screen_->CalculateIdleTime();
}

void ScreenOzone::AddObserver(display::DisplayObserver* observer) {
  platform_screen_->AddObserver(observer);
}

void ScreenOzone::RemoveObserver(display::DisplayObserver* observer) {
  platform_screen_->RemoveObserver(observer);
}

std::string ScreenOzone::GetCurrentWorkspace() {
  return platform_screen_->GetCurrentWorkspace();
}

base::Value::List ScreenOzone::GetGpuExtraInfo(
    const gfx::GpuExtraInfo& gpu_extra_info) {
  return platform_screen_->GetGpuExtraInfo(gpu_extra_info);
}

std::optional<float> ScreenOzone::GetPreferredScaleFactorForWindow(
    gfx::NativeWindow window) const {
  return ui::OzonePlatform::GetInstance()
                 ->GetPlatformRuntimeProperties()
                 .supports_per_window_scaling
             ? platform_screen_->GetPreferredScaleFactorForAcceleratedWidget(
                   GetAcceleratedWidgetForWindow(window))
             : Screen::GetPreferredScaleFactorForWindow(window);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
display::TabletState ScreenOzone::GetTabletState() const {
  return platform_screen_->GetTabletState();
}

void ScreenOzone::OverrideTabletStateForTesting(
    display::TabletState tablet_state) {
  platform_screen_->OnTabletStateChanged(tablet_state);
}
#endif

gfx::NativeWindow ScreenOzone::GetNativeWindowFromAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  return nullptr;
}

gfx::AcceleratedWidget ScreenOzone::GetAcceleratedWidgetForWindow(
    aura::Window* window) const {
  if (!window)
    return gfx::kNullAcceleratedWidget;

  aura::WindowTreeHost* host = window->GetHost();
  if (!host)
    return gfx::kNullAcceleratedWidget;

  return host->GetAcceleratedWidget();
}

}  // namespace aura

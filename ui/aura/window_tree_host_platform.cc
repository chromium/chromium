// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tree_host_platform.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/host_frame_rate_throttler.h"
#include "ui/aura/native_window_occlusion_tracker.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/layout.h"
#include "ui/base/view_prop.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/keyboard_hook.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/events/keycodes/dom/dom_keyboard_layout_map.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/platform_window/win/win_window.h"
#endif

namespace aura {

namespace {
WindowTreeHostPlatform::PlatformWindowFactoryDelegateForTesting*
    g_platform_window_factory_delegate_for_testing = nullptr;

const char kWindowTreeHostPlatformForAcceleratedWidget[] =
    "__AURA_WINDOW_TREE_HOST_PLATFORM_ACCELERATED_WIDGET__";
}

// static
std::unique_ptr<WindowTreeHost> WindowTreeHost::Create(
    ui::PlatformWindowInitProperties properties) {
  return std::make_unique<WindowTreeHostPlatform>(
      std::move(properties),
      std::make_unique<aura::Window>(nullptr, client::WINDOW_TYPE_UNKNOWN));
}

WindowTreeHostPlatform::WindowTreeHostPlatform(
    ui::PlatformWindowInitProperties properties,
    std::unique_ptr<Window> window)
    : WindowTreeHost(std::move(window)) {
  size_in_pixels_ = properties.bounds.size();
  CreateCompositor(false, false, properties.enable_compositing_based_throttling,
                   properties.compositor_memory_limit_mb);
  CreateAndSetPlatformWindow(std::move(properties));
}

WindowTreeHostPlatform::WindowTreeHostPlatform(std::unique_ptr<Window> window)
    : WindowTreeHost(std::move(window)),
      widget_(gfx::kNullAcceleratedWidget),
      current_cursor_(ui::mojom::CursorType::kNull) {}

// static
WindowTreeHostPlatform* WindowTreeHostPlatform::GetHostForWindow(
    aura::Window* window) {
  return reinterpret_cast<WindowTreeHostPlatform*>(
      ui::ViewProp::GetValue(window->GetHost()->GetAcceleratedWidget(),
                             kWindowTreeHostPlatformForAcceleratedWidget));
}

void WindowTreeHostPlatform::CreateAndSetPlatformWindow(
    ui::PlatformWindowInitProperties properties) {
  // Cache initial size used to create |platform_window_| so that it does not
  // end up propagating unneeded bounds change event when it is first notified
  // through OnBoundsChanged, which may lead to unneeded re-layouts, etc.
  size_in_pixels_ = properties.bounds.size();
  platform_window_ = CreatePlatformWindow(std::move(properties));
}

void WindowTreeHostPlatform::SetPlatformWindow(
    std::unique_ptr<ui::PlatformWindow> window) {
  platform_window_ = std::move(window);
}

WindowTreeHostPlatform::~WindowTreeHostPlatform() {
  DestroyCompositor();
  DestroyDispatcher();

  // |platform_window_| may not exist yet.
  if (platform_window_)
    platform_window_->Close();
}

ui::EventSource* WindowTreeHostPlatform::GetEventSource() {
  return this;
}

gfx::AcceleratedWidget WindowTreeHostPlatform::GetAcceleratedWidget() {
  return widget_;
}

void WindowTreeHostPlatform::ShowImpl() {
  platform_window_->Show();
}

void WindowTreeHostPlatform::HideImpl() {
  platform_window_->Hide();
}

gfx::Rect WindowTreeHostPlatform::GetBoundsInPixels() const {
  return platform_window_->GetBoundsInPixels();
}

void WindowTreeHostPlatform::SetBoundsInPixels(const gfx::Rect& bounds) {
  platform_window_->SetBoundsInPixels(bounds);
}

void WindowTreeHostPlatform::SetCapture() {
#if BUILDFLAG(IS_OZONE)
  if (ui::IsNativeUiEventDispatchDisabled()) {
    return;
  }
#endif
  platform_window_->SetCapture();
}

void WindowTreeHostPlatform::ReleaseCapture() {
  platform_window_->ReleaseCapture();
}

gfx::Point WindowTreeHostPlatform::GetLocationOnScreenInPixels() const {
  return platform_window_->GetBoundsInPixels().origin();
}

bool WindowTreeHostPlatform::CaptureSystemKeyEventsImpl(
    std::optional<base::flat_set<ui::DomCode>> dom_codes) {
  // Only one KeyboardHook should be active at a time, otherwise there will be
  // problems with event routing (i.e. which Hook takes precedence) and
  // destruction ordering.
  DCHECK(!keyboard_hook_);
  keyboard_hook_ = ui::KeyboardHook::CreateModifierKeyboardHook(
      std::move(dom_codes), GetAcceleratedWidget(),
      base::BindRepeating(
          [](ui::PlatformWindowDelegate* delegate, ui::KeyEvent* event) {
            delegate->DispatchEvent(event);
          },
          base::Unretained(this)));

  return keyboard_hook_ != nullptr;
}

void WindowTreeHostPlatform::ReleaseSystemKeyEventCapture() {
  keyboard_hook_.reset();
}

bool WindowTreeHostPlatform::IsKeyLocked(ui::DomCode dom_code) {
  return keyboard_hook_ && keyboard_hook_->IsKeyLocked(dom_code);
}

base::flat_map<std::string, std::string>
WindowTreeHostPlatform::GetKeyboardLayoutMap() {
#if BUILDFLAG(IS_OZONE)
  return ui::GenerateDomKeyboardLayoutMap();
#else
  NOTIMPLEMENTED();
  return {};
#endif
}

void WindowTreeHostPlatform::OnVideoCaptureLockCreated() {
  if (platform_window_) {
    platform_window_->SetVideoCapture();
  }
}

void WindowTreeHostPlatform::OnVideoCaptureLockDestroyed() {
  if (platform_window_) {
    platform_window_->ReleaseVideoCapture();
  }
}

void WindowTreeHostPlatform::SetCursorNative(gfx::NativeCursor cursor) {
  if (cursor == current_cursor_)
    return;
  current_cursor_ = cursor;

  platform_window_->SetCursor(cursor.platform());
}

void WindowTreeHostPlatform::MoveCursorToScreenLocationInPixels(
    const gfx::Point& location_in_pixels) {
#if BUILDFLAG(IS_OZONE)
  if (ui::IsNativeUiEventDispatchDisabled()) {
    // Unit tests should not test or rely on the native cursor position because
    // it is shared between multiple tests.
    return;
  }
#endif
  platform_window_->MoveCursorTo(location_in_pixels);
}

void WindowTreeHostPlatform::OnCursorVisibilityChangedNative(bool show) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowTreeHostPlatform::LockMouse(Window* window) {
  window->SetCapture();
  WindowTreeHost::LockMouse(window);
}

std::unique_ptr<ui::PlatformWindow>
WindowTreeHostPlatform::CreatePlatformWindow(
    ui::PlatformWindowInitProperties properties) {
  if (g_platform_window_factory_delegate_for_testing) {
    return g_platform_window_factory_delegate_for_testing->Create(this);
  }
#if BUILDFLAG(IS_OZONE)
  return ui::OzonePlatform::GetInstance()->CreatePlatformWindow(
      this, std::move(properties));
#elif BUILDFLAG(IS_WIN)
  return std::make_unique<ui::WinWindow>(this, properties.bounds);
#else
  NOTIMPLEMENTED();
  return nullptr;
#endif
}

// static
void WindowTreeHostPlatform::SetPlatformWindowFactoryDelegateForTesting(
    PlatformWindowFactoryDelegateForTesting* delegate) {
  CHECK_IS_TEST();
  g_platform_window_factory_delegate_for_testing = delegate;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
std::string WindowTreeHostPlatform::GetUniqueId() const {
  return platform_window()->GetWindowUniqueId();
}
#endif

void WindowTreeHostPlatform::OnBoundsChanged(const BoundsChange& change) {
  // It's possible this function may be called recursively. Only notify
  // observers on initial entry. This way observers can safely assume that
  // OnHostDidProcessBoundsChange() is called when all bounds changes have
  // completed.
  if (++on_bounds_changed_recursion_depth_ == 1) {
    observers().Notify(&WindowTreeHostObserver::OnHostWillProcessBoundsChange,
                       this);
  }

  const auto preferred_scale =
      display::Screen::GetScreen()->GetPreferredScaleFactorForWindow(window());
  float current_scale = compositor()->device_scale_factor();
  float new_scale = preferred_scale.value_or(1.0f);
  auto weak_ref = GetWeakPtr();
  auto new_size = GetBoundsInPixels().size();
  bool size_changed = size_in_pixels_ != new_size;
  size_in_pixels_ = new_size;
  if (change.origin_changed) {
    OnHostMovedInPixels();
    // Changing the bounds may destroy this.
    if (!weak_ref)
      return;
  }
  if (size_changed || current_scale != new_scale) {
    OnHostResizedInPixels(new_size);
    // Changing the size may destroy this.
    if (!weak_ref)
      return;
  }
  DCHECK_GT(on_bounds_changed_recursion_depth_, 0);
  if (--on_bounds_changed_recursion_depth_ == 0) {
    observers().Notify(&WindowTreeHostObserver::OnHostDidProcessBoundsChange,
                       this);
  }
}

void WindowTreeHostPlatform::OnDamageRect(const gfx::Rect& damage_rect) {
  compositor()->ScheduleRedrawRect(damage_rect);
}

void WindowTreeHostPlatform::DispatchEvent(ui::Event* event) {
  TRACE_EVENT0("input", "WindowTreeHostPlatform::DispatchEvent");
  ui::EventDispatchDetails details = SendEventToSink(event);
  if (details.dispatcher_destroyed)
    event->SetHandled();
}

void WindowTreeHostPlatform::OnCloseRequest() {
  OnHostCloseRequested();
}

void WindowTreeHostPlatform::OnClosed() {}

void WindowTreeHostPlatform::OnWindowStateChanged(
    ui::PlatformWindowState old_state,
    ui::PlatformWindowState new_state) {}

void WindowTreeHostPlatform::OnLostCapture() {
  OnHostLostWindowCapture();
}

void WindowTreeHostPlatform::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
  prop_ = std::make_unique<ui::ViewProp>(
      widget, kWindowTreeHostPlatformForAcceleratedWidget, this);
  widget_ = widget;
  // This may be called before the Compositor has been created.
  if (compositor())
    WindowTreeHost::OnAcceleratedWidgetAvailable();
}

void WindowTreeHostPlatform::OnWillDestroyAcceleratedWidget() {}

void WindowTreeHostPlatform::OnAcceleratedWidgetDestroyed() {
  gfx::AcceleratedWidget widget = compositor()->ReleaseAcceleratedWidget();
  DCHECK_EQ(widget, widget_);
  widget_ = gfx::kNullAcceleratedWidget;
}

void WindowTreeHostPlatform::OnActivationChanged(bool active) {}

void WindowTreeHostPlatform::OnMouseEnter() {
  client::CursorClient* cursor_client = client::GetCursorClient(window());
  if (cursor_client) {
    auto display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(window());
    DCHECK(display.is_valid());
    cursor_client->SetDisplay(display);
  }
}

void WindowTreeHostPlatform::OnOcclusionStateChanged(
    ui::PlatformWindowOcclusionState occlusion_state) {
  auto aura_occlusion_state = Window::OcclusionState::UNKNOWN;
  switch (occlusion_state) {
    case ui::PlatformWindowOcclusionState::kUnknown:
      aura_occlusion_state = Window::OcclusionState::UNKNOWN;
      break;
    case ui::PlatformWindowOcclusionState::kVisible:
      aura_occlusion_state = Window::OcclusionState::VISIBLE;
      break;
    case ui::PlatformWindowOcclusionState::kOccluded:
      aura_occlusion_state = Window::OcclusionState::OCCLUDED;
      break;
    case ui::PlatformWindowOcclusionState::kHidden:
      aura_occlusion_state = Window::OcclusionState::HIDDEN;
      break;
  }
  SetNativeWindowOcclusionState(aura_occlusion_state, {});
}

int64_t WindowTreeHostPlatform::OnStateUpdate(
    const PlatformWindowDelegate::State& old,
    const PlatformWindowDelegate::State& latest) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Notify the fullscreen type change before the window state change to reflect
  // the immersive status at OnWindowStateChanged.
  if (old.fullscreen_type != latest.fullscreen_type) {
    OnFullscreenTypeChanged(old.fullscreen_type, latest.fullscreen_type);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  if (old.window_state != latest.window_state) {
    OnWindowStateChanged(old.window_state, latest.window_state);
  }

  if (old.bounds_dip != latest.bounds_dip || old.size_px != latest.size_px ||
      old.window_scale != latest.window_scale) {
    bool origin_changed = old.bounds_dip.origin() != latest.bounds_dip.origin();
    OnBoundsChanged({origin_changed});
  }

  if (old.raster_scale != latest.raster_scale) {
    compositor()->SetExternalPageScaleFactor(latest.raster_scale);
  }

  bool needs_frame = latest.WillProduceFrameOnUpdateFrom(old);
  if (old.occlusion_state != latest.occlusion_state &&
      NativeWindowOcclusionTracker::
          IsNativeWindowOcclusionTrackingAlwaysEnabled(this)) {
    const bool visible_before = compositor()->IsVisible();
    OnOcclusionStateChanged(latest.occlusion_state);
    if (!visible_before && compositor()->IsVisible()) {
      // If the compositor has become visible, make sure to wait for a frame.
      needs_frame = true;
    }
  }

  // Only set the sequence ID if this change will produce a frame.
  // If it won't, we may wait indefinitely for a frame that will never come.
  // If the compositor is not visible, we will not get a frame, so don't wait.
  if (!needs_frame || !compositor()->IsVisible()) {
    return -1;
  }

  // Update window()'s LocalSurfaceId. This will ensure that the parent ID is
  // updated both here and for LayerTreeHostImpl. So, the CompositorFrame sent
  // by LayerTreeHostImpl will include the updated parent ID for
  // synchronization. Some operations may have already updated the
  // LocalSurfaceId, but this only modifies pending commit state, so it's not
  // expensive.
  window()->AllocateLocalSurfaceId();
  compositor()->SetLocalSurfaceIdFromParent(window()->GetLocalSurfaceId());

  return window()->GetLocalSurfaceId().parent_sequence_number();
}

void WindowTreeHostPlatform::SetFrameRateThrottleEnabled(bool enabled) {
  if (enabled)
    HostFrameRateThrottler::GetInstance().AddHost(this);
  else
    HostFrameRateThrottler::GetInstance().RemoveHost(this);
}

void WindowTreeHostPlatform::DisableNativeWindowOcclusion() {
  SetNativeWindowOcclusionEnabled(false);
}

}  // namespace aura

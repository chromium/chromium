// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tree_host_platform.h"

#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/layout.h"
#include "ui/compositor/compositor.h"
#include "ui/events/event.h"
#include "ui/events/keyboard_hook.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_keyboard_layout_map.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if defined(OS_ANDROID)
#include "ui/platform_window/android/platform_window_android.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(OS_WIN)
#include "ui/base/cursor/cursor_loader_win.h"
#include "ui/platform_window/win/win_window.h"
#endif

#if defined(USE_X11)
#include "ui/platform_window/x11/x11_window.h"
#endif

namespace aura {

// static
std::unique_ptr<WindowTreeHost> WindowTreeHost::Create(
    ui::PlatformWindowInitProperties properties,
    Env* env) {
  return std::make_unique<WindowTreeHostPlatform>(
      std::move(properties),
      std::make_unique<aura::Window>(nullptr, client::WINDOW_TYPE_UNKNOWN,
                                     env ? env : Env::GetInstance()));
}

WindowTreeHostPlatform::WindowTreeHostPlatform(
    ui::PlatformWindowInitProperties properties,
    std::unique_ptr<Window> window,
    const char* trace_environment_name)
    : WindowTreeHost(std::move(window)) {
  bounds_ = properties.bounds;
  CreateCompositor(viz::FrameSinkId(),
                   /* force_software_compositor */ false,
                   /* external_begin_frames_enabled */ false,
                   /* are_events_in_pixels */ true, trace_environment_name);
  CreateAndSetPlatformWindow(std::move(properties));
}

WindowTreeHostPlatform::WindowTreeHostPlatform(std::unique_ptr<Window> window)
    : WindowTreeHost(std::move(window)),
      widget_(gfx::kNullAcceleratedWidget),
      current_cursor_(ui::CursorType::kNull) {}

void WindowTreeHostPlatform::CreateAndSetPlatformWindow(
    ui::PlatformWindowInitProperties properties) {
#if defined(USE_OZONE)
  platform_window_ = ui::OzonePlatform::GetInstance()->CreatePlatformWindow(
      this, std::move(properties));
#elif defined(OS_WIN)
  platform_window_.reset(new ui::WinWindow(this, properties.bounds));
#elif defined(OS_ANDROID)
  platform_window_.reset(new ui::PlatformWindowAndroid(this));
#elif defined(USE_X11)
  platform_window_.reset(new ui::X11Window(this, properties.bounds));
#else
  NOTIMPLEMENTED();
#endif
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
  return platform_window_ ? platform_window_->GetBounds() : gfx::Rect();
}

void WindowTreeHostPlatform::SetBoundsInPixels(
    const gfx::Rect& bounds,
    const viz::LocalSurfaceId& local_surface_id,
    base::TimeTicks local_surface_id_allocation_time) {
  pending_size_ = bounds.size();
  pending_local_surface_id_ = local_surface_id;
  pending_local_surface_id_allocation_time_ = local_surface_id_allocation_time;
  platform_window_->SetBounds(bounds);
}

gfx::Point WindowTreeHostPlatform::GetLocationOnScreenInPixels() const {
  return platform_window_->GetBounds().origin();
}

void WindowTreeHostPlatform::SetCapture() {
  platform_window_->SetCapture();
}

void WindowTreeHostPlatform::ReleaseCapture() {
  platform_window_->ReleaseCapture();
}

bool WindowTreeHostPlatform::CaptureSystemKeyEventsImpl(
    base::Optional<base::flat_set<ui::DomCode>> dom_codes) {
  // Only one KeyboardHook should be active at a time, otherwise there will be
  // problems with event routing (i.e. which Hook takes precedence) and
  // destruction ordering.
  DCHECK(!keyboard_hook_);
  keyboard_hook_ = ui::KeyboardHook::Create(
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
#if !defined(X11)
  return ui::GenerateDomKeyboardLayoutMap();
#else
  NOTIMPLEMENTED();
  return {};
#endif
}

void WindowTreeHostPlatform::SetCursorNative(gfx::NativeCursor cursor) {
  if (cursor == current_cursor_)
    return;
  current_cursor_ = cursor;

#if defined(OS_WIN)
  ui::CursorLoaderWin cursor_loader;
  cursor_loader.SetPlatformCursor(&cursor);
#endif

  platform_window_->SetCursor(cursor.platform());
}

void WindowTreeHostPlatform::MoveCursorToScreenLocationInPixels(
    const gfx::Point& location_in_pixels) {
  platform_window_->MoveCursorTo(location_in_pixels);
}

void WindowTreeHostPlatform::OnCursorVisibilityChangedNative(bool show) {
  NOTIMPLEMENTED();
}

void WindowTreeHostPlatform::OnBoundsChanged(const gfx::Rect& new_bounds) {
  float current_scale = compositor()->device_scale_factor();
  float new_scale = ui::GetScaleFactorForNativeView(window());
  gfx::Rect old_bounds = bounds_;
  bounds_ = new_bounds;
  if (bounds_.origin() != old_bounds.origin())
    OnHostMovedInPixels(bounds_.origin());
  if (pending_local_surface_id_.is_valid() ||
      bounds_.size() != old_bounds.size() || current_scale != new_scale) {
    viz::LocalSurfaceId local_surface_id;
    base::TimeTicks allocation_time;
    if (bounds_.size() == pending_size_) {
      local_surface_id = pending_local_surface_id_;
      allocation_time = pending_local_surface_id_allocation_time_;
    }
    pending_local_surface_id_ = viz::LocalSurfaceId();
    pending_local_surface_id_allocation_time_ = base::TimeTicks();
    pending_size_ = gfx::Size();
    OnHostResizedInPixels(bounds_.size(), local_surface_id, allocation_time);
  }
}

void WindowTreeHostPlatform::OnDamageRect(const gfx::Rect& damage_rect) {
  compositor()->ScheduleRedrawRect(damage_rect);
}

void WindowTreeHostPlatform::DispatchEvent(ui::Event* event) {
  TRACE_EVENT0("input", "WindowTreeHostPlatform::DispatchEvent");
  ui::EventDispatchDetails details = SendEventToSink(event);
  if (details.dispatcher_destroyed) {
    event->SetHandled();
    return;
  }

  // Reset the cursor on ET_MOUSE_EXITED, so that when the mouse re-enters the
  // window, the cursor is updated correctly.
  if (event->type() == ui::ET_MOUSE_EXITED) {
    client::CursorClient* cursor_client = client::GetCursorClient(window());
    if (cursor_client) {
      // The cursor-change needs to happen through the CursorClient so that
      // other external states are updated correctly, instead of just changing
      // |current_cursor_| here.
      cursor_client->SetCursor(ui::CursorType::kNone);
      DCHECK_EQ(ui::CursorType::kNone, current_cursor_.native_type());
    }
  }
}

void WindowTreeHostPlatform::OnCloseRequest() {
#if defined(OS_WIN)
  // TODO: this obviously shouldn't be here.
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
#else
  OnHostCloseRequested();
#endif
}

void WindowTreeHostPlatform::OnClosed() {}

void WindowTreeHostPlatform::OnWindowStateChanged(
    ui::PlatformWindowState new_state) {}

void WindowTreeHostPlatform::OnLostCapture() {
  OnHostLostWindowCapture();
}

void WindowTreeHostPlatform::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
  widget_ = widget;
  // This may be called before the Compositor has been created.
  if (compositor())
    WindowTreeHost::OnAcceleratedWidgetAvailable();
}

void WindowTreeHostPlatform::OnAcceleratedWidgetDestroyed() {
  gfx::AcceleratedWidget widget = compositor()->ReleaseAcceleratedWidget();
  DCHECK_EQ(widget, widget_);
  widget_ = gfx::kNullAcceleratedWidget;
}

void WindowTreeHostPlatform::OnActivationChanged(bool active) {
}

}  // namespace aura

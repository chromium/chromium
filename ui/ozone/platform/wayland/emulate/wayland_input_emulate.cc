// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/emulate/wayland_input_emulate.h"

#include <linux/input.h>
#include <wayland-client-protocol.h>
#include <weston-test-client-protocol.h>

#include <memory>

#include "base/logging.h"
#include "base/time/time.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/platform_window/common/platform_window_defaults.h"

namespace wl {

WaylandInputEmulate::PendingEvent::PendingEvent(
    ui::EventType event_type,
    gfx::AcceleratedWidget target_widget)
    : type(event_type), widget(target_widget) {
  DCHECK(type == ui::EventType::ET_MOUSE_MOVED ||
         type == ui::EventType::ET_MOUSE_PRESSED ||
         type == ui::EventType::ET_MOUSE_RELEASED ||
         type == ui::EventType::ET_KEY_PRESSED ||
         type == ui::EventType::ET_KEY_RELEASED);
}

WaylandInputEmulate::PendingEvent::~PendingEvent() = default;

WaylandInputEmulate::TestWindow::TestWindow(
    gfx::AcceleratedWidget target_widget,
    WaylandInputEmulate* input_emulate)
    : widget(target_widget), emulate(input_emulate) {}

WaylandInputEmulate::TestWindow::~TestWindow() = default;

WaylandInputEmulate::WaylandInputEmulate() {
  auto* wayland_proxy = wl::WaylandProxy::GetInstance();
  DCHECK(wayland_proxy);

  wayland_proxy->SetDelegate(this);

  registry_ = wl_display_get_registry(wayland_proxy->GetDisplayWrapper());
  if (!registry_)
    LOG(FATAL) << "Failed to get Wayland registry";

  static const wl_registry_listener registry_listener = {
      &WaylandInputEmulate::Global};

  wl_registry_add_listener(registry_, &registry_listener, this);

  // Roundtrip one time to get the weston-test global.
  wayland_proxy->RoundTripQueue();
  if (!weston_test_)
    LOG(FATAL) << "weston-test is not available.";

  static const struct weston_test_listener test_listener = {
      &WaylandInputEmulate::HandlePointerPosition,
      &WaylandInputEmulate::HandlePointerButton,
      &WaylandInputEmulate::HandleKeyboardKey,
  };
  weston_test_add_listener(weston_test_, &test_listener, this);
}

WaylandInputEmulate::~WaylandInputEmulate() {
  DCHECK(observers_.empty());
  auto* wayland_proxy = wl::WaylandProxy::GetInstance();
  DCHECK(wayland_proxy);
  wayland_proxy->SetDelegate(nullptr);

  weston_test_destroy(weston_test_);
  wl_registry_destroy(registry_);
}

void WaylandInputEmulate::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void WaylandInputEmulate::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void WaylandInputEmulate::EmulatePointerMotion(gfx::AcceleratedWidget widget,
                                               gfx::Point mouse_surface_loc) {
  auto it = windows_.find(widget);
  DCHECK(it != windows_.end());

  auto* test_window = it->second.get();
  if (!test_window->buffer_attached_and_configured) {
    auto pending_event =
        std::make_unique<PendingEvent>(ui::EventType::ET_MOUSE_MOVED, widget);
    pending_event->pointer_surface_location_in_px = mouse_surface_loc;
    test_window->pending_events.emplace_back(std::move(pending_event));
    return;
  }

  auto* wayland_proxy = wl::WaylandProxy::GetInstance();
  DCHECK(wayland_proxy);

  auto* wlsurface = wayland_proxy->GetWlSurfaceForAcceleratedWidget(widget);

  // If it's a toplevel window, activate it. This results in raising the the
  // parent window and its children windows.
  auto window_type = wayland_proxy->GetWindowType(widget);
  if (window_type != ui::PlatformWindowType::kTooltip &&
      window_type != ui::PlatformWindowType::kMenu &&
      !wayland_proxy->WindowHasPointerFocus(widget)) {
    weston_test_activate_surface(weston_test_, wlsurface);
  }

  timespec ts = (base::TimeTicks::Now() - base::TimeTicks()).ToTimeSpec();
  weston_test_move_pointer(weston_test_, wlsurface,
                           static_cast<uint64_t>(ts.tv_sec) >> 32,
                           ts.tv_sec & 0xffffffff, ts.tv_nsec,
                           mouse_surface_loc.x(), mouse_surface_loc.y());
  wayland_proxy->ScheduleDisplayFlush();
}

void WaylandInputEmulate::EmulatePointerButton(gfx::AcceleratedWidget widget,
                                               ui::EventType event_type,
                                               uint32_t changed_button) {
  DCHECK(event_type == ui::EventType::ET_MOUSE_PRESSED ||
         event_type == ui::EventType::ET_MOUSE_RELEASED);
  // A button press/release event uses previous location that Ozone/Wayland got
  // when OnPointerMotionEvent was called.
  auto it = windows_.find(widget);
  DCHECK(it != windows_.end());

  auto* test_window = it->second.get();
  if (!test_window->buffer_attached_and_configured) {
    auto pending_event = std::make_unique<PendingEvent>(event_type, widget);
    pending_event->mouse_button = changed_button;
    test_window->pending_events.emplace_back(std::move(pending_event));
    return;
  }

  DCHECK_NE(0u, changed_button);
  timespec ts = (base::TimeTicks::Now() - base::TimeTicks()).ToTimeSpec();
  weston_test_send_button(weston_test_, static_cast<uint64_t>(ts.tv_sec) >> 32,
                          ts.tv_sec & 0xffffffff, ts.tv_nsec, changed_button,
                          (event_type == ui::EventType::ET_MOUSE_PRESSED
                               ? WL_POINTER_BUTTON_STATE_PRESSED
                               : WL_POINTER_BUTTON_STATE_RELEASED));
}

void WaylandInputEmulate::EmulateKeyboardKey(gfx::AcceleratedWidget widget,
                                             ui::EventType event_type,
                                             ui::DomCode dom_code) {
  DCHECK(event_type == ui::EventType::ET_KEY_PRESSED ||
         event_type == ui::EventType::ET_KEY_RELEASED);

  auto it = windows_.find(widget);
  DCHECK(it != windows_.end());

  auto* test_window = it->second.get();
  if (!test_window->buffer_attached_and_configured) {
    auto pending_event = std::make_unique<PendingEvent>(event_type, widget);
    pending_event->key_dom_code = dom_code;
    test_window->pending_events.emplace_back(std::move(pending_event));
    return;
  }

  auto* wayland_proxy = wl::WaylandProxy::GetInstance();
  DCHECK(wayland_proxy);

  auto* wlsurface = wayland_proxy->GetWlSurfaceForAcceleratedWidget(widget);

  // Raise the window and set keyboard focus.
  if (!wayland_proxy->WindowHasKeyboardFocus(widget))
    weston_test_activate_surface(weston_test_, wlsurface);

  timespec ts = (base::TimeTicks::Now() - base::TimeTicks()).ToTimeSpec();
  weston_test_send_key(weston_test_, static_cast<uint64_t>(ts.tv_sec) >> 32,
                       ts.tv_sec & 0xffffffff, ts.tv_nsec,
                       ui::KeycodeConverter::DomCodeToEvdevCode(dom_code),
                       (event_type == ui::EventType::ET_KEY_PRESSED
                            ? WL_KEYBOARD_KEY_STATE_PRESSED
                            : WL_KEYBOARD_KEY_STATE_RELEASED));
  wayland_proxy->ScheduleDisplayFlush();
}

void WaylandInputEmulate::OnWindowConfigured(gfx::AcceleratedWidget widget,
                                             bool is_configured) {
  auto it = windows_.find(widget);
  DCHECK(it != windows_.end());

  auto* test_surface = it->second.get();
  // The buffer is no longer attached as the window lost its role. Wait until
  // the configuration event comes.
  if (!is_configured) {
    test_surface->buffer_attached_and_configured = false;
    // Also destroy the frame callback...
    if (test_surface->frame_callback) {
      wl_callback_destroy(test_surface->frame_callback);
      test_surface->frame_callback = nullptr;
    }
    // ... and the buffer.
    if (test_surface->buffer) {
      auto* wayland_proxy = wl::WaylandProxy::GetInstance();
      DCHECK(wayland_proxy);
      wayland_proxy->DestroyShmForWlBuffer(test_surface->buffer);
      wayland_proxy->ScheduleDisplayFlush();
      test_surface->buffer = nullptr;
    }
    return;
  }

  if (test_surface->buffer_attached_and_configured)
    return;

  auto* wayland_proxy = wl::WaylandProxy::GetInstance();
  DCHECK(wayland_proxy);

  // Once window is configured aka xdg_toplevel/popup role is assigned, a buffer
  // with correct size must be attached. Otherwise, actual size of the surface
  // will be size of the last attached buffer or 0x0).
  //
  // This is needed as running some tests doesn't result in sending frames that
  // require buffers to be created.
  auto buffer_size = wayland_proxy->GetWindowBounds(widget).size();
  // Adjust the buffer size in case if the window was created with empty size.
  if (buffer_size.IsEmpty())
    buffer_size.SetSize(1, 1);
  test_surface->buffer = wayland_proxy->CreateShmBasedWlBuffer(buffer_size);

  auto* wlsurface = wayland_proxy->GetWlSurfaceForAcceleratedWidget(widget);
  wl_surface_attach(wlsurface, test_surface->buffer, 0, 0);
  wl_surface_damage(wlsurface, 0, 0, buffer_size.width(), buffer_size.height());

  static const struct wl_callback_listener kFrameCallbackListener = {
      &WaylandInputEmulate::FrameCallbackHandler};

  // Setup frame callback to know when the surface is finally ready to get
  // events. Otherwise, the width & height might not have been correctly set
  // before the mouse events are sent.
  test_surface->frame_callback = wl_surface_frame(wlsurface);
  wl_callback_add_listener(test_surface->frame_callback,
                           &kFrameCallbackListener, this);

  wl_surface_commit(wlsurface);

  wayland_proxy->ScheduleDisplayFlush();
}

void WaylandInputEmulate::OnWindowRemoved(gfx::AcceleratedWidget widget) {
  auto it = windows_.find(widget);
  DCHECK(it != windows_.end());

  // Destroy the frame callback.
  if (it->second->frame_callback) {
    wl_callback_destroy(it->second->frame_callback);
    it->second->frame_callback = nullptr;
  }

  // Destroy the attached buffer.
  if (it->second->buffer) {
    auto* wayland_proxy = wl::WaylandProxy::GetInstance();
    DCHECK(wayland_proxy);
    wayland_proxy->DestroyShmForWlBuffer(it->second->buffer);
    wayland_proxy->ScheduleDisplayFlush();
  }
  windows_.erase(it);
}

void WaylandInputEmulate::OnWindowAdded(gfx::AcceleratedWidget widget) {
  // It must be a first run. Thus, reset the pointer state so that the next
  // tests do not inherit the previous test's clicks. Otherwise, there can be
  // a button pressed state left if the previous test crashed.
  if (windows_.empty()) {
    weston_test_reset_pointer(weston_test_);
    auto* wayland_proxy = wl::WaylandProxy::GetInstance();
    DCHECK(wayland_proxy);
    wayland_proxy->ScheduleDisplayFlush();
  }

  windows_.emplace(
      widget, std::make_unique<WaylandInputEmulate::TestWindow>(widget, this));
}

// static
void WaylandInputEmulate::HandlePointerPosition(void* data,
                                                struct weston_test* weston_test,
                                                wl_fixed_t x,
                                                wl_fixed_t y) {
  WaylandInputEmulate* emulate = static_cast<WaylandInputEmulate*>(data);
  auto mouse_position_on_screen_px =
      gfx::Point(wl_fixed_to_int(x), wl_fixed_to_int(y));
  for (WaylandInputEmulate::Observer& observer : emulate->observers_)
    observer.OnPointerMotionGlobal(mouse_position_on_screen_px);
}

// static
void WaylandInputEmulate::HandlePointerButton(void* data,
                                              struct weston_test* weston_test,
                                              int32_t button,
                                              uint32_t state) {
  WaylandInputEmulate* emulate = static_cast<WaylandInputEmulate*>(data);
  for (WaylandInputEmulate::Observer& observer : emulate->observers_) {
    observer.OnPointerButtonGlobal(button,
                                   state == WL_POINTER_BUTTON_STATE_PRESSED);
  }
}

// static
void WaylandInputEmulate::HandleKeyboardKey(void* data,
                                            struct weston_test* weston_test,
                                            uint32_t key,
                                            uint32_t state) {
  WaylandInputEmulate* emulate = static_cast<WaylandInputEmulate*>(data);
  for (WaylandInputEmulate::Observer& observer : emulate->observers_)
    observer.OnKeyboardKey(key, state == WL_KEYBOARD_KEY_STATE_PRESSED);
}

// static
void WaylandInputEmulate::Global(void* data,
                                 wl_registry* registry,
                                 uint32_t name,
                                 const char* interface,
                                 uint32_t version) {
  auto* emulate = static_cast<WaylandInputEmulate*>(data);
  if (strcmp(interface, "weston_test") == 0) {
    const struct wl_interface* wayland_interface =
        static_cast<const struct wl_interface*>(&weston_test_interface);
    emulate->weston_test_ = static_cast<struct weston_test*>(
        wl_registry_bind(registry, name, wayland_interface, version));
  }
}

// static
void WaylandInputEmulate::FrameCallbackHandler(void* data,
                                               struct wl_callback* callback,
                                               uint32_t time) {
  WaylandInputEmulate* emulate = static_cast<WaylandInputEmulate*>(data);
  CHECK(emulate)
      << "WaylandInputEmulate was destroyed before a frame callback arrived";

  WaylandInputEmulate::TestWindow* window = nullptr;
  for (const auto& window_item : emulate->windows_) {
    if (window_item.second->frame_callback == callback) {
      window = window_item.second.get();
      break;
    }
  }

  if (!window)
    return;

  wl_callback_destroy(window->frame_callback);
  window->frame_callback = nullptr;

  DCHECK(!window->buffer_attached_and_configured);
  window->buffer_attached_and_configured = true;

  while (!window->pending_events.empty()) {
    auto event = std::move(window->pending_events.front());
    window->pending_events.pop_front();

    auto* input_emulate = window->emulate;
    DCHECK(input_emulate);

    switch (event->type) {
      case ui::EventType::ET_MOUSE_MOVED:
        input_emulate->EmulatePointerMotion(
            window->widget, event->pointer_surface_location_in_px);
        break;
      case ui::EventType::ET_MOUSE_PRESSED:
      case ui::EventType::ET_MOUSE_RELEASED:
        input_emulate->EmulatePointerButton(window->widget, event->type,
                                            event->mouse_button);
        break;
      case ui::EventType::ET_KEY_PRESSED:
      case ui::EventType::ET_KEY_RELEASED:
        input_emulate->EmulateKeyboardKey(window->widget, event->type,
                                          event->key_dom_code);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

}  // namespace wl

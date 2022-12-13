// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/emulate/wayland_input_emulate.h"

#include <linux/input.h>
#include <wayland-client-protocol.h>
#include <weston-test-client-protocol.h>
#include <weston-test-server-protocol.h>

#include <memory>

#include "base/logging.h"
#include "base/time/time.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/platform_window/common/platform_window_defaults.h"

namespace wl {

WaylandInputEmulate::PendingEvent::PendingEvent(
    ui::EventType event_type,
    gfx::AcceleratedWidget target_widget,
    WaylandInputEmulate* emulate)
    : type(event_type), widget(target_widget) {
  DCHECK(type == ui::EventType::ET_MOUSE_MOVED ||
         type == ui::EventType::ET_MOUSE_PRESSED ||
         type == ui::EventType::ET_MOUSE_RELEASED ||
         type == ui::EventType::ET_KEY_PRESSED ||
         type == ui::EventType::ET_KEY_RELEASED ||
         type == ui::EventType::ET_TOUCH_PRESSED ||
         type == ui::EventType::ET_TOUCH_MOVED ||
         type == ui::EventType::ET_TOUCH_RELEASED);
  auto it = emulate->windows_.find(widget);
  if (it != emulate->windows_.end()) {
    test_window = it->second->weak_factory.GetWeakPtr();
  }
}

namespace {

int EventTypeToWaylandTouchType(ui::EventType event_type) {
  switch (event_type) {
    case ui::EventType::ET_TOUCH_PRESSED:
      return WL_TOUCH_DOWN;
    case ui::EventType::ET_TOUCH_MOVED:
      return WL_TOUCH_MOTION;
    default:
      return WL_TOUCH_UP;
  }
}

}  // namespace

WaylandInputEmulate::PendingEvent::~PendingEvent() = default;

WaylandInputEmulate::TestWindow::TestWindow() = default;
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
      nullptr,  // capture_screenshot_done
      &WaylandInputEmulate::HandleTouchReceived,
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

void WaylandInputEmulate::EmulatePointerMotion(
    gfx::AcceleratedWidget widget,
    const gfx::Point& mouse_surface_loc,
    const gfx::Point& mouse_screen_loc_in_px) {
  if (AnyWindowWaitingForBufferCommit()) {
    auto pending_event = std::make_unique<PendingEvent>(
        ui::EventType::ET_MOUSE_MOVED, widget, this);
    pending_event->pointer_surface_location = mouse_surface_loc;
    pending_event->pointer_screen_location_in_px = mouse_screen_loc_in_px;
    pending_events_.emplace_back(std::move(pending_event));
    return;
  }

  // If the widget does not have a buffer, pretend it doesn't exist. It is
  // treated similarly on the server.
  auto it = windows_.find(widget);
  if (it != windows_.end()) {
    if (!it->second->buffer_attached_and_configured)
      widget = 0;
  }

  auto* wayland_proxy = wl::WaylandProxy::GetInstance();
  DCHECK(wayland_proxy);

  wl_surface* target_surface = nullptr;
  gfx::Point target_location = mouse_screen_loc_in_px;
  if (widget) {
    auto* wlsurface = wayland_proxy->GetWlSurfaceForAcceleratedWidget(widget);
    bool screen_coordinates =
        wayland_proxy->GetWaylandWindowForAcceleratedWidget(widget)
            ->IsScreenCoordinatesEnabled();

    target_surface = screen_coordinates ? nullptr : wlsurface;
    target_location =
        screen_coordinates ? mouse_screen_loc_in_px : mouse_surface_loc;
  }

  // TODO(crbug.com/1306688): The coordinate should be in DIP.
  timespec ts = (base::TimeTicks::Now() - base::TimeTicks()).ToTimeSpec();
  weston_test_move_pointer(weston_test_, target_surface,
                           static_cast<uint64_t>(ts.tv_sec) >> 32,
                           ts.tv_sec & 0xffffffff, ts.tv_nsec,
                           target_location.x(), target_location.y());
  wayland_proxy->FlushForTesting();
}

void WaylandInputEmulate::EmulatePointerButton(gfx::AcceleratedWidget widget,
                                               ui::EventType event_type,
                                               uint32_t changed_button) {
  DCHECK(event_type == ui::EventType::ET_MOUSE_PRESSED ||
         event_type == ui::EventType::ET_MOUSE_RELEASED);

  if (AnyWindowWaitingForBufferCommit()) {
    auto pending_event =
        std::make_unique<PendingEvent>(event_type, widget, this);
    pending_event->mouse_button = changed_button;
    pending_events_.emplace_back(std::move(pending_event));
    return;
  }

  DCHECK_NE(0u, changed_button);
  timespec ts = (base::TimeTicks::Now() - base::TimeTicks()).ToTimeSpec();
  weston_test_send_button(weston_test_, static_cast<uint64_t>(ts.tv_sec) >> 32,
                          ts.tv_sec & 0xffffffff, ts.tv_nsec, changed_button,
                          (event_type == ui::EventType::ET_MOUSE_PRESSED
                               ? WL_POINTER_BUTTON_STATE_PRESSED
                               : WL_POINTER_BUTTON_STATE_RELEASED));
  auto* wayland_proxy = wl::WaylandProxy::GetInstance();
  wayland_proxy->FlushForTesting();
}

void WaylandInputEmulate::EmulateKeyboardKey(gfx::AcceleratedWidget widget,
                                             ui::EventType event_type,
                                             ui::DomCode dom_code) {
  DCHECK(event_type == ui::EventType::ET_KEY_PRESSED ||
         event_type == ui::EventType::ET_KEY_RELEASED);

  if (AnyWindowWaitingForBufferCommit()) {
    auto pending_event =
        std::make_unique<PendingEvent>(event_type, widget, this);
    pending_event->key_dom_code = dom_code;
    pending_events_.emplace_back(std::move(pending_event));
    return;
  }

  timespec ts = (base::TimeTicks::Now() - base::TimeTicks()).ToTimeSpec();
  weston_test_send_key(weston_test_, static_cast<uint64_t>(ts.tv_sec) >> 32,
                       ts.tv_sec & 0xffffffff, ts.tv_nsec,
                       ui::KeycodeConverter::DomCodeToEvdevCode(dom_code),
                       (event_type == ui::EventType::ET_KEY_PRESSED
                            ? WL_KEYBOARD_KEY_STATE_PRESSED
                            : WL_KEYBOARD_KEY_STATE_RELEASED));
  auto* wayland_proxy = wl::WaylandProxy::GetInstance();
  wayland_proxy->FlushForTesting();
}

void WaylandInputEmulate::EmulateTouch(gfx::AcceleratedWidget widget,
                                       ui::EventType event_type,
                                       int id,
                                       const gfx::Point& touch_screen_loc) {
  if (AnyWindowWaitingForBufferCommit()) {
    auto pending_event =
        std::make_unique<PendingEvent>(event_type, widget, this);
    pending_event->touch_screen_location = touch_screen_loc;
    pending_event->touch_id = id;
    pending_events_.emplace_back(std::move(pending_event));
    return;
  }

  timespec ts = (base::TimeTicks::Now() - base::TimeTicks()).ToTimeSpec();
  weston_test_send_touch(weston_test_, static_cast<uint64_t>(ts.tv_sec) >> 32,
                         ts.tv_sec & 0xffffffff, ts.tv_nsec, id,
                         wl_fixed_from_int(touch_screen_loc.x()),
                         wl_fixed_from_int(touch_screen_loc.y()),
                         EventTypeToWaylandTouchType(event_type));
  auto* wayland_proxy = wl::WaylandProxy::GetInstance();
  wayland_proxy->FlushForTesting();
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
    test_surface->waiting_for_buffer_commit = false;
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
      wayland_proxy->FlushForTesting();
      test_surface->buffer = nullptr;
    }
    DispatchPendingEvents();
    return;
  }

  if (test_surface->buffer_attached_and_configured)
    return;

  test_surface->waiting_for_buffer_commit = true;
  auto* wayland_proxy = wl::WaylandProxy::GetInstance();
  DCHECK(wayland_proxy);

  // Once window is configured aka xdg_toplevel/popup role is assigned, a buffer
  // with correct size must be attached. Otherwise, actual size of the surface
  // will be size of the last attached buffer or 0x0).
  //
  // This is needed as running some tests doesn't result in sending frames that
  // require buffers to be created.
  auto* wayland_window =
      wayland_proxy->GetWaylandWindowForAcceleratedWidget(widget);
  auto buffer_size = wayland_window->GetBoundsInPixels().size();
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

  wayland_proxy->FlushForTesting();
}

void WaylandInputEmulate::OnWindowRoleAssigned(gfx::AcceleratedWidget widget) {
  auto it = windows_.find(widget);
  DCHECK(it != windows_.end());

  // If a window has been assigned a popup role, then we must wait for a buffer
  // to be committed before any events can be processed.
  auto* test_surface = it->second.get();
  test_surface->waiting_for_buffer_commit = true;
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
    wayland_proxy->FlushForTesting();
  }
  windows_.erase(it);
}

void WaylandInputEmulate::OnWindowAdded(gfx::AcceleratedWidget widget) {
  // It must be a first run. Thus, reset the pointer state so that the next
  // tests do not inherit the previous test's clicks. Otherwise, there can be
  // a button pressed state left if the previous test crashed.
  if (windows_.empty()) {
    weston_test_reset_pointer(weston_test_);

    // Release all meta-keys to deal with carry-over state from previous tests.
    std::vector<ui::DomCode> meta_keys = {
        ui::DomCode::CONTROL_LEFT,  ui::DomCode::SHIFT_LEFT,
        ui::DomCode::ALT_LEFT,      ui::DomCode::META_LEFT,
        ui::DomCode::CONTROL_RIGHT, ui::DomCode::SHIFT_RIGHT,
        ui::DomCode::ALT_RIGHT,     ui::DomCode::META_RIGHT,
    };
    for (auto key : meta_keys) {
      timespec ts = (base::TimeTicks::Now() - base::TimeTicks()).ToTimeSpec();
      weston_test_send_key(weston_test_, static_cast<uint64_t>(ts.tv_sec) >> 32,
                           ts.tv_sec & 0xffffffff, ts.tv_nsec,
                           ui::KeycodeConverter::DomCodeToEvdevCode(key),
                           WL_KEYBOARD_KEY_STATE_RELEASED);
    }

    auto* wayland_proxy = wl::WaylandProxy::GetInstance();
    DCHECK(wayland_proxy);
    wayland_proxy->FlushForTesting();
  }

  windows_.emplace(widget, std::make_unique<WaylandInputEmulate::TestWindow>());
}

// static
void WaylandInputEmulate::HandlePointerPosition(void* data,
                                                struct weston_test* weston_test,
                                                wl_fixed_t x,
                                                wl_fixed_t y) {
  auto* emulate = static_cast<WaylandInputEmulate*>(data);
  gfx::Point mouse_position_on_screen_px(wl_fixed_to_int(x),
                                         wl_fixed_to_int(y));
  for (WaylandInputEmulate::Observer& observer : emulate->observers_)
    observer.OnPointerMotionGlobal(mouse_position_on_screen_px);
}

// static
void WaylandInputEmulate::HandlePointerButton(void* data,
                                              struct weston_test* weston_test,
                                              int32_t button,
                                              uint32_t state) {
  auto* emulate = static_cast<WaylandInputEmulate*>(data);
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
  auto* emulate = static_cast<WaylandInputEmulate*>(data);
  for (WaylandInputEmulate::Observer& observer : emulate->observers_)
    observer.OnKeyboardKey(key, state == WL_KEYBOARD_KEY_STATE_PRESSED);
}

// static
void WaylandInputEmulate::HandleTouchReceived(void* data,
                                              struct weston_test* weston_test,
                                              wl_fixed_t x,
                                              wl_fixed_t y) {
  auto* emulate = static_cast<WaylandInputEmulate*>(data);
  auto touch_position_on_screen_px =
      gfx::Point(wl_fixed_to_int(x), wl_fixed_to_int(y));
  for (WaylandInputEmulate::Observer& observer : emulate->observers_)
    observer.OnTouchReceived(touch_position_on_screen_px);
}

// static
void WaylandInputEmulate::Global(void* data,
                                 wl_registry* registry,
                                 uint32_t name,
                                 const char* interface,
                                 uint32_t version) {
  auto* emulate = static_cast<WaylandInputEmulate*>(data);
  if (strcmp(interface, "weston_test") == 0) {
    const auto* wayland_interface =
        static_cast<const struct wl_interface*>(&weston_test_interface);
    emulate->weston_test_ = static_cast<struct weston_test*>(
        wl_registry_bind(registry, name, wayland_interface, version));
  }
}

// static
void WaylandInputEmulate::FrameCallbackHandler(void* data,
                                               struct wl_callback* callback,
                                               uint32_t time) {
  auto* emulate = static_cast<WaylandInputEmulate*>(data);
  CHECK(emulate)
      << "WaylandInputEmulate was destroyed before a frame callback arrived";

  WaylandInputEmulate::TestWindow* window = nullptr;
  for (const auto& window_item : emulate->windows_) {
    if (window_item.second->frame_callback == callback) {
      window = window_item.second.get();
      break;
    }
  }

  if (window) {
    wl_callback_destroy(window->frame_callback);
    window->frame_callback = nullptr;

    DCHECK(!window->buffer_attached_and_configured);
    window->buffer_attached_and_configured = true;
    window->waiting_for_buffer_commit = false;
  }

  emulate->DispatchPendingEvents();
}

bool WaylandInputEmulate::AnyWindowWaitingForBufferCommit() {
  for (auto& it : windows_) {
    if (it.second->waiting_for_buffer_commit)
      return true;
  }
  return false;
}

void WaylandInputEmulate::DispatchPendingEvents() {
  while (!pending_events_.empty()) {
    // Cannot dispatch pending events if there's a window waiting for a buffer
    // commit.
    if (AnyWindowWaitingForBufferCommit())
      return;
    auto event = std::move(pending_events_.front());
    pending_events_.pop_front();

    switch (event->type) {
      case ui::EventType::ET_MOUSE_MOVED:
        // If the test window has been destroyed then do not use a widget.
        if (!event->test_window)
          event->widget = 0;
        EmulatePointerMotion(
            /*widget=*/event->widget, event->pointer_surface_location,
            event->pointer_screen_location_in_px);
        break;
      case ui::EventType::ET_MOUSE_PRESSED:
      case ui::EventType::ET_MOUSE_RELEASED:
        EmulatePointerButton(/*widget=*/0, event->type, event->mouse_button);
        break;
      case ui::EventType::ET_KEY_PRESSED:
      case ui::EventType::ET_KEY_RELEASED:
        EmulateKeyboardKey(/*widget=*/0, event->type, event->key_dom_code);
        break;
      case ui::EventType::ET_TOUCH_PRESSED:
      case ui::EventType::ET_TOUCH_MOVED:
      case ui::EventType::ET_TOUCH_RELEASED:
        EmulateTouch(/*widget=*/0, event->type, event->touch_id,
                     event->touch_screen_location);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

}  // namespace wl

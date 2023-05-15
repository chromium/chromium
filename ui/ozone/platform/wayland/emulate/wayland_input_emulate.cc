// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/emulate/wayland_input_emulate.h"

#include <ui-controls-unstable-v1-client-protocol.h>
#include <wayland-client-protocol.h>

#include "base/logging.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/ozone/platform/wayland/host/shell_toplevel_wrapper.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/xdg_surface_wrapper_impl.h"
#include "ui/ozone/platform/wayland/host/xdg_toplevel_wrapper_impl.h"

namespace {

// send_key_events() is only available since version 2.
constexpr uint32_t kMinVersion = 2;

}  // namespace

namespace wl {

WaylandInputEmulate::PendingRequest::PendingRequest(
    PendingRequestType request_type,
    uint32_t request_id)
    : type(request_type), request_id(request_id) {}

WaylandInputEmulate::PendingRequest::~PendingRequest() = default;

WaylandInputEmulate::TestWindow::TestWindow() = default;
WaylandInputEmulate::TestWindow::~TestWindow() = default;

WaylandInputEmulate::WaylandInputEmulate(
    base::RepeatingCallback<void(uint32_t)> request_processed)
    : request_processed_callback_(std::move(request_processed)) {
  CHECK(!request_processed_callback_.is_null());

  auto* wayland_proxy = wl::WaylandProxy::GetInstance();
  DCHECK(wayland_proxy);
  wayland_proxy->SetDelegate(this);
}

WaylandInputEmulate::~WaylandInputEmulate() {
  auto* wayland_proxy = wl::WaylandProxy::GetInstance();
  DCHECK(wayland_proxy);
  wayland_proxy->SetDelegate(nullptr);

  // If Initialize() failed, `ui_controls_` is null.
  if (ui_controls_) {
    zcr_ui_controls_v1_destroy(ui_controls_);
  }

  CHECK(registry_)
      << "WaylandInputEmulate destroyed before Initialize() called";
  wl_registry_destroy(registry_);
}

bool WaylandInputEmulate::Initialize() {
  auto* wayland_proxy = wl::WaylandProxy::GetInstance();
  DCHECK(wayland_proxy);

  registry_ = wl_display_get_registry(wayland_proxy->GetDisplayWrapper());
  if (!registry_) {
    // If we can't get the registry, this means there is a bigger problem with
    // the Wayland connection than just ui_controls not being available.
    // Therefore, we crash instead of just returning false.
    LOG(FATAL) << "Failed to get Wayland registry.";
  }

  static const wl_registry_listener registry_listener = {
      &WaylandInputEmulate::Global};

  wl_registry_add_listener(registry_, &registry_listener, this);

  // Roundtrip one time to get the ui_controls global.
  wayland_proxy->RoundTripQueue();
  if (!ui_controls_) {
    return false;
  }

  static const struct zcr_ui_controls_v1_listener listener = {
      &WaylandInputEmulate::HandleRequestProcessed};
  zcr_ui_controls_v1_add_listener(ui_controls_, &listener, this);

  return true;
}

void WaylandInputEmulate::EmulateKeyboardKey(ui::DomCode dom_code,
                                             int key_state,
                                             int accelerator_state,
                                             uint32_t request_id) {
  if (AnyWindowWaitingForBufferCommit()) {
    auto pending_request = std::make_unique<PendingRequest>(
        PendingRequestType::KeyPress, request_id);
    pending_request->key_dom_code = dom_code;
    pending_request->key_state = key_state;
    pending_request->accelerator_state = accelerator_state;
    pending_requests_.emplace_back(std::move(pending_request));
    return;
  }

  zcr_ui_controls_v1_send_key_events(
      ui_controls_, ui::KeycodeConverter::DomCodeToEvdevCode(dom_code),
      key_state, accelerator_state, request_id);

  auto* wayland_proxy = wl::WaylandProxy::GetInstance();
  wayland_proxy->FlushForTesting();
}

void WaylandInputEmulate::EmulatePointerMotion(
    gfx::AcceleratedWidget widget,
    const gfx::Point& mouse_surface_location,
    const gfx::Point& mouse_screen_location,
    uint32_t request_id) {
  if (AnyWindowWaitingForBufferCommit()) {
    auto pending_request = std::make_unique<PendingRequest>(
        PendingRequestType::MouseMove, request_id);
    pending_request->widget = widget;
    pending_request->mouse_surface_location = mouse_surface_location;
    pending_request->mouse_screen_location = mouse_screen_location;
    pending_requests_.emplace_back(std::move(pending_request));
    return;
  }

  // If the widget does not have a buffer, pretend it doesn't exist. It is
  // treated similarly on the server.
  auto it = windows_.find(widget);
  if (it != windows_.end()) {
    if (!it->second->buffer_attached_and_configured) {
      widget = 0;
    }
  }

  auto* wayland_proxy = wl::WaylandProxy::GetInstance();
  DCHECK(wayland_proxy);

  xdg_surface* target_surface = nullptr;
  gfx::Point target_location = mouse_screen_location;
  if (widget) {
    auto* window = wayland_proxy->GetWaylandWindowForAcceleratedWidget(widget);
    auto* toplevel_window = window->AsWaylandToplevelWindow();
    auto* xdg_surface = toplevel_window ? toplevel_window->shell_toplevel()
                                              ->AsXDGToplevelWrapper()
                                              ->xdg_surface_wrapper()
                                              ->xdg_surface()
                                        : nullptr;
    bool screen_coordinates = window->IsScreenCoordinatesEnabled();

    target_surface = screen_coordinates ? nullptr : xdg_surface;
    target_location =
        screen_coordinates ? mouse_screen_location : mouse_surface_location;
  }

  zcr_ui_controls_v1_send_mouse_move(ui_controls_, target_location.x(),
                                     target_location.y(), target_surface,
                                     request_id);
  wayland_proxy->FlushForTesting();
}

void WaylandInputEmulate::EmulatePointerButton(ui_controls::MouseButton button,
                                               int button_state,
                                               int accelerator_state,
                                               uint32_t request_id) {
  if (AnyWindowWaitingForBufferCommit()) {
    auto pending_request = std::make_unique<PendingRequest>(
        PendingRequestType::MouseButton, request_id);
    pending_request->button_state = button_state;
    pending_request->button = button;
    pending_request->accelerator_state = accelerator_state;
    pending_requests_.emplace_back(std::move(pending_request));
    return;
  }

  zcr_ui_controls_v1_send_mouse_button(ui_controls_, button, button_state,
                                       accelerator_state, request_id);

  auto* wayland_proxy = wl::WaylandProxy::GetInstance();
  wayland_proxy->FlushForTesting();
}

void WaylandInputEmulate::EmulateTouch(int action,
                                       const gfx::Point& touch_screen_location,
                                       int touch_id,
                                       uint32_t request_id) {
  if (AnyWindowWaitingForBufferCommit()) {
    auto pending_request =
        std::make_unique<PendingRequest>(PendingRequestType::Touch, request_id);
    pending_request->action = action;
    pending_request->touch_screen_location = touch_screen_location;
    pending_request->touch_id = touch_id;
    pending_requests_.emplace_back(std::move(pending_request));
    return;
  }

  zcr_ui_controls_v1_send_touch(
      ui_controls_, action, touch_id, touch_screen_location.x(),
      touch_screen_location.y(), /*surface=*/nullptr, request_id);

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
    DispatchPendingRequests();
    return;
  }

  if (test_surface->buffer_attached_and_configured) {
    return;
  }

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
  if (buffer_size.IsEmpty()) {
    buffer_size.SetSize(1, 1);
  }
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
  windows_.emplace(widget, std::make_unique<WaylandInputEmulate::TestWindow>());
}

// static
void WaylandInputEmulate::HandleRequestProcessed(
    void* data,
    struct zcr_ui_controls_v1* zcr_ui_controls_v1,
    uint32_t id) {
  WaylandInputEmulate* emulate = static_cast<WaylandInputEmulate*>(data);
  emulate->request_processed_callback_.Run(id);
}

// static
void WaylandInputEmulate::Global(void* data,
                                 wl_registry* registry,
                                 uint32_t name,
                                 const char* interface,
                                 uint32_t version) {
  auto* emulate = static_cast<WaylandInputEmulate*>(data);
  if (strcmp(interface, "zcr_ui_controls_v1") == 0 && version >= kMinVersion) {
    const struct wl_interface* wayland_interface =
        static_cast<const struct wl_interface*>(&zcr_ui_controls_v1_interface);
    emulate->ui_controls_ = static_cast<struct zcr_ui_controls_v1*>(
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

  if (window) {
    wl_callback_destroy(window->frame_callback);
    window->frame_callback = nullptr;

    DCHECK(!window->buffer_attached_and_configured);
    window->buffer_attached_and_configured = true;
    window->waiting_for_buffer_commit = false;
  }

  emulate->DispatchPendingRequests();
}

bool WaylandInputEmulate::AnyWindowWaitingForBufferCommit() {
  for (auto& it : windows_) {
    if (it.second->waiting_for_buffer_commit) {
      return true;
    }
  }
  return false;
}

void WaylandInputEmulate::DispatchPendingRequests() {
  while (!pending_requests_.empty()) {
    // Cannot dispatch pending events if there's a window waiting for a buffer
    // commit.
    if (AnyWindowWaitingForBufferCommit()) {
      return;
    }
    auto event = std::move(pending_requests_.front());
    pending_requests_.pop_front();

    switch (event->type) {
      case PendingRequestType::KeyPress:
        EmulateKeyboardKey(event->key_dom_code, event->key_state,
                           event->accelerator_state, event->request_id);
        break;
      case PendingRequestType::MouseMove:
        // If the test window has been destroyed, use 0 as |widget|.
        if (windows_.find(event->widget) == windows_.end()) {
          event->widget = 0;
        }

        EmulatePointerMotion(event->widget, event->mouse_surface_location,
                             event->mouse_screen_location, event->request_id);
        break;
      case PendingRequestType::MouseButton:
        EmulatePointerButton(event->button, event->button_state,
                             event->accelerator_state, event->request_id);
        break;
      case PendingRequestType::Touch:
        EmulateTouch(event->action, event->touch_screen_location,
                     event->touch_id, event->request_id);
        break;
    }
  }
}

}  // namespace wl

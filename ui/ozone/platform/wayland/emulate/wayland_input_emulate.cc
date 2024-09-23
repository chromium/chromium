// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/emulate/wayland_input_emulate.h"

#include <ui-controls-unstable-v1-client-protocol.h>
#include <wayland-client-protocol.h>

#include <cstdint>
#include <string>

#include "base/logging.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/ozone/platform/wayland/host/shell_toplevel_wrapper.h"
#include "ui/ozone/platform/wayland/host/wayland_popup.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/xdg_popup_wrapper_impl.h"
#include "ui/ozone/platform/wayland/host/xdg_surface_wrapper_impl.h"
#include "ui/ozone/platform/wayland/host/xdg_toplevel_wrapper_impl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/base/wayland/wayland_display_util.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/test/display_test_util.h"
#endif

namespace {

// TODO(b/302179948) Increment version once server change hits beta.
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
  wayland_proxy->SetDelegate(this);

  registry_ = wayland_proxy->GetRegistry();
  if (!registry_) {
    // If we can't get the registry, this means there is a bigger problem with
    // the Wayland connection than just ui_controls not being available.
    // Therefore, we crash instead of just returning false.
    LOG(FATAL) << "Failed to get Wayland registry.";
  }

  static constexpr wl_registry_listener kRegistryListener = {
      .global = &OnGlobal, .global_remove = &OnGlobalRemove};
  wl_registry_add_listener(registry_, &kRegistryListener, this);

  // Roundtrip one time to get the ui_controls global.
  wayland_proxy->RoundTripQueue();
  if (!ui_controls_) {
    LOG(FATAL) << "ui-controls protocol extension is not available.";
  }

  static constexpr zcr_ui_controls_v1_listener kUiControlsListener = {
      .request_processed = &OnRequestProcessed};
  zcr_ui_controls_v1_add_listener(ui_controls_, &kUiControlsListener, this);
}

WaylandInputEmulate::~WaylandInputEmulate() {
  auto* wayland_proxy = wl::WaylandProxy::GetInstance();
  wayland_proxy->SetDelegate(nullptr);

  zcr_ui_controls_v1_destroy(ui_controls_);
  wl_registry_destroy(registry_);
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

  VLOG(1) << "Requesting keyboard key: dom_code="
          << ui::KeycodeConverter::DomCodeToCodeString(dom_code)
          << " state=" << key_state
          << " accelerator_state=" << accelerator_state;

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

  xdg_surface* target_surface = nullptr;
  gfx::Point target_location = mouse_screen_location;
  if (widget) {
    auto* window = wayland_proxy->GetWaylandWindowForAcceleratedWidget(widget);
    xdg_surface* xdg_surface = nullptr;
    if (auto* toplevel_window = window->AsWaylandToplevelWindow()) {
      xdg_surface = toplevel_window->shell_toplevel()
                        ->AsXDGToplevelWrapper()
                        ->xdg_surface_wrapper()
                        ->xdg_surface();
    } else if (auto* popup = window->AsWaylandPopup()) {
      xdg_surface = popup->shell_popup()
                        ->AsXDGPopupWrapper()
                        ->xdg_surface_wrapper()
                        ->xdg_surface();
    }
    bool screen_coordinates = window->IsScreenCoordinatesEnabled();
    if (force_use_screen_coordinates_once_) {
      screen_coordinates = true;
      force_use_screen_coordinates_once_ = false;
    }

    // If we can't use screen coordinates, we must have a surface so we can use
    // surface-local coordinates.
    DCHECK(screen_coordinates || xdg_surface);

    target_surface = screen_coordinates ? nullptr : xdg_surface;
    // Ignore `force_use_screen_coordinates_once_` for selecting which
    // coordinates to use. This is because the only difference between
    // `mouse_screen_location` and `mouse_surface_location` is that the former
    // is offset by the window's origin, while the latter isn't. If screen
    // coordinates aren't enabled, the window's origin should always be (0, 0),
    // and thus it shouldn't matter if we use `mouse_screen_location` or
    // `mouse_surface_location`. But as described in https://crbug.com/1454427,
    // the origin isn't actually (0, 0) until the first `xdg_toplevel.configure`
    // event with non-zero width and height is received, so we must use
    // `mouse_surface_location` even if `force_use_screen_coordinates_once_` is
    // true.
    target_location = window->IsScreenCoordinatesEnabled()
                          ? mouse_screen_location
                          : mouse_surface_location;
  }

  VLOG(1) << "Requesting pointer motion: location="
          << target_location.ToString();

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

  VLOG(1) << "Requesting pointer button: button=" << button
          << " button_state=" << button_state;

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

  VLOG(1) << "Requesting touch: location=" << touch_screen_location.ToString()
          << " action=" << action << " touch_id=" << touch_id;

  zcr_ui_controls_v1_send_touch(
      ui_controls_, action, touch_id, touch_screen_location.x(),
      touch_screen_location.y(), /*surface=*/nullptr, request_id);
  auto* wayland_proxy = wl::WaylandProxy::GetInstance();
  wayland_proxy->FlushForTesting();
}

#if BUILDFLAG(IS_CHROMEOS)
void WaylandInputEmulate::EmulateUpdateDisplay(const std::string& display_specs,
                                               uint32_t request_id) {
  VLOG(1) << "Updating display specs to: " << display_specs;
  if (zcr_ui_controls_v1_get_version(ui_controls_) >=
      ZCR_UI_CONTROLS_V1_DISPLAY_INFO_LIST_DONE_SINCE_VERSION) {
    const std::vector<display::Display>& existing_displays =
        display::Screen::GetScreen()->GetAllDisplays();
    auto info_list = display::CreateDisplayInfoListFromSpecs(
        display_specs, std::vector<display::Display>(), false);

    // Reuse existing display IDs on the client side, and let the server side
    // generate new IDs.
    size_t existing_display_index = 0;
    for (const auto& pending_display : info_list) {
      if (existing_display_index < existing_displays.size()) {
        auto id_pair = ui::wayland::ToWaylandDisplayIdPair(
            existing_displays[existing_display_index].id());
        zcr_ui_controls_v1_set_display_info_id(ui_controls_, id_pair.high,
                                               id_pair.low);
        ++existing_display_index;
      }

      zcr_ui_controls_v1_set_display_info_size(
          ui_controls_, pending_display.bounds_in_native().width(),
          pending_display.bounds_in_native().height());

      float device_scale_factor = pending_display.device_scale_factor();
      uint32_t scale_factor_value =
          *reinterpret_cast<const uint32_t*>(&device_scale_factor);
      zcr_ui_controls_v1_set_display_info_device_scale_factor(
          ui_controls_, scale_factor_value);
      zcr_ui_controls_v1_display_info_done(ui_controls_);
    }

    zcr_ui_controls_v1_display_info_list_done(ui_controls_, request_id);
    auto* wayland_proxy = wl::WaylandProxy::GetInstance();
    wayland_proxy->FlushForTesting();
  }
}
#endif

#if BUILDFLAG(IS_LINUX)
void WaylandInputEmulate::ForceUseScreenCoordinatesOnce() {
  force_use_screen_coordinates_once_ = true;
}
#endif

void WaylandInputEmulate::OnWindowConfigured(gfx::AcceleratedWidget widget,
                                             bool is_configured) {
  auto it = windows_.find(widget);
  CHECK(it != windows_.end());

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

  // Setup frame callback to know when the surface is finally ready to get
  // events. Otherwise, the width & height might not have been correctly set
  // before the mouse events are sent.
  static constexpr wl_callback_listener kFrameCallbackListener = {
      .done = &OnFrameDone};
  test_surface->frame_callback = wl_surface_frame(wlsurface);
  wl_callback_add_listener(test_surface->frame_callback,
                           &kFrameCallbackListener, this);

  wl_surface_commit(wlsurface);

  wayland_proxy->FlushForTesting();
}

void WaylandInputEmulate::OnWindowRoleAssigned(gfx::AcceleratedWidget widget) {
  auto it = windows_.find(widget);
  CHECK(it != windows_.end());

  // If a window has been assigned a popup role, then we must wait for a buffer
  // to be committed before any events can be processed.
  auto* test_surface = it->second.get();
  test_surface->waiting_for_buffer_commit = true;
}

void WaylandInputEmulate::OnWindowRemoved(gfx::AcceleratedWidget widget) {
  auto it = windows_.find(widget);
  CHECK(it != windows_.end());

  // Destroy the frame callback.
  if (it->second->frame_callback) {
    wl_callback_destroy(it->second->frame_callback);
    it->second->frame_callback = nullptr;
  }

  // Destroy the attached buffer.
  if (it->second->buffer) {
    auto* wayland_proxy = wl::WaylandProxy::GetInstance();
    wayland_proxy->DestroyShmForWlBuffer(it->second->buffer);
    wayland_proxy->FlushForTesting();
  }
  windows_.erase(it);
}

void WaylandInputEmulate::OnWindowAdded(gfx::AcceleratedWidget widget) {
  windows_.emplace(widget, std::make_unique<WaylandInputEmulate::TestWindow>());
}

// static
void WaylandInputEmulate::OnRequestProcessed(void* data,
                                             zcr_ui_controls_v1* ui_controls,
                                             uint32_t id) {
  auto* self = static_cast<WaylandInputEmulate*>(data);
  self->request_processed_callback_.Run(id);
}

// static
void WaylandInputEmulate::OnGlobal(void* data,
                                   wl_registry* registry,
                                   uint32_t name,
                                   const char* interface,
                                   uint32_t version) {
  auto* self = static_cast<WaylandInputEmulate*>(data);
  if (strcmp(interface, "zcr_ui_controls_v1") == 0 && version >= kMinVersion) {
    const wl_interface* wayland_interface =
        static_cast<const wl_interface*>(&zcr_ui_controls_v1_interface);
    self->ui_controls_ = static_cast<zcr_ui_controls_v1*>(
        wl_registry_bind(registry, name, wayland_interface, version));
  }
}

// static
void WaylandInputEmulate::OnGlobalRemove(void* data,
                                         wl_registry* registry,
                                         uint32_t name) {}

// static
void WaylandInputEmulate::OnFrameDone(void* data,
                                      wl_callback* callback,
                                      uint32_t time) {
  auto* self = static_cast<WaylandInputEmulate*>(data);
  CHECK(self)
      << "WaylandInputEmulate was destroyed before a frame callback arrived";

  WaylandInputEmulate::TestWindow* window = nullptr;
  for (const auto& window_item : self->windows_) {
    if (window_item.second->frame_callback == callback) {
      window = window_item.second.get();
      break;
    }
  }

  if (window) {
    wl_callback_destroy(window->frame_callback);
    window->frame_callback = nullptr;

    CHECK(!window->buffer_attached_and_configured);
    window->buffer_attached_and_configured = true;
    window->waiting_for_buffer_commit = false;
  }

  self->DispatchPendingRequests();
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

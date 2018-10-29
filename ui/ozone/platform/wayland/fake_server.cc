// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/fake_server.h"

#include <sys/socket.h>
#include <text-input-unstable-v1-server-protocol.h>
#include <wayland-server.h>
#include <xdg-shell-unstable-v5-server-protocol.h>
#include <xdg-shell-unstable-v6-server-protocol.h>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"

namespace wl {
namespace {

constexpr uint32_t kCompositorVersion = 4;
constexpr uint32_t kOutputVersion = 2;
constexpr uint32_t kDataDeviceManagerVersion = 3;
constexpr uint32_t kSeatVersion = 4;
constexpr uint32_t kTextInputManagerVersion = 1;
constexpr uint32_t kXdgShellVersion = 1;

bool ResourceHasImplementation(wl_resource* resource,
                               const wl_interface* interface,
                               const void* impl) {
  return wl_resource_instance_of(resource, interface, impl);
}

template <class T>
T* GetUserDataAs(wl_resource* resource) {
  return static_cast<T*>(wl_resource_get_user_data(resource));
}

template <class T>
std::unique_ptr<T> TakeUserDataAs(wl_resource* resource) {
  std::unique_ptr<T> user_data = base::WrapUnique(GetUserDataAs<T>(resource));
  // Make sure ServerObject doesn't try to destroy the resource twice.
  ServerObject::OnResourceDestroyed(resource);
  wl_resource_set_user_data(resource, nullptr);
  return user_data;
}

template <class T>
void DestroyUserData(wl_resource* resource) {
  TakeUserDataAs<T>(resource);
}

// TODO(msisov): Move all the callers to use this template implementation set
// helper with automatic user data destruction.
template <class T>
void SetImplementation(wl_resource* resource,
                       const void* implementation,
                       std::unique_ptr<T> user_data) {
  wl_resource_set_implementation(resource, implementation, user_data.release(),
                                 DestroyUserData<T>);
}

// Deprecated. Going to be removed.
template <class T>
void SetImplementation(wl_resource* resource,
                       const void* implementation,
                       T* user_data) {
  wl_resource_set_implementation(resource, implementation, user_data,
                                 &ServerObject::OnResourceDestroyed);
}

void DestroyResource(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void WriteDataOnWorkerThread(base::ScopedFD fd, const std::string& utf8_text) {
  if (!base::WriteFileDescriptor(fd.get(), utf8_text.data(), utf8_text.size()))
    LOG(ERROR) << "Failed to write selection data to clipboard.";
}

std::vector<uint8_t> ReadDataOnWorkerThread(base::ScopedFD fd) {
  constexpr size_t kChunkSize = 1024;
  std::vector<uint8_t> bytes;
  while (true) {
    uint8_t chunk[kChunkSize];
    ssize_t bytes_read = HANDLE_EINTR(read(fd.get(), chunk, kChunkSize));
    if (bytes_read > 0) {
      bytes.insert(bytes.end(), chunk, chunk + bytes_read);
      continue;
    }
    if (!bytes_read)
      return bytes;
    if (bytes_read < 0) {
      LOG(ERROR) << "Failed to read selection data from clipboard.";
      return std::vector<uint8_t>();
    }
  }
}

void CreatePipe(base::ScopedFD* read_pipe, base::ScopedFD* write_pipe) {
  int raw_pipe[2];
  PCHECK(0 == pipe(raw_pipe));
  read_pipe->reset(raw_pipe[0]);
  write_pipe->reset(raw_pipe[1]);
}

// wl_compositor

void CreateSurface(wl_client* client, wl_resource* resource, uint32_t id) {
  auto* compositor = GetUserDataAs<MockCompositor>(resource);
  wl_resource* surface_resource = wl_resource_create(
      client, &wl_surface_interface, wl_resource_get_version(resource), id);
  if (!surface_resource) {
    wl_client_post_no_memory(client);
    return;
  }
  compositor->AddSurface(std::make_unique<MockSurface>(surface_resource));
}

const struct wl_compositor_interface compositor_impl = {
    &CreateSurface,  // create_surface
    nullptr,         // create_region
};

// wl_surface

void Attach(wl_client* client,
            wl_resource* resource,
            wl_resource* buffer_resource,
            int32_t x,
            int32_t y) {
  GetUserDataAs<MockSurface>(resource)->Attach(buffer_resource, x, y);
}

void Damage(wl_client* client,
            wl_resource* resource,
            int32_t x,
            int32_t y,
            int32_t width,
            int32_t height) {
  GetUserDataAs<MockSurface>(resource)->Damage(x, y, width, height);
}

void Commit(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockSurface>(resource)->Commit();
}

const struct wl_surface_interface surface_impl = {
    &DestroyResource,  // destroy
    &Attach,           // attach
    &Damage,           // damage
    nullptr,           // frame
    nullptr,           // set_opaque_region
    nullptr,           // set_input_region
    &Commit,           // commit
    nullptr,           // set_buffer_transform
    nullptr,           // set_buffer_scale
    nullptr,           // damage_buffer
};

// xdg_shell

void UseUnstableVersion(wl_client* client,
                        wl_resource* resource,
                        int32_t version) {
  GetUserDataAs<MockXdgShell>(resource)->UseUnstableVersion(version);
}

// xdg_shell and zxdg_shell_v6

void Pong(wl_client* client, wl_resource* resource, uint32_t serial) {
  GetUserDataAs<MockXdgShell>(resource)->Pong(serial);
}

// xdg_shell

void GetXdgSurfaceV5(wl_client* client,
                     wl_resource* resource,
                     uint32_t id,
                     wl_resource* surface_resource);

void GetXdgPopupV5(struct wl_client* client,
                   struct wl_resource* resource,
                   uint32_t id,
                   struct wl_resource* surface,
                   struct wl_resource* parent,
                   struct wl_resource* seat,
                   uint32_t serial,
                   int32_t x,
                   int32_t y);

const struct xdg_shell_interface xdg_shell_impl = {
    &DestroyResource,     // destroy
    &UseUnstableVersion,  // use_unstable_version
    &GetXdgSurfaceV5,     // get_xdg_surface
    &GetXdgPopupV5,       // get_xdg_popup
    &Pong,                // pong
};

// zxdg_shell_v6

void GetXdgSurfaceV6(wl_client* client,
                     wl_resource* resource,
                     uint32_t id,
                     wl_resource* surface_resource);

void CreatePositioner(wl_client* client,
                      struct wl_resource* resource,
                      uint32_t id);

const struct zxdg_shell_v6_interface zxdg_shell_v6_impl = {
    &DestroyResource,   // destroy
    &CreatePositioner,  // create_positioner
    &GetXdgSurfaceV6,   // get_xdg_surface
    &Pong,              // pong
};

// zxdg_positioner_v6

void SetSize(struct wl_client* wl_client,
             struct wl_resource* resource,
             int32_t width,
             int32_t height) {
  if (width < 1 || height < 1) {
    wl_resource_post_error(resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                           "width and height must be positive and non-zero");
    return;
  }

  GetUserDataAs<MockPositioner>(resource)->set_size(gfx::Size(width, height));
}

void SetAnchorRect(struct wl_client* client,
                   struct wl_resource* resource,
                   int32_t x,
                   int32_t y,
                   int32_t width,
                   int32_t height) {
  if (width < 1 || height < 1) {
    wl_resource_post_error(resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                           "width and height must be positive and non-zero");
    return;
  }

  GetUserDataAs<MockPositioner>(resource)->set_anchor_rect(
      gfx::Rect(x, y, width, height));
}

void SetAnchor(struct wl_client* wl_client,
               struct wl_resource* resource,
               uint32_t anchor) {
  if (((anchor & ZXDG_POSITIONER_V6_ANCHOR_LEFT) &&
       (anchor & ZXDG_POSITIONER_V6_ANCHOR_RIGHT)) ||
      ((anchor & ZXDG_POSITIONER_V6_ANCHOR_TOP) &&
       (anchor & ZXDG_POSITIONER_V6_ANCHOR_BOTTOM))) {
    wl_resource_post_error(resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                           "same-axis values are not allowed");
    return;
  }

  GetUserDataAs<MockPositioner>(resource)->set_anchor(anchor);
}

void SetGravity(struct wl_client* client,
                struct wl_resource* resource,
                uint32_t gravity) {
  if (((gravity & ZXDG_POSITIONER_V6_GRAVITY_LEFT) &&
       (gravity & ZXDG_POSITIONER_V6_GRAVITY_RIGHT)) ||
      ((gravity & ZXDG_POSITIONER_V6_GRAVITY_TOP) &&
       (gravity & ZXDG_POSITIONER_V6_GRAVITY_BOTTOM))) {
    wl_resource_post_error(resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                           "same-axis values are not allowed");
    return;
  }

  GetUserDataAs<MockPositioner>(resource)->set_gravity(gravity);
}

const struct zxdg_positioner_v6_interface zxdg_positioner_v6_impl = {
    &DestroyResource,  // destroy
    &SetSize,          // set_size
    &SetAnchorRect,    // set_anchor_rect
    &SetAnchor,        // set_anchor
    &SetGravity,       // set_gravity
    nullptr,           // set_constraint_adjustment
    nullptr,           // set_offset
};

// wl_data_device

void DataDeviceStartDrag(wl_client* client,
                         wl_resource* resource,
                         wl_resource* source,
                         wl_resource* origin,
                         wl_resource* icon,
                         uint32_t serial) {
  NOTIMPLEMENTED();
}

void DataDeviceSetSelection(wl_client* client,
                            wl_resource* resource,
                            wl_resource* data_source,
                            uint32_t serial) {
  GetUserDataAs<MockDataDevice>(resource)->SetSelection(
      data_source ? GetUserDataAs<MockDataSource>(data_source) : nullptr,
      serial);
}

void DataDeviceRelease(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_data_device_interface data_device_impl = {
    &DataDeviceStartDrag, &DataDeviceSetSelection, &DataDeviceRelease};

// wl_data_device_manager

void CreateDataSource(wl_client* client, wl_resource* resource, uint32_t id) {
  wl_resource* data_source_resource = wl_resource_create(
      client, &wl_data_source_interface, wl_resource_get_version(resource), id);
  if (!data_source_resource) {
    wl_client_post_no_memory(client);
    return;
  }

  std::unique_ptr<MockDataSource> data_source(
      new MockDataSource(data_source_resource));

  auto* data_device_manager = GetUserDataAs<MockDataDeviceManager>(resource);
  data_device_manager->set_data_source(std::move(data_source));
}

void GetDataDevice(wl_client* client,
                   wl_resource* resource,
                   uint32_t id,
                   wl_resource* seat_resource) {
  wl_resource* data_device_resource = wl_resource_create(
      client, &wl_data_device_interface, wl_resource_get_version(resource), id);
  if (!data_device_resource) {
    wl_client_post_no_memory(client);
    return;
  }

  std::unique_ptr<MockDataDevice> data_device(
      new MockDataDevice(client, data_device_resource));

  auto* data_device_manager = GetUserDataAs<MockDataDeviceManager>(resource);
  data_device_manager->set_data_device(std::move(data_device));
}

const struct wl_data_device_manager_interface data_device_manager_impl = {
    &CreateDataSource, &GetDataDevice};

// wl_data_offer

void DataOfferAccept(wl_client* client,
                     wl_resource* resource,
                     uint32_t serial,
                     const char* mime_type) {
  NOTIMPLEMENTED();
}

void DataOfferReceive(wl_client* client,
                      wl_resource* resource,
                      const char* mime_type,
                      int fd) {
  GetUserDataAs<MockDataOffer>(resource)->Receive(mime_type,
                                                  base::ScopedFD(fd));
}

void DataOfferDestroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void DataOfferFinish(wl_client* client, wl_resource* resource) {
  NOTIMPLEMENTED();
}

void DataOfferSetActions(wl_client* client,
                         wl_resource* resource,
                         uint32_t dnd_actions,
                         uint32_t preferred_action) {
  NOTIMPLEMENTED();
}

const struct wl_data_offer_interface data_offer_impl = {
    DataOfferAccept, DataOfferReceive, DataOfferDestroy, DataOfferFinish,
    DataOfferSetActions};

// wl_data_source

void DataSourceOffer(wl_client* client,
                     wl_resource* resource,
                     const char* mime_type) {
  GetUserDataAs<MockDataSource>(resource)->Offer(mime_type);
}

void DataSourceDestroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void SetActions(wl_client* client,
                wl_resource* resource,
                uint32_t dnd_actions) {
  NOTIMPLEMENTED();
}

const struct wl_data_source_interface data_source_impl = {
    DataSourceOffer, DataSourceDestroy, SetActions};

// wl_seat

void GetPointer(wl_client* client, wl_resource* resource, uint32_t id) {
  wl_resource* pointer_resource = wl_resource_create(
      client, &wl_pointer_interface, wl_resource_get_version(resource), id);
  if (!pointer_resource) {
    wl_client_post_no_memory(client);
    return;
  }
  auto* seat = GetUserDataAs<MockSeat>(resource);
  seat->set_pointer(std::make_unique<MockPointer>(pointer_resource));
}

void GetKeyboard(wl_client* client, wl_resource* resource, uint32_t id) {
  wl_resource* keyboard_resource = wl_resource_create(
      client, &wl_keyboard_interface, wl_resource_get_version(resource), id);
  if (!keyboard_resource) {
    wl_client_post_no_memory(client);
    return;
  }
  auto* seat = GetUserDataAs<MockSeat>(resource);
  seat->set_keyboard(std::make_unique<MockKeyboard>(keyboard_resource));
}

void GetTouch(wl_client* client, wl_resource* resource, uint32_t id) {
  wl_resource* touch_resource = wl_resource_create(
      client, &wl_touch_interface, wl_resource_get_version(resource), id);
  if (!touch_resource) {
    wl_client_post_no_memory(client);
    return;
  }
  auto* seat = GetUserDataAs<MockSeat>(resource);
  seat->set_touch(std::make_unique<MockTouch>(touch_resource));
}

const struct wl_seat_interface seat_impl = {
    &GetPointer,       // get_pointer
    &GetKeyboard,      // get_keyboard
    &GetTouch,         // get_touch,
    &DestroyResource,  // release
};

// wl_keyboard

const struct wl_keyboard_interface keyboard_impl = {
    &DestroyResource,  // release
};

// wl_pointer

const struct wl_pointer_interface pointer_impl = {
    nullptr,           // set_cursor
    &DestroyResource,  // release
};

// wl_touch

const struct wl_touch_interface touch_impl = {
    &DestroyResource,  // release
};

// zwp_text_input_v1

void TextInputV1Activate(wl_client* client,
                         wl_resource* resource,
                         wl_resource* seat,
                         wl_resource* surface) {
  static_cast<MockZwpTextInput*>(wl_resource_get_user_data(resource))
      ->Activate(surface);
}

void TextInputV1Deactivate(wl_client* client,
                           wl_resource* resource,
                           wl_resource* seat) {
  static_cast<MockZwpTextInput*>(wl_resource_get_user_data(resource))
      ->Deactivate();
}

void TextInputV1ShowInputPanel(wl_client* client, wl_resource* resource) {
  static_cast<MockZwpTextInput*>(wl_resource_get_user_data(resource))
      ->ShowInputPanel();
}

void TextInputV1HideInputPanel(wl_client* client, wl_resource* resource) {
  static_cast<MockZwpTextInput*>(wl_resource_get_user_data(resource))
      ->HideInputPanel();
}

void TextInputV1Reset(wl_client* client, wl_resource* resource) {
  static_cast<MockZwpTextInput*>(wl_resource_get_user_data(resource))->Reset();
}

void TextInputV1SetCursorRectangle(wl_client* client,
                                   wl_resource* resource,
                                   int32_t x,
                                   int32_t y,
                                   int32_t width,
                                   int32_t height) {
  static_cast<MockZwpTextInput*>(wl_resource_get_user_data(resource))
      ->SetCursorRect(x, y, width, height);
}

const struct zwp_text_input_v1_interface zwp_text_input_v1_impl = {
    &TextInputV1Activate,            // activate
    &TextInputV1Deactivate,          // deactivate
    &TextInputV1ShowInputPanel,      // show_input_panel
    &TextInputV1HideInputPanel,      // hide_input_panel
    &TextInputV1Reset,               // reset
    nullptr,                         // set_surrounding_text
    nullptr,                         // set_content_type
    &TextInputV1SetCursorRectangle,  // set_cursor_rectangle
    nullptr,                         // set_preferred_language
    nullptr,                         // commit_state
    nullptr,                         // invoke_action
};

// zwp_text_input_manager_v1

void CreateTextInput(struct wl_client* client,
                     struct wl_resource* resource,
                     uint32_t id) {
  auto* im =
      static_cast<MockTextInputManagerV1*>(wl_resource_get_user_data(resource));
  wl_resource* text_resource =
      wl_resource_create(client, &zwp_text_input_v1_interface,
                         wl_resource_get_version(resource), id);
  if (!text_resource) {
    wl_client_post_no_memory(client);
    return;
  }
  im->text_input.reset(
      new MockZwpTextInput(text_resource, &zwp_text_input_v1_impl));
}

const struct zwp_text_input_manager_v1_interface
    zwp_text_input_manager_v1_impl = {
        &CreateTextInput,  // create_text_input
};

// xdg_surface, zxdg_surface_v6 and zxdg_toplevel shared methods.

void SetTitle(wl_client* client, wl_resource* resource, const char* title) {
  GetUserDataAs<MockXdgSurface>(resource)->SetTitle(title);
}

void SetAppId(wl_client* client, wl_resource* resource, const char* app_id) {
  GetUserDataAs<MockXdgSurface>(resource)->SetAppId(app_id);
}

void Move(wl_client* client,
          wl_resource* resource,
          wl_resource* seat,
          uint32_t serial) {
  GetUserDataAs<MockXdgSurface>(resource)->Move(serial);
}

void Resize(wl_client* client,
            wl_resource* resource,
            wl_resource* seat,
            uint32_t serial,
            uint32_t edges) {
  GetUserDataAs<MockXdgSurface>(resource)->Resize(serial, edges);
}

void AckConfigure(wl_client* client, wl_resource* resource, uint32_t serial) {
  GetUserDataAs<MockXdgSurface>(resource)->AckConfigure(serial);
}

void SetWindowGeometry(wl_client* client,
                       wl_resource* resource,
                       int32_t x,
                       int32_t y,
                       int32_t width,
                       int32_t height) {
  GetUserDataAs<MockXdgSurface>(resource)->SetWindowGeometry(x, y, width,
                                                             height);
}

void SetMaximized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockXdgSurface>(resource)->SetMaximized();
}

void UnsetMaximized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockXdgSurface>(resource)->UnsetMaximized();
}

void SetFullscreen(wl_client* client,
                   wl_resource* resource,
                   wl_resource* output) {
  GetUserDataAs<MockXdgSurface>(resource)->SetFullscreen();
}

void UnsetFullscreen(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockXdgSurface>(resource)->UnsetFullscreen();
}

void SetMinimized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockXdgSurface>(resource)->SetMinimized();
}

const struct xdg_surface_interface xdg_surface_impl = {
    &DestroyResource,    // destroy
    nullptr,             // set_parent
    &SetTitle,           // set_title
    &SetAppId,           // set_app_id
    nullptr,             // show_window_menu
    &Move,               // move
    &Resize,             // resize
    &AckConfigure,       // ack_configure
    &SetWindowGeometry,  // set_window_geometry
    &SetMaximized,       // set_maximized
    &UnsetMaximized,     // set_unmaximized
    &SetFullscreen,      // set_fullscreen
    &UnsetFullscreen,    // unset_fullscreen
    &SetMinimized,       // set_minimized
};

// xdg_popup_v5

const struct xdg_popup_interface xdg_popup_impl = {
    &DestroyResource,  // destroy
};

// zxdg_popup_v6

void Grab(struct wl_client* client,
          struct wl_resource* resource,
          struct wl_resource* seat,
          uint32_t serial) {
  GetUserDataAs<MockXdgPopup>(resource)->Grab(serial);
}

const struct zxdg_popup_v6_interface zxdg_popup_v6_impl = {
    &DestroyResource,  // destroy
    &Grab,             // grab
};

// zxdg_surface specific interface

void GetTopLevel(wl_client* client, wl_resource* resource, uint32_t id) {
  auto* surface = GetUserDataAs<MockXdgSurface>(resource);
  if (surface->xdg_toplevel()) {
    wl_resource_post_error(resource, ZXDG_SURFACE_V6_ERROR_ALREADY_CONSTRUCTED,
                           "surface has already been constructed");
    return;
  }
  wl_resource* xdg_toplevel_resource =
      wl_resource_create(client, &zxdg_toplevel_v6_interface, 1, id);
  if (!xdg_toplevel_resource) {
    wl_client_post_no_memory(client);
    return;
  }
  surface->set_xdg_toplevel(
      std::make_unique<MockXdgTopLevel>(xdg_toplevel_resource));
}

void GetZXdgPopupV6(struct wl_client* client,
                    struct wl_resource* resource,
                    uint32_t id,
                    struct wl_resource* parent,
                    struct wl_resource* positioner) {
  auto* mock_xdg_surface = GetUserDataAs<MockXdgSurface>(resource);
  wl_resource* current_resource = mock_xdg_surface->resource();
  if (current_resource &&
      (ResourceHasImplementation(current_resource, &zxdg_popup_v6_interface,
                                 &zxdg_popup_v6_impl) ||
       ResourceHasImplementation(current_resource,
                                 &zxdg_positioner_v6_interface,
                                 &zxdg_positioner_v6_impl))) {
    wl_resource_post_error(resource, ZXDG_SURFACE_V6_ERROR_ALREADY_CONSTRUCTED,
                           "surface has already been constructed");
    return;
  }

  auto* mock_positioner = GetUserDataAs<MockPositioner>(positioner);
  if (mock_positioner->size().width() == 0 ||
      mock_positioner->anchor_rect().width() == 0) {
    wl_resource_post_error(resource, ZXDG_SHELL_V6_ERROR_INVALID_POSITIONER,
                           "Positioner object is not complete");
    return;
  }

  wl_resource* popup_resource = wl_resource_create(
      client, &zxdg_popup_v6_interface, wl_resource_get_version(resource), id);
  if (!popup_resource) {
    wl_client_post_no_memory(client);
    return;
  }

  SetImplementation(
      popup_resource, &zxdg_popup_v6_impl,
      std::make_unique<MockXdgPopup>(popup_resource, &zxdg_popup_v6_impl));
}

const struct zxdg_surface_v6_interface zxdg_surface_v6_impl = {
    &DestroyResource,  // destroy
    &GetTopLevel,      // get_toplevel
    &GetZXdgPopupV6,   // get_popup

    &SetWindowGeometry,  // set_window_geometry
    &AckConfigure,       // ack_configure
};

const struct zxdg_toplevel_v6_interface zxdg_toplevel_v6_impl = {
    &DestroyResource,  // destroy
    nullptr,           // set_parent
    &SetTitle,         // set_title
    &SetAppId,         // set_app_id
    nullptr,           // show_window_menu
    &Move,             // move
    &Resize,           // resize
    nullptr,           // set_max_size
    nullptr,           // set_min_size
    &SetMaximized,     // set_maximized
    &UnsetMaximized,   // set_unmaximized
    &SetFullscreen,    // set_fullscreen
    &UnsetFullscreen,  // unset_fullscreen
    &SetMinimized,     // set_minimized
};

void GetXdgSurfaceImpl(wl_client* client,
                       wl_resource* resource,
                       uint32_t id,
                       wl_resource* surface_resource,
                       const struct wl_interface* interface,
                       const void* implementation) {
  auto* surface = GetUserDataAs<MockSurface>(surface_resource);
  if (surface->xdg_surface()) {
    uint32_t xdg_error = implementation == &xdg_surface_impl
                             ? XDG_SHELL_ERROR_ROLE
                             : ZXDG_SHELL_V6_ERROR_ROLE;
    wl_resource_post_error(resource, xdg_error, "surface already has a role");
    return;
  }
  wl_resource* xdg_surface_resource = wl_resource_create(
      client, interface, wl_resource_get_version(resource), id);

  if (!xdg_surface_resource) {
    wl_client_post_no_memory(client);
    return;
  }
  surface->set_xdg_surface(
      std::make_unique<MockXdgSurface>(xdg_surface_resource, implementation));
}

// xdg_shell

void GetXdgSurfaceV5(wl_client* client,
                     wl_resource* resource,
                     uint32_t id,
                     wl_resource* surface_resource) {
  GetXdgSurfaceImpl(client, resource, id, surface_resource,
                    &xdg_surface_interface, &xdg_surface_impl);
}

void GetXdgPopupV5(struct wl_client* client,
                   struct wl_resource* resource,
                   uint32_t id,
                   struct wl_resource* surface,
                   struct wl_resource* parent,
                   struct wl_resource* seat,
                   uint32_t serial,
                   int32_t x,
                   int32_t y) {
  auto* mock_surface = GetUserDataAs<MockSurface>(surface);
  if (mock_surface->resource() &&
      ResourceHasImplementation(mock_surface->resource(), &xdg_popup_interface,
                                &xdg_popup_impl)) {
    wl_resource_post_error(resource, XDG_SHELL_ERROR_ROLE,
                           "surface has already assigned a role");
    return;
  }

  wl_resource* popup_resource = wl_resource_create(
      client, &xdg_popup_interface, wl_resource_get_version(resource), id);
  if (!popup_resource) {
    wl_client_post_no_memory(client);
    return;
  }

  SetImplementation(
      popup_resource, &xdg_popup_impl,
      std::make_unique<MockXdgPopup>(popup_resource, &xdg_popup_impl));
}

// zxdg_shell_v6

void GetXdgSurfaceV6(wl_client* client,
                     wl_resource* resource,
                     uint32_t id,
                     wl_resource* surface_resource) {
  GetXdgSurfaceImpl(client, resource, id, surface_resource,
                    &zxdg_surface_v6_interface, &zxdg_surface_v6_impl);
}

void CreatePositioner(wl_client* client,
                      struct wl_resource* resource,
                      uint32_t id) {
  wl_resource* positioner_resource =
      wl_resource_create(client, &zxdg_positioner_v6_interface,
                         wl_resource_get_version(resource), id);
  if (!positioner_resource) {
    wl_client_post_no_memory(client);
    return;
  }

  SetImplementation(positioner_resource, &zxdg_positioner_v6_impl,
                    std::make_unique<MockPositioner>(positioner_resource));
}

}  // namespace

ServerObject::ServerObject(wl_resource* resource) : resource_(resource) {}

ServerObject::~ServerObject() {
  if (resource_)
    wl_resource_destroy(resource_);
}

// static
void ServerObject::OnResourceDestroyed(wl_resource* resource) {
  auto* obj = GetUserDataAs<ServerObject>(resource);
  obj->resource_ = nullptr;
}

MockXdgSurface::MockXdgSurface(wl_resource* resource,
                               const void* implementation)
    : ServerObject(resource) {
  SetImplementation(resource, implementation, this);
}

MockXdgSurface::~MockXdgSurface() {}

MockXdgTopLevel::MockXdgTopLevel(wl_resource* resource)
    : MockXdgSurface(resource, &zxdg_surface_v6_impl) {
  SetImplementation(resource, &zxdg_toplevel_v6_impl, this);
}

MockXdgTopLevel::~MockXdgTopLevel() {}

MockPositioner::MockPositioner(wl_resource* resource)
    : ServerObject(resource) {}

MockPositioner::~MockPositioner() {}

MockXdgPopup::MockXdgPopup(wl_resource* resource, const void* implementation)
    : ServerObject(resource) {}

MockXdgPopup::~MockXdgPopup() {}

MockSurface::MockSurface(wl_resource* resource) : ServerObject(resource) {
  SetImplementation(resource, &surface_impl, this);
}

MockSurface::~MockSurface() {
  if (xdg_surface_ && xdg_surface_->resource())
    wl_resource_destroy(xdg_surface_->resource());
}

MockSurface* MockSurface::FromResource(wl_resource* resource) {
  if (!wl_resource_instance_of(resource, &wl_surface_interface, &surface_impl))
    return nullptr;
  return GetUserDataAs<MockSurface>(resource);
}

MockPointer::MockPointer(wl_resource* resource) : ServerObject(resource) {
  SetImplementation(resource, &pointer_impl, this);
}

MockPointer::~MockPointer() {}

MockKeyboard::MockKeyboard(wl_resource* resource) : ServerObject(resource) {
  SetImplementation(resource, &keyboard_impl, this);
}

MockKeyboard::~MockKeyboard() {}

MockTouch::MockTouch(wl_resource* resource) : ServerObject(resource) {
  SetImplementation(resource, &touch_impl, this);
}

MockTouch::~MockTouch() {}

MockZwpTextInput::MockZwpTextInput(wl_resource* resource,
                                   const void* implementation)
    : ServerObject(resource) {
  wl_resource_set_implementation(resource, implementation, this,
                                 &ServerObject::OnResourceDestroyed);
}

MockZwpTextInput::~MockZwpTextInput() {}

MockDataOffer::MockDataOffer(wl_resource* resource)
    : ServerObject(resource),
      io_thread_("Worker thread"),
      write_data_weak_ptr_factory_(this) {
  SetImplementation(resource, &data_offer_impl, this);

  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread_.StartWithOptions(options);
}

MockDataOffer::~MockDataOffer() {}

void MockDataOffer::Receive(const std::string& mime_type, base::ScopedFD fd) {
  DCHECK(fd.is_valid());
  std::string text_data;
  if (mime_type == kTextMimeTypeUtf8)
    text_data = kSampleClipboardText;
  else if (mime_type == kTextMimeTypeText)
    text_data = kSampleTextForDragAndDrop;

  io_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WriteDataOnWorkerThread, std::move(fd), text_data));
}

void MockDataOffer::OnOffer(const std::string& mime_type) {
  wl_data_offer_send_offer(resource(), mime_type.c_str());
}

MockDataDevice::MockDataDevice(wl_client* client, wl_resource* resource)
    : ServerObject(resource), client_(client) {
  SetImplementation(resource, &data_device_impl, this);
}

MockDataDevice::~MockDataDevice() {}

void MockDataDevice::SetSelection(MockDataSource* data_source,
                                  uint32_t serial) {
  NOTIMPLEMENTED();
}

MockDataOffer* MockDataDevice::OnDataOffer() {
  wl_resource* data_offer_resource =
      wl_resource_create(client_, &wl_data_offer_interface,
                         wl_resource_get_version(resource()), 0);
  data_offer_.reset(new MockDataOffer(data_offer_resource));
  wl_data_device_send_data_offer(resource(), data_offer_resource);

  return GetUserDataAs<MockDataOffer>(data_offer_resource);
}

void MockDataDevice::OnEnter(uint32_t serial,
                             wl_resource* surface,
                             wl_fixed_t x,
                             wl_fixed_t y,
                             MockDataOffer& data_offer) {
  wl_data_device_send_enter(resource(), serial, surface, x, y,
                            data_offer.resource());
}

void MockDataDevice::OnLeave() {
  wl_data_device_send_leave(resource());
}

void MockDataDevice::OnMotion(uint32_t time, wl_fixed_t x, wl_fixed_t y) {
  wl_data_device_send_motion(resource(), time, x, y);
}

void MockDataDevice::OnDrop() {
  wl_data_device_send_drop(resource());
}

void MockDataDevice::OnSelection(MockDataOffer& data_offer) {
  wl_data_device_send_selection(resource(), data_offer.resource());
}

MockDataSource::MockDataSource(wl_resource* resource)
    : ServerObject(resource),
      io_thread_("Worker thread"),
      read_data_weak_ptr_factory_(this) {
  SetImplementation(resource, &data_source_impl, this);

  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread_.StartWithOptions(options);
}

MockDataSource::~MockDataSource() {}

void MockDataSource::Offer(const std::string& mime_type) {
  NOTIMPLEMENTED();
}

void MockDataSource::ReadData(ReadDataCallback callback) {
  base::ScopedFD read_fd;
  base::ScopedFD write_fd;
  CreatePipe(&read_fd, &write_fd);

  wl_data_source_send_send(resource(), kTextMimeTypeUtf8, write_fd.get());

  base::PostTaskAndReplyWithResult(
      io_thread_.task_runner().get(), FROM_HERE,
      base::BindOnce(&ReadDataOnWorkerThread, std::move(read_fd)),
      base::BindOnce(&MockDataSource::DataReadCb,
                     read_data_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void MockDataSource::DataReadCb(ReadDataCallback callback,
                                const std::vector<uint8_t>& data) {
  std::move(callback).Run(data);
}

void MockDataSource::OnCancelled() {
  wl_data_source_send_cancelled(resource());
}

void GlobalDeleter::operator()(wl_global* global) {
  wl_global_destroy(global);
}

Global::Global(const wl_interface* interface,
               const void* implementation,
               uint32_t version)
    : interface_(interface),
      implementation_(implementation),
      version_(version) {}

Global::~Global() {}

bool Global::Initialize(wl_display* display) {
  global_.reset(wl_global_create(display, interface_, version_, this, &Bind));
  return global_ != nullptr;
}

// static
void Global::Bind(wl_client* client,
                  void* data,
                  uint32_t version,
                  uint32_t id) {
  auto* global = static_cast<Global*>(data);
  wl_resource* resource = wl_resource_create(
      client, global->interface_, std::min(version, global->version_), id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  if (!global->resource_)
    global->resource_ = resource;
  SetImplementation(resource, global->implementation_, global);
  global->OnBind();
}

// static
void Global::OnResourceDestroyed(wl_resource* resource) {
  auto* global = GetUserDataAs<Global>(resource);
  if (global->resource_ == resource)
    global->resource_ = nullptr;
}

MockCompositor::MockCompositor()
    : Global(&wl_compositor_interface, &compositor_impl, kCompositorVersion) {}

MockCompositor::~MockCompositor() {}

void MockCompositor::AddSurface(std::unique_ptr<MockSurface> surface) {
  surfaces_.push_back(std::move(surface));
}

MockDataDeviceManager::MockDataDeviceManager()
    : Global(&wl_data_device_manager_interface,
             &data_device_manager_impl,
             kDataDeviceManagerVersion) {}

MockDataDeviceManager::~MockDataDeviceManager() {}

MockOutput::MockOutput()
    : Global(&wl_output_interface, nullptr, kOutputVersion),
      rect_(gfx::Rect(0, 0, 800, 600)) {}

MockOutput::~MockOutput() {}

// Notify clients of the change for output position.
void MockOutput::OnBind() {
  const char* kUnknownMake = "unknown";
  const char* kUnknownModel = "unknown";
  wl_output_send_geometry(resource(), rect_.x(), rect_.y(), 0, 0, 0,
                          kUnknownMake, kUnknownModel, 0);
  wl_output_send_mode(resource(), WL_OUTPUT_MODE_CURRENT, rect_.width(),
                      rect_.height(), 0);
}

MockSeat::MockSeat() : Global(&wl_seat_interface, &seat_impl, kSeatVersion) {}

MockSeat::~MockSeat() {}

MockXdgShell::MockXdgShell()
    : Global(&xdg_shell_interface, &xdg_shell_impl, kXdgShellVersion) {}

MockXdgShell::~MockXdgShell() {}

MockXdgShellV6::MockXdgShellV6()
    : Global(&zxdg_shell_v6_interface, &zxdg_shell_v6_impl, kXdgShellVersion) {}

MockXdgShellV6::~MockXdgShellV6() {}

MockTextInputManagerV1::MockTextInputManagerV1()
    : Global(&zwp_text_input_manager_v1_interface,
             &zwp_text_input_manager_v1_impl,
             kTextInputManagerVersion) {}

MockTextInputManagerV1::~MockTextInputManagerV1() {}

void DisplayDeleter::operator()(wl_display* display) {
  wl_display_destroy(display);
}

FakeServer::FakeServer()
    : Thread("fake_wayland_server"),
      pause_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                   base::WaitableEvent::InitialState::NOT_SIGNALED),
      resume_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                    base::WaitableEvent::InitialState::NOT_SIGNALED),
      controller_(FROM_HERE) {}

FakeServer::~FakeServer() {
  Resume();
  Stop();
}

bool FakeServer::Start(uint32_t shell_version) {
  display_.reset(wl_display_create());
  if (!display_)
    return false;
  event_loop_ = wl_display_get_event_loop(display_.get());

  int fd[2];
  if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd) < 0)
    return false;
  base::ScopedFD server_fd(fd[0]);
  base::ScopedFD client_fd(fd[1]);

  if (wl_display_init_shm(display_.get()) < 0)
    return false;
  if (!compositor_.Initialize(display_.get()))
    return false;
  if (!output_.Initialize(display_.get()))
    return false;
  if (!data_device_manager_.Initialize(display_.get()))
    return false;
  if (!seat_.Initialize(display_.get()))
    return false;
  if (shell_version == 5) {
    if (!xdg_shell_.Initialize(display_.get()))
      return false;
  } else {
    if (!zxdg_shell_v6_.Initialize(display_.get()))
      return false;
  }
  if (!zwp_text_input_manager_v1_.Initialize(display_.get()))
    return false;

  client_ = wl_client_create(display_.get(), server_fd.get());
  if (!client_)
    return false;
  (void)server_fd.release();

  base::Thread::Options options;
  options.message_pump_factory = base::BindRepeating(
      &FakeServer::CreateMessagePump, base::Unretained(this));
  if (!base::Thread::StartWithOptions(options))
    return false;

  setenv("WAYLAND_SOCKET", base::UintToString(client_fd.release()).c_str(), 1);

  return true;
}

void FakeServer::Pause() {
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&FakeServer::DoPause, base::Unretained(this)));
  pause_event_.Wait();
}

void FakeServer::Resume() {
  if (display_)
    wl_display_flush_clients(display_.get());
  resume_event_.Signal();
}

void FakeServer::DoPause() {
  base::RunLoop().RunUntilIdle();
  pause_event_.Signal();
  resume_event_.Wait();
}

std::unique_ptr<base::MessagePump> FakeServer::CreateMessagePump() {
  auto pump = base::WrapUnique(new base::MessagePumpLibevent);
  pump->WatchFileDescriptor(wl_event_loop_get_fd(event_loop_), true,
                            base::MessagePumpLibevent::WATCH_READ, &controller_,
                            this);
  return std::move(pump);
}

void FakeServer::OnFileCanReadWithoutBlocking(int fd) {
  wl_event_loop_dispatch(event_loop_, 0);
  wl_display_flush_clients(display_.get());
}

void FakeServer::OnFileCanWriteWithoutBlocking(int fd) {}

}  // namespace wl

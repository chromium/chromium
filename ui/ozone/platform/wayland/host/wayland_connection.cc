// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_connection.h"

#include <xdg-shell-client-protocol.h>
#include <xdg-shell-unstable-v6-client-protocol.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/task/current_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/gtk_primary_selection_device_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_clipboard.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_position.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_data_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_drm.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_input_method_context.h"
#include "ui/ozone/platform/wayland/host/wayland_keyboard.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_shm.h"
#include "ui/ozone/platform/wayland/host/wayland_touch.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_linux_dmabuf.h"
#include "ui/ozone/platform/wayland/host/xdg_foreign_wrapper.h"

#if defined(USE_LIBWAYLAND_STUBS)
#include <dlfcn.h>

#include "third_party/wayland/libwayland_stubs.h"  // nogncheck
#endif

namespace ui {

namespace {
constexpr uint32_t kMaxCompositorVersion = 4;
constexpr uint32_t kMaxGtkPrimarySelectionDeviceManagerVersion = 1;
constexpr uint32_t kMaxKeyboardExtensionVersion = 1;
constexpr uint32_t kMaxLinuxDmabufVersion = 3;
constexpr uint32_t kMaxSeatVersion = 5;
constexpr uint32_t kMaxShmVersion = 1;
constexpr uint32_t kMaxXdgShellVersion = 1;
constexpr uint32_t kMaxDeviceManagerVersion = 3;
constexpr uint32_t kMaxWpPresentationVersion = 1;
constexpr uint32_t kMaxWpViewporterVersion = 1;
constexpr uint32_t kMaxTextInputManagerVersion = 1;
constexpr uint32_t kMaxExplicitSyncVersion = 2;
constexpr uint32_t kMinAuraShellVersion = 10;
constexpr uint32_t kMinWlDrmVersion = 2;
constexpr uint32_t kMinWlOutputVersion = 2;
constexpr uint32_t kMaxXdgDecorationVersion = 1;
}  // namespace

WaylandConnection::WaylandConnection() = default;

WaylandConnection::~WaylandConnection() = default;

bool WaylandConnection::Initialize() {
#if defined(USE_LIBWAYLAND_STUBS)
  // Use RTLD_NOW to load all symbols, since the stubs will try to load all of
  // them anyway.  Use RTLD_GLOBAL to add the symbols to the global namespace.
  auto dlopen_flags = RTLD_NOW | RTLD_GLOBAL;
  if (void* libwayland_client =
          dlopen("libwayland-client.so.0", dlopen_flags)) {
    third_party_wayland::InitializeLibwaylandclient(libwayland_client);
  } else {
    LOG(ERROR) << "Failed to load wayland client libraries.";
    return false;
  }
  if (void* libwayland_egl = dlopen("libwayland-egl.so.1", dlopen_flags))
    third_party_wayland::InitializeLibwaylandegl(libwayland_egl);
#endif

  static const wl_registry_listener registry_listener = {
      &WaylandConnection::Global,
      &WaylandConnection::GlobalRemove,
  };

  display_.reset(wl_display_connect(nullptr));
  if (!display_) {
    LOG(ERROR) << "Failed to connect to Wayland display";
    return false;
  }

  registry_.reset(wl_display_get_registry(display_.get()));
  if (!registry_) {
    LOG(ERROR) << "Failed to get Wayland registry";
    return false;
  }

  // Now that the connection with the display server has been properly
  // estabilished, initialize the event source and input objects.
  DCHECK(!event_source_);
  event_source_ =
      std::make_unique<WaylandEventSource>(display(), wayland_window_manager());

  wl_registry_add_listener(registry_.get(), &registry_listener, this);
  while (!wayland_output_manager_ ||
         !wayland_output_manager_->IsOutputReady()) {
    wl_display_roundtrip(display_.get());
  }

  buffer_manager_host_ = std::make_unique<WaylandBufferManagerHost>(this);

  if (!compositor_) {
    LOG(ERROR) << "No wl_compositor object";
    return false;
  }
  if (!shm_) {
    LOG(ERROR) << "No wl_shm object";
    return false;
  }
  if (!shell_v6_ && !shell_) {
    LOG(ERROR) << "No Wayland shell found";
    return false;
  }

  // When we are running tests with weston in headless mode, the seat is not
  // announced.
  if (!seat_)
    LOG(WARNING) << "No wl_seat object. The functionality may suffer.";

  return true;
}

void WaylandConnection::ScheduleFlush() {
  // When we are in tests, the message loop is set later when the
  // initialization of the OzonePlatform complete. Thus, just
  // flush directly. This doesn't happen in normal run.
  if (!base::CurrentUIThread::IsSet()) {
    Flush();
  } else if (!scheduled_flush_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandConnection::Flush, base::Unretained(this)));
    scheduled_flush_ = true;
  }
}

void WaylandConnection::SetShutdownCb(base::OnceCallback<void()> shutdown_cb) {
  event_source()->SetShutdownCb(std::move(shutdown_cb));
}

void WaylandConnection::SetCursorBitmap(const std::vector<SkBitmap>& bitmaps,
                                        const gfx::Point& location) {
  if (!cursor_)
    return;
  cursor_->UpdateBitmap(bitmaps, location, serial());
}

bool WaylandConnection::IsDragInProgress() const {
  // |data_drag_controller_| can be null when running on headless weston.
  return data_drag_controller_ && data_drag_controller_->state() !=
                                      WaylandDataDragController::State::kIdle;
}

wl::Object<wl_surface> WaylandConnection::CreateSurface() {
  DCHECK(compositor_);
  return wl::Object<wl_surface>(
      wl_compositor_create_surface(compositor_.get()));
}

void WaylandConnection::Flush() {
  wl_display_flush(display_.get());
  scheduled_flush_ = false;
}

void WaylandConnection::UpdateInputDevices(wl_seat* seat,
                                           uint32_t capabilities) {
  DCHECK(seat);
  DCHECK(event_source_);
  auto has_pointer = capabilities & WL_SEAT_CAPABILITY_POINTER;
  auto has_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;
  auto has_touch = capabilities & WL_SEAT_CAPABILITY_TOUCH;

  if (!has_pointer) {
    pointer_.reset();
    cursor_.reset();
    wayland_cursor_position_.reset();
  } else if (!pointer_) {
    if (wl_pointer* pointer = wl_seat_get_pointer(seat)) {
      pointer_ =
          std::make_unique<WaylandPointer>(pointer, this, event_source());
      cursor_ = std::make_unique<WaylandCursor>(pointer_.get(), this);
      wayland_cursor_position_ = std::make_unique<WaylandCursorPosition>();
    } else {
      LOG(ERROR) << "Failed to get wl_pointer from seat";
    }
  }

  if (!has_keyboard) {
    keyboard_.reset();
  } else if (!keyboard_) {
    if (!CreateKeyboard()) {
      LOG(ERROR) << "Failed to create WaylandKeyboard";
    }
  }

  if (!has_touch) {
    touch_.reset();
  } else if (!touch_) {
    if (wl_touch* touch = wl_seat_get_touch(seat)) {
      touch_ = std::make_unique<WaylandTouch>(touch, this, event_source());
    } else {
      LOG(ERROR) << "Failed to get wl_touch from seat";
    }
  }
}

bool WaylandConnection::CreateKeyboard() {
  wl_keyboard* keyboard = wl_seat_get_keyboard(seat_.get());
  if (!keyboard)
    return false;

  auto* layout_engine = KeyboardLayoutEngineManager::GetKeyboardLayoutEngine();
  // Make sure to destroy the old WaylandKeyboard (if it exists) before creating
  // the new one.
  keyboard_.reset();
  keyboard_.reset(new WaylandKeyboard(keyboard, keyboard_extension_v1_.get(),
                                      this, layout_engine, event_source()));
  return true;
}

void WaylandConnection::CreateDataObjectsIfReady() {
  if (data_device_manager_ && seat_) {
    DCHECK(!data_drag_controller_);
    data_drag_controller_ = std::make_unique<WaylandDataDragController>(
        this, data_device_manager_.get());

    DCHECK(!window_drag_controller_);
    window_drag_controller_ = std::make_unique<WaylandWindowDragController>(
        this, data_device_manager_.get(), event_source());

    DCHECK(!clipboard_);
    clipboard_ =
        std::make_unique<WaylandClipboard>(this, data_device_manager_.get());
  }
}

// static
void WaylandConnection::Global(void* data,
                               wl_registry* registry,
                               uint32_t name,
                               const char* interface,
                               uint32_t version) {
  static const wl_seat_listener seat_listener = {
      &WaylandConnection::Capabilities,
      &WaylandConnection::Name,
  };
  static const xdg_wm_base_listener shell_listener = {
      &WaylandConnection::Ping,
  };
  static const zxdg_shell_v6_listener shell_v6_listener = {
      &WaylandConnection::PingV6,
  };

  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  if (!connection->compositor_ && strcmp(interface, "wl_compositor") == 0) {
    connection->compositor_ = wl::Bind<wl_compositor>(
        registry, name, std::min(version, kMaxCompositorVersion));
    connection->compositor_version_ = version;
    if (!connection->compositor_)
      LOG(ERROR) << "Failed to bind to wl_compositor global";
  } else if (!connection->subcompositor_ &&
             strcmp(interface, "wl_subcompositor") == 0) {
    connection->subcompositor_ = wl::Bind<wl_subcompositor>(registry, name, 1);
    if (!connection->subcompositor_)
      LOG(ERROR) << "Failed to bind to wl_subcompositor global";
  } else if (!connection->shm_ && strcmp(interface, "wl_shm") == 0) {
    wl::Object<wl_shm> shm =
        wl::Bind<wl_shm>(registry, name, std::min(version, kMaxShmVersion));
    connection->shm_ = std::make_unique<WaylandShm>(shm.release(), connection);
    if (!connection->shm_)
      LOG(ERROR) << "Failed to bind to wl_shm global";
  } else if (!connection->seat_ && strcmp(interface, "wl_seat") == 0) {
    connection->seat_ =
        wl::Bind<wl_seat>(registry, name, std::min(version, kMaxSeatVersion));
    if (!connection->seat_) {
      LOG(ERROR) << "Failed to bind to wl_seat global";
      return;
    }
    wl_seat_add_listener(connection->seat_.get(), &seat_listener, connection);
    connection->CreateDataObjectsIfReady();
  } else if (!connection->shell_v6_ &&
             strcmp(interface, "zxdg_shell_v6") == 0) {
    // Check for zxdg_shell_v6 first.
    connection->shell_v6_ = wl::Bind<zxdg_shell_v6>(
        registry, name, std::min(version, kMaxXdgShellVersion));
    if (!connection->shell_v6_) {
      LOG(ERROR) << "Failed to bind to zxdg_shell_v6 global";
      return;
    }
    zxdg_shell_v6_add_listener(connection->shell_v6_.get(), &shell_v6_listener,
                               connection);
  } else if (!connection->shell_ && strcmp(interface, "xdg_wm_base") == 0) {
    connection->shell_ = wl::Bind<xdg_wm_base>(
        registry, name, std::min(version, kMaxXdgShellVersion));
    if (!connection->shell_) {
      LOG(ERROR) << "Failed to bind to xdg_wm_base global";
      return;
    }
    xdg_wm_base_add_listener(connection->shell_.get(), &shell_listener,
                             connection);
  } else if (base::EqualsCaseInsensitiveASCII(interface, "wl_output")) {
    if (version < kMinWlOutputVersion) {
      LOG(ERROR)
          << "Unable to bind to the unsupported wl_output object with version= "
          << version << ". Minimum supported version is "
          << kMinWlOutputVersion;
      return;
    }

    wl::Object<wl_output> output = wl::Bind<wl_output>(registry, name, version);
    if (!output) {
      LOG(ERROR) << "Failed to bind to wl_output global";
      return;
    }

    if (!connection->wayland_output_manager_) {
      connection->wayland_output_manager_ =
          std::make_unique<WaylandOutputManager>();
    }
    connection->wayland_output_manager_->AddWaylandOutput(name,
                                                          output.release());
  } else if (!connection->data_device_manager_ &&
             strcmp(interface, "wl_data_device_manager") == 0) {
    wl::Object<wl_data_device_manager> data_device_manager =
        wl::Bind<wl_data_device_manager>(
            registry, name, std::min(version, kMaxDeviceManagerVersion));
    if (!data_device_manager) {
      LOG(ERROR) << "Failed to bind to wl_data_device_manager global";
      return;
    }
    connection->data_device_manager_ =
        std::make_unique<WaylandDataDeviceManager>(
            data_device_manager.release(), connection);
    connection->CreateDataObjectsIfReady();
  } else if (!connection->primary_selection_device_manager_ &&
             strcmp(interface, "gtk_primary_selection_device_manager") == 0) {
    wl::Object<gtk_primary_selection_device_manager> manager =
        wl::Bind<gtk_primary_selection_device_manager>(
            registry, name, kMaxGtkPrimarySelectionDeviceManagerVersion);
    connection->primary_selection_device_manager_ =
        std::make_unique<GtkPrimarySelectionDeviceManager>(manager.release(),
                                                           connection);
  } else if (!connection->linux_explicit_synchronization_ &&
             (strcmp(interface, "zwp_linux_explicit_synchronization_v1") ==
              0)) {
    connection->linux_explicit_synchronization_ =
        wl::Bind<zwp_linux_explicit_synchronization_v1>(
            registry, name, std::min(version, kMaxExplicitSyncVersion));
  } else if (!connection->zwp_dmabuf_ &&
             (strcmp(interface, "zwp_linux_dmabuf_v1") == 0)) {
    wl::Object<zwp_linux_dmabuf_v1> zwp_linux_dmabuf =
        wl::Bind<zwp_linux_dmabuf_v1>(
            registry, name, std::min(version, kMaxLinuxDmabufVersion));
    connection->zwp_dmabuf_ = std::make_unique<WaylandZwpLinuxDmabuf>(
        zwp_linux_dmabuf.release(), connection);
  } else if (!connection->presentation_ &&
             (strcmp(interface, "wp_presentation") == 0)) {
    connection->presentation_ =
        wl::Bind<wp_presentation>(registry, name, kMaxWpPresentationVersion);
  } else if (!connection->viewporter_ &&
             (strcmp(interface, "wp_viewporter") == 0)) {
    connection->viewporter_ =
        wl::Bind<wp_viewporter>(registry, name, kMaxWpViewporterVersion);
  } else if (!connection->keyboard_extension_v1_ &&
             strcmp(interface, "zcr_keyboard_extension_v1") == 0) {
    connection->keyboard_extension_v1_ = wl::Bind<zcr_keyboard_extension_v1>(
        registry, name, kMaxKeyboardExtensionVersion);
    if (!connection->keyboard_extension_v1_) {
      LOG(ERROR) << "Failed to bind zcr_keyboard_extension_v1";
      return;
    }
    // CreateKeyboard may fail if we do not have keyboard seat capabilities yet.
    // We will create the keyboard when get them in that case.
    connection->CreateKeyboard();
  } else if (!connection->text_input_manager_v1_ &&
             strcmp(interface, "zwp_text_input_manager_v1") == 0) {
    connection->text_input_manager_v1_ = wl::Bind<zwp_text_input_manager_v1>(
        registry, name, std::min(version, kMaxTextInputManagerVersion));
    if (!connection->text_input_manager_v1_) {
      LOG(ERROR) << "Failed to bind to zwp_text_input_manager_v1 global";
      return;
    }
  } else if (!connection->xdg_foreign_ &&
             strcmp(interface, "zxdg_exporter_v1") == 0) {
    connection->xdg_foreign_ = std::make_unique<XdgForeignWrapper>(
        connection, wl::Bind<zxdg_exporter_v1>(registry, name, version));
  } else if (!connection->drm_ && (strcmp(interface, "wl_drm") == 0) &&
             version >= kMinWlDrmVersion) {
    auto wayland_drm = wl::Bind<struct wl_drm>(registry, name, version);
    connection->drm_ =
        std::make_unique<WaylandDrm>(wayland_drm.release(), connection);
  } else if (!connection->aura_shell_ &&
             (strcmp(interface, "zaura_shell") == 0) &&
             version >= kMinAuraShellVersion) {
    connection->aura_shell_ =
        wl::Bind<struct zaura_shell>(registry, name, version);
    if (!connection->aura_shell_) {
      LOG(ERROR) << "Failed to bind zaura_shell";
      return;
    }
  } else if (!connection->xdg_decoration_manager_ &&
             strcmp(interface, "zxdg_decoration_manager_v1") == 0) {
    connection->xdg_decoration_manager_ =
        wl::Bind<struct zxdg_decoration_manager_v1>(registry, name,
                                                    kMaxXdgDecorationVersion);
  }

  connection->ScheduleFlush();
}

// static
void WaylandConnection::GlobalRemove(void* data,
                                     wl_registry* registry,
                                     uint32_t name) {
  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  // The Wayland protocol distinguishes global objects by unique numeric names,
  // which the WaylandOutputManager uses as unique output ids. But, it is only
  // possible to figure out, what global object is going to be removed on the
  // WaylandConnection::GlobalRemove call. Thus, whatever unique |name| comes,
  // it's forwarded to the WaylandOutputManager, which checks if such a global
  // output object exists and removes it.
  if (connection->wayland_output_manager_)
    connection->wayland_output_manager_->RemoveWaylandOutput(name);
}

// static
void WaylandConnection::Capabilities(void* data,
                                     wl_seat* seat,
                                     uint32_t capabilities) {
  WaylandConnection* self = static_cast<WaylandConnection*>(data);
  DCHECK(self);
  self->UpdateInputDevices(seat, capabilities);
  self->ScheduleFlush();
}

// static
void WaylandConnection::Name(void* data, wl_seat* seat, const char* name) {}

// static
void WaylandConnection::PingV6(void* data,
                               zxdg_shell_v6* shell_v6,
                               uint32_t serial) {
  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  zxdg_shell_v6_pong(shell_v6, serial);
  connection->ScheduleFlush();
}

// static
void WaylandConnection::Ping(void* data, xdg_wm_base* shell, uint32_t serial) {
  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  xdg_wm_base_pong(shell, serial);
  connection->ScheduleFlush();
}

}  // namespace ui

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_connection.h"

#include <xdg-shell-unstable-v5-client-protocol.h>
#include <xdg-shell-unstable-v6-client-protocol.h>
#include <memory>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop_current.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_drm.h"
#include "ui/ozone/platform/wayland/host/wayland_input_method_context.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_shm.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_linux_dmabuf.h"

static_assert(XDG_SHELL_VERSION_CURRENT == 5, "Unsupported xdg-shell version");

namespace ui {

namespace {
constexpr uint32_t kMaxCompositorVersion = 4;
constexpr uint32_t kMaxGtkPrimarySelectionDeviceManagerVersion = 1;
constexpr uint32_t kMaxLinuxDmabufVersion = 3;
constexpr uint32_t kMaxSeatVersion = 4;
constexpr uint32_t kMaxShmVersion = 1;
constexpr uint32_t kMaxXdgShellVersion = 1;
constexpr uint32_t kMaxDeviceManagerVersion = 3;
constexpr uint32_t kMaxWpPresentationVersion = 1;
constexpr uint32_t kMaxTextInputManagerVersion = 1;
constexpr uint32_t kMinWlDrmVersion = 2;
constexpr uint32_t kMinWlOutputVersion = 2;
}  // namespace

WaylandConnection::WaylandConnection() : controller_(FROM_HERE) {}

WaylandConnection::~WaylandConnection() = default;

bool WaylandConnection::Initialize() {
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
  if (!seat_) {
    LOG(ERROR) << "No wl_seat object";
    return false;
  }
  if (!shell_v6_ && !shell_) {
    LOG(ERROR) << "No xdg_shell object";
    return false;
  }

  return true;
}

bool WaylandConnection::StartProcessingEvents() {
  if (watching_)
    return true;

  DCHECK(display_);

  MaybePrepareReadQueue();

  // Displatch event from display to server.
  wl_display_flush(display_.get());

  return BeginWatchingFd(base::MessagePumpLibevent::WATCH_READ);
}

void WaylandConnection::MaybePrepareReadQueue() {
  if (prepared_)
    return;

  if (wl_display_prepare_read(display()) != -1) {
    prepared_ = true;
    return;
  }
  // Nothing to read, send events to the queue.
  wl_display_dispatch_pending(display());
}

void WaylandConnection::ScheduleFlush() {
  if (scheduled_flush_)
    return;
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandConnection::Flush, base::Unretained(this)));
  scheduled_flush_ = true;
}

void WaylandConnection::SetCursorBitmap(const std::vector<SkBitmap>& bitmaps,
                                        const gfx::Point& location) {
  if (!pointer_ || !pointer_->cursor())
    return;
  pointer_->cursor()->UpdateBitmap(bitmaps, location, serial_);
}

int WaylandConnection::GetKeyboardModifiers() const {
  int modifiers = 0;
  if (keyboard_)
    modifiers = keyboard_->modifiers();
  return modifiers;
}

void WaylandConnection::StartDrag(const ui::OSExchangeData& data,
                                  int operation) {
  if (!dragdrop_data_source_)
    dragdrop_data_source_ = data_device_manager_->CreateSource();
  dragdrop_data_source_->Offer(data);
  dragdrop_data_source_->SetAction(operation);
  data_device_->StartDrag(dragdrop_data_source_->data_source(), data);
}

void WaylandConnection::FinishDragSession(uint32_t dnd_action,
                                          WaylandWindow* source_window) {
  if (source_window)
    source_window->OnDragSessionClose(dnd_action);
  data_device_->ResetSourceData();
  dragdrop_data_source_.reset();
}

void WaylandConnection::DeliverDragData(const std::string& mime_type,
                                        std::string* buffer) {
  data_device_->DeliverDragData(mime_type, buffer);
}

void WaylandConnection::RequestDragData(
    const std::string& mime_type,
    base::OnceCallback<void(const std::vector<uint8_t>&)> callback) {
  data_device_->RequestDragData(mime_type, std::move(callback));
}

bool WaylandConnection::IsDragInProgress() {
  return data_device_->IsDragEntered() || drag_data_source();
}

void WaylandConnection::ResetPointerFlags() {
  if (pointer_)
    pointer_->ResetFlags();
}

void WaylandConnection::OnDispatcherListChanged() {
  StartProcessingEvents();
}

void WaylandConnection::Flush() {
  wl_display_flush(display_.get());
  scheduled_flush_ = false;
}

void WaylandConnection::DispatchUiEvent(Event* event) {
  PlatformEventSource::DispatchEvent(event);
}

void WaylandConnection::OnFileCanReadWithoutBlocking(int fd) {
  if (prepared_) {
    prepared_ = false;
    if (wl_display_read_events(display()) == -1)
      return;
    wl_display_dispatch_pending(display());
  }

  MaybePrepareReadQueue();

  if (!prepared_)
    return;

  // Automatic Flush.
  int ret = wl_display_flush(display_.get());
  if (ret != -1 || errno != EAGAIN)
    return;

  // if all data could not be written, errno will be set to EAGAIN and -1
  // returned. In that case, use poll on the display file descriptor to wait for
  // it to become writable again.
  BeginWatchingFd(base::MessagePumpLibevent::WATCH_WRITE);
}

void WaylandConnection::OnFileCanWriteWithoutBlocking(int fd) {
  int ret = wl_display_flush(display_.get());
  if (ret != -1 || errno != EAGAIN)
    BeginWatchingFd(base::MessagePumpLibevent::WATCH_READ);
  else if (ret < 0 && errno != EPIPE && prepared_)
    wl_display_cancel_read(display());

  // Otherwise just continue watching in the same mode.
}

void WaylandConnection::EnsureDataDevice() {
  if (!data_device_manager_ || !seat_)
    return;
  DCHECK(!data_device_);
  wl_data_device* data_device = data_device_manager_->GetDevice();
  data_device_ = std::make_unique<WaylandDataDevice>(this, data_device);

  if (primary_selection_device_manager_) {
    primary_selection_device_ = std::make_unique<GtkPrimarySelectionDevice>(
        this, primary_selection_device_manager_->GetDevice());
  }

  clipboard_ = std::make_unique<WaylandClipboard>(
      data_device_manager_.get(), data_device_.get(),
      primary_selection_device_manager_.get(), primary_selection_device_.get());
}

bool WaylandConnection::BeginWatchingFd(
    base::WatchableIOMessagePumpPosix::Mode mode) {
  if (watching_) {
    // Stop watching first.
    watching_ = !controller_.StopWatchingFileDescriptor();
    DCHECK(!watching_);
  }

  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  watching_ = base::MessageLoopCurrentForUI::Get()->WatchFileDescriptor(
      wl_display_get_fd(display_.get()), true, mode, &controller_, this);
  return watching_;
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
  static const xdg_shell_listener shell_listener = {
      &WaylandConnection::Ping,
  };
  static const zxdg_shell_v6_listener shell_v6_listener = {
      &WaylandConnection::PingV6,
  };

  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  if (!connection->compositor_ && strcmp(interface, "wl_compositor") == 0) {
    connection->compositor_ = wl::Bind<wl_compositor>(
        registry, name, std::min(version, kMaxCompositorVersion));
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
    connection->EnsureDataDevice();
  } else if (!connection->shell_v6_ &&
             strcmp(interface, "zxdg_shell_v6") == 0) {
    // Check for zxdg_shell_v6 first.
    connection->shell_v6_ = wl::Bind<zxdg_shell_v6>(
        registry, name, std::min(version, kMaxXdgShellVersion));
    if (!connection->shell_v6_) {
      LOG(ERROR) << "Failed to  bind to zxdg_shell_v6 global";
      return;
    }
    zxdg_shell_v6_add_listener(connection->shell_v6_.get(), &shell_v6_listener,
                               connection);
  } else if (!connection->shell_v6_ && !connection->shell_ &&
             strcmp(interface, "xdg_shell") == 0) {
    connection->shell_ = wl::Bind<xdg_shell>(
        registry, name, std::min(version, kMaxXdgShellVersion));
    if (!connection->shell_) {
      LOG(ERROR) << "Failed to  bind to xdg_shell global";
      return;
    }
    xdg_shell_add_listener(connection->shell_.get(), &shell_listener,
                           connection);
    xdg_shell_use_unstable_version(connection->shell_.get(),
                                   XDG_SHELL_VERSION_CURRENT);
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
    connection->EnsureDataDevice();
  } else if (!connection->primary_selection_device_manager_ &&
             strcmp(interface, "gtk_primary_selection_device_manager") == 0) {
    wl::Object<gtk_primary_selection_device_manager> manager =
        wl::Bind<gtk_primary_selection_device_manager>(
            registry, name, kMaxGtkPrimarySelectionDeviceManagerVersion);
    connection->primary_selection_device_manager_ =
        std::make_unique<GtkPrimarySelectionDeviceManager>(manager.release(),
                                                           connection);
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
  } else if (!connection->text_input_manager_v1_ &&
             strcmp(interface, "zwp_text_input_manager_v1") == 0) {
    connection->text_input_manager_v1_ = wl::Bind<zwp_text_input_manager_v1>(
        registry, name, std::min(version, kMaxTextInputManagerVersion));
    if (!connection->text_input_manager_v1_) {
      LOG(ERROR) << "Failed to bind to zwp_text_input_manager_v1 global";
      return;
    }
  } else if (!connection->drm_ && (strcmp(interface, "wl_drm") == 0) &&
             version >= kMinWlDrmVersion) {
    auto wayland_drm = wl::Bind<struct wl_drm>(registry, name, version);
    connection->drm_ =
        std::make_unique<WaylandDrm>(wayland_drm.release(), connection);
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
  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
    if (!connection->pointer_) {
      wl_pointer* pointer = wl_seat_get_pointer(connection->seat_.get());
      if (!pointer) {
        LOG(ERROR) << "Failed to get wl_pointer from seat";
        return;
      }
      connection->pointer_ = std::make_unique<WaylandPointer>(
          pointer, base::BindRepeating(&WaylandConnection::DispatchUiEvent,
                                       base::Unretained(connection)));
      connection->pointer_->set_connection(connection);

      connection->wayland_cursor_position_ =
          std::make_unique<WaylandCursorPosition>();
    }
  } else if (connection->pointer_) {
    connection->pointer_.reset();
    connection->wayland_cursor_position_.reset();
  }
  if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
    if (!connection->keyboard_) {
      wl_keyboard* keyboard = wl_seat_get_keyboard(connection->seat_.get());
      if (!keyboard) {
        LOG(ERROR) << "Failed to get wl_keyboard from seat";
        return;
      }
      connection->keyboard_ = std::make_unique<WaylandKeyboard>(
          keyboard, KeyboardLayoutEngineManager::GetKeyboardLayoutEngine(),
          base::BindRepeating(&WaylandConnection::DispatchUiEvent,
                              base::Unretained(connection)));
      connection->keyboard_->set_connection(connection);
    }
  } else if (connection->keyboard_) {
    connection->keyboard_.reset();
  }
  if (capabilities & WL_SEAT_CAPABILITY_TOUCH) {
    if (!connection->touch_) {
      wl_touch* touch = wl_seat_get_touch(connection->seat_.get());
      if (!touch) {
        LOG(ERROR) << "Failed to get wl_touch from seat";
        return;
      }
      connection->touch_ = std::make_unique<WaylandTouch>(
          touch, base::BindRepeating(&WaylandConnection::DispatchUiEvent,
                                     base::Unretained(connection)));
      connection->touch_->SetConnection(connection);
    }
  } else if (connection->touch_) {
    connection->touch_.reset();
  }
  connection->ScheduleFlush();
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
void WaylandConnection::Ping(void* data, xdg_shell* shell, uint32_t serial) {
  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  xdg_shell_pong(shell, serial);
  connection->ScheduleFlush();
}

}  // namespace ui

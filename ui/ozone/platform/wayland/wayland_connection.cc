// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_connection.h"

#include <xdg-shell-unstable-v5-client-protocol.h>
#include <xdg-shell-unstable-v6-client-protocol.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_current.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/platform/wayland/wayland_buffer_manager.h"
#include "ui/ozone/platform/wayland/wayland_input_method_context.h"
#include "ui/ozone/platform/wayland/wayland_object.h"
#include "ui/ozone/platform/wayland/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

static_assert(XDG_SHELL_VERSION_CURRENT == 5, "Unsupported xdg-shell version");

namespace ui {

namespace {
constexpr uint32_t kMaxCompositorVersion = 4;
constexpr uint32_t kMaxLinuxDmabufVersion = 3;
constexpr uint32_t kMaxSeatVersion = 4;
constexpr uint32_t kMaxShmVersion = 1;
constexpr uint32_t kMaxXdgShellVersion = 1;
constexpr uint32_t kMaxDeviceManagerVersion = 3;
constexpr uint32_t kMaxWpPresentationVersion = 1;
constexpr uint32_t kMaxTextInputManagerVersion = 1;

std::unique_ptr<WaylandDataSource> CreateWaylandDataSource(
    WaylandDataDeviceManager* data_device_manager,
    WaylandConnection* connection) {
  wl_data_source* data_source = data_device_manager->CreateSource();
  return std::make_unique<WaylandDataSource>(data_source, connection);
}
}  // namespace

WaylandConnection::WaylandConnection()
    : controller_(FROM_HERE), binding_(this) {}

WaylandConnection::~WaylandConnection() = default;

bool WaylandConnection::Initialize() {
  static const wl_registry_listener registry_listener = {
      &WaylandConnection::Global, &WaylandConnection::GlobalRemove,
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
         !wayland_output_manager_->IsPrimaryOutputReady()) {
    wl_display_roundtrip(display_.get());
  }

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
  wl_display_flush(display_.get());

  DCHECK(base::MessageLoopForUI::IsCurrent());
  if (!base::MessageLoopCurrentForUI::Get()->WatchFileDescriptor(
          wl_display_get_fd(display_.get()), true,
          base::MessagePumpLibevent::WATCH_READ, &controller_, this))
    return false;

  watching_ = true;
  return true;
}

void WaylandConnection::ScheduleFlush() {
  if (scheduled_flush_ || !watching_)
    return;
  DCHECK(base::MessageLoopForUI::IsCurrent());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandConnection::Flush, base::Unretained(this)));
  scheduled_flush_ = true;
}

WaylandWindow* WaylandConnection::GetWindow(gfx::AcceleratedWidget widget) {
  auto it = window_map_.find(widget);
  return it == window_map_.end() ? nullptr : it->second;
}

WaylandWindow* WaylandConnection::GetCurrentFocusedWindow() {
  for (auto entry : window_map_) {
    WaylandWindow* window = entry.second;
    if (window->has_pointer_focus())
      return window;
  }
  return nullptr;
}

WaylandWindow* WaylandConnection::GetCurrentKeyboardFocusedWindow() {
  for (auto entry : window_map_) {
    WaylandWindow* window = entry.second;
    if (window->has_keyboard_focus())
      return window;
  }
  return nullptr;
}

void WaylandConnection::AddWindow(gfx::AcceleratedWidget widget,
                                  WaylandWindow* window) {
  window_map_[widget] = window;
}

void WaylandConnection::RemoveWindow(gfx::AcceleratedWidget widget) {
  if (touch_)
    touch_->RemoveTouchPoints(window_map_[widget]);
  window_map_.erase(widget);
}

void WaylandConnection::SetCursorBitmap(const std::vector<SkBitmap>& bitmaps,
                                        const gfx::Point& location) {
  if (!pointer_ || !pointer_->cursor())
    return;
  pointer_->cursor()->UpdateBitmap(bitmaps, location, serial_);
}

int WaylandConnection::GetKeyboardModifiers() {
  int modifiers = 0;
  if (keyboard_)
    modifiers = keyboard_->modifiers();
  return modifiers;
}

void WaylandConnection::CreateZwpLinuxDmabuf(
    base::File file,
    uint32_t width,
    uint32_t height,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    uint32_t format,
    const std::vector<uint64_t>& modifiers,
    uint32_t planes_count,
    uint32_t buffer_id) {
  DCHECK(base::MessageLoopForUI::IsCurrent());
  if (!buffer_manager_->CreateBuffer(std::move(file), width, height, strides,
                                     offsets, format, modifiers, planes_count,
                                     buffer_id)) {
    TerminateGpuProcess(buffer_manager_->error_message());
  }
}

void WaylandConnection::DestroyZwpLinuxDmabuf(uint32_t buffer_id) {
  DCHECK(base::MessageLoopForUI::IsCurrent());
  if (!buffer_manager_->DestroyBuffer(buffer_id)) {
    TerminateGpuProcess(buffer_manager_->error_message());
  }
}

void WaylandConnection::ScheduleBufferSwap(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::Rect& damage_region,
    ScheduleBufferSwapCallback callback) {
  DCHECK(base::MessageLoopForUI::IsCurrent());
  if (!buffer_manager_->ScheduleBufferSwap(widget, buffer_id, damage_region,
                                           std::move(callback))) {
    TerminateGpuProcess(buffer_manager_->error_message());
  }
}

ClipboardDelegate* WaylandConnection::GetClipboardDelegate() {
  return this;
}

void WaylandConnection::OfferClipboardData(
    const ClipboardDelegate::DataMap& data_map,
    ClipboardDelegate::OfferDataClosure callback) {
  if (!data_source_) {
    data_source_ = CreateWaylandDataSource(data_device_manager_.get(), this);
    data_source_->WriteToClipboard(data_map);
  }
  data_source_->UpdataDataMap(data_map);
  std::move(callback).Run();
}

void WaylandConnection::RequestClipboardData(
    const std::string& mime_type,
    ClipboardDelegate::DataMap* data_map,
    ClipboardDelegate::RequestDataClosure callback) {
  read_clipboard_closure_ = std::move(callback);

  DCHECK(data_map);
  data_map_ = data_map;
  data_device_->RequestSelectionData(mime_type);
}

bool WaylandConnection::IsSelectionOwner() {
  return !!data_source_;
}

ozone::mojom::WaylandConnectionPtr WaylandConnection::BindInterface() {
  // This mustn't be called twice or when the zwp_linux_dmabuf interface is not
  // available.
  DCHECK(!binding_.is_bound() || buffer_manager_);
  ozone::mojom::WaylandConnectionPtr ptr;
  binding_.Bind(MakeRequest(&ptr));
  return ptr;
}

std::vector<gfx::BufferFormat> WaylandConnection::GetSupportedBufferFormats() {
  if (buffer_manager_)
    return buffer_manager_->supported_buffer_formats();
  return std::vector<gfx::BufferFormat>();
}

void WaylandConnection::SetTerminateGpuCallback(
    base::OnceCallback<void(std::string)> terminate_callback) {
  terminate_gpu_cb_ = std::move(terminate_callback);
}

void WaylandConnection::StartDrag(const ui::OSExchangeData& data,
                                  int operation) {
  if (!drag_data_source_) {
    drag_data_source_ =
        CreateWaylandDataSource(data_device_manager_.get(), this);
  }
  drag_data_source_->Offer(data);
  drag_data_source_->SetAction(operation);
  data_device_->StartDrag(*(drag_data_source_->data_source()), data);
}

void WaylandConnection::FinishDragSession(uint32_t dnd_action,
                                          WaylandWindow* source_window) {
  if (source_window)
    source_window->OnDragSessionClose(dnd_action);
  data_device_->ResetSourceData();
  drag_data_source_.reset();
}

void WaylandConnection::DeliverDragData(const std::string& mime_type,
                                        std::string* buffer) {
  data_device_->DeliverDragData(mime_type, buffer);
}

void WaylandConnection::RequestDragData(
    const std::string& mime_type,
    base::OnceCallback<void(const std::string&)> callback) {
  data_device_->RequestDragData(mime_type, std::move(callback));
}

void WaylandConnection::ResetPointerFlags() {
  if (pointer_)
    pointer_->ResetFlags();
}

void WaylandConnection::GetAvailableMimeTypes(
    ClipboardDelegate::GetMimeTypesClosure callback) {
  std::move(callback).Run(data_device_->GetAvailableMimeTypes());
}

void WaylandConnection::DataSourceCancelled() {
  SetClipboardData(std::string(), std::string());
  data_source_.reset();
}

void WaylandConnection::SetClipboardData(const std::string& contents,
                                         const std::string& mime_type) {
  if (!data_map_)
    return;

  (*data_map_)[mime_type] =
      std::vector<uint8_t>(contents.begin(), contents.end());

  if (!read_clipboard_closure_.is_null()) {
    auto it = data_map_->find(mime_type);
    DCHECK(it != data_map_->end());
    std::move(read_clipboard_closure_).Run(it->second);
  }
  data_map_ = nullptr;
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
  wl_display_dispatch(display_.get());
  for (const auto& window : window_map_)
    window.second->ApplyPendingBounds();
}

void WaylandConnection::OnFileCanWriteWithoutBlocking(int fd) {}

void WaylandConnection::TerminateGpuProcess(std::string reason) {
  std::move(terminate_gpu_cb_).Run(std::move(reason));
  binding_.Unbind();
  buffer_manager_->ClearState();
}

// static
void WaylandConnection::Global(void* data,
                               wl_registry* registry,
                               uint32_t name,
                               const char* interface,
                               uint32_t version) {
  static const wl_seat_listener seat_listener = {
      &WaylandConnection::Capabilities, &WaylandConnection::Name,
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
    connection->shm_ =
        wl::Bind<wl_shm>(registry, name, std::min(version, kMaxShmVersion));
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

    // TODO(tonikitoo,msisov): The connection passed to WaylandInputDevice must
    // have a valid data device manager. We should ideally be robust to the
    // compositor advertising a wl_seat first. No known compositor does this,
    // fortunately.
    if (!connection->data_device_manager_) {
      LOG(ERROR)
          << "No data device manager. Clipboard won't be fully functional";
      return;
    }
    wl_data_device* data_device = connection->data_device_manager_->GetDevice();
    connection->data_device_.reset(
        new WaylandDataDevice(connection, data_device));
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
    wl::Object<wl_output> output = wl::Bind<wl_output>(registry, name, 1);
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
    connection->data_device_manager_.reset(
        new WaylandDataDeviceManager(data_device_manager.release()));
    connection->data_device_manager_->set_connection(connection);
  } else if (!connection->buffer_manager_ &&
             (strcmp(interface, "zwp_linux_dmabuf_v1") == 0)) {
    wl::Object<zwp_linux_dmabuf_v1> zwp_linux_dmabuf =
        wl::Bind<zwp_linux_dmabuf_v1>(
            registry, name, std::min(version, kMaxLinuxDmabufVersion));
    connection->buffer_manager_.reset(
        new WaylandBufferManager(zwp_linux_dmabuf.release(), connection));
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
    }
  } else if (connection->pointer_) {
    connection->pointer_.reset();
  }
  if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
    if (!connection->keyboard_) {
      wl_keyboard* keyboard = wl_seat_get_keyboard(connection->seat_.get());
      if (!keyboard) {
        LOG(ERROR) << "Failed to get wl_keyboard from seat";
        return;
      }
      connection->keyboard_ = std::make_unique<WaylandKeyboard>(
          keyboard, base::BindRepeating(&WaylandConnection::DispatchUiEvent,
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
      connection->touch_->set_connection(connection);
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

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CONNECTION_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CONNECTION_H_

#include <memory>
#include <vector>

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/events/event.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_clipboard.h"
#include "ui/ozone/platform/wayland/host/wayland_data_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"

struct wl_cursor;

namespace gfx {
class Point;
}

namespace ui {

class WaylandBufferManagerHost;
class WaylandCursor;
class WaylandCursorBufferListener;
class WaylandDrm;
class WaylandEventSource;
class WaylandKeyboard;
class WaylandOutputManager;
class WaylandPointer;
class WaylandShm;
class WaylandTouch;
class WaylandZAuraShell;
class WaylandZcrCursorShapes;
class WaylandZwpLinuxDmabuf;
class WaylandDataDeviceManager;
class WaylandCursorPosition;
class WaylandWindowDragController;
class GtkPrimarySelectionDeviceManager;
class ZwpPrimarySelectionDeviceManager;
class XdgForeignWrapper;

class WaylandConnection {
 public:
  // Stores the last serial and the event type it is associated with.
  struct EventSerial {
    uint32_t serial = 0;
    EventType event_type = EventType::ET_UNKNOWN;
  };

  WaylandConnection();
  WaylandConnection(const WaylandConnection&) = delete;
  WaylandConnection& operator=(const WaylandConnection&) = delete;
  ~WaylandConnection();

  bool Initialize();

  // Schedules a flush of the Wayland connection.
  void ScheduleFlush();

  // Sets a callback that that shutdowns the browser in case of unrecoverable
  // error. Called by WaylandEventWatcher.
  void SetShutdownCb(base::OnceCallback<void()> shutdown_cb);

  wl_display* display() const { return display_.get(); }
  wl_compositor* compositor() const { return compositor_.get(); }
  // The server version of the compositor interface (might be higher than the
  // version binded).
  uint32_t compositor_version() const { return compositor_version_; }
  wl_subcompositor* subcompositor() const { return subcompositor_.get(); }
  wp_viewporter* viewporter() const { return viewporter_.get(); }
  xdg_wm_base* shell() const { return shell_.get(); }
  zxdg_shell_v6* shell_v6() const { return shell_v6_.get(); }
  wl_seat* seat() const { return seat_.get(); }
  wp_presentation* presentation() const { return presentation_.get(); }
  zwp_text_input_manager_v1* text_input_manager_v1() const {
    return text_input_manager_v1_.get();
  }
  zwp_linux_explicit_synchronization_v1* linux_explicit_synchronization_v1()
      const {
    return linux_explicit_synchronization_.get();
  }
  zxdg_decoration_manager_v1* xdg_decoration_manager_v1() const {
    return xdg_decoration_manager_.get();
  }
  zcr_extended_drag_v1* extended_drag_v1() const {
    return extended_drag_v1_.get();
  }

  void set_serial(uint32_t serial, EventType event_type) {
    serial_ = {serial, event_type};
  }
  uint32_t serial() const { return serial_.serial; }
  EventSerial event_serial() const { return serial_; }

  void SetPlatformCursor(wl_cursor* cursor_data, int buffer_scale);

  void SetCursorBufferListener(WaylandCursorBufferListener* listener);

  void SetCursorBitmap(const std::vector<SkBitmap>& bitmaps,
                       const gfx::Point& hotspot_in_dips,
                       int buffer_scale);

  WaylandEventSource* event_source() const { return event_source_.get(); }

  // Returns the current touch, which may be null.
  WaylandTouch* touch() const { return touch_.get(); }

  // Returns the current pointer, which may be null.
  WaylandPointer* pointer() const { return pointer_.get(); }

  // Returns the current keyboard, which may be null.
  WaylandKeyboard* keyboard() const { return keyboard_.get(); }

  WaylandClipboard* clipboard() const { return clipboard_.get(); }

  WaylandOutputManager* wayland_output_manager() const {
    return wayland_output_manager_.get();
  }

  // Returns the cursor position, which may be null.
  WaylandCursorPosition* wayland_cursor_position() const {
    return wayland_cursor_position_.get();
  }

  WaylandBufferManagerHost* buffer_manager_host() const {
    return buffer_manager_host_.get();
  }

  WaylandZAuraShell* zaura_shell() const { return zaura_shell_.get(); }

  WaylandZcrCursorShapes* zcr_cursor_shapes() const {
    return zcr_cursor_shapes_.get();
  }

  WaylandZwpLinuxDmabuf* zwp_dmabuf() const { return zwp_dmabuf_.get(); }

  WaylandDrm* drm() const { return drm_.get(); }

  WaylandShm* shm() const { return shm_.get(); }

  WaylandWindowManager* wayland_window_manager() {
    return &wayland_window_manager_;
  }

  WaylandDataDeviceManager* data_device_manager() const {
    return data_device_manager_.get();
  }

  GtkPrimarySelectionDeviceManager* gtk_primary_selection_device_manager()
      const {
    return gtk_primary_selection_device_manager_.get();
  }

  ZwpPrimarySelectionDeviceManager* zwp_primary_selection_device_manager() const {
    return zwp_primary_selection_device_manager_.get();
  }

  WaylandDataDragController* data_drag_controller() const {
    return data_drag_controller_.get();
  }

  WaylandWindowDragController* window_drag_controller() const {
    return window_drag_controller_.get();
  }

  XdgForeignWrapper* xdg_foreign() const { return xdg_foreign_.get(); }

  // Returns true when dragging is entered or started.
  bool IsDragInProgress() const;

  // Creates a new wl_surface.
  wl::Object<wl_surface> CreateSurface();

 private:
  friend class WaylandConnectionTestApi;

  void Flush();
  void UpdateInputDevices(wl_seat* seat, uint32_t capabilities);

  // Initialize data-related objects if required protocol objects are already
  // in place, i.e: wl_seat and wl_data_device_manager.
  void CreateDataObjectsIfReady();

  // Creates WaylandKeyboard with the currently acquired protocol objects, if
  // possible. Returns true iff WaylandKeyboard was created.
  bool CreateKeyboard();

  // wl_registry_listener
  static void Global(void* data,
                     wl_registry* registry,
                     uint32_t name,
                     const char* interface,
                     uint32_t version);
  static void GlobalRemove(void* data, wl_registry* registry, uint32_t name);

  // wl_seat_listener
  static void Capabilities(void* data, wl_seat* seat, uint32_t capabilities);
  static void Name(void* data, wl_seat* seat, const char* name);

  // zxdg_shell_v6_listener
  static void PingV6(void* data, zxdg_shell_v6* zxdg_shell_v6, uint32_t serial);

  // xdg_wm_base_listener
  static void Ping(void* data, xdg_wm_base* shell, uint32_t serial);

  uint32_t compositor_version_ = 0;
  wl::Object<wl_display> display_;
  wl::Object<wl_registry> registry_;
  wl::Object<wl_compositor> compositor_;
  wl::Object<wl_subcompositor> subcompositor_;
  wl::Object<wl_seat> seat_;
  wl::Object<xdg_wm_base> shell_;
  wl::Object<zxdg_shell_v6> shell_v6_;
  wl::Object<wp_presentation> presentation_;
  wl::Object<wp_viewporter> viewporter_;
  wl::Object<zcr_keyboard_extension_v1> keyboard_extension_v1_;
  wl::Object<zwp_text_input_manager_v1> text_input_manager_v1_;
  wl::Object<zwp_linux_explicit_synchronization_v1>
      linux_explicit_synchronization_;
  wl::Object<zxdg_decoration_manager_v1> xdg_decoration_manager_;
  wl::Object<zcr_extended_drag_v1> extended_drag_v1_;

  // Event source instance. Must be declared before input objects so it
  // outlives them so thus being able to properly handle their destruction.
  std::unique_ptr<WaylandEventSource> event_source_;

  // Input device objects.
  std::unique_ptr<WaylandKeyboard> keyboard_;
  std::unique_ptr<WaylandPointer> pointer_;
  std::unique_ptr<WaylandTouch> touch_;

  std::unique_ptr<WaylandCursor> cursor_;
  std::unique_ptr<WaylandDataDeviceManager> data_device_manager_;
  std::unique_ptr<WaylandClipboard> clipboard_;
  std::unique_ptr<WaylandOutputManager> wayland_output_manager_;
  std::unique_ptr<WaylandCursorPosition> wayland_cursor_position_;
  std::unique_ptr<WaylandZAuraShell> zaura_shell_;
  std::unique_ptr<WaylandZcrCursorShapes> zcr_cursor_shapes_;
  std::unique_ptr<WaylandZwpLinuxDmabuf> zwp_dmabuf_;
  std::unique_ptr<WaylandDrm> drm_;
  std::unique_ptr<WaylandShm> shm_;
  std::unique_ptr<WaylandBufferManagerHost> buffer_manager_host_;
  std::unique_ptr<XdgForeignWrapper> xdg_foreign_;

  std::unique_ptr<GtkPrimarySelectionDeviceManager>
      gtk_primary_selection_device_manager_;

  std::unique_ptr<ZwpPrimarySelectionDeviceManager>
      zwp_primary_selection_device_manager_;

  std::unique_ptr<WaylandDataDragController> data_drag_controller_;
  std::unique_ptr<WaylandWindowDragController> window_drag_controller_;

  // Manages Wayland windows.
  WaylandWindowManager wayland_window_manager_;

  WaylandCursorBufferListener* listener_ = nullptr;

  bool scheduled_flush_ = false;

  EventSerial serial_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CONNECTION_H_

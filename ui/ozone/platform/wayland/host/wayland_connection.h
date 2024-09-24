// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CONNECTION_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CONNECTION_H_

#include <time.h>

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/tablet_state.h"
#include "ui/events/event.h"
#include "ui/gl/gl_display.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/single_pixel_buffer.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_clipboard.h"
#include "ui/ozone/platform/wayland/host/wayland_data_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"

struct wl_cursor;
struct wl_event_queue;

namespace gfx {
class Point;
}

namespace wl {
class WaylandProxy;
}

namespace ui {

struct InputDevice;
class OrgKdeKwinIdle;
class SurfaceAugmenter;
struct KeyboardDevice;
struct TouchscreenDevice;
class WaylandBufferFactory;
class WaylandBufferManagerHost;
class WaylandCursor;
class WaylandCursorBufferListener;
class WaylandEventSource;
class WaylandOutputManager;
class WaylandSeat;
class WaylandZAuraShell;
class WaylandZAuraOutputManagerV2;
class WaylandZcrColorManager;
class WaylandZcrCursorShapes;
class WaylandZcrTouchpadHaptics;
class WaylandZwpPointerConstraints;
class WaylandZwpPointerGestures;
class WaylandZwpRelativePointerManager;
class WaylandDataDeviceManager;
class WaylandCursorPosition;
class WaylandCursorShape;
class WaylandWindowDragController;
class GtkPrimarySelectionDeviceManager;
class GtkShell1;
class ZwpIdleInhibitManager;
class ZwpPrimarySelectionDeviceManager;
class XdgActivation;
class XdgForeignWrapper;
class OverlayPrioritizer;

// These values are persisted to logs.  Entries should not be renumbered and
// numeric values should never be reused.
//
// Append new shells before kMaxValue and update LinuxWaylandShell
// in tools/metrics/histograms/enums.xml accordingly.
//
// See also tools/metrics/histograms/README.md#enum-histograms
enum class UMALinuxWaylandShell {
  kZauraShell = 0,
  kGtkShell1 = 1,
  kOrgKdePlasmaShell = 2,
  kXdgWmBase = 3,
  kXdgShellV6 = 4,
  kZwlrLayerShellV1 = 5,
  kMaxValue = kZwlrLayerShellV1,
};

void ReportShellUMA(UMALinuxWaylandShell shell);

class WaylandConnection {
 public:
  WaylandConnection();
  WaylandConnection(const WaylandConnection&) = delete;
  WaylandConnection& operator=(const WaylandConnection&) = delete;
  ~WaylandConnection();

  bool Initialize(bool use_threaded_polling = false);

  // Immediately flushes the Wayland display.
  void Flush();

  // Calls wl_display_roundtrip_queue. Might be required during initialization
  // of some objects that should block until they are initialized.
  void RoundTripQueue();

  // Sets a callback that that shutdowns the browser in case of unrecoverable
  // error. Called by WaylandEventWatcher.
  void SetShutdownCb(base::OnceCallback<void()> shutdown_cb);

  // Returns the dotted number version of the Wayland server. For Lacros, this
  // is the Ash Chrome version.
  base::Version GetServerVersion() const;

  wl_compositor* compositor() const { return compositor_.get(); }
  // The server version of the compositor interface (might be higher than the
  // version binded).
  uint32_t compositor_version() const { return compositor_version_; }
  wl_subcompositor* subcompositor() const { return subcompositor_.get(); }
  wp_content_type_manager_v1* content_type_manager_v1() const {
    return content_type_manager_v1_.get();
  }
  wp_viewporter* viewporter() const { return viewporter_.get(); }
  zcr_alpha_compositing_v1* alpha_compositing() const {
    return alpha_compositing_.get();
  }
  xdg_wm_base* shell() const { return shell_.get(); }
  wp_presentation* presentation() const { return presentation_.get(); }
  zcr_keyboard_extension_v1* keyboard_extension_v1() const {
    return keyboard_extension_v1_.get();
  }
  zwp_keyboard_shortcuts_inhibit_manager_v1*
  keyboard_shortcuts_inhibit_manager_v1() const {
    return keyboard_shortcuts_inhibit_manager_v1_.get();
  }
  zcr_stylus_v2* stylus_v2() const { return zcr_stylus_v2_.get(); }
  zwp_text_input_manager_v1* text_input_manager_v1() const {
    return text_input_manager_v1_.get();
  }
  zcr_text_input_extension_v1* text_input_extension_v1() const {
    return text_input_extension_v1_.get();
  }
  zwp_text_input_manager_v3* text_input_manager_v3() const {
    return text_input_manager_v3_.get();
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
  xdg_toplevel_drag_manager_v1* toplevel_drag_manager_v1() const {
    return xdg_toplevel_drag_manager_v1_.get();
  }

  zxdg_output_manager_v1* xdg_output_manager_v1() const {
    return xdg_output_manager_.get();
  }

  wp_fractional_scale_manager_v1* fractional_scale_manager_v1() const {
    return fractional_scale_manager_v1_.get();
  }

  xdg_toplevel_icon_manager_v1* toplevel_icon_manager_v1() const {
    return toplevel_icon_manager_v1_.get();
  }

  void SetPlatformCursor(wl_cursor* cursor_data, int buffer_scale);

  void SetCursorBufferListener(WaylandCursorBufferListener* listener);

  void SetCursorBitmap(const std::vector<SkBitmap>& bitmaps,
                       const gfx::Point& hotspot_in_dips,
                       int buffer_scale);

  WaylandEventSource* event_source() const { return event_source_.get(); }

  WaylandSeat* seat() const { return seat_.get(); }

  WaylandClipboard* clipboard() const { return clipboard_.get(); }

  WaylandOutputManager* wayland_output_manager() const {
    return output_manager_.get();
  }

  // Returns the cursor position, which may be null.
  WaylandCursorPosition* wayland_cursor_position() const {
    return cursor_position_.get();
  }

  WaylandBufferManagerHost* buffer_manager_host() const {
    return buffer_manager_host_.get();
  }

  WaylandZAuraOutputManagerV2* zaura_output_manager_v2() const {
    return zaura_output_manager_v2_.get();
  }

  WaylandZAuraShell* zaura_shell() const { return zaura_shell_.get(); }

  WaylandZcrColorManager* zcr_color_manager() const {
    return zcr_color_manager_.get();
  }

  WaylandCursorShape* wayland_cursor_shape() const {
    return cursor_shape_.get();
  }

  WaylandZcrCursorShapes* zcr_cursor_shapes() const {
    return zcr_cursor_shapes_.get();
  }

  WaylandZcrTouchpadHaptics* zcr_touchpad_haptics() const {
    return zcr_touchpad_haptics_.get();
  }

  WaylandWindowManager* window_manager() { return &window_manager_; }

  WaylandBufferFactory* buffer_factory() const { return buffer_factory_.get(); }

  WaylandDataDeviceManager* data_device_manager() const {
    return data_device_manager_.get();
  }

  GtkPrimarySelectionDeviceManager* gtk_primary_selection_device_manager()
      const {
    return gtk_primary_selection_device_manager_.get();
  }

  GtkShell1* gtk_shell1() { return gtk_shell1_.get(); }

  OrgKdeKwinIdle* org_kde_kwin_idle() { return org_kde_kwin_idle_.get(); }

  ZwpPrimarySelectionDeviceManager* zwp_primary_selection_device_manager()
      const {
    return zwp_primary_selection_device_manager_.get();
  }

  WaylandDataDragController* data_drag_controller() const {
    return data_drag_controller_.get();
  }

  WaylandWindowDragController* window_drag_controller() const {
    return window_drag_controller_.get();
  }

  WaylandZwpPointerConstraints* zwp_pointer_constraints() const {
    return zwp_pointer_constraints_.get();
  }

  WaylandZwpPointerGestures* zwp_pointer_gestures() const {
    return zwp_pointer_gestures_.get();
  }

  WaylandZwpRelativePointerManager* zwp_relative_pointer_manager() const {
    return zwp_relative_pointer_manager_.get();
  }

  const XdgActivation* xdg_activation() const { return xdg_activation_.get(); }

  XdgForeignWrapper* xdg_foreign() const { return xdg_foreign_.get(); }

  ZwpIdleInhibitManager* zwp_idle_inhibit_manager() const {
    return zwp_idle_inhibit_manager_.get();
  }

  OverlayPrioritizer* overlay_prioritizer() const {
    return overlay_prioritizer_.get();
  }

  SurfaceAugmenter* surface_augmenter() const {
    return surface_augmenter_.get();
  }

  SinglePixelBuffer* single_pixel_buffer() const {
    return single_pixel_buffer_.get();
  }

  // Returns whether protocols that support setting window geometry are
  // available.
  bool SupportsSetWindowGeometry() const;

  // Returns true when there an active outgoing drag-and-drop session.
  bool IsDragInProgress() const;

  // Creates a new wl_surface.
  wl::Object<wl_surface> CreateSurface();

  // base::TimeTicks::Now() in posix uses CLOCK_MONOTONIC, wp_presentation
  // timestamps are in clk_id sent in wp_presentation.clock_id event. This
  // converts wp_presentation timestamp to base::TimeTicks.
  // The approximation relies on presentation timestamp to be close to current
  // time. The further it is from current time and the bigger the speed
  // difference between the two clock domains, the bigger the conversion error.
  // Conversion error due to system load is biased and unbounded.
  base::TimeTicks ConvertPresentationTime(uint32_t tv_sec_hi,
                                          uint32_t tv_sec_lo,
                                          uint32_t tv_nsec);

  const std::vector<std::pair<std::string, uint32_t>>& available_globals()
      const {
    return available_globals_;
  }

  bool surface_submission_in_pixel_coordinates() const {
    return surface_submission_in_pixel_coordinates_;
  }

  void set_surface_submission_in_pixel_coordinates(bool enabled) {
    surface_submission_in_pixel_coordinates_ = enabled;
  }

  bool supports_viewporter_surface_scaling() const {
    return supports_viewporter_surface_scaling_;
  }

  void set_supports_viewporter_surface_scaling(bool enabled) {
    supports_viewporter_surface_scaling_ = enabled;
  }

  bool UseViewporterSurfaceScaling() const {
    return supports_viewporter_surface_scaling_ &&
           !surface_submission_in_pixel_coordinates_;
  }

  bool UsePerSurfaceScaling() const {
    return base::FeatureList::IsEnabled(features::kWaylandPerSurfaceScale) &&
           UseViewporterSurfaceScaling();
  }

  bool IsUiScaleEnabled() const {
    return base::FeatureList::IsEnabled(features::kWaylandUiScale) &&
           UsePerSurfaceScaling();
  }

  bool ShouldUseOverlayDelegation() const;

  // True if the client has bound the either aura output manager globals. If
  // present aura output manager handles the responsibilities of keeping
  // output metrics up to date and triggering delegate notifications.
  bool IsUsingZAuraOutputManager() const;

  wl::SerialTracker& serial_tracker() { return serial_tracker_; }

  void set_tablet_layout_state(display::TabletState tablet_layout_state) {
    tablet_layout_state_ = tablet_layout_state;
  }
  bool GetTabletMode() {
    return tablet_layout_state_ == display::TabletState::kInTabletMode ||
           tablet_layout_state_ == display::TabletState::kEnteringTabletMode;
  }
  display::TabletState GetTabletState() { return tablet_layout_state_; }

  const gfx::PointF MaybeConvertLocation(const gfx::PointF& location,
                                         const WaylandWindow* window) const;

  void DumpState(std::ostream& out) const;

  bool UseImplicitSyncInterop() const {
    return !linux_explicit_synchronization_v1() &&
           WaylandBufferManagerHost::SupportsImplicitSyncInterop();
  }

  // Returns a sync callback, which is invoked when the server has processed all
  // pending events prior to this sync point.
  struct wl_callback* GetSyncCallback();

  gl::EGLDisplayPlatform GetNativeDisplay();

  struct wl_registry* GetRegistry();

 private:
  friend class WaylandConnectionTestApi;

  // All global Wayland objects are friends of the Wayland connection.
  // Conceptually, this is correct: globals are owned by the connection and are
  // accessed via it, so they are essentially parts of it.  Also this friendship
  // makes it possible to avoid exposing setters for all those global objects:
  // these setters would only be needed by the globals but would be visible to
  // everyone.
  friend class FractionalScaleManager;
  friend class GtkPrimarySelectionDeviceManager;
  friend class GtkShell1;
  friend class OrgKdeKwinIdle;
  friend class OverlayPrioritizer;
  friend class SinglePixelBuffer;
  friend class SurfaceAugmenter;
  friend class ToplevelIconManager;
  friend class WaylandDataDeviceManager;
  friend class WaylandOutput;
  friend class WaylandSeat;
  friend class WaylandZAuraOutputManagerV2;
  friend class WaylandZAuraShell;
  friend class WaylandZcrTouchpadHaptics;
  friend class WaylandZwpPointerConstraints;
  friend class WaylandZwpPointerGestures;
  friend class WaylandZwpRelativePointerManager;
  friend class WaylandZcrColorManager;
  friend class WaylandCursorShape;
  friend class WaylandZcrCursorShapes;
  friend class XdgActivation;
  friend class XdgForeignWrapper;
  friend class ZwpIdleInhibitManager;
  friend class ZwpPrimarySelectionDeviceManager;

  // A correct display must be chosen when creating objects or calling
  // roundtrips. That is, all the methods that deal with polling, pulling event
  // queues, etc, must use original display. All the other methods that create
  // various wayland objects must use |display_wrapper_| so that the new objects
  // are associated with the correct event queue. See the comment below about
  // the |event_queue_|.
  wl_display* display() const { return display_.get(); }
  wl_display* display_wrapper() const {
    return reinterpret_cast<wl_display*>(wrapped_display_.get());
  }

  void RegisterGlobalObjectFactory(const char* interface_name,
                                   wl::GlobalObjectFactory factory);

  // Returns true if the required wl_globals are announced by the server.
  bool WlGlobalsReady() const;

  // Based on the bound globals, returns true if required information are
  // announced by the server. E.g. server version from zaura-shell.
  bool WlObjectsReady() const;

  // Updates InputDevice structures in Chrome. Currently, Wayland doesn't
  // support such, so the devices are derived from the connected interfaces.
  // Also, currently, Wayland doesn't expose InputDeviceType so marked as
  // UNKNOWN.
  // TODO(crbug.com/40254071): We need further investigation and proper design
  // how to model these input devices.
  void UpdateInputDevices();
  std::vector<InputDevice> CreateMouseDevices() const;
  std::vector<KeyboardDevice> CreateKeyboardDevices() const;
  std::vector<TouchscreenDevice> CreateTouchscreenDevices() const;

  // Updates cursor related objects in this instance.
  void UpdateCursor();

  // Initialize data-related objects if required protocol objects are already
  // in place, i.e: wl_seat and wl_data_device_manager.
  void CreateDataObjectsIfReady();

  // wl_registry_listener callbacks:
  static void OnGlobal(void* data,
                       wl_registry* registry,
                       uint32_t name,
                       const char* interface,
                       uint32_t version);
  static void OnGlobalRemove(void* data, wl_registry* registry, uint32_t name);

  // xdg_wm_base_listener callbacks:
  static void OnPing(void* data, xdg_wm_base* shell, uint32_t serial);

  // wp_presentation_listener callbacks:
  static void OnClockId(void* data,
                        wp_presentation* presentation,
                        uint32_t clk_id);

  void HandleGlobal(wl_registry* registry,
                    uint32_t name,
                    const char* interface,
                    uint32_t version);

  base::flat_map<std::string, wl::GlobalObjectFactory> global_object_factories_;

  uint32_t compositor_version_ = 0;
  wl::Object<wl_display> display_;
  // `event_queue_` must be declared before `wrapped_display_`, so that the
  // latter is destroyed first. This prevents libwayland warnings about the
  // queue being destroyed while the proxy is still attached.
  wl::Object<wl_event_queue> event_queue_;
  // A non-default display that Ozone/Wayland uses for event dispatching
  // (a non-default `event_queue_` is created using this display). This is
  // necessary to avoid any possible deadlocks (in case of API's misuse. See
  // https://crrev.com/c/2844573 for more context) or to avoid cases when other
  // clients' events are consumed (such as GTK and others) if both Ozone/Wayland
  // and those clients use the default display returned by |wl_display_connect|.
  wl::Object<wl_proxy> wrapped_display_;
  wl::Object<wl_registry> registry_;
  wl::Object<wl_compositor> compositor_;
  wl::Object<wl_subcompositor> subcompositor_;
  wl::Object<xdg_wm_base> shell_;
  wl::Object<wp_content_type_manager_v1> content_type_manager_v1_;
  wl::Object<wp_presentation> presentation_;
  wl::Object<wp_viewporter> viewporter_;
  wl::Object<zcr_alpha_compositing_v1> alpha_compositing_;
  wl::Object<zcr_keyboard_extension_v1> keyboard_extension_v1_;
  wl::Object<zwp_keyboard_shortcuts_inhibit_manager_v1>
      keyboard_shortcuts_inhibit_manager_v1_;
  wl::Object<zcr_stylus_v2> zcr_stylus_v2_;
  wl::Object<zwp_text_input_manager_v1> text_input_manager_v1_;
  wl::Object<zwp_text_input_manager_v3> text_input_manager_v3_;
  wl::Object<zcr_text_input_extension_v1> text_input_extension_v1_;
  wl::Object<zwp_linux_explicit_synchronization_v1>
      linux_explicit_synchronization_;
  wl::Object<zxdg_decoration_manager_v1> xdg_decoration_manager_;
  wl::Object<zcr_extended_drag_v1> extended_drag_v1_;
  wl::Object<::xdg_toplevel_drag_manager_v1> xdg_toplevel_drag_manager_v1_;
  wl::Object<zxdg_output_manager_v1> xdg_output_manager_;
  wl::Object<wp_fractional_scale_manager_v1> fractional_scale_manager_v1_;
  wl::Object<xdg_toplevel_icon_manager_v1> toplevel_icon_manager_v1_;

  // Manages Wayland windows.
  WaylandWindowManager window_manager_{this};

  // Event source instance. Must be declared before input objects so it
  // outlives them so thus being able to properly handle their destruction.
  std::unique_ptr<WaylandEventSource> event_source_;

  // Factory that wraps all the supported wayland objects that are provide
  // capabilities to create wl_buffers.
  std::unique_ptr<WaylandBufferFactory> buffer_factory_;

  std::unique_ptr<WaylandCursor> cursor_;
  std::unique_ptr<WaylandDataDeviceManager> data_device_manager_;
  std::unique_ptr<WaylandOutputManager> output_manager_;
  std::unique_ptr<WaylandCursorPosition> cursor_position_;
  std::unique_ptr<WaylandZAuraOutputManagerV2> zaura_output_manager_v2_;
  std::unique_ptr<WaylandZAuraShell> zaura_shell_;
  std::unique_ptr<WaylandZcrColorManager> zcr_color_manager_;
  std::unique_ptr<WaylandCursorShape> cursor_shape_;
  std::unique_ptr<WaylandZcrCursorShapes> zcr_cursor_shapes_;
  std::unique_ptr<WaylandZcrTouchpadHaptics> zcr_touchpad_haptics_;
  std::unique_ptr<WaylandZwpPointerConstraints> zwp_pointer_constraints_;
  std::unique_ptr<WaylandZwpRelativePointerManager>
      zwp_relative_pointer_manager_;
  std::unique_ptr<WaylandZwpPointerGestures> zwp_pointer_gestures_;
  std::unique_ptr<WaylandSeat> seat_;
  std::unique_ptr<WaylandBufferManagerHost> buffer_manager_host_;
  std::unique_ptr<XdgActivation> xdg_activation_;
  std::unique_ptr<XdgForeignWrapper> xdg_foreign_;
  std::unique_ptr<ZwpIdleInhibitManager> zwp_idle_inhibit_manager_;
  std::unique_ptr<OverlayPrioritizer> overlay_prioritizer_;
  std::unique_ptr<SurfaceAugmenter> surface_augmenter_;
  std::unique_ptr<SinglePixelBuffer> single_pixel_buffer_;

  // Clipboard-related objects. |clipboard_| must be declared after all
  // DeviceManager instances it depends on, otherwise tests may crash with
  // UAFs while attempting to access already destroyed manager pointers.
  std::unique_ptr<GtkPrimarySelectionDeviceManager>
      gtk_primary_selection_device_manager_;
  std::unique_ptr<ZwpPrimarySelectionDeviceManager>
      zwp_primary_selection_device_manager_;
  std::unique_ptr<WaylandClipboard> clipboard_;

  std::unique_ptr<GtkShell1> gtk_shell1_;

  // Objects specific to KDE Plasma desktop environment.
  std::unique_ptr<OrgKdeKwinIdle> org_kde_kwin_idle_;

  std::unique_ptr<WaylandDataDragController> data_drag_controller_;
  std::unique_ptr<WaylandWindowDragController> window_drag_controller_;

  // Describes the clock domain that wp_presentation timestamps are in.
  uint32_t presentation_clk_id_ = CLOCK_MONOTONIC;

  // Allows input emulation access some data of objects that Wayland holds.
  // For example, wl_surface and others. It's only created when platform window
  // test config is set.
  std::unique_ptr<wl::WaylandProxy> wayland_proxy_;

  raw_ptr<WaylandCursorBufferListener> listener_ = nullptr;

  // The current window table mode layout state.
  display::TabletState tablet_layout_state_ =
      display::TabletState::kInClamshellMode;

  // Surfaces are submitted in pixel coordinates. Their buffer scales are always
  // advertised to server as 1, and the scale via vp_viewporter won't be
  // applied. The server will be responsible to scale the buffers to the right
  // sizes.
  bool surface_submission_in_pixel_coordinates_ = false;

  // This is set if wp_viewporter may be used to instruct the compositor to
  // properly scale fractional scaled surfaces.
  bool supports_viewporter_surface_scaling_ = false;

  wl::SerialTracker serial_tracker_;

  // Global Wayland interfaces available in the current session, with their
  // versions.
  std::vector<std::pair<std::string, uint32_t>> available_globals_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CONNECTION_H_

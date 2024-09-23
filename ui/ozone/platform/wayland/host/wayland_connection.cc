// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_connection.h"

#include <content-type-v1-client-protocol.h>
#include <extended-drag-unstable-v1-client-protocol.h>
#include <presentation-time-client-protocol.h>
#include <xdg-shell-client-protocol.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/task/current_thread.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/fractional_scale_manager.h"
#include "ui/ozone/platform/wayland/host/gtk_primary_selection_device_manager.h"
#include "ui/ozone/platform/wayland/host/gtk_shell1.h"
#include "ui/ozone/platform/wayland/host/org_kde_kwin_idle.h"
#include "ui/ozone/platform/wayland/host/overlay_prioritizer.h"
#include "ui/ozone/platform/wayland/host/proxy/wayland_proxy_impl.h"
#include "ui/ozone/platform/wayland/host/single_pixel_buffer.h"
#include "ui/ozone/platform/wayland/host/surface_augmenter.h"
#include "ui/ozone/platform/wayland/host/toplevel_icon_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_position.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_shape.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_drm.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_input_method_context.h"
#include "ui/ozone/platform/wayland/host/wayland_keyboard.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_shm.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_zaura_output_manager_v2.h"
#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_management_output.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_cursor_shapes.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_touchpad_haptics.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_linux_dmabuf.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_pointer_constraints.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_pointer_gestures.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_relative_pointer_manager.h"
#include "ui/ozone/platform/wayland/host/xdg_activation.h"
#include "ui/ozone/platform/wayland/host/xdg_foreign_wrapper.h"
#include "ui/ozone/platform/wayland/host/zwp_idle_inhibit_manager.h"
#include "ui/ozone/platform/wayland/host/zwp_primary_selection_device_manager.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/platform_window/common/platform_window_defaults.h"

namespace ui {

namespace {

// The maximum supported versions for a given interface.
// The version bound will be the minimum of the value and the version
// advertised by the server.
constexpr uint32_t kMaxCompositorVersion = 4;
constexpr uint32_t kMaxKeyboardExtensionVersion = 2;
constexpr uint32_t kMaxXdgShellVersion = 5;
constexpr uint32_t kMaxWpPresentationVersion = 1;
constexpr uint32_t kMaxWpViewporterVersion = 1;
constexpr uint32_t kMaxTextInputManagerV1Version = 1;
constexpr uint32_t kMaxTextInputManagerV3Version = 1;
constexpr uint32_t kMaxTextInputExtensionVersion = 14;
constexpr uint32_t kMaxExplicitSyncVersion = 2;
constexpr uint32_t kMaxAlphaCompositingVersion = 1;
constexpr uint32_t kMaxXdgDecorationVersion = 1;
constexpr uint32_t kMaxExtendedDragVersion = 1;
constexpr uint32_t kMaxXdgToplevelDragVersion = 1;
constexpr uint32_t kMaxXdgOutputManagerVersion = 3;
constexpr uint32_t kMaxKeyboardShortcutsInhibitManagerVersion = 1;
constexpr uint32_t kMaxStylusVersion = 2;
constexpr uint32_t kMaxWpContentTypeVersion = 1;

int64_t ConvertTimespecToMicros(const struct timespec& ts) {
  // On 32-bit systems, the calculation cannot overflow int64_t.
  // 2**32 * 1000000 + 2**64 / 1000 < 2**63
  if (sizeof(ts.tv_sec) <= 4 && sizeof(ts.tv_nsec) <= 8) {
    int64_t result = ts.tv_sec;
    result *= base::Time::kMicrosecondsPerSecond;
    result += (ts.tv_nsec / base::Time::kNanosecondsPerMicrosecond);
    return result;
  }
  base::CheckedNumeric<int64_t> result(ts.tv_sec);
  result *= base::Time::kMicrosecondsPerSecond;
  result += (ts.tv_nsec / base::Time::kNanosecondsPerMicrosecond);
  return result.ValueOrDie();
}

int64_t ConvertTimespecResultToMicros(uint32_t tv_sec_hi,
                                      uint32_t tv_sec_lo,
                                      uint32_t tv_nsec) {
  base::CheckedNumeric<int64_t> result(tv_sec_hi);
  result <<= 32;
  result += tv_sec_lo;
  result *= base::Time::kMicrosecondsPerSecond;
  result += (tv_nsec / base::Time::kNanosecondsPerMicrosecond);
  return result.ValueOrDie();
}

}  // namespace

void ReportShellUMA(UMALinuxWaylandShell shell) {
  static std::set<UMALinuxWaylandShell> reported_shells;
  if (reported_shells.count(shell) > 0)
    return;
  base::UmaHistogramEnumeration("Linux.Wayland.Shell", shell);
  reported_shells.insert(shell);
}

WaylandConnection::WaylandConnection() = default;

WaylandConnection::~WaylandConnection() = default;

bool WaylandConnection::Initialize(bool use_threaded_polling) {
  // Register factories for classes that implement wl::GlobalObjectRegistrar<T>.
  // Keep alphabetical order for convenience.
  RegisterGlobalObjectFactory(FractionalScaleManager::kInterfaceName,
                              &FractionalScaleManager::Instantiate);
  RegisterGlobalObjectFactory(GtkPrimarySelectionDeviceManager::kInterfaceName,
                              &GtkPrimarySelectionDeviceManager::Instantiate);
  RegisterGlobalObjectFactory(GtkShell1::kInterfaceName,
                              &GtkShell1::Instantiate);
  RegisterGlobalObjectFactory(OrgKdeKwinIdle::kInterfaceName,
                              &OrgKdeKwinIdle::Instantiate);
  RegisterGlobalObjectFactory(OverlayPrioritizer::kInterfaceName,
                              &OverlayPrioritizer::Instantiate);
  RegisterGlobalObjectFactory(SinglePixelBuffer::kInterfaceName,
                              &SinglePixelBuffer::Instantiate);
  RegisterGlobalObjectFactory(SurfaceAugmenter::kInterfaceName,
                              &SurfaceAugmenter::Instantiate);
  RegisterGlobalObjectFactory(ToplevelIconManager::kInterfaceName,
                              &ToplevelIconManager::Instantiate);
  RegisterGlobalObjectFactory(WaylandZAuraOutputManagerV2::kInterfaceName,
                              &WaylandZAuraOutputManagerV2::Instantiate);
  RegisterGlobalObjectFactory(WaylandDataDeviceManager::kInterfaceName,
                              &WaylandDataDeviceManager::Instantiate);
  RegisterGlobalObjectFactory(WaylandDrm::kInterfaceName,
                              &WaylandDrm::Instantiate);
  RegisterGlobalObjectFactory(WaylandOutput::kInterfaceName,
                              &WaylandOutput::Instantiate);
  RegisterGlobalObjectFactory(WaylandSeat::kInterfaceName,
                              &WaylandSeat::Instantiate);
  RegisterGlobalObjectFactory(WaylandShm::kInterfaceName,
                              &WaylandShm::Instantiate);
  RegisterGlobalObjectFactory(WaylandZAuraShell::kInterfaceName,
                              &WaylandZAuraShell::Instantiate);
  if (features::IsLacrosColorManagementEnabled()) {
    RegisterGlobalObjectFactory(WaylandZcrColorManager::kInterfaceName,
                                &WaylandZcrColorManager::Instantiate);
  }
  RegisterGlobalObjectFactory(WaylandCursorShape::kInterfaceName,
                              &WaylandCursorShape::Instantiate);
  RegisterGlobalObjectFactory(WaylandZcrCursorShapes::kInterfaceName,
                              &WaylandZcrCursorShapes::Instantiate);
  RegisterGlobalObjectFactory(WaylandZcrTouchpadHaptics::kInterfaceName,
                              &WaylandZcrTouchpadHaptics::Instantiate);
  RegisterGlobalObjectFactory(WaylandZwpLinuxDmabuf::kInterfaceName,
                              &WaylandZwpLinuxDmabuf::Instantiate);
  RegisterGlobalObjectFactory(WaylandZwpPointerConstraints::kInterfaceName,
                              &WaylandZwpPointerConstraints::Instantiate);
  RegisterGlobalObjectFactory(WaylandZwpPointerGestures::kInterfaceName,
                              &WaylandZwpPointerGestures::Instantiate);
  RegisterGlobalObjectFactory(WaylandZwpRelativePointerManager::kInterfaceName,
                              &WaylandZwpRelativePointerManager::Instantiate);
  RegisterGlobalObjectFactory(XdgActivation::kInterfaceName,
                              &XdgActivation::Instantiate);
  RegisterGlobalObjectFactory(XdgForeignWrapper::kInterfaceNameV1,
                              &XdgForeignWrapper::Instantiate);
  RegisterGlobalObjectFactory(XdgForeignWrapper::kInterfaceNameV2,
                              &XdgForeignWrapper::Instantiate);
  RegisterGlobalObjectFactory(ZwpIdleInhibitManager::kInterfaceName,
                              &ZwpIdleInhibitManager::Instantiate);
  RegisterGlobalObjectFactory(ZwpPrimarySelectionDeviceManager::kInterfaceName,
                              &ZwpPrimarySelectionDeviceManager::Instantiate);

  display_.reset(wl_display_connect(nullptr));
  if (!display_) {
    PLOG(ERROR) << "Failed to connect to Wayland display";
    return false;
  }

  wrapped_display_.reset(
      reinterpret_cast<wl_proxy*>(wl_proxy_create_wrapper(display())));
  // Create a non-default event queue so that we wouldn't flush messages for
  // client applications.
  event_queue_.reset(wl_display_create_queue(display()));
  wl_proxy_set_queue(wrapped_display_.get(), event_queue_.get());

  registry_.reset(GetRegistry());
  if (!registry_) {
    LOG(ERROR) << "Failed to get Wayland registry";
    return false;
  }

  // UnitTest doesn't support threaded polling wayland event
  if (UseTestConfigForPlatformWindows()) {
    use_threaded_polling = false;
  }

  // Now that the connection with the display server has been properly
  // estabilished, initialize the event source and input objects.
  DCHECK(!event_source_);
  event_source_ = std::make_unique<WaylandEventSource>(
      display(), event_queue_.get(), window_manager(), this,
      use_threaded_polling);

  // Create the buffer factory before registry listener is set so that shm, drm,
  // zwp_linux_dmabuf objects are able to be stored.
  buffer_factory_ = std::make_unique<WaylandBufferFactory>();

  wl::RecordConnectionMetrics(display());

  static constexpr wl_registry_listener kRegistryListener = {
      .global = &OnGlobal,
      .global_remove = &OnGlobalRemove,
  };
  wl_registry_add_listener(registry_.get(), &kRegistryListener, this);

  // "To mark the end of the initial burst of events, the client can
  // use the wl_display.sync request immediately after calling
  // wl_display.get_registry."
  // https://gitlab.freedesktop.org/wayland/wayland/-/blob/main/protocol/wayland.xml
  //
  // `RoundTripQueue()` internally calls `wl_display_roundtrip_queue()`, which
  // blocks until wl_display.sync is done. Use it to ensure the required globals
  // are emitted.
  while (!WlGlobalsReady()) {
    RoundTripQueue();
  }

  // Some wl_globals emits important information when bound.
  // E.g. server version.
  // Synchronously wait for it as well.
  while (!WlObjectsReady()) {
    RoundTripQueue();
  }

  buffer_manager_host_ = std::make_unique<WaylandBufferManagerHost>(this);

  if (!compositor_) {
    LOG(ERROR) << "No wl_compositor object";
    return false;
  }
  if (!buffer_factory()->shm()) {
    LOG(ERROR) << "No wl_shm object";
    return false;
  }
  if (!shell_) {
    LOG(ERROR) << "No Wayland shell found";
    return false;
  }

  // When we are running tests with weston in headless mode, the seat is not
  // announced.
  if (!seat_)
    LOG(WARNING) << "No wl_seat object. The functionality may suffer.";

  if (UseTestConfigForPlatformWindows())
    wayland_proxy_ = std::make_unique<wl::WaylandProxyImpl>(this);
  return true;
}

void WaylandConnection::RoundTripQueue() {
  DCHECK(event_source_);
  DCHECK(event_queue_.get());
  event_source_->RoundTripQueue();
}

void WaylandConnection::SetShutdownCb(base::OnceCallback<void()> shutdown_cb) {
  event_source()->SetShutdownCb(std::move(shutdown_cb));
}

base::Version WaylandConnection::GetServerVersion() const {
  return zaura_shell()
             ? zaura_shell()->server_version().value_or(base::Version{})
             : base::Version{};
}

void WaylandConnection::SetPlatformCursor(wl_cursor* cursor_data,
                                          int buffer_scale) {
  if (!cursor_)
    return;
  cursor_->SetPlatformShape(cursor_data, buffer_scale);
}

void WaylandConnection::SetCursorBufferListener(
    WaylandCursorBufferListener* listener) {
  listener_ = listener;
  if (!cursor_)
    return;
  cursor_->set_listener(listener_);
}

void WaylandConnection::SetCursorBitmap(const std::vector<SkBitmap>& bitmaps,
                                        const gfx::Point& hotspot_in_dips,
                                        int buffer_scale) {
  if (!cursor_)
    return;
  cursor_->UpdateBitmap(bitmaps, hotspot_in_dips, buffer_scale);
}

bool WaylandConnection::IsDragInProgress() const {
  // An active drag requires a seat exist.
  return seat_ && data_device_manager_ &&
         data_device_manager_->GetDevice()->IsDragInProgress();
}

bool WaylandConnection::SupportsSetWindowGeometry() const {
  return !!shell_;
}

wl::Object<wl_surface> WaylandConnection::CreateSurface() {
  DCHECK(compositor_);
  return wl::Object<wl_surface>(
      wl_compositor_create_surface(compositor_.get()));
}

void WaylandConnection::RegisterGlobalObjectFactory(
    const char* interface_name,
    wl::GlobalObjectFactory factory) {
  // If we get duplicate interface names, something is seriously wrong.
  CHECK_EQ(global_object_factories_.count(interface_name), 0U);

  global_object_factories_[interface_name] = factory;
}

bool WaylandConnection::WlGlobalsReady() const {
  bool ready = !!compositor_;

  // Output manager must be able to instantiate a valid WaylandScreen when
  // requested by the upper layers.
  ready &= output_manager_ && output_manager_->IsOutputReady();

  return ready;
}

bool WaylandConnection::WlObjectsReady() const {
  DCHECK(WlGlobalsReady());

  bool ready = true;

  // Lacros requires server version synchronously for gpu init.
  if (zaura_shell_ && zaura_shell_get_version(zaura_shell_->wl_object()) >=
                          ZAURA_SHELL_COMPOSITOR_VERSION_SINCE_VERSION) {
    ready &= zaura_shell_->server_version().has_value();
  }

  return ready;
}

void WaylandConnection::Flush() {
  wl_display_flush(display());
}

void WaylandConnection::UpdateInputDevices() {
  DeviceHotplugEventObserver* observer = DeviceDataManager::GetInstance();
  observer->OnMouseDevicesUpdated(CreateMouseDevices());
  observer->OnKeyboardDevicesUpdated(CreateKeyboardDevices());
  observer->OnTouchscreenDevicesUpdated(CreateTouchscreenDevices());
  observer->OnDeviceListsComplete();
}

std::vector<InputDevice> WaylandConnection::CreateMouseDevices() const {
  std::vector<InputDevice> devices;
  if (const auto* pointer = seat_->pointer()) {
    devices.emplace_back(pointer->id(), InputDeviceType::INPUT_DEVICE_UNKNOWN,
                         "pointer");
  }
  return devices;
}

std::vector<KeyboardDevice> WaylandConnection::CreateKeyboardDevices() const {
  std::vector<KeyboardDevice> devices;
  if (const auto* keyboard = seat_->keyboard()) {
    devices.emplace_back(keyboard->id(), InputDeviceType::INPUT_DEVICE_UNKNOWN,
                         "keyboard");
  }
  return devices;
}

std::vector<TouchscreenDevice> WaylandConnection::CreateTouchscreenDevices()
    const {
  std::vector<TouchscreenDevice> devices;
  if (const auto* touch = seat_->touch()) {
    // Currently, there's no protocol on wayland to know how many touch points
    // are supported on the device. Just use a fixed number to tell Chrome
    // that there's some touch point available. Currently, 10, which is
    // derived from some ChromeOS devices.
    devices.emplace_back(touch->id(), InputDeviceType::INPUT_DEVICE_UNKNOWN,
                         "touch", gfx::Size(),
                         /*touch_points=*/10);
  }
  return devices;
}

void WaylandConnection::UpdateCursor() {
  if (auto* pointer = seat_->pointer()) {
    cursor_ = std::make_unique<WaylandCursor>(pointer, this);
    cursor_->set_listener(listener_);
    cursor_position_ = std::make_unique<WaylandCursorPosition>();

    // Pointer is required for PointerGestures to be functional.
    if (zwp_pointer_gestures_) {
      zwp_pointer_gestures_->Init();
    }
  } else {
    cursor_.reset();
    cursor_position_.reset();
  }
}

void WaylandConnection::CreateDataObjectsIfReady() {
  if (data_device_manager_ && seat_) {
    DCHECK(!data_drag_controller_);
    data_drag_controller_ = std::make_unique<WaylandDataDragController>(
        this, data_device_manager_.get(), event_source(), event_source());

    DCHECK(!window_drag_controller_);
    window_drag_controller_ = std::make_unique<WaylandWindowDragController>(
        this, data_device_manager_.get(), event_source(), event_source(),
        event_source());

    DCHECK(!clipboard_);
    clipboard_ =
        std::make_unique<WaylandClipboard>(this, data_device_manager_.get());
  }
}

base::TimeTicks WaylandConnection::ConvertPresentationTime(uint32_t tv_sec_hi,
                                                           uint32_t tv_sec_lo,
                                                           uint32_t tv_nsec) {
  DCHECK(presentation());
  // base::TimeTicks::Now() uses CLOCK_MONOTONIC, no need to convert clock
  // domain if wp_presentation also uses it.
  if (presentation_clk_id_ == CLOCK_MONOTONIC) {
    return base::TimeTicks() + base::Microseconds(ConvertTimespecResultToMicros(
                                   tv_sec_hi, tv_sec_lo, tv_nsec));
  }

  struct timespec presentation_now;
  base::TimeTicks now = base::TimeTicks::Now();
  int ret = clock_gettime(presentation_clk_id_, &presentation_now);

  if (ret < 0) {
    presentation_now.tv_sec = 0;
    presentation_now.tv_nsec = 0;
    PLOG(ERROR) << "Error: failure to read the wp_presentation clock "
                << presentation_clk_id_;
    return base::TimeTicks::Now();
  }

  int64_t delta_us =
      ConvertTimespecResultToMicros(tv_sec_hi, tv_sec_lo, tv_nsec) -
      ConvertTimespecToMicros(presentation_now);

  return now + base::Microseconds(delta_us);
}

const gfx::PointF WaylandConnection::MaybeConvertLocation(
    const gfx::PointF& location,
    const WaylandWindow* window) const {
  if (!surface_submission_in_pixel_coordinates_ || !window)
    return location;
  gfx::PointF converted(location);
  converted.InvScale(window->applied_state().window_scale);
  return converted;
}

void WaylandConnection::DumpState(std::ostream& out) const {
  out << "available globals:";
  for (const auto& pair : available_globals_) {
    out << pair.first << ',';
  }
  out << std::endl;

  if (event_source_) {
    event_source_->DumpState(out);
    out << std::endl;
  }
  window_manager_.DumpState(out);
  out << std::endl;

  if (window_drag_controller_) {
    window_drag_controller_->DumpState(out);
    out << std::endl;
  }

  if (data_drag_controller_) {
    data_drag_controller_->DumpState(out);
    out << std::endl;
  }

  if (cursor_position_) {
    cursor_position_->DumpState(out);
    out << std::endl;
  }

  if (output_manager_) {
    output_manager_->DumpState(out);
    out << std::endl;
  }

  if (zaura_output_manager_v2_) {
    zaura_output_manager_v2_->DumpState(out);
    out << std::endl;
  }
}

bool WaylandConnection::ShouldUseOverlayDelegation() const {
  // Since using fractional_scale_v1 requires using viewport to rescale the
  // window to Wayland logical coordinates, using overlays in conjunction with
  // fractional_scale_v1 would require support for subpixel viewport
  // destination sizes and subpixel subsurface positions, which currently
  // isn't present on any non-exo Wayland compositors.
  bool should_use_overlay_delegation =
      IsWaylandOverlayDelegationEnabled() && !fractional_scale_manager_v1();
#if BUILDFLAG(IS_LINUX)
  // Overlay delegation also requires a single-pixel-buffer protocol, which
  // allows creation of non-backed solid color buffers. Even though only video
  // overlays can be supported on Linux, these color buffers are still needed
  // due to a peculiarity of the design of the Ozone/Wayland with the
  // WaylandOverlayDelegation feature enabled, which implies usage of a
  // transparent background buffer for a root surface while the content itself
  // is attached to a subsurface.
  should_use_overlay_delegation &= !!single_pixel_buffer();
#endif
  return should_use_overlay_delegation;
}

bool WaylandConnection::IsUsingZAuraOutputManager() const {
  return !!zaura_output_manager_v2_;
}

// static
void WaylandConnection::OnGlobal(void* data,
                                 wl_registry* registry,
                                 uint32_t name,
                                 const char* interface,
                                 uint32_t version) {
  auto* self = static_cast<WaylandConnection*>(data);
  DCHECK(self);
  self->HandleGlobal(registry, name, interface, version);
}

// static
void WaylandConnection::OnGlobalRemove(void* data,
                                       wl_registry* registry,
                                       uint32_t name) {
  auto* self = static_cast<WaylandConnection*>(data);

  if (self->zaura_output_manager_v2_) {
    // Removal will be handled by the aura output manager and the end of the
    // output-change transaction.
    self->zaura_output_manager_v2_->ScheduleRemoveWaylandOutput(name);
    return;
  }
  // The Wayland protocol distinguishes global objects by unique numeric names,
  // which the WaylandOutputManager uses as unique output ids. But, it is only
  // possible to figure out, what global object is going to be removed on the
  // WaylandConnection::GlobalRemove call. Thus, whatever unique |name| comes,
  // it's forwarded to the WaylandOutputManager, which checks if such a global
  // output object exists and removes it.
  if (self->output_manager_) {
    self->output_manager_->RemoveWaylandOutput(name);
  }
}

// static
void WaylandConnection::OnPing(void* data,
                               xdg_wm_base* shell,
                               uint32_t serial) {
  auto* connection = static_cast<WaylandConnection*>(data);
  xdg_wm_base_pong(shell, serial);
  connection->Flush();
}

// static
void WaylandConnection::OnClockId(void* data,
                                  wp_presentation* presentation,
                                  uint32_t clk_id) {
  DCHECK_EQ(base::TimeTicks::GetClock(),
            base::TimeTicks::Clock::LINUX_CLOCK_MONOTONIC);
  auto* connection = static_cast<WaylandConnection*>(data);
  connection->presentation_clk_id_ = clk_id;
}

void WaylandConnection::HandleGlobal(wl_registry* registry,
                                     uint32_t name,
                                     const char* interface,
                                     uint32_t version) {
  auto factory_it = global_object_factories_.find(interface);
  if (factory_it != global_object_factories_.end()) {
    (*factory_it->second)(this, registry, name, interface, version);
  } else if (!compositor_ && strcmp(interface, "wl_compositor") == 0) {
    compositor_ = wl::Bind<wl_compositor>(
        registry, name, std::min(version, kMaxCompositorVersion));
    compositor_version_ = version;
    if (!compositor_) {
      LOG(ERROR) << "Failed to bind to wl_compositor global";
      return;
    }
  } else if (!subcompositor_ && strcmp(interface, "wl_subcompositor") == 0) {
    subcompositor_ = wl::Bind<wl_subcompositor>(registry, name, 1);
    if (!subcompositor_) {
      LOG(ERROR) << "Failed to bind to wl_subcompositor global";
      return;
    }
  } else if (!shell_ && strcmp(interface, "xdg_wm_base") == 0) {
    shell_ = wl::Bind<xdg_wm_base>(registry, name,
                                   std::min(version, kMaxXdgShellVersion));
    if (!shell_) {
      LOG(ERROR) << "Failed to bind to xdg_wm_base global";
      return;
    }
    static constexpr xdg_wm_base_listener kShellBaseListener = {
        .ping = &OnPing,
    };
    xdg_wm_base_add_listener(shell_.get(), &kShellBaseListener, this);
    ReportShellUMA(UMALinuxWaylandShell::kXdgWmBase);
  } else if (!alpha_compositing_ &&
             (strcmp(interface, "zcr_alpha_compositing_v1") == 0)) {
    alpha_compositing_ = wl::Bind<zcr_alpha_compositing_v1>(
        registry, name, std::min(version, kMaxAlphaCompositingVersion));
    if (!alpha_compositing_) {
      LOG(ERROR) << "Failed to bind zcr_alpha_compositing_v1";
      return;
    }
  } else if (!linux_explicit_synchronization_ &&
             (strcmp(interface, "zwp_linux_explicit_synchronization_v1") ==
              0)) {
    linux_explicit_synchronization_ =
        wl::Bind<zwp_linux_explicit_synchronization_v1>(
            registry, name, std::min(version, kMaxExplicitSyncVersion));
    if (!linux_explicit_synchronization_) {
      LOG(ERROR) << "Failed to bind zwp_linux_explicit_synchronization_v1";
      return;
    }
  } else if (!content_type_manager_v1_ &&
             (strcmp(interface, "wp_content_type_manager_v1") == 0)) {
    content_type_manager_v1_ = wl::Bind<wp_content_type_manager_v1>(
        registry, name, std::min(version, kMaxWpContentTypeVersion));
    if (!content_type_manager_v1_) {
      LOG(ERROR) << "Failed to bind wp_content_type_v1";
      return;
    }
  } else if (!presentation_ && (strcmp(interface, "wp_presentation") == 0)) {
    presentation_ = wl::Bind<wp_presentation>(
        registry, name, std::min(version, kMaxWpPresentationVersion));
    if (!presentation_) {
      LOG(ERROR) << "Failed to bind wp_presentation";
      return;
    }
    static constexpr wp_presentation_listener kPresentationListener = {
        .clock_id = &OnClockId,
    };
    wp_presentation_add_listener(presentation_.get(), &kPresentationListener,
                                 this);
  } else if (!viewporter_ && (strcmp(interface, "wp_viewporter") == 0)) {
    viewporter_ = wl::Bind<wp_viewporter>(
        registry, name, std::min(version, kMaxWpViewporterVersion));
    if (!viewporter_) {
      LOG(ERROR) << "Failed to bind wp_viewporter";
      return;
    }
  } else if (!keyboard_extension_v1_ &&
             strcmp(interface, "zcr_keyboard_extension_v1") == 0) {
    keyboard_extension_v1_ = wl::Bind<zcr_keyboard_extension_v1>(
        registry, name, std::min(version, kMaxKeyboardExtensionVersion));
    if (!keyboard_extension_v1_) {
      LOG(ERROR) << "Failed to bind zcr_keyboard_extension_v1";
      return;
    }
    // CreateKeyboard may fail if we do not have keyboard seat capabilities yet.
    // We will create the keyboard when get them in that case.
    if (seat_) {
      seat_->RefreshKeyboard();
    }
  } else if (!keyboard_shortcuts_inhibit_manager_v1_ &&
             strcmp(interface, "zwp_keyboard_shortcuts_inhibit_manager_v1") ==
                 0) {
    keyboard_shortcuts_inhibit_manager_v1_ =
        wl::Bind<zwp_keyboard_shortcuts_inhibit_manager_v1>(
            registry, name,
            std::min(version, kMaxKeyboardShortcutsInhibitManagerVersion));
    if (!keyboard_shortcuts_inhibit_manager_v1_) {
      LOG(ERROR) << "Failed to bind zwp_keyboard_shortcuts_inhibit_manager_v1";
      return;
    }
  } else if (!text_input_manager_v1_ &&
             strcmp(interface, "zwp_text_input_manager_v1") == 0) {
    text_input_manager_v1_ = wl::Bind<zwp_text_input_manager_v1>(
        registry, name, std::min(version, kMaxTextInputManagerV1Version));
    if (!text_input_manager_v1_) {
      LOG(ERROR) << "Failed to bind to zwp_text_input_manager_v1 global";
      return;
    }
  } else if (!text_input_extension_v1_ &&
             strcmp(interface, "zcr_text_input_extension_v1") == 0) {
    text_input_extension_v1_ = wl::Bind<zcr_text_input_extension_v1>(
        registry, name, std::min(version, kMaxTextInputExtensionVersion));
  } else if (!text_input_manager_v3_ &&
             strcmp(interface, "zwp_text_input_manager_v3") == 0) {
    text_input_manager_v3_ = wl::Bind<zwp_text_input_manager_v3>(
        registry, name, std::min(version, kMaxTextInputManagerV3Version));
    if (!text_input_manager_v3_) {
      LOG(ERROR) << "Failed to bind to zwp_text_input_manager_v3 global";
      return;
    }
  } else if (!xdg_decoration_manager_ &&
             strcmp(interface, "zxdg_decoration_manager_v1") == 0) {
    xdg_decoration_manager_ = wl::Bind<zxdg_decoration_manager_v1>(
        registry, name, std::min(version, kMaxXdgDecorationVersion));
    if (!xdg_decoration_manager_) {
      LOG(ERROR) << "Failed to bind zxdg_decoration_manager_v1";
      return;
    }
  } else if (!extended_drag_v1_ &&
             strcmp(interface, "zcr_extended_drag_v1") == 0) {
    extended_drag_v1_ = wl::Bind<zcr_extended_drag_v1>(
        registry, name, std::min(version, kMaxExtendedDragVersion));
    if (!extended_drag_v1_) {
      LOG(ERROR) << "Failed to bind to zcr_extended_drag_v1 global";
      return;
    }
  } else if (!xdg_toplevel_drag_manager_v1_ &&
             strcmp(interface, "xdg_toplevel_drag_manager_v1") == 0 &&
             IsWaylandXdgToplevelDragEnabled()) {
    xdg_toplevel_drag_manager_v1_ = wl::Bind<::xdg_toplevel_drag_manager_v1>(
        registry, name, std::min(version, kMaxXdgToplevelDragVersion));
    if (!xdg_toplevel_drag_manager_v1_) {
      LOG(ERROR) << "Failed to bind to xdg_toplevel_drag_manager_v1 global";
      return;
    }
  } else if (!xdg_output_manager_ &&
             strcmp(interface, "zxdg_output_manager_v1") == 0) {
    // Responsibilities of zxdg_output_manager_v1 have been subsumed into the
    // zaura output manager extensions. If using the either extensions avoid
    // binding unnecessarily.
    if (IsUsingZAuraOutputManager()) {
      LOG(WARNING) << "Skipping bind to zxdg_output_manager_v1";
      return;
    }
    xdg_output_manager_ = wl::Bind<zxdg_output_manager_v1>(
        registry, name, std::min(version, kMaxXdgOutputManagerVersion));
    if (!xdg_output_manager_) {
      LOG(ERROR) << "Failed to bind zxdg_output_manager_v1";
      return;
    }
    if (output_manager_) {
      output_manager_->InitializeAllXdgOutputs();
    }
  } else if (strcmp(interface, "org_kde_plasma_shell") == 0) {
    // Recognized but not yet supported.
    NOTIMPLEMENTED_LOG_ONCE();
    ReportShellUMA(UMALinuxWaylandShell::kOrgKdePlasmaShell);
  } else if (strcmp(interface, "zwlr_layer_shell_v1") == 0) {
    // Recognized but not yet supported.
    NOTIMPLEMENTED_LOG_ONCE();
    ReportShellUMA(UMALinuxWaylandShell::kZwlrLayerShellV1);
  } else if (!zcr_stylus_v2_ && strcmp(interface, "zcr_stylus_v2") == 0) {
    zcr_stylus_v2_ = wl::Bind<zcr_stylus_v2>(
        registry, name, std::min(version, kMaxStylusVersion));
    if (!zcr_stylus_v2_) {
      LOG(ERROR) << "Failed to bind to zcr_stylus_v2";
      return;
    }
  }

  available_globals_.emplace_back(interface, version);
  Flush();
}

struct wl_callback* WaylandConnection::GetSyncCallback() {
  // We use display_wrapped here since we create all the objects against this
  // display, and the default one is responsible for a different event queue.
  return wl_display_sync(display_wrapper());
}

gl::EGLDisplayPlatform WaylandConnection::GetNativeDisplay() {
  return gl::EGLDisplayPlatform(
      reinterpret_cast<EGLNativeDisplayType>(display()));
}

struct wl_registry* WaylandConnection::GetRegistry() {
  return wl_display_get_registry(display_wrapper());
}

}  // namespace ui

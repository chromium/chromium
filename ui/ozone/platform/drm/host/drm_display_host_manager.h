// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_MANAGER_H_

#include <stdint.h>
#include <map>
#include <memory>

#include "base/containers/queue.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/events/ozone/device/device_event.h"
#include "ui/events/ozone/device/device_event_observer.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/ozone/platform/drm/common/display_types.h"
#include "ui/ozone/platform/drm/host/gpu_thread_observer.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {

class DeviceManager;
class DrmDisplayHost;
class DrmDisplayHostManager;
class DrmNativeDisplayDelegate;
class DrmWrapper;
class GpuThreadAdapter;

// The portion of the DrmDisplayHostManager implementation that is agnostic
// in how its communication with GPU-specific functionality is implemented.
// This is used from both the IPC and the in-process versions in  MUS.
class DrmDisplayHostManager : public DeviceEventObserver, GpuThreadObserver {
 public:
  DrmDisplayHostManager(
      GpuThreadAdapter* proxy,
      DeviceManager* device_manager,
      OzonePlatform::PlatformRuntimeProperties* host_properties,
      InputControllerEvdev* input_controller);

  DrmDisplayHostManager(const DrmDisplayHostManager&) = delete;
  DrmDisplayHostManager& operator=(const DrmDisplayHostManager&) = delete;

  ~DrmDisplayHostManager() override;

  DrmDisplayHost* GetDisplay(int64_t display_id);

  // External API.
  void AddDelegate(DrmNativeDisplayDelegate* delegate);
  void RemoveDelegate(DrmNativeDisplayDelegate* delegate);
  void TakeDisplayControl(display::DisplayControlCallback callback);
  void RelinquishDisplayControl(display::DisplayControlCallback callback);
  void UpdateDisplays(display::GetDisplaysCallback callback);
  void ConfigureDisplays(
      const std::vector<display::DisplayConfigurationParams>& config_requests,
      display::ConfigureCallback callback,
      display::ModesetFlags modeset_flags);

  // DeviceEventObserver overrides:
  void OnDeviceEvent(const DeviceEvent& event) override;

  // GpuThreadObserver overrides:
  void OnGpuProcessLaunched() override;
  void OnGpuThreadReady() override;
  void OnGpuThreadRetired() override;

  // Communication-free implementations of actions performed in response to
  // messages from the GPU thread.
  void GpuHasUpdatedNativeDisplays(MovableDisplaySnapshots displays);
  void GpuSetHdcpKeyProp(int64_t display_id, bool success);
  void GpuReceivedHDCPState(int64_t display_id,
                            bool status,
                            display::HDCPState state,
                            display::ContentProtectionMethod protection_method);
  void GpuUpdatedHDCPState(int64_t display_id, bool status);
  void GpuTookDisplayControl(bool status);
  void GpuRelinquishedDisplayControl(bool status);
  void GpuShouldDisplayEventTriggerConfiguration(bool should_trigger);

 private:
  struct DisplayEvent {
    DisplayEvent(DeviceEvent::ActionType action_type,
                 const base::FilePath& path,
                 const EventPropertyMap& properties);
    DisplayEvent(const DisplayEvent&);
    DisplayEvent& operator=(const DisplayEvent&);
    ~DisplayEvent();

    DeviceEvent::ActionType action_type;
    base::FilePath path;
    EventPropertyMap display_event_props;
  };

  // Handle hotplug events sequentially.
  void ProcessEvent();

  // Called as a result of finishing to process the display hotplug event. These
  // are responsible for dequing the event and scheduling the next event.
  void OnAddGraphicsDevice(const base::FilePath& path,
                           const base::FilePath& sysfs_path,
                           std::unique_ptr<DrmWrapper> drm);
  void OnUpdateGraphicsDevice(const EventPropertyMap& udev_event_props);
  void OnRemoveGraphicsDevice(const base::FilePath& path);

  void RunUpdateDisplaysCallback(display::GetDisplaysCallback callback) const;

  void NotifyDisplayDelegate() const;

  const raw_ptr<GpuThreadAdapter> proxy_;                 // Not owned.
  const raw_ptr<DeviceManager> device_manager_;           // Not owned.
  const raw_ptr<InputControllerEvdev> input_controller_;  // Not owned.

  raw_ptr<DrmNativeDisplayDelegate> delegate_ = nullptr;  // Not owned.

  // File path for the primary graphics card which is opened by default in the
  // GPU process. We'll avoid opening this in hotplug events since it will race
  // with the GPU process trying to open it and aquire DRM master.
  const base::FilePath primary_graphics_card_path_;

  // Keeps track if there is a dummy display. This happens on initialization
  // when there is no connection to the GPU to update the displays.
  bool has_dummy_display_ = false;

  std::vector<std::unique_ptr<DrmDisplayHost>> displays_;

  display::GetDisplaysCallback get_displays_callback_;

  bool display_externally_controlled_ = false;
  bool display_control_change_pending_ = false;
  display::DisplayControlCallback take_display_control_callback_;
  display::DisplayControlCallback relinquish_display_control_callback_;

  // Used to serialize display event processing. This is done since
  // opening/closing DRM devices cannot be done on the UI thread and are handled
  // on a worker thread. Thus, we need to queue events in order to process them
  // in the correct order.
  base::queue<DisplayEvent> event_queue_;

  // True if a display event is currently being processed on a worker thread.
  bool task_pending_ = false;

  // Keeps track of all the active DRM devices. The key is the device path, the
  // value is the sysfs path which has been resolved from the device path.
  std::map<base::FilePath, base::FilePath> drm_devices_;

  // This is used to cache the primary DRM device until the channel is
  // established.
  std::unique_ptr<DrmWrapper> primary_drm_device_;

  // Used to disable input devices when the display is externally controlled.
  std::unique_ptr<ScopedDisableInputDevices> scoped_input_devices_disabler_;

  base::WeakPtrFactory<DrmDisplayHostManager> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_MANAGER_H_

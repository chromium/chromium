// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/host_drm_device.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/ozone/platform/drm/common/display_types.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/host/drm_device_connector.h"
#include "ui/ozone/platform/drm/host/drm_display_host_manager.h"
#include "ui/ozone/platform/drm/host/host_cursor_proxy.h"

namespace ui {

HostDrmDevice::HostDrmDevice(DrmCursor* cursor) : cursor_(cursor) {}

HostDrmDevice::~HostDrmDevice() {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  for (GpuThreadObserver& observer : gpu_thread_observers_)
    observer.OnGpuThreadRetired();
}

void HostDrmDevice::OnDrmServiceStarted() {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  DCHECK(!connected_);

  connected_ = true;

  for (GpuThreadObserver& observer : gpu_thread_observers_)
    observer.OnGpuThreadReady();

  DCHECK(cursor_proxy_)
      << "We should have already created a cursor proxy previously";
  cursor_->SetDrmCursorProxy(std::move(cursor_proxy_));

  // TODO(rjkroege): Call ResetDrmCursorProxy when the mojo connection to the
  // DRM thread is broken.
}

void HostDrmDevice::SetDisplayManager(DrmDisplayHostManager* display_manager) {
  display_manager_ = display_manager;
}

void HostDrmDevice::AddGpuThreadObserver(GpuThreadObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  gpu_thread_observers_.AddObserver(observer);
  if (IsConnected())
    observer->OnGpuThreadReady();
}

void HostDrmDevice::RemoveGpuThreadObserver(GpuThreadObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  gpu_thread_observers_.RemoveObserver(observer);
}

bool HostDrmDevice::IsConnected() {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);

  // TODO(rjkroege): Need to set to connected_ to false when we lose the Viz
  // process connection.
  return connected_;
}

// Services needed for DrmDisplayHostMananger.
void HostDrmDevice::RegisterHandlerForDrmDisplayHostManager(
    DrmDisplayHostManager* handler) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  display_manager_ = handler;
}

void HostDrmDevice::UnRegisterHandlerForDrmDisplayHostManager() {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  display_manager_ = nullptr;
}

bool HostDrmDevice::GpuCreateWindow(gfx::AcceleratedWidget widget,
                                    const gfx::Rect& initial_bounds) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;

  drm_device_->CreateWindow(widget, initial_bounds);
  return true;
}

bool HostDrmDevice::GpuDestroyWindow(gfx::AcceleratedWidget widget) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;

  drm_device_->DestroyWindow(widget);
  return true;
}

bool HostDrmDevice::GpuWindowBoundsChanged(gfx::AcceleratedWidget widget,
                                           const gfx::Rect& bounds) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;

  drm_device_->SetWindowBounds(widget, bounds);

  return true;
}

bool HostDrmDevice::GpuRefreshNativeDisplays() {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;

  auto callback =
      base::BindOnce(&HostDrmDevice::GpuRefreshNativeDisplaysCallback, this);
  drm_device_->RefreshNativeDisplays(std::move(callback));

  return true;
}

void HostDrmDevice::GpuConfigureNativeDisplays(
    const std::vector<display::DisplayConfigurationParams>& config_requests,
    display::ConfigureCallback callback,
    uint32_t modeset_flag) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (IsConnected()) {
    drm_device_->ConfigureNativeDisplays(config_requests, modeset_flag,
                                         std::move(callback));
  } else {
    // Post this task to protect the callstack from accumulating too many
    // recursive calls to ConfigureDisplaysTask::Run() in cases in which the GPU
    // process crashes repeatedly.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
  }
}

bool HostDrmDevice::GpuTakeDisplayControl() {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;
  auto callback =
      base::BindOnce(&HostDrmDevice::GpuTakeDisplayControlCallback, this);

  drm_device_->TakeDisplayControl(std::move(callback));

  return true;
}

bool HostDrmDevice::GpuRelinquishDisplayControl() {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;
  auto callback =
      base::BindOnce(&HostDrmDevice::GpuRelinquishDisplayControlCallback, this);

  drm_device_->RelinquishDisplayControl(std::move(callback));

  return true;
}

void HostDrmDevice::GpuAddGraphicsDevice(const base::FilePath& path,
                                         base::ScopedFD fd) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!drm_device_.is_bound())
    return;

  base::File file(std::move(fd));
  drm_device_->AddGraphicsDevice(path, std::move(file));
}

bool HostDrmDevice::GpuRemoveGraphicsDevice(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;

  drm_device_->RemoveGraphicsDevice(std::move(path));

  return true;
}

void HostDrmDevice::GpuShouldDisplayEventTriggerConfiguration(
    const EventPropertyMap& event_props) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);

  // No connection to DRM device. Block the event since the entire configuration
  // will most likely fail.
  if (!IsConnected()) {
    GpuShouldDisplayEventTriggerConfigurationCallback(/*should_trigger=*/false);
    return;
  }

  auto callback = base::BindOnce(
      &HostDrmDevice::GpuShouldDisplayEventTriggerConfigurationCallback, this);
  drm_device_->ShouldDisplayEventTriggerConfiguration(event_props,
                                                      std::move(callback));
}

bool HostDrmDevice::GpuGetHDCPState(int64_t display_id) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;
  auto callback = base::BindOnce(&HostDrmDevice::GpuGetHDCPStateCallback, this);

  drm_device_->GetHDCPState(display_id, std::move(callback));

  return true;
}

bool HostDrmDevice::GpuSetHDCPState(
    int64_t display_id,
    display::HDCPState state,
    display::ContentProtectionMethod protection_method) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;
  auto callback = base::BindOnce(&HostDrmDevice::GpuSetHDCPStateCallback, this);

  drm_device_->SetHDCPState(display_id, state, protection_method,
                            std::move(callback));

  return true;
}

bool HostDrmDevice::GpuSetColorMatrix(int64_t display_id,
                                      const std::vector<float>& color_matrix) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;

  drm_device_->SetColorMatrix(display_id, color_matrix);
  return true;
}

bool HostDrmDevice::GpuSetGammaCorrection(
    int64_t display_id,
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;

  drm_device_->SetGammaCorrection(display_id, degamma_lut, gamma_lut);
  return true;
}

void HostDrmDevice::GpuSetPrivacyScreen(
    int64_t display_id,
    bool enabled,
    display::SetPrivacyScreenCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (IsConnected()) {
    drm_device_->SetPrivacyScreen(display_id, enabled, std::move(callback));
  } else {
    // There's no connection to the DRM device, so trigger Chrome's callback
    // with a failed state.
    std::move(callback).Run(/*success=*/false);
  }
}

void HostDrmDevice::GpuRefreshNativeDisplaysCallback(
    MovableDisplaySnapshots displays) const {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  display_manager_->GpuHasUpdatedNativeDisplays(std::move(displays));
}

void HostDrmDevice::GpuTakeDisplayControlCallback(bool success) const {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  display_manager_->GpuTookDisplayControl(success);
}

void HostDrmDevice::GpuRelinquishDisplayControlCallback(bool success) const {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  display_manager_->GpuRelinquishedDisplayControl(success);
}

void HostDrmDevice::GpuShouldDisplayEventTriggerConfigurationCallback(
    bool should_trigger) const {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  display_manager_->GpuShouldDisplayEventTriggerConfiguration(should_trigger);
}

void HostDrmDevice::GpuGetHDCPStateCallback(
    int64_t display_id,
    bool success,
    display::HDCPState state,
    display::ContentProtectionMethod protection_method) const {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  display_manager_->GpuReceivedHDCPState(display_id, success, state,
                                         protection_method);
}

void HostDrmDevice::GpuSetHDCPStateCallback(int64_t display_id,
                                            bool success) const {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  display_manager_->GpuUpdatedHDCPState(display_id, success);
}

void HostDrmDevice::OnGpuServiceLaunched(
    mojo::PendingRemote<ui::ozone::mojom::DrmDevice> drm_device) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);

  // We can get into this state if a new instance of GpuProcessHost is created
  // before the old one is destroyed.
  if (IsConnected())
    OnGpuServiceLost();

  drm_device_.Bind(std::move(drm_device));
  for (GpuThreadObserver& observer : gpu_thread_observers_)
    observer.OnGpuProcessLaunched();

  // Create two DeviceCursor connections: one for the UI thread and one for the
  // IO thread.
  mojo::PendingAssociatedRemote<ui::ozone::mojom::DeviceCursor> cursor_ui,
      cursor_io;
  drm_device_->GetDeviceCursor(cursor_ui.InitWithNewEndpointAndPassReceiver());
  drm_device_->GetDeviceCursor(cursor_io.InitWithNewEndpointAndPassReceiver());

  // The cursor is special since it will process input events on the IO thread
  // and can by-pass the UI thread. As a result, it has a Remote for both the UI
  // and I/O thread. cursor_io is already bound correctly to an I/O thread by
  // GpuProcessHost.
  cursor_proxy_ = std::make_unique<HostCursorProxy>(std::move(cursor_ui),
                                                    std::move(cursor_io));

  OnDrmServiceStarted();
}

void HostDrmDevice::OnGpuServiceLost() {
  cursor_proxy_.reset();
  connected_ = false;
  drm_device_.reset();
  // TODO(rjkroege): OnGpuThreadRetired is not currently used.
  for (GpuThreadObserver& observer : gpu_thread_observers_)
    observer.OnGpuThreadRetired();
}

}  // namespace ui

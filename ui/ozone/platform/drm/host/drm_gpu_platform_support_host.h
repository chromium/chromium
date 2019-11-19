// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_GPU_PLATFORM_SUPPORT_HOST_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_GPU_PLATFORM_SUPPORT_HOST_H_

#include <vector>

#include "base/callback.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/platform/drm/host/gpu_thread_adapter.h"
#include "ui/ozone/public/gpu_platform_support_host.h"

namespace gfx {
class Point;
}

namespace ui {

class DrmCursor;
class DrmDisplayHostMananger;
class DrmOverlayManagerHost;
class GpuThreadObserver;

class DrmGpuPlatformSupportHost : public GpuPlatformSupportHost,
                                  public GpuThreadAdapter,
                                  public IPC::Sender {
 public:
  explicit DrmGpuPlatformSupportHost(DrmCursor* cursor);
  ~DrmGpuPlatformSupportHost() override;

  // GpuPlatformSupportHost:
  void OnGpuProcessLaunched(
      int host_id,
      scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
      scoped_refptr<base::SingleThreadTaskRunner> send_runner,
      base::RepeatingCallback<void(IPC::Message*)> send_callback) override;
  void OnChannelDestroyed(int host_id) override;
  void OnGpuServiceLaunched(
      int host_id,
      scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_runner,
      GpuHostBindInterfaceCallback binder,
      GpuHostTerminateCallback terminate_callback) override;

  void OnMessageReceived(const IPC::Message& message) override;

  // IPC::Sender:
  bool Send(IPC::Message* message) override;

  // GpuThreadAdapter.
  // Core functionality.
  void AddGpuThreadObserver(GpuThreadObserver* observer) override;
  void RemoveGpuThreadObserver(GpuThreadObserver* observer) override;
  bool IsConnected() override;

  // Services needed for DrmDisplayHostMananger.
  void RegisterHandlerForDrmDisplayHostManager(
      DrmDisplayHostManager* handler) override;
  void UnRegisterHandlerForDrmDisplayHostManager() override;

  bool GpuTakeDisplayControl() override;
  bool GpuRefreshNativeDisplays() override;
  bool GpuRelinquishDisplayControl() override;
  bool GpuAddGraphicsDeviceOnUIThread(const base::FilePath& path,
                                      base::ScopedFD fd) override;
  void GpuAddGraphicsDeviceOnIOThread(const base::FilePath& path,
                                      base::ScopedFD fd) override;
  bool GpuRemoveGraphicsDevice(const base::FilePath& path) override;

  // Methods needed for DrmOverlayManagerHost.
  // Methods for DrmOverlayManagerHost.
  void RegisterHandlerForDrmOverlayManager(
      DrmOverlayManagerHost* handler) override;
  void UnRegisterHandlerForDrmOverlayManager() override;

  // Services needed by DrmOverlayManagerHost
  bool GpuCheckOverlayCapabilities(
      gfx::AcceleratedWidget widget,
      const OverlaySurfaceCandidateList& new_params) override;

  // Services needed by DrmDisplayHost
  bool GpuConfigureNativeDisplay(int64_t display_id,
                                 const ui::DisplayMode_Params& display_mode,
                                 const gfx::Point& point) override;
  bool GpuDisableNativeDisplay(int64_t display_id) override;
  bool GpuGetHDCPState(int64_t display_id) override;
  bool GpuSetHDCPState(int64_t display_id, display::HDCPState state) override;
  bool GpuSetColorMatrix(int64_t display_id,
                         const std::vector<float>& color_matrix) override;
  bool GpuSetGammaCorrection(
      int64_t display_id,
      const std::vector<display::GammaRampRGBEntry>& degamma_lut,
      const std::vector<display::GammaRampRGBEntry>& gamma_lut) override;

  // Services needed by DrmWindowHost
  bool GpuDestroyWindow(gfx::AcceleratedWidget widget) override;
  bool GpuCreateWindow(gfx::AcceleratedWidget widget,
                       const gfx::Rect& initial_bounds) override;
  bool GpuWindowBoundsChanged(gfx::AcceleratedWidget widget,
                              const gfx::Rect& bounds) override;

 private:
  void OnChannelEstablished();
  bool OnMessageReceivedForDrmDisplayHostManager(const IPC::Message& message);
  void OnUpdateNativeDisplays(
      const std::vector<DisplaySnapshot_Params>& displays);
  void OnDisplayConfigured(int64_t display_id, bool status);
  void OnHDCPStateReceived(int64_t display_id,
                           bool status,
                           display::HDCPState state);
  void OnHDCPStateUpdated(int64_t display_id, bool status);
  void OnTakeDisplayControl(bool status);
  void OnRelinquishDisplayControl(bool status);

  bool OnMessageReceivedForDrmOverlayManager(const IPC::Message& message);
  void OnOverlayResult(gfx::AcceleratedWidget widget,
                       const std::vector<OverlayCheck_Params>& params,
                       const std::vector<OverlayCheckReturn_Params>& returns);

  int host_id_ = -1;
  bool channel_established_ = false;

  scoped_refptr<base::SingleThreadTaskRunner> ui_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> send_runner_;
  base::RepeatingCallback<void(IPC::Message*)> send_callback_;

  DrmDisplayHostManager* display_manager_;  // Not owned.
  DrmOverlayManagerHost* overlay_manager_;  // Not owned.

  DrmCursor* const cursor_;  // Not owned.
  base::ObserverList<GpuThreadObserver>::Unchecked gpu_thread_observers_;

  base::WeakPtr<DrmGpuPlatformSupportHost> weak_ptr_;
  base::WeakPtrFactory<DrmGpuPlatformSupportHost> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(DrmGpuPlatformSupportHost);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_GPU_PLATFORM_SUPPORT_HOST_H_

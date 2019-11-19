// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_GPU_THREAD_ADAPTER_H_
#define UI_OZONE_PLATFORM_DRM_HOST_GPU_THREAD_ADAPTER_H_

#include "base/file_descriptor_posix.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace ui {

class DrmDisplayHostManager;
class DrmOverlayManagerHost;
class GpuThreadObserver;

// Provides the services that the various host components need
// to use either a GPU process or thread for their implementation.
class GpuThreadAdapter {
 public:
  virtual ~GpuThreadAdapter() {}

  virtual bool IsConnected() = 0;
  virtual void AddGpuThreadObserver(GpuThreadObserver* observer) = 0;
  virtual void RemoveGpuThreadObserver(GpuThreadObserver* observer) = 0;

  // Methods for Display management.
  virtual void RegisterHandlerForDrmDisplayHostManager(
      DrmDisplayHostManager* handler) = 0;
  virtual void UnRegisterHandlerForDrmDisplayHostManager() = 0;

  // Services needed for DrmDisplayHostMananger
  virtual bool GpuTakeDisplayControl() = 0;
  virtual bool GpuRefreshNativeDisplays() = 0;
  virtual bool GpuRelinquishDisplayControl() = 0;
  virtual bool GpuAddGraphicsDeviceOnUIThread(const base::FilePath& path,
                                              base::ScopedFD fd) = 0;
  virtual void GpuAddGraphicsDeviceOnIOThread(const base::FilePath& path,
                                              base::ScopedFD fd) = 0;
  virtual bool GpuRemoveGraphicsDevice(const base::FilePath& path) = 0;

  // Methods for DrmOverlayManagerHost.
  virtual void RegisterHandlerForDrmOverlayManager(
      DrmOverlayManagerHost* handler) = 0;
  virtual void UnRegisterHandlerForDrmOverlayManager() = 0;

  // Services needed by DrmOverlayManagerHost
  virtual bool GpuCheckOverlayCapabilities(
      gfx::AcceleratedWidget widget,
      const OverlaySurfaceCandidateList& overlays) = 0;

  // Services needed by DrmDisplayHost
  virtual bool GpuConfigureNativeDisplay(
      int64_t display_id,
      const ui::DisplayMode_Params& display_mode,
      const gfx::Point& point) = 0;
  virtual bool GpuDisableNativeDisplay(int64_t display_id) = 0;
  virtual bool GpuGetHDCPState(int64_t display_id) = 0;
  virtual bool GpuSetHDCPState(int64_t display_id,
                               display::HDCPState state) = 0;
  virtual bool GpuSetColorMatrix(int64_t display_id,
                                 const std::vector<float>& color_matrix) = 0;
  virtual bool GpuSetGammaCorrection(
      int64_t display_id,
      const std::vector<display::GammaRampRGBEntry>& degamma_lut,
      const std::vector<display::GammaRampRGBEntry>& gamma_lut) = 0;

  // Services needed by DrmWindowHost
  virtual bool GpuDestroyWindow(gfx::AcceleratedWidget widget) = 0;
  virtual bool GpuCreateWindow(gfx::AcceleratedWidget widget,
                               const gfx::Rect& initial_bounds) = 0;
  virtual bool GpuWindowBoundsChanged(gfx::AcceleratedWidget widget,
                                      const gfx::Rect& bounds) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_GPU_THREAD_ADAPTER_H_

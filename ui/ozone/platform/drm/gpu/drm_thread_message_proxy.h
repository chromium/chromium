// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_MESSAGE_PROXY_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_MESSAGE_PROXY_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ipc/message_filter.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/drm/common/display_types.h"
#include "ui/ozone/platform/drm/gpu/inter_thread_messaging_proxy.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace base {
struct FileDescriptor;
class FilePath;
}

namespace gfx {
class Point;
class Rect;
}

namespace ui {
class DrmThread;
struct DisplayMode_Params;
struct OverlayCheck_Params;

class DrmThreadMessageProxy : public IPC::MessageFilter,
                              public InterThreadMessagingProxy {
 public:
  DrmThreadMessageProxy();

  // InterThreadMessagingProxy.
  void SetDrmThread(DrmThread* thread) override;

  // IPC::MessageFilter:
  void OnFilterAdded(IPC::Channel* channel) override;
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  ~DrmThreadMessageProxy() override;

  void OnCreateWindow(gfx::AcceleratedWidget widget,
                      const gfx::Rect& initial_bounds);
  void OnDestroyWindow(gfx::AcceleratedWidget widget);
  void OnWindowBoundsChanged(gfx::AcceleratedWidget widget,
                             const gfx::Rect& bounds);
  void OnCursorSet(gfx::AcceleratedWidget widget,
                   const std::vector<SkBitmap>& bitmaps,
                   const gfx::Point& location,
                   int frame_delay_ms);
  void OnCursorMove(gfx::AcceleratedWidget widget, const gfx::Point& location);
  void OnCheckOverlayCapabilities(
      gfx::AcceleratedWidget widget,
      const std::vector<OverlayCheck_Params>& overlays);

  // Display related IPC handlers.
  void OnRefreshNativeDisplays();
  void OnConfigureNativeDisplay(int64_t id,
                                const DisplayMode_Params& mode,
                                const gfx::Point& origin);
  void OnDisableNativeDisplay(int64_t id);
  void OnTakeDisplayControl();
  void OnRelinquishDisplayControl();
  void OnAddGraphicsDevice(const base::FilePath& path,
                           const base::FileDescriptor& fd);
  void OnRemoveGraphicsDevice(const base::FilePath& path);
  void OnGetHDCPState(int64_t display_id);
  void OnSetHDCPState(int64_t display_id, display::HDCPState state);
  void OnSetColorMatrix(int64_t display_id,
                        const std::vector<float>& color_matrix);
  void OnSetGammaCorrection(
      int64_t display_id,
      const std::vector<display::GammaRampRGBEntry>& degamma_lut,
      const std::vector<display::GammaRampRGBEntry>& gamma_lut);

  void OnCheckOverlayCapabilitiesCallback(
      gfx::AcceleratedWidget widget,
      const OverlaySurfaceCandidateList& overlays,
      const OverlayStatusList& returns) const;
  void OnRefreshNativeDisplaysCallback(MovableDisplaySnapshots displays) const;
  void OnConfigureNativeDisplayCallback(int64_t display_id, bool success) const;
  void OnDisableNativeDisplayCallback(int64_t display_id, bool success) const;
  void OnTakeDisplayControlCallback(bool success) const;
  void OnRelinquishDisplayControlCallback(bool success) const;
  void OnGetHDCPStateCallback(int64_t display_id,
                              bool success,
                              display::HDCPState state) const;
  void OnSetHDCPStateCallback(int64_t display_id, bool success) const;

  DrmThread* drm_thread_ = nullptr;

  IPC::Sender* sender_ = nullptr;

  base::WeakPtrFactory<DrmThreadMessageProxy> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DrmThreadMessageProxy);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_MESSAGE_PROXY_H_

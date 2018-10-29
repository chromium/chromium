// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_WINDOW_PROXY_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_WINDOW_PROXY_H_

#include <vector>

#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/public/swap_completion_callback.h"

namespace ui {

class DrmThread;
struct DrmOverlayPlane;

class DrmWindowProxy {
 public:
  DrmWindowProxy(gfx::AcceleratedWidget widget, DrmThread* drm_thread);
  ~DrmWindowProxy();

  gfx::AcceleratedWidget widget() const { return widget_; }

  void SchedulePageFlip(std::vector<DrmOverlayPlane> planes,
                        SwapCompletionOnceCallback submission_callback,
                        PresentationOnceCallback presentation_callback);

  bool SupportsGpuFences() const;

 private:
  const gfx::AcceleratedWidget widget_;

  DrmThread* const drm_thread_;

  DISALLOW_COPY_AND_ASSIGN(DrmWindowProxy);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_WINDOW_PROXY_H_

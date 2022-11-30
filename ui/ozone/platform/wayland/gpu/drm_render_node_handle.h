// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_DRM_RENDER_NODE_HANDLE_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_DRM_RENDER_NODE_HANDLE_H_

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"

namespace ui {

// A wrapper around a DRM render node device handle.
class DrmRenderNodeHandle {
 public:
  DrmRenderNodeHandle();

  DrmRenderNodeHandle(const DrmRenderNodeHandle&) = delete;
  DrmRenderNodeHandle& operator=(const DrmRenderNodeHandle&) = delete;

  ~DrmRenderNodeHandle();

  bool Initialize(const base::FilePath& path);

  base::ScopedFD PassFD();

 private:
  base::ScopedFD drm_fd_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_DRM_RENDER_NODE_HANDLE_H_

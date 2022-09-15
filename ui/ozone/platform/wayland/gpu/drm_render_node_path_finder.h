// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_DRM_RENDER_NODE_PATH_FINDER_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_DRM_RENDER_NODE_PATH_FINDER_H_

#include "base/files/file_path.h"

namespace ui {

// A helper class that finds a DRM render node device and returns a path to it.
class DrmRenderNodePathFinder {
 public:
  // Triggers FindDrmRenderNodePath.
  DrmRenderNodePathFinder();

  DrmRenderNodePathFinder(const DrmRenderNodePathFinder&) = delete;
  DrmRenderNodePathFinder& operator=(const DrmRenderNodePathFinder&) = delete;

  ~DrmRenderNodePathFinder();

  // Returns a path to a drm render node device.
  base::FilePath GetDrmRenderNodePath() const;

 private:
  void FindDrmRenderNodePath();

  base::FilePath drm_render_node_path_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_DRM_RENDER_NODE_PATH_FINDER_H_

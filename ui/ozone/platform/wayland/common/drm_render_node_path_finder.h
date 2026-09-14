// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_COMMON_DRM_RENDER_NODE_PATH_FINDER_H_
#define UI_OZONE_PLATFORM_WAYLAND_COMMON_DRM_RENDER_NODE_PATH_FINDER_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"

namespace ui {

// Represents an inspected, viable DRM render node candidate.
struct DrmRenderNodeCandidate {
  std::string driver_name;
  base::FilePath path;
};

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

  // Pure business logic: selects the best DRM render node path from a list of
  // candidates based on driver preferences and fallbacks.
  static base::FilePath SelectRenderNodeFromCandidates(
      base::span<const DrmRenderNodeCandidate> candidates);

 private:
  void FindDrmRenderNodePath();

  base::FilePath drm_render_node_path_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_COMMON_DRM_RENDER_NODE_PATH_FINDER_H_

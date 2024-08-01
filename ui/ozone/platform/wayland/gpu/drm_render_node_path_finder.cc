// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/platform/wayland/gpu/drm_render_node_path_finder.h"

#include <fcntl.h>
#include <gbm.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include "base/files/scoped_file.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/linux/scoped_gbm_device.h"  // nogncheck

namespace ui {

namespace {

// Drm render node path template.
constexpr char kDriRenderNodeTemplate[] = "/dev/dri/renderD%u";

// Number of files to look for when discovering DRM devices.
constexpr uint32_t kDrmMajor = 226;
constexpr uint32_t kDrmMaxMinor = 15;
constexpr uint32_t kRenderNodeStart = 128;
constexpr uint32_t kRenderNodeEnd = kRenderNodeStart + kDrmMaxMinor + 1;

}  // namespace

DrmRenderNodePathFinder::DrmRenderNodePathFinder() {
  FindDrmRenderNodePath();
}

DrmRenderNodePathFinder::~DrmRenderNodePathFinder() = default;

base::FilePath DrmRenderNodePathFinder::GetDrmRenderNodePath() const {
  return drm_render_node_path_;
}

void DrmRenderNodePathFinder::FindDrmRenderNodePath() {
  for (uint32_t i = kRenderNodeStart; i < kRenderNodeEnd; i++) {
    /* First,  look in sysfs and skip if this is the vgem render node. */
    std::string node_link(
        base::StringPrintf("/sys/dev/char/%d:%d/device", kDrmMajor, i));
    char device_link[256];
    ssize_t len = readlink(node_link.c_str(), device_link, sizeof(device_link));
    if (len < 0 || len == sizeof(device_link))
      continue;

    // Convert device_link to a string for safe substring comparison.
    std::string device_link_str(device_link, len);

    // readlink does not place a nul byte at the end of the string.
    if (std::string(device_link, len).ends_with("vgem")) {
      continue;
    }

    std::string dri_render_node(base::StringPrintf(kDriRenderNodeTemplate, i));
    base::ScopedFD drm_fd(open(dri_render_node.c_str(), O_RDWR));
    if (drm_fd.get() < 0)
      continue;

    // In case the first node /dev/dri/renderD128 can be opened but fails to
    // create gbm device on certain driver (E.g. PowerVR). Skip such paths.
    ScopedGbmDevice device(gbm_create_device(drm_fd.get()));
    if (!device)
      continue;

    drm_render_node_path_ = base::FilePath(dri_render_node);
    break;
  }
}

}  // namespace ui

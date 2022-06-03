// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/drm_render_node_handle.h"

#include <fcntl.h>
#include <xf86drm.h>

#include "base/logging.h"

namespace ui {

DrmRenderNodeHandle::DrmRenderNodeHandle() = default;

DrmRenderNodeHandle::~DrmRenderNodeHandle() = default;

bool DrmRenderNodeHandle::Initialize(const base::FilePath& path) {
  base::ScopedFD drm_fd(open(path.value().c_str(), O_RDWR));
  if (drm_fd.get() < 0)
    return false;

  drmVersionPtr drm_version = drmGetVersion(drm_fd.get());
  if (!drm_version) {
    LOG(FATAL) << "Can't get version for device: '" << path << "'";
    return false;
  }

  drm_fd_ = std::move(drm_fd);
  return true;
}

base::ScopedFD DrmRenderNodeHandle::PassFD() {
  return std::move(drm_fd_);
}

}  // namespace ui

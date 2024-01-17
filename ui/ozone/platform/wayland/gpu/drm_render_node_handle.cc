// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/drm_render_node_handle.h"

#include <fcntl.h>
#include <xf86drm.h>

#include "base/logging.h"

namespace ui {

namespace {

struct DrmVersionDeleter {
  void operator()(drmVersion* version) const { drmFreeVersion(version); }
};

typedef std::unique_ptr<drmVersion, DrmVersionDeleter> ScopedDrmVersionPtr;

}  // namespace

DrmRenderNodeHandle::DrmRenderNodeHandle() = default;

DrmRenderNodeHandle::~DrmRenderNodeHandle() = default;

bool DrmRenderNodeHandle::Initialize(const base::FilePath& path) {
  base::ScopedFD drm_fd(open(path.value().c_str(), O_RDWR));
  if (drm_fd.get() < 0)
    return false;

  ScopedDrmVersionPtr version(drmGetVersion(drm_fd.get()));
  if (!version) {
    LOG(FATAL) << "Can't get version for device: '" << path << "'";
  }

  drm_fd_ = std::move(drm_fd);
  return true;
}

base::ScopedFD DrmRenderNodeHandle::PassFD() {
  return std::move(drm_fd_);
}

}  // namespace ui

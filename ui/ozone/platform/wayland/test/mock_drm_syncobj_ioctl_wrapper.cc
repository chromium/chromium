// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_drm_syncobj_ioctl_wrapper.h"

#include "base/posix/eintr_wrapper.h"

namespace ui {

// static
bool MockDrmSyncobjIoctlWrapper::fail_on_syncobj_create = false;
// static
bool MockDrmSyncobjIoctlWrapper::fail_on_syncobj_handle_to_fd = false;

MockDrmSyncobjIoctlWrapper::MockDrmSyncobjIoctlWrapper()
    : DrmSyncobjIoctlWrapper(base::ScopedFD()) {}

MockDrmSyncobjIoctlWrapper::~MockDrmSyncobjIoctlWrapper() = default;

int MockDrmSyncobjIoctlWrapper::SyncobjCreate(uint32_t flags,
                                              uint32_t* handle) {
  if (fail_on_syncobj_create) {
    return EIO;
  }
  *handle = ++handle_counter_;
  syncobjs_[handle_counter_] = fd_factory_.CreateFd();
  return 0;
}

int MockDrmSyncobjIoctlWrapper::SyncobjDestroy(uint32_t handle) {
  if (auto it = syncobjs_.find(handle); it != syncobjs_.end()) {
    syncobjs_.erase(it);
    return 0;
  }
  return ENOENT;
}

int MockDrmSyncobjIoctlWrapper::SyncobjHandleToFD(uint32_t handle,
                                                  int* obj_fd) {
  if (fail_on_syncobj_handle_to_fd) {
    return EIO;
  }
  if (auto it = syncobjs_.find(handle); it != syncobjs_.end()) {
    *obj_fd = HANDLE_EINTR(dup(it->second.get()));
    return 0;
  }
  return ENOENT;
}

}  // namespace ui

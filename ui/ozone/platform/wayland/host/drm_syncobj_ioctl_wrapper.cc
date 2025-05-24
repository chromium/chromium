// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/drm_syncobj_ioctl_wrapper.h"

#include <xf86drm.h>

namespace ui {

DrmSyncobjIoctlWrapper::DrmSyncobjIoctlWrapper(base::ScopedFD fd)
    : fd_(std::move(fd)) {}
DrmSyncobjIoctlWrapper::~DrmSyncobjIoctlWrapper() = default;

int DrmSyncobjIoctlWrapper::SyncobjCreate(uint32_t flags, uint32_t* handle) {
  return drmSyncobjCreate(fd_.get(), flags, handle);
}

int DrmSyncobjIoctlWrapper::SyncobjDestroy(uint32_t handle) {
  return drmSyncobjDestroy(fd_.get(), handle);
}

int DrmSyncobjIoctlWrapper::SyncobjTransfer(uint32_t dst_handle,
                                            uint64_t dst_point,
                                            uint32_t src_handle,
                                            uint64_t src_point,
                                            uint32_t flags) {
  return drmSyncobjTransfer(fd_.get(), dst_handle, dst_point, src_handle,
                            src_point, flags);
}

int DrmSyncobjIoctlWrapper::SyncobjHandleToFD(uint32_t handle, int* obj_fd) {
  return drmSyncobjHandleToFD(fd_.get(), handle, obj_fd);
}

int DrmSyncobjIoctlWrapper::SyncobjImportSyncFile(uint32_t handle,
                                                  int sync_file_fd) {
  return drmSyncobjImportSyncFile(fd_.get(), handle, sync_file_fd);
}

int DrmSyncobjIoctlWrapper::SyncobjExportSyncFile(uint32_t handle,
                                                  int* sync_file_fd) {
  return drmSyncobjExportSyncFile(fd_.get(), handle, sync_file_fd);
}

int DrmSyncobjIoctlWrapper::SyncobjEventfd(uint32_t handle,
                                           uint64_t point,
                                           int ev_fd,
                                           uint32_t flags) {
  return drmSyncobjEventfd(fd_.get(), handle, point, ev_fd, flags);
}
}  // namespace ui

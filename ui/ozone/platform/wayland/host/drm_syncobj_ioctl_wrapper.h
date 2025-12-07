// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_DRM_SYNCOBJ_IOCTL_WRAPPER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_DRM_SYNCOBJ_IOCTL_WRAPPER_H_

#include <cstdint>

#include "base/files/scoped_file.h"

namespace ui {

// Wrapper around libdrm ioctl APIs.
class DrmSyncobjIoctlWrapper {
 public:
  explicit DrmSyncobjIoctlWrapper(base::ScopedFD fd);
  virtual ~DrmSyncobjIoctlWrapper();

  DrmSyncobjIoctlWrapper(const DrmSyncobjIoctlWrapper&) = delete;
  DrmSyncobjIoctlWrapper& operator=(const DrmSyncobjIoctlWrapper&) = delete;

  virtual int SyncobjCreate(uint32_t flags, uint32_t* handle);
  virtual int SyncobjDestroy(uint32_t handle);
  virtual int SyncobjTransfer(uint32_t dst_handle,
                              uint64_t dst_point,
                              uint32_t src_handle,
                              uint64_t src_point,
                              uint32_t flags);
  virtual int SyncobjHandleToFD(uint32_t handle, int* obj_fd);
  virtual int SyncobjImportSyncFile(uint32_t handle, int sync_file_fd);
  virtual int SyncobjExportSyncFile(uint32_t handle, int* sync_file_fd);
  virtual int SyncobjEventfd(uint32_t handle,
                             uint64_t point,
                             int ev_fd,
                             uint32_t flags);

 private:
  base::ScopedFD fd_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_DRM_SYNCOBJ_IOCTL_WRAPPER_H_

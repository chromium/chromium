// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_DEVICE_HANDLE_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_DEVICE_HANDLE_H_

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"

namespace base {
class FilePath;
}

namespace ui {

class DrmDeviceHandle {
 public:
  DrmDeviceHandle();

  DrmDeviceHandle(const DrmDeviceHandle&) = delete;
  DrmDeviceHandle& operator=(const DrmDeviceHandle&) = delete;

  ~DrmDeviceHandle();

  int fd() const { return file_.get(); }
  const base::FilePath& sys_path() { return sys_path_; }
  bool has_atomic_capabilities() const { return has_atomic_capabilities_; }

  bool Initialize(const base::FilePath& dev_path,
                  const base::FilePath& sys_path);

  bool IsValid() const;
  base::ScopedFD PassFD();

 private:
  base::FilePath sys_path_;
  base::ScopedFD file_;
  bool has_atomic_capabilities_ = false;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_DEVICE_HANDLE_H_

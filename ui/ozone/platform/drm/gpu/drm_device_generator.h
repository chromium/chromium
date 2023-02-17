// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_DEVICE_GENERATOR_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_DEVICE_GENERATOR_H_

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"

namespace ui {

class DrmDevice;

class DrmDeviceGenerator {
 public:
  DrmDeviceGenerator();

  DrmDeviceGenerator(const DrmDeviceGenerator&) = delete;
  DrmDeviceGenerator& operator=(const DrmDeviceGenerator&) = delete;

  virtual ~DrmDeviceGenerator();

  // Creates a DRM device for |file|. |device_path| describes the location of
  // the DRM device.
  virtual scoped_refptr<DrmDevice> CreateDevice(
      const base::FilePath& device_path,
      base::ScopedFD fd,
      bool is_primary_device) = 0;

 public:
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_DEVICE_GENERATOR_H_

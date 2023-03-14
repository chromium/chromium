// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_FAKE_DRM_DEVICE_GENERATOR_H_
#define UI_OZONE_PLATFORM_DRM_GPU_FAKE_DRM_DEVICE_GENERATOR_H_

#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"

namespace ui {

class DrmDevice;

class FakeDrmDeviceGenerator : public DrmDeviceGenerator {
  // DrmDeviceGenerator:
  scoped_refptr<DrmDevice> CreateDevice(const base::FilePath& path,
                                        base::ScopedFD fd,
                                        bool is_primary_device) override;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_FAKE_DRM_DEVICE_GENERATOR_H_

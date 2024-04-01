// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/fake_drm_device_generator.h"

#include "ui/gfx/linux/test/mock_gbm_device.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device.h"

namespace ui {
scoped_refptr<DrmDevice> FakeDrmDeviceGenerator::CreateDevice(
    const base::FilePath& path,
    base::ScopedFD fd,
    bool is_primary_device) {
  auto gbm_device = std::make_unique<MockGbmDevice>();
  if (path.empty())
    return base::MakeRefCounted<FakeDrmDevice>(std::move(gbm_device));

  return base::MakeRefCounted<FakeDrmDevice>(
      std::move(path), std::move(gbm_device), is_primary_device);
}

}  // namespace ui

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"

#include "ui/gfx/linux/gbm_device.h"

namespace ui {
MockDrmDevice::MockDrmDevice(const base::FilePath& path,
                             std::unique_ptr<GbmDevice> gbm_device,
                             bool is_primary_device)
    : FakeDrmDevice(path, std::move(gbm_device), is_primary_device) {}

MockDrmDevice::~MockDrmDevice() = default;

}  // namespace ui

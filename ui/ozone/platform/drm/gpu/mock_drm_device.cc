// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"

#include "ui/gfx/linux/gbm_device.h"
#include "ui/gfx/linux/test/mock_gbm_device.h"

namespace ui {
// static
scoped_refptr<MockDrmDevice> MockDrmDevice::Create() {
  auto gbm_device = std::make_unique<MockGbmDevice>();
  auto drm_device = base::MakeRefCounted<testing::NiceMock<MockDrmDevice>>(
      base::FilePath(), std::move(gbm_device), true);
  return drm_device;
}

MockDrmDevice::MockDrmDevice(const base::FilePath& path,
                             std::unique_ptr<GbmDevice> gbm_device,
                             bool is_primary_device)
    : FakeDrmDevice(path, std::move(gbm_device), is_primary_device) {
  ON_CALL(*this, CommitProperties)
      .WillByDefault([this](drmModeAtomicReq* request, uint32_t flags,
                            uint32_t crtc_count,
                            scoped_refptr<PageFlipRequest> callback) {
        return FakeDrmDevice::CommitProperties(request, flags, crtc_count,
                                               callback);
      });
}

MockDrmDevice::~MockDrmDevice() = default;

}  // namespace ui

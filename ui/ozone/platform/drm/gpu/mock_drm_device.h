// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_MOCK_DRM_DEVICE_H_
#define UI_OZONE_PLATFORM_DRM_GPU_MOCK_DRM_DEVICE_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device.h"

namespace ui {

// MockDrmDevice provides a gmockable interface for DrmDevice,
// but also defaults to FakeDrmDevice unless a mock is specified.
class MockDrmDevice : public FakeDrmDevice {
 public:
  // Create atomic MockDrmDevice with a stub GbmDevice.
  static scoped_refptr<MockDrmDevice> Create();

  MockDrmDevice(const base::FilePath& path,
                std::unique_ptr<GbmDevice> gbm_device,
                bool is_primary_device);

  MOCK_METHOD(bool,
              CommitProperties,
              (drmModeAtomicReq * request,
               uint32_t flags,
               uint32_t crtc_count,
               scoped_refptr<PageFlipRequest> callback),
              (override));

 protected:
  ~MockDrmDevice() override;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_MOCK_DRM_DEVICE_H_

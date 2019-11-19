// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_MOCK_GBM_DEVICE_H_
#define UI_OZONE_PLATFORM_DRM_GPU_MOCK_GBM_DEVICE_H_

#include "ui/ozone/common/linux/gbm_device.h"

namespace ui {

// The real DrmDevice makes actual DRM calls which we can't use in unit tests.
class MockGbmDevice : public GbmDevice {
 public:
  MockGbmDevice();
  ~MockGbmDevice() override;

  void set_allocation_failure(bool should_fail_allocations);

  // GbmDevice:
  std::unique_ptr<GbmBuffer> CreateBuffer(uint32_t format,
                                          const gfx::Size& size,
                                          uint32_t flags) override;
  std::unique_ptr<GbmBuffer> CreateBufferWithModifiers(
      uint32_t format,
      const gfx::Size& size,
      uint32_t flags,
      const std::vector<uint64_t>& modifiers) override;
  std::unique_ptr<GbmBuffer> CreateBufferFromHandle(
      uint32_t format,
      const gfx::Size& size,
      gfx::NativePixmapHandle handle) override;

 private:
  uint32_t next_handle_ = 0;
  bool should_fail_allocations_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockGbmDevice);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_MOCK_GBM_DEVICE_H_

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_LINUX_TEST_MOCK_GBM_DEVICE_H_
#define UI_GFX_LINUX_TEST_MOCK_GBM_DEVICE_H_

#include <drm_fourcc.h>
#include <vector>
#include "ui/gfx/linux/gbm_device.h"

namespace ui {

// The real DrmDevice makes actual DRM calls which we can't use in unit tests.
class MockGbmDevice : public GbmDevice {
 public:
  MockGbmDevice();

  MockGbmDevice(const MockGbmDevice&) = delete;
  MockGbmDevice& operator=(const MockGbmDevice&) = delete;

  ~MockGbmDevice() override;

  void set_allocation_failure(bool should_fail_allocations);
  std::vector<uint64_t> GetSupportedModifiers() const;

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
  bool CanCreateBufferForFormat(uint32_t format) override;

 private:
  uint32_t next_handle_ = 0;
  bool should_fail_allocations_ = false;

  // List of modifiers that MockGbm validates when used.
  const std::vector<uint64_t> supported_modifiers_ = {
      DRM_FORMAT_MOD_LINEAR, I915_FORMAT_MOD_X_TILED, I915_FORMAT_MOD_Y_TILED,
      I915_FORMAT_MOD_Yf_TILED_CCS};
};

}  // namespace ui

#endif  // UI_GFX_LINUX_TEST_MOCK_GBM_DEVICE_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_LINUX_GBM_DEVICE_H_
#define UI_OZONE_COMMON_LINUX_GBM_DEVICE_H_

#include <gbm.h>
#include <memory>

#include "base/files/file.h"
#include "base/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace ui {

class GbmBuffer;

class GbmDevice {
 public:
  virtual ~GbmDevice() {}

  virtual std::unique_ptr<GbmBuffer> CreateBuffer(uint32_t format,
                                                  const gfx::Size& size,
                                                  uint32_t flags) = 0;
  virtual std::unique_ptr<GbmBuffer> CreateBufferWithModifiers(
      uint32_t format,
      const gfx::Size& size,
      uint32_t flags,
      const std::vector<uint64_t>& modifiers) = 0;
  virtual std::unique_ptr<GbmBuffer> CreateBufferFromHandle(
      uint32_t format,
      const gfx::Size& size,
      gfx::NativePixmapHandle handle) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_LINUX_GBM_DEVICE_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_LINUX_GBM_BUFFER_H_
#define UI_OZONE_COMMON_LINUX_GBM_BUFFER_H_

#include <inttypes.h>

#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap_handle.h"

class SkSurface;

namespace ui {

class GbmBuffer {
 public:
  virtual ~GbmBuffer() {}

  virtual uint32_t GetFormat() const = 0;
  virtual uint64_t GetFormatModifier() const = 0;
  virtual uint32_t GetFlags() const = 0;
  // TODO(reveman): This should not be needed once crbug.com/597932 is
  // fixed, as the size would be queried directly from the underlying bo.
  virtual gfx::Size GetSize() const = 0;
  virtual gfx::BufferFormat GetBufferFormat() const = 0;
  virtual bool AreFdsValid() const = 0;
  virtual size_t GetNumPlanes() const = 0;
  virtual int GetPlaneFd(size_t plane) const = 0;
  virtual uint32_t GetPlaneHandle(size_t plane) const = 0;
  virtual uint32_t GetPlaneStride(size_t plane) const = 0;
  virtual size_t GetPlaneOffset(size_t plane) const = 0;
  virtual size_t GetPlaneSize(size_t plane) const = 0;
  virtual uint32_t GetHandle() const = 0;
  virtual gfx::NativePixmapHandle ExportHandle() const = 0;
  virtual sk_sp<SkSurface> GetSurface() = 0;
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_LINUX_GBM_BUFFER_H_

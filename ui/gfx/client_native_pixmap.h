// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CLIENT_NATIVE_PIXMAP_H_
#define UI_GFX_CLIENT_NATIVE_PIXMAP_H_

#include "ui/gfx/gfx_export.h"

namespace gfx {

struct NativePixmapHandle;

// This represents a buffer that can be written to directly by regular CPU code,
// but can also be read by the GPU.
// NativePixmap is its counterpart in GPU process.
class GFX_EXPORT ClientNativePixmap {
 public:
  virtual ~ClientNativePixmap() {}

  // Map each plane in the client address space.
  // Return false on error.
  virtual bool Map() = 0;
  virtual void Unmap() = 0;

  virtual size_t GetNumberOfPlanes() const = 0;
  virtual void* GetMemoryAddress(size_t plane) const = 0;
  virtual int GetStride(size_t plane) const = 0;
  virtual NativePixmapHandle CloneHandleForIPC() const = 0;
};

}  // namespace gfx

#endif  // UI_GFX_CLIENT_NATIVE_PIXMAP_H_

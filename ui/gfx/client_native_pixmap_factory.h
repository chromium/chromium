// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CLIENT_NATIVE_PIXMAP_FACTORY_H_
#define UI_GFX_CLIENT_NATIVE_PIXMAP_FACTORY_H_

#include <memory>
#include <vector>

#include "base/files/scoped_file.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/client_native_pixmap.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

struct NativePixmapHandle;
class Size;

// The Ozone interface allows external implementations to hook into Chromium to
// provide a client pixmap for non-GPU processes (though ClientNativePixmap
// instances created using this interface can be used in the GPU process).
class GFX_EXPORT ClientNativePixmapFactory {
 public:
  virtual ~ClientNativePixmapFactory() {}

  // Import the native pixmap from |handle|. Implementations must verify that
  // the buffer in |handle| fits an image of the specified |size| and |format|.
  // Otherwise nullptr is returned. Note that a |handle| with no planes may or
  // may not be considered valid depending on the implementation.
  virtual std::unique_ptr<ClientNativePixmap> ImportFromHandle(
      gfx::NativePixmapHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) = 0;
};

}  // namespace gfx

#endif  // UI_GFX_CLIENT_NATIVE_PIXMAP_FACTORY_H_

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/surface_ozone_canvas.h"

#include "base/notreached.h"

#include <ostream>

namespace ui {

SurfaceOzoneCanvas::~SurfaceOzoneCanvas() = default;

bool SurfaceOzoneCanvas::SupportsAsyncBufferSwap() const {
  return false;
}

bool SurfaceOzoneCanvas::SupportsOverridePlatformSize() const {
  return false;
}

void SurfaceOzoneCanvas::OnSwapBuffers(SwapBuffersCallback swap_ack_callback,
                                       gfx::FrameData data) {
  NOTREACHED_IN_MIGRATION()
      << "If the SurfaceOzoneCanvas wants to handle the buffer swap "
         "callback, it must override this method.";
}

int SurfaceOzoneCanvas::MaxFramesPending() const {
  return 1;
}

}  // namespace ui

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_OVERLAY_H_
#define UI_GL_GL_SURFACE_OVERLAY_H_

#include "base/memory/scoped_refptr.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_plane_data.h"
#include "ui/gl/gl_export.h"

namespace gfx {
class GpuFence;
}  // namespace gfx

namespace gl {

// For saving the properties of an overlay plane and scheduling it later.
class GL_EXPORT GLSurfaceOverlay {
 public:
  GLSurfaceOverlay(scoped_refptr<gfx::NativePixmap> pixmap,
                   std::unique_ptr<gfx::GpuFence> gpu_fence,
                   const gfx::OverlayPlaneData& overlay_plane_data);
  GLSurfaceOverlay(GLSurfaceOverlay&& other);
  ~GLSurfaceOverlay();

  // Schedule the image as an overlay plane to be shown at swap time for
  // |widget|.
  //
  // This should be called at most once.
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget);

  gfx::GpuFence* gpu_fence() const { return gpu_fence_.get(); }
  int z_order() const { return overlay_plane_data_.z_order; }
  gfx::OverlayType overlay_type() const {
    return overlay_plane_data_.overlay_type;
  }

 private:
  scoped_refptr<gfx::NativePixmap> pixmap_;
  std::unique_ptr<gfx::GpuFence> gpu_fence_;
  gfx::OverlayPlaneData overlay_plane_data_;
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_OVERLAY_H_

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_IO_SURFACE_H_
#define UI_GL_GL_IMAGE_IO_SURFACE_H_

#include <CoreVideo/CVPixelBuffer.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <IOSurface/IOSurfaceRef.h>
#include <stdint.h>
#include <map>

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/generic_shared_memory_id.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image.h"

namespace gl {

class GL_EXPORT GLImageIOSurface : public GLImage {
 public:
  static GLImageIOSurface* Create(const gfx::Size& size);

  GLImageIOSurface(const GLImageIOSurface&) = delete;
  GLImageIOSurface& operator=(const GLImageIOSurface&) = delete;

  // Initialize to wrap of |io_surface|. The format of the plane to wrap is
  // specified in |format|. The index of the plane to wrap is
  // |io_surface_plane|. If |format| is a multi-planar format (e.g,
  // YUV_420_BIPLANAR or P010), then this will automatically convert from YUV
  // to RGB, and |io_surface_plane| is ignored.
  bool Initialize(IOSurfaceRef io_surface,
                  uint32_t io_surface_plane,
                  gfx::GenericSharedMemoryId io_surface_id,
                  gfx::BufferFormat format);

  // IOSurfaces coming from video decode are wrapped in a CVPixelBuffer
  // and may be discarded if the owning CVPixelBuffer is destroyed. This
  // initialization will ensure that the CVPixelBuffer be retained for the
  // lifetime of the GLImage. This will set `disable_in_use_by_window_server_`
  // because the existence of the CVPixelBuffer causes IOSurfaceIsInUse to
  // always return true. The color space specified in `color_space` must be
  // set to the color space specified by `cv_pixel_buffer`'s attachments.
  bool InitializeWithCVPixelBuffer(CVPixelBufferRef cv_pixel_buffer,
                                   uint32_t io_surface_plane,
                                   gfx::GenericSharedMemoryId io_surface_id,
                                   gfx::BufferFormat format,
                                   const gfx::ColorSpace& color_space);

  // Dumps information about the memory backing this instance to a dump named
  // |dump_name|.
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name);

  // Overridden from GLImage:
  gfx::Size GetSize() override;

  gfx::BufferFormat format() const { return format_; }
  gfx::GenericSharedMemoryId io_surface_id() const { return io_surface_id_; }
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface() { return io_surface_; }
  uint32_t io_surface_plane() const { return io_surface_plane_; }
  base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer() {
    return cv_pixel_buffer_;
  }

 protected:
  GLImageIOSurface(const gfx::Size& size);
  ~GLImageIOSurface() override;

  const gfx::Size size_;
  gfx::BufferFormat format_ = gfx::BufferFormat::RGBA_8888;
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;
  base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer_;
  gfx::GenericSharedMemoryId io_surface_id_;

  // The plane that is bound for this image. If the plane is invalid, then
  // this is a multi-planar IOSurface, which will be copied instead of bound.
  static constexpr uint32_t kInvalidIOSurfacePlane = -1;
  uint32_t io_surface_plane_ = kInvalidIOSurfacePlane;

  base::ThreadChecker thread_checker_;

  bool disable_in_use_by_window_server_ = false;

 private:
  void SetColorSpace(const gfx::ColorSpace& color_space);

  gfx::ColorSpace color_space_;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_IO_SURFACE_H_

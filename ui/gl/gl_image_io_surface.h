// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_IO_SURFACE_H_
#define UI_GL_GL_IMAGE_IO_SURFACE_H_

#include <CoreVideo/CVPixelBuffer.h>
#include <IOSurface/IOSurface.h>
#include <stdint.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/generic_shared_memory_id.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image.h"

#if defined(__OBJC__)
@class CALayer;
#else
typedef void* CALayer;
#endif

namespace gl {

class GL_EXPORT GLImageIOSurface : public GLImage {
 public:
  static GLImageIOSurface* Create(const gfx::Size& size,
                                  unsigned internalformat);

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
  // lifetime of the GLImage.
  bool InitializeWithCVPixelBuffer(CVPixelBufferRef cv_pixel_buffer,
                                   uint32_t io_surface_plane,
                                   gfx::GenericSharedMemoryId io_surface_id,
                                   gfx::BufferFormat format);

  // Overridden from GLImage:
  gfx::Size GetSize() override;
  unsigned GetInternalFormat() override;
  unsigned GetDataType() override;
  BindOrCopy ShouldBindOrCopy() override;
  bool BindTexImage(unsigned target) override;
  bool BindTexImageWithInternalformat(unsigned target,
                                      unsigned internalformat) override;
  void ReleaseTexImage(unsigned target) override {}
  bool CopyTexImage(unsigned target) override;
  bool CopyTexSubImage(unsigned target,
                       const gfx::Point& offset,
                       const gfx::Rect& rect) override;
  void SetColorSpace(const gfx::ColorSpace& color_space) override;
  void Flush() override {}
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;
  bool EmulatingRGB() const override;
  bool IsInUseByWindowServer() const override;
  void DisableInUseByWindowServer() override;

  gfx::GenericSharedMemoryId io_surface_id() const { return io_surface_id_; }
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface();
  base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer();

  // For IOSurfaces that need manual conversion to a GL texture before being
  // sampled from, specify the color space in which to do the required YUV to
  // RGB transformation.
  void SetColorSpaceForYUVToRGBConversion(const gfx::ColorSpace& color_space);

  // Sets the color space of the GLImage without modifying the underlying
  // IOSurface. Callers should ensure the color spaces match.
  void SetColorSpaceShallow(const gfx::ColorSpace& color_space);

  static unsigned GetInternalFormatForTesting(gfx::BufferFormat format);

  // Downcasts from |image|. Returns |nullptr| on failure.
  static GLImageIOSurface* FromGLImage(GLImage* image);

 protected:
  GLImageIOSurface(const gfx::Size& size, unsigned internalformat);
  ~GLImageIOSurface() override;
  virtual bool BindTexImageImpl(unsigned target, unsigned internalformat);

  static bool ValidFormat(gfx::BufferFormat format);
  Type GetType() const override;
  class RGBConverter;

  const gfx::Size size_;

  // The "internalformat" exposed to the command buffer, which may not be
  // "internalformat" requested by the client.
  const unsigned internalformat_;

  // The "internalformat" requested by the client.
  const unsigned client_internalformat_;

  gfx::BufferFormat format_;
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;
  base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer_;
  gfx::GenericSharedMemoryId io_surface_id_;

  // The plane that is bound for this image. If the plane is invalid, then
  // this is a multi-planar IOSurface, which will be copied instead of bound.
  static constexpr uint32_t kInvalidIOSurfacePlane = -1;
  uint32_t io_surface_plane_ = kInvalidIOSurfacePlane;

  base::ThreadChecker thread_checker_;
  // The default value of Rec. 601 is based on historical shader code.
  gfx::ColorSpace color_space_for_yuv_to_rgb_ = gfx::ColorSpace::CreateREC601();

  bool disable_in_use_by_window_server_ = false;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_IO_SURFACE_H_

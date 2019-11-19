// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_NATIVE_PIXMAP_H_
#define UI_GL_GL_IMAGE_NATIVE_PIXMAP_H_

#include <stdint.h>

#include <string>

#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image_egl.h"

namespace gl {

class GL_EXPORT GLImageNativePixmap : public gl::GLImageEGL {
 public:
  GLImageNativePixmap(const gfx::Size& size, gfx::BufferFormat format);

  // Create an EGLImage from a given NativePixmap.
  bool Initialize(scoped_refptr<gfx::NativePixmap> pixmap);
  // Create an EGLImage from a given GL texture.
  bool InitializeFromTexture(uint32_t texture_id);
  // Export the wrapped EGLImage to dmabuf fds.
  gfx::NativePixmapHandle ExportHandle();

  // Overridden from GLImage:
  unsigned GetInternalFormat() override;
  unsigned GetDataType() override;
  bool CopyTexImage(unsigned target) override;
  bool CopyTexSubImage(unsigned target,
                       const gfx::Point& offset,
                       const gfx::Rect& rect) override;
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int z_order,
                            gfx::OverlayTransform transform,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override;
  void SetColorSpace(const gfx::ColorSpace& color_space) override {}
  void Flush() override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;

 protected:
  ~GLImageNativePixmap() override;

 private:
  gfx::BufferFormat format_;
  scoped_refptr<gfx::NativePixmap> pixmap_;
  bool has_image_flush_external_;
  bool has_image_dma_buf_export_;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_NATIVE_PIXMAP_H_

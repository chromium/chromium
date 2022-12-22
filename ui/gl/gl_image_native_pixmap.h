// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_NATIVE_PIXMAP_H_
#define UI_GL_GL_IMAGE_NATIVE_PIXMAP_H_

#include <stdint.h>

#include <string>

#include "ui/gfx/color_space.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image_egl.h"

namespace gl {

class GL_EXPORT GLImageNativePixmap : public gl::GLImageEGL {
 public:
  // Create an EGLImage from a given NativePixmap.
  static scoped_refptr<GLImageNativePixmap> Create(
      const gfx::Size& size,
      gfx::BufferFormat format,
      scoped_refptr<gfx::NativePixmap> pixmap);

  // Create an EGLImage from a given NativePixmap and plane.
  static scoped_refptr<GLImageNativePixmap> CreateForPlane(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferPlane plane,
      scoped_refptr<gfx::NativePixmap> pixmap);
  // Create an EGLImage from a given GL texture.
  static scoped_refptr<GLImageNativePixmap> CreateFromTexture(
      const gfx::Size& size,
      gfx::BufferFormat format,
      uint32_t texture_id);

  // Export the wrapped EGLImage to dmabuf fds.
  gfx::NativePixmapHandle ExportHandle();

  // Set the color space when the image is used as an overlay. The color space
  // may also be useful for images backed by YUV buffers: if the GL driver can
  // sample the YUV buffer as RGB, we need to tell it the encoding (BT.601,
  // BT.709, or BT.2020) and range (limited or null), and |color_space| conveys
  // this.
  void SetColorSpace(const gfx::ColorSpace& color_space) {
    color_space_ = color_space;
  }

  // Overridden from GLImage:
  unsigned GetInternalFormat() override;
  unsigned GetDataType() override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;
  scoped_refptr<gfx::NativePixmap> GetNativePixmap() override;

 protected:
  ~GLImageNativePixmap() override;

 private:
  GLImageNativePixmap(const gfx::Size& size,
                      gfx::BufferFormat format,
                      gfx::BufferPlane plane);
  // Create an EGLImage from a given NativePixmap.
  bool Initialize(scoped_refptr<gfx::NativePixmap> pixmap);
  // Create an EGLImage from a given GL texture.
  bool InitializeFromTexture(uint32_t texture_id);

  gfx::BufferFormat format_;
  scoped_refptr<gfx::NativePixmap> pixmap_;
  gfx::BufferPlane plane_;
  bool has_image_dma_buf_export_;
  gfx::ColorSpace color_space_;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_NATIVE_PIXMAP_H_

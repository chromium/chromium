// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_NATIVE_PIXMAP_H_
#define UI_GL_GL_IMAGE_NATIVE_PIXMAP_H_

#include <stdint.h>

#include <EGL/eglplatform.h>

#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image.h"

namespace gl {

class GL_EXPORT GLImageNativePixmap : public GLImage {
 public:
  // Create an EGLImage from a given NativePixmap.
  static scoped_refptr<GLImageNativePixmap> Create(
      const gfx::Size& size,
      gfx::BufferFormat format,
      scoped_refptr<gfx::NativePixmap> pixmap);

  // Create an EGLImage from a given NativePixmap and plane. The color space is
  // for the external sampler: When we sample the YUV buffer as RGB, we need to
  // tell it the encoding (BT.601, BT.709, or BT.2020) and range (limited or
  // null), and |color_space| conveys this.
  static scoped_refptr<GLImageNativePixmap> CreateForPlane(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferPlane plane,
      scoped_refptr<gfx::NativePixmap> pixmap,
      const gfx::ColorSpace& color_space);
  // Create an EGLImage from a given GL texture.
  static scoped_refptr<GLImageNativePixmap> CreateFromTexture(
      const gfx::Size& size,
      gfx::BufferFormat format,
      uint32_t texture_id);

  // Export the wrapped EGLImage to dmabuf fds.
  gfx::NativePixmapHandle ExportHandle();

  // Get the GL internal format of the image.
  // It is aligned with glTexImage{2|3}D's parameter |internalformat|.
  unsigned GetInternalFormat();

  // Returns the NativePixmap backing this instance.
  scoped_refptr<gfx::NativePixmap> GetNativePixmap();

  // Overridden from GLImage:
  gfx::Size GetSize() override;
  bool BindTexImage(unsigned target) override;

 protected:
  ~GLImageNativePixmap() override;

 private:
  GLImageNativePixmap(const gfx::Size& size,
                      gfx::BufferFormat format,
                      gfx::BufferPlane plane);
  // Create an EGLImage from a given NativePixmap.
  bool InitializeFromNativePixmap(scoped_refptr<gfx::NativePixmap> pixmap,
                                  const gfx::ColorSpace& color_space);
  // Create an EGLImage from a given GL texture.
  bool InitializeFromTexture(uint32_t texture_id);

  // Same semantic as specified for eglCreateImageKHR. There two main usages:
  // 1- When using the |target| EGL_GL_TEXTURE_2D_KHR it is required to pass
  // a valid |context|. This allows to create an EGLImage from a GL texture.
  // Then this EGLImage can be converted to an external resource to be shared
  // with other client APIs.
  // 2- When using the |target| EGL_NATIVE_PIXMAP_KHR or EGL_LINUX_DMA_BUF_EXT
  // it is required to pass EGL_NO_CONTEXT. This allows to create an EGLImage
  // from an external resource. Then this EGLImage can be converted to a GL
  // texture.
  bool Initialize(void* context /* EGLContext */,
                  unsigned target /* EGLenum */,
                  void* buffer /* EGLClientBuffer */,
                  const EGLint* attrs);

  raw_ptr<void, DanglingUntriaged> egl_image_ /* EGLImageKHR */;
  const gfx::Size size_;
  THREAD_CHECKER(thread_checker_);
  gfx::BufferFormat format_;
  scoped_refptr<gfx::NativePixmap> pixmap_;
  gfx::BufferPlane plane_;
  bool has_image_dma_buf_export_;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_NATIVE_PIXMAP_H_

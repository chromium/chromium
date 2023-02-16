// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_GL_TEXTURE_H_
#define UI_GL_GL_IMAGE_GL_TEXTURE_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image.h"

namespace gl {

class GL_EXPORT GLImageGLTexture : public GLImage {
 public:
  // Create an EGLImage from a given GL texture.
  static scoped_refptr<GLImageGLTexture> CreateFromTexture(
      const gfx::Size& size,
      gfx::BufferFormat format,
      uint32_t texture_id);

  // Export the wrapped EGLImage to dmabuf fds.
  gfx::NativePixmapHandle ExportHandle();

  // Overridden from GLImage:
  gfx::Size GetSize() override;

  // Binds image to texture currently bound to |target|.
  void BindTexImage(unsigned target);

 protected:
  ~GLImageGLTexture() override;

 private:
  GLImageGLTexture(const gfx::Size& size, gfx::BufferFormat format);
  // Create an EGLImage from a given GL texture. This EGLImage can be converted
  // to an external resource to be shared with other client APIs.
  bool InitializeFromTexture(uint32_t texture_id);

  // Get the GL internal format of the image.
  // It is aligned with glTexImage{2|3}D's parameter |internalformat|.
  unsigned GetInternalFormat();

  raw_ptr<void, DanglingUntriaged> egl_image_ /* EGLImageKHR */;
  const gfx::Size size_;
  THREAD_CHECKER(thread_checker_);
  gfx::BufferFormat format_;
  bool has_image_dma_buf_export_;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_GL_TEXTURE_H_

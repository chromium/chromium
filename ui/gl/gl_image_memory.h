// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_MEMORY_H_
#define UI_GL_GL_IMAGE_MEMORY_H_

#include "ui/gl/gl_image.h"

#include <stddef.h>

#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_math.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_export.h"

namespace gpu {
class SharedMemoryImageBacking;
}

namespace gl {
class GLContext;
class GLImageMemoryForTesting;
class GLSurface;

class GL_EXPORT GLImageMemory : public GLImage {
 public:
  GLImageMemory(const GLImageMemory&) = delete;
  GLImageMemory& operator=(const GLImageMemory&) = delete;

  bool Initialize(const unsigned char* memory,
                  gfx::BufferFormat format,
                  size_t stride,
                  bool disable_pbo_upload = false);

  // Overridden from GLImage:
  gfx::Size GetSize() override;
  unsigned GetInternalFormat() override;
  unsigned GetDataFormat() override;
  unsigned GetDataType() override;
  BindOrCopy ShouldBindOrCopy() override;
  bool BindTexImage(unsigned target) override;
  void ReleaseTexImage(unsigned target) override {}
  bool CopyTexImage(unsigned target) override;
  bool CopyTexSubImage(unsigned target,
                       const gfx::Point& offset,
                       const gfx::Rect& rect) override;
  Type GetType() const override;

  const unsigned char* memory() { return memory_; }
  size_t stride() const { return stride_; }
  gfx::BufferFormat format() const { return format_; }

 protected:
  ~GLImageMemory() override;

 private:
  // Make constructor private to ensure that only specified friend classes can
  // create GLImageMemory instances.
  explicit GLImageMemory(const gfx::Size& size);

  // GLImageMemory should be created in production only by
  // SharedMemoryImageBacking. Some tests need to subclass GLImageMemory in
  // anonymous namespaces, for which GLImageMemoryForTesting exists.
  friend class gpu::SharedMemoryImageBacking;
  friend class GLImageMemoryForTesting;

  static bool ValidFormat(gfx::BufferFormat format);

  const gfx::Size size_;
  const unsigned char* memory_;
  gfx::BufferFormat format_;
  size_t stride_;

  unsigned buffer_ = 0;
  // The context/surface from which the |buffer_| is created.
  base::WeakPtr<GLContext> original_context_;
  base::WeakPtr<GLSurface> original_surface_;
  size_t buffer_bytes_ = 0;
  int memcpy_tasks_ = 0;
};

// GLImageMemoryForTesting supports test use cases for subclassing
// GLImageMemory in anonymous namespaces. This class should never be used in
// production.
class GL_EXPORT GLImageMemoryForTesting : public GLImageMemory {
 protected:
  explicit GLImageMemoryForTesting(const gfx::Size& size);

 protected:
  ~GLImageMemoryForTesting() override;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_MEMORY_H_

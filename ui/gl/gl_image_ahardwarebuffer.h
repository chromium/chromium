// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_AHARDWAREBUFFER_H_
#define UI_GL_GL_IMAGE_AHARDWAREBUFFER_H_

#include <memory>

#include "base/android/scoped_hardware_buffer_handle.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image_egl.h"

namespace gl {

class GL_EXPORT GLImageAHardwareBuffer : public GLImageEGL {
 public:
  explicit GLImageAHardwareBuffer(const gfx::Size& size);

  GLImageAHardwareBuffer(const GLImageAHardwareBuffer&) = delete;
  GLImageAHardwareBuffer& operator=(const GLImageAHardwareBuffer&) = delete;

  // Create an EGLImage from a given Android hardware buffer.
  bool Initialize(AHardwareBuffer* buffer, bool preserved);

  // Overridden from GLImage:
  unsigned GetInternalFormat() override;
  unsigned GetDataType() override;
  bool BindTexImage(unsigned target) override;
  bool CopyTexImage(unsigned target) override;
  bool CopyTexSubImage(unsigned target,
                       const gfx::Point& offset,
                       const gfx::Rect& rect) override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;

 protected:
  ~GLImageAHardwareBuffer() override;

 private:
  base::android::ScopedHardwareBufferHandle handle_;
  unsigned internal_format_ = GL_RGBA;
  unsigned data_type_ = GL_UNSIGNED_BYTE;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_AHARDWAREBUFFER_H_

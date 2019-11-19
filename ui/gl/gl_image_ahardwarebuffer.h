// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_AHARDWAREBUFFER_H_
#define UI_GL_GL_IMAGE_AHARDWAREBUFFER_H_

#include <memory>

#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/macros.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image_egl.h"

namespace base {
namespace android {
class ScopedHardwareBufferFenceSync;
}  // namespace android
}  // namespace base

namespace gl {

class GL_EXPORT GLImageAHardwareBuffer : public GLImageEGL {
 public:
  explicit GLImageAHardwareBuffer(const gfx::Size& size);

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
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int z_order,
                            gfx::OverlayTransform transform,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override;
  void Flush() override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer() override;

 protected:
  ~GLImageAHardwareBuffer() override;

 private:
  class ScopedHardwareBufferFenceSyncImpl;

  base::android::ScopedHardwareBufferHandle handle_;
  unsigned internal_format_ = GL_RGBA;
  unsigned data_type_ = GL_UNSIGNED_BYTE;

  DISALLOW_COPY_AND_ASSIGN(GLImageAHardwareBuffer);
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_AHARDWAREBUFFER_H_

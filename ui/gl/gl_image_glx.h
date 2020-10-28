// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_GLX_H_
#define UI_GL_GL_IMAGE_GLX_H_

#include <stdint.h>

#include "base/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/x/glx.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image.h"

namespace gl {

class GL_EXPORT GLImageGLX : public GLImage {
 public:
  GLImageGLX(const gfx::Size& size, gfx::BufferFormat format);

  bool Initialize(x11::Pixmap pixmap);

  // Overridden from GLImage:
  gfx::Size GetSize() override;
  unsigned GetInternalFormat() override;
  unsigned GetDataType() override;
  BindOrCopy ShouldBindOrCopy() override;
  bool BindTexImage(unsigned target) override;
  void ReleaseTexImage(unsigned target) override;
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
  void Flush() override {}
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;

 protected:
  ~GLImageGLX() override;

  gfx::BufferFormat format() const { return format_; }

 private:
  uint32_t glx_pixmap_;
  const gfx::Size size_;
  gfx::BufferFormat format_;

  DISALLOW_COPY_AND_ASSIGN(GLImageGLX);
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_GLX_H_

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_STUB_H_
#define UI_GL_GL_SURFACE_STUB_H_

#include "ui/gfx/frame_data.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface.h"

namespace gl {

// A GLSurface that does nothing for unit tests.
class GL_EXPORT GLSurfaceStub : public GLSurface {
 public:
  void SetSize(const gfx::Size& size) { size_ = size; }
  void set_buffers_flipped(bool flipped) { buffers_flipped_ = flipped; }

  // Implement GLSurface.
  void Destroy() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  bool IsOffscreen() override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback,
                              gfx::FrameData data) override;
  gfx::Size GetSize() override;
  void* GetHandle() override;
  bool BuffersFlipped() const override;
  GLSurfaceFormat GetFormat() override;

 protected:
  ~GLSurfaceStub() override;

 private:
  gfx::Size size_;
  bool buffers_flipped_ = false;
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_STUB_H_

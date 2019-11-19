// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_CONTEXT_GLX_H_
#define UI_GL_GL_CONTEXT_GLX_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_export.h"

namespace gl {

class GLSurface;

// Encapsulates a GLX OpenGL context.
class GL_EXPORT GLContextGLX : public GLContextReal {
 public:
  explicit GLContextGLX(GLShareGroup* share_group);

  XDisplay* display();

  // Implement GLContext.
  bool Initialize(GLSurface* compatible_surface,
                  const GLContextAttribs& attribs) override;
  bool MakeCurrent(GLSurface* surface) override;
  void ReleaseCurrent(GLSurface* surface) override;
  bool IsCurrent(GLSurface* surface) override;
  void* GetHandle() override;
  unsigned int CheckStickyGraphicsResetStatus() override;

 protected:
  ~GLContextGLX() override;

 private:
  void Destroy();

  void* context_;
  XDisplay* display_;
  unsigned int graphics_reset_status_ = 0;  // GL_NO_ERROR

  DISALLOW_COPY_AND_ASSIGN(GLContextGLX);
};

}  // namespace gl

#endif  // UI_GL_GL_CONTEXT_GLX_H_

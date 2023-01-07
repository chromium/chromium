// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_CONTEXT_GLX_H_
#define UI_GL_GL_CONTEXT_GLX_H_

#include "base/memory/raw_ptr.h"
#include "ui/gfx/x/connection.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_export.h"

namespace gl {

class GLSurface;

// Encapsulates a GLX OpenGL context.
class GL_EXPORT GLContextGLX : public GLContextReal {
 public:
  explicit GLContextGLX(GLShareGroup* share_group);

  GLContextGLX(const GLContextGLX&) = delete;
  GLContextGLX& operator=(const GLContextGLX&) = delete;

  // Implement GLContext.
  bool Initialize(GLSurface* compatible_surface,
                  const GLContextAttribs& attribs) override;
  bool MakeCurrentImpl(GLSurface* surface) override;
  void ReleaseCurrent(GLSurface* surface) override;
  bool IsCurrent(GLSurface* surface) override;
  void* GetHandle() override;
  unsigned int CheckStickyGraphicsResetStatusImpl() override;

 protected:
  ~GLContextGLX() override;

 private:
  void Destroy();

  raw_ptr<void> context_ = nullptr;
  raw_ptr<x11::Connection> connection_ = nullptr;
  unsigned int graphics_reset_status_ = 0;  // GL_NO_ERROR
};

}  // namespace gl

#endif  // UI_GL_GL_CONTEXT_GLX_H_

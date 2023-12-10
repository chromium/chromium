// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_CONTEXT_EGL_H_
#define UI_GL_GL_CONTEXT_EGL_H_

#include "base/memory/raw_ptr.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_export.h"

typedef void* EGLContext;
typedef void* EGLConfig;

namespace gl {
class GLDisplayEGL;
class GLSurface;

// Encapsulates an EGL OpenGL ES context.
class GL_EXPORT GLContextEGL : public GLContextReal {
 public:
  explicit GLContextEGL(GLShareGroup* share_group);

  GLContextEGL(const GLContextEGL&) = delete;
  GLContextEGL& operator=(const GLContextEGL&) = delete;

  // Implement GLContext.
  bool InitializeImpl(GLSurface* compatible_surface,
                      const GLContextAttribs& attribs) override;
  bool MakeCurrentImpl(GLSurface* surface) override;
  void ReleaseCurrent(GLSurface* surface) override;
  bool IsCurrent(GLSurface* surface) override;
  void* GetHandle() override;
  unsigned int CheckStickyGraphicsResetStatusImpl() override;
  void SetUnbindFboOnMakeCurrent() override;
  void SetVisibility(bool visibility) override;
  GLDisplayEGL* GetGLDisplayEGL() override;
  GLContextEGL* AsGLContextEGL() override;
  bool CanShareTexturesWithContext(GLContext* other_context) override;

 protected:
  ~GLContextEGL() override;

 private:
  void Destroy();
  void ReleaseBackpressureFences();

  EGLContext context_ = nullptr;
  raw_ptr<GLDisplayEGL> gl_display_ = nullptr;
  EGLConfig config_ = nullptr;
  unsigned int graphics_reset_status_ = 0;  // GL_NO_ERROR;
  bool unbind_fbo_on_makecurrent_ = false;
  bool lost_ = false;

  // Cached values used by |CanShareTexturesWithContext|.
  bool global_texture_share_group_ = false;
  AngleContextVirtualizationGroup angle_context_virtualization_group_number_ =
      AngleContextVirtualizationGroup::kDefault;
};

}  // namespace gl

#endif  // UI_GL_GL_CONTEXT_EGL_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_GL_OZONE_GLX_H_
#define UI_OZONE_PLATFORM_X11_GL_OZONE_GLX_H_

#include "ui/gl/gl_implementation.h"
#include "ui/ozone/public/gl_ozone.h"

namespace ui {

class GLOzoneGLX : public GLOzone {
 public:
  GLOzoneGLX() {}

  GLOzoneGLX(const GLOzoneGLX&) = delete;
  GLOzoneGLX& operator=(const GLOzoneGLX&) = delete;

  ~GLOzoneGLX() override {}

  gl::GLDisplay* InitializeGLOneOffPlatform(uint64_t system_device_id) override;
  bool InitializeStaticGLBindings(
      const gl::GLImplementationParts& implementation) override;
  void SetDisabledExtensionsPlatform(
      const std::string& disabled_extensions) override;
  bool InitializeExtensionSettingsOneOffPlatform(
      gl::GLDisplay* display) override;
  void ShutdownGL(gl::GLDisplay* display) override;
  bool GetGLWindowSystemBindingInfo(
      const gl::GLVersionInfo& gl_info,
      gl::GLWindowSystemBindingInfo* info) override;
  scoped_refptr<gl::GLContext> CreateGLContext(
      gl::GLShareGroup* share_group,
      gl::GLSurface* compatible_surface,
      const gl::GLContextAttribs& attribs) override;
  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gfx::AcceleratedWidget window) override;
  scoped_refptr<gl::GLSurface> CreateSurfacelessViewGLSurface(
      gfx::AcceleratedWidget window) override;
  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      const gfx::Size& size) override;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_GL_OZONE_GLX_H_

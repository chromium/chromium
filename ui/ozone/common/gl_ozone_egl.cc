// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/gl_ozone_egl.h"

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_egl_api_implementation.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/presenter.h"

namespace ui {

gl::GLDisplay* GLOzoneEGL::InitializeGLOneOffPlatform(
    bool supports_angle,
    std::vector<gl::DisplayType> init_displays,
    gl::GpuPreference gpu_preference) {
  gl::GLDisplayEGL* display = gl::GetDisplayEGL(gpu_preference);
  if (!display->Initialize(supports_angle, init_displays, GetNativeDisplay())) {
    LOG(ERROR) << "GLDisplayEGL::Initialize failed.";
    return nullptr;
  }
  return display;
}

bool GLOzoneEGL::InitializeStaticGLBindings(
    const gl::GLImplementationParts& implementation) {
  if (!LoadGLES2Bindings(implementation))
    return false;

  gl::SetGLImplementationParts(implementation);
  gl::InitializeStaticGLBindingsGL();
  gl::InitializeStaticGLBindingsEGL();

  return true;
}

void GLOzoneEGL::SetDisabledExtensionsPlatform(
    const std::string& disabled_extensions) {
  gl::SetDisabledExtensionsEGL(disabled_extensions);
}

bool GLOzoneEGL::InitializeExtensionSettingsOneOffPlatform(
    gl::GLDisplay* display) {
  return gl::InitializeExtensionSettingsOneOffEGL(
      static_cast<gl::GLDisplayEGL*>(display));
}

void GLOzoneEGL::ShutdownGL(gl::GLDisplay* display) {
  if (display)
    display->Shutdown();
  gl::ClearBindingsGL();
  gl::ClearBindingsEGL();
}

bool GLOzoneEGL::CanImportNativePixmap(gfx::BufferFormat format) {
  return false;
}

std::unique_ptr<NativePixmapGLBinding> GLOzoneEGL::ImportNativePixmap(
    scoped_refptr<gfx::NativePixmap> pixmap,
    gfx::BufferFormat plane_format,
    gfx::BufferPlane plane,
    gfx::Size plane_size,
    const gfx::ColorSpace& color_space,
    GLenum target,
    GLuint texture_id) {
  return nullptr;
}

bool GLOzoneEGL::GetGLWindowSystemBindingInfo(
    const gl::GLVersionInfo& gl_info,
    gl::GLWindowSystemBindingInfo* info) {
  return gl::GetGLWindowSystemBindingInfoEGL(info);
}

scoped_refptr<gl::GLContext> GLOzoneEGL::CreateGLContext(
    gl::GLShareGroup* share_group,
    gl::GLSurface* compatible_surface,
    const gl::GLContextAttribs& attribs) {
  return gl::InitializeGLContext(new gl::GLContextEGL(share_group),
                                 compatible_surface, attribs);
}

scoped_refptr<gl::Presenter> GLOzoneEGL::CreateSurfacelessViewGLSurface(
    gl::GLDisplay* display,
    gfx::AcceleratedWidget window) {
  // This will usually not be implemented by the platform specific version.
  return nullptr;
}

}  // namespace ui

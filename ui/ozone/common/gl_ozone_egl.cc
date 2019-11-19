// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/gl_ozone_egl.h"

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_egl_api_implementation.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_egl.h"

namespace ui {

bool GLOzoneEGL::InitializeGLOneOffPlatform() {
  if (!gl::GLSurfaceEGL::InitializeOneOff(GetNativeDisplay())) {
    LOG(ERROR) << "GLSurfaceEGL::InitializeOneOff failed.";
    return false;
  }
  return true;
}

bool GLOzoneEGL::InitializeStaticGLBindings(
    gl::GLImplementation implementation) {
  if (!LoadGLES2Bindings(implementation))
    return false;

  gl::SetGLImplementation(implementation);
  gl::InitializeStaticGLBindingsGL();
  gl::InitializeStaticGLBindingsEGL();

  return true;
}

void GLOzoneEGL::InitializeDebugGLBindings() {
  gl::InitializeDebugGLBindingsGL();
  gl::InitializeDebugGLBindingsEGL();
}

void GLOzoneEGL::SetDisabledExtensionsPlatform(
    const std::string& disabled_extensions) {
  gl::SetDisabledExtensionsEGL(disabled_extensions);
}

bool GLOzoneEGL::InitializeExtensionSettingsOneOffPlatform() {
  return gl::InitializeExtensionSettingsOneOffEGL();
}

void GLOzoneEGL::ShutdownGL() {
  gl::GLSurfaceEGL::ShutdownOneOff();
  gl::ClearBindingsGL();
  gl::ClearBindingsEGL();
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

scoped_refptr<gl::GLSurface> GLOzoneEGL::CreateSurfacelessViewGLSurface(
    gfx::AcceleratedWidget window) {
  // This will usually not be implemented by the platform specific version.
  return nullptr;
}

}  // namespace ui

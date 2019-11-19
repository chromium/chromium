// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/gl_ozone_glx.h"

#include "base/command_line.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_glx.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_glx_api_implementation.h"
#include "ui/ozone/platform/x11/gl_surface_glx_ozone.h"

namespace ui {

namespace {

#if defined(OS_OPENBSD)
const char kGLLibraryName[] = "libGL.so";
#else
const char kGLLibraryName[] = "libGL.so.1";
#endif

}  // namespace

bool GLOzoneGLX::InitializeGLOneOffPlatform() {
  if (!gl::GLSurfaceGLX::InitializeOneOff()) {
    LOG(ERROR) << "GLSurfaceGLX::InitializeOneOff failed.";
    return false;
  }
  return true;
}

bool GLOzoneGLX::InitializeStaticGLBindings(
    gl::GLImplementation implementation) {
  base::NativeLibrary library = nullptr;
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(switches::kTestGLLib))
    library = gl::LoadLibraryAndPrintError(
        command_line->GetSwitchValueASCII(switches::kTestGLLib).c_str());

  if (!library)
    library = gl::LoadLibraryAndPrintError(kGLLibraryName);

  if (!library)
    return false;

  gl::GLGetProcAddressProc get_proc_address =
      reinterpret_cast<gl::GLGetProcAddressProc>(
          base::GetFunctionPointerFromNativeLibrary(library,
                                                    "glXGetProcAddress"));
  if (!get_proc_address) {
    LOG(ERROR) << "glxGetProcAddress not found.";
    base::UnloadNativeLibrary(library);
    return false;
  }

  gl::SetGLGetProcAddressProc(get_proc_address);
  gl::AddGLNativeLibrary(library);
  gl::SetGLImplementation(gl::kGLImplementationDesktopGL);

  gl::InitializeStaticGLBindingsGL();
  gl::InitializeStaticGLBindingsGLX();

  return true;
}

void GLOzoneGLX::InitializeDebugGLBindings() {
  gl::InitializeDebugGLBindingsGL();
  gl::InitializeDebugGLBindingsGLX();
}

void GLOzoneGLX::SetDisabledExtensionsPlatform(
    const std::string& disabled_extensions) {
  gl::SetDisabledExtensionsGLX(disabled_extensions);
}

bool GLOzoneGLX::InitializeExtensionSettingsOneOffPlatform() {
  return gl::InitializeExtensionSettingsOneOffGLX();
}

void GLOzoneGLX::ShutdownGL() {
  gl::ClearBindingsGL();
  gl::ClearBindingsGLX();
}

bool GLOzoneGLX::GetGLWindowSystemBindingInfo(
    const gl::GLVersionInfo& gl_info,
    gl::GLWindowSystemBindingInfo* info) {
  return gl::GetGLWindowSystemBindingInfoGLX(gl_info, info);
}

scoped_refptr<gl::GLContext> GLOzoneGLX::CreateGLContext(
    gl::GLShareGroup* share_group,
    gl::GLSurface* compatible_surface,
    const gl::GLContextAttribs& attribs) {
  return gl::InitializeGLContext(new gl::GLContextGLX(share_group),
                                 compatible_surface, attribs);
}

scoped_refptr<gl::GLSurface> GLOzoneGLX::CreateViewGLSurface(
    gfx::AcceleratedWidget window) {
  return gl::InitializeGLSurface(new GLSurfaceGLXOzone(window));
}

scoped_refptr<gl::GLSurface> GLOzoneGLX::CreateSurfacelessViewGLSurface(
    gfx::AcceleratedWidget window) {
  return nullptr;
}

scoped_refptr<gl::GLSurface> GLOzoneGLX::CreateOffscreenGLSurface(
    const gfx::Size& size) {
  return gl::InitializeGLSurface(new gl::UnmappedNativeViewGLSurfaceGLX(size));
}

}  // namespace ui

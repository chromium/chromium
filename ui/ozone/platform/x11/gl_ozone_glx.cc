// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/gl_ozone_glx.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_glx.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_glx_api_implementation.h"
#include "ui/gl/gl_surface_glx_x11.h"
#include "ui/gl/gl_utils.h"
#include "ui/ozone/platform/x11/native_pixmap_glx_binding.h"

namespace ui {

namespace {

#if BUILDFLAG(IS_OPENBSD)
const char kGLLibraryName[] = "libGL.so";
#else
const char kGLLibraryName[] = "libGL.so.1";
#endif

}  // namespace

gl::GLDisplay* GLOzoneGLX::InitializeGLOneOffPlatform(
    uint64_t system_device_id) {
  // TODO(https://crbug.com/1251724): GLSurfaceGLX::InitializeOneOff()
  // should take |system_device_id| and return a GLDisplayX11.
  if (!gl::GLSurfaceGLX::InitializeOneOff()) {
    LOG(ERROR) << "GLSurfaceGLX::InitializeOneOff failed.";
    return nullptr;
  }
  return gl::GetDisplayX11(system_device_id);
}

bool GLOzoneGLX::InitializeStaticGLBindings(
    const gl::GLImplementationParts& implementation) {
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

void GLOzoneGLX::SetDisabledExtensionsPlatform(
    const std::string& disabled_extensions) {
  gl::SetDisabledExtensionsGLX(disabled_extensions);
}

bool GLOzoneGLX::InitializeExtensionSettingsOneOffPlatform(
    gl::GLDisplay* display) {
  return gl::InitializeExtensionSettingsOneOffGLX();
}

void GLOzoneGLX::ShutdownGL(gl::GLDisplay* display) {
  gl::ClearBindingsGL();
  gl::ClearBindingsGLX();
}

bool GLOzoneGLX::CanImportNativePixmap() {
  return false;
}

std::unique_ptr<NativePixmapGLBinding> GLOzoneGLX::ImportNativePixmap(
    scoped_refptr<gfx::NativePixmap> pixmap,
    gfx::BufferFormat plane_format,
    gfx::BufferPlane plane,
    gfx::Size plane_size,
    const gfx::ColorSpace& color_space,
    GLenum target,
    GLuint texture_id) {
  return NativePixmapGLXBinding::Create(pixmap, plane_format, plane, plane_size,
                                        target, texture_id);
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
    gl::GLDisplay* display,
    gfx::AcceleratedWidget window) {
  return gl::InitializeGLSurface(new gl::GLSurfaceGLXX11(window));
}

scoped_refptr<gl::GLSurface> GLOzoneGLX::CreateSurfacelessViewGLSurface(
    gl::GLDisplay* display,
    gfx::AcceleratedWidget window) {
  return nullptr;
}

scoped_refptr<gl::GLSurface> GLOzoneGLX::CreateOffscreenGLSurface(
    gl::GLDisplay* display,
    const gfx::Size& size) {
  return gl::InitializeGLSurface(new gl::UnmappedNativeViewGLSurfaceGLX(size));
}

}  // namespace ui

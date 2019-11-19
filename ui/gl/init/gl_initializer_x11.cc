// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_initializer.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_egl_api_implementation.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_glx_api_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_glx.h"
#include "ui/gl/gl_switches.h"

namespace gl {
namespace init {

namespace {

#if defined(OS_OPENBSD)
const char kGLLibraryName[] = "libGL.so";
#else
const char kGLLibraryName[] = "libGL.so.1";
#endif

const char kGLESv2LibraryName[] = "libGLESv2.so.2";
const char kEGLLibraryName[] = "libEGL.so.1";

const char kGLESv2ANGLELibraryName[] = "libGLESv2.so";
const char kEGLANGLELibraryName[] = "libEGL.so";

#if BUILDFLAG(ENABLE_SWIFTSHADER)
const char kGLESv2SwiftShaderLibraryName[] = "libGLESv2.so";
const char kEGLSwiftShaderLibraryName[] = "libEGL.so";
#endif

bool InitializeStaticGLXInternal() {
  base::NativeLibrary library = NULL;
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(switches::kTestGLLib))
    library = LoadLibraryAndPrintError(
        command_line->GetSwitchValueASCII(switches::kTestGLLib).c_str());

  if (!library) {
    library = LoadLibraryAndPrintError(kGLLibraryName);
  }

  if (!library)
    return false;

  GLGetProcAddressProc get_proc_address =
      reinterpret_cast<GLGetProcAddressProc>(
          base::GetFunctionPointerFromNativeLibrary(library,
                                                    "glXGetProcAddress"));
  if (!get_proc_address) {
    LOG(ERROR) << "glxGetProcAddress not found.";
    base::UnloadNativeLibrary(library);
    return false;
  }

  SetGLGetProcAddressProc(get_proc_address);
  AddGLNativeLibrary(library);
  SetGLImplementation(kGLImplementationDesktopGL);

  InitializeStaticGLBindingsGL();
  InitializeStaticGLBindingsGLX();

  return true;
}

bool InitializeStaticEGLInternal(GLImplementation implementation) {
  base::FilePath glesv2_path(kGLESv2LibraryName);
  base::FilePath egl_path(kEGLLibraryName);

  if (implementation == kGLImplementationSwiftShaderGL) {
#if BUILDFLAG(ENABLE_SWIFTSHADER)
    base::FilePath module_path;
    if (!base::PathService::Get(base::DIR_MODULE, &module_path))
      return false;
    module_path = module_path.Append("swiftshader/");

    glesv2_path = module_path.Append(kGLESv2SwiftShaderLibraryName);
    egl_path = module_path.Append(kEGLSwiftShaderLibraryName);
#else
    return false;
#endif
  } else if (implementation == kGLImplementationEGLANGLE) {
    base::FilePath module_path;
    if (!base::PathService::Get(base::DIR_MODULE, &module_path))
      return false;

    glesv2_path = module_path.Append(kGLESv2ANGLELibraryName);
    egl_path = module_path.Append(kEGLANGLELibraryName);
  }

  base::NativeLibrary gles_library = LoadLibraryAndPrintError(glesv2_path);
  if (!gles_library)
    return false;
  base::NativeLibrary egl_library = LoadLibraryAndPrintError(egl_path);
  if (!egl_library) {
    base::UnloadNativeLibrary(gles_library);
    return false;
  }

  GLGetProcAddressProc get_proc_address =
      reinterpret_cast<GLGetProcAddressProc>(
          base::GetFunctionPointerFromNativeLibrary(egl_library,
                                                    "eglGetProcAddress"));
  if (!get_proc_address) {
    LOG(ERROR) << "eglGetProcAddress not found.";
    base::UnloadNativeLibrary(egl_library);
    base::UnloadNativeLibrary(gles_library);
    return false;
  }

  SetGLGetProcAddressProc(get_proc_address);
  AddGLNativeLibrary(egl_library);
  AddGLNativeLibrary(gles_library);
  if (implementation == kGLImplementationEGLANGLE) {
    SetGLImplementation(kGLImplementationEGLANGLE);
  } else {
    SetGLImplementation(kGLImplementationEGLGLES2);
  }

  InitializeStaticGLBindingsGL();
  InitializeStaticGLBindingsEGL();

  return true;
}

}  // namespace

bool InitializeGLOneOffPlatform() {
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
      if (!GLSurfaceGLX::InitializeOneOff()) {
        LOG(ERROR) << "GLSurfaceGLX::InitializeOneOff failed.";
        return false;
      }
      return true;
    case kGLImplementationSwiftShaderGL:
    case kGLImplementationEGLGLES2:
    case kGLImplementationEGLANGLE:
      if (!GLSurfaceEGL::InitializeOneOff(gfx::GetXDisplay())) {
        LOG(ERROR) << "GLSurfaceEGL::InitializeOneOff failed.";
        return false;
      }
      return true;
    default:
      return true;
  }
}

bool InitializeStaticGLBindings(GLImplementation implementation) {
  // Prevent reinitialization with a different implementation. Once the gpu
  // unit tests have initialized with kGLImplementationMock, we don't want to
  // later switch to another GL implementation.
  DCHECK_EQ(kGLImplementationNone, GetGLImplementation());

  // Allow the main thread or another to initialize these bindings
  // after instituting restrictions on I/O. Going forward they will
  // likely be used in the browser process on most platforms. The
  // one-time initialization cost is small, between 2 and 5 ms.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  switch (implementation) {
    case kGLImplementationDesktopGL:
      return InitializeStaticGLXInternal();
    case kGLImplementationSwiftShaderGL:
    case kGLImplementationEGLGLES2:
    case kGLImplementationEGLANGLE:
      return InitializeStaticEGLInternal(implementation);
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      SetGLImplementation(implementation);
      InitializeStaticGLBindingsGL();
      return true;
    default:
      NOTREACHED();
  }

  return false;
}

void InitializeDebugGLBindings() {
  InitializeDebugGLBindingsEGL();
  InitializeDebugGLBindingsGL();
  InitializeDebugGLBindingsGLX();
}

void ShutdownGLPlatform() {
  GLSurfaceEGL::ShutdownOneOff();
  GLSurfaceGLX::ShutdownOneOff();
  ClearBindingsEGL();
  ClearBindingsGL();
  ClearBindingsGLX();
}

}  // namespace init
}  // namespace gl

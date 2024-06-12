// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_initializer.h"

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/init/gl_display_initializer.h"
#include "ui/gl/gl_egl_api_implementation.h"

namespace gl {
namespace init {

namespace {

const char kGLESv2ANGLELibraryName[] = "libGLESv2.dylib";
const char kEGLANGLELibraryName[] = "libEGL.dylib";

bool InitializeStaticEGLInternalFromLibrary(GLImplementation implementation) {
#if BUILDFLAG(USE_STATIC_ANGLE)
  NOTREACHED_IN_MIGRATION();
#endif

  // Some unit test targets depend on Angle/SwiftShader but aren't built
  // as app bundles. In that case, the .dylib is next to the executable.
  base::FilePath base_dir;
  if (base::apple::AmIBundled()) {
    base_dir = base::apple::FrameworkBundlePath().Append("Libraries");
  } else {
    if (!base::PathService::Get(base::FILE_EXE, &base_dir)) {
      LOG(ERROR) << "PathService::Get failed.";
      return false;
    }
    base_dir = base_dir.DirName();
  }

  base::FilePath glesv2_path = base_dir.Append(kGLESv2ANGLELibraryName);
  base::NativeLibrary gles_library = LoadLibraryAndPrintError(glesv2_path);
  if (!gles_library) {
    return false;
  }

  base::FilePath egl_path = base_dir.Append(kEGLANGLELibraryName);
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
  // FIXME: SwiftShader must load symbols from libGLESv2 before libEGL on MacOS
  // currently
  AddGLNativeLibrary(gles_library);
  AddGLNativeLibrary(egl_library);

  return true;
}

bool InitializeStaticEGLInternal(GLImplementationParts implementation) {
#if BUILDFLAG(USE_STATIC_ANGLE)
  if (implementation.gl == kGLImplementationEGLANGLE) {
    // Use ANGLE if it is requested and it is statically linked
    if (!InitializeStaticANGLEEGL())
      return false;
  } else if (!InitializeStaticEGLInternalFromLibrary(implementation.gl)) {
    return false;
  }
#else
  if (!InitializeStaticEGLInternalFromLibrary(implementation.gl)) {
    return false;
  }
#endif  // !BUILDFLAG(USE_STATIC_ANGLE)

  SetGLImplementationParts(implementation);
  InitializeStaticGLBindingsGL();
  InitializeStaticGLBindingsEGL();

  return true;
}

}  // namespace

GLDisplay* InitializeGLOneOffPlatform(gl::GpuPreference gpu_preference) {
  GLDisplayEGL* display = GetDisplayEGL(gpu_preference);
  switch (GetGLImplementation()) {
    case kGLImplementationEGLGLES2:
    case kGLImplementationEGLANGLE:
      if (!InitializeDisplay(display, EGLDisplayPlatform(0))) {
        LOG(ERROR) << "GLDisplayEGL::Initialize failed.";
        return nullptr;
      }
      break;

    default:
      break;
  }
  return display;
}

bool InitializeStaticGLBindings(GLImplementationParts implementation) {
  // Prevent reinitialization with a different implementation. Once the gpu
  // unit tests have initialized with kGLImplementationMock, we don't want to
  // later switch to another GL implementation.
  DCHECK_EQ(kGLImplementationNone, GetGLImplementation());

  // Allow the main thread or another to initialize these bindings
  // after instituting restrictions on I/O. Going forward they will
  // likely be used in the browser process on most platforms. The
  // one-time initialization cost is small, between 2 and 5 ms.
  base::ScopedAllowBlocking allow_blocking;

  switch (implementation.gl) {
    case kGLImplementationEGLGLES2:
    case kGLImplementationEGLANGLE:
      return InitializeStaticEGLInternal(implementation);

    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      SetGLImplementationParts(implementation);
      InitializeStaticGLBindingsGL();
      return true;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  return false;
}

void ShutdownGLPlatform(GLDisplay* display) {
  ClearBindingsGL();

  if (display)
    display->Shutdown();
  ClearBindingsEGL();
}

}  // namespace init
}  // namespace gl

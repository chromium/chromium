// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_initializer.h"

#include <OpenGL/CGLRenderers.h>

#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
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

#if defined(USE_EGL)
#include "ui/gl/gl_egl_api_implementation.h"
#endif  // defined(USE_EGL)

namespace gl {
namespace init {

namespace {

const char kOpenGLFrameworkPath[] =
    "/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL";

bool InitializeOneOffForSandbox() {
  static bool initialized = false;
  if (initialized)
    return true;

  // This is called from the sandbox warmup code on Mac OS X.
  // GPU-related stuff is very slow without this, probably because
  // the sandbox prevents loading graphics drivers or some such.
  std::vector<CGLPixelFormatAttribute> attribs;
  if (GLContext::SwitchableGPUsSupported()) {
    // Avoid switching to the discrete GPU just for this pixel
    // format selection.
    attribs.push_back(kCGLPFAAllowOfflineRenderers);
  }
  attribs.push_back(static_cast<CGLPixelFormatAttribute>(0));

  CGLPixelFormatObj format;
  GLint num_pixel_formats;
  if (CGLChoosePixelFormat(&attribs.front(), &format, &num_pixel_formats) !=
      kCGLNoError) {
    LOG(ERROR) << "Error choosing pixel format.";
    return false;
  }
  if (!format) {
    LOG(ERROR) << "format == 0.";
    return false;
  }
  CGLReleasePixelFormat(format);
  DCHECK_NE(num_pixel_formats, 0);
  initialized = true;
  return true;
}

bool InitializeStaticCGLInternal(GLImplementation implementation) {
  base::NativeLibrary library =
      base::LoadNativeLibrary(base::FilePath(kOpenGLFrameworkPath), nullptr);
  if (!library) {
    LOG(ERROR) << "OpenGL framework not found";
    return false;
  }

  AddGLNativeLibrary(library);
  SetGLImplementation(implementation);

  InitializeStaticGLBindingsGL();
  return true;
}

#if defined(USE_EGL)
const char kGLESv2ANGLELibraryName[] = "libGLESv2.dylib";
const char kEGLANGLELibraryName[] = "libEGL.dylib";

bool InitializeStaticEGLInternalFromLibrary(GLImplementation implementation) {
#if BUILDFLAG(USE_STATIC_ANGLE)
  NOTREACHED();
#endif

  // Some unit test targets depend on Angle/SwiftShader but aren't built
  // as app bundles. In that case, the .dylib is next to the executable.
  base::FilePath base_dir;
  if (base::mac::AmIBundled()) {
    base_dir = base::mac::FrameworkBundlePath().Append("Libraries");
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
#endif  // defined(USE_EGL)

}  // namespace

GLDisplay* InitializeGLOneOffPlatform(uint64_t system_device_id) {
  GLDisplayEGL* display = GetDisplayEGL(system_device_id);
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
    case kGLImplementationDesktopGLCoreProfile:
      if (!InitializeOneOffForSandbox()) {
        LOG(ERROR) << "GLSurfaceCGL::InitializeOneOff failed.";
      }
      break;
#if defined(USE_EGL)
    case kGLImplementationEGLGLES2:
    case kGLImplementationEGLANGLE:
      if (!display->Initialize(EGLDisplayPlatform(0))) {
        LOG(ERROR) << "GLDisplayEGL::Initialize failed.";
        return nullptr;
      }
      break;
#endif  // defined(USE_EGL)
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
    case kGLImplementationDesktopGL:
    case kGLImplementationDesktopGLCoreProfile:
      return InitializeStaticCGLInternal(implementation.gl);
#if defined(USE_EGL)
    case kGLImplementationEGLGLES2:
    case kGLImplementationEGLANGLE:
      return InitializeStaticEGLInternal(implementation);
#endif  // #if defined(USE_EGL)
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      SetGLImplementationParts(implementation);
      InitializeStaticGLBindingsGL();
      return true;
    default:
      NOTREACHED();
  }

  return false;
}

void ShutdownGLPlatform(GLDisplay* display) {
  ClearBindingsGL();
#if defined(USE_EGL)
  if (display)
    display->Shutdown();
  ClearBindingsEGL();
#endif  // defined(USE_EGL)
}

}  // namespace init
}  // namespace gl

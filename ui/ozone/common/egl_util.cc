// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/egl_util.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"

#if BUILDFLAG(USE_OPENGL_APITRACE)
#include <stdlib.h>
#endif

namespace ui {
namespace {

#if defined(OS_WIN)
const base::FilePath::CharType kDefaultEglSoname[] =
    FILE_PATH_LITERAL("libEGL.dll");
const base::FilePath::CharType kDefaultGlesSoname[] =
    FILE_PATH_LITERAL("libGLESv2.dll");
const base::FilePath::CharType kAngleEglSoname[] =
    FILE_PATH_LITERAL("libEGL.dll");
const base::FilePath::CharType kAngleGlesSoname[] =
    FILE_PATH_LITERAL("libGLESv2.dll");
#else
#if defined(OS_FUCHSIA)
const base::FilePath::CharType kDefaultEglSoname[] =
    FILE_PATH_LITERAL("libEGL.so");
const base::FilePath::CharType kDefaultGlesSoname[] =
    FILE_PATH_LITERAL("libGLESv2.so");
#else  // !defined(OS_FUCHSIA)
const base::FilePath::CharType kDefaultEglSoname[] =
    FILE_PATH_LITERAL("libEGL.so.1");
const base::FilePath::CharType kDefaultGlesSoname[] =
    FILE_PATH_LITERAL("libGLESv2.so.2");
#endif
const base::FilePath::CharType kAngleEglSoname[] =
    FILE_PATH_LITERAL("libEGL.so");
const base::FilePath::CharType kAngleGlesSoname[] =
    FILE_PATH_LITERAL("libGLESv2.so");
#endif  // !defined(OS_WIN)

#if BUILDFLAG(ENABLE_SWIFTSHADER)
#if defined(OS_WIN)
const base::FilePath::CharType kGLESv2SwiftShaderLibraryName[] =
    FILE_PATH_LITERAL("libGLESv2.dll");
const base::FilePath::CharType kEGLSwiftShaderLibraryName[] =
    FILE_PATH_LITERAL("libEGL.dll");
#elif defined(OS_FUCHSIA)
const base::FilePath::CharType kGLESv2SwiftShaderLibraryName[] =
    FILE_PATH_LITERAL("libswiftshader_libGLESv2.so");
const base::FilePath::CharType kEGLSwiftShaderLibraryName[] =
    FILE_PATH_LITERAL("libswiftshader_libEGL.so");
#else   // !defined(OS_WIN) && !defined(OS_FUCHSIA)
const base::FilePath::CharType kGLESv2SwiftShaderLibraryName[] =
    FILE_PATH_LITERAL("libGLESv2.so");
const base::FilePath::CharType kEGLSwiftShaderLibraryName[] =
    FILE_PATH_LITERAL("libEGL.so");
#endif  // !defined(OS_WIN) && !defined(OS_FUCHSIA)
#endif  // BUILDFLAG(ENABLE_SWIFTSHADER)

bool LoadEGLGLES2Bindings(const base::FilePath& egl_library_path,
                          const base::FilePath& gles_library_path) {
  base::NativeLibraryLoadError error;
  base::NativeLibrary gles_library =
      base::LoadNativeLibrary(gles_library_path, &error);
  if (!gles_library) {
    LOG(ERROR) << "Failed to load GLES library: " << gles_library_path << ": "
               << error.ToString();
    return false;
  }

  base::NativeLibrary egl_library =
      base::LoadNativeLibrary(base::FilePath(egl_library_path), &error);
  if (!egl_library) {
    LOG(ERROR) << "Failed to load EGL library: " << egl_library_path << ": "
               << error.ToString();
    base::UnloadNativeLibrary(gles_library);
    return false;
  }

  gl::GLGetProcAddressProc get_proc_address =
      reinterpret_cast<gl::GLGetProcAddressProc>(
          base::GetFunctionPointerFromNativeLibrary(egl_library,
                                                    "eglGetProcAddress"));
  if (!get_proc_address) {
    LOG(ERROR) << "eglGetProcAddress not found.";
    base::UnloadNativeLibrary(egl_library);
    base::UnloadNativeLibrary(gles_library);
    return false;
  }

  gl::SetGLGetProcAddressProc(get_proc_address);

#if BUILDFLAG(USE_OPENGL_APITRACE)
  constexpr char kTraceLibegl[] = "TRACE_LIBEGL";
  constexpr char kTraceLibglesv2[] = "TRACE_LIBGLESV2";
  constexpr char kTraceFile[] = "TRACE_FILE";

  if (egl_library_path.BaseName().value() != kDefaultEglSoname) {
    LOG(ERROR) << "Unsupported EGL library '"
               << egl_library_path.BaseName().value()
               << "'. egltrace may not work.";
  }
  if (gles_library_path.BaseName().value() != kDefaultGlesSoname) {
    LOG(ERROR) << "Unsupported GLESv2 library '"
               << gles_library_path.BaseName().value()
               << "'. egltrace may not work.";
  }

  // Send correct library names to egttrace.
  setenv(kTraceLibegl, egl_library_path.BaseName().value().c_str(),
         /*overwrite=*/0);
  setenv(kTraceLibglesv2, gles_library_path.BaseName().value().c_str(),
         /*overwrite=*/0);
#if defined(OS_CHROMEOS)
  setenv(kTraceFile, "/tmp/gltrace.dat", /*overwrite=*/0);
#else
  if (!getenv(kTraceFile)) {
    LOG(ERROR) << "egltrace requires valid TRACE_FILE environment variable but "
                  "none were found. Chrome will probably crash.";
  }
#endif

  LOG(WARNING) << "Loading egltrace.so with TRACE_LIBEGL="
               << getenv(kTraceLibegl)
               << " TRACE_LIBGLESV2=" << getenv(kTraceLibglesv2)
               << " TRACE_FILE=" << getenv(kTraceFile);
  const base::FilePath::CharType kDefaultTraceSoname[] =
      FILE_PATH_LITERAL("egltrace.so");
  base::NativeLibrary trace_library =
      base::LoadNativeLibrary(base::FilePath(kDefaultTraceSoname), &error);
  gl::AddGLNativeLibrary(trace_library);
#endif

  gl::AddGLNativeLibrary(egl_library);
  gl::AddGLNativeLibrary(gles_library);

  return true;
}

}  // namespace

bool LoadDefaultEGLGLES2Bindings(gl::GLImplementation implementation) {
  base::FilePath glesv2_path;
  base::FilePath egl_path;

  if (implementation == gl::kGLImplementationSwiftShaderGL) {
#if BUILDFLAG(ENABLE_SWIFTSHADER)
    base::FilePath module_path;
#if !defined(OS_FUCHSIA)
    if (!base::PathService::Get(base::DIR_MODULE, &module_path))
      return false;
    module_path = module_path.Append(FILE_PATH_LITERAL("swiftshader/"));
#endif

    glesv2_path = module_path.Append(kGLESv2SwiftShaderLibraryName);
    egl_path = module_path.Append(kEGLSwiftShaderLibraryName);
#else
    return false;
#endif
  } else if (implementation == gl::kGLImplementationEGLANGLE) {
    glesv2_path = base::FilePath(kAngleGlesSoname);
    egl_path = base::FilePath(kAngleEglSoname);
  } else {
    glesv2_path = base::FilePath(kDefaultGlesSoname);
    egl_path = base::FilePath(kDefaultEglSoname);
  }

  return LoadEGLGLES2Bindings(egl_path, glesv2_path);
}

EGLConfig ChooseEGLConfig(EGLDisplay display, const int32_t* attributes) {
  int32_t num_configs;
  if (!eglChooseConfig(display, attributes, nullptr, 0, &num_configs)) {
    LOG(ERROR) << "eglChooseConfig failed with error "
               << GetLastEGLErrorString();
    return nullptr;
  }

  if (num_configs == 0) {
    LOG(ERROR) << "No suitable EGL configs found.";
    return nullptr;
  }

  EGLConfig config;
  if (!eglChooseConfig(display, attributes, &config, 1, &num_configs)) {
    LOG(ERROR) << "eglChooseConfig failed with error "
               << GetLastEGLErrorString();
    return nullptr;
  }
  return config;
}

}  // namespace ui

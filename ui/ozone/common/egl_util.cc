// Copyright 2014 The Chromium Authors
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

#if BUILDFLAG(IS_FUCHSIA)
const base::FilePath::CharType kDefaultEglSoname[] =
    FILE_PATH_LITERAL("libEGL.so");
const base::FilePath::CharType kDefaultGlesSoname[] =
    FILE_PATH_LITERAL("libGLESv2.so");
#else  // BUILDFLAG(IS_FUCHSIA)
const base::FilePath::CharType kDefaultEglSoname[] =
    FILE_PATH_LITERAL("libEGL.so.1");
const base::FilePath::CharType kDefaultGlesSoname[] =
    FILE_PATH_LITERAL("libGLESv2.so.2");
#endif
const base::FilePath::CharType kAngleEglSoname[] =
    FILE_PATH_LITERAL("libEGL.so");
const base::FilePath::CharType kAngleGlesSoname[] =
    FILE_PATH_LITERAL("libGLESv2.so");

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
#if BUILDFLAG(IS_CHROMEOS)
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
  if (!trace_library)
    LOG(ERROR) << error.ToString();
  gl::AddGLNativeLibrary(trace_library);
#endif

  gl::AddGLNativeLibrary(egl_library);
  gl::AddGLNativeLibrary(gles_library);

  return true;
}

}  // namespace

bool LoadDefaultEGLGLES2Bindings(
    const gl::GLImplementationParts& implementation) {
  base::FilePath glesv2_path;
  base::FilePath egl_path;

  if (implementation.gl == gl::kGLImplementationEGLANGLE) {
    base::FilePath module_path;
#if !BUILDFLAG(IS_FUCHSIA)
    if (!base::PathService::Get(base::DIR_MODULE, &module_path))
      return false;
#endif

    glesv2_path = module_path.Append(kAngleGlesSoname);
    egl_path = module_path.Append(kAngleEglSoname);
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

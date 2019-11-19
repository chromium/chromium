// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMPLEMENTATION_H_
#define UI_GL_GL_IMPLEMENTATION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/native_library.h"
#include "build/build_config.h"
#include "ui/gfx/extension_set.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_switches.h"

namespace gl {

class GLApi;

// The GL implementation currently in use.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. It should match enum GLImplementation
// in /tool/metrics/histograms/enums.xml
enum GLImplementation {
  kGLImplementationNone = 0,
  kGLImplementationDesktopGL = 1,
  kGLImplementationDesktopGLCoreProfile = 2,
  kGLImplementationSwiftShaderGL = 3,
  kGLImplementationAppleGL = 4,
  kGLImplementationEGLGLES2 = 5,  // Native EGL/GLES2
  kGLImplementationMockGL = 6,
  kGLImplementationStubGL = 7,
  kGLImplementationDisabled = 8,
  kGLImplementationEGLANGLE = 9,  // EGL/GL implemented using ANGLE
  kMaxValue = kGLImplementationEGLANGLE,
};

struct GL_EXPORT GLWindowSystemBindingInfo {
  GLWindowSystemBindingInfo();
  ~GLWindowSystemBindingInfo();
  std::string vendor;
  std::string version;
  std::string extensions;
  std::string direct_rendering_version;
};

using GLFunctionPointerType = void (*)();
#if defined(OS_WIN)
typedef GLFunctionPointerType(WINAPI* GLGetProcAddressProc)(const char* name);
#else
typedef GLFunctionPointerType (*GLGetProcAddressProc)(const char* name);
#endif

// Initialize stub methods for drawing operations in the GL bindings. The
// null draw bindings default to enabled, so that draw operations do nothing.
GL_EXPORT void InitializeNullDrawGLBindings();

// TODO(danakj): Remove this when all test suites are using null-draw.
GL_EXPORT bool HasInitializedNullDrawGLBindings();

// Filter a list of disabled_extensions from GL style space-separated
// extension_list, returning a space separated list of filtered extensions, in
// the same order as the input.
GL_EXPORT std::string FilterGLExtensionList(
    const char* extension_list,
    const std::vector<std::string>& disabled_extensions);

// Once initialized, instantiating this turns the stub methods for drawing
// operations off allowing drawing will occur while the object is alive.
class GL_EXPORT DisableNullDrawGLBindings {
 public:
  DisableNullDrawGLBindings();
  ~DisableNullDrawGLBindings();

 private:
  bool initial_enabled_;
};

// Set the current GL implementation.
GL_EXPORT void SetGLImplementation(GLImplementation implementation);

// Get the current GL implementation.
GL_EXPORT GLImplementation GetGLImplementation();

// Get the software GL implementation for the current platform.
GL_EXPORT GLImplementation GetSoftwareGLImplementation();

// Does the underlying GL support all features from Desktop GL 2.0 that were
// removed from the ES 2.0 spec without requiring specific extension strings.
GL_EXPORT bool HasDesktopGLFeatures();

// Get the GL implementation with a given name.
GL_EXPORT GLImplementation GetNamedGLImplementation(const std::string& name);

// Get the name of a GL implementation.
GL_EXPORT const char* GetGLImplementationName(GLImplementation implementation);

// Add a native library to those searched for GL entry points.
GL_EXPORT void AddGLNativeLibrary(base::NativeLibrary library);

// Unloads all native libraries.
GL_EXPORT void UnloadGLNativeLibraries(bool due_to_fallback);

// Set an additional function that will be called to find GL entry points.
// Exported so that tests may set the function used in the mock implementation.
GL_EXPORT void SetGLGetProcAddressProc(GLGetProcAddressProc proc);

// Find an entry point in the current GL implementation. Note that the function
// may return a non-null pointer to something else than the GL function if an
// unsupported function is queried. Spec-compliant eglGetProcAddress and
// glxGetProcAddress are allowed to return garbage for unsupported functions,
// and when querying functions from the EGL library supplied by Android, it may
// return a function that prints a log message about the function being
// unsupported.
GL_EXPORT GLFunctionPointerType GetGLProcAddress(const char* name);

// Helper for fetching the OpenGL extensions from the current context.
// This helper abstracts over differences between the desktop OpenGL
// core profile, and OpenGL ES and the compatibility profile.  It's
// intended for users of the bindings, not the implementation of the
// bindings themselves. This is a relatively expensive call, so
// callers should cache the result.
GL_EXPORT std::string GetGLExtensionsFromCurrentContext();
GL_EXPORT std::string GetGLExtensionsFromCurrentContext(GLApi* api);

GL_EXPORT gfx::ExtensionSet GetRequestableGLExtensionsFromCurrentContext();
GL_EXPORT gfx::ExtensionSet GetRequestableGLExtensionsFromCurrentContext(
    GLApi* api);

// Helper for the GL bindings implementation to understand whether
// glGetString(GL_EXTENSIONS) or glGetStringi(GL_EXTENSIONS, i) will
// be used in the function above.
GL_EXPORT bool WillUseGLGetStringForExtensions();
GL_EXPORT bool WillUseGLGetStringForExtensions(GLApi* api);

// Helpers to load a library and log error on failure.
GL_EXPORT base::NativeLibrary LoadLibraryAndPrintError(
    const base::FilePath::CharType* filename);
GL_EXPORT base::NativeLibrary LoadLibraryAndPrintError(
    const base::FilePath& filename);

}  // namespace gl

#endif  // UI_GL_GL_IMPLEMENTATION_H_

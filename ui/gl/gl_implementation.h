// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMPLEMENTATION_H_
#define UI_GL_GL_IMPLEMENTATION_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/native_library.h"
#include "build/build_config.h"
#include "ui/gfx/extension_set.h"
#include "ui/gl/angle_implementation.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_switches.h"

namespace base {
class CommandLine;
}

namespace gl {

class GLApi;

// The GL implementation currently in use.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. It should match enum GLImplementation
// in /tool/metrics/histograms/enums.xml
enum GLImplementation {
  kGLImplementationNone = 0,
  // Note: 1, 2, 3, 4 are skipped and should not be reused.
  // 1 used to be desktop GL.
  // 2 used to be desktop GL core profile.
  // 3 used to be legacy SwiftShader.
  // 4 used to be Apple's software GL.
  kGLImplementationEGLGLES2 = 5,  // Native EGL/GLES2
  kGLImplementationMockGL = 6,
  kGLImplementationStubGL = 7,
  kGLImplementationDisabled = 8,
  kGLImplementationEGLANGLE = 9,  // EGL/GL implemented using ANGLE
  kMaxValue = kGLImplementationEGLANGLE,
};

struct GL_EXPORT GLImplementationParts {
  constexpr explicit GLImplementationParts(const ANGLEImplementation angle_impl)
      : gl(kGLImplementationEGLANGLE),
        angle(MakeANGLEImplementation(kGLImplementationEGLANGLE, angle_impl)) {}

  constexpr explicit GLImplementationParts(const GLImplementation gl_impl)
      : gl(gl_impl),
        angle(MakeANGLEImplementation(gl_impl, ANGLEImplementation::kDefault)) {
  }

  GLImplementation gl = kGLImplementationNone;
  ANGLEImplementation angle = ANGLEImplementation::kNone;

  constexpr bool operator==(const GLImplementationParts& other) const {
    return (gl == other.gl && angle == other.angle);
  }
  constexpr bool operator!=(const GLImplementationParts& other) const {
    return !operator==(other);
  }

  constexpr bool operator==(const ANGLEImplementation angle_impl) const {
    return operator==(GLImplementationParts(angle_impl));
  }
  constexpr bool operator!=(const ANGLEImplementation angle_impl) const {
    return !operator==(angle_impl);
  }

  constexpr bool operator==(const GLImplementation gl_impl) const {
    return operator==(GLImplementationParts(gl_impl));
  }
  constexpr bool operator!=(const GLImplementation gl_impl) const {
    return !operator==(gl_impl);
  }

  bool IsValid() const;
  bool IsAllowed(const std::vector<GLImplementationParts>& allowed_impls) const;
  std::string ToString() const;
  std::string GLString() const;
  std::string ANGLEString() const;

 private:
  static constexpr ANGLEImplementation MakeANGLEImplementation(
      const GLImplementation gl_impl,
      const ANGLEImplementation angle_impl) {
    if (gl_impl == kGLImplementationEGLANGLE) {
      if (angle_impl == ANGLEImplementation::kNone) {
        return ANGLEImplementation::kDefault;
      } else {
        return angle_impl;
      }
    } else {
      return ANGLEImplementation::kNone;
    }
  }
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
#if BUILDFLAG(IS_WIN)
typedef GLFunctionPointerType(WINAPI* GLGetProcAddressProc)(const char* name);
#define STDCALL __stdcall
#else
typedef GLFunctionPointerType (*GLGetProcAddressProc)(const char* name);
#define STDCALL
#endif

// Sets stub methods for drawing operations in the GL bindings. The
// null draw bindings default to enabled, so that draw operations do nothing.
GL_EXPORT void SetNullDrawGLBindings(bool enabled);

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

// Set the current GL and ANGLE implementation.
GL_EXPORT void SetGLImplementationParts(
    const GLImplementationParts& implementation);

// Get the current GL and ANGLE implementation.
GL_EXPORT const GLImplementationParts& GetGLImplementationParts();

// Set the current GL implementation.
GL_EXPORT void SetGLImplementation(GLImplementation implementation);

// Get the current GL implementation.
GL_EXPORT GLImplementation GetGLImplementation();

// Set the current ANGLE implementation.
GL_EXPORT void SetANGLEImplementation(ANGLEImplementation implementation);

// Get the current ANGLE implementation.
GL_EXPORT ANGLEImplementation GetANGLEImplementation();

// Get the software GL implementation
GL_EXPORT GLImplementationParts GetSoftwareGLImplementation();

// Set the software GL implementation on the provided command line
GL_EXPORT void SetSoftwareGLCommandLineSwitches(
    base::CommandLine* command_line);

// Set the software WebGL implementation on the provided command line
GL_EXPORT void SetSoftwareWebGLCommandLineSwitches(
    base::CommandLine* command_line);

// Return requested GL implementation by checking commandline. If there isn't
// gl related argument, nullopt is returned.
GL_EXPORT std::optional<GLImplementationParts>
GetRequestedGLImplementationFromCommandLine(
    const base::CommandLine* command_line);

// Whether the implementation is one of the software GL implementations
GL_EXPORT bool IsSoftwareGLImplementation(GLImplementationParts implementation);

// Get the GL implementation with a given name.
GL_EXPORT GLImplementationParts
GetNamedGLImplementation(const std::string& gl_name,
                         const std::string& angle_name);

// Get the name of a GL implementation.
GL_EXPORT const char* GetGLImplementationGLName(
    GLImplementationParts implementation);
GL_EXPORT const char* GetGLImplementationANGLEName(
    GLImplementationParts implementation);

// Add a native library to those searched for GL entry points.
GL_EXPORT void AddGLNativeLibrary(base::NativeLibrary library);

// Unloads all native libraries.
GL_EXPORT void UnloadGLNativeLibraries(bool due_to_fallback);

// Set an additional function that will be called to find GL entry points.
// Exported so that tests may set the function used in the mock implementation.
GL_EXPORT void SetGLGetProcAddressProc(GLGetProcAddressProc proc);

// Find an entry point in the current GL implementation. Note that the function
// may return a non-null pointer to something else than the GL function if an
// unsupported function is queried. Spec-compliant eglGetProcAddress is allowed
// to return garbage for unsupported functions, and when querying functions
// from the EGL library supplied by Android, it may return a function that
// prints a log message about the function being unsupported.
STDCALL GL_EXPORT GLFunctionPointerType GetGLProcAddress(const char* name);

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

// Helpers to load a library and log error on failure.
GL_EXPORT base::NativeLibrary LoadLibraryAndPrintError(
    const base::FilePath::CharType* filename);
GL_EXPORT base::NativeLibrary LoadLibraryAndPrintError(
    const base::FilePath& filename);

#if BUILDFLAG(USE_OPENGL_APITRACE)
// Notify end of frame at buffer swap request.
GL_EXPORT void TerminateFrame();
#endif

}  // namespace gl

#endif  // UI_GL_GL_IMPLEMENTATION_H_

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_implementation.h"

#include <stddef.h>

#include <cstdlib>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_version_info.h"

namespace gl {

bool GLImplementationParts::IsValid() const {
  if (angle == ANGLEImplementation::kNone) {
    return (gl != kGLImplementationEGLANGLE);
  } else {
    return (gl == kGLImplementationEGLANGLE);
  }
}

bool GLImplementationParts::IsAllowed(
    const std::vector<GLImplementationParts>& allowed_impls) const {
  // Given a vector of GLImplementationParts, this function checks if "this"
  // GLImplementation is found in the list, with a special case where if "this"
  // implementation is kDefault, and we see any ANGLE implementation in the
  // list, then we allow "this" implementation, or vice versa, if "this" is
  // any ANGLE implementation, and we see kDefault in the list, "this" is
  // allowed.
  for (const GLImplementationParts& impl_iter : allowed_impls) {
    if (gl == kGLImplementationEGLANGLE &&
        impl_iter.gl == kGLImplementationEGLANGLE) {
      if (impl_iter.angle == ANGLEImplementation::kDefault) {
        return true;
      } else if (angle == ANGLEImplementation::kDefault) {
        return true;
      } else if (angle == impl_iter.angle) {
        return true;
      }
    } else if (gl == impl_iter.gl) {
      return true;
    }
  }
  return false;
}

std::string GLImplementationParts::ToString() const {
  std::stringstream s;
  s << "(gl=";
  s << GLString();
  s << ",angle=";
  s << ANGLEString();
  s << ")";
  return s.str();
}

std::string GLImplementationParts::GLString() const {
  switch (gl) {
    case GLImplementation::kGLImplementationNone:
      return "none";
    case GLImplementation::kGLImplementationEGLGLES2:
      return "egl-gles2";
    case GLImplementation::kGLImplementationMockGL:
      return "mock-gl";
    case GLImplementation::kGLImplementationStubGL:
      return "stub-gl";
    case GLImplementation::kGLImplementationDisabled:
      return "disabled";
    case GLImplementation::kGLImplementationEGLANGLE:
      return "egl-angle";
  }
  return "";
}

std::string GLImplementationParts::ANGLEString() const {
  switch (angle) {
    case ANGLEImplementation::kNone:
      return "none";
    case ANGLEImplementation::kD3D9:
      return "d3d9";
    case ANGLEImplementation::kD3D11:
      return "d3d11";
    case ANGLEImplementation::kOpenGL:
      return "opengl";
    case ANGLEImplementation::kOpenGLES:
      return "opengles";
    case ANGLEImplementation::kNull:
      return "null";
    case ANGLEImplementation::kVulkan:
      return "vulkan";
    case ANGLEImplementation::kSwiftShader:
      return "swiftshader";
    case ANGLEImplementation::kMetal:
      return "metal";
    case ANGLEImplementation::kDefault:
      return "default";
  }
  return "";
}

namespace {

const struct {
  const char* gl_name;
  const char* angle_name;
  GLImplementationParts implementation;
} kGLImplementationNamePairs[] = {
    {kGLImplementationEGLName, kANGLEImplementationNoneName,
     GLImplementationParts(kGLImplementationEGLGLES2)},
    {kGLImplementationANGLEName, kANGLEImplementationNoneName,
     GLImplementationParts(ANGLEImplementation::kDefault)},
    {kGLImplementationANGLEName, kANGLEImplementationDefaultName,
     GLImplementationParts(ANGLEImplementation::kDefault)},
    {kGLImplementationANGLEName, kANGLEImplementationD3D9Name,
     GLImplementationParts(ANGLEImplementation::kD3D9)},
    {kGLImplementationANGLEName, kANGLEImplementationD3D11Name,
     GLImplementationParts(ANGLEImplementation::kD3D11)},
    {kGLImplementationANGLEName, kANGLEImplementationD3D11on12Name,
     GLImplementationParts(ANGLEImplementation::kD3D11)},
    {kGLImplementationANGLEName, kANGLEImplementationD3D11NULLName,
     GLImplementationParts(ANGLEImplementation::kD3D11)},
    {kGLImplementationANGLEName, kANGLEImplementationOpenGLName,
     GLImplementationParts(ANGLEImplementation::kOpenGL)},
    {kGLImplementationANGLEName, kANGLEImplementationOpenGLEGLName,
     GLImplementationParts(ANGLEImplementation::kOpenGL)},
    {kGLImplementationANGLEName, kANGLEImplementationOpenGLNULLName,
     GLImplementationParts(ANGLEImplementation::kOpenGL)},
    {kGLImplementationANGLEName, kANGLEImplementationOpenGLESName,
     GLImplementationParts(ANGLEImplementation::kOpenGLES)},
    {kGLImplementationANGLEName, kANGLEImplementationOpenGLESEGLName,
     GLImplementationParts(ANGLEImplementation::kOpenGLES)},
    {kGLImplementationANGLEName, kANGLEImplementationOpenGLESNULLName,
     GLImplementationParts(ANGLEImplementation::kOpenGLES)},
    {kGLImplementationANGLEName, kANGLEImplementationVulkanName,
     GLImplementationParts(ANGLEImplementation::kVulkan)},
    {kGLImplementationANGLEName, kANGLEImplementationVulkanNULLName,
     GLImplementationParts(ANGLEImplementation::kVulkan)},
    {kGLImplementationANGLEName, kANGLEImplementationMetalName,
     GLImplementationParts(ANGLEImplementation::kMetal)},
    {kGLImplementationANGLEName, kANGLEImplementationMetalNULLName,
     GLImplementationParts(ANGLEImplementation::kMetal)},
    {kGLImplementationANGLEName, kANGLEImplementationSwiftShaderName,
     GLImplementationParts(ANGLEImplementation::kSwiftShader)},
    {kGLImplementationANGLEName, kANGLEImplementationSwiftShaderForWebGLName,
     GLImplementationParts(ANGLEImplementation::kSwiftShader)},
    {kGLImplementationANGLEName, kANGLEImplementationNullName,
     GLImplementationParts(ANGLEImplementation::kNull)},
    {kGLImplementationMockName, kANGLEImplementationNoneName,
     GLImplementationParts(kGLImplementationMockGL)},
    {kGLImplementationStubName, kANGLEImplementationNoneName,
     GLImplementationParts(kGLImplementationStubGL)},
    {kGLImplementationDisabledName, kANGLEImplementationNoneName,
     GLImplementationParts(kGLImplementationDisabled)}};

typedef std::vector<base::NativeLibrary> LibraryArray;

GLImplementationParts g_gl_implementation =
    GLImplementationParts(kGLImplementationNone);
LibraryArray* g_libraries;
GLGetProcAddressProc g_get_proc_address;

void CleanupNativeLibraries(void* due_to_fallback) {
  if (g_libraries) {
#if BUILDFLAG(IS_MAC)
    // Mac `NativeLibrary` is heap-allocated, so always unload to ensure they're
    // freed.
    for (auto* library : *g_libraries)
      base::UnloadNativeLibrary(library);
#else
    // We do not call base::UnloadNativeLibrary() for these libraries as
    // unloading libGL without closing X display is not allowed. See
    // https://crbug.com/250813 for details.
    // However, if we fallback to a software renderer (e.g., SwiftShader),
    // then the above concern becomes irrelevant.
    // During fallback from ANGLE to SwiftShader ANGLE library needs to
    // be unloaded, otherwise software SwiftShader loading will fail. See
    // https://crbug.com/760063 for details.
    // During fallback from VMware mesa to SwiftShader mesa libraries need
    // to be unloaded. See https://crbug.com/852537 for details.
    if (due_to_fallback && *static_cast<bool*>(due_to_fallback)) {
      for (auto* library : *g_libraries)
        base::UnloadNativeLibrary(library);
    }
#endif  // BUILDFLAG(IS_MAC)
    delete g_libraries;
    g_libraries = nullptr;
  }
}

gfx::ExtensionSet GetGLExtensionsFromCurrentContext(GLApi* api,
                                                    GLenum extensions_enum) {
  const char* extensions =
      reinterpret_cast<const char*>(api->glGetStringFn(extensions_enum));
  return extensions ? gfx::MakeExtensionSet(extensions) : gfx::ExtensionSet();
}

}  // namespace

EGLApi* g_current_egl_context;

GLImplementationParts GetNamedGLImplementation(const std::string& gl_name,
                                               const std::string& angle_name) {
  for (auto name_pair : kGLImplementationNamePairs) {
    if (gl_name == name_pair.gl_name && angle_name == name_pair.angle_name)
      return name_pair.implementation;
  }

  return GLImplementationParts(kGLImplementationNone);
}

GLImplementationParts GetSoftwareGLImplementation() {
  return GLImplementationParts(ANGLEImplementation::kSwiftShader);
}

bool IsSoftwareGLImplementation(GLImplementationParts implementation) {
  return (implementation == GetSoftwareGLImplementation());
}

void SetSoftwareGLCommandLineSwitches(base::CommandLine* command_line) {
  GLImplementationParts implementation = GetSoftwareGLImplementation();
  command_line->AppendSwitchASCII(
      switches::kUseGL, gl::GetGLImplementationGLName(implementation));
  command_line->AppendSwitchASCII(
      switches::kUseANGLE, gl::GetGLImplementationANGLEName(implementation));
}

void SetSoftwareWebGLCommandLineSwitches(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(switches::kUseGL, kGLImplementationANGLEName);
  command_line->AppendSwitchASCII(switches::kUseANGLE,
                                  kANGLEImplementationSwiftShaderForWebGLName);
}

std::optional<GLImplementationParts>
GetRequestedGLImplementationFromCommandLine(
    const base::CommandLine* command_line) {
  bool overrideUseSoftwareGL =
      command_line->HasSwitch(switches::kOverrideUseSoftwareGLForTests);
#if BUILDFLAG(IS_LINUX) || \
    (BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_CHROMEOS_DEVICE))
  if (std::getenv("RUNNING_UNDER_RR")) {
    // https://rr-project.org/ is a Linux-only record-and-replay debugger that
    // is unhappy when things like GPU drivers write directly into the
    // process's address space.  Using swiftshader helps ensure that doesn't
    // happen and keeps Chrome and linux-chromeos usable with rr.
    overrideUseSoftwareGL = true;
  }
#endif
  if (overrideUseSoftwareGL) {
    return GetSoftwareGLImplementation();
  }

  if (!command_line->HasSwitch(switches::kUseGL) &&
      !command_line->HasSwitch(switches::kUseANGLE)) {
    return std::nullopt;
  }

  std::string gl_name = command_line->GetSwitchValueASCII(switches::kUseGL);
  std::string angle_name =
      command_line->GetSwitchValueASCII(switches::kUseANGLE);

  // If --use-angle was specified but --use-gl was not, assume --use-gl=angle
  if (command_line->HasSwitch(switches::kUseANGLE) &&
      !command_line->HasSwitch(switches::kUseGL)) {
    gl_name = kGLImplementationANGLEName;
  }

  if ((gl_name == kGLImplementationANGLEName) &&
      ((angle_name == kANGLEImplementationSwiftShaderName) ||
       (angle_name == kANGLEImplementationSwiftShaderForWebGLName))) {
    return GLImplementationParts(ANGLEImplementation::kSwiftShader);
  }
  return GetNamedGLImplementation(gl_name, angle_name);
}

const char* GetGLImplementationGLName(GLImplementationParts implementation) {
  for (auto name_pair : kGLImplementationNamePairs) {
    if (implementation.gl == name_pair.implementation.gl &&
        implementation.angle == name_pair.implementation.angle)
      return name_pair.gl_name;
  }

  return "unknown";
}

const char* GetGLImplementationANGLEName(GLImplementationParts implementation) {
  for (auto name_pair : kGLImplementationNamePairs) {
    if (implementation.gl == name_pair.implementation.gl &&
        implementation.angle == name_pair.implementation.angle)
      return name_pair.angle_name;
  }

  return "not defined";
}

void SetGLImplementationParts(const GLImplementationParts& implementation) {
  DCHECK(implementation.IsValid());
  g_gl_implementation = GLImplementationParts(implementation);
}

const GLImplementationParts& GetGLImplementationParts() {
  return g_gl_implementation;
}

void SetGLImplementation(GLImplementation implementation) {
  g_gl_implementation = GLImplementationParts(implementation);
  DCHECK(g_gl_implementation.IsValid());
}

GLImplementation GetGLImplementation() {
  return g_gl_implementation.gl;
}

void SetANGLEImplementation(ANGLEImplementation implementation) {
  g_gl_implementation = GLImplementationParts(implementation);
  DCHECK(g_gl_implementation.IsValid());
}

ANGLEImplementation GetANGLEImplementation() {
  return g_gl_implementation.angle;
}

void AddGLNativeLibrary(base::NativeLibrary library) {
  DCHECK(library);

  if (!g_libraries) {
    g_libraries = new LibraryArray;
    base::AtExitManager::RegisterCallback(CleanupNativeLibraries, NULL);
  }

  g_libraries->push_back(library);
}

void UnloadGLNativeLibraries(bool due_to_fallback) {
  CleanupNativeLibraries(&due_to_fallback);
}

void SetGLGetProcAddressProc(GLGetProcAddressProc proc) {
  DCHECK(proc);
  g_get_proc_address = proc;
}

NO_SANITIZE("cfi-icall")
STDCALL GLFunctionPointerType GetGLProcAddress(const char* name) {
  DCHECK(g_gl_implementation.gl != kGLImplementationNone);

  if (g_libraries) {
    for (size_t i = 0; i < g_libraries->size(); ++i) {
      GLFunctionPointerType proc = reinterpret_cast<GLFunctionPointerType>(
          base::GetFunctionPointerFromNativeLibrary((*g_libraries)[i], name));
      if (proc)
        return proc;
    }
  }
  if (g_get_proc_address) {
    GLFunctionPointerType proc = g_get_proc_address(name);
    if (proc)
      return proc;
  }

  return NULL;
}

void SetNullDrawGLBindings(bool enabled) {
  SetNullDrawGLBindingsEnabled(enabled);
}

bool HasInitializedNullDrawGLBindings() {
  return GetNullDrawBindingsEnabled();
}

std::string FilterGLExtensionList(
    const char* extensions,
    const std::vector<std::string>& disabled_extensions) {
  if (extensions == NULL)
    return "";

  std::vector<std::string_view> extension_vec = base::SplitStringPiece(
      extensions, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  auto is_disabled = [&disabled_extensions](std::string_view ext) {
    return base::Contains(disabled_extensions, ext);
  };
  std::erase_if(extension_vec, is_disabled);

  return base::JoinString(extension_vec, " ");
}

DisableNullDrawGLBindings::DisableNullDrawGLBindings() {
  initial_enabled_ = SetNullDrawGLBindingsEnabled(false);
}

DisableNullDrawGLBindings::~DisableNullDrawGLBindings() {
  SetNullDrawGLBindingsEnabled(initial_enabled_);
}

GLWindowSystemBindingInfo::GLWindowSystemBindingInfo() {}
GLWindowSystemBindingInfo::~GLWindowSystemBindingInfo() {}

std::string GetGLExtensionsFromCurrentContext() {
  return GetGLExtensionsFromCurrentContext(g_current_gl_context);
}

std::string GetGLExtensionsFromCurrentContext(GLApi* api) {
  const char* extensions =
      reinterpret_cast<const char*>(api->glGetStringFn(GL_EXTENSIONS));
  return extensions ? std::string(extensions) : std::string();
}

gfx::ExtensionSet GetRequestableGLExtensionsFromCurrentContext() {
  return GetRequestableGLExtensionsFromCurrentContext(g_current_gl_context);
}

gfx::ExtensionSet GetRequestableGLExtensionsFromCurrentContext(GLApi* api) {
  return GetGLExtensionsFromCurrentContext(api,
                                           GL_REQUESTABLE_EXTENSIONS_ANGLE);
}

base::NativeLibrary LoadLibraryAndPrintError(
    const base::FilePath::CharType* filename) {
  return LoadLibraryAndPrintError(base::FilePath(filename));
}

base::NativeLibrary LoadLibraryAndPrintError(const base::FilePath& filename) {
  base::NativeLibraryLoadError error;
  base::NativeLibrary library = base::LoadNativeLibrary(filename, &error);
  if (!library) {
    LOG(ERROR) << "Failed to load " << filename.MaybeAsASCII() << ": "
               << error.ToString();
    return NULL;
  }
  return library;
}

#if BUILDFLAG(USE_OPENGL_APITRACE)
void TerminateFrame() {
  GetGLProcAddress("glFrameTerminatorGREMEDY")();
}
#endif

}  // namespace gl

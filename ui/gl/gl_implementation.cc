// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_implementation.h"

#include <stddef.h>

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_version_info.h"

namespace gl {

namespace {

const struct {
  const char* name;
  GLImplementation implementation;
} kGLImplementationNamePairs[] = {
    {kGLImplementationDesktopName, kGLImplementationDesktopGL},
    {kGLImplementationSwiftShaderName, kGLImplementationSwiftShaderGL},
#if defined(OS_MACOSX)
    {kGLImplementationAppleName, kGLImplementationAppleGL},
#endif
    {kGLImplementationEGLName, kGLImplementationEGLGLES2},
    {kGLImplementationANGLEName, kGLImplementationEGLANGLE},
    {kGLImplementationMockName, kGLImplementationMockGL},
    {kGLImplementationStubName, kGLImplementationStubGL},
    {kGLImplementationDisabledName, kGLImplementationDisabled}};

typedef std::vector<base::NativeLibrary> LibraryArray;

GLImplementation g_gl_implementation = kGLImplementationNone;
LibraryArray* g_libraries;
GLGetProcAddressProc g_get_proc_address;

void CleanupNativeLibraries(void* due_to_fallback) {
  if (g_libraries) {
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
    delete g_libraries;
    g_libraries = nullptr;
  }
}

gfx::ExtensionSet GetGLExtensionsFromCurrentContext(
    GLApi* api,
    GLenum extensions_enum,
    GLenum num_extensions_enum) {
  if (WillUseGLGetStringForExtensions(api)) {
    const char* extensions =
        reinterpret_cast<const char*>(api->glGetStringFn(extensions_enum));
    return extensions ? gfx::MakeExtensionSet(extensions) : gfx::ExtensionSet();
  }

  GLint num_extensions = 0;
  api->glGetIntegervFn(num_extensions_enum, &num_extensions);

  std::vector<base::StringPiece> exts(num_extensions);
  for (GLint i = 0; i < num_extensions; ++i) {
    const char* extension =
        reinterpret_cast<const char*>(api->glGetStringiFn(extensions_enum, i));
    DCHECK(extension != NULL);
    exts[i] = extension;
  }
  return gfx::ExtensionSet(exts);
}

}  // namespace

base::ThreadLocalPointer<CurrentGL>* g_current_gl_context_tls = NULL;

#if defined(USE_EGL)
EGLApi* g_current_egl_context;
#endif

#if defined(OS_WIN)
WGLApi* g_current_wgl_context;
#endif

#if defined(USE_GLX)
GLXApi* g_current_glx_context;
#endif

GLImplementation GetNamedGLImplementation(const std::string& name) {
  for (size_t i = 0; i < base::size(kGLImplementationNamePairs); ++i) {
    if (name == kGLImplementationNamePairs[i].name)
      return kGLImplementationNamePairs[i].implementation;
  }

  return kGLImplementationNone;
}

GLImplementation GetSoftwareGLImplementation() {
  return kGLImplementationSwiftShaderGL;
}

const char* GetGLImplementationName(GLImplementation implementation) {
  for (size_t i = 0; i < base::size(kGLImplementationNamePairs); ++i) {
    if (implementation == kGLImplementationNamePairs[i].implementation)
      return kGLImplementationNamePairs[i].name;
  }

  return "unknown";
}

void SetGLImplementation(GLImplementation implementation) {
  g_gl_implementation = implementation;
}

GLImplementation GetGLImplementation() {
  return g_gl_implementation;
}

bool HasDesktopGLFeatures() {
  return kGLImplementationDesktopGL == g_gl_implementation ||
         kGLImplementationDesktopGLCoreProfile == g_gl_implementation ||
         kGLImplementationAppleGL == g_gl_implementation;
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
GLFunctionPointerType GetGLProcAddress(const char* name) {
  DCHECK(g_gl_implementation != kGLImplementationNone);

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

void InitializeNullDrawGLBindings() {
  SetNullDrawGLBindingsEnabled(true);
}

bool HasInitializedNullDrawGLBindings() {
  return GetNullDrawBindingsEnabled();
}

std::string FilterGLExtensionList(
    const char* extensions,
    const std::vector<std::string>& disabled_extensions) {
  if (extensions == NULL)
    return "";

  std::vector<base::StringPiece> extension_vec = base::SplitStringPiece(
      extensions, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  auto is_disabled = [&disabled_extensions](const base::StringPiece& ext) {
    return base::Contains(disabled_extensions, ext);
  };
  base::EraseIf(extension_vec, is_disabled);

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
  if (WillUseGLGetStringForExtensions(api)) {
    const char* extensions =
        reinterpret_cast<const char*>(api->glGetStringFn(GL_EXTENSIONS));
    return extensions ? std::string(extensions) : std::string();
  }

  GLint num_extensions = 0;
  api->glGetIntegervFn(GL_NUM_EXTENSIONS, &num_extensions);

  std::vector<base::StringPiece> exts(num_extensions);
  for (GLint i = 0; i < num_extensions; ++i) {
    const char* extension =
        reinterpret_cast<const char*>(api->glGetStringiFn(GL_EXTENSIONS, i));
    DCHECK(extension != NULL);
    exts[i] = extension;
  }
  return base::JoinString(exts, " ");
}

gfx::ExtensionSet GetRequestableGLExtensionsFromCurrentContext() {
  return GetRequestableGLExtensionsFromCurrentContext(g_current_gl_context);
}

gfx::ExtensionSet GetRequestableGLExtensionsFromCurrentContext(GLApi* api) {
  return GetGLExtensionsFromCurrentContext(api, GL_REQUESTABLE_EXTENSIONS_ANGLE,
                                           GL_NUM_REQUESTABLE_EXTENSIONS_ANGLE);
}

bool WillUseGLGetStringForExtensions() {
  return WillUseGLGetStringForExtensions(g_current_gl_context);
}

bool WillUseGLGetStringForExtensions(GLApi* api) {
  const char* version_str =
      reinterpret_cast<const char*>(api->glGetStringFn(GL_VERSION));
  gfx::ExtensionSet extensions;
  GLVersionInfo version_info(version_str, nullptr, extensions);
  return version_info.is_es || version_info.major_version < 3;
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

}  // namespace gl

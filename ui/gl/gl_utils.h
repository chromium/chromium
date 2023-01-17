// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains some useful utilities for the ui/gl classes.

#ifndef UI_GL_GL_UTILS_H_
#define UI_GL_GL_UTILS_H_

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gpu_preference.h"

#if BUILDFLAG(IS_WIN)
#include <dxgi1_6.h>
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/files/scoped_file.h"
#endif

namespace gl {
class GLApi;
#if defined(USE_EGL)
class GLDisplayEGL;
#endif  // USE_EGL
#if defined(USE_GLX)
class GLDisplayX11;
#endif  // USE_GLX
class GLDisplay;

GL_EXPORT void Crash();
GL_EXPORT void Hang();

#if BUILDFLAG(IS_ANDROID)
GL_EXPORT base::ScopedFD MergeFDs(base::ScopedFD a, base::ScopedFD b);
#endif

GL_EXPORT bool UsePassthroughCommandDecoder(
    const base::CommandLine* command_line);

GL_EXPORT bool PassthroughCommandDecoderSupported();

#if BUILDFLAG(IS_WIN)
// Calculates present during in 100 ns from number of frames per second.
GL_EXPORT unsigned int FrameRateToPresentDuration(float frame_rate);

// BufferCount for the root surface swap chain.
GL_EXPORT unsigned int DirectCompositionRootSurfaceBufferCount();

// Labels swapchain with the name_prefix and ts buffers buffers with the string
// name_prefix + _Buffer_ + <buffer_number>.
GL_EXPORT void LabelSwapChainAndBuffers(IDXGISwapChain* swap_chain,
                                        const char* name_prefix);

// Same as LabelSwapChainAndBuffers, but only does the buffers. Used for resize
// operations.
GL_EXPORT void LabelSwapChainBuffers(IDXGISwapChain* swap_chain,
                                     const char* name_prefix);
#endif

// The following functions expose functionalities from GLDisplayManagerEGL
// and GLDisplayManagerX11 for access outside the ui/gl module. This is because
// the two GLDisplayManager classes are singletons and in component build,
// calling GetInstance() directly returns different instances in different
// components.
#if defined(USE_EGL)
// Add an entry <preference, system_device_id> to GLDisplayManagerEGL.
GL_EXPORT void SetGpuPreferenceEGL(GpuPreference preference,
                                   uint64_t system_device_id);

// Remove the entry at <preference> from GLDisplayManagerEGL.
GL_EXPORT void RemoveGpuPreferenceEGL(GpuPreference preference);

// Query the default GLDisplay. May return either a GLDisplayEGL or
// GLDisplayX11.
GL_EXPORT GLDisplay* GetDefaultDisplay();

// Query the GLDisplay by |gpu_preference|. May return either a GLDisplayEGL or
// GLDisplayX11.
GL_EXPORT GLDisplay* GetDisplay(GpuPreference gpu_preference);

// Query the default GLDisplayEGL.
GL_EXPORT GLDisplayEGL* GetDefaultDisplayEGL();

// Query the GLDisplayEGL by |gpu_preference|.
GL_EXPORT GLDisplayEGL* GetDisplayEGL(GpuPreference gpu_preference);
#endif  // USE_EGL

// Temporarily allows compilation of shaders that use the
// ARB_texture_rectangle/ANGLE_texture_rectangle extension. We don't want to
// expose the extension to WebGL user shaders but we still need to use it for
// parts of the implementation on macOS. Note that the extension is always
// enabled on macOS and this only controls shader compilation.
class GL_EXPORT ScopedEnableTextureRectangleInShaderCompiler {
 public:
  ScopedEnableTextureRectangleInShaderCompiler(
      const ScopedEnableTextureRectangleInShaderCompiler&) = delete;
  ScopedEnableTextureRectangleInShaderCompiler& operator=(
      const ScopedEnableTextureRectangleInShaderCompiler&) = delete;

  // This class is a no-op except on macOS.
#if !BUILDFLAG(IS_MAC)
  explicit ScopedEnableTextureRectangleInShaderCompiler(gl::GLApi* gl_api) {}

#else
  explicit ScopedEnableTextureRectangleInShaderCompiler(gl::GLApi* gl_api);
  ~ScopedEnableTextureRectangleInShaderCompiler();

 private:
  raw_ptr<gl::GLApi> gl_api_;
#endif
};

class GL_EXPORT ScopedPixelStore {
 public:
  ScopedPixelStore(unsigned int name, int value);
  ~ScopedPixelStore();
  ScopedPixelStore(ScopedPixelStore&) = delete;
  ScopedPixelStore& operator=(ScopedPixelStore&) = delete;

 private:
  const unsigned int name_;
  const int old_value_;
  const int value_;
};

}  // namespace gl

#endif  // UI_GL_GL_UTILS_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains some useful utilities for the ui/gl classes.

#include "ui/gl/gl_utils.h"

#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_display_manager.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_surface_egl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/posix/eintr_wrapper.h"
#include "third_party/libsync/src/include/sync/sync.h"  // nogncheck
#endif

#if BUILDFLAG(IS_WIN)
#include <d3d11_1.h>
#include "base/strings/stringprintf.h"
#include "ui/gl/debug_utils.h"
#include "ui/gl/direct_composition_support.h"
#endif

namespace gl {
namespace {

// The global set of workarounds.
GlWorkarounds g_workarounds;
bool g_is_angle_enabled = true;

int GetIntegerv(unsigned int name) {
  int value = 0;
  glGetIntegerv(name, &value);
  return value;
}

}  // namespace

// Used by chrome://gpucrash and gpu_benchmarking_extension's
// CrashForTesting.
void Crash() {
  DVLOG(1) << "GPU: Simulating GPU crash";
  // Good bye, cruel world.
  volatile int* it_s_the_end_of_the_world_as_we_know_it = nullptr;
  *it_s_the_end_of_the_world_as_we_know_it = 0xdead;
}

// Used by chrome://gpuhang.
void Hang() {
  DVLOG(1) << "GPU: Simulating GPU hang";
  int do_not_delete_me = 0;
  for (;;) {
    // Do not sleep here. The GPU watchdog timer tracks
    // the amount of user time this thread is using and
    // it doesn't use much while calling Sleep.

    // The following are multiple mechanisms to prevent compilers from
    // optimizing out the endless loop. Hope at least one of them works.
    base::debug::Alias(&do_not_delete_me);
    ++do_not_delete_me;

    __asm__ volatile("");
  }
}

#if BUILDFLAG(IS_ANDROID)
base::ScopedFD MergeFDs(base::ScopedFD a, base::ScopedFD b) {
  if (!a.is_valid())
    return b;
  if (!b.is_valid())
    return a;

  base::ScopedFD merged(HANDLE_EINTR(sync_merge("", a.get(), b.get())));
  if (!merged.is_valid())
    LOG(ERROR) << "Failed to merge fences.";
  return merged;
}

void DisableANGLE() {
  DCHECK_NE(GetGLImplementation(), kGLImplementationEGLANGLE);
  g_is_angle_enabled = false;
}
#endif

bool UsePassthroughCommandDecoder(const base::CommandLine* command_line) {
  if (!g_is_angle_enabled) {
    return false;
  }

  std::string switch_value;
  if (command_line->HasSwitch(switches::kUseCmdDecoder)) {
    switch_value = command_line->GetSwitchValueASCII(switches::kUseCmdDecoder);
  }

#if !BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER)
  if (switch_value == kCmdDecoderValidatingName) {
    LOG(WARNING) << "Ignoring request for the validating command decoder. It "
                    "is not supported on this platform.";
  }
  return true;
#else
  if (switch_value == kCmdDecoderPassthroughName) {
    return true;
  } else if (switch_value == kCmdDecoderValidatingName) {
    return false;
  } else {
    // Unrecognized or missing switch, use the default.
    return features::UsePassthroughCommandDecoder();
  }
#endif  // !BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER)
}

bool PassthroughCommandDecoderSupported() {
  GLDisplayEGL* display = gl::GLSurfaceEGL::GetGLDisplayEGL();
  // Using the passthrough command buffer requires that specific ANGLE
  // extensions are exposed
  return display->ext->b_EGL_CHROMIUM_create_context_bind_generates_resource &&
         display->ext->b_EGL_ANGLE_create_context_webgl_compatibility &&
         display->ext->b_EGL_ANGLE_robust_resource_initialization &&
         display->ext->b_EGL_ANGLE_display_texture_share_group &&
         display->ext->b_EGL_ANGLE_create_context_client_arrays;
}

const GlWorkarounds& GetGlWorkarounds() {
  return g_workarounds;
}

void SetGlWorkarounds(const GlWorkarounds& workarounds) {
  g_workarounds = workarounds;
}

#if BUILDFLAG(IS_WIN)
unsigned int FrameRateToPresentDuration(float frame_rate) {
  if (frame_rate == 0)
    return 0u;
  // Present duration unit is 100 ns.
  return static_cast<unsigned int>(1.0E7 / frame_rate);
}

unsigned int DirectCompositionRootSurfaceBufferCount() {
  return base::FeatureList::IsEnabled(features::kDCompTripleBufferRootSwapChain)
             ? 3u
             : 2u;
}

// Labels swapchain buffers with the string name_prefix + _Buffer_ +
// <buffer_number>
void LabelSwapChainBuffers(IDXGISwapChain* swap_chain,
                           const char* name_prefix) {
  DXGI_SWAP_CHAIN_DESC desc;
  HRESULT hr = swap_chain->GetDesc(&desc);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to GetDesc from swap chain: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }
  for (unsigned int i = 0; i < desc.BufferCount; i++) {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> swap_chain_buffer;
    hr = swap_chain->GetBuffer(i, IID_PPV_ARGS(&swap_chain_buffer));
    if (FAILED(hr)) {
      DLOG(ERROR) << "GetBuffer on swap chain buffer " << i
                  << "failed: " << logging::SystemErrorCodeToString(hr);
      return;
    }
    const std::string buffer_name =
        base::StringPrintf("%s_Buffer_%d", name_prefix, i);
    hr = SetDebugName(swap_chain_buffer.Get(), buffer_name.c_str());
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to label swap chain buffer " << i << ": "
                  << logging::SystemErrorCodeToString(hr);
    }
  }
}

// Same as LabelSwapChainAndBuffers, but only does the buffers. Used for resize
// operations
void LabelSwapChainAndBuffers(IDXGISwapChain* swap_chain,
                              const char* name_prefix) {
  SetDebugName(swap_chain, name_prefix);
  LabelSwapChainBuffers(swap_chain, name_prefix);
}
#endif  // BUILDFLAG(IS_WIN)

GLDisplay* GetDisplay(GpuPreference gpu_preference) {
  return GetDisplay(gpu_preference, gl::DisplayKey::kDefault);
}

GL_EXPORT GLDisplay* GetDisplay(GpuPreference gpu_preference,
                                gl::DisplayKey display_key) {
  // TODO(344606399): Consider making callers directly create the EGL display.
  return GLDisplayManagerEGL::GetInstance()->GetDisplay(gpu_preference,
                                                        display_key);
}

GLDisplay* GetDefaultDisplay() {
  return GetDisplay(GpuPreference::kDefault);
}

void SetGpuPreferenceEGL(GpuPreference preference, uint64_t system_device_id) {
  GLDisplayManagerEGL::GetInstance()->SetGpuPreference(preference,
                                                       system_device_id);
}

void RemoveGpuPreferenceEGL(GpuPreference preference) {
  GLDisplayManagerEGL::GetInstance()->RemoveGpuPreference(preference);
}

GLDisplayEGL* GetDefaultDisplayEGL() {
  return GLDisplayManagerEGL::GetInstance()->GetDisplay(
      GpuPreference::kDefault);
}

GLDisplayEGL* GetDisplayEGL(GpuPreference gpu_preference) {
  return GLDisplayManagerEGL::GetInstance()->GetDisplay(gpu_preference);
}

#if BUILDFLAG(IS_MAC)

ScopedEnableTextureRectangleInShaderCompiler::
    ScopedEnableTextureRectangleInShaderCompiler(gl::GLApi* gl_api) {
  if (gl_api) {
    DCHECK(!gl_api->glIsEnabledFn(GL_TEXTURE_RECTANGLE_ANGLE));
    gl_api->glEnableFn(GL_TEXTURE_RECTANGLE_ANGLE);
    gl_api_ = gl_api;
  } else {
    gl_api_ = nullptr;  // Signal to the destructor that this is a no-op.
  }
}

ScopedEnableTextureRectangleInShaderCompiler::
    ~ScopedEnableTextureRectangleInShaderCompiler() {
  if (gl_api_)
    gl_api_->glDisableFn(GL_TEXTURE_RECTANGLE_ANGLE);
}

#endif  // BUILDFLAG(IS_MAC)

ScopedPixelStore::ScopedPixelStore(unsigned int name, int value)
    : name_(name), old_value_(GetIntegerv(name)), value_(value) {
  if (value_ != old_value_)
    glPixelStorei(name_, value_);
}

ScopedPixelStore::~ScopedPixelStore() {
  if (value_ != old_value_)
    glPixelStorei(name_, old_value_);
}

}  // namespace gl

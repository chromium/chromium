// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains some useful utilities for the ui/gl classes.

#include "ui/gl/gl_utils.h"

#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/logging.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_switches.h"

#if defined(USE_EGL)
#include "ui/gl/gl_surface_egl.h"
#endif  // defined(USE_EGL)

#if defined(OS_ANDROID)
#include "base/posix/eintr_wrapper.h"
#include "third_party/libsync/src/include/sync/sync.h"
#endif

#if defined(OS_WIN)
#include "ui/gl/direct_composition_surface_win.h"
#endif

#if defined(USE_X11) || defined(USE_OZONE_PLATFORM_X11)
#include "ui/gfx/linux/gpu_memory_buffer_support_x11.h"  // nogncheck
#include "ui/gl/gl_implementation.h"                     // nogncheck
#include "ui/gl/gl_visual_picker_glx.h"                  // nogncheck
#endif

namespace gl {

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

#if defined(OS_ANDROID)
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
#endif

bool UsePassthroughCommandDecoder(const base::CommandLine* command_line) {
  std::string switch_value;
  if (command_line->HasSwitch(switches::kUseCmdDecoder)) {
    switch_value = command_line->GetSwitchValueASCII(switches::kUseCmdDecoder);
  }

  if (switch_value == kCmdDecoderPassthroughName) {
    return true;
  } else if (switch_value == kCmdDecoderValidatingName) {
    return false;
  } else {
    // Unrecognized or missing switch, use the default.
    return base::FeatureList::IsEnabled(
        features::kDefaultPassthroughCommandDecoder);
  }
}

bool PassthroughCommandDecoderSupported() {
#if defined(USE_EGL)
  // Using the passthrough command buffer requires that specific ANGLE
  // extensions are exposed
  return gl::GLSurfaceEGL::IsCreateContextBindGeneratesResourceSupported() &&
         gl::GLSurfaceEGL::IsCreateContextWebGLCompatabilitySupported() &&
         gl::GLSurfaceEGL::IsRobustResourceInitSupported() &&
         gl::GLSurfaceEGL::IsDisplayTextureShareGroupSupported() &&
         gl::GLSurfaceEGL::IsCreateContextClientArraysSupported();
#else
  // The passthrough command buffer is only supported on top of ANGLE/EGL
  return false;
#endif  // defined(USE_EGL)
}

#if defined(OS_WIN)
// This function is thread safe.
bool AreOverlaysSupportedWin() {
  return gl::DirectCompositionSurfaceWin::AreOverlaysSupported();
}

unsigned int FrameRateToPresentDuration(float frame_rate) {
  if (frame_rate == 0)
    return 0u;
  // Present duration unit is 100 ns.
  return static_cast<unsigned int>(1.0E7 / frame_rate);
}

UINT GetOverlaySupportFlags(DXGI_FORMAT format) {
  return gl::DirectCompositionSurfaceWin::GetOverlaySupportFlags(format);
}

unsigned int DirectCompositionRootSurfaceBufferCount() {
  return base::FeatureList::IsEnabled(features::kDCompTripleBufferRootSwapChain)
             ? 3u
             : 2u;
}

bool ShouldForceDirectCompositionRootSurfaceFullDamage() {
  static bool should_force = []() {
    const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    if (cmd_line->HasSwitch(
            switches::kDirectCompositionForceFullDamageForTesting)) {
      return true;
    }
    UINT brga_flags = DirectCompositionSurfaceWin::GetOverlaySupportFlags(
        DXGI_FORMAT_B8G8R8A8_UNORM);
    constexpr UINT kSupportBits =
        DXGI_OVERLAY_SUPPORT_FLAG_DIRECT | DXGI_OVERLAY_SUPPORT_FLAG_SCALING;
    if ((brga_flags & kSupportBits) == 0)
      return false;
    if (!base::FeatureList::IsEnabled(
            features::kDirectCompositionForceFullDamage)) {
      return false;
    }
    return true;
  }();
  return should_force;
}
#endif  // OS_WIN

#if defined(USE_X11) || defined(USE_OZONE_PLATFORM_X11)
void CollectX11GpuExtraInfo(bool enable_native_gpu_memory_buffers,
                            gfx::GpuExtraInfo& info) {
  // TODO(https://crbug.com/1031269): Enable by default.
  if (enable_native_gpu_memory_buffers) {
    info.gpu_memory_buffer_support_x11 =
        ui::GpuMemoryBufferSupportX11::GetInstance()->supported_configs();
  }

  if (GetGLImplementation() == kGLImplementationDesktopGL) {
    // Create the GLVisualPickerGLX singleton now while the GbmSupportX11
    // singleton is busy being created on another thread.
    GLVisualPickerGLX* visual_picker = GLVisualPickerGLX::GetInstance();

    info.system_visual = visual_picker->system_visual();
    info.rgba_visual = visual_picker->rgba_visual();

    // With GLX, only BGR(A) buffer formats are supported.  EGL does not have
    // this restriction.
    info.gpu_memory_buffer_support_x11.erase(
        std::remove_if(info.gpu_memory_buffer_support_x11.begin(),
                       info.gpu_memory_buffer_support_x11.end(),
                       [&](gfx::BufferUsageAndFormat usage_and_format) {
                         return visual_picker->GetFbConfigForFormat(
                                    usage_and_format.format) ==
                                x11::Glx::FbConfig{};
                       }),
        info.gpu_memory_buffer_support_x11.end());
  } else if (GetGLImplementation() == kGLImplementationEGLANGLE) {
    // ANGLE does not yet support EGL_EXT_image_dma_buf_import[_modifiers].
    info.gpu_memory_buffer_support_x11.clear();
  }
}
#endif  // defined(USE_X11) || BUILDFLAG(OZONE_PLATFORM_X11)

}  // namespace gl

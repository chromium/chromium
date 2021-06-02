// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains some useful utilities for the ui/gl classes.

#ifndef UI_GL_GL_UTILS_H_
#define UI_GL_GL_UTILS_H_

#include "base/command_line.h"
#include "build/build_config.h"
#include "ui/gl/gl_export.h"

#if defined(USE_OZONE)
#include "ui/ozone/buildflags.h"
#if BUILDFLAG(OZONE_PLATFORM_X11)
#define USE_OZONE_PLATFORM_X11
#endif
#endif

#if defined(OS_WIN)
#include <dxgi1_6.h>
#endif

#if defined(OS_ANDROID)
#include "base/files/scoped_file.h"
#endif

#if defined(USE_X11) || defined(USE_OZONE_PLATFORM_X11)
#include "ui/gfx/gpu_extra_info.h"  // nogncheck
#endif

namespace gl {
GL_EXPORT void Crash();
GL_EXPORT void Hang();

#if defined(OS_ANDROID)
GL_EXPORT base::ScopedFD MergeFDs(base::ScopedFD a, base::ScopedFD b);
#endif

GL_EXPORT bool UsePassthroughCommandDecoder(
    const base::CommandLine* command_line);

GL_EXPORT bool PassthroughCommandDecoderSupported();

#if defined(OS_WIN)
GL_EXPORT bool AreOverlaysSupportedWin();

// Calculates present during in 100 ns from number of frames per second.
GL_EXPORT unsigned int FrameRateToPresentDuration(float frame_rate);

GL_EXPORT UINT GetOverlaySupportFlags(DXGI_FORMAT format);

// BufferCount for the root surface swap chain.
GL_EXPORT unsigned int DirectCompositionRootSurfaceBufferCount();

// Whether to use full damage when direct compostion root surface presents.
// This function is thread safe.
GL_EXPORT bool ShouldForceDirectCompositionRootSurfaceFullDamage();
#endif

#if defined(USE_X11) || defined(USE_OZONE_PLATFORM_X11)
// Collects the GPU extra info on X11 platforms.
// |enable_native_gpu_memory_buffers| should be taken from GpuPreferences.
// TODO(crbug/1096425) remove this once Ozone is default on Linux.
GL_EXPORT void CollectX11GpuExtraInfo(bool enable_native_gpu_memory_buffer,
                                      gfx::GpuExtraInfo& info);
#endif

}  // namespace gl

#endif  // UI_GL_GL_UTILS_H_

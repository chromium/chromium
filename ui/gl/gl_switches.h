// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SWITCHES_H_
#define UI_GL_GL_SWITCHES_H_

// Defines all the command-line switches used by ui/gl.

#include <stddef.h>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "ui/gl/gl_export.h"

namespace gl {

// The GL implementation names that can be passed to --use-gl.
GL_EXPORT extern const char kGLImplementationEGLName[];
GL_EXPORT extern const char kGLImplementationANGLEName[];
GL_EXPORT extern const char kGLImplementationMockName[];
GL_EXPORT extern const char kGLImplementationStubName[];
GL_EXPORT extern const char kGLImplementationDisabledName[];

GL_EXPORT extern const char kANGLEImplementationDefaultName[];
GL_EXPORT extern const char kANGLEImplementationD3D9Name[];
GL_EXPORT extern const char kANGLEImplementationD3D11Name[];
GL_EXPORT extern const char kANGLEImplementationD3D11on12Name[];
GL_EXPORT extern const char kANGLEImplementationOpenGLName[];
GL_EXPORT extern const char kANGLEImplementationOpenGLEGLName[];
GL_EXPORT extern const char kANGLEImplementationOpenGLESName[];
GL_EXPORT extern const char kANGLEImplementationOpenGLESEGLName[];
GL_EXPORT extern const char kANGLEImplementationNullName[];
GL_EXPORT extern const char kANGLEImplementationVulkanName[];
GL_EXPORT extern const char kANGLEImplementationSwiftShaderName[];
GL_EXPORT extern const char kANGLEImplementationSwiftShaderForWebGLName[];
GL_EXPORT extern const char kANGLEImplementationMetalName[];
GL_EXPORT extern const char kANGLEImplementationNoneName[];

GL_EXPORT extern const char kANGLEImplementationD3D11NULLName[];
GL_EXPORT extern const char kANGLEImplementationOpenGLNULLName[];
GL_EXPORT extern const char kANGLEImplementationOpenGLESNULLName[];
GL_EXPORT extern const char kANGLEImplementationVulkanNULLName[];
GL_EXPORT extern const char kANGLEImplementationMetalNULLName[];

GL_EXPORT extern const char kCmdDecoderValidatingName[];
GL_EXPORT extern const char kCmdDecoderPassthroughName[];

GL_EXPORT extern const char kSwapChainFormatNV12[];
GL_EXPORT extern const char kSwapChainFormatYUY2[];
GL_EXPORT extern const char kSwapChainFormatBGRA[];

}  // namespace gl

namespace switches {

GL_EXPORT extern const char kDisableGpuDriverBugWorkarounds[];
GL_EXPORT extern const char kDisableGpuVsync[];
GL_EXPORT extern const char kEnableGPUServiceLogging[];
GL_EXPORT extern const char kEnableGPUServiceTracing[];
GL_EXPORT extern const char kGpuNoContextLost[];

GL_EXPORT extern const char kUseANGLE[];
GL_EXPORT extern const char kUseCmdDecoder[];
GL_EXPORT extern const char kEnableANGLEFeatures[];
GL_EXPORT extern const char kDisableANGLEFeatures[];
GL_EXPORT extern const char kUseGL[];
GL_EXPORT extern const char kTestGLLib[];
GL_EXPORT extern const char kUseGpuInTests[];
GL_EXPORT extern const char kEnableSgiVideoSync[];
GL_EXPORT extern const char kDisableGLExtensions[];
GL_EXPORT extern const char kEnableSwapBuffersWithBounds[];
GL_EXPORT extern const char kEnableDirectCompositionVideoOverlays[];
GL_EXPORT extern const char kUseAdapterLuid[];
GL_EXPORT extern const char kEnableUnsafeSwiftShader[];

GL_EXPORT extern const char kDirectCompositionVideoSwapChainFormat[];

// These flags are used by the test harness code, not passed in by users.
GL_EXPORT extern const char kDisableGLDrawingForTests[];
GL_EXPORT extern const char kOverrideUseSoftwareGLForTests[];

GL_EXPORT extern const char* const kGLSwitchesCopiedFromGpuProcessHost[];
GL_EXPORT extern const size_t kGLSwitchesCopiedFromGpuProcessHostNumSwitches;

#if BUILDFLAG(IS_ANDROID)
GL_EXPORT extern const char kDisableAndroidNativeFenceSyncForTesting[];
#endif

}  // namespace switches

namespace features {

GL_EXPORT BASE_DECLARE_FEATURE(kDCompDebugVisualization);
GL_EXPORT BASE_DECLARE_FEATURE(kDCompTripleBufferRootSwapChain);
GL_EXPORT BASE_DECLARE_FEATURE(kDCompTripleBufferVideoSwapChain);
GL_EXPORT BASE_DECLARE_FEATURE(kDirectCompositionSoftwareOverlays);
GL_EXPORT BASE_DECLARE_FEATURE(kDirectCompositionLetterboxVideoOptimization);
GL_EXPORT BASE_DECLARE_FEATURE(kDirectCompositionUnlimitedOverlays);
GL_EXPORT BASE_DECLARE_FEATURE(kEGLDualGPURendering);
GL_EXPORT BASE_DECLARE_FEATURE(kIntelVpSuperResolution);
GL_EXPORT BASE_DECLARE_FEATURE(kNvidiaVpSuperResolution);
GL_EXPORT BASE_DECLARE_FEATURE(kNvidiaVpTrueHDR);
GL_EXPORT BASE_DECLARE_FEATURE(kDefaultANGLEOpenGL);
GL_EXPORT BASE_DECLARE_FEATURE(kDefaultANGLEMetal);
GL_EXPORT BASE_DECLARE_FEATURE(kDefaultANGLEVulkan);
GL_EXPORT BASE_DECLARE_FEATURE(kTrackCurrentShaders);
GL_EXPORT BASE_DECLARE_FEATURE(kVulkanFromANGLE);
GL_EXPORT BASE_DECLARE_FEATURE(kDXGIWaitableSwapChain);
GL_EXPORT BASE_DECLARE_FEATURE(kGpuVsync);
GL_EXPORT extern const base::FeatureParam<int>
    kDXGIWaitableSwapChainMaxQueuedFrames;
GL_EXPORT BASE_DECLARE_FEATURE(kDXGISwapChainPresentInterval0);

GL_EXPORT bool IsDefaultANGLEVulkan();

GL_EXPORT bool SupportsEGLDualGPURendering();

}  // namespace features

#endif  // UI_GL_GL_SWITCHES_H_

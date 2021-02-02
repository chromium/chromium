// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_switches.h"

#include "base/stl_util.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace gl {

const char kGLImplementationDesktopName[] = "desktop";
const char kGLImplementationAppleName[] = "apple";
const char kGLImplementationEGLName[] = "egl";
const char kGLImplementationANGLEName[] = "angle";
const char kGLImplementationSwiftShaderName[] = "swiftshader";
const char kGLImplementationSwiftShaderForWebGLName[] = "swiftshader-webgl";
const char kGLImplementationMockName[] = "mock";
const char kGLImplementationStubName[] = "stub";
const char kGLImplementationDisabledName[] = "disabled";

const char kANGLEImplementationDefaultName[]  = "default";
const char kANGLEImplementationD3D9Name[]     = "d3d9";
const char kANGLEImplementationD3D11Name[]    = "d3d11";
const char kANGLEImplementationD3D11on12Name[] = "d3d11on12";
const char kANGLEImplementationOpenGLName[]   = "gl";
const char kANGLEImplementationOpenGLEGLName[] = "gl-egl";
const char kANGLEImplementationOpenGLESName[] = "gles";
const char kANGLEImplementationOpenGLESEGLName[] = "gles-egl";
const char kANGLEImplementationNullName[] = "null";
const char kANGLEImplementationVulkanName[] = "vulkan";
const char kANGLEImplementationSwiftShaderName[] = "swiftshader";
const char kANGLEImplementationMetalName[] = "metal";

// Special switches for "NULL"/stub driver implementations.
const char kANGLEImplementationD3D11NULLName[] = "d3d11-null";
const char kANGLEImplementationOpenGLNULLName[] = "gl-null";
const char kANGLEImplementationOpenGLESNULLName[] = "gles-null";
const char kANGLEImplementationVulkanNULLName[] = "vulkan-null";
const char kANGLEImplementationMetalNULLName[] = "metal-null";

// The command decoder names that can be passed to --use-cmd-decoder.
const char kCmdDecoderValidatingName[] = "validating";
const char kCmdDecoderPassthroughName[] = "passthrough";

// Swap chain formats for direct composition SDR video overlays.
const char kSwapChainFormatNV12[] = "nv12";
const char kSwapChainFormatYUY2[] = "yuy2";
const char kSwapChainFormatBGRA[] = "bgra";

}  // namespace gl

namespace switches {

// Disables use of D3D11.
const char kDisableD3D11[]                  = "disable-d3d11";

// Disables use of ES3 backend (use ES2 backend instead).
const char kDisableES3GLContext[]           = "disable-es3-gl-context";

// Disables use of ES3 backend at a lower level, for testing purposes.
// This isn't guaranteed to work everywhere, so it's test-only.
const char kDisableES3GLContextForTesting[] =
    "disable-es3-gl-context-for-testing";

// Disable workarounds for various GPU driver bugs.
const char kDisableGpuDriverBugWorkarounds[] =
    "disable-gpu-driver-bug-workarounds";

// Stop the GPU from synchronizing presentation with vblank.
const char kDisableGpuVsync[]               = "disable-gpu-vsync";

// Turns on GPU logging (debug build only).
const char kEnableGPUServiceLogging[]       = "enable-gpu-service-logging";

// Turns on calling TRACE for every GL call.
const char kEnableGPUServiceTracing[]       = "enable-gpu-service-tracing";

// Select which ANGLE backend to use. Options are:
//  default: Attempts several ANGLE renderers until one successfully
//           initializes, varying ES support by platform.
//  d3d9: Legacy D3D9 renderer, ES2 only.
//  d3d11: D3D11 renderer, ES2 and ES3.
//  warp: D3D11 renderer using software rasterization, ES2 and ES3.
//  gl: Desktop GL renderer, ES2 and ES3.
//  gles: GLES renderer, ES2 and ES3.
const char kUseANGLE[]                      = "use-angle";

// Use the Pass-through command decoder, skipping all validation and state
// tracking. Switch lives in ui/gl because it affects the GL binding
// initialization on platforms that would otherwise not default to using
// EGL bindings.
const char kUseCmdDecoder[] = "use-cmd-decoder";

// ANGLE features are defined per-backend in third_party/angle/include/platform
// Enables specified comma separated ANGLE features if found.
const char kEnableANGLEFeatures[] = "enable-angle-features";
// Disables specified comma separated ANGLE features if found.
const char kDisableANGLEFeatures[] = "disable-angle-features";

// Select which implementation of GL the GPU process should use. Options are:
//  desktop: whatever desktop OpenGL the user has installed (Linux and Mac
//           default).
//  egl: whatever EGL / GLES2 the user has installed (Windows default - actually
//       ANGLE).
//  swiftshader: The SwiftShader software renderer.
const char kUseGL[]                         = "use-gl";

// Inform Chrome that a GPU context will not be lost in power saving mode,
// screen saving mode, etc.  Note that this flag does not ensure that a GPU
// context will never be lost in any situations, say, a GPU reset.
const char kGpuNoContextLost[]              = "gpu-no-context-lost";

// Flag used for Linux tests: for desktop GL bindings, try to load this GL
// library first, but fall back to regular library if loading fails.
const char kTestGLLib[]                     = "test-gl-lib";

// Use hardware gpu, if available, for tests.
const char kUseGpuInTests[] = "use-gpu-in-tests";

// Enable use of the SGI_video_sync extension, which can have
// driver/sandbox/window manager compatibility issues.
const char kEnableSgiVideoSync[] = "enable-sgi-video-sync";

// Disables GL drawing operations which produce pixel output. With this
// the GL output will not be correct but tests will run faster.
const char kDisableGLDrawingForTests[] = "disable-gl-drawing-for-tests";

// Forces the use of software GL instead of hardware gpu.
const char kOverrideUseSoftwareGLForTests[] =
    "override-use-software-gl-for-tests";

// Disables specified comma separated GL Extensions if found.
const char kDisableGLExtensions[] = "disable-gl-extensions";

// Enables SwapBuffersWithBounds if it is supported.
const char kEnableSwapBuffersWithBounds[] = "enable-swap-buffers-with-bounds";

// Disables DirectComposition surface.
const char kDisableDirectComposition[] = "disable-direct-composition";

// Enables using DirectComposition video overlays, even if hardware overlays
// aren't supported.
const char kEnableDirectCompositionVideoOverlays[] =
    "enable-direct-composition-video-overlays";

// Disables using DirectComposition video overlays, even if hardware overlays
// are supported.
const char kDisableDirectCompositionVideoOverlays[] =
    "disable-direct-composition-video-overlays";

// Initialize the GPU process using the adapter with the specified LUID. This is
// only used on Windows, as LUID is a Windows specific structure.
const char kUseAdapterLuid[] = "use-adapter-luid";

// Enable kDirectCompositionForceFullDamage feature regardless of overlay
// support.
const char kDirectCompositionForceFullDamageForTesting[] =
    "direct-composition-force-full-damage-for-testing";

// Used for overriding the swap chain format for direct composition SDR video
// overlays.
const char kDirectCompositionVideoSwapChainFormat[] =
    "direct-composition-video-swap-chain-format";

// This is the list of switches passed from this file that are passed from the
// GpuProcessHost to the GPU Process. Add your switch to this list if you need
// to read it in the GPU process, else don't add it.
const char* const kGLSwitchesCopiedFromGpuProcessHost[] = {
    kDisableGpuDriverBugWorkarounds,
    kDisableGpuVsync,
    kDisableD3D11,
    kDisableES3GLContext,
    kDisableES3GLContextForTesting,
    kEnableGPUServiceLogging,
    kEnableGPUServiceTracing,
    kEnableSgiVideoSync,
    kGpuNoContextLost,
    kDisableGLDrawingForTests,
    kOverrideUseSoftwareGLForTests,
    kUseANGLE,
    kEnableSwapBuffersWithBounds,
    kDisableDirectComposition,
    kEnableDirectCompositionVideoOverlays,
    kDisableDirectCompositionVideoOverlays,
    kDirectCompositionForceFullDamageForTesting,
    kDirectCompositionVideoSwapChainFormat,
};
const int kGLSwitchesCopiedFromGpuProcessHostNumSwitches =
    base::size(kGLSwitchesCopiedFromGpuProcessHost);

}  // namespace switches

namespace features {

// Use BufferCount of 3 for the direct composition root swap chain.
const base::Feature kDCompTripleBufferRootSwapChain{
    "DCompTripleBufferRootSwapChain", base::FEATURE_DISABLED_BY_DEFAULT};

// Use BufferCount of 3 for direct composition video swap chains.
const base::Feature kDCompTripleBufferVideoSwapChain{
    "DCompTripleBufferVideoSwapChain", base::FEATURE_DISABLED_BY_DEFAULT};

// Forces Chrome's main backbuffer to full damage if the actual damage
// is large enough and allows DWM to consider the main backbuffer as an
// an overlay candidate.
const base::Feature kDirectCompositionForceFullDamage{
    "DirectCompositionForceFullDamage", base::FEATURE_ENABLED_BY_DEFAULT};

// Use presentation feedback event queries (must be enabled) to limit latency.
const base::Feature kDirectCompositionLowLatencyPresentation{
    "DirectCompositionLowLatencyPresentation",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Allow overlay swapchain to present on all GPUs even if they only support
// software overlays.
const base::Feature kDirectCompositionSoftwareOverlays{
    "DirectCompositionSoftwareOverlays", base::FEATURE_DISABLED_BY_DEFAULT};

// Default to using ANGLE's OpenGL backend
const base::Feature kDefaultANGLEOpenGL{"DefaultANGLEOpenGL",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Default to using ANGLE's Metal backend.
const base::Feature kDefaultANGLEMetal{"DefaultANGLEMetal",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Default to using ANGLE's Vulkan backend.
const base::Feature kDefaultANGLEVulkan{"DefaultANGLEVulkan",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Track current program's shaders at glUseProgram() call for crash report
// purpose. Only effective on Windows because the attached shaders may only
// be reliably retrieved with ANGLE backend.
const base::Feature kTrackCurrentShaders{"TrackCurrentShaders",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

bool IsDefaultANGLEVulkan() {
#if defined(OS_ANDROID)
  // No support for devices before Q -- exit before checking feature flags
  // so that devices are not counted in finch trials.
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_Q)
    return false;
#endif  // defined(OS_ANDROID)
  return base::FeatureList::IsEnabled(kDefaultANGLEVulkan);
}

}  // namespace features

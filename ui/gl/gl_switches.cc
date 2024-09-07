// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_switches.h"

#include "build/build_config.h"
#include "ui/gl/gl_display_manager.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <vulkan/vulkan_core.h>
#include "third_party/angle/src/gpu_info_util/SystemInfo.h"  // nogncheck
#endif

namespace gl {

const char kGLImplementationEGLName[] = "egl";
const char kGLImplementationANGLEName[] = "angle";
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
const char kANGLEImplementationSwiftShaderForWebGLName[] = "swiftshader-webgl";
const char kANGLEImplementationMetalName[] = "metal";
const char kANGLEImplementationNoneName[] = "";

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

// Forces the use of software GL instead of hardware gpu for tests.
const char kOverrideUseSoftwareGLForTests[] =
    "override-use-software-gl-for-tests";

// Disables specified comma separated GL Extensions if found.
const char kDisableGLExtensions[] = "disable-gl-extensions";

// Enables SwapBuffersWithBounds if it is supported.
const char kEnableSwapBuffersWithBounds[] = "enable-swap-buffers-with-bounds";

// Enables using DirectComposition video overlays, even if hardware overlays
// aren't supported.
const char kEnableDirectCompositionVideoOverlays[] =
    "enable-direct-composition-video-overlays";

// Initialize the GPU process using the adapter with the specified LUID. This is
// only used on Windows, as LUID is a Windows specific structure.
const char kUseAdapterLuid[] = "use-adapter-luid";

// Allow usage of SwiftShader for WebGL
const char kEnableUnsafeSwiftShader[] = "enable-unsafe-swiftshader";

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
    kEnableGPUServiceLogging,
    kEnableGPUServiceTracing,
    kEnableSgiVideoSync,
    kGpuNoContextLost,
    kDisableGLDrawingForTests,
    kOverrideUseSoftwareGLForTests,
    kUseANGLE,
    kEnableSwapBuffersWithBounds,
    kEnableDirectCompositionVideoOverlays,
    kDirectCompositionVideoSwapChainFormat,
    kEnableUnsafeSwiftShader,
};
const size_t kGLSwitchesCopiedFromGpuProcessHostNumSwitches =
    std::size(kGLSwitchesCopiedFromGpuProcessHost);

#if BUILDFLAG(IS_ANDROID)
// On some Android emulators with software GL, ANGLE
// is exposing the native fence sync extension but it doesn't
// actually work. This switch is used to disable the Android native fence sync
// during test to avoid crashes.
//
// TODO(https://crbug.com/337886037): Remove this flag once the upstream ANGLE
// is fixed.
const char kDisableAndroidNativeFenceSyncForTesting[] =
    "disable-android-native-fence-sync-for-testing";
#endif
}  // namespace switches

namespace features {

// Enable DComp debug visualizations. This can be useful to determine how much
// work DWM is doing when we update our tree.
//
// Please be aware that some of these visualizations result in quickly flashing
// colors.
BASE_FEATURE(kDCompDebugVisualization,
             "DCompDebugVisualization",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use BufferCount of 3 for the direct composition root swap chain.
BASE_FEATURE(kDCompTripleBufferRootSwapChain,
             "DCompTripleBufferRootSwapChain",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use BufferCount of 3 for direct composition video swap chains.
BASE_FEATURE(kDCompTripleBufferVideoSwapChain,
             "DCompTripleBufferVideoSwapChain",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allow overlay swapchain to present on all GPUs even if they only support
// software overlays. GPU deny lists limit it to NVIDIA only at the moment.
BASE_FEATURE(kDirectCompositionSoftwareOverlays,
             "DirectCompositionSoftwareOverlays",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Adjust the letterbox video size and position to the center of the screen so
// that DWM power optimization can be turned on.
BASE_FEATURE(kDirectCompositionLetterboxVideoOptimization,
             "DirectCompositionLetterboxVideoOptimization",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Do not consider hardware YUV overlay count when promoting quads to DComp
// visuals. If there are more videos than hardware overlay planes, there may be
// a performance hit compared to drawing all the videos into a single swap
// chain. This feature is intended for testing and debugging.
BASE_FEATURE(kDirectCompositionUnlimitedOverlays,
             "DirectCompositionUnlimitedOverlays",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allow dual GPU rendering through EGL where supported, i.e., allow a WebGL
// or WebGPU context to be on the high performance GPU if preferred and Chrome
// internal rendering to be on the low power GPU.
BASE_FEATURE(kEGLDualGPURendering,
             "EGLDualGPURendering",
#if BUILDFLAG(IS_MAC)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Allow overlay swapchain to use Intel video processor for super resolution.
BASE_FEATURE(kIntelVpSuperResolution,
             "IntelVpSuperResolution",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allow overlay swapchain to use NVIDIA video processor for super resolution.
BASE_FEATURE(kNvidiaVpSuperResolution,
             "NvidiaVpSuperResolution",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allow overlay swapchain to use NVIDIA video processor for trueHDR.
BASE_FEATURE(kNvidiaVpTrueHDR,
             "NvidiaVpTrueHDR",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Default to using ANGLE's OpenGL backend
BASE_FEATURE(kDefaultANGLEOpenGL,
             "DefaultANGLEOpenGL",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Default to using ANGLE's Metal backend.
BASE_FEATURE(kDefaultANGLEMetal,
             "DefaultANGLEMetal",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Default to using ANGLE's Vulkan backend.
BASE_FEATURE(kDefaultANGLEVulkan,
             "DefaultANGLEVulkan",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Track current program's shaders at glUseProgram() call for crash report
// purpose. Only effective on Windows because the attached shaders may only
// be reliably retrieved with ANGLE backend.
BASE_FEATURE(kTrackCurrentShaders,
             "TrackCurrentShaders",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable sharing Vulkan device queue with ANGLE's Vulkan backend.
BASE_FEATURE(kVulkanFromANGLE,
             "VulkanFromANGLE",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDefaultANGLEVulkan() {
#if defined(MEMORY_SANITIZER)
  return false;
#else  // !defined(MEMORY_SANITIZER)
#if BUILDFLAG(IS_ANDROID)
  // No support for devices before Q -- exit before checking feature flags
  // so that devices are not counted in finch trials.
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_Q)
    return false;
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  angle::SystemInfo system_info;
  if (!angle::GetSystemInfoVulkan(&system_info))
    return false;

  if (static_cast<size_t>(system_info.activeGPUIndex) >=
      system_info.gpus.size()) {
    return false;
  }

  const auto& active_gpu = system_info.gpus[system_info.activeGPUIndex];

#if BUILDFLAG(IS_LINUX)
  // Vulkan 1.1 is required.
  if (active_gpu.driverApiVersion < VK_VERSION_1_1)
    return false;

  // AMDVLK driver is buggy, so disable Vulkan with AMDVLK for now.
  // crbug.com/1340081
  if (active_gpu.driverId == VK_DRIVER_ID_AMD_OPEN_SOURCE)
    return false;
#endif  // BUILDFLAG(IS_LINUX)

  // The performance of MESA llvmpipe is really bad.
  if (active_gpu.driverId == VK_DRIVER_ID_MESA_LLVMPIPE) {
    return false;
  }

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return base::FeatureList::IsEnabled(kDefaultANGLEVulkan);
#endif  // !defined(MEMORY_SANITIZER)
}

// Use waitable swap chain on Windows to reduce display latency.
BASE_FEATURE(kDXGIWaitableSwapChain,
             "DXGIWaitableSwapChain",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If using waitable swap chain, specify the maximum number of queued frames.
const base::FeatureParam<int> kDXGIWaitableSwapChainMaxQueuedFrames{
    &kDXGIWaitableSwapChain, "DXGIWaitableSwapChainMaxQueuedFrames", 2};

// Force a present interval of 0. This asks Windows to cancel the remaining time
// on the previously presented frame instead of synchronizing with vblank(s).
// Frames may be discarded if they are presented more frequently than one per
// vblank.
BASE_FEATURE(kDXGISwapChainPresentInterval0,
             "DXGISwapChainPresentInterval0",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool SupportsEGLDualGPURendering() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return base::FeatureList::IsEnabled(kEGLDualGPURendering);
#else
  return false;
#endif  // IS_WIN || IS_MAC
}

}  // namespace features

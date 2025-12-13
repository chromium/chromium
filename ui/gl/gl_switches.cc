// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_switches.h"

#include <array>

#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_display_manager.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_info.h"
#endif

#if BUILDFLAG(ENABLE_VULKAN) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID))
#include <vulkan/vulkan_core.h>
#include "third_party/angle/src/gpu_info_util/SystemInfo.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_VULKAN) && (BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID))

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
const char kANGLEImplementationD3D11WarpName[] = "d3d11-warp";
const char kANGLEImplementationD3D11WarpForWebGLName[] = "d3d11-warp-webgl";
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
const char kSwapChainFormatP010[] = "p010";

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

// Disable DirectComposition.
const char kDisableDirectComposition[] = "disable-direct-composition";

// Enable DirectComposition video overlays even if hardware doesn't support it.
const char kEnableDirectCompositionVideoOverlays[] =
    "enable-direct-composition-video-overlays";

// Initialize the GPU process using the adapter with the specified LUID. This is
// only used on Windows, as LUID is a Windows specific structure.
const char kUseAdapterLuid[] = "use-adapter-luid";

// Allow usage of SwiftShader for WebGL
const char kEnableUnsafeSwiftShader[] = "enable-unsafe-swiftshader";

// Explicitly disable D3D11 WARP fallback. Some test suites prefer falling back
// to swiftshader.
const char kDisableD3D11Warp[] = "disable-d3d11-warp";

// Used for overriding the swap chain format for direct composition SDR video
// overlays.
const char kDirectCompositionVideoSwapChainFormat[] =
    "direct-composition-video-swap-chain-format";

// Tint `SwapChainPresenter` with the following colors:
//
// - Decode swap chain: blue
// - VP blit: magenta
// - VP blit w/ staging texture: orange
// - MF proxy surface: green
//
// This is similar to `HKLM\Software\Microsoft\Windows\DWM` `OverlayTestMode=1`
// in DWM, but to help understand `SwapChainPresenter` state.
const char kTintDcLayer[] = "tint-dc-layer";

// This is the list of switches passed from this file that are passed from the
// GpuProcessHost to the GPU Process. Add your switch to this list if you need
// to read it in the GPU process, else don't add it.
const auto kGLSwitchesCopiedFromGpuProcessHostArray = std::to_array({
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
    kDisableDirectComposition,
    kEnableDirectCompositionVideoOverlays,
    kDirectCompositionVideoSwapChainFormat,
    kTintDcLayer,
    kEnableUnsafeSwiftShader,
    kDisableD3D11Warp,
});
// An external span to the array above, so that it can be exposed from the
// header file without specifying the size of the array manually.
const base::span<const char* const> kGLSwitchesCopiedFromGpuProcessHost =
    kGLSwitchesCopiedFromGpuProcessHostArray;

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
BASE_FEATURE(kDCompDebugVisualization, base::FEATURE_DISABLED_BY_DEFAULT);

// Use BufferCount of 3 for direct composition video swap chains.
BASE_FEATURE(kDCompTripleBufferVideoSwapChain,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allow overlay swapchain to present on all GPUs even if they only support
// software overlays. GPU deny lists limit it to NVIDIA only at the moment.
BASE_FEATURE(kDirectCompositionSoftwareOverlays,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Detect and mark a single full screen video during overlay processing.
BASE_FEATURE(kEarlyFullScreenVideoOptimization,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Adjust the letterbox video size and position to the center of the screen so
// that DWM power optimization can be turned on.
BASE_FEATURE(kDirectCompositionLetterboxVideoOptimization,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Remove the topmost desktop plane for Media Foundation full screen
// letterboxing. This is a kill switch for the desktop plane removal
// optimization for Media Foundation Renderer, which should be enabled by
// default when crbug.com/406175378 is resolved.
BASE_FEATURE(kDesktopPlaneRemovalForMFFullScreenLetterbox,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Do not consider hardware YUV overlay count when promoting quads to DComp
// visuals. If there are more videos than hardware overlay planes, there may be
// a performance hit compared to drawing all the videos into a single swap
// chain. This feature is intended for testing and debugging.
BASE_FEATURE(kDirectCompositionUnlimitedOverlays,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allow dual GPU rendering through EGL where supported, i.e., allow a WebGL
// or WebGPU context to be on the high performance GPU if preferred and Chrome
// internal rendering to be on the low power GPU.
BASE_FEATURE(kEGLDualGPURendering,
#if BUILDFLAG(IS_MAC)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Allow overlay swapchain to use Intel video processor for super resolution.
BASE_FEATURE(kIntelVpSuperResolution, base::FEATURE_DISABLED_BY_DEFAULT);

// Default to using ANGLE's Metal backend.
BASE_FEATURE(kDefaultANGLEMetal, base::FEATURE_ENABLED_BY_DEFAULT);

// Default to using ANGLE's Vulkan backend.
BASE_FEATURE(kDefaultANGLEVulkan,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Track current program's shaders at glUseProgram() call for crash report
// purpose. Only effective on Windows because the attached shaders may only
// be reliably retrieved with ANGLE backend.
BASE_FEATURE(kTrackCurrentShaders, base::FEATURE_DISABLED_BY_DEFAULT);

// Enable sharing Vulkan device queue with ANGLE's Vulkan backend.
BASE_FEATURE(kVulkanFromANGLE,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable skipping the Vulkan blocklist.
BASE_FEATURE(kSkipVulkanBlocklist,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDefaultANGLEVulkan() {
  // Force on if DefaultANGLEVulkan feature is enabled from command line.
  base::FeatureList* feature_list = base::FeatureList::GetInstance();
  if (feature_list && feature_list->IsFeatureOverriddenFromCommandLine(
                          features::kDefaultANGLEVulkan.name,
                          base::FeatureList::OVERRIDE_ENABLE_FEATURE)) {
    return true;
  }

#if defined(MEMORY_SANITIZER)
  return false;
#else  // !defined(MEMORY_SANITIZER)
#if BUILDFLAG(IS_ANDROID)
  // No support for devices before Q -- exit before checking feature flags
  // so that devices are not counted in finch trials.
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_Q) {
    return false;
  }

  // For the sake of finch trials, limit to newer devices (Android T+); this
  // condition can be relaxed over time.
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_T) {
    return false;
  }
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(ENABLE_VULKAN) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID))
  angle::SystemInfo system_info;
  {
    TRACE_EVENT("gpu,startup", "angle::GetSystemInfoVulkan");
    if (!angle::GetSystemInfoVulkan(&system_info)) {
      return false;
    }
  }

  if (static_cast<size_t>(system_info.activeGPUIndex) >=
      system_info.gpus.size()) {
    return false;
  }

  const auto& active_gpu = system_info.gpus[system_info.activeGPUIndex];

  // Vulkan 1.1 is ANGLE's minimum requirement, but drivers older than 1.3 are
  // rarely reliable enough.
  if (active_gpu.driverApiVersion < VK_VERSION_1_3) {
    return false;
  }

  // If |dirverId| is 0, the driver lacks VK_KHR_driver_properties.
  // Consider this driver too old to be usable.
  if (active_gpu.driverId == 0)
    return false;

#if BUILDFLAG(IS_ANDROID)
  // Samsung GPUs already use ANGLE as the GLES driver.  Always choose
  // ANGLE/Vulkan on these GPUs to avoid the inefficiencies of translating
  // over ANGLE twice.  This is not done if the feature is explicitly disabled
  // (from command line, or by webview).
  if (active_gpu.driverId == VK_DRIVER_ID_SAMSUNG_PROPRIETARY) {
    if (!(feature_list && feature_list->IsFeatureOverriddenFromCommandLine(
                              features::kDefaultANGLEVulkan.name,
                              base::FeatureList::OVERRIDE_DISABLE_FEATURE))) {
      return true;
    }
  }

  // Exclude SwiftShader-based Android emulators for now.
  if (active_gpu.driverId == VK_DRIVER_ID_GOOGLE_SWIFTSHADER) {
    return false;
  }

  // Encountered bugs with older Imagination drivers.  New drivers seem fixed,
  // but disabled for the sake of experiment for now. crbug.com/371512561
  if (active_gpu.driverId == VK_DRIVER_ID_IMAGINATION_PROPRIETARY) {
    return false;
  }

  // Exclude old ARM drivers due to crashes related to creating
  // AHB-based Video images in Vulkan.  http://crbug.com/382676807.
  if (active_gpu.driverId == VK_DRIVER_ID_ARM_PROPRIETARY &&
      active_gpu.detailedDriverVersion.major <= 32) {
    return false;
  }

  // Exclude old ARM chipsets due to rendering bugs, G52 is still found in
  // Xiaomi phones. Note that if included in the future, there still seems to be
  // a driver bug with async garbage collection, so that feature needs to be
  // disabled in ANGLE. http://crbug.com/405085132
  if (active_gpu.driverId == VK_DRIVER_ID_ARM_PROPRIETARY &&
      active_gpu.deviceName.find("G52") != std::string::npos) {
    return false;
  }

  // Exclude old Qualcomm drivers due to inefficient (and buggy) fallback
  // to CPU path in glCopyTextureCHROMIUM with multi-plane images.
  // http://crbug.com/383056998.
  if (active_gpu.driverId == VK_DRIVER_ID_QUALCOMM_PROPRIETARY &&
      active_gpu.detailedDriverVersion.minor <= 530) {
    return false;
  }

  // Exclude Qualcomm 512.615 driver on Xiaomi phones that is the cause of
  // yet-to-be explained GPU hangs.
  // http://crbug.com/382725542
  if (active_gpu.driverId == VK_DRIVER_ID_QUALCOMM_PROPRIETARY &&
      active_gpu.detailedDriverVersion.minor == 615) {
    return false;
  }

  // Exclude Qualcomm 512.676 driver on Adreno 720 that is the cause of
  // undiagnosed rendering issues.
  // http://crbug.com/440110161
  if (active_gpu.driverId == VK_DRIVER_ID_QUALCOMM_PROPRIETARY &&
      active_gpu.deviceName.find("Adreno") != std::string::npos &&
      active_gpu.deviceName.find("720") != std::string::npos &&
      active_gpu.detailedDriverVersion.minor == 676) {
    return false;
  }
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX)
  // AMDVLK driver is buggy, so disable Vulkan with AMDVLK for now.
  // crbug.com/1340081
  if (active_gpu.driverId == VK_DRIVER_ID_AMD_OPEN_SOURCE)
    return false;
#endif  // BUILDFLAG(IS_LINUX)

  // The performance of MESA llvmpipe is really bad.
  if (active_gpu.driverId == VK_DRIVER_ID_MESA_LLVMPIPE) {
    return false;
  }

#endif  // BUILDFLAG(ENABLE_VULKAN) && (BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID))

  return base::FeatureList::IsEnabled(kDefaultANGLEVulkan);
#endif  // !defined(MEMORY_SANITIZER)
}

// Use waitable swap chain on Windows to reduce display latency.
BASE_FEATURE(kDXGIWaitableSwapChain, base::FEATURE_DISABLED_BY_DEFAULT);

// If using waitable swap chain, specify the maximum number of queued frames.
const base::FeatureParam<int> kDXGIWaitableSwapChainMaxQueuedFrames{
    &kDXGIWaitableSwapChain, "DXGIWaitableSwapChainMaxQueuedFrames", 2};

// Force a present interval of 0. This asks Windows to cancel the remaining time
// on the previously presented frame instead of synchronizing with vblank(s).
// Frames may be discarded if they are presented more frequently than one per
// vblank.
BASE_FEATURE(kDXGISwapChainPresentInterval0, base::FEATURE_DISABLED_BY_DEFAULT);

bool SupportsEGLDualGPURendering() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return base::FeatureList::IsEnabled(kEGLDualGPURendering);
#else
  return false;
#endif  // IS_WIN || IS_MAC
}

}  // namespace features

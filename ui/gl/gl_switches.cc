// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_switches.h"

#include <array>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_display_manager.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_info.h"
#include "base/android/device_info.h"
#include "third_party/re2/src/re2/re2.h"
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

#if BUILDFLAG(USE_STATIC_ANGLE)
// Use ANGLE shared libraries even if ANGLE is built as a static library.
const char kUseDynamicAngle[] = "use-dynamic-angle";
#endif

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

// Indicate that the this is being used by Android WebView and its draw functor
// is using vulkan.
const char kWebViewDrawFunctorUsesVulkan[] = "webview-draw-functor-uses-vulkan";

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
#if BUILDFLAG(USE_STATIC_ANGLE)
    kUseDynamicAngle,
#endif
    kDisableDirectComposition,
    kEnableDirectCompositionVideoOverlays,
    kDirectCompositionVideoSwapChainFormat,
    kTintDcLayer,
    kEnableUnsafeSwiftShader,
    kDisableD3D11Warp,
#if BUILDFLAG(IS_WIN)
    kFakeVsyncRate,
#endif
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

#if BUILDFLAG(IS_WIN)
const char kFakeVsyncRate[] = "fake-vsync-rate";

std::optional<base::TimeDelta> GetFakeVsyncIntervalFromCommandLine() {
  static const std::optional<base::TimeDelta> fake_interval = [] {
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    double fake_hz = 0;
    if (command_line->HasSwitch(kFakeVsyncRate) &&
        base::StringToDouble(command_line->GetSwitchValueASCII(kFakeVsyncRate),
                             &fake_hz) &&
        fake_hz > 0) {
      return std::optional<base::TimeDelta>(base::Seconds(1.0 / fake_hz));
    }
    return std::optional<base::TimeDelta>();
  }();
  return fake_interval;
}
#endif

}  // namespace switches

namespace features {

namespace {

#if BUILDFLAG(ENABLE_VULKAN) && BUILDFLAG(IS_ANDROID)

constexpr uint32_t kVendorARM = 0x13b5;
constexpr uint32_t kVendorQualcomm = 0x5143;
constexpr uint32_t kVendorImagination = 0x1010;
constexpr uint32_t kVendorIntel = 0x8086;
constexpr uint32_t kVendorGoogle = 0x1AE0;
constexpr uint32_t kDeviceSwiftShader = 0xC0DE;

bool IsDeviceBlocked(std::string_view field, std::string_view block_list) {
  auto disable_patterns = base::SplitString(
      block_list, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& disable_pattern : disable_patterns) {
    if (base::MatchPattern(field, disable_pattern)) {
      return true;
    }
  }
  return false;
}

int GetEMUIVersion() {
  // TODO(crbug.com/40136096): check Honor devices as well.
  if (base::android::android_info::manufacturer() != "HUAWEI") {
    return -1;
  }

  // Huawei puts EMUI version in the build version incremental.
  // Example: 11.0.0.130C00
  int version = 0;
  if (UNSAFE_TODO(
          sscanf(base::android::android_info::version_incremental().c_str(),
                 "%d.", &version)) != 1) {
    return -1;
  }

  return version;
}

bool IsBlockedByBuildInfo() {
  const char* kBlockListByHardware = "mt*";
  const char* kBlockListByBrand = "HONOR";
  const char* kBlockListByDevice = "OP4863|OP4883";
  const char* kBlockListByBoard =
      "RM67*|RM68*|k68*|mt6*|oppo67*|oppo68*|QM215|rk30sdk";

  if (IsDeviceBlocked(base::android::android_info::hardware(),
                      kBlockListByHardware)) {
    return true;
  }
  if (IsDeviceBlocked(base::android::android_info::brand(),
                      kBlockListByBrand)) {
    return true;
  }
  if (IsDeviceBlocked(base::android::android_info::device(),
                      kBlockListByDevice)) {
    return true;
  }
  if (IsDeviceBlocked(base::android::android_info::board(),
                      kBlockListByBoard)) {
    return true;
  }

  return false;
}

// Everything that passed 2022 deQP tests.
bool HasMinDeqpLevelForMediaTek() {
  // We require at least android V deqp test to pass for v2.
  constexpr int32_t kVulkanDEQPAndroidV = 0x7e80301;
  if (base::android::device_info::vulkan_deqp_level() < kVulkanDEQPAndroidV) {
    return false;
  }

  return true;
}
#endif  // BUILDFLAG(ENABLE_VULKAN) && BUILDFLAG(IS_ANDROID)

}  // namespace

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

// Enable Vulkan graphics backend for compositing and rasterization. Defaults to
// native implementation if --use-vulkan flag is not used. Otherwise
// --use-vulkan will be followed.
// Note Android WebView uses kWebViewDrawFunctorUsesVulkan instead of this.
BASE_FEATURE(kVulkan,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

VulkanPhysicalDeviceProperties::VulkanPhysicalDeviceProperties() = default;
VulkanPhysicalDeviceProperties::~VulkanPhysicalDeviceProperties() = default;

bool IsDefaultANGLEVulkan() {
  // Force on if DefaultANGLEVulkan feature is enabled from command line.
  base::FeatureList* feature_list = base::FeatureList::GetInstance();
  if (feature_list && feature_list->IsFeatureOverriddenFromCommandLine(
                          features::kDefaultANGLEVulkan.name,
                          base::FeatureList::OVERRIDE_ENABLE_FEATURE)) {
    return true;
  }

  // The platform checks below gather Vulkan system info before consulting the
  // feature so that ineligible devices never join a DefaultANGLEVulkan field
  // trial. Gathering it loads the Vulkan loader and every installed ICD and
  // creates an instance, which costs tens of milliseconds of GPU process
  // startup. When nothing overrides the feature (no field trial, command line
  // or flag), its state is the compile-time default and there is no trial
  // population to protect, so answer directly.
  if (feature_list &&
      !feature_list->IsFeatureOverridden(features::kDefaultANGLEVulkan.name) &&
      features::kDefaultANGLEVulkan.default_state ==
          base::FEATURE_DISABLED_BY_DEFAULT) {
    return false;
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
  // Exclude SwiftShader-based Android emulators for now.
  if (active_gpu.driverId == VK_DRIVER_ID_GOOGLE_SWIFTSHADER) {
    return false;
  }

  // Encountered bugs with older Imagination drivers. crbug.com/371512561
  if (active_gpu.driverId == VK_DRIVER_ID_IMAGINATION_PROPRIETARY &&
      (active_gpu.detailedDriverVersion.major < 1 ||
       active_gpu.detailedDriverVersion.minor < 662)) {
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

bool IsUsingVulkan() {
#if BUILDFLAG(IS_ANDROID)
  // Force on if Vulkan feature is enabled from command line.
  base::FeatureList* feature_list = base::FeatureList::GetInstance();
  if (feature_list &&
      feature_list->IsFeatureOverriddenFromCommandLine(
          features::kVulkan.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE)) {
    return true;
  }

  // WebView checks, which do not use (and disables) kVulkan.
  // Do this above the Android version check because there are test devices
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebViewDrawFunctorUsesVulkan)) {
    return true;
  }
#endif

#if BUILDFLAG(ENABLE_VULKAN)
  return base::FeatureList::IsEnabled(kVulkan);
#else
  return false;
#endif
}

bool CheckVulkanCompatibilities(
    const VulkanPhysicalDeviceProperties& device_properties) {
#if !BUILDFLAG(ENABLE_VULKAN)
  return false;
#elif !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_LINUX) && !defined(OZONE_PLATFORM_IS_X11)
  // Vulkan is only supported with X11 on Linux for now.
  return false;
#else
  return true;
#endif
#else   // BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kSkipVulkanBlocklist)) {
    return true;
  }

  if (IsBlockedByBuildInfo() && !HasMinDeqpLevelForMediaTek()) {
    return false;
  }

  if (device_properties.vendor_id == kVendorARM) {
    int emui_version = GetEMUIVersion();
    // TODO(crbug.com/40136096) Display problem with Huawei EMUI < 11 and Honor
    // devices with Mali GPU. The Mali driver version is < 19.0.0.
    if (device_properties.driver_version < VK_MAKE_VERSION(19, 0, 0) &&
        emui_version < 11) {
      return false;
    }

    // Remove "Mali-" prefix.
    std::string_view device_name(device_properties.device_name);
    if (!base::StartsWith(device_name, "Mali-")) {
      LOG(ERROR) << "Unexpected device_name " << device_name;
      return false;
    }
    device_name.remove_prefix(5);

    // Remove anything trailing a space (e.g. "G76 MC4" => "G76").
    device_name = device_name.substr(0, device_name.find(" "));

    // Older Mali GPUs are not performant with Vulkan -- this blocks all Utgard
    // gen, Midgard gen, and some Bifrost 1st & 2nd gen.
    std::vector<const char*> slow_gpus = {"2??", "3??", "4??", "T???",
                                          "G31", "G51", "G52"};
    for (std::string_view slow_gpu : slow_gpus) {
      if (base::MatchPattern(device_name, slow_gpu)) {
        return false;
      }
    }

    // Most Mali-G57 devices had vkCreateInstance() fail and would use GL. Add
    // them to the blocklist to keep these devices from using Vulkan with
    // Graphite. The exception is devices with driver version >= 41 had
    // vkCreateInstance() pass and were running Vulkan. See
    // https://crbug.com/384531040 for more info.
    if (device_name == "G57" &&
        device_properties.driver_version < VK_MAKE_VERSION(41, 0, 0)) {
      return false;
    }

    // Allow remaining Mali GPUs that aren't MediaTek. https://crbug.com/1183702
    if (!IsDeviceBlocked(device_properties.device_name, "*Mali-G?? M*")) {
      return true;
    }

    // MediaTek Mali-G57 has problems initializing Vulkan even with 2022 deQP
    // tests passed, devices that init successfully show performance regression.
    if (device_name == "G57") {
      return false;
    }

    // For MediaTek allow everything that passed 2022 deQP tests.
    return HasMinDeqpLevelForMediaTek();
  }

  if (device_properties.vendor_id == kVendorQualcomm) {
    // Only Adreno 630 with drivers newer than 444.0. This was launched for
    // Pixel 3 in the original Vulkan launch but otherwise Vulkan hasn't
    // performan as well as GL. https://crbug.com/1165783
    return device_properties.device_name ==
               std::string_view("Adreno (TM) 630") &&
           device_properties.driver_version > VK_MAKE_VERSION(512, 444, 0);
  }

  if (device_properties.vendor_id == kVendorImagination) {
    // Only newer PowerVR GPU series allowed. Older PowerVR GPUs showed poor
    // performance and stability problems, see https://crbug.com/1122650.
    return RE2::FullMatch(device_properties.device_name,
                          "PowerVR ([CDE]-Series)? [CDE]X.*");
  }

  // Some devices implement Vulkan using Swiftshader. We do not want those,
  // because of performance, and stability (crbug.com/1479335).
  if (device_properties.vendor_id == kVendorGoogle &&
      device_properties.device_id == kDeviceSwiftShader) {
    return false;
  }

  // Some android x86 devices (e.g older gpu on auto devices) don't report
  // format support correctly. See crbug.com/379205391
  if (device_properties.vendor_id == kVendorIntel) {
    return false;
  }

  return true;
#endif  // BUILDFLAG(IS_ANDROID)
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

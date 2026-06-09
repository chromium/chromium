// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_features.h"

#include "base/byte_size.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_info.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "ui/gfx/android/achoreographer_compat.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#endif

namespace features {
namespace {

#if BUILDFLAG(IS_ANDROID)
const base::FeatureParam<std::string>
    kPassthroughCommandDecoderBlockListByBrand{
        &kDefaultPassthroughCommandDecoder, "BlockListByBrand", ""};

const base::FeatureParam<std::string>
    kPassthroughCommandDecoderBlockListByDevice{
        &kDefaultPassthroughCommandDecoder, "BlockListByDevice", ""};

const base::FeatureParam<std::string>
    kPassthroughCommandDecoderBlockListByAndroidBuildId{
        &kDefaultPassthroughCommandDecoder, "BlockListByAndroidBuildId", ""};

const base::FeatureParam<std::string>
    kPassthroughCommandDecoderBlockListByManufacturer{
        &kDefaultPassthroughCommandDecoder, "BlockListByManufacturer", ""};

// b/455412928 flickering issue with WebView on the following XR devices
const base::FeatureParam<std::string>
    kPassthroughCommandDecoderBlockListByModel{
        &kDefaultPassthroughCommandDecoder, "BlockListByModel",
        "SM-I610|SM-I610H|Robin XR"};

const base::FeatureParam<std::string>
    kPassthroughCommandDecoderBlockListByBoard{
        &kDefaultPassthroughCommandDecoder, "BlockListByBoard", ""};

const base::FeatureParam<std::string>
    kPassthroughCommandDecoderBlockListByAndroidBuildFP{
        &kDefaultPassthroughCommandDecoder, "BlockListByAndroidBuildFP", ""};

bool IsDeviceBlocked(const std::string& field, const std::string& block_list) {
  auto disable_patterns = base::SplitString(
      block_list, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& disable_pattern : disable_patterns) {
    if (base::MatchPattern(field, disable_pattern))
      return true;
  }
  return false;
}
#endif

BASE_FEATURE(kForceANGLEFeatures, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kForcedANGLEEnabledFeaturesFP{
    &kForceANGLEFeatures, "EnabledFeatures", ""};
const base::FeatureParam<std::string> kForcedANGLEDisabledFeaturesFP{
    &kForceANGLEFeatures, "DisabledFeatures", ""};

void SplitAndAppendANGLEFeatureList(const std::string& list,
                                    std::vector<std::string>& out_features) {
  for (std::string& feature_name : base::SplitString(
           list, ", ;", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    out_features.push_back(std::move(feature_name));
  }
}

}  // namespace

#if BUILDFLAG(IS_APPLE)
BASE_FEATURE(kGpuVsync, base::FEATURE_DISABLED_BY_DEFAULT);
#else
BASE_FEATURE(kGpuVsync, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER)
// Use the passthrough command decoder by default.  This can be overridden with
// the --use-cmd-decoder=passthrough or --use-cmd-decoder=validating flags.
// Feature lives in ui/gl because it affects the GL binding initialization on
// platforms that would otherwise not default to using EGL bindings.
BASE_FEATURE(kDefaultPassthroughCommandDecoder,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER)

// Controls whether the GPU process falls back to software if GLES3 is not
// supported.
BASE_FEATURE(kFallbackToSWIfGLES3NotSupported,
#if BUILDFLAG(IS_WIN)
             // TODO(https://crbug.com/444049511): Currently disabled on
             // Windows for D3D9 users that are still on ES 2. Enable once
             // crbug.com/40874754 is fixed, deprecating D3D9 usage.
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_WIN)
// If true, VsyncThreadWin will use the compositor clock
// to determine the vsync interval.
BASE_FEATURE(kUseCompositorClockVSyncInterval,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool UseCompositorClockVSyncInterval() {
  return base::win::GetVersion() >= base::win::Version::WIN11_24H2 &&
         base::FeatureList::IsEnabled(
             features::kUseCompositorClockVSyncInterval);
}

// Enables DirectComposition textures backed by D3D12 resources.
// When this feature is enabled, the GPU pipeline may create and present DComp-
// backed surfaces using the D3D12 path and leverage Dawn’s D3D12 device. This
// allows unified resource sharing and fences between Dawn (WebGPU), Skia
// Graphite, and DComp when the system supports D3D12.
//
// Important notes:
// - Keyed-mutex resources are not compatible with the D3D12 unwrap path and
//   will continue using D3D11.
// - WebGL will continue to use the D3D11 runtime backed by D3D11 drivers with
//   ANGLE's D3D11 device.
// - Certain SharedImage functionality such as copies to staging
//   textures rely on the D3D11 DDI. These code paths will use
//   D3D11on12 when this feature is enabled.
//
// This feature requires SkiaGraphite with a dawn-d3d12 backend, BufferQueue to
// be enabled, and either DelegatedCompositing to be disabled or in full
// mode. As there is currently no support for D3D12 swapchains or DComp
// surfaces, BufferQueue is required to manage presentation. BufferQueue is not
// supported on Windows with partial delegated compositing, so in that mode this
// feature will not work.
//
// Example command line to enable this feature:
// --enable-features=SkiaGraphite,BufferQueue,DCompOnD3D12 AND
// --disable-features=DelegatedCompositing or
// --enable-features=DelegatedCompositing:mode/full AND
// --skia-graphite-backend=dawn-d3d12
BASE_FEATURE(kDCompOnD3D12, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

bool UseGpuVsync() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableGpuVsync) &&
         base::FeatureList::IsEnabled(features::kGpuVsync);
}

bool IsAndroidFrameDeadlineEnabled() {
#if BUILDFLAG(IS_ANDROID)
  static bool enabled = base::android::android_info::sdk_int() >=
                            base::android::android_info::SDK_VERSION_T &&
                        gfx::AChoreographerCompat33::Get().supported &&
                        gfx::SurfaceControl::SupportsSetFrameTimeline() &&
                        gfx::SurfaceControl::SupportsSetEnableBackPressure();
  return enabled;
#else
  return false;
#endif
}

bool UsePassthroughCommandDecoder() {
#if !BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER)
  return true;
#else

  if (!base::FeatureList::IsEnabled(kDefaultPassthroughCommandDecoder))
    return false;

#if BUILDFLAG(IS_ANDROID)
  // Check block list against build info.
  if (IsDeviceBlocked(base::android::android_info::brand(),
                      kPassthroughCommandDecoderBlockListByBrand.Get())) {
    return false;
  }
  if (IsDeviceBlocked(base::android::android_info::device(),
                      kPassthroughCommandDecoderBlockListByDevice.Get())) {
    return false;
  }
  if (IsDeviceBlocked(
          base::android::android_info::android_build_id(),
          kPassthroughCommandDecoderBlockListByAndroidBuildId.Get())) {
    return false;
  }
  if (IsDeviceBlocked(
          base::android::android_info::manufacturer(),
          kPassthroughCommandDecoderBlockListByManufacturer.Get())) {
    return false;
  }
  if (IsDeviceBlocked(base::android::android_info::model(),
                      kPassthroughCommandDecoderBlockListByModel.Get())) {
    return false;
  }
  if (IsDeviceBlocked(base::android::android_info::board(),
                      kPassthroughCommandDecoderBlockListByBoard.Get())) {
    return false;
  }
  if (IsDeviceBlocked(
          base::android::android_info::android_build_fp(),
          kPassthroughCommandDecoderBlockListByAndroidBuildFP.Get())) {
    return false;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  return true;
#endif  // defined(PASSTHROUGH_COMMAND_DECODER_LAUNCHED)
}

#if DCHECK_IS_ON()
bool IsANGLEValidationEnabled() {
  return true;
}
#else
// Enables the use of ANGLE validation for EGL and GL (non-WebGL) contexts.
BASE_FEATURE(kDefaultEnableANGLEValidation, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsANGLEValidationEnabled() {
  return base::FeatureList::IsEnabled(kDefaultEnableANGLEValidation) &&
         UsePassthroughCommandDecoder();
}
#endif

// Killswitch feature for allowing ANGLE to pass untranslated shaders to the
// driver.
BASE_FEATURE(kAllowANGLEPassthroughShaders, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsANGLEPassthroughShadersAllowed() {
  return base::FeatureList::IsEnabled(kAllowANGLEPassthroughShaders);
}

void GetANGLEFeaturesFromCommandLineAndFinch(
    const base::CommandLine* command_line,
    std::vector<std::string>& enabled_angle_features,
    std::vector<std::string>& disabled_angle_features) {
  SplitAndAppendANGLEFeatureList(
      command_line->GetSwitchValueASCII(switches::kEnableANGLEFeatures),
      enabled_angle_features);
  SplitAndAppendANGLEFeatureList(
      command_line->GetSwitchValueASCII(switches::kDisableANGLEFeatures),
      disabled_angle_features);

  if (base::FeatureList::IsEnabled(kForceANGLEFeatures)) {
    SplitAndAppendANGLEFeatureList(kForcedANGLEEnabledFeaturesFP.Get(),
                                   enabled_angle_features);
    SplitAndAppendANGLEFeatureList(kForcedANGLEDisabledFeaturesFP.Get(),
                                   disabled_angle_features);
  }
}

bool ShouldFallbackToSWIfGLES3NotSupported() {
#if BUILDFLAG(IS_CHROMEOS)
  static bool is_enabled =
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kRevenBranding) &&
      base::FeatureList::IsEnabled(kFallbackToSWIfGLES3NotSupported);
  return is_enabled;
#else   // !BUILDFLAG(IS_CHROMEOS)
  return base::FeatureList::IsEnabled(kFallbackToSWIfGLES3NotSupported);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(ENABLE_SWIFTSHADER)
#if BUILDFLAG(IS_FUCHSIA)
// SwiftShader is always used on Fuchsia, sometimes at the system level.
bool IsSwiftShaderAllowedByCommandLine(const base::CommandLine* command_line) {
  return true;
}
#else
bool IsSwiftShaderAllowedByCommandLine(const base::CommandLine* command_line) {
  // If the switch to opt-into unsafe SwiftShader is present, always allow
  // SwiftShader.
  if (command_line->HasSwitch(switches::kEnableUnsafeSwiftShader)) {
    return true;
  }

  std::string angle_name =
      command_line->GetSwitchValueASCII(switches::kUseANGLE);
  if (angle_name == gl::kANGLEImplementationSwiftShaderName ||
      angle_name == gl::kANGLEImplementationSwiftShaderForWebGLName) {
    // If SwiftShader is specifically requested with the --use-angle command
    // line flag, allow it.
    return true;
  }

  return false;
}

bool IsSwiftShaderUsedForWebGLByCommandLine(
    const base::CommandLine* command_line) {
  std::string use_gl = command_line->GetSwitchValueASCII(switches::kUseGL);
  if (!use_gl.empty() && use_gl != gl::kGLImplementationANGLEName) {
    return false;
  }
  return command_line->GetSwitchValueASCII(switches::kUseANGLE) ==
         gl::kANGLEImplementationSwiftShaderForWebGLName;
}
#endif

// Allow fallback to SwfitShader without command line flags during the
// deprecation period.
BASE_FEATURE(kAllowSwiftShaderFallback, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSwiftShaderAllowedByFeature() {
  return base::FeatureList::IsEnabled(kAllowSwiftShaderFallback);
}
#else
bool IsSwiftShaderAllowedByCommandLine(const base::CommandLine*) {
  return false;
}

bool IsSwiftShaderAllowedByFeature() {
  return false;
}

bool IsSwiftShaderUsedForWebGLByCommandLine(const base::CommandLine*) {
  return false;
}
#endif

bool IsSwiftShaderAllowed(const base::CommandLine* command_line) {
  return IsSwiftShaderAllowedByCommandLine(command_line) ||
         IsSwiftShaderAllowedByFeature();
}

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kAllowD3D11WarpFallback, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsWARPAllowed(const base::CommandLine* command_line) {
  if (command_line->HasSwitch(switches::kDisableD3D11Warp)) {
    return false;
  }
  return base::FeatureList::IsEnabled(kAllowD3D11WarpFallback);
}
#else
bool IsWARPAllowed(const base::CommandLine*) {
  return false;
}
#endif

bool IsAnySoftwareGLAllowed(const base::CommandLine* command_line) {
  return IsWARPAllowed(command_line) || IsSwiftShaderAllowed(command_line);
}

BASE_FEATURE(kAllowSoftwareGLFallbackDueToCrashes,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSoftwareGLFallbackDueToCrashesAllowed(
    const base::CommandLine* command_line) {
  if (!IsAnySoftwareGLAllowed(command_line)) {
    return false;
  }

  return base::FeatureList::IsEnabled(kAllowSoftwareGLFallbackDueToCrashes);
}

BASE_FEATURE(kAllowANGLED3D9Fallback, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsANGLED3D9FallbackAllowed() {
  return base::FeatureList::IsEnabled(kAllowANGLED3D9Fallback);
}

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kAndroidLimitRgb565DisplayToApi32,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool PreferRGB565ResourcesForDisplay() {
  return base::SysInfo::AmountOfTotalPhysicalMemory().InMiB() <= 512 &&
         (base::android::android_info::sdk_int() <=
              base::android::android_info::SDK_VERSION_Sv2 ||
          !base::FeatureList::IsEnabled(kAndroidLimitRgb565DisplayToApi32));
}
#endif

}  // namespace features

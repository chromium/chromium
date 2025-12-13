// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_features.h"

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

const base::FeatureParam<std::string>
    kPassthroughCommandDecoderBlockListByModel{
        &kDefaultPassthroughCommandDecoder, "BlockListByModel", ""};

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
             base::FEATURE_DISABLED_BY_DEFAULT);

// Add a small delay in shader compiling if validating command decoder is used.
// This is to verify if passthrough command decoder impacting negatively top
// level metrics could be due to slower shader compiling.
BASE_FEATURE(kAddDelayToGLCompileShader, base::FEATURE_DISABLED_BY_DEFAULT);
// Histogram |GrCompileShaderUs| mean is 1.8ms (native) vs 3.1ms (ANGLE).
// Therefore, we add a 1.3ms delay to shader compiling.
constexpr base::FeatureParam<base::TimeDelta> kGLCompileShaderDelay = {
    &kAddDelayToGLCompileShader, /*name=*/"interval",
    /*default_value=*/base::Microseconds(1300)};
#endif  // !defined(PASSTHROUGH_COMMAND_DECODER_LAUNCHED)

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
  if (angle_name == gl::kANGLEImplementationSwiftShaderName) {
    // If SwiftShader is specifically requested with the --use-angle command
    // line flag, allow it.
    return true;
  }

  return false;
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

base::TimeDelta GetGLCompileShaderDelay() {
#if BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER)
  if (UsePassthroughCommandDecoder()) {
    return base::TimeDelta();
  }
  if (!base::FeatureList::IsEnabled(kAddDelayToGLCompileShader)) {
    return base::TimeDelta();
  }
  return kGLCompileShaderDelay.Get();
#else
  return base::TimeDelta();
#endif  // BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER)
}

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kAndroidLimitRgb565DisplayToApi32,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool PreferRGB565ResourcesForDisplay() {
  return base::SysInfo::AmountOfPhysicalMemory().InMiB() <= 512 &&
         (base::android::android_info::sdk_int() <=
              base::android::android_info::SDK_VERSION_Sv2 ||
          !base::FeatureList::IsEnabled(kAndroidLimitRgb565DisplayToApi32));
}
#endif

}  // namespace features

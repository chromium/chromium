// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "third_party/angle/src/gpu_info_util/SystemInfo.h"  // nogncheck
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

const base::FeatureParam<std::string>
    kPassthroughCommandDecoderBlockListByGPUVendorId{
        &kDefaultPassthroughCommandDecoder, "BlockListByGPUVendorId", ""};

bool IsDeviceBlocked(const char* field, const std::string& block_list) {
  auto disable_patterns = base::SplitString(
      block_list, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& disable_pattern : disable_patterns) {
    if (base::MatchPattern(field, disable_pattern))
      return true;
  }
  return false;
}
bool IsDeviceBlocked(angle::VendorID vendor_id, const std::string& block_list) {
  auto disable_vendors = base::SplitString(
      block_list, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& disable_vendor_str : disable_vendors) {
    angle::VendorID disable_vendor = 0;
    if (!base::StringToUint(disable_vendor_str, &disable_vendor)) {
      DCHECK(false) << "BlockListByGPUVendorId vendor \"" << disable_vendor_str
                    << "\" failed to parse as a VendorID.";
      return false;
    }

    if (vendor_id == disable_vendor) {
      return true;
    }
  }
  return false;
}
#endif

BASE_FEATURE(kForceANGLEFeatures,
             "ForceANGLEFeatures",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
BASE_FEATURE(kGpuVsync, "GpuVsync", base::FEATURE_DISABLED_BY_DEFAULT);
#else
BASE_FEATURE(kGpuVsync, "GpuVsync", base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER)
// Use the passthrough command decoder by default.  This can be overridden with
// the --use-cmd-decoder=passthrough or --use-cmd-decoder=validating flags.
// Feature lives in ui/gl because it affects the GL binding initialization on
// platforms that would otherwise not default to using EGL bindings.
BASE_FEATURE(kDefaultPassthroughCommandDecoder,
             "DefaultPassthroughCommandDecoder",
             base::FEATURE_ENABLED_BY_DEFAULT
);
#endif  // !defined(PASSTHROUGH_COMMAND_DECODER_LAUNCHED)

#if BUILDFLAG(IS_MAC)
// If true, metal shader programs are written to disk.
//
// As the gpu process writes to disk when this is set, you must also disable
// the sandbox.
//
// The path the shaders are written to is controlled via the command line switch
// --shader-cache-path (default is /tmp/shaders).
BASE_FEATURE(kWriteMetalShaderCacheToDisk,
             "WriteMetalShaderCacheToDisk",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If true, the metal shader cache is read from a file and put into BlobCache
// during startup.
BASE_FEATURE(kUseBuiltInMetalShaderCache,
             "UseBuiltInMetalShaderCache",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_WIN)
// If true, VSyncThreadWin will use the primary monitor's
// refresh rate as the vsync interval.
BASE_FEATURE(kUsePrimaryMonitorVSyncIntervalOnSV3,
             "UsePrimaryMonitorVSyncIntervalOnSV3",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

bool UseGpuVsync() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableGpuVsync) &&
         base::FeatureList::IsEnabled(features::kGpuVsync);
}

bool IsAndroidFrameDeadlineEnabled() {
#if BUILDFLAG(IS_ANDROID)
  static bool enabled =
      base::android::BuildInfo::GetInstance()->is_at_least_t() &&
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
  const auto* build_info = base::android::BuildInfo::GetInstance();
  if (IsDeviceBlocked(build_info->brand(),
                      kPassthroughCommandDecoderBlockListByBrand.Get()))
    return false;
  if (IsDeviceBlocked(build_info->device(),
                      kPassthroughCommandDecoderBlockListByDevice.Get()))
    return false;
  if (IsDeviceBlocked(
          build_info->android_build_id(),
          kPassthroughCommandDecoderBlockListByAndroidBuildId.Get()))
    return false;
  if (IsDeviceBlocked(build_info->manufacturer(),
                      kPassthroughCommandDecoderBlockListByManufacturer.Get()))
    return false;
  if (IsDeviceBlocked(build_info->model(),
                      kPassthroughCommandDecoderBlockListByModel.Get()))
    return false;
  if (IsDeviceBlocked(build_info->board(),
                      kPassthroughCommandDecoderBlockListByBoard.Get()))
    return false;
  if (IsDeviceBlocked(
          build_info->android_build_fp(),
          kPassthroughCommandDecoderBlockListByAndroidBuildFP.Get()))
    return false;

  // Only check system info once and cache if the vendor is blocked.
  static std::optional<bool> gpu_vendor_blocked;
  if (!gpu_vendor_blocked.has_value()) {
    angle::SystemInfo angle_system_info;
    if (angle::GetSystemInfo(&angle_system_info) &&
        !angle_system_info.gpus.empty()) {
      angle::VendorID gpu_vendor_id =
          angle_system_info.gpus[angle_system_info.activeGPUIndex].vendorId;
      gpu_vendor_blocked = IsDeviceBlocked(
          gpu_vendor_id,
          kPassthroughCommandDecoderBlockListByGPUVendorId.Get());
    } else {
      // If system info collection fails, do not blocklist this device by GPU
      // vendor ID. Instead rely on individual device model or device ID
      // blocking.
      gpu_vendor_blocked = false;
    }
  }

  DCHECK(gpu_vendor_blocked.has_value());
  if (gpu_vendor_blocked.value()) {
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
BASE_FEATURE(kDefaultEnableANGLEValidation,
             "DefaultEnableANGLEValidation",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsANGLEValidationEnabled() {
  return base::FeatureList::IsEnabled(kDefaultEnableANGLEValidation) &&
         UsePassthroughCommandDecoder();
}
#endif

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

#if BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(features::kWriteMetalShaderCacheToDisk)) {
    disabled_angle_features.push_back("enableParallelMtlLibraryCompilation");
    enabled_angle_features.push_back("compileMetalShaders");
    enabled_angle_features.push_back("disableProgramCaching");
  }
  if (base::FeatureList::IsEnabled(features::kUseBuiltInMetalShaderCache)) {
    enabled_angle_features.push_back("loadMetalShadersFromBlobCache");
  }
#endif
}

#if BUILDFLAG(ENABLE_SWIFTSHADER)
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

// Allow fallback to SwfitShader without command line flags during the
// deprecation period.
BASE_FEATURE(kAllowSwiftShaderFallback,
             "AllowSwiftShaderFallback",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
}  // namespace features

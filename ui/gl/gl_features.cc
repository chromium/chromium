// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "ui/gfx/android/achoreographer_compat.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#endif

namespace features {
namespace {

#if BUILDFLAG(IS_APPLE)
BASE_FEATURE(kGpuVsync, "GpuVsync", base::FEATURE_DISABLED_BY_DEFAULT);
#else
BASE_FEATURE(kGpuVsync, "GpuVsync", base::FEATURE_ENABLED_BY_DEFAULT);
#endif

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

bool IsDeviceBlocked(const char* field, const std::string& block_list) {
  auto disable_patterns = base::SplitString(
      block_list, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& disable_pattern : disable_patterns) {
    if (base::MatchPattern(field, disable_pattern))
      return true;
  }
  return false;
}
#endif

}  // namespace

#if BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER)
// Use the passthrough command decoder by default.  This can be overridden with
// the --use-cmd-decoder=passthrough or --use-cmd-decoder=validating flags.
// Feature lives in ui/gl because it affects the GL binding initialization on
// platforms that would otherwise not default to using EGL bindings.
BASE_FEATURE(kDefaultPassthroughCommandDecoder,
             "DefaultPassthroughCommandDecoder",
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
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
         base::FeatureList::IsEnabled(kGpuVsync);
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
#endif  // BUILDFLAG(IS_ANDROID)

  return true;
#endif  // defined(PASSTHROUGH_COMMAND_DECODER_LAUNCHED)
}

}  // namespace features

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display_features.h"

#include "base/feature_list.h"
#include "base/features.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace display {
namespace features {

#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/368060445): Remove this when the feature is fully launched.
BASE_FEATURE(kSkipEmptyDisplayHotplugEvent, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
// Enables using HDR transfer function if the monitor says it supports it.
BASE_FEATURE(kUseHDRTransferFunction,
// TODO(b/168843009): Temporarily disable on ARM while investigating.
#if defined(ARCH_CPU_ARM_FAMILY)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// Enables using HDR10(PQ) mode if the monitor says it supports it.
BASE_FEATURE(kEnableExternalDisplayHDR10Mode,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature to control if the CTM is dynamically set to the primary transform
// from plane color space to output color space.
BASE_FEATURE(kCtmColorManagement, base::FEATURE_ENABLED_BY_DEFAULT);

// Feature to control if we assume that setting the DRM color space to Default
// will cause the color primaries to be interpreted as Rec709 (as opposed to
// the color primaries from the EDID).
BASE_FEATURE(kDrmColorSpaceDefaultIsRec709, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// This features allows listing all display modes of external displays in the
// display settings and setting any one of them exactly as requested, which can
// be very useful for debugging and development purposes.
BASE_FEATURE(kListAllDisplayModes, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsListAllDisplayModesEnabled() {
  return base::FeatureList::IsEnabled(kListAllDisplayModes);
}

// TODO(gildekel): A temporary flag to control whether EDID-based (vs.
// port-based) display IDs are generated per display. Remove once the migration
// process it complete (b/193019614).
BASE_FEATURE(kEnableEdidBasedDisplayIds, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsEdidBasedDisplayIdsEnabled() {
  return base::FeatureList::IsEnabled(kEnableEdidBasedDisplayIds);
}

// Enable display scale factor meant for OLED display.
BASE_FEATURE(kOledScaleFactorEnabled, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsOledScaleFactorEnabled() {
  return base::FeatureList::IsEnabled(kOledScaleFactorEnabled);
}

// A temporary flag to control hardware mirroring until it is decided whether to
// permanently remove hardware mirroring support. See crbug.com/1161556 for
// details.
BASE_FEATURE(kEnableHardwareMirrorMode, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsHardwareMirrorModeEnabled() {
  return base::FeatureList::IsEnabled(kEnableHardwareMirrorMode);
}

// A temporary flag to require Content Protection to use provisioned key as the
// kernel doesn't expose that it requires this yet.(b/112172923)
BASE_FEATURE(kRequireHdcpKeyProvisioning, base::FEATURE_DISABLED_BY_DEFAULT);
bool IsHdcpKeyProvisioningRequired() {
  return base::FeatureList::IsEnabled(kRequireHdcpKeyProvisioning);
}

BASE_FEATURE(kPanelSelfRefresh2, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPanelSelfRefresh2Enabled() {
  return base::FeatureList::IsEnabled(kPanelSelfRefresh2);
}

BASE_FEATURE(kTiledDisplaySupport, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTiledDisplaySupportEnabled() {
  return base::FeatureList::IsEnabled(kTiledDisplaySupport);
}

BASE_FEATURE(kExcludeDisplayInMirrorMode, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsExcludeDisplayInMirrorModeEnabled() {
  return base::FeatureList::IsEnabled(kExcludeDisplayInMirrorMode);
}

BASE_FEATURE(kFastDrmMasterDrop, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsFastDrmMasterDropEnabled() {
  return base::FeatureList::IsEnabled(kFastDrmMasterDrop);
}

// TODO(crbug.com/392021508): Remove the flag once the feature is launched.
BASE_FEATURE(kFormFactorControlsSubpixelRendering,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool DoesFormFactorControlSubpixelRendering() {
  return base::FeatureList::IsEnabled(kFormFactorControlsSubpixelRendering);
}

// Open Pluggable Specification (OPS) is a special industry standard with
// slot-in computing modules.
BASE_FEATURE(kOpsDisplayScaleFactor, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsOpsDisplayScaleFactorEnabled() {
  return base::FeatureList::IsEnabled(kOpsDisplayScaleFactor);
}

// Optimizes ScreenWinDisplay lookup by caching an HMONITOR for each display.
// This is part of a combined performance experiment so requires both this flag
// and "ReducePPMs". In case of errors this flag can be disabled without
// affecting the rest of the experiment.
BASE_FEATURE(kScreenWinDisplayLookupByHMONITOR,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsScreenWinDisplayLookupByHMONITOREnabled() {
  return base::FeatureList::IsEnabled(base::features::kReducePPMs) &&
         base::FeatureList::IsEnabled(kScreenWinDisplayLookupByHMONITOR);
}

// When this feature is enabled, a different notification will be displayed to
// indicate there is a limit on the number of displays supported by the device.
// This feature takes in a param "display_limit", and it has to be an integer
// value greater than or equal to 0 for this feature to have any effect.
BASE_FEATURE(kMaxExternalDisplaySupportedNotification,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kMaxExternalDisplaySupportedNotificationLimit{
    &kMaxExternalDisplaySupportedNotification, "display_limit", -1};

bool IsMaxExternalDisplaySupportedNotificationEnabled() {
  return base::FeatureList::IsEnabled(
             kMaxExternalDisplaySupportedNotification) &&
         kMaxExternalDisplaySupportedNotificationLimit.Get() >= 0;
}

}  // namespace features
}  // namespace display

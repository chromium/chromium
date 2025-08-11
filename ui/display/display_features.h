// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_DISPLAY_FEATURES_H_
#define UI_DISPLAY_DISPLAY_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace display {
namespace features {

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(DISPLAY_FEATURES)
BASE_DECLARE_FEATURE(kSkipEmptyDisplayHotplugEvent);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(DISPLAY_FEATURES)
BASE_DECLARE_FEATURE(kUseHDRTransferFunction);

COMPONENT_EXPORT(DISPLAY_FEATURES)
BASE_DECLARE_FEATURE(kEnableExternalDisplayHDR10Mode);

COMPONENT_EXPORT(DISPLAY_FEATURES)
BASE_DECLARE_FEATURE(kCtmColorManagement);
#endif

COMPONENT_EXPORT(DISPLAY_FEATURES) BASE_DECLARE_FEATURE(kListAllDisplayModes);

COMPONENT_EXPORT(DISPLAY_FEATURES) bool IsListAllDisplayModesEnabled();

COMPONENT_EXPORT(DISPLAY_FEATURES)
BASE_DECLARE_FEATURE(kEnableEdidBasedDisplayIds);

COMPONENT_EXPORT(DISPLAY_FEATURES) bool IsEdidBasedDisplayIdsEnabled();

COMPONENT_EXPORT(DISPLAY_FEATURES)
BASE_DECLARE_FEATURE(kOledScaleFactorEnabled);

COMPONENT_EXPORT(DISPLAY_FEATURES) bool IsOledScaleFactorEnabled();

COMPONENT_EXPORT(DISPLAY_FEATURES)
BASE_DECLARE_FEATURE(kEnableHardwareMirrorMode);

COMPONENT_EXPORT(DISPLAY_FEATURES) bool IsHardwareMirrorModeEnabled();

COMPONENT_EXPORT(DISPLAY_FEATURES)

BASE_DECLARE_FEATURE(kRequireHdcpKeyProvisioning);
COMPONENT_EXPORT(DISPLAY_FEATURES) bool IsHdcpKeyProvisioningRequired();

COMPONENT_EXPORT(DISPLAY_FEATURES) BASE_DECLARE_FEATURE(kPanelSelfRefresh2);

COMPONENT_EXPORT(DISPLAY_FEATURES) bool IsPanelSelfRefresh2Enabled();

COMPONENT_EXPORT(DISPLAY_FEATURES) BASE_DECLARE_FEATURE(kTiledDisplaySupport);

COMPONENT_EXPORT(DISPLAY_FEATURES) bool IsTiledDisplaySupportEnabled();

COMPONENT_EXPORT(DISPLAY_FEATURES)
BASE_DECLARE_FEATURE(kExcludeDisplayInMirrorMode);

COMPONENT_EXPORT(DISPLAY_FEATURES) bool IsExcludeDisplayInMirrorModeEnabled();

COMPONENT_EXPORT(DISPLAY_FEATURES) BASE_DECLARE_FEATURE(kFastDrmMasterDrop);

COMPONENT_EXPORT(DISPLAY_FEATURES) bool IsFastDrmMasterDropEnabled();

COMPONENT_EXPORT(DISPLAY_FEATURES)
bool DoesFormFactorControlSubpixelRendering();

COMPONENT_EXPORT(DISPLAY_FEATURES)
BASE_DECLARE_FEATURE(kOpsDisplayScaleFactor);

COMPONENT_EXPORT(DISPLAY_FEATURES) bool IsOpsDisplayScaleFactorEnabled();

COMPONENT_EXPORT(DISPLAY_FEATURES)
BASE_DECLARE_FEATURE(kScreenWinDisplayLookupByHMONITOR);

COMPONENT_EXPORT(DISPLAY_FEATURES)
bool IsScreenWinDisplayLookupByHMONITOREnabled();

COMPONENT_EXPORT(DISPLAY_FEATURES)
BASE_DECLARE_FEATURE(kMaxExternalDisplaySupportedNotification);

COMPONENT_EXPORT(DISPLAY_FEATURES)
extern const base::FeatureParam<int>
    kMaxExternalDisplaySupportedNotificationLimit;

COMPONENT_EXPORT(DISPLAY_FEATURES)
bool IsMaxExternalDisplaySupportedNotificationEnabled();

}  // namespace features
}  // namespace display

#endif  // UI_DISPLAY_DISPLAY_FEATURES_H_

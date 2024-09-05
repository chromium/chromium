// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_DISPLAY_FEATURES_H_
#define UI_DISPLAY_DISPLAY_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/chromeos_buildflags.h"

namespace display {
namespace features {

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(DISPLAY_FEATURES) BASE_DECLARE_FEATURE(kRoundedDisplay);

COMPONENT_EXPORT(DISPLAY_FEATURES) bool IsRoundedDisplayEnabled();

COMPONENT_EXPORT(DISPLAY_FEATURES)
BASE_DECLARE_FEATURE(kUseHDRTransferFunction);

COMPONENT_EXPORT(DISPLAY_FEATURES)
BASE_DECLARE_FEATURE(kEnableExternalDisplayHDR10Mode);
#endif

#if BUILDFLAG(IS_CHROMEOS)
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

}  // namespace features
}  // namespace display

#endif  // UI_DISPLAY_DISPLAY_FEATURES_H_

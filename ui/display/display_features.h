// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_DISPLAY_FEATURES_H_
#define UI_DISPLAY_DISPLAY_FEATURES_H_

#include "base/feature_list.h"
#include "build/chromeos_buildflags.h"
#include "ui/display/display_export.h"

namespace display {
namespace features {

#if BUILDFLAG(IS_CHROMEOS_ASH)
DISPLAY_EXPORT BASE_DECLARE_FEATURE(kRoundedDisplay);

DISPLAY_EXPORT bool IsRoundedDisplayEnabled();

DISPLAY_EXPORT BASE_DECLARE_FEATURE(kUseHDRTransferFunction);
#endif

DISPLAY_EXPORT BASE_DECLARE_FEATURE(kListAllDisplayModes);

DISPLAY_EXPORT bool IsListAllDisplayModesEnabled();

DISPLAY_EXPORT BASE_DECLARE_FEATURE(kEnableEdidBasedDisplayIds);

DISPLAY_EXPORT bool IsEdidBasedDisplayIdsEnabled();

DISPLAY_EXPORT BASE_DECLARE_FEATURE(kEnableHardwareMirrorMode);

DISPLAY_EXPORT bool IsHardwareMirrorModeEnabled();

DISPLAY_EXPORT BASE_DECLARE_FEATURE(kRequireHdcpKeyProvisioning);
DISPLAY_EXPORT bool IsHdcpKeyProvisioningRequired();

}  // namespace features
}  // namespace display

#endif  // UI_DISPLAY_DISPLAY_FEATURES_H_

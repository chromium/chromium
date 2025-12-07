// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define all the command-line switches used by ui/accessibility.
#ifndef UI_ACCESSIBILITY_ACCESSIBILITY_SWITCHES_H_
#define UI_ACCESSIBILITY_ACCESSIBILITY_SWITCHES_H_

#include "build/build_config.h"
#include "ui/accessibility/ax_base_export.h"

namespace switches {

AX_BASE_EXPORT extern const char kEnableExperimentalAccessibilityAutoclick[];
AX_BASE_EXPORT extern const char
    kEnableExperimentalAccessibilityLabelsDebugging[];
AX_BASE_EXPORT extern const char
    kEnableExperimentalAccessibilityLanguageDetection[];
AX_BASE_EXPORT extern const char
    kEnableExperimentalAccessibilityLanguageDetectionDynamic[];
AX_BASE_EXPORT extern const char kEnableExperimentalAccessibilityManifestV3[];
AX_BASE_EXPORT extern const char
    kEnableExperimentalAccessibilitySwitchAccessText[];
AX_BASE_EXPORT extern const char kEnableMacAccessibilityAPIMigration[];

// Returns true if experimental accessibility language detection is enabled.
AX_BASE_EXPORT bool IsExperimentalAccessibilityLanguageDetectionEnabled();

// Returns true if experimental accessibility language detection support for
// dynamic content is enabled.
AX_BASE_EXPORT bool
IsExperimentalAccessibilityLanguageDetectionDynamicEnabled();

// Returns true if experimental accessibility Switch Access text is enabled.
AX_BASE_EXPORT bool IsExperimentalAccessibilitySwitchAccessTextEnabled();

// Returns true if Switch Access point scanning is enabled.
AX_BASE_EXPORT bool IsMagnifierDebugDrawRectEnabled();

// For development / testing only.
// When enabled the switch generates expectations files upon running an
// ax_inspect test. For example, when running content_browsertests, it saves
// output of failing accessibility tests to their expectations files in
// content/test/data/accessibility/, overwriting existing file content.
AX_BASE_EXPORT extern const char kGenerateAccessibilityTestExpectations[];

AX_BASE_EXPORT extern const char kDisableRendererAccessibility[];

AX_BASE_EXPORT extern const char kForceRendererAccessibility[];

}  // namespace switches

#endif  // UI_ACCESSIBILITY_ACCESSIBILITY_SWITCHES_H_

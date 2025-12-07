// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/accessibility_switches.h"

#include "base/command_line.h"
#include "build/build_config.h"

namespace switches {

// Shows additional automatic click features that haven't launched yet.
const char kEnableExperimentalAccessibilityAutoclick[] =
    "enable-experimental-accessibility-autoclick";

// Enables support for visually debugging the accessibility labels
// feature, which provides images descriptions for screen reader users.
const char kEnableExperimentalAccessibilityLabelsDebugging[] =
    "enable-experimental-accessibility-labels-debugging";

// Enables language detection on in-page text content which is then exposed to
// assistive technology such as screen readers.
const char kEnableExperimentalAccessibilityLanguageDetection[] =
    "enable-experimental-accessibility-language-detection";

// Enables language detection for dynamic content which is then exposed to
// assistive technology such as screen readers.
const char kEnableExperimentalAccessibilityLanguageDetectionDynamic[] =
    "enable-experimental-accessibility-language-detection-dynamic";

// Switches accessibility extensions to use extensions manifest v3 while the
// migration is still in progress.
const char kEnableExperimentalAccessibilityManifestV3[] =
    "enable-experimental-accessibility-manifest-v3";

// Enables in progress Switch Access features for text input.
const char kEnableExperimentalAccessibilitySwitchAccessText[] =
    "enable-experimental-accessibility-switch-access-text";

// Enables debug feature for drawing rectangle around magnified region, without
// zooming in.
const char kEnableMagnifierDebugDrawRect[] = "enable-magnifier-debug-draw-rect";

// Enables the switchover to the newer NSAccessibility property-based API.
const char kEnableMacAccessibilityAPIMigration[] =
    "enable-mac-accessibility-api-migration";

bool IsExperimentalAccessibilityLanguageDetectionEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableExperimentalAccessibilityLanguageDetection);
}

bool IsExperimentalAccessibilityLanguageDetectionDynamicEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableExperimentalAccessibilityLanguageDetectionDynamic);
}

bool IsExperimentalAccessibilitySwitchAccessTextEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableExperimentalAccessibilitySwitchAccessText);
}

bool IsMagnifierDebugDrawRectEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableMagnifierDebugDrawRect);
}

const char kGenerateAccessibilityTestExpectations[] =
    "generate-accessibility-test-expectations";

// Turns off the accessibility in the renderer.
const char kDisableRendererAccessibility[] = "disable-renderer-accessibility";

// Force renderer accessibility to be on instead of enabling it on demand when
// a screen reader is detected. The disable-renderer-accessibility switch
// overrides this if present.
// This switch has an optional parameter that forces an AXMode bundle. The three
// available bundle settings are: 'basic', 'form-controls', and 'complete'. If
// the bundle argument is invalid, then the forced AXMode will default to
// 'complete'. If the bundle argument is missing, then the initial AXMode will
// default to complete but allow changes to the AXMode during execution.
const char kForceRendererAccessibility[] = "force-renderer-accessibility";

}  // namespace switches
